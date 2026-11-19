#!/usr/bin/env python3
"""CLI for Sonic Colors Recomp stage music mods (mods/music/manifest.toml)."""
from __future__ import annotations

import argparse
import os
import re
import struct
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DIR = ROOT / "mods" / "music"
MANIFEST = "manifest.toml"

# Stage ID → default BGM cue (from sonic2010_0.cpk Synth/ table)
STAGE_CUES = {
    "stg110": "bgm_stg110_rso", "stg120": "bgm_stg120_rso", "stg130": "bgm_stg130_rso",
    "stg140": "bgm_stg140_rso", "stg150": "bgm_stg150_rso", "stg160": "bgm_stg160_rso",
    "stg210": "bgm_stg210_fct", "stg220": "bgm_stg220_fct", "stg230": "bgm_stg230_fct",
    "stg240": "bgm_stg240_fct", "stg250": "bgm_stg250_fct", "stg260": "bgm_stg260_fct",
    "stg310": "bgm_stg310_shp", "stg320": "bgm_stg320_shp", "stg330": "bgm_stg330_shp",
    "stg340": "bgm_stg340_shp", "stg350": "bgm_stg350_shp", "stg360": "bgm_stg360_shp",
    "stg410": "bgm_stg410_pla", "stg420": "bgm_stg420_pla", "stg430": "bgm_stg430_pla",
    "stg440": "bgm_stg440_pla", "stg450": "bgm_stg450_pla", "stg460": "bgm_stg460_pla",
    "stg510": "bgm_stg510_qua", "stg520": "bgm_stg520_qua", "stg530": "bgm_stg530_qua",
    "stg540": "bgm_stg540_qua", "stg550": "bgm_stg550_qua", "stg560": "bgm_stg560_qua",
    "stg610": "bgm_stg610_ast", "stg620": "bgm_stg620_ast", "stg630": "bgm_stg630_ast",
    "stg640": "bgm_stg640_ast", "stg650": "bgm_stg650_ast", "stg660": "bgm_stg660_ast",
    "stg710": "bgm_stg710_elv", "stg720": "bgm_stg720_elv", "stg730": "bgm_stg730_elv",
}

WORLD = {
    "rso": "Tropical Resort", "fct": "Sweet Mountain", "shp": "Starlight Carnival",
    "pla": "Planet Wisp", "qua": "Aquarium Park", "ast": "Asteroid Coaster", "elv": "Terminal Velocity",
}


def music_dir(args) -> Path:
    return Path(args.dir) if getattr(args, "dir", None) else DEFAULT_DIR


def parse_manifest(path: Path) -> dict[str, dict]:
    tracks: dict[str, dict] = {}
    if not path.exists():
        return tracks
    cur = None
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        m = re.match(r"\[tracks\.([^\]]+)\]", line)
        if m:
            cur = m.group(1)
            tracks[cur] = {"id": cur, "enabled": True, "loop": True, "volume": 1.0,
                           "fade_in_ms": 0, "fade_out_ms": 0, "file": "", "stage": "", "cue": ""}
            continue
        if cur is None or "=" not in line:
            continue
        k, v = line.split("=", 1)
        k, v = k.strip(), v.strip().strip('"').strip("'")
        if k in ("enabled", "loop"):
            tracks[cur][k] = v.lower() in ("true", "1", "yes")
        elif k in ("volume", "loop_start", "loop_end"):
            tracks[cur][k] = float(v)
        elif k in ("fade_in_ms", "fade_out_ms", "act"):
            tracks[cur][k] = int(float(v))
        else:
            tracks[cur][k] = v
    for t in tracks.values():
        if not t.get("cue") and t.get("stage"):
            t["cue"] = STAGE_CUES.get(t["stage"], "")
    return tracks


def write_manifest(path: Path, tracks: dict[str, dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# Generated/updated by tools/music_mod", ""]
    for tid, t in tracks.items():
        lines.append(f"[tracks.{tid}]")
        for k in ("stage", "cue", "file", "enabled", "loop", "volume", "fade_in_ms", "fade_out_ms",
                  "loop_start", "loop_end"):
            if k not in t:
                continue
            if k in ("loop_start", "loop_end") and float(t[k]) == 0:
                continue
            v = t[k]
            if isinstance(v, bool):
                lines.append(f"{k} = {'true' if v else 'false'}")
            elif isinstance(v, float):
                lines.append(f"{k} = {v}")
            elif isinstance(v, int):
                lines.append(f"{k} = {v}")
            else:
                lines.append(f'{k} = "{v}"')
        lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def inspect_wav(path: Path) -> tuple[bool, str]:
    try:
        with wave.open(str(path), "rb") as w:
            ch, width, rate, nframes, comptype, _ = w.getparams()
        if comptype != "NONE":
            return False, f"compressed WAV ({comptype}) not supported"
        bits = width * 8
        if bits not in (8, 16, 24, 32):
            return False, f"unsupported bit depth {bits}"
        if ch not in (1, 2):
            return False, f"unsupported channels {ch}"
        return True, f"{ch}ch {bits}-bit {rate} Hz, {nframes} frames ({nframes / rate:.1f}s)"
    except wave.Error as e:
        # Manual RIFF for float / 24-bit
        data = path.read_bytes()
        if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
            return False, f"not a WAV ({e})"
        i = 12
        fmt = rate = ch = bits = None
        while i + 8 <= len(data):
            cid, sz = data[i:i+4], struct.unpack_from("<I", data, i + 4)[0]
            body = i + 8
            if cid == b"fmt " and sz >= 16:
                audio_format, ch, rate, _, _, bits = struct.unpack_from("<HHIIHH", data, body)
                if audio_format not in (1, 3, 0xFFFE):
                    return False, f"unsupported format tag {audio_format}"
                return True, f"{ch}ch {bits}-bit {rate} Hz (fmt={audio_format})"
            i = body + sz + (sz & 1)
        return False, "missing fmt chunk"
    except OSError as e:
        return False, str(e)


def cmd_list_stages(_: argparse.Namespace) -> int:
    print(f"{'stage':8}  {'cue':22}  world")
    for stg, cue in STAGE_CUES.items():
        world = WORLD.get(cue.rsplit("_", 1)[-1], "?")
        print(f"{stg:8}  {cue:22}  {world}")
    return 0


def cmd_list_cues(_: argparse.Namespace) -> int:
    for cue in STAGE_CUES.values():
        print(cue)
    print("# Also: bgm_sys_*, bgm_zmap_*, bgm_pha_*, bgm_jingle_*, bgm_boss_* (see docs/MUSIC_MODDING.md)")
    return 0


def cmd_list(args: argparse.Namespace) -> int:
    tracks = parse_manifest(music_dir(args) / MANIFEST)
    if not tracks:
        print("(no tracks in manifest)")
        return 0
    for tid, t in tracks.items():
        en = "on" if t.get("enabled", True) else "off"
        print(f"{tid}: stage={t.get('stage')} cue={t.get('cue')} file={t.get('file')} [{en}]")
    return 0


def cmd_validate(args: argparse.Namespace) -> int:
    d = music_dir(args)
    man = d / MANIFEST
    tracks = parse_manifest(man)
    errors = 0
    if not man.exists():
        print(f"ERROR: missing {man}")
        return 1
    seen_cue: dict[str, str] = {}
    for tid, t in tracks.items():
        stage, cue, f = t.get("stage", ""), t.get("cue", ""), t.get("file", "")
        if not f:
            print(f"ERROR: {tid}: missing file"); errors += 1
            continue
        path = d / f
        if not path.exists():
            print(f"ERROR: {tid}: file not found: {path}"); errors += 1
        else:
            ok, msg = inspect_wav(path)
            print(f"{'OK' if ok else 'ERROR'}: {tid}: {msg} ({f})")
            if not ok:
                errors += 1
        if stage and stage not in STAGE_CUES:
            print(f"WARN: {tid}: unknown stage {stage}")
        if cue:
            if cue in seen_cue:
                print(f"ERROR: {tid}: cue {cue} conflicts with {seen_cue[cue]}"); errors += 1
            seen_cue[cue] = tid
    print(f"{'OK' if errors == 0 else 'FAILED'}: {len(tracks)} track(s), {errors} error(s)")
    return 1 if errors else 0


def cmd_add(args: argparse.Namespace) -> int:
    d = music_dir(args)
    d.mkdir(parents=True, exist_ok=True)
    stage = args.stage
    if stage not in STAGE_CUES:
        print(f"ERROR: unknown stage {stage}", file=sys.stderr)
        return 1
    src = Path(args.wav).expanduser().resolve()
    ok, msg = inspect_wav(src)
    if not ok:
        print(f"ERROR: {msg}", file=sys.stderr)
        return 1
    tracks_dir = d / "tracks"
    tracks_dir.mkdir(parents=True, exist_ok=True)
    dest_name = args.name or f"{stage}.wav"
    # Prefer managed tracks/ layout (same as the launcher Music tab)
    try:
        rel_src = src.relative_to(d.resolve())
        dest_rel = rel_src.as_posix()
    except ValueError:
        dest = tracks_dir / dest_name
        if src != dest:
            dest.write_bytes(src.read_bytes())
        dest_rel = f"tracks/{dest_name}"
    tracks = parse_manifest(d / MANIFEST)
    tid = args.track_id or stage
    tracks[tid] = {
        "id": tid, "stage": stage, "cue": STAGE_CUES[stage], "file": dest_rel,
        "enabled": True, "loop": True, "volume": 1.0, "fade_in_ms": 500, "fade_out_ms": 750,
    }
    write_manifest(d / MANIFEST, tracks)
    print(f"Added {tid} → {dest_rel} ({msg})")
    return 0


def cmd_remove(args: argparse.Namespace) -> int:
    d = music_dir(args)
    tracks = parse_manifest(d / MANIFEST)
    key = args.stage
    # match by id or stage
    drop = [tid for tid, t in tracks.items() if tid == key or t.get("stage") == key]
    if not drop:
        print(f"ERROR: no track for {key}", file=sys.stderr)
        return 1
    for tid in drop:
        del tracks[tid]
        print(f"Removed {tid}")
    write_manifest(d / MANIFEST, tracks)
    return 0


def cmd_enable(args: argparse.Namespace, enabled: bool) -> int:
    d = music_dir(args)
    tracks = parse_manifest(d / MANIFEST)
    key = args.stage
    hit = False
    for tid, t in tracks.items():
        if tid == key or t.get("stage") == key:
            t["enabled"] = enabled
            hit = True
            print(f"{'Enabled' if enabled else 'Disabled'} {tid}")
    if not hit:
        print(f"ERROR: no track for {key}", file=sys.stderr)
        return 1
    write_manifest(d / MANIFEST, tracks)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Manage Sonic Colors Recomp music mods")
    ap.add_argument("--dir", default=str(DEFAULT_DIR), help="music mods directory")
    sp = ap.add_subparsers(dest="cmd", required=True)

    sp.add_parser("list-stages", help="known stage IDs and default BGM cues")
    sp.add_parser("list-cues", help="default stage BGM cue IDs")
    sp.add_parser("list", help="configured replacements")
    sp.add_parser("validate", help="validate manifest + WAV files")

    p = sp.add_parser("add", help="add/replace a stage mapping")
    p.add_argument("stage", help="e.g. stg110")
    p.add_argument("wav", help="path to WAV file")
    p.add_argument("--name", help="destination filename under mods/music")
    p.add_argument("--track-id", help="manifest track id (default: stage)")

    p = sp.add_parser("remove", help="remove a mapping")
    p.add_argument("stage", help="stage or track id")

    p = sp.add_parser("enable", help="enable a mapping")
    p.add_argument("stage")
    p = sp.add_parser("disable", help="disable a mapping")
    p.add_argument("stage")

    args = ap.parse_args()
    if args.cmd == "list-stages":
        return cmd_list_stages(args)
    if args.cmd == "list-cues":
        return cmd_list_cues(args)
    if args.cmd == "list":
        return cmd_list(args)
    if args.cmd == "validate":
        return cmd_validate(args)
    if args.cmd == "add":
        return cmd_add(args)
    if args.cmd == "remove":
        return cmd_remove(args)
    if args.cmd == "enable":
        return cmd_enable(args, True)
    if args.cmd == "disable":
        return cmd_enable(args, False)
    return 1


if __name__ == "__main__":
    sys.exit(main())
