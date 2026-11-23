# Packaged portable DXBC shader cache (A3w / A3z)

This directory is the **shipping** copy of the offline-precompiled shader cache.

| Contents | Role |
|---|---|
| `vs_keys.bin` / `ps_keys.bin` | GX shader key lists for startup preload |
| `*.cso` | Portable HLSL→DXBC blobs (source-hash named) |
| `manifest.json` | Schema / generator / key-size compatibility |

At runtime, `Renderer_Init` → `PreloadShaders()` loads these automatically (no CLI flags).  
Missing variants still compile at runtime. Diagnostics: `--no-shader-preload` / `SCR_SHADER_PRELOAD=0`.

Package scripts (`tools/package_linux.sh`, `tools/package.ps1`,
`tools/package_windows_cross.sh`, `tools/package_windows_installer.sh`) copy this
tree next to the executable as `shadercache/`.

Do **not** put copyrighted game assets here — only compiler output derived from GX keys.
