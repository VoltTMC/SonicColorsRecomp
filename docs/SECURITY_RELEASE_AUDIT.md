# Security release audit — Sonic Colors Recomp v0.3.7 (Windows)

**Investigation date:** 2026-11-21  
**Branch:** `security/v037-alyac-audit` (documentation + CI workflow only)  
**Scope:** ALYac / VirusTotal detection on `SonicColorsRecomp-v0.3.7-windows-x86_64.zip` blocking RecompStore listing.  
**Constraint:** No new GitHub Release, no replacement of published v0.3.7 assets, no AV-evasion binary edits, no A3c work in this branch.

---

## Executive summary

| Item | Finding |
|------|---------|
| Original ZIP SHA-256 | `013418440e4ebea92f2cb6e6d11d9777979a7e9eb946b5e225ad1e0d53e932ab` — **matches** GitHub Release asset digest and VirusTotal sample |
| Source revision | Annotated tag `v0.3.7` → commit `89452043ff74f4da76c12e8ac17f34f99d3189d1` |
| Build provenance | **Local MinGW cross-build** packaged on the maintainer host; **no** `.github/workflows` existed at release time |
| PE members in ZIP | 4: `SonicColorsRecomp.exe`, `music_asset.exe`, `libwinpthread-1.dll` ×2 |
| Authenticode | **None** (Security Directory empty on main EXE / helper / DLL) |
| Likely detection target | **Most likely** `SonicColorsRecomp.exe` (≈489 MiB unsigned MinGW PE). Per-engine member attribution was **not** confirmed from VT’s public HTML scrape |
| Malicious behaviour found? | **No confirmed malware.** Observed network/process APIs match documented updater, mods, and Music-tool flows |
| Heuristic risk factors | Huge RelWithDebInfo PE (**≈451 MiB DWARF debug still embedded**), unsigned, MinGW, WinHttp/CreateProcess/PowerShell update helper |
| False-positive submission | Draft prepared (`docs/security/ESTSECURITY_FALSE_POSITIVE_SUBMISSION.md`); **not submitted** by this investigation |
| Replacement artifact | **None published.** Original v0.3.7 remains the sole official Windows ZIP |
| RecompStore | Still blocked pending AV clearance / store policy |

**Verdict classes used below:** **Confirmed** · **Likely** · **Unresolved**.

---

## 1. Original release provenance (Confirmed)

| Field | Value |
|-------|-------|
| Tag | `v0.3.7` (annotated) |
| Tag message | `v0.3.7 — hotfix: Music preview, deps persistence, Music tab performance` |
| Commit | `89452043ff74f4da76c12e8ac17f34f99d3189d1` — *Prepare AppStream and docs for the v0.3.7 hotfix release.* |
| Tag date | 2026-11-20 21:48:40 +0000 |
| Release published | 2026-11-20T21:53:36Z |
| Release URL | https://github.com/VoltTMC/SonicColorsRecomp/releases/tag/v0.3.7 |
| `target_commitish` | `main` |
| Publish script | `tools/publish_v0.3.7.sh` — requires pre-existing files under `dist/`, then `gh release create` |

### Release assets (GitHub API digests)

| Asset | Size | SHA-256 (GitHub `digest`) |
|-------|------|---------------------------|
| `SonicColorsRecomp-v0.3.7-windows-x86_64.zip` | 179 471 488 | `013418440e4ebea92f2cb6e6d11d9777979a7e9eb946b5e225ad1e0d53e932ab` |
| `SonicColorsRecomp-Setup-v0.3.7.exe` | 121 653 112 | `90f89a999efcb95072903c21eeef222e9b623aebb207b0b3294dbd2f4bc1902a` |
| `SonicColorsRecomp-v0.3.7-linux-x86_64.AppImage` | 29 768 184 | `2ae429622bde79fa756e6354f147a8db3272e1e4b0eda7f0c4fa906b12a0147a` |

### Preserved local copy

```
out/security_audit_v037/SonicColorsRecomp-v0.3.7-windows-x86_64.zip
sha256sum → 013418440e4ebea92f2cb6e6d11d9777979a7e9eb946b5e225ad1e0d53e932ab
```

Identical to `dist/SonicColorsRecomp-v0.3.7-windows-x86_64.zip` on the maintainer machine.

### Toolchain / packaging (as designed for this project)

| Component | Role |
|-----------|------|
| CMake preset `windows-mingw` | `CMAKE_BUILD_TYPE=RelWithDebInfo`, output `out-windows/` |
| Compiler | `x86_64-w64-mingw32-g++` (host check during audit: GCC **16.2.0**) |
| Package ZIP | `tools/package_windows_cross.sh` |
| Installer | `tools/package_windows_installer.sh` → Inno Setup 7.x (`SonicColorsRecomp-Setup-v0.3.7.exe`) |
| GitHub Actions | **Absent** at v0.3.7 (no `.github/` in tree). Release was **not** CI-built |

`main` at investigation time (`f4559b1…`) is **ahead** of `v0.3.7` and must not be treated as the release source.

---

## 2. ZIP inventory (Confirmed)

Full tables: [`docs/security/v0.3.7-windows-zip-inventory.md`](security/v0.3.7-windows-zip-inventory.md) / [`.json`](security/v0.3.7-windows-zip-inventory.json).

**40 files** after unzip (VT reports 51 contained nodes including directories).

### Executables and DLLs

| Path | Kind | Size | SHA-256 |
|------|------|------|---------|
| `SonicColorsRecomp/SonicColorsRecomp.exe` | PE-EXE | 512 433 009 | `2bc687b765f82dbc91e21f75213aebd03bdc6c1e1eb392500ecf7612557fb554` |
| `SonicColorsRecomp/tools/music_asset.exe` | PE-EXE | 16 372 524 | `ba06251a67febf79d368cacba388324fcafa5ff5c86a58d4b354eddfee973e2b` |
| `SonicColorsRecomp/libwinpthread-1.dll` | PE-DLL | 61 429 | `9230fabfb5b5909d6f223327d2ac72cf17e161dfe51e9c7537517ae61a05e62e` |
| `SonicColorsRecomp/tools/libwinpthread-1.dll` | PE-DLL | 61 429 | *same as above* |

Scripts: `tools/music_asset.cmd`, `tools/music_deps_setup.cmd` (batch; create venv / pip install cricodecs — user-initiated).

Remaining members: README/TESTING/RUNTIME_DEPS, licenses, music catalog JSON, `recomplauncher.json`, shader keys, Python `music_asset_pkg/*`, empty `portable.txt`.

---

## 3. Detection target (Likely / Unresolved detail)

| Claim | Status |
|-------|--------|
| VT sample is the official ZIP | **Confirmed** (hash match) |
| Detection name / engine | **Confirmed** from RecompStore report: ALYac `Gen:Variant.Barys.535120` |
| Exact archive member ALYac scored | **Unresolved** from public VT HTML (SPA; API returned empty without auth). VT “bundled files” lists the EXE/DLL hashes above |
| Most probable member | **Likely** `SonicColorsRecomp.exe` — sole large PE, unsigned, atypical size |

`libwinpthread-1.dll` SHA-256 is **byte-identical** to `/usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll` on the audit host (**Confirmed** distro MinGW runtime, not a trojanized DLL).

---

## 4. Windows build & package audit (Confirmed)

### Pipeline

1. `cmake --preset windows-mingw && cmake --build --preset windows-mingw`
2. `tools/package_windows_cross.sh` stages `dist/SonicColorsRecomp/` and zips it
3. Optional `tools/package_windows_installer.sh` builds Inno Setup installer
4. `tools/publish_v0.3.7.sh` uploads whatever is already in `dist/`

No UPX/packer, no post-link encryption, no host-EXE injection step beyond copying MinGW `libwinpthread` and build outputs.

### RelWithDebInfo without strip (Confirmed — major packaging issue)

`SonicColorsRecomp.exe` PE layout:

| Region | Approx. raw size |
|--------|------------------|
| `.text` + normal data/reloc | ≈ **61 MiB** |
| `.debug_*` DWARF sections | ≈ **451 MiB** |
| **Total file** | ≈ **512 MiB** |

Entropy of the file is moderate (~6.45); sections do **not** look like high-entropy packing. Shipping full debug info in a “release” ZIP is a **quality / AV-heuristic problem**, not evidence of malware.

PE `Time/Date`: Fri Nov 20 21:46:42 2026 — consistent with release packaging the same evening.

### Imports (main EXE) — expected for this app

`KERNEL32`, `USER32`, `GDI32`, `SHELL32`, `ole32`, `d3d11`, `D3DCOMPILER_47`, `XAudio2_9`, `XINPUT1_3`, `WINMM`, `WINHTTP`, `WS2_32`, `bcrypt` / `bcryptprimitives`, `dbghelp`, `dwmapi`, `USERENV`, UCRT `api-ms-win-crt-*`, `libwinpthread-1.dll`, `ntdll`.

No `URLDownloadToFile`, no `VirtualAllocEx` / `WriteProcessMemory` / `CreateRemoteThread` strings in the scanned image prefix.

### Legitimate behaviours that AV may observe

| Behaviour | Source | Assessment |
|-----------|--------|------------|
| HTTPS to GitHub Releases / API | `runtime/ui/update_check.cpp`, `runtime/platform/win32/http.cpp` | Expected updater |
| Download update ZIP + checksum | update UI | Expected; validates size/paths |
| `powershell.exe` apply helper | update apply (`apply_update.ps1` generated at runtime) | Expected install-update path; **can** raise heuristics |
| GameBanana HTTPS | launcher / mods | Expected optional feature |
| Music deps download (pinned SHA-256) | `data/music_deps_manifest.json` + dependency downloader | Expected; URLs are python.org / PyPI / GitHub / FFmpeg-Builds |
| `CreateProcessW` for Python / music_asset | `runtime/platform/win32/platform.cpp`, `run_process.cpp` | Expected tooling |
| `ShellExecuteW` open URL/folder | `runtime/platform/win32/ui.cpp` | Expected |

### Setup.exe

`SonicColorsRecomp-Setup-v0.3.7.exe` is an **Inno Setup 7.1.1** installer (PE32 i386 stub). Separate from the flagged ZIP; included here for completeness. Hash matches GitHub digest.

---

## 5. Dependency inventory (Confirmed)

| Name | Version / identity | Source | In ZIP? | Licence | Notes |
|------|--------------------|--------|---------|---------|-------|
| SonicColorsRecomp.exe | built from `89452043` | Project MinGW build | Yes | GPL-2.0 | Primary host |
| music_asset.exe | same build | Project (`music_asset` target) | Yes | Project | Music helper |
| libwinpthread-1.dll | MinGW-w64 runtime | Distro `/usr/x86_64-w64-mingw32/bin/` | Yes (×2) | MinGW/winpthreads | Matches system file hash |
| music_asset_pkg (Python) | shipped sources | `tools/music_asset_pkg/` | Yes | Project | Not bytecode |
| catalog.json | music catalog | `mods/music/` | Yes | Project data | No audio payloads |
| imgui / nod / libarchive / zlib / nlohmann / liblzma | linked or licensed | vendored / prebuilts per `docs/DEPENDENCIES.md` | Licenses yes; nod.dll **not** present in this ZIP | Various OSS | RUNTIME_DEPS lists system DLLs only |
| Python 3.12.8 embed | manifest pin | python.org | **No** (runtime download) | PSF | SHA-256 pinned |
| cricodecs 1.2.0 wheel | manifest pin | PyPI | **No** | MIT | SHA-256 pinned |
| vgmstream r2168 | manifest pin | GitHub release | **No** | ISC | SHA-256 pinned |
| FFmpeg LGPL shared | manifest pin | FFmpeg-Builds | **No** | LGPL-2.1+ | SHA-256 pinned |

No unexpected PE files from the developer desktop (devtools, test harnesses, random DLLs) were found in the ZIP.

---

## 6. Clean GitHub Actions build (Prepared — not a replacement release)

Added workflow: [`.github/workflows/windows-release.yml`](../.github/workflows/windows-release.yml)

| Property | Behaviour |
|----------|-----------|
| Trigger | `workflow_dispatch` with explicit `ref` (default `v0.3.7`) |
| Checkout | Exact ref |
| Environment | `ubuntu-24.04` + apt MinGW |
| Build | `windows-mingw` preset; targets `SonicColorsRecomp` + `music_asset` |
| Strip option | Default **on** (`x86_64-w64-mingw32-strip --strip-debug`) |
| Package | Existing `tools/package_windows_cross.sh` |
| Output | Artifact upload + `SHA256SUMS` / provenance — **does not** `gh release` |
| Tests | Compiles/packages only; native Windows GPU tests remain out of scope on Linux runners |

### Hash comparison expectation

A CI rebuild of `v0.3.7` **will not** match `2bc687b7…` even without strip: MinGW embeds timestamps/paths; RelWithDebInfo is not bit-reproducible across hosts. After strip, size should drop toward ≈60–80 MiB class. **Different hashes ≠ safer by itself**; provenance + source identity matter.

**Status:** Workflow committed on this branch for review. **Not** run as a published replacement for v0.3.7 in this investigation.

---

## 7. Security validation summary

| Check | Result |
|-------|--------|
| ZIP integrity vs GitHub | Pass (SHA-256) |
| PE magic / machine | PE32+ x86-64 for EXE/DLL |
| Authenticode | Absent |
| Section entropy / packer | No UPX-like packing; large DWARF sections |
| Import table review | Consistent with D3D11 game host + WinHttp |
| String review (non-debug image prefix) | GitHub/GameBanana/update/music strings; PowerShell update helper |
| winpthread authenticity | Matches distro file |
| Dynamic malware sandbox | **Not run** (no Windows VM in this pass). Limitation documented |
| Live AV disable / exclusions | Not used |

**Confirmed facts:** provenance, inventory, unsigned MinGW RelWithDebInfo-with-debug, legitimate API surface.  
**Likely explanation of ALYac hit:** generic heuristic (`Gen:Variant.Barys.*`) on a large unsigned uncommon PE; Bitdefender-family engines often own `Gen:Variant.*` names.  
**Unresolved:** exact VT member ALYac scored; whether Bitdefender.com itself also detects the EXE; dynamic sandbox behaviour.

---

## 8. False-positive submission

Draft: [`docs/security/ESTSECURITY_FALSE_POSITIVE_SUBMISSION.md`](security/ESTSECURITY_FALSE_POSITIVE_SUBMISSION.md)

| Step | Status |
|------|--------|
| Prepare hashes, VT URL, product description | Done |
| Maintainer sends to help@alyac.co.kr / ESRC / AYReport | **Pending human** |
| Parallel Bitdefender incorrect-detection report | **Recommended, pending** |
| Vendor confirmation + fresh VT analysis | **Not claimed** |

---

## 9. Code signing options (assessment only — no purchase)

| Option | Cost | Eligibility | Notes |
|--------|------|-------------|-------|
| **SignPath Foundation** (OSS) | Free if accepted | OSI licence, public repo, maintained, no malware/PUA, trusted CI builds | Strong fit **after** GitHub Actions packaging exists; SignPath signs CI artifacts via HSM; Foundation is publisher when using their cert. Apply: https://signpath.org/ |
| **OV Authenticode** (commercial CA) | Typically hundreds USD/year | Org/individual identity verification; USB/HSM often required for new certs | Works with `signtool` + RFC 3161 timestamp; key must never enter git |
| **Azure Artifact Signing** | Azure subscription | Publisher identity + Azure tenancy | Viable if publisher already has Azure; still needs identity proof |

**Recommendation:** (1) land CI packaging + strip debug for *future* builds; (2) apply to SignPath Foundation for OSS signing of CI artifacts; (3) keep OV/Azure as paid fallback.  
**Signing alone does not prove the ALYac verdict was wrong**, but it reduces heuristic score for future releases.

Do **not** use self-signed Authenticode for public distribution.

---

## 10. Acceptance criteria checklist

| # | Criterion | Status |
|---|-----------|--------|
| 1 | Original source commit identified | Done (`89452043`) |
| 2 | ZIP inventory complete | Done |
| 3 | Exact detection target identified or uncertainty documented | Uncertainty documented; EXE most likely |
| 4 | Windows build/package audited | Done |
| 5 | Dependencies accounted for | Done |
| 6 | Clean CI workflow prepared | Done (not a published replacement) |
| 7 | Hash comparison completed | Original preserved; CI expected non-matching |
| 8 | Security checks documented | Done (+ sandbox limitation) |
| 9 | FP submission prepared / submitted | Prepared; submission pending maintainer |
| 10 | Signing options assessed | Done |
| 11 | Replacement artifact status clear | **No replacement published** |

---

## 11. Remaining risks & RecompStore

- ALYac detection may remain until vendor updates signatures.
- Even after FP clearance, RecompStore may require CI-built and/or signed artifacts for policy.
- Shipping 500 MiB debug EXEs will keep stressing scanners and download UX.
- Update-via-PowerShell and dependency downloaders are legitimate but will always be “interesting” to heuristics — document clearly for stores.

**RecompStore acceptance:** **Not cleared** as of this audit. Provide this document + hashes + VT link + (once available) ESTsecurity case ID.

---

## 12. Commands used (reproducible)

```bash
# Preserve + hash
sha256sum dist/SonicColorsRecomp-v0.3.7-windows-x86_64.zip
# → 013418440e4ebea92f2cb6e6d11d9777979a7e9eb946b5e225ad1e0d53e932ab

git rev-parse v0.3.7
# → 89452043ff74f4da76c12e8ac17f34f99d3189d1

# Inventory (see out/security_audit_v037/)
python3 -c 'import zipfile,hashlib,...'  # full script in audit notes

# PE imports
x86_64-w64-mingw32-objdump -p unzipped/SonicColorsRecomp/SonicColorsRecomp.exe

# winpthread authenticity
sha256sum unzipped/SonicColorsRecomp/libwinpthread-1.dll \
          /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll
```

---

## Document control

| Version | Date | Notes |
|---------|------|-------|
| 1.0 | 2026-11-21 | Initial audit on `security/v037-alyac-audit` |
