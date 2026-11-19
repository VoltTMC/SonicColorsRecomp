"""Non-destructive CPK payload replacement with TOC size/offset updates.

Full CPK rebuild via CriCodecs fails on SNCP8P (duplicate TOC file IDs).
We copy the archive and patch payloads + TOC FileSize/ExtractSize/(FileOffset).

Oversized replacements are relocated to the end of the archive (aligned).
Never truncate AAX payloads.
"""

from __future__ import annotations

import hashlib
import os
import shutil
import struct
import sys
from pathlib import Path

from .cricodecs_env import ensure_cricodecs


def sha256_file(path: Path, chunk: int = 8 * 1024 * 1024) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            b = f.read(chunk)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def find_entry(cpk_path: Path, internal_path: str):
    ensure_cricodecs()
    from cricodecs.cpk import Cpk

    cpk = Cpk.load(str(cpk_path))
    for e in cpk.file_infos():
        if e.full_path == internal_path or e.filename == Path(internal_path).name:
            return e
    raise KeyError(f"CPK entry not found: {internal_path}")


def _align(v: int, align: int) -> int:
    return (v + align - 1) & ~(align - 1)


def _find_toc_row(toc: bytes, file_size: int, extract_size: int, abs_offset: int) -> dict:
    """Locate TOC row fields for an uncompressed (or size-known) entry.

    Observed SNCP8P TOC row fragment (big-endian):
      FileSize | ExtractSize | 0 | FileOffset_toc | …
    where abs_offset == FileOffset_toc + bias (bias is typically 0x800).
    """
    needle = struct.pack(">I", file_size) + struct.pack(">I", extract_size)
    off = 0
    candidates = []
    while True:
        i = toc.find(needle, off)
        if i < 0:
            break
        if i + 16 <= len(toc) and toc[i + 8 : i + 12] == b"\x00\x00\x00\x00":
            toc_fo = struct.unpack_from(">I", toc, i + 12)[0]
            bias = abs_offset - toc_fo
            # Bias is small (alignment / header quirk), not a huge relative offset.
            if 0 <= bias <= 0x10000:
                candidates.append(
                    {
                        "size_off": i,
                        "extract_off": i + 4,
                        "offset_off": i + 12,
                        "toc_file_offset": toc_fo,
                        "bias": bias,
                    }
                )
        off = i + 1
    if not candidates:
        raise RuntimeError(
            f"TOC row not found for FileSize={file_size} ExtractSize={extract_size} "
            f"abs_offset={abs_offset}"
        )
    if len(candidates) > 1:
        # Prefer bias 0x800 (seen on SNCP8P audio entries).
        preferred = [c for c in candidates if c["bias"] == 0x800]
        if len(preferred) == 1:
            return preferred[0]
        raise RuntimeError(f"ambiguous TOC rows ({len(candidates)}) for size {file_size}")
    return candidates[0]


def inplace_replace(
    src_cpk: Path,
    dst_cpk: Path,
    replacements: dict[str, Path],
) -> dict:
    """
    Copy src_cpk → dst_cpk (atomic) and overwrite listed internal paths.

    - Updates TOC FileSize + ExtractSize to the replacement length.
    - If replacement fits the original slot, keeps FileOffset.
    - If replacement is larger, appends at archive end (aligned) and updates FileOffset.
    - Never truncates replacement bytes.
    """
    ensure_cricodecs()
    from cricodecs.cpk import Cpk

    src_cpk = Path(src_cpk)
    dst_cpk = Path(dst_cpk)
    if not src_cpk.is_file():
        raise FileNotFoundError(src_cpk)

    cpk = Cpk.load(str(src_cpk))
    info = cpk.info()
    content_offset = int(info.content_offset)
    align = int(info.alignment) or 2048
    by_path = {e.full_path: e for e in cpk.file_infos()}

    planned = []
    for internal, new_path in replacements.items():
        new_path = Path(new_path)
        if internal not in by_path:
            raise KeyError(f"CPK entry not found: {internal}")
        e = by_path[internal]
        data = new_path.read_bytes()
        if e.is_compressed:
            raise RuntimeError(f"{internal}: compressed CPK entry not supported for in-place patch")
        if not data.startswith(b"@UTF"):
            raise RuntimeError(f"{internal}: replacement does not look like AAX (@UTF)")
        planned.append((e, data, new_path))

    dst_cpk.parent.mkdir(parents=True, exist_ok=True)
    tmp = dst_cpk.with_suffix(dst_cpk.suffix + ".partial")
    if tmp.exists():
        tmp.unlink()
    print(f"[MUSIC-ASSET] copying CPK ({src_cpk.stat().st_size} bytes)…", file=sys.stderr)
    shutil.copy2(src_cpk, tmp)

    raw = bytearray(tmp.read_bytes())
    toc = memoryview(raw)[:content_offset]
    archive_end = len(raw)
    replaced_meta = []

    with open(tmp, "r+b") as f:
        # Re-read via file for writes; keep `raw` for TOC edits then flush TOC.
        for e, data, new_path in planned:
            row = _find_toc_row(bytes(toc), e.file_size, e.extract_size, e.file_offset)
            new_len = len(data)
            relocate = new_len > e.file_size
            if relocate:
                write_off = _align(archive_end, align)
                print(
                    f"[MUSIC-ASSET] relocate {e.full_path}: {new_len} > slot {e.file_size}; "
                    f"append @ {write_off}",
                    file=sys.stderr,
                )
                f.seek(write_off)
                f.write(data)
                # Pad to alignment for subsequent appends
                pad = _align(write_off + new_len, align) - (write_off + new_len)
                if pad:
                    f.write(b"\x00" * pad)
                archive_end = write_off + new_len + pad
                toc_fo = write_off - row["bias"]
                if toc_fo < 0:
                    raise RuntimeError(f"negative TOC FileOffset for {e.full_path}")
                struct.pack_into(">I", raw, row["offset_off"], toc_fo)
            else:
                write_off = e.file_offset
                print(
                    f"[MUSIC-ASSET] patch {e.full_path} @ {write_off} "
                    f"({new_len}/{e.file_size} bytes) ← {new_path}",
                    file=sys.stderr,
                )
                f.seek(write_off)
                f.write(data)
                # Clear remainder of old slot so stale ADX/AAX tails cannot be misread
                # if a reader ignores the updated TOC size.
                rem = e.file_size - new_len
                if rem > 0:
                    f.write(b"\x00" * rem)

            struct.pack_into(">I", raw, row["size_off"], new_len)
            struct.pack_into(">I", raw, row["extract_off"], new_len)
            replaced_meta.append(
                {
                    "path": e.full_path,
                    "toc_index": e.toc_index,
                    "old_file_offset": e.file_offset,
                    "old_file_size": e.file_size,
                    "new_file_offset": write_off if relocate else e.file_offset,
                    "new_file_size": new_len,
                    "relocated": relocate,
                    "sha256": sha256_bytes(data),
                    "toc": {
                        "size_off": row["size_off"],
                        "extract_off": row["extract_off"],
                        "offset_off": row["offset_off"],
                        "bias": row["bias"],
                    },
                }
            )

        # Flush TOC edits
        f.seek(0)
        f.write(raw[:content_offset])

    os.replace(tmp, dst_cpk)

    # Verify via CriCodecs that sizes/offsets match
    verify_cpk = Cpk.load(str(dst_cpk))
    by_new = {x.full_path: x for x in verify_cpk.file_infos()}
    for meta in replaced_meta:
        e2 = by_new[meta["path"]]
        if e2.file_size != meta["new_file_size"] or e2.extract_size != meta["new_file_size"]:
            raise RuntimeError(
                f"TOC size verify failed for {meta['path']}: "
                f"got file_size={e2.file_size} extract_size={e2.extract_size} "
                f"expected {meta['new_file_size']}"
            )
        if e2.file_offset != meta["new_file_offset"]:
            raise RuntimeError(
                f"TOC offset verify failed for {meta['path']}: "
                f"got {e2.file_offset} expected {meta['new_file_offset']}"
            )
        got = verify_cpk.file_bytes(e2.toc_index)
        if got != Path(replacements[meta["path"]]).read_bytes():
            # Compare hashes for clearer error
            raise RuntimeError(
                f"payload verify failed for {meta['path']}: "
                f"sha {sha256_bytes(got)} != {meta['sha256']}"
            )

    orig_hash = sha256_file(src_cpk)
    new_hash = sha256_file(dst_cpk)
    if orig_hash == new_hash:
        # Test B: writing the original payload back is a no-op and is allowed.
        with open(src_cpk, "rb") as sf:
            for e, data, _ in planned:
                sf.seek(e.file_offset)
                if sf.read(e.file_size) != data:
                    raise RuntimeError("patched CPK hash matches original — no bytes changed?")
    return {
        "src": str(src_cpk),
        "dst": str(dst_cpk),
        "src_sha256": orig_hash,
        "dst_sha256": new_hash,
        "size": dst_cpk.stat().st_size,
        "replaced": [m["path"] for m in replaced_meta],
        "entries": replaced_meta,
        "alignment": align,
        "content_offset": content_offset,
    }


def verify_entry(cpk_path: Path, internal_path: str) -> dict:
    ensure_cricodecs()
    from cricodecs.cpk import Cpk
    from cricodecs.aax import Aax

    cpk = Cpk.load(str(cpk_path))
    e = None
    for x in cpk.file_infos():
        if x.full_path == internal_path:
            e = x
            break
    if e is None:
        raise KeyError(internal_path)
    blob = cpk.file_bytes(e.toc_index)
    a = Aax.load_bytes(blob)
    return {
        "path": internal_path,
        "toc_index": e.toc_index,
        "file_offset": e.file_offset,
        "file_size": e.file_size,
        "extract_size": e.extract_size,
        "payload_sha256": sha256_bytes(blob),
        "aax_channels": a.channels,
        "aax_sample_rate": a.sample_rate,
        "aax_sample_count": a.sample_count,
        "aax_segments": a.segment_count,
        "aax_bytes": len(blob),
    }
