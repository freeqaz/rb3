cmake_minimum_required(VERSION 3.16)
project(rb3-wii-native CXX C ASM)

# Requires Clang — the Milo engine is a MetroWerks CodeWarrior (Wii/Gekko)
# decomp; we compile the matched fork under clang LP64 with MWCC compat shims.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "rb3-wii native build expects Clang; got ${CMAKE_CXX_COMPILER_ID}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(REPO_ROOT ${CMAKE_SOURCE_DIR}/..)

# ============================================================================
# milo-native-engine (shared LP64 engine) — consumed via add_subdirectory with
# a soft SHA pin (warn on mismatch). For milestone (a) we DO NOT inject decomp
# context (MILO_ENGINE_DECOMP_INCLUDE_DIRS), so HAVE_CONTEXT stays OFF and the
# engine builds only the placeholder libmilo-engine.a (no gfx/Dawn/audio).
# ============================================================================
set(MILO_ENGINE_PATH "${CMAKE_SOURCE_DIR}/../../milo-native-engine"
    CACHE PATH "Path to milo-native-engine checkout")
set(MILO_ENGINE_PIN "291a70fab054624e50d4970b651ccbfdf56090ce"
    CACHE STRING "Engine commit SHA verified against")
if(EXISTS "${MILO_ENGINE_PATH}/.git")
    execute_process(COMMAND git rev-parse HEAD
        WORKING_DIRECTORY ${MILO_ENGINE_PATH}
        OUTPUT_VARIABLE _milo_engine_head OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _milo_engine_head MATCHES "^${MILO_ENGINE_PIN}")
        message(WARNING
            "milo-native-engine HEAD is ${_milo_engine_head}\n"
            "  but rb3-native pins ${MILO_ENGINE_PIN}\n"
            "  (update MILO_ENGINE_PIN once the new engine state is canonical)")
    endif()
endif()
# Milestone (a): intentionally NOT setting MILO_ENGINE_DECOMP_INCLUDE_DIRS ->
# engine builds placeholder-only.
add_subdirectory(${MILO_ENGINE_PATH} ${CMAKE_BINARY_DIR}/milo-engine)

# ============================================================================
# Engine core: the subset of the RB3-Wii matched fork needed for DTA parsing.
#   obj  (DataNode/DataArray/Object), utl (MemMgr/Str/Symbol/streams),
#   os   (File/Debug/System/Timer), math (Vec/Geo/Rot/...).
# ============================================================================
file(GLOB ENGINE_OBJ  ${REPO_ROOT}/src/system/obj/*.cpp)
file(GLOB ENGINE_UTL  ${REPO_ROOT}/src/system/utl/*.cpp)
file(GLOB ENGINE_OS   ${REPO_ROOT}/src/system/os/*.cpp)
file(GLOB ENGINE_MATH ${REPO_ROOT}/src/system/math/*.cpp)
set(ENGINE_SOURCES ${ENGINE_OBJ} ${ENGINE_UTL} ${ENGINE_OS} ${ENGINE_MATH})

# Drop platform-specific TUs — native shims (rvl_shims.cpp) replace them.
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "_(Xbox|Win|Wii|X360|Gekko)\\.cpp$")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "(Dx9|Wgpu|Gpu|D3D|GX)")
# DataFlex_target.cpp is a separate yytarget:: tester lexer (uses Windows <io.h>,
# not the DTA-parse path); the real lexer is DataFlex.c. Drop the target variant.
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "DataFlex_target\\.cpp$")
# Off-path Wii-platform TUs that pull unshimmed Wii SDK surface (NAND saves,
# RevoEX nhttp, DVD disc I/O, content/audio backends) or are otherwise not on
# the DTA-parse path. Not needed to load a songs.dta; any symbols they'd define
# are off-path (the loose linker flags below tolerate unresolved refs).
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/(BufStreamNAND|HttpWii|FakeSongMgr|CDReader|WaveFile|AsyncFileCNT)\\.cpp$")
# AsyncFile.cpp / HolmesClient.cpp pull os/AsyncFileCNT.h, which hard-includes
# the real sdk/RVL_SDK cnt.h -> ARC.h chain (un-shimmable cheaply). The DTA path
# reads synchronously via FileStream, so the async backends aren't needed.
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/(AsyncFile|HolmesClient)\\.cpp$")

# Flex-generated DTA lexer (C source).
set(DTA_LEXER ${REPO_ROOT}/src/system/obj/DataFlex.c)
set_source_files_properties(${DTA_LEXER} PROPERTIES LANGUAGE C)

# Native shims.
set(NATIVE_SHIMS
    ${CMAKE_SOURCE_DIR}/src/rvl_shims.cpp
    ${CMAKE_SOURCE_DIR}/src/native_link_glue.cpp
    ${CMAKE_SOURCE_DIR}/src/native_file.cpp   # stdio-backed File for the DTA read path
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s  # weak no-op stubs for off-path symbols
)

add_executable(rb3-dta
    ${CMAKE_SOURCE_DIR}/src/main_dta.cpp
    ${ENGINE_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
)

# --- MWCC-compat + decomp flags ---
# -fms-compatibility / -fms-extensions / -fdelayed-template-parsing relax clang
# enough to accept the decomp's permissive template code (implicit `typename`
# on dependent names, `register` storage class, etc.) that MWCC also accepts.
set(DECOMP_FLAGS
    -w
    -Wno-register             # decomp uses the (C++17-removed) `register` keyword
    -Wno-dynamic-exception-spec # operator new(...) throw(std::bad_alloc) (pre-C++17)
    -Wno-c++11-narrowing      # decomp has a few narrowing constant inits
    -Wno-return-type          # decomp has a few `return;` in non-void fns (MWCC-tolerated)
    -ferror-limit=0
    -fno-omit-frame-pointer
    -fms-compatibility
    -fms-compatibility-version=19.29
    -fms-extensions
    -fdelayed-template-parsing
    -fno-strict-aliasing
    "SHELL:-include ${CMAKE_SOURCE_DIR}/src/mwcc_compat.h"
)
target_compile_options(rb3-dta PRIVATE ${DECOMP_FLAGS})
target_compile_definitions(rb3-dta PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)

# native include dir provides revolution/* shims + (via engine) the __wrap_iter
# STL shim. The engine's include/ is exported BEFORE/SYSTEM by milo-engine.
target_include_directories(rb3-dta SYSTEM BEFORE PRIVATE
    ${MILO_ENGINE_PATH}/include)
target_include_directories(rb3-dta PRIVATE
    ${CMAKE_SOURCE_DIR}/src              # native shims + revolution/* shim headers
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/system/oggvorbis
)

find_package(Threads REQUIRED)
target_link_libraries(rb3-dta PRIVATE milo-engine Threads::Threads)
find_package(ZLIB)
if(ZLIB_FOUND)
    target_link_libraries(rb3-dta PRIVATE ZLIB::ZLIB)
endif()

# Off-path symbols (rendering/audio/Bink/MIDI/RSO/Wii managers + their vtables/
# typeinfo) are satisfied by the weak no-op stubs in dta_link_stubs.s — calling
# one harmlessly returns, reading one yields a valid non-null pointer. This is
# preferable to --unresolved-symbols=ignore-all, which leaves null symbols that
# crash when a global ctor (e.g. ThePlatformMgr) calls one at static-init time.
target_link_options(rb3-dta PRIVATE -Wl,--allow-multiple-definition)
