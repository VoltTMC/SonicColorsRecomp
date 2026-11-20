"""Locate the CriCodecs-enabled Python interpreter / CLI and host binaries."""

from __future__ import annotations

import os
import sys
from pathlib import Path

_PKG = Path(__file__).resolve().parent
_TOOLS = _PKG.parent
_ROOT = _TOOLS.parent if _TOOLS.name == "tools" else _TOOLS


def _user_data_roots() -> list[Path]:
    roots: list[Path] = []
    for key in ("SCR_USER_DIR", "APPDATA", "XDG_DATA_HOME"):
        v = os.environ.get(key)
        if not v:
            continue
        p = Path(v)
        if key != "SCR_USER_DIR":
            p = p / "SonicColorsRecomp"
        if p not in roots:
            roots.append(p)
    return roots


def _venv_pythons() -> list[Path]:
    out: list[Path] = []
    for root in [*_user_data_roots(), _ROOT, _TOOLS]:
        out.extend(
            [
                root / ".venv-music" / "Scripts" / "python.exe",
                root / ".venv-music" / "bin" / "python",
            ]
        )
    return out


def python() -> str:
    for p in _venv_pythons():
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
    # Prefer project / user-data / install venv site-packages even if this interpreter differs
    candidates: list[Path] = []
    for root in [*_user_data_roots(), _ROOT, _TOOLS]:
        candidates.extend([root / ".venv-music" / "lib", root / ".venv-music" / "Lib"])
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
