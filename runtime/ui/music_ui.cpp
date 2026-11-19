#include "music_ui.h"
#include "../audio/music_catalog.h"
#include "../audio/music_manifest.h"
#include "../audio/wav_file.h"
#include "../config.h"
#include "../log.h"
#include "../platform/platform.h"
#include "../platform/ui.h"

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <SDL3/SDL.h>
#endif

namespace fs = std::filesystem;

namespace
{
fs::path s_dir;
MusicCatalog s_catalog;
std::vector<MusicAssetMapping> s_assets;
bool s_loaded = false;
std::string s_statusMsg;
std::string s_help;

char s_search[128] = {};
int s_filterCat = 0;
int s_filterVar = 0;
int s_filterRepl = 0;
int s_filterWorld = 0;
const char* kCats[] = {"All", "stage", "boss", "menu", "map", "jingle", "special", "other"};
const char* kWorlds[] = {
    "All", "Tropical Resort", "Sweet Mountain", "Starlight Carnival",
    "Planet Wisp", "Aquarium Park", "Asteroid Coaster", "Terminal Velocity",
};

enum class PreviewKind { None, Original, Replacement };
PreviewKind s_prevKind = PreviewKind::None;
#ifndef _WIN32
SDL_AudioStream* s_prevStream = nullptr;
SDL_AudioDeviceID s_prevDev = 0;
#endif
WavData s_prevWav;
std::mutex s_prevMu;
size_t s_prevCursor = 0;
bool s_prevPlaying = false;
bool s_prevPaused = false;
float s_prevVolume = 0.8f;
bool s_prevLoop = true;
std::string s_prevLabel;
std::string s_prevPath;

// Async original decode — never block the ImGui thread on vgmstream/CPK extract.
std::atomic<bool> s_decodeRunning{false};
std::mutex s_decodeMu;
std::string s_decodeWav;
std::string s_decodeLabel;
std::string s_decodeError;
std::string s_decodeAssetId;

std::atomic<bool> s_prepRunning{false};
std::string s_prepLog;
std::mutex s_prepMu;
std::string s_cacheSummary;
std::mutex s_cacheMu;

std::string s_selectedId;
std::string s_editSyncedId;
double s_editLoopStart = 0.0;
std::string s_prevSelectedId; // stop preview when selection changes

// Export Original as WAV
int s_exportMode = 0;   // 0=complete 1=loops 2=smpl
int s_exportBits = 0;   // 0=16 1=24
float s_exportLoops = 2.f;
std::atomic<bool> s_exportRunning{false};
std::mutex s_exportMu;
std::string s_exportStatus;
std::string s_lastExportPath;

void Hover(const char* t)
{
    if (ImGui::IsItemHovered() || ImGui::IsItemFocused()) s_help = t;
}

fs::path MusicDir()
{
    if (!g_config.musicModsDir.empty()) return g_config.musicModsDir;
    return MusicManifest_DefaultDir();
}

fs::path FindMusicAssetTool()
{
    if (const char* env = getenv("SCR_MUSIC_ASSET"))
    {
        fs::path p(env);
        if (fs::exists(p)) return p;
    }
    const fs::path root = Platform_ExeDir();
    for (const fs::path& base : { fs::current_path(), root, root / "..", root / "../.." })
    {
        fs::path p = base / "tools" / "music_asset";
        std::error_code ec;
        if (fs::exists(p, ec)) return fs::weakly_canonical(p, ec);
    }
    return {};
}

fs::path OriginalCpk()
{
    fs::path p = fs::path(g_config.dataDir) / "files" / "sonic2010_0.cpk";
    if (fs::exists(p)) return p;
    p = fs::current_path() / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk";
    if (fs::exists(p)) return p;
    p = Platform_ExeDir() / ".." / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk";
    return p;
}

fs::path EnsureWritableDir(const fs::path& preferred, const fs::path& fallback)
{
    std::error_code ec;
    fs::create_directories(preferred, ec);
    if (fs::is_directory(preferred, ec)) return preferred;
    fs::create_directories(fallback, ec);
    return fallback;
}

fs::path CacheRoot()
{
    return EnsureWritableDir(fs::current_path() / "userdata" / "music_mods",
                             Platform_UserDir() / "music_mods");
}
fs::path PreviewCache()
{
    return EnsureWritableDir(fs::current_path() / "userdata" / "music_preview",
                             Platform_UserDir() / "music_preview");
}

void RefreshCacheSummary()
{
    std::string summary;
    const fs::path cur = CacheRoot() / "current.json";
    if (!fs::exists(cur))
        summary = "CPK cache: not built";
    else
        summary = "CPK cache ready (" + cur.string() + ")";
    std::lock_guard lk(s_cacheMu);
    s_cacheSummary = std::move(summary);
}

MusicAssetMapping* FindMapping(const std::string& path)
{
    return MusicManifest_FindAssetByPath(s_assets, path);
}

void StopPreview()
{
    std::lock_guard lk(s_prevMu);
    s_prevPlaying = false;
    s_prevPaused = false;
    s_prevCursor = 0;
    s_prevKind = PreviewKind::None;
#ifndef _WIN32
    if (s_prevStream)
    {
        SDL_ClearAudioStream(s_prevStream);
        SDL_DestroyAudioStream(s_prevStream);
        s_prevStream = nullptr;
    }
    if (s_prevDev)
    {
        SDL_CloseAudioDevice(s_prevDev);
        s_prevDev = 0;
    }
#endif
    s_prevWav = {};
    s_prevPath.clear();
    s_prevLabel.clear();
}

#ifndef _WIN32
void PumpPreview()
{
    std::lock_guard lk(s_prevMu);
    if (!s_prevPlaying || s_prevPaused || !s_prevStream || s_prevWav.samples.empty()) return;
    const int rate = s_prevWav.sampleRate > 0 ? s_prevWav.sampleRate : 44100;
    const int want = rate / 5;
    int queued = SDL_GetAudioStreamQueued(s_prevStream) / (int)(sizeof(float) * 2);
    if (queued > want) return;
    const size_t frames = s_prevWav.samples.size() / 2;
    if (s_prevCursor >= frames)
    {
        if (s_prevLoop) s_prevCursor = 0;
        else { s_prevPlaying = false; return; }
    }
    const size_t n = std::min((size_t)want, frames - s_prevCursor);
    std::vector<float> buf(n * 2);
    for (size_t i = 0; i < n; i++)
    {
        buf[i * 2] = s_prevWav.samples[(s_prevCursor + i) * 2] * s_prevVolume;
        buf[i * 2 + 1] = s_prevWav.samples[(s_prevCursor + i) * 2 + 1] * s_prevVolume;
    }
    SDL_PutAudioStreamData(s_prevStream, buf.data(), (int)(buf.size() * sizeof(float)));
    s_prevCursor += n;
}
#endif

bool StartPreviewWav(const fs::path& path, PreviewKind kind, const std::string& label)
{
    StopPreview();
#ifdef _WIN32
    s_statusMsg = "Opening with system player…";
    Shell_OpenUrl(path.string());
    return false;
#else
    WavData w;
    if (!Wav_Load(path.string(), w))
    {
        s_statusMsg = "Preview failed: " + w.error;
        return false;
    }
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        s_statusMsg = std::string("SDL audio: ") + SDL_GetError();
        return false;
    }
    s_prevDev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!s_prevDev)
    {
        s_statusMsg = std::string("No playback device: ") + SDL_GetError();
        return false;
    }
    SDL_AudioSpec spec{ SDL_AUDIO_F32, 2, w.sampleRate };
    s_prevStream = SDL_CreateAudioStream(&spec, nullptr);
    if (!s_prevStream || !SDL_BindAudioStream(s_prevDev, s_prevStream))
    {
        s_statusMsg = std::string("Audio stream: ") + SDL_GetError();
        StopPreview();
        return false;
    }
    {
        std::lock_guard lk(s_prevMu);
        s_prevWav = std::move(w);
        s_prevCursor = 0;
        s_prevPlaying = true;
        s_prevPaused = false;
        s_prevPath = path.string();
        s_prevLabel = label;
        s_prevKind = kind;
    }
    PumpPreview();
    s_statusMsg = kind == PreviewKind::Original ? "Playing original preview"
                                                : "Playing replacement preview";
    return true;
#endif
}

void TogglePause(PreviewKind kind)
{
    std::lock_guard lk(s_prevMu);
    if (s_prevKind != kind || !s_prevPlaying) return;
    s_prevPaused = !s_prevPaused;
#ifndef _WIN32
    if (s_prevPaused && s_prevStream) SDL_ClearAudioStream(s_prevStream);
#endif
}

void RequestOriginalPreview(const MusicCatalogAsset& asset)
{
    if (s_decodeRunning.load())
    {
        s_statusMsg = "Original preview already decoding…";
        return;
    }
    const fs::path tool = FindMusicAssetTool();
    const fs::path cpk = OriginalCpk();
    if (tool.empty() || !fs::exists(cpk))
    {
        s_statusMsg = "Cannot preview original: music_asset tool or CPK missing";
        return;
    }
    {
        std::lock_guard lk(s_decodeMu);
        s_decodeWav.clear();
        s_decodeError.clear();
        s_decodeLabel = "Original · " + asset.label;
        s_decodeAssetId = asset.id;
    }
    s_statusMsg = "Decoding original AAX…";
    s_decodeRunning = true;
    const std::string path = asset.path;
    const std::string label = asset.label;
    std::thread([tool, cpk, path, label] {
        const fs::path cache = PreviewCache();
        fs::create_directories(cache);
        const fs::path outJson = cache / "last_preview.json";
        const fs::path outErr = cache / "last_preview.err";
        std::string cmd = "\"" + tool.string() + "\" preview-original"
                          " --cpk \"" + cpk.string() + "\""
                          " --path \"" + path + "\""
                          " --cache \"" + cache.string() + "\""
                          " > \"" + outJson.string() + "\" 2> \"" + outErr.string() + "\"";
        const int rc = std::system(cmd.c_str());
        std::ifstream in(outJson);
        std::string js((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto get = [&](const char* key) -> std::string {
            const std::string pat = std::string("\"") + key + "\"";
            auto pos = js.find(pat);
            if (pos == std::string::npos) return {};
            pos = js.find(':', pos);
            if (pos == std::string::npos) return {};
            pos = js.find('"', pos);
            if (pos == std::string::npos) return {};
            auto end = js.find('"', pos + 1);
            if (end == std::string::npos) return {};
            return js.substr(pos + 1, end - pos - 1);
        };
        const bool okJson = js.find("\"ok\": true") != std::string::npos
                            || js.find("\"ok\":true") != std::string::npos;
        std::lock_guard lk(s_decodeMu);
        if (rc != 0 || !okJson)
        {
            s_decodeError = get("error");
            if (s_decodeError.empty())
            {
                std::ifstream er(outErr);
                s_decodeError.assign((std::istreambuf_iterator<char>(er)), std::istreambuf_iterator<char>());
                if (s_decodeError.size() > 240) s_decodeError.resize(240);
            }
            if (s_decodeError.empty()) s_decodeError = "decode failed";
        }
        else
        {
            s_decodeWav = get("wav");
            s_decodeLabel = "Original · " + label;
            if (s_decodeWav.empty()) s_decodeError = "Original preview produced no WAV";
        }
        s_decodeRunning = false;
    }).detach();
}

void PollAsyncDecode()
{
    if (s_decodeRunning.load()) return;
    std::string wav, label, err;
    {
        std::lock_guard lk(s_decodeMu);
        if (s_decodeWav.empty() && s_decodeError.empty()) return;
        wav = std::move(s_decodeWav);
        label = std::move(s_decodeLabel);
        err = std::move(s_decodeError);
        s_decodeWav.clear();
        s_decodeLabel.clear();
        s_decodeError.clear();
    }
    if (!err.empty())
    {
        s_statusMsg = "Original preview failed: " + err;
        return;
    }
    if (!wav.empty())
        StartPreviewWav(wav, PreviewKind::Original, label);
}

void Reload()
{
    s_dir = MusicDir();
    std::string err;
    s_catalog = {};
    if (!MusicCatalog_Load(MusicCatalog_DefaultPath(s_dir), s_catalog, err))
    {
        const fs::path tool = FindMusicAssetTool();
        if (!tool.empty() && fs::exists(OriginalCpk()))
        {
            std::string cmd = "\"" + tool.string() + "\" catalog --cpk \"" + OriginalCpk().string()
                              + "\" --out \"" + MusicCatalog_DefaultPath(s_dir).string() + "\"";
            std::system(cmd.c_str());
            err.clear();
            MusicCatalog_Load(MusicCatalog_DefaultPath(s_dir), s_catalog, err);
        }
        if (s_catalog.assets.empty())
            s_statusMsg = "Catalog missing — run: tools/music_asset catalog";
    }
    s_assets.clear();
    if (!MusicManifest_LoadAssets(MusicManifest_Path(s_dir), s_assets, err))
    {
        if (err == "manifest not found")
        {
            s_assets.clear();
            s_loaded = true;
            s_statusMsg = std::format("{} music assets · no replacements yet", s_catalog.musicCount);
        }
        else s_statusMsg = "Manifest: " + err;
    }
    else
    {
        for (auto& a : s_assets)
        {
            if (!a.path.empty()) continue;
            if (a.stage.empty()) continue;
            for (auto& c : s_catalog.assets)
            {
                if (c.stage == a.stage && c.variant == (a.variant.empty() ? "main" : a.variant))
                {
                    a.path = c.path;
                    if (a.cue.empty()) a.cue = c.cue;
                    if (a.id.empty() || a.id.rfind("legacy_", 0) == 0)
                        a.id = c.stage + "_" + c.variant;
                    break;
                }
            }
        }
        s_statusMsg = std::format("{} music assets · {} replacement(s)",
                                  s_catalog.musicCount, s_assets.size());
    }
    s_loaded = true;
    RefreshCacheSummary();
}

bool SaveAssets()
{
    std::vector<MusicAssetMapping> clean;
    for (auto& a : s_assets)
        if (!a.path.empty() && !a.source.empty()) clean.push_back(a);
    s_assets.swap(clean);
    std::string err;
    if (!MusicManifest_SaveAssets(MusicManifest_Path(s_dir), s_assets, err))
    {
        s_statusMsg = "Save failed: " + err;
        return false;
    }
    s_statusMsg = "Saved " + MusicManifest_Path(s_dir).string();
    return true;
}

void StartPrepareAsync()
{
    if (s_prepRunning.exchange(true)) return;
    {
        std::lock_guard lk(s_prepMu);
        s_prepLog.clear();
    }
    std::thread([] {
        const fs::path tool = FindMusicAssetTool();
        const fs::path musicDir = MusicDir();
        const fs::path manifest = MusicManifest_Path(musicDir);
        const fs::path cache = CacheRoot();
        const fs::path cpk = OriginalCpk();
        fs::create_directories(cache);
        const fs::path outJson = cache / "prepare_gui.json";
        const fs::path outErr = cache / "prepare_gui.err";
        int rc = 1;
        std::string log;
        if (tool.empty()) log = "tools/music_asset not found";
        else if (!fs::exists(manifest)) log = "manifest.toml missing";
        else if (!fs::exists(cpk)) log = "sonic2010_0.cpk not found";
        else
        {
            std::string cmd = "\"" + tool.string() + "\" prepare"
                              " --manifest \"" + manifest.string() + "\""
                              " --music-dir \"" + musicDir.string() + "\""
                              " --cpk \"" + cpk.string() + "\""
                              " --cache \"" + cache.string() + "\""
                              " > \"" + outJson.string() + "\" 2> \"" + outErr.string() + "\"";
            rc = std::system(cmd.c_str());
            std::ifstream ej(outJson), er(outErr);
            std::string js((std::istreambuf_iterator<char>(ej)), std::istreambuf_iterator<char>());
            std::string es((std::istreambuf_iterator<char>(er)), std::istreambuf_iterator<char>());
            log = es;
            if (!js.empty())
            {
                if (!log.empty()) log += "\n";
                log += js.size() > 1500 ? js.substr(0, 1500) + "…" : js;
            }
            if (rc != 0) log = "PREPARE FAILED (rc=" + std::to_string(rc) + ")\n" + log;
        }
        {
            std::lock_guard lk(s_prepMu);
            s_prepLog = std::move(log);
            s_statusMsg = (rc == 0) ? "Prepare succeeded" : "Prepare failed — see log";
        }
        RefreshCacheSummary();
        s_prepRunning = false;
    }).detach();
}

bool PassesFilters(const MusicCatalogAsset& a, const MusicAssetMapping* map)
{
    if (s_filterCat > 0 && a.category != kCats[s_filterCat]) return false;
    if (s_filterVar == 1 && a.variant != "main") return false;
    if (s_filterVar == 2 && a.variant != "fx") return false;
    if (s_filterWorld > 0 && a.world != kWorlds[s_filterWorld]) return false;
    const bool replaced = map && map->enabled && !map->source.empty();
    if (s_filterRepl == 1 && !replaced) return false;
    if (s_filterRepl == 2 && replaced) return false;
    if (s_search[0])
    {
        std::string q = s_search;
        for (char& c : q) c = (char)tolower((unsigned char)c);
        auto has = [&](const std::string& s) {
            std::string t = s;
            for (char& c : t) c = (char)tolower((unsigned char)c);
            return t.find(q) != std::string::npos;
        };
        if (!has(a.label) && !has(a.filename) && !has(a.path) && !has(a.cue)
            && !has(a.stage) && !has(a.world))
            return false;
    }
    return true;
}

void DrawPreviewTransport()
{
#ifndef _WIN32
    PumpPreview();
#endif
    bool wantStop = false;
    PreviewKind kind = PreviewKind::None;
    std::string label;
    double pos = 0, dur = 0;
    float frac = 0.f;
    bool playing = false, paused = false;
    {
        std::lock_guard lk(s_prevMu);
        kind = s_prevKind;
        if (kind == PreviewKind::None && s_prevPath.empty()) return;
        label = s_prevLabel.empty() ? s_prevPath : s_prevLabel;
        playing = s_prevPlaying;
        paused = s_prevPaused;
        const size_t frames = s_prevWav.samples.size() / 2;
        const double rate = s_prevWav.sampleRate > 0 ? s_prevWav.sampleRate : 1;
        dur = frames / rate;
        pos = s_prevCursor / rate;
        frac = dur > 0 ? (float)(pos / dur) : 0.f;
    }

    const char* kindName = kind == PreviewKind::Original ? "Original"
                         : kind == PreviewKind::Replacement ? "Replacement" : "Preview";
    ImGui::BeginChild("preview_transport", ImVec2(0, ImGui::GetFrameHeightWithSpacing() * 3.6f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::Text("%s preview", kindName);
    ImGui::SameLine();
    ImGui::TextDisabled("%.1f / %.1f s", pos, dur);
    ImGui::TextWrapped("%s", label.c_str());
    if (ImGui::SliderFloat("##seek", &frac, 0.f, 1.f, ""))
    {
        std::lock_guard lk(s_prevMu);
        const size_t frames = s_prevWav.samples.size() / 2;
        s_prevCursor = (size_t)(frac * (double)frames);
#ifndef _WIN32
        if (s_prevStream) SDL_ClearAudioStream(s_prevStream);
#endif
    }
    Hover("Seek within the active preview.");
    if (ImGui::Button(paused || !playing ? "Resume" : "Pause"))
    {
        std::lock_guard lk(s_prevMu);
        if (!s_prevPlaying) { s_prevPlaying = true; s_prevPaused = false; }
        else s_prevPaused = !s_prevPaused;
#ifndef _WIN32
        if (s_prevPaused && s_prevStream) SDL_ClearAudioStream(s_prevStream);
#endif
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop preview")) wantStop = true;
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &s_prevLoop);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderFloat("Vol", &s_prevVolume, 0.f, 1.f, "%.2f");
    ImGui::EndChild();
    if (wantStop) StopPreview();
}

void DrawFilterBar(float availW)
{
    const float comboW = std::clamp(availW * 0.18f, ImGui::GetFontSize() * 7.f, ImGui::GetFontSize() * 12.f);
    ImGui::SetNextItemWidth(std::min(availW, ImGui::GetFontSize() * 18.f));
    ImGui::InputTextWithHint("##search", "Search label / file / cue / stage", s_search, sizeof(s_search));

    auto maybeBreak = [&](float need) {
        if (ImGui::GetContentRegionAvail().x < need)
            ImGui::NewLine();
        else
            ImGui::SameLine();
    };
    maybeBreak(comboW + 80);
    ImGui::SetNextItemWidth(comboW);
    ImGui::Combo("Category", &s_filterCat, kCats, IM_ARRAYSIZE(kCats));
    maybeBreak(comboW + 80);
    ImGui::SetNextItemWidth(comboW * 0.85f);
    ImGui::Combo("Variant", &s_filterVar, "All\0Main\0FX\0");
    maybeBreak(comboW + 80);
    ImGui::SetNextItemWidth(comboW);
    ImGui::Combo("Replaced", &s_filterRepl, "All\0Replaced\0Original\0");
    maybeBreak(comboW + 80);
    ImGui::SetNextItemWidth(comboW * 1.2f);
    ImGui::Combo("World", &s_filterWorld, kWorlds, IM_ARRAYSIZE(kWorlds));
}

std::string SuggestedExportName(const MusicCatalogAsset& asset)
{
    std::string base = asset.label.empty() ? asset.filename : asset.label;
    for (char& c : base)
        if (!isalnum((unsigned char)c) && c != '-' && c != '_') c = '_';
    while (!base.empty() && base.back() == '_') base.pop_back();
    if (base.empty()) base = "track";
    if (asset.variant == "fx") base += "_FX";
    return base + "_Original.wav";
}

void RequestExportOriginal(const MusicCatalogAsset& asset)
{
    if (s_exportRunning.exchange(true))
    {
        s_statusMsg = "Export already running…";
        return;
    }
    const fs::path tool = FindMusicAssetTool();
    const fs::path cpk = OriginalCpk();
    if (tool.empty() || !fs::exists(cpk))
    {
        s_exportRunning = false;
        s_statusMsg = "Cannot export: music_asset tool or CPK missing";
        return;
    }
    fs::path dest;
    if (!Dialog_SaveWavFile(dest, SuggestedExportName(asset)))
    {
        s_exportRunning = false;
        return;
    }
    if (fs::exists(dest))
    {
        // Native dialogs may not confirm; refuse silent overwrite.
        s_statusMsg = "Export refused: file exists — choose another name or delete it first";
        s_exportRunning = false;
        return;
    }
    const char* modes[] = {"complete", "loops", "smpl"};
    const std::string mode = modes[std::clamp(s_exportMode, 0, 2)];
    const int bits = s_exportBits == 1 ? 24 : 16;
    const float loops = s_exportLoops;
    const std::string path = asset.path;
    const std::string label = asset.label;
    const std::string variant = asset.variant;
    {
        std::lock_guard lk(s_exportMu);
        s_exportStatus = "Exporting…";
    }
    std::thread([tool, cpk, dest, path, label, variant, mode, bits, loops] {
        const fs::path outJson = PreviewCache() / "last_export.json";
        const fs::path outErr = PreviewCache() / "last_export.err";
        fs::create_directories(PreviewCache());
        std::string cmd = "\"" + tool.string() + "\" export-original"
                          " --cpk \"" + cpk.string() + "\""
                          " --path \"" + path + "\""
                          " --out \"" + dest.string() + "\""
                          " --mode " + mode
                          + " --loops " + std::to_string(loops)
                          + " --bits " + std::to_string(bits)
                          + " --label \"" + label + "\""
                          + " --variant \"" + variant + "\""
                          " > \"" + outJson.string() + "\" 2> \"" + outErr.string() + "\"";
        const int rc = std::system(cmd.c_str());
        std::ifstream in(outJson);
        std::string js((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const bool ok = rc == 0 && (js.find("\"ok\": true") != std::string::npos
                                    || js.find("\"ok\":true") != std::string::npos);
        std::lock_guard lk(s_exportMu);
        if (ok)
        {
            s_exportStatus = "Exported: " + dest.string();
            s_lastExportPath = dest.string();
            s_statusMsg = s_exportStatus;
        }
        else
        {
            std::ifstream er(outErr);
            std::string es((std::istreambuf_iterator<char>(er)), std::istreambuf_iterator<char>());
            s_exportStatus = "Export failed: " + (es.empty() ? js.substr(0, 200) : es.substr(0, 200));
            s_statusMsg = s_exportStatus;
        }
        s_exportRunning = false;
    }).detach();
}

void DrawPlayerPanel(const MusicCatalogAsset& asset, MusicAssetMapping* map, bool launcher)
{
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.25f);
    ImGui::TextWrapped("%s", asset.label.c_str());
    ImGui::PopFont();
    ImGui::TextDisabled("%s · %s · %s",
                        asset.category.c_str(),
                        asset.variant.c_str(),
                        asset.stage.empty() ? (asset.world.empty() ? "—" : asset.world.c_str())
                                            : asset.stage.c_str());
    ImGui::TextDisabled("Original file: %s", asset.filename.c_str());
    if (map && !map->source.empty())
        ImGui::Text("Replacement: %s", fs::path(map->source).filename().string().c_str());
    else
        ImGui::TextDisabled("Replacement: (none)");
    ImGui::TextDisabled("%.1fs · %d Hz · %d ch · %d seg",
                        asset.durationSec, asset.sampleRate, asset.channels, asset.segmentCount);

    if (!launcher)
    {
        ImGui::TextWrapped("Preview and export are available in the launcher.");
        return;
    }

    const bool decoding = s_decodeRunning.load();
    const bool exporting = s_exportRunning.load();
    const bool hasRepl = map && !map->source.empty();
    bool origActive = false, replActive = false, paused = false;
    {
        std::lock_guard lk(s_prevMu);
        origActive = s_prevKind == PreviewKind::Original;
        replActive = s_prevKind == PreviewKind::Replacement;
        paused = s_prevPaused;
    }

    ImGui::BeginDisabled(exporting || decoding);
    if (ImGui::Button("Export Original as WAV", ImVec2(-1, 0)))
        RequestExportOriginal(asset);
    Hover("Decode the unmodified CPK asset to a standard WAV (save dialog).");
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##exmode", &s_exportMode, "Complete (intro+loop once)\0Expanded loops\0Loop-aware WAV (smpl)\0");
    if (s_exportMode == 1)
    {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        ImGui::SliderFloat("Loops", &s_exportLoops, 1.f, 8.f, "%.0f");
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
    ImGui::Combo("##exbits", &s_exportBits, "16-bit\0" "24-bit\0");
    ImGui::TextDisabled("~%.0fs · ADX decoded to PCM",
                        s_exportMode == 1 && asset.durationSec > 0
                            ? asset.durationSec * (0.25 + 0.75 * s_exportLoops)
                            : asset.durationSec);

    ImGui::SeparatorText("ORIGINAL");
    ImGui::BeginDisabled(decoding);
    if (ImGui::Button("Play##orig_play"))
        RequestOriginalPreview(asset);
    Hover("Play from the unmodified game CPK (async decode).");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(origActive && paused ? "Resume##orig" : "Pause##orig"))
        TogglePause(PreviewKind::Original);
    ImGui::SameLine();
    if (ImGui::Button("Stop##orig") && origActive)
        StopPreview();
    if (origActive)
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "Playing original");

    ImGui::SeparatorText("REPLACEMENT");
    ImGui::BeginDisabled(!hasRepl);
    if (ImGui::Button("Play##repl_play") && hasRepl)
        StartPreviewWav(s_dir / map->source, PreviewKind::Replacement,
                        "Replacement · " + fs::path(map->source).filename().string());
    Hover(hasRepl ? "Play the assigned WAV." : "Assign a WAV first.");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(replActive && paused ? "Resume##repl" : "Pause##repl"))
        TogglePause(PreviewKind::Replacement);
    ImGui::SameLine();
    if (ImGui::Button("Stop##repl") && replActive)
        StopPreview();
    if (replActive)
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "Playing replacement");

    ImGui::BeginDisabled(decoding);
    if (ImGui::Button("Compare A/B", ImVec2(-1, 0)))
    {
        if (s_prevKind == PreviewKind::Original && hasRepl)
            StartPreviewWav(s_dir / map->source, PreviewKind::Replacement,
                            "Replacement · " + fs::path(map->source).filename().string());
        else
            RequestOriginalPreview(asset);
    }
    Hover("A/B switch between original game audio and the assigned WAV.");
    ImGui::EndDisabled();
    {
        std::lock_guard lk(s_exportMu);
        if (!s_exportStatus.empty())
            ImGui::TextWrapped("%s", s_exportStatus.c_str());
        if (!s_lastExportPath.empty() && ImGui::SmallButton("Open export folder"))
            Shell_OpenFolder(fs::path(s_lastExportPath).parent_path());
    }
}

const char* StatusLabel(MusicTrackStatus::Kind k)
{
    switch (k)
    {
    case MusicTrackStatus::Kind::Enabled: return "Enabled";
    case MusicTrackStatus::Kind::Disabled: return "Disabled";
    case MusicTrackStatus::Kind::Missing: return "Missing";
    case MusicTrackStatus::Kind::Invalid: return "Invalid";
    default: return "Original";
    }
}

ImVec4 StatusColor(MusicTrackStatus::Kind k)
{
    switch (k)
    {
    case MusicTrackStatus::Kind::Enabled:
        return ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    case MusicTrackStatus::Kind::Missing:
        return ImVec4(1, 0.55f, 0.2f, 1);
    case MusicTrackStatus::Kind::Invalid:
        return ImVec4(1, 0.35f, 0.3f, 1);
    default:
        return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    }
}

} // namespace

void MusicUI_Draw(SettingsHost host, const char*& help)
{
    if (!s_loaded) Reload();
    s_help.clear();
#ifndef _WIN32
    PumpPreview();
#endif
    PollAsyncDecode();

    // Test aid: BKR_UI_SHOT_MUSIC_SELECT=<asset id|first> selects a row for layout shots.
    if (const char* sel = getenv("BKR_UI_SHOT_MUSIC_SELECT"))
    {
        static bool applied = false;
        if (!applied && !s_catalog.assets.empty())
        {
            applied = true;
            if (!strcmp(sel, "first"))
                s_selectedId = s_catalog.assets.front().id;
            else
                s_selectedId = sel;
        }
    }

    const float availW = ImGui::GetContentRegionAvail().x;
    const bool compact = availW < 900.f;
    const bool launcher = host == SettingsHost::Launcher;

    ImGui::SeparatorText("Music Library");
    if (ImGui::Checkbox("Enable custom music assets", &g_config.musicModsEnabled))
        Config_Save();
    Hover("When off, the game uses original sonic2010_0.cpk.");

    {
        std::lock_guard lk(s_cacheMu);
        if (compact)
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.55f, 1.f), "%s",
                               s_cacheSummary.find("ready") != std::string::npos ? "CPK cache ready" : s_cacheSummary.c_str());
        else
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.55f, 1.f), "%s", s_cacheSummary.c_str());
    }
    if (!compact && !s_statusMsg.empty())
        ImGui::TextWrapped("%s", s_statusMsg.c_str());
    else if (compact && !s_statusMsg.empty())
        ImGui::TextDisabled("%s", s_statusMsg.c_str());
    if (s_decodeRunning.load())
        ImGui::TextDisabled("Decoding original AAX…");

    if (ImGui::Button("Refresh")) Reload();
    ImGui::SameLine();
    if (ImGui::Button("Open folder")) Shell_OpenFolder(s_dir);
    ImGui::SameLine();
    if (s_prepRunning)
    {
        ImGui::BeginDisabled();
        ImGui::Button("Preparing…");
        ImGui::EndDisabled();
    }
    else if (ImGui::Button(compact ? "Prepare All" : "Prepare All Music Replacements"))
        StartPrepareAsync();
    Hover("Runs tools/music_asset prepare for every enabled mapping.");

    {
        std::lock_guard lk(s_prepMu);
        if (!compact && !s_prepLog.empty())
        {
            const float logH = ImGui::GetTextLineHeightWithSpacing() * 4.f;
            ImGui::BeginChild("prep_log", ImVec2(0, logH), ImGuiChildFlags_Borders);
            ImGui::TextUnformatted(s_prepLog.c_str());
            ImGui::EndChild();
        }
    }

    DrawPreviewTransport();
    DrawFilterBar(availW);

    // Side-by-side on wide windows keeps preview controls visible while the list scrolls.
    // Narrow windows stack list then detail inside one scroll region.
    const float remainH = ImGui::GetContentRegionAvail().y;
    const bool sideBySide = availW >= 1000.f && remainH >= ImGui::GetTextLineHeightWithSpacing() * 8.f;
    float listW = 0.f, detailW = 0.f, paneH = remainH;
    if (sideBySide)
    {
        listW = availW * 0.58f;
        detailW = availW - listW - ImGui::GetStyle().ItemSpacing.x;
        paneH = remainH;
    }

    const MusicCatalogAsset* selected = nullptr;
    auto drawList = [&]() {
        if (compact || !sideBySide)
        {
            for (auto& a : s_catalog.assets)
            {
                MusicAssetMapping* map = FindMapping(a.path);
                if (!PassesFilters(a, map)) continue;
                ImGui::PushID(a.id.c_str());
                const bool sel = s_selectedId == a.id;
                auto st = MusicManifest_AssetStatus(s_dir, map);
                std::string row = a.label + "  [" + a.variant + "]  " + StatusLabel(st.kind);
                if (ImGui::Selectable(row.c_str(), sel))
                {
                    if (s_selectedId != a.id) StopPreview();
                    s_selectedId = a.id;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s\n%s", a.filename.c_str(), a.path.c_str());
                if (sel) selected = &a;
                ImGui::PopID();
            }
        }
        else if (ImGui::BeginTable("music_lib", 5,
                                   ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY
                                       | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp
                                       | ImGuiTableFlags_ScrollX,
                                   ImVec2(0, 0)))
        {
            ImGui::TableSetupColumn("Music resource", ImGuiTableColumnFlags_WidthStretch, 2.6f);
            ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("Variant", ImGuiTableColumnFlags_WidthStretch, 0.7f);
            ImGui::TableSetupColumn("Replacement", ImGuiTableColumnFlags_WidthStretch, 1.4f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            for (auto& a : s_catalog.assets)
            {
                MusicAssetMapping* map = FindMapping(a.path);
                if (!PassesFilters(a, map)) continue;
                ImGui::TableNextRow();
                ImGui::PushID(a.id.c_str());
                const bool sel = s_selectedId == a.id;
                ImGui::TableNextColumn();
                // SpanAllColumns alone stole clicks; AllowOverlap keeps later cells usable.
                // Preview buttons live in the detail pane, not overlapping this selectable.
                if (ImGui::Selectable(a.label.c_str(), sel,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                {
                    if (s_selectedId != a.id) StopPreview();
                    s_selectedId = a.id;
                }
                if (sel) selected = &a;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", a.filename.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(a.category.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(a.variant.c_str());
                ImGui::TableNextColumn();
                if (map && !map->source.empty())
                    ImGui::TextUnformatted(fs::path(map->source).filename().string().c_str());
                else
                    ImGui::TextDisabled("Original");
                ImGui::TableNextColumn();
                auto st = MusicManifest_AssetStatus(s_dir, map);
                ImGui::TextColored(StatusColor(st.kind), "%s", StatusLabel(st.kind));
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    };

    auto drawDetail = [&]() {
        if (!selected)
            for (auto& a : s_catalog.assets)
                if (a.id == s_selectedId) { selected = &a; break; }
        if (!selected)
        {
            ImGui::TextDisabled("Select a music asset to preview, export, or assign a replacement.");
            return;
        }
        if (s_prevSelectedId != selected->id)
        {
            if (!s_prevSelectedId.empty()) StopPreview();
            s_prevSelectedId = selected->id;
            {
                std::lock_guard lk(s_exportMu);
                s_exportStatus.clear();
            }
        }
        MusicAssetMapping* map = FindMapping(selected->path);
        if (s_editSyncedId != selected->id)
        {
            s_editSyncedId = selected->id;
            s_editLoopStart = map ? map->loopStart : 0.0;
        }
        DrawPlayerPanel(*selected, map, launcher);
        ImGui::Spacing();
        if (ImGui::Button("Assign Replacement…"))
        {
            fs::path picked;
            if (Dialog_PickWavFile(picked))
            {
                std::string err;
                std::string tag = !selected->stage.empty() ? selected->stage : selected->id;
                std::string rel = MusicManifest_ImportWav(s_dir, tag, picked, err);
                if (rel.empty()) s_statusMsg = err;
                else
                {
                    if (!map)
                    {
                        MusicAssetMapping a;
                        a.id = selected->stage.empty()
                                   ? selected->id.substr(0, std::min<size_t>(selected->id.size(), 40))
                                   : selected->stage + "_" + selected->variant;
                        a.archive = selected->archive;
                        a.path = selected->path;
                        a.stage = selected->stage;
                        a.variant = selected->variant;
                        a.cue = selected->cue;
                        a.enabled = true;
                        a.source = rel;
                        a.loopStart = s_editLoopStart;
                        s_assets.push_back(a);
                    }
                    else
                    {
                        map->source = rel;
                        map->enabled = true;
                    }
                    SaveAssets();
                    map = FindMapping(selected->path);
                }
            }
        }
        ImGui::SameLine();
        if (map && ImGui::Button(map->enabled ? "Disable" : "Enable"))
        {
            map->enabled = !map->enabled;
            SaveAssets();
        }
        ImGui::SameLine();
        if (map && ImGui::Button("Restore original"))
        {
            s_assets.erase(std::remove_if(s_assets.begin(), s_assets.end(),
                                          [&](const MusicAssetMapping& x) { return x.path == selected->path; }),
                           s_assets.end());
            SaveAssets();
            map = nullptr;
            s_editLoopStart = 0.0;
        }
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
        ImGui::InputDouble("Loop start (s)", &s_editLoopStart, 0.1, 1.0, "%.2f");
        Hover("Used when converting multi-segment AAX (intro/loop). 0 = auto.");
        if (map && ImGui::Button("Save loop setting"))
        {
            map->loopStart = s_editLoopStart;
            SaveAssets();
        }
    };

    if (sideBySide)
    {
        ImGui::BeginChild("music_list", ImVec2(listW, paneH), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
        drawList();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("music_detail", ImVec2(detailW, paneH),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);
        drawDetail();
        ImGui::EndChild();
    }
    else
    {
        // Detail first so Original/Replacement controls stay on-screen without scrolling.
        const float line = ImGui::GetTextLineHeightWithSpacing();
        float detailTop = std::clamp(remainH * 0.58f, line * 10.f, line * 16.f);
        if (detailTop > remainH - line * 3.f)
            detailTop = std::max(remainH - line * 3.f, line * 8.f);
        ImGui::BeginChild("music_detail", ImVec2(0, detailTop),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);
        drawDetail();
        ImGui::EndChild();
        ImGui::BeginChild("music_list", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
        drawList();
        ImGui::EndChild();
    }

    if (const char* check = getenv("BKR_UI_LAYOUT_CHECK"))
    {
        if (check[0] && availW > 0 && getenv("BKR_UI_LAYOUT_CHECK_LOG"))
            LOG_INFO("[UI] layout check ok availW={:.0f} remainH={:.0f} sideBySide={} compact={}",
                     availW, remainH, sideBySide, compact);
    }

    help = s_help.empty() ? nullptr : s_help.c_str();
}

void MusicUI_Shutdown()
{
    StopPreview();
    s_prevSelectedId.clear();
}
