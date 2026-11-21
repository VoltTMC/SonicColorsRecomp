"""Convert a user WAV into a game-compatible CRI AAX (ADX intro + loop segments)."""

from __future__ import annotations

import shutil
import struct
import subprocess
import tempfile
import wave
from pathlib import Path

from .cricodecs_env import ensure_cricodecs, which_ffmpeg

TARGET_RATE = 48000


def _ffmpeg_convert(src: Path, dst: Path, *, channels: int, rate: int = TARGET_RATE) -> None:
    ff = which_ffmpeg()
    if not ff:
        raise RuntimeError("ffmpeg is required to resample/convert WAV → 48 kHz PCM")
    dst.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        ff, "-y", "-i", str(src),
        "-ar", str(rate), "-ac", str(channels),
        "-c:a", "pcm_s16le",
        str(dst),
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"ffmpeg failed: {r.stderr[-800:]}")


def _wav_duration_sec(path: Path) -> float:
    with wave.open(str(path), "rb") as w:
        return w.getnframes() / float(w.getframerate() or 1)


def analyze_wav(path: Path, *, silence_db: float = -40.0) -> dict:
    """Return duration/levels/leading-silence for a WAV (via ffmpeg when needed)."""
    path = Path(path)
    ff = which_ffmpeg()
    info: dict = {"path": str(path), "ok": False}
    if not path.is_file():
        info["error"] = "missing"
        return info
    # Prefer wave module for PCM s16; fall back to ffmpeg for float WAVs.
    try:
        with wave.open(str(path), "rb") as w:
            nch, sw, sr, nf = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
            raw = w.readframes(nf)
        info.update(
            {
                "sample_rate": sr,
                "channels": nch,
                "bits": sw * 8,
                "frames": nf,
                "duration_sec": nf / float(sr or 1),
                "codec": f"pcm_s{sw*8}le",
            }
        )
        if sw == 2 and nf > 0:
            samples = memoryview(raw).cast("h")
            peak = max(abs(int(s)) for s in samples)
            # RMS over all samples
            acc = 0.0
            for s in samples:
                acc += float(s) * float(s)
            rms = (acc / len(samples)) ** 0.5
            info["peak"] = peak
            info["peak_dbfs"] = 20.0 * __import__("math").log10(peak / 32768.0) if peak else -120.0
            info["rms"] = rms
            thresh = int(32768.0 * (10.0 ** (silence_db / 20.0)))
            first = None
            for i in range(0, len(samples), nch):
                frame_peak = max(abs(int(samples[i + c])) for c in range(nch))
                if frame_peak >= thresh:
                    first = (i // nch) / float(sr)
                    break
            info["first_audible_sec"] = first if first is not None else info["duration_sec"]
            info["leading_silence_sec"] = info["first_audible_sec"]
            info["leading_silence_warning"] = (info["leading_silence_sec"] or 0) >= 2.0
            info["ok"] = True
            return info
    except wave.Error:
        pass

    if not ff:
        info["error"] = "unsupported WAV (need ffmpeg for float/other codecs)"
        return info
    # ffmpeg-only probe + volume/silence analysis. Managed Windows installs ship
    # ffmpeg.exe but no ffprobe.exe, so stream metadata is parsed from ffmpeg's
    # own decode output instead.
    det = subprocess.run(
        [
            ff, "-hide_banner", "-i", str(path),
            "-af", f"volumedetect,silencedetect=noise={silence_db}dB:d=0.25",
            "-f", "null", "-",
        ],
        capture_output=True,
        text=True,
    )
    err = det.stderr or ""
    import re

    stream = re.search(r"^\s*Stream #\d+:\d+.*?Audio:\s*(.+)$", err, re.MULTILINE)
    if stream:
        desc = stream.group(1)
        info["codec"] = desc.split(",", 1)[0].split("(", 1)[0].strip()
        m = re.search(r"(\d+)\s*Hz", desc)
        if m:
            info["sample_rate"] = int(m.group(1))
        m = re.search(r"\b(mono|stereo|(?:\d+(?:\.\d+)?)\s*channels)\b", desc, re.IGNORECASE)
        if m:
            ch = m.group(1).lower()
            if ch == "mono":
                info["channels"] = 1
            elif ch == "stereo":
                info["channels"] = 2
            else:
                info["channels"] = int(float(ch.split()[0]))
        m = re.search(r"\bpcm_s(\d+)(?:le|be)\b", desc, re.IGNORECASE)
        if m:
            info["bits"] = int(m.group(1))
        elif re.search(r"\bflt[pb]?\b", desc, re.IGNORECASE):
            info["bits"] = 32
        elif re.search(r"\bu8\b", desc, re.IGNORECASE):
            info["bits"] = 8
    m = re.search(r"Duration:\s*(\d+):(\d+):(\d+(?:\.\d+)?)", err)
    if m:
        info["duration_sec"] = int(m.group(1)) * 3600 + int(m.group(2)) * 60 + float(m.group(3))
    m = re.search(r"max_volume:\s*([-\d.]+)", err)
    if m:
        info["peak_dbfs"] = float(m.group(1))
    m = re.search(r"mean_volume:\s*([-\d.]+)", err)
    if m:
        info["mean_dbfs"] = float(m.group(1))
    # first silence_end after silence_start:0 ⇒ leading silence
    lead = 0.0
    if re.search(r"silence_start:\s*0(?:\.0+)?\b", err):
        m = re.search(r"silence_end:\s*([-\d.]+)", err)
        if m:
            lead = float(m.group(1))
    info["leading_silence_sec"] = lead
    info["first_audible_sec"] = lead
    info["leading_silence_warning"] = lead >= 2.0
    info["ok"] = "peak_dbfs" in info or "duration_sec" in info
    if not info["ok"]:
        info["error"] = "ffmpeg analysis failed"
    return info


def _split_wav(src: Path, intro: Path, loop: Path, loop_start_sec: float) -> float:
    ff = which_ffmpeg()
    if not ff:
        raise RuntimeError("ffmpeg is required to split intro/loop")
    dur = _wav_duration_sec(src)
    if loop_start_sec <= 0.0 or loop_start_sec >= dur * 0.95:
        loop_start_sec = min(30.0, max(1.0, dur * 0.04))
    for args in (
        [ff, "-y", "-i", str(src), "-t", f"{loop_start_sec:.6f}", "-c", "copy", str(intro)],
        [ff, "-y", "-i", str(src), "-ss", f"{loop_start_sec:.6f}", "-c", "copy", str(loop)],
    ):
        r = subprocess.run(args, capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(f"ffmpeg split failed: {r.stderr[-400:]}")
    return loop_start_sec


def _encode_adx(wav: Path, adx: Path, *, channels: int) -> None:
    ensure_cricodecs()
    from cricodecs.adx import AdxEncodeConfig, encode
    from cricodecs.wav import Wav

    cfg = AdxEncodeConfig()
    cfg.version = 4
    cfg.encoding_mode = 3
    cfg.highpass_freq = 500
    cfg.sample_rate = TARGET_RATE
    cfg.channels = channels
    encoded = encode(Wav.load(str(wav)), cfg)
    adx.write_bytes(encoded)


def _pack_aax_segments(template: Path, segments: list[tuple[bytes, bool]], out: Path) -> dict:
    """Pack ADX segment bytes into an AAX with SNCP8P-compatible UTF alignment.

    ``segments`` is a list of (adx_bytes, loop_flag) matching the original
    template's segment count and loop pattern.
    """
    ensure_cricodecs()
    from cricodecs import utf
    from cricodecs.aax import Aax

    tmpl = Aax.load(str(template))
    if tmpl.segment_count < 1:
        raise RuntimeError(f"template {template} has no segments")
    if len(segments) != tmpl.segment_count:
        raise RuntimeError(
            f"template has {tmpl.segment_count} segments but encoder produced {len(segments)}"
        )
    for i, (seg, _) in enumerate(segments):
        if not seg.startswith(b"\x80\x00"):
            raise RuntimeError(f"ADX segment {i} missing 0x8000 signature")

    table = utf.Utf.create(table_name="AAX")
    table.add_column("data", utf.ColumnType.VLDATA)
    table.add_column("lpflg", utf.ColumnType.UINT8)
    table.set_data_alignment(8)
    for seg, loop in segments:
        table.add_row()
        idx = table.row_count - 1
        table.set(idx, "data", seg)
        table.set(idx, "lpflg", 1 if loop else 0)
    blob = bytes(table.build())

    data_field = int.from_bytes(blob[16:20], "big")
    data_abs = 8 + data_field
    if data_abs % 8 != 0:
        raise RuntimeError(f"AAX data section not 8-byte aligned (abs={data_abs})")
    a = Aax.load_bytes(blob)
    if a.segment_count != len(segments):
        raise RuntimeError("built AAX segment count mismatch")

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(blob)
    gap = 0
    if a.segment_count >= 2:
        u = utf.Utf.load_bytes(blob)
        r0, r1 = u.value(0, "data"), u.value(1, "data")
        gap = r1.offset - (r0.offset + r0.size)
    return {
        "path": str(out),
        "size": out.stat().st_size,
        "template": str(template),
        "segment_count": a.segment_count,
        "sample_rate": a.sample_rate,
        "channels": a.channels,
        "sample_count": a.sample_count,
        "has_loop_segments": a.has_loop_segments,
        "data_abs": data_abs,
        "inter_segment_gap": gap,
        "pack_method": "utf_data_alignment_8",
        "template_channels": tmpl.channels,
    }


def _pack_aax(template: Path, intro_adx: Path, loop_adx: Path, out: Path) -> dict:
    return _pack_aax_segments(
        template,
        [(intro_adx.read_bytes(), False), (loop_adx.read_bytes(), True)],
        out,
    )


def wav_to_aax(
    wav: Path,
    template_aax: Path,
    out_aax: Path,
    *,
    channels: int,
    loop_start_sec: float = 0.0,
    work_dir: Path | None = None,
    max_bytes: int | None = None,
) -> dict:
    """Build a 2-segment AAX by replacing ADX payloads in an original template.

    max_bytes is accepted for API compatibility but NEVER silently trims audio.
    If the result would exceed max_bytes, the caller must relocate in the CPK
    (see cpk_patch.inplace_replace). A clear error is raised only when max_bytes
    is set and the caller opted into fail-on-oversize via env — by default we
    always emit the full AAX and report size vs slot.
    """
    ensure_cricodecs()
    from cricodecs.aax import Aax

    wav = Path(wav)
    template_aax = Path(template_aax)
    out_aax = Path(out_aax)
    if not wav.is_file():
        raise FileNotFoundError(wav)
    if not template_aax.is_file():
        raise FileNotFoundError(template_aax)

    tmpl = Aax.load(str(template_aax))
    # Prefer template channel count when caller passes 0
    if channels <= 0:
        channels = tmpl.channels or 2

    own_tmp = work_dir is None
    tmp = Path(work_dir) if work_dir else Path(tempfile.mkdtemp(prefix="scr_aax_"))
    tmp.mkdir(parents=True, exist_ok=True)
    try:
        pcm = tmp / f"pcm_{channels}ch.wav"
        _ffmpeg_convert(wav, pcm, channels=channels)

        if tmpl.segment_count == 1:
            loop_flag = bool(tmpl.segments[0].loop_segment) if tmpl.segments else True
            adx = tmp / "full.adx"
            _encode_adx(pcm, adx, channels=channels)
            info = _pack_aax_segments(template_aax, [(adx.read_bytes(), loop_flag)], out_aax)
            info["loop_start_sec"] = 0.0
            info["intro_adx_bytes"] = 0
            info["loop_adx_bytes"] = adx.stat().st_size
        else:
            # Match original loop flags when possible (intro non-loop + body loop).
            flags = [bool(tmpl.segments[i].loop_segment) for i in range(tmpl.segment_count)]
            if tmpl.segment_count == 2 and flags == [False, True]:
                intro_wav = tmp / "intro.wav"
                loop_wav = tmp / "loop.wav"
                used_loop = _split_wav(pcm, intro_wav, loop_wav, loop_start_sec)
                intro_adx = tmp / "intro.adx"
                loop_adx = tmp / "loop.adx"
                _encode_adx(intro_wav, intro_adx, channels=channels)
                _encode_adx(loop_wav, loop_adx, channels=channels)
                info = _pack_aax_segments(
                    template_aax,
                    [(intro_adx.read_bytes(), False), (loop_adx.read_bytes(), True)],
                    out_aax,
                )
                info["loop_start_sec"] = used_loop
                info["intro_adx_bytes"] = intro_adx.stat().st_size
                info["loop_adx_bytes"] = loop_adx.stat().st_size
            else:
                # Generic: encode whole track into each segment proportionally by sample_count.
                total = sum(max(1, s.sample_count) for s in tmpl.segments) or 1
                dur = _wav_duration_sec(pcm)
                segs: list[tuple[bytes, bool]] = []
                t0 = 0.0
                ff = which_ffmpeg()
                for i, s in enumerate(tmpl.segments):
                    frac = s.sample_count / total
                    t1 = dur if i == tmpl.segment_count - 1 else t0 + dur * frac
                    part = tmp / f"part_{i}.wav"
                    r = __import__("subprocess").run(
                        [ff, "-y", "-i", str(pcm), "-ss", f"{t0:.6f}", "-to", f"{t1:.6f}",
                         "-c", "copy", str(part)],
                        capture_output=True, text=True,
                    )
                    if r.returncode != 0:
                        raise RuntimeError(f"ffmpeg segment split failed: {r.stderr[-400:]}")
                    adx = tmp / f"part_{i}.adx"
                    _encode_adx(part, adx, channels=channels)
                    segs.append((adx.read_bytes(), bool(s.loop_segment)))
                    t0 = t1
                info = _pack_aax_segments(template_aax, segs, out_aax)
                info["loop_start_sec"] = loop_start_sec
                info["intro_adx_bytes"] = len(segs[0][0]) if segs else 0
                info["loop_adx_bytes"] = len(segs[1][0]) if len(segs) > 1 else 0

        info["channels_requested"] = channels
        info["source_wav"] = str(wav)
        info["duration_scale"] = 1.0
        if max_bytes is not None:
            info["slot_bytes"] = max_bytes
            info["exceeds_slot"] = out_aax.stat().st_size > max_bytes
            if info["exceeds_slot"]:
                info["note"] = (
                    f"AAX {out_aax.stat().st_size} bytes exceeds original slot {max_bytes}; "
                    "CPK patcher will relocate (no truncation)."
                )
        return info
    finally:
        if own_tmp:
            shutil.rmtree(tmp, ignore_errors=True)


def make_diagnostic_wav(
    out: Path,
    *,
    duration_sec: float = 8.0,
    sample_rate: int = TARGET_RATE,
    freq_hz: float = 440.0,
    peak_dbfs: float = -12.0,
) -> dict:
    """Write a clearly audible stereo PCM16 diagnostic tone (no leading silence)."""
    import math

    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    n = int(duration_sec * sample_rate)
    amp = int(32767 * (10.0 ** (peak_dbfs / 20.0)))
    with wave.open(str(out), "w") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(sample_rate)
        for i in range(n):
            # Dual tone (440 + 554) so mono downmixes stay audible
            t = i / sample_rate
            s = int(amp * 0.65 * math.sin(2 * math.pi * freq_hz * t)
                    + amp * 0.35 * math.sin(2 * math.pi * (freq_hz * 5 / 4) * t))
            s = max(-32767, min(32767, s))
            w.writeframes(struct.pack("<hh", s, s))
    return analyze_wav(out)


def validate_aax(path: Path) -> dict:
    ensure_cricodecs()
    from cricodecs.aax import Aax

    a = Aax.load(str(path))
    return {
        "path": str(path),
        "segment_count": a.segment_count,
        "channels": a.channels,
        "sample_rate": a.sample_rate,
        "sample_count": a.sample_count,
        "has_loop_segments": a.has_loop_segments,
        "size": Path(path).stat().st_size,
    }
