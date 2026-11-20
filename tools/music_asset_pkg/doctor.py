"""Music Library dependency discovery and capability report."""

from __future__ import annotations

import json
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

# tools/music_asset_pkg → tools → install root (repo or redistributable)
_PKG = Path(__file__).resolve().parent
_TOOLS = _PKG.parent
_ROOT = _TOOLS.parent if _TOOLS.name == "tools" else _TOOLS


def _bin_dirs() -> list[Path]:
    """Application-managed binary search paths (before system PATH).

    Priority (matches tools/bin/README + Windows installed layout):
      1. SCR_MUSIC_BIN (explicit override; launcher sets this to user-data tools/bin)
      2. Packaged tools/bin beside music_asset_pkg
      3. User-data drop-in dirs (AppData / XDG) when distinct from the above
      4. PATH (unless SCR_MUSIC_APP_BIN_ONLY)
    """
    dirs: list[Path] = []
    env_bin = Path(os.environ["SCR_MUSIC_BIN"]) if os.environ.get("SCR_MUSIC_BIN") else None
    managed = Path(os.environ["SCR_MUSIC_MANAGED_BIN"]) if os.environ.get("SCR_MUSIC_MANAGED_BIN") else None
    appdata = os.environ.get("APPDATA") or os.environ.get("XDG_DATA_HOME")
    localapp = os.environ.get("LOCALAPPDATA")
    xdg = os.environ.get("XDG_DATA_HOME")
    packaged = [
        _TOOLS / "bin",
        _ROOT / "tools" / "bin",
        _ROOT / "bin",
    ]
    user = []
    if appdata:
        user.append(Path(appdata) / "SonicColorsRecomp" / "tools" / "bin")
    if localapp:
        user.append(Path(localapp) / "SonicColorsRecomp" / "tools" / "bin")
        # Launcher-managed downloads (Windows auto-setup).
        user.append(Path(localapp) / "SonicColorsRecomp" / "dependencies" / "current" / "bin")
    if xdg and not appdata:
        user.append(Path(xdg) / "SonicColorsRecomp" / "tools" / "bin")
    # Linux default when XDG_DATA_HOME unset
    if os.name != "nt" and not xdg:
        user.append(Path.home() / ".local" / "share" / "SonicColorsRecomp" / "tools" / "bin")

    # Priority: SCR_MUSIC_BIN → user tools/bin → managed deps → packaged tools/bin → PATH
    for d in [env_bin, *user, managed, *packaged]:
        if not d:
            continue
        try:
            d = d.expanduser()
        except Exception:
            pass
        if d.is_dir() and d not in dirs:
            dirs.append(d)
    return dirs


def _app_bin_only() -> bool:
    """When set, ignore system PATH (clean-package / isolation tests)."""
    v = os.environ.get("SCR_MUSIC_APP_BIN_ONLY", "").strip().lower()
    return v in ("1", "true", "yes", "on")


def _windows_names(names: list[str]) -> list[str]:
    """Prefer .exe forms first on Windows so drop-ins resolve without PATHEXT tricks."""
    if os.name != "nt":
        return names
    out: list[str] = []
    seen: set[str] = set()
    for n in names:
        variants = []
        if n.lower().endswith(".exe"):
            variants = [n, n[:-4]]
        else:
            variants = [n + ".exe", n]
        for v in variants:
            if v not in seen:
                seen.add(v)
                out.append(v)
    return out


def find_in_app(names: list[str]) -> tuple[str | None, list[str]]:
    """Locate a host binary. Returns (absolute path or None, dirs/paths searched)."""
    names = _windows_names(names)
    searched: list[str] = []
    for d in _bin_dirs():
        searched.append(str(d))
        for name in names:
            p = d / name
            try:
                if not p.is_file():
                    continue
            except OSError:
                continue
            # Windows: os.access(..., X_OK) is unreliable; accept regular files.
            if os.name == "nt" or name.lower().endswith(".exe") or os.access(p, os.X_OK):
                # Keep the tools/bin path (do not resolve symlinks away to /usr/bin).
                try:
                    return str(p.absolute()), searched
                except OSError:
                    return str(p), searched
    if _app_bin_only():
        searched.append("PATH (skipped: SCR_MUSIC_APP_BIN_ONLY)")
        return None, searched
    searched.append("PATH")
    for name in names:
        hit = shutil.which(name)
        if hit:
            return hit, searched
    return None, searched


def which_in_app(names: list[str]) -> str | None:
    path, _ = find_in_app(names)
    return path


def which_ffmpeg() -> str | None:
    info = probe_ffmpeg()
    return info.get("path") if info.get("ok") else None


def which_vgmstream() -> str | None:
    info = probe_vgmstream()
    return info.get("path") if info.get("ok") else None


def missing_tool_message(label: str, names: list[str], *, searched: list[str] | None = None) -> str:
    if searched is None:
        _, searched = find_in_app(names)
    drop = (
        str(Path(os.environ["APPDATA"]) / "SonicColorsRecomp" / "tools" / "bin")
        if os.environ.get("APPDATA")
        else str(_TOOLS / "bin")
    )
    win_hint = "vgmstream-cli.exe" if "vgmstream" in label.lower() else "ffmpeg.exe"
    lines = [
        f"{label} not found.",
        f"Place the executable (e.g. {win_hint} on Windows) in:",
        f"  {drop}",
        "or set SCR_MUSIC_BIN to a directory containing it, or add it to PATH.",
        "Searched:",
    ]
    for s in searched:
        lines.append(f"  - {s}")
    return "\n".join(lines)


def _venv_python() -> Path | None:
    from .cricodecs_env import _venv_pythons

    for p in _venv_pythons():
        if p.is_file():
            return p
    return None


def run_host_binary(cmd: list[str], *, timeout: float | None = None) -> subprocess.CompletedProcess[str]:
    """Run a resolved host tool with DLL-friendly PATH and no console window on Windows."""
    env = os.environ.copy()
    if cmd:
        tool_dir = str(Path(cmd[0]).resolve().parent)
        env["PATH"] = tool_dir + os.pathsep + env.get("PATH", "")
    kwargs: dict = {
        "capture_output": True,
        "text": True,
        "env": env,
    }
    if timeout is not None:
        kwargs["timeout"] = timeout
    if os.name == "nt":
        # Avoid flashing a console when the GUI launcher decodes previews.
        kwargs["creationflags"] = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    return subprocess.run(cmd, **kwargs)


def _run_version(cmd: list[str], *, timeout: float = 8.0) -> tuple[int | None, str]:
    try:
        r = run_host_binary(cmd, timeout=timeout)
        out = ((r.stdout or "") + "\n" + (r.stderr or "")).strip()
        return r.returncode, out[-2000:]
    except FileNotFoundError:
        return None, "not executable / FileNotFoundError"
    except PermissionError:
        return None, "permission denied"
    except subprocess.TimeoutExpired:
        return None, "timed out"
    except OSError as e:
        return None, f"OSError: {e}"


def _probe_tool(
    *,
    names: list[str],
    version_args: list[str],
    version_re: re.Pattern[str],
    label: str,
) -> dict:
    info: dict = {
        "ok": False,
        "status": "missing",
        "path": None,
        "version": None,
        "error": None,
        "label": label,
        "host": f"{platform.system()}-{platform.machine()}",
        "searched": [],
    }
    path, searched = find_in_app(names)
    info["searched"] = searched
    if not path:
        info["error"] = missing_tool_message(label, names, searched=searched)
        return info

    info["path"] = path
    p = Path(path)
    if not p.is_file():
        info["status"] = "missing"
        info["error"] = "path does not exist"
        return info

    try:
        size = p.stat().st_size
    except OSError as e:
        info["status"] = "unusable"
        info["error"] = str(e)
        return info
    if size == 0:
        info["status"] = "unusable"
        info["error"] = "file is empty"
        return info

    rc, out = _run_version([path, *version_args])
    if rc is None:
        info["status"] = "unusable"
        info["error"] = out
        return info
    m = version_re.search(out)
    if m:
        info["version"] = m.group(1)
        info["ok"] = True
        info["status"] = "ok"
        info["probe_output"] = out[:400]
        return info
    if rc != 0 and not out:
        info["status"] = "unusable"
        info["error"] = f"exit {rc} with empty output"
        return info
    if rc == 0:
        info["ok"] = True
        info["status"] = "ok"
        info["version"] = "unknown"
        info["probe_output"] = out[:400]
        info["warning"] = "ran successfully but version string not recognised"
        return info
    info["status"] = "unusable"
    info["error"] = f"exit {rc}: {out[:300]}"
    return info


def probe_ffmpeg() -> dict:
    return _probe_tool(
        names=["ffmpeg", "ffmpeg.exe"],
        version_args=["-version"],
        version_re=re.compile(r"ffmpeg\s+version\s+(\S+)", re.I),
        label="ffmpeg",
    )


def probe_vgmstream() -> dict:
    names = [
        "vgmstream-cli",
        "vgmstream_cli",
        "vgmstream123",
        "vgmstream-cli.exe",
        "vgmstream_cli.exe",
        "vgmstream123.exe",
    ]
    info = _probe_tool(
        names=names,
        version_args=["-h"],
        version_re=re.compile(r"vgmstream(?:-cli)?\s+(\d[\w.\-]*)", re.I),
        label="vgmstream",
    )
    if info.get("ok"):
        return info
    path = info.get("path")
    if path:
        for args in (["-v"], ["--version"], []):
            rc, out = _run_version([path, *args] if args else [path])
            if rc is None:
                continue
            m = re.search(r"vgmstream(?:-cli)?\s+(\d[\w.\-]*)", out, re.I)
            if m or (rc in (0, 1) and "usage" in out.lower()):
                info["ok"] = True
                info["status"] = "ok"
                info["version"] = m.group(1) if m else "unknown"
                info["error"] = None
                info["probe_output"] = out[:400]
                return info
    return info


def cricodecs_status() -> dict:
    info: dict = {"ok": False, "status": "missing", "module": None, "error": None, "python": sys.executable}
    try:
        import cricodecs  # noqa: F401

        info["ok"] = True
        info["status"] = "ok"
        info["module"] = getattr(cricodecs, "__file__", "cricodecs")
        try:
            import importlib.metadata as md

            info["version"] = md.version("cricodecs")
            lic = md.metadata("cricodecs").get("License")
            if lic:
                info["license"] = lic
        except Exception:
            pass
        return info
    except ImportError as e:
        info["error"] = str(e)

    from . import cricodecs_env

    try:
        cricodecs_env.ensure_cricodecs()
        import cricodecs  # noqa: F401

        info["ok"] = True
        info["status"] = "ok"
        info["module"] = getattr(cricodecs, "__file__", "cricodecs")
        info["via"] = "venv site-packages on path"
        try:
            import importlib.metadata as md

            info["version"] = md.version("cricodecs")
        except Exception:
            pass
        return info
    except Exception as e:
        info["error"] = str(e)
        info["status"] = "missing"

    vp = _venv_python()
    if vp:
        info["suggested_python"] = str(vp)
        info["error"] = (
            f"cricodecs not importable in {sys.executable}; "
            f"try {vp} or re-run music_deps_setup"
        )
    return info


# Operation → required deps (tool launcher always needs a Python that can import the pkg).
OPS: dict[str, dict] = {
    "browse_catalog": {
        "label": "Browse Music Library catalog",
        "needs": [],
        "notes": "Uses packaged mods/music/catalog.json only.",
    },
    "replacement_wav_preview": {
        "label": "Preview assigned replacement WAV",
        "needs": [],
        "notes": "Decoded in-process; no host tools.",
    },
    "in_game_replacement": {
        "label": "Play with prepared music replacements",
        "needs": [],
        "notes": "Uses music_mods cache after Prepare; no host tools at play time.",
    },
    "preview_original": {
        "label": "Preview original AAX from game CPK",
        "needs": ["python", "cricodecs", "vgmstream"],
        "notes": "cricodecs extracts AAX; vgmstream decodes to WAV.",
    },
    "export_original": {
        "label": "Export original AAX → WAV",
        "needs": ["python", "cricodecs", "vgmstream"],
        "optional": ["ffmpeg"],
        "notes": "ffmpeg only when PCM bit-depth/layout normalize is required.",
    },
    "prepare_replacements": {
        "label": "Prepare replacements (WAV→ADX/AAX→patched CPK)",
        "needs": ["python", "cricodecs", "ffmpeg"],
        "notes": "ffmpeg resamples WAVs to 48 kHz; cricodecs builds AAX + CPK TOC patch.",
    },
    "catalog_rebuild": {
        "label": "Rebuild catalog.json from CPK",
        "needs": ["python", "cricodecs"],
        "notes": "Not needed when packaged catalog.json is present.",
    },
}


def doctor() -> dict:
    py = {
        "ok": True,
        "status": "ok",
        "executable": sys.executable,
        "version": sys.version.split()[0],
        "path": sys.path[:6],
    }
    ff = probe_ffmpeg()
    vg = probe_vgmstream()
    cr = cricodecs_status()
    tool_bin = [str(d) for d in _bin_dirs()]

    managed_bin = os.environ.get("SCR_MUSIC_MANAGED_BIN")
    localapp = os.environ.get("LOCALAPPDATA")
    managed_root = (
        str(Path(localapp) / "SonicColorsRecomp" / "dependencies") if localapp else None
    )
    managed_diag = {
        "root": managed_root,
        "bin": managed_bin
        or (str(Path(managed_root) / "current" / "bin") if managed_root else None),
        "python": os.environ.get("SCR_MUSIC_PYTHON")
        or (
            str(Path(managed_root) / "python" / "3.12.8" / "python.exe") if managed_root else None
        ),
        "vgmstream": None,
        "ffmpeg": None,
    }
    if managed_diag["bin"]:
        mb = Path(managed_diag["bin"])
        for name, key in (("vgmstream-cli.exe", "vgmstream"), ("ffmpeg.exe", "ffmpeg")):
            p = mb / name
            managed_diag[key] = {
                "path": str(p),
                "present": p.is_file(),
            }

    deps = {
        "python": py,
        "cricodecs": cr,
        "ffmpeg": ff,
        "vgmstream": vg,
        "app_bin_dirs": tool_bin,
        "app_bin_only": _app_bin_only(),
        "tools_dir": str(_TOOLS),
        "root": str(_ROOT),
        "scr_music_bin": os.environ.get("SCR_MUSIC_BIN"),
        "scr_music_managed_bin": managed_bin,
        "scr_music_python": os.environ.get("SCR_MUSIC_PYTHON"),
        "managed": managed_diag,
    }

    have = {
        "python": True,
        "cricodecs": bool(cr.get("ok")),
        "ffmpeg": bool(ff.get("ok")),
        "vgmstream": bool(vg.get("ok")),
    }

    ops_out = []
    for key, meta in OPS.items():
        missing = []
        warnings = []
        for n in meta["needs"]:
            if not have.get(n, False):
                missing.append(n)
        for n in meta.get("optional", []):
            if not have.get(n, False):
                warnings.append(f"{n} (optional)")
        ops_out.append(
            {
                "id": key,
                "label": meta["label"],
                "ok": not missing,
                "missing": missing,
                "warnings": warnings,
                "notes": meta["notes"],
            }
        )

    ready = [o for o in ops_out if o["ok"]]
    blocked = [o for o in ops_out if not o["ok"]]
    summary_bits = []
    for name in ("cricodecs", "ffmpeg", "vgmstream"):
        st = deps[name].get("status") or ("ok" if deps[name].get("ok") else "missing")
        summary_bits.append(f"{name}={st}")
    return {
        "ok": len(blocked) == 0,
        "summary": ", ".join(summary_bits),
        "deps": deps,
        "operations": ops_out,
        "ready": [o["id"] for o in ready],
        "blocked": [o["id"] for o in blocked],
        "setup": {
            "drop_in_bin": str(_TOOLS / "bin"),
            "drop_in_user": (
                str(Path(os.environ["APPDATA"]) / "SonicColorsRecomp" / "tools" / "bin")
                if os.environ.get("APPDATA")
                else str(Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share")) / "SonicColorsRecomp" / "tools" / "bin")
            ),
            "drop_in_readme": str(_TOOLS / "bin" / "README.md"),
            "venv_hint_linux": "tools/music_deps_setup.sh",
            "venv_hint_windows": "tools\\music_deps_setup.cmd",
            "steps": _setup_steps(deps, blocked),
            "bundling": (
                "Installers/redistributables do not ship FFmpeg, vgmstream, Python, or cricodecs. "
                "On Windows the launcher can download pinned LGPL FFmpeg + vgmstream + Python embed + cricodecs "
                "into %LOCALAPPDATA%\\SonicColorsRecomp\\dependencies. "
                "User drop-ins in %APPDATA%\\SonicColorsRecomp\\tools\\bin still override managed copies."
            ),
            "managed_root": (
                str(Path(os.environ["LOCALAPPDATA"]) / "SonicColorsRecomp" / "dependencies")
                if os.environ.get("LOCALAPPDATA")
                else None
            ),
        },
    }


def _setup_steps(deps: dict, blocked: list) -> list[str]:
    steps: list[str] = []
    user_bin = (
        str(Path(os.environ["APPDATA"]) / "SonicColorsRecomp" / "tools" / "bin")
        if os.environ.get("APPDATA")
        else "user-data/tools/bin"
    )
    if not deps["cricodecs"].get("ok"):
        steps.append(
            "Run tools/music_deps_setup.sh (Linux) or tools\\music_deps_setup.cmd (Windows) "
            "to create .venv-music and install cricodecs."
        )
    if not deps["ffmpeg"].get("ok"):
        steps.append(
            f"Install an LGPL ffmpeg build into {user_bin} (installed) or tools/bin/ (portable), "
            "or on PATH. Needed for Prepare."
        )
    if not deps["vgmstream"].get("ok"):
        st = deps["vgmstream"].get("status")
        if st == "unusable" and deps["vgmstream"].get("path"):
            steps.append(
                f"vgmstream at {deps['vgmstream']['path']} is unusable "
                f"({deps['vgmstream'].get('error')}). Replace it or install matching DLLs beside it."
            )
        else:
            steps.append(
                f"Install vgmstream-cli.exe into {user_bin} (installed) or tools/bin/ (portable), "
                "or on PATH. Needed for original preview/export. No reboot required — use Check tools."
            )
    if not steps and not blocked:
        steps.append("All Music Library host tools look usable. Re-run doctor after changing tools/bin.")
    elif blocked:
        steps.append("Re-run: music_asset doctor  (or Music tab → Check tools)")
    return steps


def main_print() -> int:
    report = doctor()
    print(json.dumps(report, indent=2))
    return 0 if report.get("ok") else 1
