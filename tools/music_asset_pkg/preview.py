"""Decode original AAX from unmodified CPK for launcher preview (vgmstream)."""

from __future__ import annotations

import hashlib
import shutil
import subprocess
from pathlib import Path

from .cricodecs_env import ensure_cricodecs, which_ffmpeg
from .cpk_patch import sha256_file


def which_vgmstream() -> str | None:
    for name in ("vgmstream-cli", "vgmstream_cli", "vgmstream123"):
        p = shutil.which(name)
        if p:
            return p
    return None


def preview_cache_key(cpk_sha: str, internal_path: str) -> str:
    h = hashlib.sha256(f"{cpk_sha}:{internal_path}:vgmstream".encode()).hexdigest()[:16]
    safe = Path(internal_path).stem.replace("/", "_")[:40]
    return f"{safe}_{h}"


def extract_and_decode_original(
    *,
    cpk_path: Path,
    internal_path: str,
    cache_dir: Path,
) -> dict:
    """Extract AAX from original CPK and decode to WAV via vgmstream.

    Never reads a patched music-mod cache CPK.
    """
    ensure_cricodecs()
    from cricodecs.cpk import Cpk

    cpk_path = Path(cpk_path)
    cache_dir = Path(cache_dir)
    cache_dir.mkdir(parents=True, exist_ok=True)

    vg = which_vgmstream()
    if not vg:
        return {"ok": False, "error": "vgmstream-cli not found on PATH"}

    cpk_sha = sha256_file(cpk_path)
    key = preview_cache_key(cpk_sha, internal_path)
    wav_out = cache_dir / f"{key}.wav"
    meta_out = cache_dir / f"{key}.json"
    if wav_out.is_file() and wav_out.stat().st_size > 44:
        return {
            "ok": True,
            "wav": str(wav_out),
            "cache_hit": True,
            "cpk": str(cpk_path),
            "path": internal_path,
            "cpk_sha256": cpk_sha,
        }

    cpk = Cpk.load(str(cpk_path))
    entry = None
    for e in cpk.file_infos():
        if e.full_path == internal_path or e.filename == Path(internal_path).name:
            entry = e
            break
    if entry is None:
        return {"ok": False, "error": f"not found in CPK: {internal_path}"}

    aax_path = cache_dir / f"{key}.aax"
    aax_path.write_bytes(cpk.file_bytes(entry.toc_index))

    # Decode looping once through without infinite loop (-i = ignore loop / play once)
    cmd = [vg, "-o", str(wav_out), "-i", str(aax_path)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0 or not wav_out.is_file():
        return {
            "ok": False,
            "error": f"vgmstream failed: {(r.stderr or r.stdout)[-600:]}",
            "aax": str(aax_path),
        }

    import json
    meta_out.write_text(
        json.dumps(
            {
                "cpk": str(cpk_path),
                "cpk_sha256": cpk_sha,
                "path": internal_path,
                "file_size": entry.file_size,
                "wav": str(wav_out),
                "vgmstream": (r.stdout or "")[-800:],
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return {
        "ok": True,
        "wav": str(wav_out),
        "aax": str(aax_path),
        "cache_hit": False,
        "cpk": str(cpk_path),
        "path": internal_path,
        "cpk_sha256": cpk_sha,
        "meta": str(meta_out),
    }
