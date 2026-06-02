# rb3-xma-convert — standalone offline XMA->PCM SFX sidecar converter.
#
# Deliberately does NOT link the milo engine (no Dawn/WebGPU/glfw): the milo
# bank parsing it needs is the tiny read-only milo_sample_scan.h (RB3 SFX banks
# are uncompressed single/multi-chunk containers). The only heavy dependency is
# libav (ffmpeg) for the XMA2 decode, reused verbatim from DC3's already-
# validated XmaSampleDecoder.cpp (DecodeXMAToPCM, gated HX_FFMPEG).
#
# Configure standalone:
#   cmake -S native/tools/xma_repack -B build-xma -G Ninja \
#         -DCMAKE_CXX_COMPILER=/usr/bin/clang++
#   cmake --build build-xma
# or it is added via add_subdirectory from native/CMakeLists.txt when
# RB3_BUILD_XMA_TOOL=ON (default OFF — keeps the heavy engine build independent).
cmake_minimum_required(VERSION 3.16)
project(rb3-xma-convert CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec libavutil libavformat)

# DC3's XMA decoder — the SOLVED codec. Reused, not reimplemented. Defaults to
# the canonical sibling checkout; override with -DDC3_DECOMP_PATH for a worktree
# or relocated tree (this RB3 source may itself live in a deep worktree path).
set(DC3_DECOMP_PATH "/home/free/code/milohax/dc3-decomp"
    CACHE PATH "Path to dc3-decomp checkout (for XmaSampleDecoder)")
set(XMA_DECODER_SRC "${DC3_DECOMP_PATH}/native/src/platform/XmaSampleDecoder.cpp")
set(XMA_DECODER_INC "${DC3_DECOMP_PATH}/native/src")
if(NOT EXISTS "${XMA_DECODER_SRC}")
    message(FATAL_ERROR
        "XmaSampleDecoder.cpp not found at ${XMA_DECODER_SRC}\n"
        "  set -DDC3_DECOMP_PATH=/abs/path/to/dc3-decomp")
endif()

add_executable(rb3-xma-convert
    xma_convert_main.cpp
    ${XMA_DECODER_SRC})
target_include_directories(rb3-xma-convert PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XMA_DECODER_INC})           # so platform/XmaSampleDecoder.h resolves
target_compile_definitions(rb3-xma-convert PRIVATE HX_FFMPEG=1)
target_link_libraries(rb3-xma-convert PRIVATE PkgConfig::FFMPEG)
