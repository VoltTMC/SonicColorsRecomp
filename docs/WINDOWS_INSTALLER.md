# Windows installer (primary distribution)

Sonic Colors Recomp on Windows is distributed as a **per-user Inno Setup installer**.
An optional **portable ZIP** remains available for advanced users.

## Install

1. Run `SonicColorsRecomp-Setup-v*.exe`.
2. Default location: `%LOCALAPPDATA%\Programs\Sonic Colors Recomp`  
   (no Administrator elevation required).
3. **Fresh install is the default.** You do **not** need a portable folder.
4. Optional: on the “Existing portable data” page, choose migrate and only then pick your old ZIP folder (copies into AppData; originals kept).
5. Launch from the Start Menu.

Silent / automation: `/VERYSILENT` installs cleanly with no portable path. Optional migrate: `/PORTABLEDIR="D:\path\to\old\portable"`.

### Paths

| Kind | Location |
|------|----------|
| Application (binaries, catalog, packaged tools) | Install dir (e.g. `%LOCALAPPDATA%\Programs\Sonic Colors Recomp`) |
| User data (config, NAND/saves, mods, Music Library) | `%APPDATA%\SonicColorsRecomp` |
| Music drop-in tools (ffmpeg, vgmstream) | `%APPDATA%\SonicColorsRecomp\tools\bin` |
| Managed Music deps (auto Install Music tools) | `%LOCALAPPDATA%\SonicColorsRecomp\dependencies` |
| Game data extracted by the launcher | `%APPDATA%\SonicColorsRecomp\game\…` |

The install tree contains `installed.txt` and **must not** contain `portable.txt`.
That marker forces user data into AppData even if someone later drops a `config.ini` beside the exe.

## Upgrade

- Running a newer Setup with the same AppId upgrades in place (same Apps & Features entry).
- In-app **Install & relaunch** still applies a downloaded ZIP over the install directory when it is writable (per-user LocalAppData installs are).
- User data and `%APPDATA%\…\tools\bin` are not overwritten by upgrades.

## Uninstall

Use Apps & Features / Start Menu → Uninstall.

- Removes the install directory and shortcuts.
- Asks whether to also delete `%APPDATA%\SonicColorsRecomp` (saves, mods, music). Default is **keep**.

## Portable ZIP (optional)

`tools/package_windows_cross.sh` still builds a ZIP with `portable.txt` for side-by-side / USB use.
Prefer the installer for normal desktops.

Force portable behaviour on an installed build with `--portable` or `SCR_PORTABLE=1` (writes beside the exe; needs a writable install dir).

## Packaging (developers)

```bash
cmake --build --preset windows-mingw --target SonicColorsRecomp music_asset
./tools/package_windows_installer.sh -v 0.3.7   # stage + Setup.exe (Wine+Inno or native ISCC)
./tools/package_windows_cross.sh -v 0.3.7       # optional portable ZIP
./tools/package_windows_install_check.sh
```

Inno Setup is bootstrapped under Wine into `tools/vendor/innosetup/` when needed (gitignored).

## Migration from portable

Migration is **opt-in** (wizard radio or `/PORTABLEDIR=`). New users never see a mandatory portable-directory prompt.

When selected, Setup copies `config.ini`, `nand/`, `mods/`, `music_mods/`, profiles, `game/`, and `tools/bin` into AppData. It does **not** delete the portable folder. Regenerable caches (`shadercache`, `music_preview`, `downloads`) are skipped. Invalid folders show an error in the GUI (or are skipped under silent `/PORTABLEDIR=` so install still completes).

```bash
./tools/package_windows_installer_test.sh   # Wine: fresh / migrate / bad-migrate / Unicode path
```
