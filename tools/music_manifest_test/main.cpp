// Host-side tests for music_manifest + wav_file (no game / GUI).
#include "../../runtime/audio/music_manifest.h"
#include "../../runtime/audio/wav_file.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Stub — MusicManifest_DefaultDir only; tests use absolute temp paths.
fs::path Platform_ExeDir() { return fs::temp_directory_path(); }

static void WritePcm16Wav(const fs::path& path, int rate = 44100, int frames = 800)
{
    const int ch = 2, bps = 16;
    const uint32_t dataSize = (uint32_t)(frames * ch * (bps / 8));
    std::vector<uint8_t> pcm(dataSize);
    for (int i = 0; i < frames; i++)
    {
        int16_t s = (int16_t)(8000.0 * std::sin(2.0 * 3.141592653589793 * 440.0 * i / rate));
        pcm[i * 4] = (uint8_t)(s & 0xff);
        pcm[i * 4 + 1] = (uint8_t)((s >> 8) & 0xff);
        pcm[i * 4 + 2] = pcm[i * 4];
        pcm[i * 4 + 3] = pcm[i * 4 + 1];
    }
    auto wu16 = [](uint16_t v) {
        return std::string{ (char)(v & 0xff), (char)(v >> 8) };
    };
    auto wu32 = [](uint32_t v) {
        return std::string{ (char)(v & 0xff), (char)((v >> 8) & 0xff), (char)((v >> 16) & 0xff),
                            (char)((v >> 24) & 0xff) };
    };
    std::string fmt = wu16(1) + wu16(2) + wu32(rate) + wu32(rate * 4) + wu16(4) + wu16(16);
    uint32_t riff = 4 + (8 + (uint32_t)fmt.size()) + (8 + dataSize);
    std::ofstream out(path, std::ios::binary);
    out.write("RIFF", 4);
    out.write(wu32(riff).data(), 4);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    out.write(wu32((uint32_t)fmt.size()).data(), 4);
    out.write(fmt.data(), (std::streamsize)fmt.size());
    out.write("data", 4);
    out.write(wu32(dataSize).data(), 4);
    out.write((const char*)pcm.data(), (std::streamsize)pcm.size());
}

static int Fail(const char* msg)
{
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

int main()
{
    const fs::path root = fs::temp_directory_path() / "scr_music_manifest_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "tracks", ec);

    // Stages catalog
    if (MusicManifest_Stages().size() < 39) return Fail("stage catalog too small");
    if (!MusicManifest_FindStage("stg110")) return Fail("stg110 missing");
    if (std::strcmp(MusicManifest_DefaultCue("stg110"), "bgm_stg110_rso") != 0)
        return Fail("default cue");

    // WAV load (source outside the music dir → copied into tracks/)
    const fs::path outside = fs::temp_directory_path() / "scr_music_src_outside.wav";
    WritePcm16Wav(outside);
    WavData w;
    if (!Wav_Load(outside.string(), w)) return Fail(w.error.c_str());
    if (w.sampleRate != 44100 || w.bitsPerSample != 16 || w.sourceChannels != 2)
        return Fail("wav metadata");

    std::string err;
    std::string rel = MusicManifest_ImportWav(root, "stg110", outside, err);
    if (rel.empty()) return Fail(err.c_str());
    if (rel.find("tracks/") != 0) return Fail("expected tracks/ relative path");
    std::string rel2 = MusicManifest_ImportWav(root, "stg110", root / rel, err);
    if (rel2 != rel) return Fail("re-import should reuse managed path");

    // Save / load / status
    MusicTrack t;
    t.id = "tropical_resort_act1";
    t.stage = "stg110";
    t.cue = "bgm_stg110_rso";
    t.file = rel;
    t.enabled = true;
    t.loop = true;
    t.volume = 0.85f;
    t.fadeInMs = 500;
    t.fadeOutMs = 750;
    std::vector<MusicTrack> tracks{ t };
    if (!MusicManifest_Save(MusicManifest_Path(root), tracks, err)) return Fail(err.c_str());

    std::vector<MusicTrack> loaded;
    if (!MusicManifest_Load(MusicManifest_Path(root), loaded, err)) return Fail(err.c_str());
    if (loaded.size() != 1 || loaded[0].stage != "stg110" || loaded[0].file != rel)
        return Fail("roundtrip");

    auto st = MusicManifest_Status(root, &loaded[0]);
    if (st.kind != MusicTrackStatus::Kind::Enabled) return Fail("status enabled");

    loaded[0].enabled = false;
    st = MusicManifest_Status(root, &loaded[0]);
    if (st.kind != MusicTrackStatus::Kind::Disabled) return Fail("status disabled");

    MusicTrack missing = loaded[0];
    missing.enabled = true;
    missing.file = "tracks/nope.wav";
    st = MusicManifest_Status(root, &missing);
    if (st.kind != MusicTrackStatus::Kind::Missing) return Fail("status missing");

    // Duplicate cue rejected
    MusicTrack dup = t;
    dup.id = "dup";
    dup.file = rel;
    std::vector<MusicTrack> bad{ t, dup };
    if (MusicManifest_Save(root / "dup.toml", bad, err))
        return Fail("duplicate should fail");
    // Keep original valid
    if (!MusicManifest_Load(MusicManifest_Path(root), loaded, err) || loaded.size() != 1)
        return Fail("valid manifest preserved after failed save");

    // Unsafe path
    MusicTrack badPath = t;
    badPath.file = "../escape.wav";
    if (MusicManifest_Save(root / "bad.toml", { badPath }, err)) return Fail("unsafe path");

    // Safe filename / path traversal
    std::string safe = MusicManifest_SafeFileName("stg110", fs::path("../../evil name!.wav"));
    if (safe.find("..") != std::string::npos) return Fail("safe name traversal");
    if (safe.find('!') != std::string::npos) return Fail("safe name charset");

    // Resource-based assets API (main + FX independent)
    {
        MusicAssetMapping mainA;
        mainA.id = "stg110_main";
        mainA.archive = "sonic2010_0.cpk";
        mainA.path = "sound/Synth/bgm_stg110_rso_wav.aax";
        mainA.source = rel;
        mainA.stage = "stg110";
        mainA.variant = "main";
        mainA.enabled = true;
        MusicAssetMapping fxA = mainA;
        fxA.id = "stg110_fx";
        fxA.path = "sound/Synth/bgm_stg110_rso_fx_wav.aax";
        fxA.variant = "fx";
        fxA.source = rel; // shared WAV OK
        std::vector<MusicAssetMapping> assets{ mainA, fxA };
        if (!MusicManifest_SaveAssets(root / "assets.toml", assets, err))
            return Fail(err.c_str());
        std::vector<MusicAssetMapping> loadedA;
        if (!MusicManifest_LoadAssets(root / "assets.toml", loadedA, err) || loadedA.size() != 2)
            return Fail("assets roundtrip");
        if (!MusicManifest_FindAssetByPath(loadedA, mainA.path)
            || !MusicManifest_FindAssetByPath(loadedA, fxA.path))
            return Fail("find by path");
        auto ast = MusicManifest_AssetStatus(root, &loadedA[0]);
        if (ast.kind != MusicTrackStatus::Kind::Enabled) return Fail("asset status");
        // Duplicate path rejected
        MusicAssetMapping dupA = mainA;
        dupA.id = "dup";
        if (MusicManifest_SaveAssets(root / "dup_assets.toml", { mainA, dupA }, err))
            return Fail("duplicate asset path should fail");
    }

    // Known PCM: sample 0 near 0, sample at quarter period near peak for 440Hz @ 44100
    {
        const size_t q = 44100 / 440 / 4;   // ~25 samples
        if (w.samples.size() < (q + 1) * 2) return Fail("wav too short for peak check");
        // frame 0 ≈ 0
        if (std::fabs(w.samples[0]) > 0.05f) return Fail("pcm signedness/scale at t=0");
        // quarter cycle ≈ +peak (we used amp 8000/32768 ≈ 0.244)
        if (w.samples[q * 2] < 0.15f) return Fail("pcm peak too low — decode/scale bug");
        if (w.samples[q * 2] > 0.35f) return Fail("pcm peak too high — decode/scale bug");
    }

    std::printf("music_manifest_test OK\n");
    fs::remove_all(root, ec);
    fs::remove(outside, ec);
    return 0;
}
