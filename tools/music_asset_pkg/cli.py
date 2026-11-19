#!/usr/bin/env python3
"""CLI for Sonic Colors file-based music asset replacement."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="WAV→AAX→CPK music asset prep for Sonic Colors Recomp")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("prepare", help="Build patched sonic2010_0.cpk from manifest")
    p.add_argument("--manifest", type=Path, default=ROOT / "mods" / "music" / "manifest.toml")
    p.add_argument("--music-dir", type=Path, default=ROOT / "mods" / "music")
    p.add_argument("--cpk", type=Path, default=ROOT / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk")
    p.add_argument("--cache", type=Path, default=ROOT / "userdata" / "music_mods")
    p.add_argument("--no-fx", action="store_true", default=True, help="(legacy) ignored; use asset paths")
    p.add_argument("--with-fx", action="store_true", help="(legacy) ignored; use asset paths")

    v = sub.add_parser("validate-aax", help="Validate an AAX with CriCodecs")
    v.add_argument("aax", type=Path)

    i = sub.add_parser("inventory", help="List stage→AAX mappings (legacy)")
    c = sub.add_parser("catalog", help="Build mods/music/catalog.json from original CPK")
    c.add_argument("--cpk", type=Path, default=ROOT / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk")
    c.add_argument("--out", type=Path, default=ROOT / "mods" / "music" / "catalog.json")
    c.add_argument("--include-voice", action="store_true")

    pr = sub.add_parser("preview-original", help="Decode original AAX from unmodified CPK to WAV")
    pr.add_argument("--cpk", type=Path, default=ROOT / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk")
    pr.add_argument("--path", required=True, help="Internal CPK path")
    pr.add_argument("--cache", type=Path, default=ROOT / "userdata" / "music_preview")

    ex = sub.add_parser("export-original", help="Export original AAX from unmodified CPK to a standalone WAV")
    ex.add_argument("--cpk", type=Path, default=ROOT / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk")
    ex.add_argument("--path", required=True, help="Internal CPK path")
    ex.add_argument("--out", type=Path, required=True, help="Destination .wav")
    ex.add_argument("--mode", choices=("complete", "loops", "smpl"), default="complete")
    ex.add_argument("--loops", type=float, default=2.0, help="Loop repetitions for mode=loops")
    ex.add_argument("--bits", type=int, choices=(16, 24), default=16)
    ex.add_argument("--label", default="")
    ex.add_argument("--variant", default="main")
    ex.add_argument("--overwrite", action="store_true")

    bx = sub.add_parser("export-all", help="Bulk-export all catalog music assets to a directory")
    bx.add_argument("--cpk", type=Path, default=ROOT / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk")
    bx.add_argument("--catalog", type=Path, default=ROOT / "mods" / "music" / "catalog.json")
    bx.add_argument("--out-dir", type=Path, required=True)
    bx.add_argument("--mode", choices=("complete", "loops", "smpl"), default="complete")
    bx.add_argument("--loops", type=float, default=2.0)
    bx.add_argument("--bits", type=int, choices=(16, 24), default=16)
    bx.add_argument("--overwrite", action="store_true")

    args = ap.parse_args(argv)
    if args.cmd == "inventory":
        from .mapping import STAGE_AAX
        for st, (main, fx) in STAGE_AAX.items():
            print(f"{st}\t{main}\t{fx or '-'}")
        return 0
    if args.cmd == "catalog":
        from .catalog import build_catalog
        cat = build_catalog(cpk_path=args.cpk, out_path=args.out, include_voice=args.include_voice)
        print(json.dumps({"ok": True, "music_count": cat["music_count"], "out": str(args.out)}, indent=2))
        return 0
    if args.cmd == "preview-original":
        from .preview import extract_and_decode_original
        st = extract_and_decode_original(cpk_path=args.cpk, internal_path=args.path, cache_dir=args.cache)
        print(json.dumps(st, indent=2))
        return 0 if st.get("ok") else 1
    if args.cmd == "export-original":
        from .export import export_original_wav
        st = export_original_wav(
            cpk_path=args.cpk,
            internal_path=args.path,
            out_wav=args.out,
            mode=args.mode,
            loops=args.loops,
            bits=args.bits,
            label=args.label,
            variant=args.variant,
            overwrite=args.overwrite,
        )
        print(json.dumps(st, indent=2))
        return 0 if st.get("ok") else 1
    if args.cmd == "export-all":
        from .export import export_bulk
        st = export_bulk(
            cpk_path=args.cpk,
            catalog=args.catalog,
            out_dir=args.out_dir,
            mode=args.mode,
            loops=args.loops,
            bits=args.bits,
            overwrite=args.overwrite,
        )
        print(json.dumps({k: st[k] for k in st if k != "results"}, indent=2))
        # Keep results available but truncated in stdout for readability
        print(json.dumps({"failures": [r for r in st["results"] if not r.get("ok")][:20]}, indent=2))
        return 0 if st.get("ok") else 1
    if args.cmd == "validate-aax":
        from .wav_to_aax import validate_aax
        print(json.dumps(validate_aax(args.aax), indent=2))
        return 0
    if args.cmd == "prepare":
        from .prepare import prepare
        st = prepare(
            manifest=args.manifest,
            music_dir=args.music_dir,
            original_cpk=args.cpk,
            cache_root=args.cache,
        )
        print(json.dumps(st, indent=2))
        return 0 if st.get("ok") else 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
