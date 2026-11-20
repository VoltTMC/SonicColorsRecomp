"""Export original Sonic Colors AAX music from the unmodified CPK to standard WAV."""

from __future__ import annotations

import json
import re
import shutil
import tempfile
import wave
from pathlib import Path

from .cpk_patch import sha256_file

# Export modes (vgmstream-driven).
# complete  — intro + loop once (-i)
# loops=N   — intro + N loop plays (-l N -f 0 -F)
# smpl      — intro + loop once with WAV smpl loop chunk (-i -L) where supported


def safe_export_stem(label: str, variant: str) -> str:
    base = re.sub(r"[^\w\-]+", "_", label.strip(), flags=re.UNICODE).strip("_")
    if not base:
        base = "track"
    var = (variant or "main").lower()
    if var and var != "main":
        base = f"{base}_{var.upper()}"
    return f"{base}_Original"[:120]


def inspect_aax_meta(aax_path: Path) -> dict:
    from .cricodecs_env import ensure_cricodecs

    ensure_cricodecs()
    from cricodecs.aax import Aax

    a = Aax.load(str(aax_path))
    segs = []
    for i, s in enumerate(a.segments):
        segs.append(
            {
                "index": i,
                "loop": bool(s.loop_segment),
                "sample_count": int(s.sample_count),
                "data_size": int(getattr(s, "data_size", 0) or 0),
            }
        )
    rate = int(a.sample_rate or 0)
    samples = int(a.sample_count or 0)
    return {
        "sample_rate": rate,
        "channels": int(a.channels or 0),
        "segment_count": int(a.segment_count or 0),
        "sample_count": samples,
        "duration_sec": (samples / rate) if rate else 0.0,
        "has_loop_segments": bool(a.has_loop_segments),
        "segments": segs,
    }


def estimate_duration_sec(meta: dict, *, mode: str, loops: float) -> float:
    rate = float(meta.get("sample_rate") or 1)
    segs = meta.get("segments") or []
    if not segs:
        return float(meta.get("duration_sec") or 0)
    intro = sum(s["sample_count"] for s in segs if not s.get("loop"))
    loop = sum(s["sample_count"] for s in segs if s.get("loop"))
    if mode == "complete" or mode == "smpl":
        return (intro + loop) / rate
    # expanded loops: intro once + loop * N
    return (intro + loop * max(0.0, float(loops))) / rate


def _validate_wav(path: Path) -> dict:
    with wave.open(str(path), "rb") as w:
        return {
            "ok": True,
            "sample_rate": w.getframerate(),
            "channels": w.getnchannels(),
            "sample_width": w.getsampwidth(),
            "frames": w.getnframes(),
            "duration_sec": w.getnframes() / float(w.getframerate() or 1),
            "pcm_bits": w.getsampwidth() * 8,
        }


def export_original_wav(
    *,
    cpk_path: Path,
    internal_path: str,
    out_wav: Path,
    mode: str = "complete",
    loops: float = 2.0,
    bits: int = 16,
    work_dir: Path | None = None,
    label: str = "",
    variant: str = "main",
    overwrite: bool = False,
) -> dict:
    """Decode an original CPK AAX to a standalone WAV.

    Always reads ``cpk_path`` (must be the unmodified game archive).
    """
    cpk_path = Path(cpk_path)
    out_wav = Path(out_wav)
    if out_wav.exists() and not overwrite:
        return {"ok": False, "error": f"output exists (refuse overwrite): {out_wav}"}
    if bits not in (16, 24):
        return {"ok": False, "error": f"unsupported PCM bit depth: {bits}"}
    if mode not in ("complete", "loops", "smpl"):
        return {"ok": False, "error": f"unknown export mode: {mode}"}

    from .doctor import probe_vgmstream, run_host_binary

    vg_info = probe_vgmstream()
    vg = vg_info.get("path") if vg_info.get("ok") else None
    if not vg:
        if vg_info.get("path") and not vg_info.get("ok"):
            return {
                "ok": False,
                "error": (
                    f"vgmstream unusable at {vg_info.get('path')}: "
                    f"{vg_info.get('error') or vg_info.get('status')}"
                ),
                "searched": vg_info.get("searched"),
            }
        return {
            "ok": False,
            "error": vg_info.get("error") or "vgmstream-cli not found",
            "searched": vg_info.get("searched"),
        }
    if not cpk_path.is_file():
        return {"ok": False, "error": f"original CPK missing: {cpk_path}"}

    from .cricodecs_env import ensure_cricodecs

    ensure_cricodecs()
    from cricodecs.cpk import Cpk

    cpk_sha = sha256_file(cpk_path)
    cpk = Cpk.load(str(cpk_path))
    entry = next((e for e in cpk.file_infos() if e.full_path == internal_path), None)
    if entry is None:
        entry = next(
            (e for e in cpk.file_infos() if e.filename == Path(internal_path).name),
            None,
        )
    if entry is None:
        return {"ok": False, "error": f"not found in CPK: {internal_path}"}

    own = work_dir is None
    tmp_root = Path(work_dir) if work_dir else Path(tempfile.mkdtemp(prefix="scr_export_"))
    tmp_root.mkdir(parents=True, exist_ok=True)
    try:
        aax_path = tmp_root / "source.aax"
        aax_path.write_bytes(cpk.file_bytes(entry.toc_index))
        meta = inspect_aax_meta(aax_path)
        expected = estimate_duration_sec(meta, mode=mode, loops=loops)

        raw_wav = tmp_root / "decoded.wav"
        cmd = [vg, "-o", str(raw_wav)]
        if mode == "complete":
            cmd += ["-i"]
        elif mode == "smpl":
            cmd += ["-i", "-L"]
        else:
            cmd += ["-l", str(loops), "-f", "0", "-F"]
        if bits == 24:
            # Prefer native sample format when requesting 24-bit; fall back to PCM16.
            cmd += ["-w"]
        cmd.append(str(aax_path))
        r = run_host_binary(cmd)
        if r.returncode != 0 or not raw_wav.is_file():
            return {
                "ok": False,
                "error": f"vgmstream decode failed: {(r.stderr or r.stdout)[-600:]}",
                "aax_meta": meta,
                "vgmstream": vg,
            }

        # Normalize to requested PCM via ffmpeg when needed (channel/layout preserved).
        final_tmp = tmp_root / "out.wav"
        from .cricodecs_env import which_ffmpeg

        ff = which_ffmpeg()
        pcm = f"pcm_s{bits}le"
        if ff:
            cmd_ff = [
                ff,
                "-y",
                "-i",
                str(raw_wav),
                "-c:a",
                pcm,
                "-ar",
                str(meta["sample_rate"] or 48000),
                str(final_tmp),
            ]
            rf = run_host_binary(cmd_ff)
            if rf.returncode != 0 or not final_tmp.is_file():
                return {
                    "ok": False,
                    "error": f"ffmpeg PCM normalize failed: {(rf.stderr or '')[-400:]}",
                    "aax_meta": meta,
                }
        else:
            shutil.copy2(raw_wav, final_tmp)

        wav_info = _validate_wav(final_tmp)
        if wav_info["frames"] < 100:
            return {"ok": False, "error": "exported WAV too short / empty", "wav": wav_info}

        out_wav.parent.mkdir(parents=True, exist_ok=True)
        # Atomic publish
        published = out_wav.with_suffix(out_wav.suffix + ".partial")
        shutil.copy2(final_tmp, published)
        published.replace(out_wav)

        return {
            "ok": True,
            "wav": str(out_wav),
            "cpk": str(cpk_path),
            "cpk_sha256": cpk_sha,
            "path": internal_path,
            "mode": mode,
            "loops": loops if mode == "loops" else (1 if mode in ("complete", "smpl") else loops),
            "bits": bits,
            "label": label,
            "variant": variant,
            "aax_meta": meta,
            "expected_duration_sec": expected,
            "wav_info": wav_info,
            "suggested_name": safe_export_stem(label or Path(internal_path).stem, variant) + ".wav",
            "lossy_source": True,  # ADX is lossy; export is faithful decode, not lossless master
            "note": "Exported from lossy CRI ADX; PCM is a decoded reconstruction, not a studio master.",
        }
    finally:
        if own:
            shutil.rmtree(tmp_root, ignore_errors=True)


def export_bulk(
    *,
    cpk_path: Path,
    catalog: Path,
    out_dir: Path,
    mode: str = "complete",
    loops: float = 2.0,
    bits: int = 16,
    overwrite: bool = False,
) -> dict:
    cat = json.loads(Path(catalog).read_text(encoding="utf-8"))
    assets = [a for a in cat.get("assets", []) if a.get("kind", "music") == "music"]
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    results = []
    ok_n = fail_n = 0
    for a in assets:
        name = safe_export_stem(a.get("label") or a.get("filename", "track"), a.get("variant") or "main")
        dest = out_dir / f"{name}.wav"
        st = export_original_wav(
            cpk_path=cpk_path,
            internal_path=a["path"],
            out_wav=dest,
            mode=mode,
            loops=loops,
            bits=bits,
            label=a.get("label") or "",
            variant=a.get("variant") or "main",
            overwrite=overwrite,
        )
        results.append({"id": a.get("id"), "path": a.get("path"), **st})
        if st.get("ok"):
            ok_n += 1
        else:
            fail_n += 1
    return {
        "ok": fail_n == 0,
        "exported": ok_n,
        "failed": fail_n,
        "total": len(assets),
        "out_dir": str(out_dir),
        "results": results,
    }
