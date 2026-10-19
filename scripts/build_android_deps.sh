#!/usr/bin/env bash
set -euo pipefail

# OpenXRay Android ARM64 Target Dependencies Build Script
# Builds: libogg, libvorbis, libtheora, liblzo2, SDL2, OpenAL Soft

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-/opt/android-ndk}"
ANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"
ANDROID_PLATFORM="${ANDROID_PLATFORM:-android-26}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

PREFIX="${ROOT_DIR}/android-deps/${ANDROID_ABI}"
BUILD_DIR="${ROOT_DIR}/build-deps-android/${ANDROID_ABI}"
SRC_DIR="${ROOT_DIR}/build-deps-src"
NUM_JOBS="$(nproc 2>/dev/null || echo 4)"

echo "=== OpenXRay Android Dependency Builder ==="
echo "NDK:            ${ANDROID_NDK_HOME}"
echo "ABI:            ${ANDROID_ABI}"
echo "Platform:       ${ANDROID_PLATFORM}"
echo "Build Type:     ${BUILD_TYPE}"
echo "Install Prefix: ${PREFIX}"
echo "Jobs:           ${NUM_JOBS}"
echo "==========================================="

if [[ ! -d "${ANDROID_NDK_HOME}" ]]; then
    echo "ERROR: Android NDK not found at ${ANDROID_NDK_HOME}" >&2
    exit 1
fi

TOOLCHAIN="${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake"
if [[ ! -f "${TOOLCHAIN}" ]]; then
    echo "ERROR: CMake toolchain not found at ${TOOLCHAIN}" >&2
    exit 1
fi

mkdir -p "${PREFIX}" "${BUILD_DIR}" "${SRC_DIR}"
export PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig:${PREFIX}/share/pkgconfig:${PKG_CONFIG_PATH:-}"

COMMON_CMAKE_ARGS=(
    -G Ninja
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}"
    -DANDROID_ABI="${ANDROID_ABI}"
    -DANDROID_PLATFORM="${ANDROID_PLATFORM}"
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DCMAKE_INSTALL_PREFIX="${PREFIX}"
    -DCMAKE_PREFIX_PATH="${PREFIX}"
    -DCMAKE_FIND_ROOT_PATH="${PREFIX}"
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH
)

# 1. libogg (v1.3.5)
echo "--- [1/6] Building libogg ---"
if [[ ! -f "${PREFIX}/lib/libogg.so" && ! -f "${PREFIX}/lib/libogg.a" ]]; then
    if [[ ! -d "${SRC_DIR}/ogg" ]]; then
        git clone --depth 1 --branch v1.3.5 https://github.com/xiph/ogg.git "${SRC_DIR}/ogg"
    fi
    mkdir -p "${BUILD_DIR}/ogg"
    cmake -B "${BUILD_DIR}/ogg" -S "${SRC_DIR}/ogg" \
        "${COMMON_CMAKE_ARGS[@]}" \
        -DBUILD_SHARED_LIBS=ON \
        -DINSTALL_DOCS=OFF
    ninja -C "${BUILD_DIR}/ogg" -j"${NUM_JOBS}"
    ninja -C "${BUILD_DIR}/ogg" install
else
    echo "libogg already installed, skipping."
fi

# 2. libvorbis (v1.3.7)
echo "--- [2/6] Building libvorbis ---"
if [[ ! -f "${PREFIX}/lib/libvorbis.so" && ! -f "${PREFIX}/lib/libvorbis.a" ]]; then
    if [[ ! -d "${SRC_DIR}/vorbis" ]]; then
        git clone --depth 1 --branch v1.3.7 https://github.com/xiph/vorbis.git "${SRC_DIR}/vorbis"
    fi
    mkdir -p "${BUILD_DIR}/vorbis"
    cmake -B "${BUILD_DIR}/vorbis" -S "${SRC_DIR}/vorbis" \
        "${COMMON_CMAKE_ARGS[@]}" \
        -DBUILD_SHARED_LIBS=ON
    ninja -C "${BUILD_DIR}/vorbis" -j"${NUM_JOBS}"
    ninja -C "${BUILD_DIR}/vorbis" install
else
    echo "libvorbis already installed, skipping."
fi

# 3. libtheora (v1.1.1)
echo "--- [3/6] Building libtheora ---"
if [[ ! -f "${PREFIX}/lib/libtheora.so" && ! -f "${PREFIX}/lib/libtheora.a" ]]; then
    if [[ ! -d "${SRC_DIR}/theora" ]]; then
        git clone --depth 1 --branch v1.1.1 https://github.com/xiph/theora.git "${SRC_DIR}/theora"
    fi
    # Provide CMakeLists.txt for libtheora
    cat << 'EOF' > "${SRC_DIR}/theora/CMakeLists.txt"
cmake_minimum_required(VERSION 3.15)
project(theora C)

include(GNUInstallDirs)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Ogg REQUIRED)
if (TARGET Ogg::Ogg)
    set(OGG_TARGET Ogg::Ogg)
elseif (TARGET Ogg::ogg)
    set(OGG_TARGET Ogg::ogg)
else()
    set(OGG_TARGET ${OGG_LIBRARIES})
endif()

set(THEORA_COMMON_SOURCES
    lib/apiwrapper.c
    lib/bitpack.c
    lib/cpu.c
    lib/dequant.c
    lib/fragment.c
    lib/idct.c
    lib/info.c
    lib/internal.c
    lib/state.c
    lib/quant.c
)

set(THEORA_DEC_SOURCES
    lib/decapiwrapper.c
    lib/decinfo.c
    lib/decode.c
    lib/huffdec.c
)

set(THEORA_ENC_SOURCES
    lib/analyze.c
    lib/encapiwrapper.c
    lib/encfrag.c
    lib/encinfo.c
    lib/encode.c
    lib/enquant.c
    lib/fdct.c
    lib/huffenc.c
    lib/mathops.c
    lib/mcenc.c
    lib/rate.c
    lib/tokenize.c
)

set(THEORA_HEADERS
    include/theora/codec.h
    include/theora/theora.h
    include/theora/theoradec.h
    include/theora/theoraenc.h
)

add_library(theora-common OBJECT ${THEORA_COMMON_SOURCES})
target_include_directories(theora-common PUBLIC include ${OGG_INCLUDE_DIRS})

add_library(theora-dec OBJECT ${THEORA_DEC_SOURCES})
target_include_directories(theora-dec PUBLIC include ${OGG_INCLUDE_DIRS})

add_library(theora-enc OBJECT ${THEORA_ENC_SOURCES})
target_include_directories(theora-enc PUBLIC include ${OGG_INCLUDE_DIRS})

add_library(theora SHARED
    $<TARGET_OBJECTS:theora-common>
    $<TARGET_OBJECTS:theora-dec>
    $<TARGET_OBJECTS:theora-enc>
)
target_link_libraries(theora PUBLIC ${OGG_TARGET} m)
target_include_directories(theora PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)

add_library(theoradec SHARED
    $<TARGET_OBJECTS:theora-common>
    $<TARGET_OBJECTS:theora-dec>
)
target_link_libraries(theoradec PUBLIC ${OGG_TARGET} m)
target_include_directories(theoradec PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)

install(TARGETS theora theoradec
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(FILES ${THEORA_HEADERS} DESTINATION include/theora)
EOF

    mkdir -p "${BUILD_DIR}/theora"
    cmake -B "${BUILD_DIR}/theora" -S "${SRC_DIR}/theora" \
        "${COMMON_CMAKE_ARGS[@]}" \
        -DBUILD_SHARED_LIBS=ON
    ninja -C "${BUILD_DIR}/theora" -j"${NUM_JOBS}"
    ninja -C "${BUILD_DIR}/theora" install
else
    echo "libtheora already installed, skipping."
fi

# 4. liblzo2 (v2.10)
echo "--- [4/6] Building liblzo2 ---"
if [[ ! -f "${PREFIX}/lib/liblzo2.so" && ! -f "${PREFIX}/lib/liblzo2.a" ]]; then
    if [[ ! -d "${SRC_DIR}/lzo-2.10" ]]; then
        curl -sL http://www.oberhumer.com/opensource/lzo/download/lzo-2.10.tar.gz | tar -xz -C "${SRC_DIR}"
    fi
    mkdir -p "${BUILD_DIR}/lzo"
    cmake -B "${BUILD_DIR}/lzo" -S "${SRC_DIR}/lzo-2.10" \
        "${COMMON_CMAKE_ARGS[@]}" \
        -DENABLE_SHARED=ON \
        -DENABLE_STATIC=ON
    ninja -C "${BUILD_DIR}/lzo" -j"${NUM_JOBS}"
    ninja -C "${BUILD_DIR}/lzo" install
else
    echo "liblzo2 already installed, skipping."
fi

# 5. SDL2 (release-2.30.8)
echo "--- [5/6] Building SDL2 ---"
if [[ ! -f "${PREFIX}/lib/libSDL2.so" && ! -f "${PREFIX}/lib/libSDL2.a" ]]; then
    if [[ ! -d "${SRC_DIR}/SDL2" ]]; then
        git clone --depth 1 --branch release-2.30.8 https://github.com/libsdl-org/SDL.git "${SRC_DIR}/SDL2"
    fi
    mkdir -p "${BUILD_DIR}/SDL2"
    cmake -B "${BUILD_DIR}/SDL2" -S "${SRC_DIR}/SDL2" \
        "${COMMON_CMAKE_ARGS[@]}" \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TEST=OFF
    ninja -C "${BUILD_DIR}/SDL2" -j"${NUM_JOBS}"
    ninja -C "${BUILD_DIR}/SDL2" install
else
    echo "SDL2 already installed, skipping."
fi

# 6. OpenAL Soft (1.23.1)
echo "--- [6/6] Building OpenAL Soft ---"
if [[ ! -f "${PREFIX}/lib/libopenal.so" && ! -f "${PREFIX}/lib/libopenal.a" ]]; then
    if [[ ! -d "${SRC_DIR}/openal-soft" ]]; then
        git clone --depth 1 --branch 1.23.1 https://github.com/kcat/openal-soft.git "${SRC_DIR}/openal-soft"
    fi
    mkdir -p "${BUILD_DIR}/openal-soft"
    cmake -B "${BUILD_DIR}/openal-soft" -S "${SRC_DIR}/openal-soft" \
        "${COMMON_CMAKE_ARGS[@]}" \
        -DLIBTYPE=SHARED \
        -DALSOFT_UTILS=OFF \
        -DALSOFT_EXAMPLES=OFF \
        -DALSOFT_TESTS=OFF \
        -DALSOFT_BACKEND_OPENSL=ON \
        -DALSOFT_BACKEND_AAUDIO=OFF
    ninja -C "${BUILD_DIR}/openal-soft" -j"${NUM_JOBS}"
    ninja -C "${BUILD_DIR}/openal-soft" install
else
    echo "OpenAL Soft already installed, skipping."
fi

echo "=== All Android ARM64 dependencies successfully built and installed to ${PREFIX} ==="
ls -lh "${PREFIX}/lib"
