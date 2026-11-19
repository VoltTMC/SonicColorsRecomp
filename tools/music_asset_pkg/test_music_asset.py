#!/usr/bin/env python3
"""Automated checks for WAV→AAX→CPK music asset pipeline."""

from __future__ import annotations

import json
import struct
import sys
import tempfile
import unittest
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))


class MusicAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from music_asset_pkg.cricodecs_env import ensure_cricodecs
        ensure_cricodecs()

    def test_mapping_tr1(self):
        from music_asset_pkg.mapping import STAGE_AAX
        main, fx = STAGE_AAX["stg110"]
        self.assertEqual(main, "sound/Synth/bgm_stg110_rso_wav.aax")
        self.assertTrue(fx.endswith("_fx_wav.aax"))

    def test_wav_to_aax_and_validate(self):
        from music_asset_pkg.wav_to_aax import validate_aax, wav_to_aax

        template = ROOT / "userdata" / "music_mods" / "templates" / "bgm_stg110_rso_wav.aax"
        if not template.is_file():
            template = ROOT / "out" / "p13_aax" / "bgm_stg110_rso_wav.aax"
        self.assertTrue(template.is_file(), "need TR1 AAX template (run prepare once)")

        with tempfile.TemporaryDirectory() as td:
            td = Path(td)
            wav = td / "t.wav"
            sr = 48000
            with wave.open(str(wav), "w") as w:
                w.setnchannels(2)
                w.setsampwidth(2)
                w.setframerate(sr)
                for i in range(sr * 3):
                    s = 1000 if (i // 1000) % 2 == 0 else -1000
                    w.writeframes(s.to_bytes(2, "little", signed=True) * 2)
            out = td / "out.aax"
            info = wav_to_aax(wav, template, out, channels=2, loop_start_sec=1.0, max_bytes=100)
            self.assertEqual(info["segment_count"], 2)
            self.assertEqual(info["sample_rate"], 48000)
            self.assertEqual(info["duration_scale"], 1.0)
            self.assertTrue(info.get("exceeds_slot"))
            self.assertEqual(info.get("pack_method"), "utf_data_alignment_8")
            self.assertEqual(info.get("data_abs", 1) % 8, 0)
            meta = validate_aax(out)
            self.assertEqual(meta["channels"], 2)
            self.assertTrue(meta["has_loop_segments"])
            # Must not silently shrink to fit max_bytes
            self.assertGreater(out.stat().st_size, 100)
            # SNCP8P originals use 8-byte-aligned @UTF data sections
            blob = out.read_bytes()
            data_abs = 8 + int.from_bytes(blob[16:20], "big")
            self.assertEqual(data_abs % 8, 0)
            self.assertEqual(blob[data_abs:data_abs + 2], b"\x80\x00")

    def test_cpk_toc_size_updated(self):
        from music_asset_pkg.cpk_patch import inplace_replace, sha256_file
        from cricodecs.cpk import Cpk

        src = ROOT / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk"
        if not src.is_file():
            self.skipTest("original CPK missing")
        template = ROOT / "userdata" / "music_mods" / "templates" / "bgm_stg110_rso_wav.aax"
        if not template.is_file():
            self.skipTest("template missing")

        with tempfile.TemporaryDirectory() as td:
            td = Path(td)
            # Tiny but valid AAX: reuse a previously converted short AAX if present
            short = ROOT / "userdata" / "music_mods" / "548b397f48d6911b" / "converted" / "bgm_stg110_rso_wav.aax"
            if not short.is_file():
                self.skipTest("need prepared diag AAX")
            dst = td / "sonic2010_0.cpk"
            path = "sound/Synth/bgm_stg110_rso_wav.aax"
            before = sha256_file(src)
            info = inplace_replace(src, dst, {path: short})
            after_src = sha256_file(src)
            self.assertEqual(before, after_src, "original CPK must stay untouched")
            self.assertNotEqual(before, info["dst_sha256"])
            e = next(x for x in Cpk.load(str(dst)).file_infos() if x.full_path == path)
            data = short.read_bytes()
            self.assertEqual(e.file_size, len(data))
            self.assertEqual(e.extract_size, len(data))
            self.assertEqual(Cpk.load(str(dst)).file_bytes(e.toc_index), data)
            self.assertFalse(info["entries"][0]["relocated"])

    def test_status_json_shape(self):
        st = ROOT / "userdata" / "music_mods" / "current.json"
        if not st.is_file():
            self.skipTest("no current.json yet")
        cur = json.loads(st.read_text())
        self.assertIn("patched_cpk", cur)
        status = Path(cur["status"])
        self.assertTrue(status.is_file())
        d = json.loads(status.read_text())
        self.assertTrue(d.get("ok"))
        self.assertNotEqual(d.get("original_sha256"), d.get("patched_sha256"))
        self.assertFalse(d.get("overlay", True))
        self.assertIn("payload_proof_tsv", d)
        self.assertTrue(Path(d["payload_proof_tsv"]).is_file())

    def test_catalog_music_only(self):
        from music_asset_pkg.catalog import build_catalog

        cat_path = ROOT / "mods" / "music" / "catalog.json"
        cpk = ROOT / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk"
        if not cpk.is_file():
            self.skipTest("original CPK missing")
        if cat_path.is_file():
            cat = json.loads(cat_path.read_text(encoding="utf-8"))
        else:
            cat = build_catalog(cpk_path=cpk, out_path=cat_path)
        self.assertGreaterEqual(cat["music_count"], 100)
        self.assertEqual(len(cat["archives"]), 1)
        self.assertEqual(cat["archives"][0]["name"], "sonic2010_0.cpk")
        kinds = {a.get("kind") for a in cat["assets"]}
        self.assertEqual(kinds, {"music"})
        # No voice leakage
        for a in cat["assets"]:
            fn = a["filename"].lower()
            self.assertFalse(fn.startswith("ve_") or fn.startswith("vj_"), fn)
        # Main + FX independence for TR1
        paths = {a["path"] for a in cat["assets"]}
        self.assertIn("sound/Synth/bgm_stg110_rso_wav.aax", paths)
        self.assertIn("sound/Synth/bgm_stg110_rso_fx_wav.aax", paths)
        fx = [a for a in cat["assets"] if a.get("variant") == "fx"]
        self.assertGreaterEqual(len(fx), 20)
        cats = {a["category"] for a in cat["assets"]}
        for need in ("stage", "boss", "menu", "map"):
            self.assertIn(need, cats)

    def test_preview_original_tr1(self):
        from music_asset_pkg.preview import extract_and_decode_original, which_vgmstream

        if not which_vgmstream():
            self.skipTest("vgmstream-cli missing")
        cpk = ROOT / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk"
        if not cpk.is_file():
            self.skipTest("original CPK missing")
        path = "sound/Synth/bgm_stg110_rso_wav.aax"
        with tempfile.TemporaryDirectory() as td:
            td = Path(td)
            st = extract_and_decode_original(cpk_path=cpk, internal_path=path, cache_dir=td)
            self.assertTrue(st.get("ok"), st)
            wav = Path(st["wav"])
            self.assertTrue(wav.is_file())
            self.assertGreater(wav.stat().st_size, 1000)
            # Must use original CPK path, not music_mods cache
            self.assertEqual(Path(st["cpk"]).resolve(), cpk.resolve())
            with wave.open(str(wav), "rb") as w:
                self.assertEqual(w.getframerate(), 48000)
                self.assertIn(w.getnchannels(), (1, 2))
                self.assertGreater(w.getnframes(), 48000)  # >1s
            # Cache hit
            st2 = extract_and_decode_original(cpk_path=cpk, internal_path=path, cache_dir=td)
            self.assertTrue(st2.get("cache_hit"))

    def test_export_original_complete_wav(self):
        from music_asset_pkg.export import export_original_wav, safe_export_stem
        from music_asset_pkg.preview import which_vgmstream

        if not which_vgmstream():
            self.skipTest("vgmstream-cli missing")
        cpk = ROOT / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk"
        if not cpk.is_file():
            self.skipTest("original CPK missing")
        self.assertIn("FX", safe_export_stem("Tropical Resort Act 1", "fx"))
        with tempfile.TemporaryDirectory() as td:
            td = Path(td)
            out = td / "tr1.wav"
            st = export_original_wav(
                cpk_path=cpk,
                internal_path="sound/Synth/bgm_stg110_rso_wav.aax",
                out_wav=out,
                mode="complete",
                bits=16,
                label="Tropical Resort Act 1",
                variant="main",
            )
            self.assertTrue(st.get("ok"), st)
            self.assertTrue(out.is_file())
            self.assertGreater(out.stat().st_size, 1000)
            info = st["wav_info"]
            self.assertEqual(info["sample_rate"], 48000)
            self.assertIn(info["channels"], (1, 2))
            self.assertEqual(info["pcm_bits"], 16)
            self.assertGreater(info["duration_sec"], 10)
            self.assertTrue(st.get("lossy_source"))
            # no silent overwrite
            st2 = export_original_wav(
                cpk_path=cpk,
                internal_path="sound/Synth/bgm_stg110_rso_wav.aax",
                out_wav=out,
                mode="complete",
                overwrite=False,
            )
            self.assertFalse(st2.get("ok"))

    def test_load_manifest_assets_legacy_and_resource(self):
        from music_asset_pkg.prepare import load_manifest_assets

        with tempfile.TemporaryDirectory() as td:
            td = Path(td)
            man = td / "manifest.toml"
            man.write_text(
                """
[assets.stg110_main]
archive = "sonic2010_0.cpk"
path = "sound/Synth/bgm_stg110_rso_wav.aax"
source = "tracks/a.wav"
enabled = true

[assets.stg110_fx]
archive = "sonic2010_0.cpk"
path = "sound/Synth/bgm_stg110_rso_fx_wav.aax"
source = "tracks/b.wav"
enabled = true

[tracks.stg310]
stage = "stg310"
file = "tracks/c.wav"
enabled = true
""",
                encoding="utf-8",
            )
            assets = load_manifest_assets(man)
            paths = {a["path"] for a in assets}
            self.assertIn("sound/Synth/bgm_stg110_rso_wav.aax", paths)
            self.assertIn("sound/Synth/bgm_stg110_rso_fx_wav.aax", paths)
            # Legacy stage migrates to main path
            self.assertTrue(any(a.get("_legacy") for a in assets))
            self.assertIn("sound/Synth/bgm_stg310_shp_nomal_wav.aax", paths)


if __name__ == "__main__":
    unittest.main()
