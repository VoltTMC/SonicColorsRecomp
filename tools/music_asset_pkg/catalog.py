"""Discover and classify Sonic Colors music AAX assets → catalog.json."""

from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path

from .cricodecs_env import ensure_cricodecs
from .cpk_patch import sha256_file

ROOT = Path(__file__).resolve().parents[2]

WORLD_NAMES = {
    "rso": "Tropical Resort",
    "fct": "Sweet Mountain",
    "shp": "Starlight Carnival",
    "pla": "Planet Wisp",
    "qua": "Aquarium Park",
    "ast": "Asteroid Coaster",
    "elv": "Terminal Velocity",
}

STAGE_LABELS = {
    # filled from well-known acts when filename matches bgm_stgXYY
}


def _asset_id(archive: str, internal_path: str) -> str:
    h = hashlib.sha256(f"{archive}:{internal_path}".encode()).hexdigest()[:12]
    stem = Path(internal_path).stem
    # Stable readable id + hash suffix for uniqueness
    safe = re.sub(r"[^a-zA-Z0-9_]+", "_", stem)[:48]
    return f"{safe}__{h}"


def _classify(filename: str) -> dict:
    """Return category, variant, label, stage, confidence for a bgm_*.aax name."""
    base = filename.replace(".aax", "").replace("_wav", "")
    fx = "_fx" in base or base.endswith("_fx")
    variant = "fx" if fx else "main"
    name = base.replace("_fx", "").replace("_nomal", "").replace("_normal", "")

    category = "other"
    stage = None
    world = None
    act = None
    label = name
    confidence = 0.7

    m = re.match(r"bgm_stg(\d)(\d{2})_(.+)", name)
    if m:
        world_digit, act_s, rest = m.group(1), m.group(2), m.group(3)
        stage = f"stg{world_digit}{act_s}"
        act = int(act_s[0]) if act_s[0] != "0" else int(act_s)
        # act codes: 10,20,30… → act 1,2,3; 40+ often special
        act = int(act_s) // 10
        world_key = rest.split("_")[0]
        world = WORLD_NAMES.get(world_key, world_key)
        category = "stage"
        confidence = 0.95
        label = f"{world} Act {act}" + (" (FX)" if fx else "")
    elif name.startswith("bgm_boss"):
        category = "boss"
        confidence = 0.9
        label = name.replace("bgm_boss_", "Boss: ").replace("_", " ")
        if fx:
            label += " (FX)"
    elif name.startswith("bgm_jingle"):
        category = "jingle"
        confidence = 0.9
        label = name.replace("bgm_jingle_", "Jingle: ").replace("_", " ")
    elif name.startswith("bgm_sys"):
        category = "menu"
        confidence = 0.9
        label = name.replace("bgm_sys_", "System: ").replace("_", " ")
    elif name.startswith("bgm_zmap") or name == "bgm_worldmap":
        category = "map"
        confidence = 0.9
        if name == "bgm_worldmap":
            label = "World Map"
        else:
            key = name.replace("bgm_zmap_", "")
            label = f"Zone Map: {WORLD_NAMES.get(key, key)}"
    elif name.startswith("bgm_mlt"):
        category = "special"
        confidence = 0.85
        label = name.replace("bgm_mlt_", "Multi: ").replace("_", " ")
        if fx:
            label += " (FX)"
    elif name.startswith("bgm_pha"):
        category = "special"
        confidence = 0.85
        label = name.replace("bgm_pha_", "Wisp/Ability: ").replace("_", " ")
    elif name.startswith("bgm_"):
        category = "other"
        confidence = 0.75
        label = name.replace("bgm_", "").replace("_", " ")
        if fx:
            label += " (FX)"

    cue = name  # approx cue stem used by game
    return {
        "category": category,
        "variant": variant,
        "label": label,
        "stage": stage,
        "world": world,
        "act": act,
        "cue": cue,
        "confidence": confidence,
    }


def build_catalog(
    *,
    cpk_path: Path,
    out_path: Path | None = None,
    include_voice: bool = False,
) -> dict:
    ensure_cricodecs()
    from cricodecs.aax import Aax
    from cricodecs.cpk import Cpk

    cpk_path = Path(cpk_path)
    archive = cpk_path.name
    cpk_sha = sha256_file(cpk_path)
    cpk = Cpk.load(str(cpk_path))

    assets = []
    skipped = []
    for e in cpk.file_infos():
        if not e.filename.lower().endswith(".aax"):
            continue
        fn = e.filename
        if fn.startswith("ve_") or fn.startswith("vj_"):
            skipped.append({"path": e.full_path, "reason": "voice"})
            if not include_voice:
                continue
            kind = "voice"
        elif fn.startswith("bgm_"):
            kind = "music"
        else:
            skipped.append({"path": e.full_path, "reason": "non-bgm"})
            continue

        meta = _classify(fn)
        try:
            blob = cpk.file_bytes(e.toc_index)
            a = Aax.load_bytes(blob)
            duration = (a.sample_count / float(a.sample_rate)) if a.sample_rate else 0.0
            audio = {
                "format": "aax",
                "sample_rate": a.sample_rate,
                "channels": a.channels,
                "sample_count": a.sample_count,
                "segment_count": a.segment_count,
                "has_loop_segments": a.has_loop_segments,
                "duration_sec": round(duration, 3),
                "segments": [
                    {
                        "index": i,
                        "sample_count": a.segments[i].sample_count,
                        "data_size": a.segments[i].data_size,
                        "loop": a.segments[i].loop_segment,
                    }
                    for i in range(a.segment_count)
                ],
            }
        except Exception as ex:
            audio = {"format": "aax", "error": str(ex)}
            duration = 0.0

        assets.append(
            {
                "id": _asset_id(archive, e.full_path),
                "kind": kind,
                "archive": archive,
                "path": e.full_path,
                "filename": fn,
                "file_id": e.id,
                "toc_index": e.toc_index,
                "file_offset": e.file_offset,
                "file_size": e.file_size,
                "extract_size": e.extract_size,
                **meta,
                "audio": audio,
            }
        )

    assets.sort(key=lambda x: (x["category"], x.get("stage") or "", x["filename"]))
    catalog = {
        "version": 1,
        "game": "SNCP8P",
        "archives": [{"name": archive, "sha256": cpk_sha, "path": str(cpk_path)}],
        "asset_count": len(assets),
        "music_count": sum(1 for a in assets if a["kind"] == "music"),
        "skipped_voice_or_other": len(skipped),
        "assets": assets,
    }
    if out_path:
        out_path = Path(out_path)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")
    return catalog


def default_catalog_path(music_dir: Path | None = None) -> Path:
    base = music_dir or (ROOT / "mods" / "music")
    return Path(base) / "catalog.json"
