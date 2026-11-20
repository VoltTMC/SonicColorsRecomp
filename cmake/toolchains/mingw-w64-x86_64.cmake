# Cross-compile Sonic Colors Recomp for Windows x86_64 from Linux (MinGW-w64 GCC).
# Used by the windows-mingw CMake preset.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)
set(CMAKE_AR           x86_64-w64-mingw32-ar)
set(CMAKE_RANLIB       x86_64-w64-mingw32-ranlib)
set(CMAKE_NM           x86_64-w64-mingw32-nm)
set(CMAKE_OBJCOPY      x86_64-w64-mingw32-objcopy)
set(CMAKE_OBJDUMP      x86_64-w64-mingw32-objdump)
set(CMAKE_STRIP        x86_64-w64-mingw32-strip)
set(CMAKE_ADDR2LINE    x86_64-w64-mingw32-addr2line)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Redistributable exe: no libgcc/libstdc++ DLLs required on the target machine.
set(CMAKE_CXX_FLAGS_INIT "-fms-extensions")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")
