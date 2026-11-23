#!/usr/bin/env python3
"""Merge area-tagged A3w collect/*.bin key dumps into vs_keys.bin / ps_keys.bin.

Usage:
  tools/shader_collect_merge.py <shadercache_dir> [--report]

Reads shadercache/collect/<area>_vs_keys.bin and <area>_ps_keys.bin written when
SCR_SHADER_COLLECT=1, deduplicates by raw key bytes, and appends any new keys
into the root vs_keys.bin / ps_keys.bin. Does not invent shaders.

Does NOT claim complete game coverage — prints per-area unique counts only.
"""
from __future__ import annotations

import argparse
import struct
import sys
from collections import defaultdict
from pathlib import Path

# Must match runtime/gx/shadergen.h layouts used by RememberKey (all uint32_t fields).
VS_KEY_SIZE = 96   # 24 × uint32
PS_KEY_SIZE = 300  # 75 × uint32


def unique_keys(path: Path, key_size: int) -> tuple[list[bytes], int]:
    if not path.is_file():
        return [], 0
    data = path.read_bytes()
    if key_size <= 0 or len(data) % key_size != 0:
        print(f"WARN: {path}: size {len(data)} not multiple of {key_size}", file=sys.stderr)
        n = len(data) // key_size if key_size else 0
        data = data[: n * key_size]
    seen: dict[bytes, None] = {}
    order: list[bytes] = []
    total = 0
    for i in range(0, len(data), key_size):
        total += 1
        k = data[i : i + key_size]
        if k not in seen:
            seen[k] = None
            order.append(k)
    return order, total


def merge_kind(cache: Path, collect: Path, name: str, key_size: int) -> dict:
    root = cache / name
    existing, root_total = unique_keys(root, key_size)
    have = {k: None for k in existing}
    by_area: dict[str, dict] = {}
    added = 0
    for p in sorted(collect.glob(f"*_{name}")):
        area = p.name[: -len(f"_{name}")]
        keys, total = unique_keys(p, key_size)
        new = 0
        for k in keys:
            if k not in have:
                have[k] = None
                existing.append(k)
                new += 1
                added += 1
        by_area[area] = {"file_entries": total, "unique": len(keys), "new_to_root": new}
    if added:
        root.write_bytes(b"".join(existing))
    return {
        "root_before": root_total,
        "root_unique": len(existing),
        "added": added,
        "areas": by_area,
    }


def detect_ps_key_size(cache: Path, collect: Path) -> int:
    for p in [cache / "ps_keys.bin", *sorted(collect.glob("*_ps_keys.bin"))]:
        if p.is_file() and p.stat().st_size > 0:
            sz = p.stat().st_size
            for cand in (PS_KEY_SIZE, 292, 296, 304):
                if sz % cand == 0:
                    return cand
            return PS_KEY_SIZE
    return PS_KEY_SIZE


def detect_vs_key_size(cache: Path, collect: Path) -> int:
    for p in [cache / "vs_keys.bin", *sorted(collect.glob("*_vs_keys.bin"))]:
        if p.is_file() and p.stat().st_size > 0:
            sz = p.stat().st_size
            for cand in (VS_KEY_SIZE, 100, 104, 108):
                if sz % cand == 0:
                    return cand
            return VS_KEY_SIZE
    return VS_KEY_SIZE


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("shadercache_dir", type=Path)
    ap.add_argument("--report", action="store_true", help="print only; still merges if new keys")
    ap.add_argument("--dry-run", action="store_true", help="do not write root key files")
    args = ap.parse_args()
    cache: Path = args.shadercache_dir
    collect = cache / "collect"
    if not cache.is_dir():
        print(f"not a directory: {cache}", file=sys.stderr)
        return 1
    if not collect.is_dir():
        print(f"no collect/ under {cache} — run with SCR_SHADER_COLLECT=1 first")
        return 0

    vs_sz = detect_vs_key_size(cache, collect)
    ps_sz = detect_ps_key_size(cache, collect)
    print(f"A3w collect merge: {cache}")
    print(f"  key sizes: VS={vs_sz} PS={ps_sz} (must match runtime sizeof)")

    if args.dry_run:
        # summarize without write
        for name, sz in (("vs_keys.bin", vs_sz), ("ps_keys.bin", ps_sz)):
            keys, total = unique_keys(cache / name, sz)
            print(f"  {name}: root_entries≈{total} unique={len(keys)}")
            for p in sorted(collect.glob(f"*_{name}")):
                k, t = unique_keys(p, sz)
                print(f"    {p.name}: entries={t} unique={len(k)}")
        print("  dry-run: no files written")
        print("  NOTE: coverage is incomplete until all areas are collected.")
        return 0

    vs = merge_kind(cache, collect, "vs_keys.bin", vs_sz)
    ps = merge_kind(cache, collect, "ps_keys.bin", ps_sz)
    print(f"  VS: unique={vs['root_unique']} added={vs['added']} areas={vs['areas']}")
    print(f"  PS: unique={ps['root_unique']} added={ps['added']} areas={ps['areas']}")
    print("  NOTE: this does NOT claim complete game shader coverage.")
    print("  Next: SonicColorsRecomp --precompile-shaders <dir>")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
