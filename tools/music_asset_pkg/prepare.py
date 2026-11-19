"""Prepare converted AAX files + patched CPK cache from the music manifest."""

from __future__ import annotations

import hashlib
import json
import tomllib
from pathlib import Path

from .cpk_patch import inplace_replace, sha256_file, verify_entry
from .mapping import CPK_DISC_NAME, STAGE_AAX
from .wav_to_aax import analyze_wav, validate_aax, wav_to_aax

ROOT = Path(__file__).resolve().parents[2]


def _file_fingerprint(path: Path) -> str:
    st = path.stat()
    return f"{path.resolve()}:{st.st_size}:{int(st.st_mtime_ns)}"


def content_hash(parts: list[str]) -> str:
    h = hashlib.sha256()
    for p in parts:
        h.update(p.encode())
        h.update(b"\0")
    return h.hexdigest()[:16]


def load_manifest_assets(manifest: Path) -> list[dict]:
    """Load resource-based [assets.*] plus legacy [tracks.*] stage mappings."""
    data = tomllib.loads(manifest.read_text(encoding="utf-8"))
    assets: list[dict] = []

    raw_assets = data.get("assets", {})
    if isinstance(raw_assets, dict):
        for key, t in raw_assets.items():
            if not isinstance(t, dict):
                continue
            assets.append({**t, "_key": key, "_legacy": False})

    # Migrate legacy stage tracks → asset entries (main only unless path set)
    raw_tracks = data.get("tracks", {})
    if isinstance(raw_tracks, dict):
        for key, t in raw_tracks.items():
            if not isinstance(t, dict):
                continue
            stage = t.get("stage", key)
            if stage not in STAGE_AAX:
                continue
            main, fx = STAGE_AAX[stage]
            # Skip if already covered by explicit assets entry with same path
            if any(a.get("path") == main for a in assets):
                continue
            assets.append(
                {
                    **t,
                    "_key": f"legacy_{stage}_main",
                    "_legacy": True,
                    "archive": t.get("archive", CPK_DISC_NAME),
                    "path": main,
                    "stage": stage,
                    "variant": "main",
                    "source": t.get("source") or t.get("file", ""),
                    "file": t.get("file", ""),
                }
            )
            # Legacy tracks did not map FX unless explicitly requested via assets
    return assets


def extract_templates_by_path(cpk: Path, out_dir: Path, paths: set[str]) -> dict[str, Path]:
    from .cricodecs_env import ensure_cricodecs
    ensure_cricodecs()
    from cricodecs.cpk import Cpk

    out_dir.mkdir(parents=True, exist_ok=True)
    cpk_obj = Cpk.load(str(cpk))
    by_path = {e.full_path: e for e in cpk_obj.file_infos()}
    result = {}
    for path in sorted(paths):
        if path not in by_path:
            raise KeyError(f"missing in CPK: {path}")
        e = by_path[path]
        dest = out_dir / Path(path).name
        if not dest.is_file() or dest.stat().st_size != e.file_size:
            dest.write_bytes(cpk_obj.file_bytes(e.toc_index))
        result[path] = dest
    return result


def prepare(
    *,
    manifest: Path,
    music_dir: Path,
    original_cpk: Path,
    cache_root: Path,
    replace_fx: bool = False,  # unused for asset mode; kept for CLI compat
) -> dict:
    """Build userdata/music_mods/<hash>/sonic2010_0.cpk from asset mappings."""
    del replace_fx  # asset paths are explicit
    manifest = Path(manifest)
    music_dir = Path(music_dir)
    original_cpk = Path(original_cpk)
    cache_root = Path(cache_root)

    if not manifest.is_file():
        return {"ok": False, "error": f"no manifest at {manifest}", "use_original": True}
    if not original_cpk.is_file():
        return {"ok": False, "error": f"original CPK missing: {original_cpk}", "use_original": True}

    entries = load_manifest_assets(manifest)
    active = []
    for t in entries:
        if t.get("enabled", True) is False:
            continue
        internal = t.get("path", "")
        rel = t.get("source") or t.get("file", "")
        if not internal or not rel:
            continue
        archive = t.get("archive", CPK_DISC_NAME)
        if archive != original_cpk.name and archive != CPK_DISC_NAME:
            return {
                "ok": False,
                "error": f"unsupported archive '{archive}' (only {CPK_DISC_NAME} for SNCP8P)",
                "use_original": True,
            }
        wav = (music_dir / rel).resolve()
        if not wav.is_file():
            return {"ok": False, "error": f"WAV missing for {internal}: {wav}", "use_original": True}
        active.append({**t, "wav": wav, "path": internal})

    if not active:
        return {
            "ok": True,
            "use_original": True,
            "message": "no enabled asset mappings — using original CPK",
            "original_cpk": str(original_cpk),
            "original_sha256": sha256_file(original_cpk),
        }

    # Reject duplicate targets
    seen = {}
    for t in active:
        p = t["path"]
        if p in seen:
            return {
                "ok": False,
                "error": f"duplicate target path {p} ({seen[p]} and {t['_key']})",
                "use_original": True,
            }
        seen[p] = t["_key"]

    fp_parts = [
        f"cpk:{sha256_file(original_cpk)[:16]}",
        f"man:{_file_fingerprint(manifest)}",
        "patcher=toc_v2",
        "pack=utf8align",
        "manifest=assets_v1",
    ]
    for t in active:
        fp_parts.append(
            f"{t['path']}:{_file_fingerprint(t['wav'])}:loop={t.get('loop_start', 0)}"
        )
    key = content_hash(fp_parts)
    cache_dir = cache_root / key
    cache_dir.mkdir(parents=True, exist_ok=True)
    status_path = cache_dir / "status.json"
    patched = cache_dir / CPK_DISC_NAME
    if patched.is_file() and status_path.is_file():
        st = json.loads(status_path.read_text(encoding="utf-8"))
        if st.get("ok") and st.get("cache_key") == key:
            st["cache_hit"] = True
            return st

    templates_dir = cache_root / "templates"
    paths = {t["path"] for t in active}
    templates = extract_templates_by_path(original_cpk, templates_dir, paths)

    converted_dir = cache_dir / "converted"
    converted_dir.mkdir(parents=True, exist_ok=True)
    replacements: dict[str, Path] = {}
    converted_meta = []
    wav_meta = []

    from cricodecs.aax import Aax
    from cricodecs.cpk import Cpk

    cpk_obj = Cpk.load(str(original_cpk))
    slot_size = {e.full_path: e.file_size for e in cpk_obj.file_infos()}

    for t in active:
        internal = t["path"]
        loop_start = float(t.get("loop_start", 0) or 0)
        wav_info = analyze_wav(t["wav"])
        wav_meta.append({"path": internal, "key": t["_key"], **wav_info})
        work = converted_dir / t["_key"]
        work.mkdir(parents=True, exist_ok=True)
        out_aax = converted_dir / Path(internal).name
        tmpl = Aax.load(str(templates[internal]))
        meta = wav_to_aax(
            t["wav"],
            templates[internal],
            out_aax,
            channels=tmpl.channels or 2,
            loop_start_sec=loop_start,
            work_dir=work,
            max_bytes=slot_size.get(internal),
        )
        validate_aax(out_aax)
        replacements[internal] = out_aax
        converted_meta.append({"key": t["_key"], "path": internal, "role": t.get("variant", "main"), **meta})

    patch_info = inplace_replace(original_cpk, patched, replacements)
    verified = [verify_entry(patched, p) for p in replacements]

    proof = {
        "patched_cpk": str(patched),
        "patched_sha256": patch_info["dst_sha256"],
        "original_cpk": str(original_cpk),
        "original_sha256": patch_info["src_sha256"],
        "entries": patch_info["entries"],
        "verified": verified,
    }
    proof_path = cache_dir / "payload_proof.json"
    proof_path.write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
    tsv_lines = ["path\tfile_offset\tfile_size\tsha256\tfirst16_hex"]
    for m in patch_info["entries"]:
        data = Path(replacements[m["path"]]).read_bytes()
        tsv_lines.append(
            f"{m['path']}\t{m['new_file_offset']}\t{m['new_file_size']}\t{m['sha256']}\t{data[:16].hex()}"
        )
    tsv_path = cache_dir / "payload_proof.tsv"
    tsv_path.write_text("\n".join(tsv_lines) + "\n", encoding="utf-8")

    status = {
        "ok": True,
        "use_original": False,
        "cache_hit": False,
        "cache_key": key,
        "cache_dir": str(cache_dir),
        "patched_cpk": str(patched),
        "disc_name": CPK_DISC_NAME,
        "original_cpk": str(original_cpk),
        "original_sha256": patch_info["src_sha256"],
        "patched_sha256": patch_info["dst_sha256"],
        # Multi-archive ready: register every entry before the game opens it.
        "overrides": [
            {
                "disc_name": CPK_DISC_NAME,
                "patched_cpk": str(patched),
                "patched_sha256": patch_info["dst_sha256"],
            }
        ],
        "replaced": patch_info["replaced"],
        "entries": patch_info["entries"],
        "converted": converted_meta,
        "wav_analysis": wav_meta,
        "verified": verified,
        "payload_proof": str(proof_path),
        "payload_proof_tsv": str(tsv_path),
        "overlay": False,
    }
    status_path.write_text(json.dumps(status, indent=2) + "\n", encoding="utf-8")
    (cache_root / "current.json").write_text(
        json.dumps(
            {
                "cache_key": key,
                "patched_cpk": str(patched),
                "use_original": False,
                "status": str(status_path),
                "payload_proof": str(proof_path),
                "payload_proof_tsv": str(tsv_path),
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return status
