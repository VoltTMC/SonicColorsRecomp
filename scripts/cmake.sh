#!/usr/bin/env bash
# Cursor's AppImage makes stock cmake look for Modules under ~/.local/share/cmake.
export CMAKE_ROOT="${CMAKE_ROOT:-/usr/share/cmake}"
exec /usr/bin/cmake "$@"
