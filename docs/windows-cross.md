# Windows cross-build from Linux (MinGW-w64)

Build a native 64-bit Windows executable (`SonicColorsRecomp.exe`) on Linux without a
Windows host. The Linux native build (`linux-clang` → `out/`) is unchanged and uses a
separate build tree and output directory.

Feature parity vs Linux is tracked in [WINDOWS_FEATURE_PARITY.md](WINDOWS_FEATURE_PARITY.md).

## Toolchain choice

**MinGW-w64 GCC** (`x86_64-w64-mingw32-g++`) is the selected Linux-hosted toolchain.

| Option | Why not / why yes |
|---|---|
| MinGW-w64 GCC | Installed on CachyOS; headers/libs for D3D11, XAudio2, XInput, WinHTTP, WIC, dbghelp; matches the existing complete `runtime/platform/win32` code path |
| LLVM/Clang → Windows GNU | Possible, but MinGW GCC is already present and sufficient |
| clang-cl + Windows SDK from Linux | Needs a full MSVC/Windows SDK tree on Linux; heavier than MinGW for this project |

The prebuilt **nod** Windows archive (`nod_static.lib`) contains `pe-x86-64` objects that
MinGW’s linker accepts. CMake aliases that library for MinGW (nod’s config package only
registers the MSVC static path when `MSVC` is on).

zlib / xz / libarchive are built from source via ExternalProject with the MinGW toolchain
file passed through (Unix `lib*.a` names, not MSVC `zs.lib`).

## Required packages (Arch / CachyOS)

```bash
sudo pacman -S mingw-w64-gcc mingw-w64-headers mingw-w64-crt mingw-w64-winpthreads \
  cmake ninja zip rust
rustup target add x86_64-pc-windows-gnu   # nod is built from source for MinGW
# optional validation:
sudo pacman -S wine   # lightweight PE smoke only; not a gameplay test
```

Verified on this machine: **GCC 16.2.0**, mingw-w64 headers/crt **14.0.0**, Rust with `x86_64-pc-windows-gnu`.

## Configure and build

```bash
# Clean PATH avoids AppImage/compiler wrapper issues in some IDE shells:
export PATH=/usr/bin:/bin

cmake --preset windows-mingw
cmake --build --preset windows-mingw
```

Debug:

```bash
cmake --preset windows-mingw-debug
cmake --build --preset windows-mingw-debug
```

Artifacts:

| Path | Role |
|---|---|
| `build/windows-mingw/` | CMake build tree (isolated from `build/linux-clang/`) |
| `out-windows/SonicColorsRecomp.exe` | Windows PE executable |
| `out-windows/licenses/` | Third-party notices copied at link time |

Do **not** reuse the Linux CMake cache or write into `out/` for the cross build.

## Package

```bash
tools/package_windows_cross.sh
# → dist/SonicColorsRecomp-<version>-windows-x86_64.zip
```

The ZIP contains:

- `SonicColorsRecomp.exe`
- `libwinpthread-1.dll` when the PE still imports it (fallback if static winpthread did not stick)
- **No** `nod.dll` / `VCRUNTIME140.dll` — MinGW builds nod from source (`windows-gnu`) and links it statically
- licenses, optional shader keys, `RUNTIME_DEPS.txt`
- no copyrighted game disc contents

`libgcc` / `libstdc++` are linked statically.

## Validation

```bash
file out-windows/SonicColorsRecomp.exe
# PE32+ executable for MS Windows … x86-64

x86_64-w64-mingw32-objdump -p out-windows/SonicColorsRecomp.exe | grep 'DLL Name'
# Expect: nod.dll, d3d11, XAudio2_9, XINPUT1_3, WINHTTP, … plus UCRT api-ms-win-crt-* 

# Optional Wine smoke (not a gameplay proof):
WINEPREFIX=$HOME/.wine-scr wine out-windows/SonicColorsRecomp.exe
```

Confirm the Linux target still works:

```bash
cmake --build --preset linux-clang
```

### Cursor / AppImage shells

If `cmake` or `x86_64-w64-mingw32-g++` fails with `CMAKE_ROOT` / `cc1` errors inside Cursor’s
AppImage environment, configure/build with a clean env:

```bash
env -i HOME="$HOME" USER="$USER" PATH=/usr/bin:/bin LANG=C.UTF-8 \
  cmake --preset windows-mingw
env -i HOME="$HOME" USER="$USER" PATH=/usr/bin:/bin LANG=C.UTF-8 \
  cmake --build --preset windows-mingw
```

## Limitations

- Cross-compilation proves a valid Windows PE and correct link against Win32 import libs.
  It does **not** prove full gameplay on Windows hardware (D3D11 presentation, XAudio2,
  controllers, install flow). Test on a real Windows machine before calling a release good.
- Wine may start the process and reach guest boot; D3D11/windowing under Wine is not
  gameplay certification. Host AVs seen under Wine are not attributed to the MinGW
  toolchain without a Windows hardware retest.
- Official prebuilt **nod** for Windows is MSVC-targeted (needs `VCRUNTIME140.dll`).
  The MinGW preset builds nod from source for `x86_64-pc-windows-gnu` instead
  (`cmake/nod_mingw.cmake`; requires `cargo` + that Rust target).
- Shader precompile during packaging is skipped under Wine; keys are copied when present;
  first Windows run may compile HLSL.
- Host-side tools (`tools/build_generated.py`, disc install via Linux `scr_install`) remain
  Linux-native; Windows `scr_install.exe` is also produced under `out-windows/`.
- Do not mix MSVC-built C++ objects with MinGW (different C++ ABI). Stay on one Windows
  toolchain per binary.
