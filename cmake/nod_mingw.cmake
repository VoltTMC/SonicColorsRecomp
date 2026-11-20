# Build nod (https://github.com/encounter/nod) for MinGW as a static library via Cargo.
# The official Windows prebuilt is MSVC (needs VCRUNTIME140.dll); a windows-gnu build does not.
find_program(SCR_CARGO cargo REQUIRED)
find_program(SCR_RUSTC rustc REQUIRED)

include(FetchContent)
FetchContent_Declare(nod_src
    URL https://github.com/encounter/nod/archive/refs/tags/${NOD_VERSION}.tar.gz
    URL_HASH SHA256=e696c90d37cbf9f1daf9edafca021c2a4006a9b9d7261af6f573f602522aa5cc
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
# Populate only — do not add_subdirectory (nod's CMakeLists pulls Corrosion for MSVC/host builds).
FetchContent_GetProperties(nod_src)
if(NOT nod_src_POPULATED)
    FetchContent_Populate(nod_src)
endif()

set(SCR_NOD_GNU_DIR ${CMAKE_BINARY_DIR}/nod-gnu)
set(SCR_NOD_GNU_LIB ${SCR_NOD_GNU_DIR}/x86_64-pc-windows-gnu/release/libnod.a)
set(SCR_NOD_GNU_INC ${nod_src_SOURCE_DIR}/nod-ffi/include)

execute_process(
    COMMAND ${SCR_RUSTC} --print target-list
    OUTPUT_VARIABLE _scr_rust_targets
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _scr_rust_targets MATCHES "x86_64-pc-windows-gnu")
    execute_process(COMMAND rustup target add x86_64-pc-windows-gnu RESULT_VARIABLE _scr_rustup_rc)
    if(NOT _scr_rustup_rc EQUAL 0)
        message(FATAL_ERROR "MinGW nod: install Rust target x86_64-pc-windows-gnu (rustup target add x86_64-pc-windows-gnu)")
    endif()
endif()

# Clean PATH: Cursor AppImage wrappers break rustc/cargo/gcc.
add_custom_command(OUTPUT ${SCR_NOD_GNU_LIB}
    COMMAND ${CMAKE_COMMAND} -E env
            HOME=$ENV{HOME}
            PATH=/usr/bin:/bin:$ENV{HOME}/.cargo/bin
            CARGO_TARGET_DIR=${SCR_NOD_GNU_DIR}
            ${SCR_CARGO} build -p nod-ffi --release --target x86_64-pc-windows-gnu
            --no-default-features
            --features compress-bzip2-static,compress-lzma-static,compress-zlib-static,compress-zstd-static,threading
    WORKING_DIRECTORY ${nod_src_SOURCE_DIR}
    COMMENT "Building nod (windows-gnu) with Cargo"
    VERBATIM)

add_custom_target(scr_nod_mingw DEPENDS ${SCR_NOD_GNU_LIB})

# INTERFACE wrapper so dependents wait for the Cargo build (IMPORTED alone does not).
add_library(scr_nod INTERFACE)
add_dependencies(scr_nod scr_nod_mingw)
target_include_directories(scr_nod INTERFACE ${SCR_NOD_GNU_INC})
target_link_libraries(scr_nod INTERFACE
    ${SCR_NOD_GNU_LIB} ws2_32 userenv ntdll bcrypt bcryptprimitives)

set(SCR_NOD_LINK scr_nod)
unset(SCR_NOD_DLL CACHE)
set(SCR_NOD_DLL "")  # statically linked — no nod.dll to ship
