#!/usr/bin/env python3
"""Automated tests for music mod WAV/manifest helpers (no gameplay)."""
from __future__ import annotations

import struct
import sys
import tempfile
import unittest
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import music_mod as mm  # noqa: E402


def write_pcm16_wav(path: Path, rate=44100, ch=2, frames=1000, freq=440.0):
    import math
    with wave.open(str(path), "w") as w:
        w.setnchannels(ch)
        w.setsampwidth(2)
        w.setframerate(rate)
        for i in range(frames):
            s = int(10000 * math.sin(2 * math.pi * freq * i / rate))
            if ch == 2:
                w.writeframes(struct.pack("<hh", s, s))
            else:
                w.writeframes(struct.pack("<h", s))


def write_float_wav(path: Path, rate=48000, frames=500):
    # Minimal IEEE float WAV
    import math
    data = bytearray()
    for i in range(frames):
        v = 0.25 * math.sin(2 * math.pi * 440 * i / rate)
        data += struct.pack("<ff", v, v)
    fmt = struct.pack("<HHIIHH", 3, 2, rate, rate * 8, 8, 32)
    riff_size = 4 + (8 + len(fmt)) + (8 + len(data))
    blob = b"RIFF" + struct.pack("<I", riff_size) + b"WAVE"
    blob += b"fmt " + struct.pack("<I", len(fmt)) + fmt
    blob += b"data" + struct.pack("<I", len(data)) + data
    path.write_bytes(blob)


class ManifestTests(unittest.TestCase):
    def test_roundtrip(self):
        with tempfile.TemporaryDirectory() as td:
            d = Path(td)
            tracks = {
                "tropical_resort_act1": {
                    "stage": "stg110", "cue": "bgm_stg110_rso", "file": "a.wav",
                    "enabled": True, "loop": True, "volume": 0.9,
                    "fade_in_ms": 500, "fade_out_ms": 750,
                }
            }
            mm.write_manifest(d / "manifest.toml", tracks)
            got = mm.parse_manifest(d / "manifest.toml")
            self.assertIn("tropical_resort_act1", got)
            self.assertEqual(got["tropical_resort_act1"]["stage"], "stg110")
            self.assertEqual(got["tropical_resort_act1"]["cue"], "bgm_stg110_rso")
            self.assertAlmostEqual(got["tropical_resort_act1"]["volume"], 0.9)

    def test_cue_derived_from_stage(self):
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "manifest.toml"
            p.write_text('[tracks.t]\nstage = "stg120"\nfile = "x.wav"\n', encoding="utf-8")
            got = mm.parse_manifest(p)
            self.assertEqual(got["t"]["cue"], "bgm_stg120_rso")

    def test_conflict_detection(self):
        with tempfile.TemporaryDirectory() as td:
            d = Path(td)
            write_pcm16_wav(d / "a.wav")
            write_pcm16_wav(d / "b.wav")
            tracks = {
                "a": {"stage": "stg110", "cue": "bgm_stg110_rso", "file": "a.wav",
                      "enabled": True, "loop": True, "volume": 1.0, "fade_in_ms": 0, "fade_out_ms": 0},
                "b": {"stage": "stg110", "cue": "bgm_stg110_rso", "file": "b.wav",
                      "enabled": True, "loop": True, "volume": 1.0, "fade_in_ms": 0, "fade_out_ms": 0},
            }
            mm.write_manifest(d / "manifest.toml", tracks)
            class A: dir = str(d)
            # validate via logic
            seen = {}
            errors = 0
            for tid, t in mm.parse_manifest(d / "manifest.toml").items():
                c = t["cue"]
                if c in seen:
                    errors += 1
                seen[c] = tid
            self.assertEqual(errors, 1)


class WavTests(unittest.TestCase):
    def test_pcm16_stereo(self):
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "t.wav"
            write_pcm16_wav(p, rate=22050, ch=2)
            ok, msg = mm.inspect_wav(p)
            self.assertTrue(ok, msg)
            self.assertIn("22050", msg)

    def test_pcm16_mono(self):
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "t.wav"
            write_pcm16_wav(p, rate=44100, ch=1)
            ok, msg = mm.inspect_wav(p)
            self.assertTrue(ok, msg)

    def test_float32(self):
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "f.wav"
            write_float_wav(p)
            ok, msg = mm.inspect_wav(p)
            self.assertTrue(ok, msg)

    def test_missing(self):
        ok, msg = mm.inspect_wav(Path("/no/such/file.wav"))
        self.assertFalse(ok)

    def test_stage_catalog(self):
        self.assertEqual(mm.STAGE_CUES["stg110"], "bgm_stg110_rso")
        self.assertEqual(len(mm.STAGE_CUES), 39)


class SelectionTests(unittest.TestCase):
    def test_enabled_filter(self):
        tracks = {
            "a": {"stage": "stg110", "cue": "bgm_stg110_rso", "enabled": False, "file": "a.wav"},
            "b": {"stage": "stg120", "cue": "bgm_stg120_rso", "enabled": True, "file": "b.wav"},
        }
        enabled = {c: t for c, t in ((t["cue"], t) for t in tracks.values()) if t["enabled"]}
        self.assertNotIn("bgm_stg110_rso", enabled)
        self.assertIn("bgm_stg120_rso", enabled)


class TracksLayoutTests(unittest.TestCase):
    def test_tracks_subdir_roundtrip(self):
        with tempfile.TemporaryDirectory() as td:
            d = Path(td)
            (d / "tracks").mkdir()
            write_pcm16_wav(d / "tracks" / "tropical_resort_act1.wav")
            tracks = {
                "tropical_resort_act1": {
                    "stage": "stg110", "cue": "bgm_stg110_rso",
                    "file": "tracks/tropical_resort_act1.wav",
                    "enabled": True, "loop": True, "volume": 0.85,
                    "fade_in_ms": 500, "fade_out_ms": 750,
                }
            }
            mm.write_manifest(d / "manifest.toml", tracks)
            got = mm.parse_manifest(d / "manifest.toml")
            self.assertEqual(got["tropical_resort_act1"]["file"], "tracks/tropical_resort_act1.wav")
            ok, msg = mm.inspect_wav(d / got["tropical_resort_act1"]["file"])
            self.assertTrue(ok, msg)

    def test_sample_repo_manifest(self):
        root = Path(__file__).resolve().parents[2]
        man = root / "mods" / "music" / "manifest.toml"
        if not man.is_file():
            self.skipTest("sample manifest missing")
        # Music Library uses [assets.*] + catalog.json; legacy host mods use [tracks.*].
        text = man.read_text(encoding="utf-8")
        if "[tracks." not in text:
            self.skipTest("no legacy [tracks.*] entries (assets-only Music Library manifest)")
        got = mm.parse_manifest(man)
        self.assertIn("tropical_resort_act1", got)
        self.assertEqual(got["tropical_resort_act1"]["stage"], "stg110")
        wav = root / "mods" / "music" / got["tropical_resort_act1"]["file"]
        self.assertTrue(wav.is_file(), f"missing {wav}")


if __name__ == "__main__":
    unittest.main()
