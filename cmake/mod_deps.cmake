# Libraries of the mod browser and installer (runtime/mods).
# libarchive (https://libarchive.org, BSD-2-Clause) unpacks downloaded mods (zip, rar, 7z),
# with zlib (zlib license) for zip and xz's liblzma (0BSD) for 7z. All three are built from
# source as static libraries the first time the project builds (ExternalProject, so each one
# configures with its own find logic) and linked as the imported target scr_archive.
include(ExternalProject)

set(SCR_DEPS_PREFIX ${CMAKE_BINARY_DIR}/deps)
set(SCR_DEPS_INSTALL ${SCR_DEPS_PREFIX}/install)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SCR_DEPS_RUNTIME MultiThreadedDebug)
    set(SCR_DEPS_CONFIG Debug)
else()
    set(SCR_DEPS_RUNTIME MultiThreaded)
    set(SCR_DEPS_CONFIG Release)
endif()
if(WIN32)
    # the C compiler of the matching toolchain (the project itself only enables C++ there)
    set(SCR_DEPS_C_COMPILER ${CMAKE_CXX_COMPILER})
else()
    set(SCR_DEPS_C_COMPILER ${CMAKE_C_COMPILER})
endif()
set(SCR_DEPS_ARGS
    -DCMAKE_C_COMPILER=${SCR_DEPS_C_COMPILER}
    -DCMAKE_BUILD_TYPE=${SCR_DEPS_CONFIG}
    -DCMAKE_INSTALL_PREFIX=${SCR_DEPS_INSTALL}
    -DCMAKE_PREFIX_PATH=${SCR_DEPS_INSTALL}
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
    -DCMAKE_MSVC_RUNTIME_LIBRARY=${SCR_DEPS_RUNTIME}
    -DBUILD_SHARED_LIBS=OFF)
# Linux: static libraries named lib*.a in lib/ (not lib64/), linkable into the PIE executable
if(WIN32)
    set(SCR_ZLIB_LIB_NAMES ${SCR_DEPS_INSTALL}/lib/zs.lib ${SCR_DEPS_INSTALL}/lib/zsd.lib)
    set(SCR_LZMA_LIB ${SCR_DEPS_INSTALL}/lib/lzma.lib)
    set(SCR_ARCHIVE_LIB ${SCR_DEPS_INSTALL}/lib/archive.lib)
else()
    list(APPEND SCR_DEPS_ARGS -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_POSITION_INDEPENDENT_CODE=ON)
    set(SCR_ZLIB_LIB_NAMES ${SCR_DEPS_INSTALL}/lib/libz.a)
    set(SCR_LZMA_LIB ${SCR_DEPS_INSTALL}/lib/liblzma.a)
    set(SCR_ARCHIVE_LIB ${SCR_DEPS_INSTALL}/lib/libarchive.a)
endif()

ExternalProject_Add(scr_zlib
    URL https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz
    URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    PREFIX ${SCR_DEPS_PREFIX}/zlib
    CMAKE_ARGS ${SCR_DEPS_ARGS} -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_TESTING=OFF
    BUILD_BYPRODUCTS ${SCR_ZLIB_LIB_NAMES})

ExternalProject_Add(scr_lzma
    URL https://github.com/tukaani-project/xz/releases/download/v5.8.4/xz-5.8.4.tar.gz
    URL_HASH SHA256=0014c7886930454fe8bd4228665b51af55eeae560ea135c9c4cd33f55b2591d9
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    PREFIX ${SCR_DEPS_PREFIX}/xz
    CMAKE_ARGS ${SCR_DEPS_ARGS} -DXZ_NLS=OFF -DXZ_DOC=OFF -DXZ_TOOL_XZ=OFF -DXZ_TOOL_XZDEC=OFF
               -DXZ_TOOL_LZMADEC=OFF -DXZ_TOOL_LZMAINFO=OFF -DXZ_TOOL_SCRIPTS=OFF -DXZ_TOOL_SYMLINKS=OFF
    BUILD_BYPRODUCTS ${SCR_LZMA_LIB})

if(NOT WIN32)
    set(SCR_ZLIB_LIB ${SCR_ZLIB_LIB_NAMES})
elseif(SCR_DEPS_CONFIG STREQUAL "Debug")
    set(SCR_ZLIB_LIB ${SCR_DEPS_INSTALL}/lib/zsd.lib)
else()
    set(SCR_ZLIB_LIB ${SCR_DEPS_INSTALL}/lib/zs.lib)
endif()
ExternalProject_Add(scr_libarchive
    URL https://github.com/libarchive/libarchive/releases/download/v3.8.9/libarchive-3.8.9.tar.gz
    URL_HASH SHA256=f5a6539059cf5e597dbeda37bfa4874b1e8dea063c8d93bf85a2b44af90a5bd4
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    PREFIX ${SCR_DEPS_PREFIX}/libarchive
    DEPENDS scr_zlib scr_lzma
    CMAKE_ARGS ${SCR_DEPS_ARGS}
               -DCMAKE_C_FLAGS=-DLZMA_API_STATIC
               -DZLIB_INCLUDE_DIR=${SCR_DEPS_INSTALL}/include -DZLIB_LIBRARY=${SCR_ZLIB_LIB}
               -DLIBLZMA_INCLUDE_DIR=${SCR_DEPS_INSTALL}/include -DLIBLZMA_LIBRARY=${SCR_LZMA_LIB}
               -DENABLE_ZLIB=ON -DENABLE_LZMA=ON -DENABLE_BZip2=OFF -DENABLE_ZSTD=OFF -DENABLE_LZ4=OFF
               -DENABLE_LIBB2=OFF -DENABLE_OPENSSL=OFF -DENABLE_MBEDTLS=OFF -DENABLE_NETTLE=OFF -DENABLE_CNG=OFF
               -DENABLE_LIBXML2=OFF -DENABLE_EXPAT=OFF -DENABLE_WIN32_XMLLITE=OFF -DENABLE_PCREPOSIX=OFF
               -DPOSIX_REGEX_LIB=NONE -DENABLE_PCRE2POSIX=OFF -DENABLE_LIBGCC=OFF -DENABLE_ICONV=OFF -DENABLE_ACL=OFF -DENABLE_XATTR=OFF
               -DENABLE_TAR=OFF -DENABLE_CPIO=OFF -DENABLE_CAT=OFF -DENABLE_UNZIP=OFF -DENABLE_TEST=OFF
    BUILD_BYPRODUCTS ${SCR_ARCHIVE_LIB})

file(MAKE_DIRECTORY ${SCR_DEPS_INSTALL}/include)
add_library(scr_archive INTERFACE)
add_dependencies(scr_archive scr_libarchive)
target_include_directories(scr_archive INTERFACE ${SCR_DEPS_INSTALL}/include)
target_compile_definitions(scr_archive INTERFACE LIBARCHIVE_STATIC)
target_link_libraries(scr_archive INTERFACE ${SCR_ARCHIVE_LIB} ${SCR_LZMA_LIB} ${SCR_ZLIB_LIB})

# nlohmann/json (https://github.com/nlohmann/json, MIT) reads GameBanana's API answers
# (the mod browser); a single header
include(FetchContent)
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp
    URL_HASH SHA256=aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63
    DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_MakeAvailable(nlohmann_json)
add_library(scr_json INTERFACE)
file(MAKE_DIRECTORY ${nlohmann_json_SOURCE_DIR}/include/nlohmann)
file(COPY ${nlohmann_json_SOURCE_DIR}/json.hpp DESTINATION ${nlohmann_json_SOURCE_DIR}/include/nlohmann)
target_include_directories(scr_json INTERFACE ${nlohmann_json_SOURCE_DIR}/include)
