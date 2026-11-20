"""Locate the CriCodecs-enabled Python interpreter / CLI and host binaries."""

from __future__ import annotations

import sys
from pathlib import Path

_PKG = Path(__file__).resolve().parent
_TOOLS = _PKG.parent
_ROOT = _TOOLS.parent if _TOOLS.name == "tools" else _TOOLS

VENV_PY = _ROOT / ".venv-music" / "bin" / "python"
VENV_PY_WIN = _ROOT / ".venv-music" / "Scripts" / "python.exe"
VENV_PY_TOOLS = _TOOLS / ".venv-music" / "bin" / "python"
VENV_PY_TOOLS_WIN = _TOOLS / ".venv-music" / "Scripts" / "python.exe"


def python() -> str:
    for p in (VENV_PY_WIN, VENV_PY, VENV_PY_TOOLS_WIN, VENV_PY_TOOLS):
        if p.is_file():
            return str(p)
    return sys.executable


def ensure_cricodecs() -> None:
    """Import cricodecs or raise a clear error."""
    try:
        import cricodecs  # noqa: F401
        return
    except ImportError:
        pass
    # Prefer project / install venv site-packages even if this interpreter differs
    candidates = [
        _ROOT / ".venv-music" / "lib",
        _ROOT / ".venv-music" / "Lib",
        _TOOLS / ".venv-music" / "lib",
        _TOOLS / ".venv-music" / "Lib",
    ]
    for sp in candidates:
        if not sp.is_dir():
            continue
        for p in list(sp.glob("python*/site-packages")) + list(sp.glob("site-packages")):
            sys.path.insert(0, str(p))
            try:
                import cricodecs  # noqa: F401
                return
            except ImportError:
                continue
    raise RuntimeError(
        "cricodecs is required for WAV→AAX / CPK patching. "
        "Run tools/music_deps_setup.sh (Linux) or tools\\music_deps_setup.cmd (Windows), "
        "or: python -m venv .venv-music && pip install cricodecs"
    )


def which_ffmpeg() -> str | None:
    from .doctor import which_ffmpeg as _which

    return _which()


def which_vgmstream() -> str | None:
    from .doctor import which_vgmstream as _which

    return _which()
