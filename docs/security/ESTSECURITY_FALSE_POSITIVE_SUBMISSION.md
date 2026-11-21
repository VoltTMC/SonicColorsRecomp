# ESTsecurity / ALYac false-positive submission draft — Sonic Colors Recomp v0.3.7

**Status:** Prepared for submission. Not yet submitted by this investigation (no vendor case ID).

**Do not claim vendor confirmation until ESTsecurity / ALYac responds.**

## Official channels

| Channel | URL / address | Notes |
|---------|---------------|-------|
| ESTsecurity support report page | https://www.estsecurity.com/public/support/report | Points to AYReport tool / support flows |
| ALYac help (오진 신고) | In-product “신고하기” / help docs | Attach sample; large files → email |
| ALYac email (documented) | help@alyac.co.kr | Preferred when attachment > ~9 MB |
| ESRC (documented) | esrc@estsecurity.com | ESTsecurity Response Center |

`Gen:Variant.*` names are commonly associated with the **Bitdefender** engine family (licensed by several OEMs including ALYac). After submitting to ESTsecurity, also file with Bitdefender’s incorrect-detection form so a root signature correction can propagate.

Bitdefender incorrect detection: https://www.bitdefender.com/consumer/support/answer/29358/

## Product identification

| Field | Value |
|-------|-------|
| Product | Sonic Colors Recomp |
| Release | v0.3.7 |
| Platform | Windows x86-64 |
| Publisher / maintainer | VoltTMC |
| Repository | https://github.com/VoltTMC/SonicColorsRecomp |
| Source commit (tag `v0.3.7`) | `89452043ff74f4da76c12e8ac17f34f99d3189d1` |
| GitHub Release | https://github.com/VoltTMC/SonicColorsRecomp/releases/tag/v0.3.7 |
| License | GPL-2.0 (see `licenses/` in package) |

## Detection details

| Field | Value |
|-------|-------|
| Engine | ALYac (ESTsecurity) |
| Detection | `Gen:Variant.Barys.535120` |
| VirusTotal (ZIP) | https://www.virustotal.com/gui/file/013418440e4ebea92f2cb6e6d11d9777979a7e9eb946b5e225ad1e0d53e932ab |
| Reported ratio | 1 / 74 engines (RecompStore / VT report) |

## Artifact hashes

| Artifact | SHA-256 |
|----------|---------|
| `SonicColorsRecomp-v0.3.7-windows-x86_64.zip` (GitHub asset + local `dist/`) | `013418440e4ebea92f2cb6e6d11d9777979a7e9eb946b5e225ad1e0d53e932ab` |
| `SonicColorsRecomp/SonicColorsRecomp.exe` (primary PE in ZIP) | `2bc687b765f82dbc91e21f75213aebd03bdc6c1e1eb392500ecf7612557fb554` |
| `SonicColorsRecomp/tools/music_asset.exe` | `ba06251a67febf79d368cacba388324fcafa5ff5c86a58d4b354eddfee973e2b` |
| `SonicColorsRecomp/libwinpthread-1.dll` (also under `tools/`) | `9230fabfb5b5909d6f223327d2ac72cf17e161dfe51e9c7537517ae61a05e62e` |
| `SonicColorsRecomp-Setup-v0.3.7.exe` (Inno Setup; separate release asset) | `90f89a999efcb95072903c21eeef222e9b623aebb207b0b3294dbd2f4bc1902a` |

**Flagged member:** VirusTotal’s public ZIP report associates the archive with the PE members above. The investigation **most likely** targets `SonicColorsRecomp.exe` (≈489 MiB unsigned MinGW PE). ALYac’s engine output on VT did not expose a per-member verdict in the static HTML scrape used here; if the vendor requires a single attachment, attach the ZIP **or** the main EXE and cite both hashes.

## What the software is

Sonic Colors Recomp is an open-source **PC port / recompilation frontend** for the user’s own Europe/PAL Wii disc of *Sonic Colors* (`SNCP8P`). The Windows ZIP contains:

- The recompiled game host (`SonicColorsRecomp.exe`) using Direct3D 11, XAudio2, XInput.
- A Music Library helper (`music_asset.exe` + Python package) for optional user-supplied WAV→AAX workflows.
- MinGW `libwinpthread-1.dll` (byte-identical to the distro MinGW package on the build host).
- Documentation, licenses, music catalog JSON, and shader-cache keys.

**No game ROM/ISO is redistributed.** Users must supply their own disc image.

## Expected network / process behaviour (legitimate)

- `WinHttp` HTTPS to `api.github.com` / GitHub Releases for optional update checks and package download.
- Optional GameBanana HTTPS for community mods (user-initiated).
- Optional download of pinned Music tools (Python embed, cricodecs wheel, vgmstream, LGPL FFmpeg) into `%LOCALAPPDATA%\SonicColorsRecomp\dependencies` with SHA-256 verification (`data/music_deps_manifest.json`).
- `CreateProcessW` to run Music helper / Python tooling.
- Optional `powershell.exe` helper to apply an already-downloaded update zip under the install directory.
- `ShellExecuteW` to open the user’s browser to the GitHub releases page.

There is **no** packing/obfuscation, **no** Authenticode signature on v0.3.7 PE files, and **no** self-modifying loader.

## Build environment (v0.3.7 as published)

| Item | Value |
|------|-------|
| Build host | Maintainer Linux machine (local), not GitHub Actions |
| Toolchain | `x86_64-w64-mingw32-g++` (MinGW-w64; host observed GCC 16.2.0) |
| CMake preset | `windows-mingw` → `CMAKE_BUILD_TYPE=RelWithDebInfo` |
| Package script | `tools/package_windows_cross.sh` |
| Publish script | `tools/publish_v0.3.7.sh` (uploads pre-built `dist/` artifacts) |
| Time/Date in PE | Fri Nov 20 21:46:42 2026 (`SonicColorsRecomp.exe`) |

**Important packaging note:** RelWithDebInfo left ≈451 MiB of DWARF `.debug_*` sections inside the shipped EXE. That inflates size and can stress heuristic scanners; it is not packing. Future CI builds can strip debug sections without changing program behaviour.

## Suggested email body (English)

Subject: False positive request — ALYac `Gen:Variant.Barys.535120` — Sonic Colors Recomp v0.3.7

```
Hello ESTsecurity / ALYac analysis team,

We believe the following detection is a false positive against our open-source
Windows release:

  Product:   Sonic Colors Recomp v0.3.7 (Windows x86-64)
  Detection: Gen:Variant.Barys.535120 (ALYac)
  VirusTotal:
    https://www.virustotal.com/gui/file/013418440e4ebea92f2cb6e6d11d9777979a7e9eb946b5e225ad1e0d53e932ab

  ZIP SHA-256:
    013418440e4ebea92f2cb6e6d11d9777979a7e9eb946b5e225ad1e0d53e932ab
  Primary EXE SHA-256:
    2bc687b765f82dbc91e21f75213aebd03bdc6c1e1eb392500ecf7612557fb554

Source repository:
  https://github.com/VoltTMC/SonicColorsRecomp
Exact commit (tag v0.3.7):
  89452043ff74f4da76c12e8ac17f34f99d3189d1
Release page:
  https://github.com/VoltTMC/SonicColorsRecomp/releases/tag/v0.3.7

Sonic Colors Recomp is a GPL-licensed PC recompilation frontend for a game the
user already owns. The Windows package is a MinGW-w64 cross-build. It is not
packed or obfuscated. Network use is limited to HTTPS update checks (GitHub),
optional mod downloads (GameBanana), and optional Music-tool dependency
downloads with pinned SHA-256 hashes. 73 of 74 VirusTotal engines reported the
ZIP as undetected at the time of the store review.

Please re-analyse and remove the detection if you agree it is a false positive.

Thank you,
<maintainer name / contact email>
VoltTMC / Sonic Colors Recomp
```

## Attachments checklist

1. `SonicColorsRecomp-v0.3.7-windows-x86_64.zip` (or EXE if size limits require splitting / external link to GitHub asset).
2. This document or the hash table above.
3. Optional: VirusTotal screenshot.
4. Optional: pointer to `docs/SECURITY_RELEASE_AUDIT.md` once published on the audit branch.

## Submission log

| Date (UTC) | Action | Reference |
|------------|--------|-----------|
| 2026-11-21 | Draft prepared in-repo | *(no vendor ticket yet)* |
| — | Actual send to help@alyac.co.kr / ESRC | *pending maintainer* |
| — | Bitdefender incorrect-detection form | *pending maintainer* |
| — | Vendor confirmation / VT rescan | *pending* |
