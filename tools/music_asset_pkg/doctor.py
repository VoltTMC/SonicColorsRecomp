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
    """Application-managed binary search paths (before system PATH)."""
    dirs: list[Path] = []
    for d in (
        _TOOLS / "bin",
        _ROOT / "tools" / "bin",
        _ROOT / "bin",
        Path(os.environ["SCR_MUSIC_BIN"]) if os.environ.get("SCR_MUSIC_BIN") else None,
    ):
        if d and d.is_dir() and d not in dirs:
            dirs.append(d)
    return dirs


def _app_bin_only() -> bool:
    """When set, ignore system PATH (clean-package / isolation tests)."""
    v = os.environ.get("SCR_MUSIC_APP_BIN_ONLY", "").strip().lower()
    return v in ("1", "true", "yes", "on")


def which_in_app(names: list[str]) -> str | None:
    for d in _bin_dirs():
        for name in names:
            p = d / name
            # Keep the app-local path (do not resolve symlinks away from tools/bin).
            if p.is_file() and os.access(p, os.X_OK):
                return str(p)
            # Windows: allow non-+x marked files
            if p.is_file() and (name.endswith(".exe") or os.name == "nt"):
                return str(p)
    if _app_bin_only():
        return None
    for name in names:
        hit = shutil.which(name)
        if hit:
            return hit
    return None


def which_ffmpeg() -> str | None:
    info = probe_ffmpeg()
    return info.get("path") if info.get("ok") else None


def which_vgmstream() -> str | None:
    info = probe_vgmstream()
    return info.get("path") if info.get("ok") else None


def _venv_python() -> Path | None:
    cands = [
        _ROOT / ".venv-music" / "bin" / "python",
        _ROOT / ".venv-music" / "Scripts" / "python.exe",
        _TOOLS / ".venv-music" / "bin" / "python",
        _TOOLS / ".venv-music" / "Scripts" / "python.exe",
    ]
    for p in cands:
        if p.is_file():
            return p
    return None


def _run_version(cmd: list[str], *, timeout: float = 8.0) -> tuple[int | None, str]:
    try:
        r = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            env={**os.environ, "PATH": os.environ.get("PATH", "")},
        )
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
    }
    path = which_in_app(names)
    if not path:
        info["error"] = f"{label} not found in tools/bin or PATH"
        return info

    info["path"] = path
    p = Path(path)
    if not p.is_file():
        info["status"] = "missing"
        info["error"] = "path does not exist"
        return info

    # Reject obviously non-binary drop-ins (empty / tiny text stubs without shebang that fail)
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
    # ffmpeg -version → 0; some builds print help and exit 1 — accept version text either way
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
    # Executable ran but version string unexpected — still usable if exit 0
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
    # Prefer -h / no args that print help; many builds support -v or print on -h
    info = _probe_tool(
        names=["vgmstream-cli", "vgmstream_cli", "vgmstream123", "vgmstream-cli.exe", "vgmstream_cli.exe"],
        version_args=["-h"],
        version_re=re.compile(r"vgmstream(?:-cli)?\s+(\d[\w.\-]*)", re.I),
        label="vgmstream",
    )
    if info.get("ok"):
        return info
    # Fallback: some builds use -v / --version
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

    deps = {
        "python": py,
        "cricodecs": cr,
        "ffmpeg": ff,
        "vgmstream": vg,
        "app_bin_dirs": tool_bin,
        "app_bin_only": _app_bin_only(),
        "tools_dir": str(_TOOLS),
        "root": str(_ROOT),
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
            "drop_in_readme": str(_TOOLS / "bin" / "README.md"),
            "venv_hint_linux": "tools/music_deps_setup.sh",
            "venv_hint_windows": "tools\\music_deps_setup.cmd",
            "steps": _setup_steps(deps, blocked),
            "bundling": (
                "Redistributables do not ship FFmpeg (often GPL), vgmstream, Python, or cricodecs. "
                "Place ISC-licensed vgmstream-cli and an LGPL FFmpeg build in tools/bin/, "
                "and run music_deps_setup to create a local venv with cricodecs."
            ),
        },
    }


def _setup_steps(deps: dict, blocked: list) -> list[str]:
    steps: list[str] = []
    if not deps["cricodecs"].get("ok"):
        steps.append(
            "Run tools/music_deps_setup.sh (Linux) or tools\\music_deps_setup.cmd (Windows) "
            "to create .venv-music and install cricodecs."
        )
    if not deps["ffmpeg"].get("ok"):
        steps.append(
            f"Install an LGPL ffmpeg build and place it in {deps.get('tools_dir', 'tools')}/bin/ "
            "(or on PATH). Needed for Prepare."
        )
    if not deps["vgmstream"].get("ok"):
        steps.append(
            f"Install vgmstream-cli and place it in {deps.get('tools_dir', 'tools')}/bin/ "
            "(or on PATH). Needed for original preview/export."
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
