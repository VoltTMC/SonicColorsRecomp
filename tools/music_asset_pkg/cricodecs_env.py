"""Locate the CriCodecs-enabled Python interpreter / CLI."""

from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VENV_PY = ROOT / ".venv-music" / "bin" / "python"
VENV_CLI = ROOT / ".venv-music" / "bin" / "cricodecs"


def python() -> str:
    if VENV_PY.is_file():
        return str(VENV_PY)
    return sys.executable


def ensure_cricodecs() -> None:
    """Import cricodecs or raise a clear error."""
    try:
        import cricodecs  # noqa: F401
        return
    except ImportError:
        pass
    # Prefer project venv site-packages even if this interpreter differs
    sp = ROOT / ".venv-music" / "lib"
    if sp.is_dir():
        for p in sp.glob("python*/site-packages"):
            sys.path.insert(0, str(p))
            try:
                import cricodecs  # noqa: F401
                return
            except ImportError:
                continue
    raise RuntimeError(
        "cricodecs is required for WAV→AAX / CPK patching. "
        "Create it with: uv venv .venv-music --python /usr/bin/python3 && "
        "uv pip install --python .venv-music/bin/python cricodecs"
    )


def which_ffmpeg() -> str | None:
    return shutil.which("ffmpeg") or shutil.which("ffmpeg.exe")
