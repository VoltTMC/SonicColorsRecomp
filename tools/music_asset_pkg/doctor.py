"""Music Library dependency discovery and capability report."""

from __future__ import annotations

import json
import os
import shutil
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


def which_in_app(names: list[str]) -> str | None:
    for d in _bin_dirs():
        for name in names:
            p = d / name
            if p.is_file() and os.access(p, os.X_OK):
                return str(p.resolve())
            # Windows: allow non-+x marked files
            if p.is_file() and (name.endswith(".exe") or os.name == "nt"):
                return str(p.resolve())
    for name in names:
        hit = shutil.which(name)
        if hit:
            return hit
    return None


def which_ffmpeg() -> str | None:
    return which_in_app(["ffmpeg", "ffmpeg.exe"])


def which_vgmstream() -> str | None:
    return which_in_app(["vgmstream-cli", "vgmstream_cli", "vgmstream123", "vgmstream-cli.exe", "vgmstream_cli.exe"])


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


def cricodecs_status() -> dict:
    info: dict = {"ok": False, "module": None, "error": None, "python": sys.executable}
    try:
        import cricodecs  # noqa: F401

        info["ok"] = True
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

    # Probe known venv site-packages without switching interpreter
    from . import cricodecs_env

    try:
        cricodecs_env.ensure_cricodecs()
        import cricodecs  # noqa: F401

        info["ok"] = True
        info["module"] = getattr(cricodecs, "__file__", "cricodecs")
        info["via"] = "venv site-packages on path"
        return info
    except Exception as e:
        info["error"] = str(e)

    vp = _venv_python()
    if vp:
        info["suggested_python"] = str(vp)
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
    "preview_original": {
        "label": "Preview original AAX from game CPK",
        "needs": ["python", "cricodecs", "vgmstream"],
        "notes": "cricodecs extracts AAX; vgmstream decodes to WAV.",
    },
    "export_original": {
        "label": "Export original AAX → WAV",
        "needs": ["python", "cricodecs", "vgmstream", "ffmpeg?"],
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
        "executable": sys.executable,
        "version": sys.version.split()[0],
        "path": sys.path[:6],
    }
    ff = which_ffmpeg()
    vg = which_vgmstream()
    cr = cricodecs_status()
    tool_bin = [str(d) for d in _bin_dirs()]

    deps = {
        "python": py,
        "cricodecs": cr,
        "ffmpeg": {"ok": bool(ff), "path": ff},
        "vgmstream": {"ok": bool(vg), "path": vg},
        "app_bin_dirs": tool_bin,
        "tools_dir": str(_TOOLS),
        "root": str(_ROOT),
    }

    have = {
        "python": True,
        "cricodecs": bool(cr.get("ok")),
        "ffmpeg": bool(ff),
        "vgmstream": bool(vg),
        "ffmpeg?": True,  # optional soft dep — mark missing only in detail
    }

    ops_out = []
    for key, meta in OPS.items():
        missing = []
        for n in meta["needs"]:
            if n == "ffmpeg?":
                if not ff:
                    missing.append("ffmpeg (optional for some exports)")
                continue
            if not have.get(n, False):
                missing.append(n)
        ops_out.append(
            {
                "id": key,
                "label": meta["label"],
                "ok": not missing,
                "missing": missing,
                "notes": meta["notes"],
            }
        )

    ready = [o for o in ops_out if o["ok"]]
    blocked = [o for o in ops_out if not o["ok"]]
    summary_bits = []
    for name in ("cricodecs", "ffmpeg", "vgmstream"):
        summary_bits.append(f"{name}={'ok' if deps[name].get('ok') else 'missing'}")
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
            "bundling": (
                "Redistributables do not ship FFmpeg (often GPL), vgmstream, Python, or cricodecs. "
                "Place ISC-licensed vgmstream-cli and an LGPL FFmpeg build in tools/bin/, "
                "and run music_deps_setup to create a local venv with cricodecs."
            ),
        },
    }


def main_print() -> int:
    report = doctor()
    print(json.dumps(report, indent=2))
    return 0 if report.get("ok") else 1
