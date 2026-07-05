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
# MWCC-compat + decomp flags. -fms-compatibility / -fms-extensions /
# -fdelayed-template-parsing relax clang enough to accept the decomp's
# permissive template code (implicit `typename` on dependent names, `register`
# storage class, etc.) that MWCC also accepts. Used both for the engine context
# injection (below) and for the rb3 matched-fork TUs each target compiles.
# ============================================================================
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
    # GCC 15/16 libstdc++ <atomic>/<thread>/<mutex> + glibc <sys/cdefs.h> need
    # the __GNUC__/__GCC_ATOMIC_* builtins that clang drops in -fms-compatibility
    # mode. The engine's audio/threading TUs include <atomic>; the rb3-dta
    # obj/utl/os/math subset never did, so these only surface once rb3-native
    # pulls the engine's full source set. Mirror dc3-native's atomic-compat set.
    -D__GNUC_STDC_INLINE__
    -D__GCC_ATOMIC_TEST_AND_SET_TRUEVAL=1
    -D__GCC_ATOMIC_BOOL_LOCK_FREE=2
    -D__GCC_ATOMIC_CHAR_LOCK_FREE=2
    -D__GCC_ATOMIC_WCHAR_T_LOCK_FREE=2
    -D__GCC_ATOMIC_CHAR16_T_LOCK_FREE=2
    -D__GCC_ATOMIC_CHAR32_T_LOCK_FREE=2
    -D__GCC_ATOMIC_SHORT_LOCK_FREE=2
    -D__GCC_ATOMIC_INT_LOCK_FREE=2
    -D__GCC_ATOMIC_LONG_LOCK_FREE=2
    -D__GCC_ATOMIC_LLONG_LOCK_FREE=2
    -D__GCC_ATOMIC_POINTER_LOCK_FREE=2
    "SHELL:-include ${CMAKE_SOURCE_DIR}/src/mwcc_compat.h"
)

# ============================================================================
# milo-native-engine (shared LP64 engine) — consumed via add_subdirectory with
# a soft SHA pin (warn on mismatch). Milestone (b): we now INJECT RB3's
# matched-fork context (MILO_ENGINE_DECOMP_INCLUDE_DIRS / _COMPAT_FLAGS) so the
# engine compiles its full (GFX-off) source set against RB3's Milo headers.
#
# CRITICAL: RB3 builds the engine with the `rb3` GPU backend flavor
# (MILO_ENGINE_GPU_BACKEND=rb3, set below): the rndobj-FREE WebGPU gfx CORE
# (GpuDevice/PipelineManager/Screenshot/headless frame) PLUS the engine's
# BandRnd : Rnd backend (src/platform/Rnd_Wgpu_RB3.cpp). The `dc3` flavor —
# WgpuRnd : NgRnd + the 6 rndobj-coupled gfx TUs (DrawRect2D/DofPass/ShadowPass/…
# reading DC3's RndMat/RndCam/RndMesh::Vert shapes) — stays OFF for RB3: RB3-Wii's
# 2010-era rndobj/ is structurally older than DC3's and cannot compile those TUs
# (Wave 2.3 finding). BandRnd reads RB3's older rndobj instead and renders real
# RB3 geometry. RB3-variants of the 6 coupled TUs are deferred (roadmap Phase 2).
# ============================================================================
set(MILO_ENGINE_PATH "${CMAKE_SOURCE_DIR}/../../milo-native-engine"
    CACHE PATH "Path to milo-native-engine checkout")
set(MILO_ENGINE_PIN "9561a1957b0c89d23e74ae8f3022da664289b2c5"
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

# RB3 matched-fork context for the engine. Mirrors the dc3-native injection
# (dc3-decomp/native/CMakeLists.txt) but with RB3's include roots + the MWCC
# DECOMP_FLAGS above. RB3 has no shared decomp PCH, so MILO_ENGINE_DECOMP_PCH
# is intentionally left unset.
set(MILO_ENGINE_DECOMP_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src              # native shims + revolution/* shim headers
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/band3               # game/ tour/ meta_band/ net_band/ bandtrack/ headers
    ${REPO_ROOT}/src/network             # net/ Platform/ Protocol/ online headers
    ${REPO_ROOT}/src/system/oggvorbis
    ${REPO_ROOT}/src/system/synth/tomcrypt)  # tomcrypt's <angled> sibling includes
set(MILO_ENGINE_DECOMP_COMPAT_FLAGS ${DECOMP_FLAGS})

# Engine platform/audio/char TUs that are DC3-shaped against subsystem headers
# (or SDK files) that are OLDER or absent in the RB3-Wii fork. ALL are OFF the
# milo scene-tree-dump path — RB3 supplies its own file/memory/thread backends
# via the native shims (native_file.cpp, rvl_shims.cpp), exactly as rb3-dta
# does. None is a real engine bug; each is a genuine RB3-header-shape / missing-
# header mismatch (the Wave-2.3 GPU partition is handled separately by
# MILO_ENGINE_BUILD_GFX=OFF). Excluded from the engine lib for RB3 only (the
# default DC3 build is unaffected — DC3 never sets this var):
#
#   --- audio / synth (DC3 synth-header shape) ---
#   SampleInst_Native.cpp   RB3 SampleData lacks HasData/NumChannels/DataPtr/
#                           GetNumSamples; SampleInst lacks mSample.
#   StreamReceiver_Native.cpp  RB3 StreamReceiver lacks IsOutputDrained/SetSlip*
#                           — header fails the `override` check.
#   Synth_Stub.cpp          RB3 StandardStream ctor differs; the synth path
#                           pulls synth/tomcrypt (broken <angled> includes +
#                           `environ` clash with <unistd.h>).
#   FxSendNative.cpp        DC3 synth FxSend shape.
# AudioDevice.cpp is NOT excluded — it's the miniaudio output device, needed for
# V1 audio playback. rb3_synth_native.cpp's NativeSynth::Init calls
# AudioDevice::GetInstance().Init(44100), and rb3_stream_receiver_native.cpp
# registers each StreamReceiver as an AudioSource.
#
#   --- input (DC3 input shape) ---
#   Joypad_Native.cpp       RB3 TheUI is a value (not pointer), JoypadData lacks
#                           mNumAnalogSticks, no JoypadTerminateCommon /
#                           kAction_ShellOption.
#
#   --- file / memory / thread (RB3 supplies these via native shims) ---
#   File_Native.cpp         RB3 os/File.h File interface + FileStat differ;
#                           uses FileMakePathBuf (DC3-only). RB3 reads via
#                           native_file.cpp (NativeStdioFile) instead.
#   AsyncFile_Native.cpp    RB3 AsyncFile is abstract w/ a different vtable.
#   CDReader_Native.cpp     RB3 Archive lacks NumArkFiles (DC3 shape).
#   Memory_Native.cpp       includes DC3's src/Memory.h, which RB3 lacks
#                           (RB3 uses utl/MemMgr). rvl_shims.cpp backs the
#                           Wii allocator surface instead.
#   ThreadCall_Native.cpp   RB3 lacks ThreadCallFunc/ThreadCallCallbackFunc
#                           typedefs (DC3 threadcall shape).
#   DataParser_Native.cpp   RB3 gDataLine is `int`, not DataType (DC3 shape);
#                           RB3 parses DTA via DataFlex.c (compiled directly).
#   Keygen_Stub.cpp         RB3 KeyChain::getKey/getNumKeys signatures differ.
#   MapFile_Stub.cpp        RB3 lacks GenericMapFile / FixedString (DC3 shape).
#   NetworkSocket_Stub.cpp  RB3 NetworkSocket vtable (Listen/GetRemoteIP/
#                           InqBoundPort return types) differs (DC3 shape).
#
#   --- bone / char (pull the GFX (GpuDevice.h -> webgpu) chain via rndobj/Rnd.h
#       and the DC3-shaped char/ headers; not needed for a names+types dump) ---
#   BoneSetup.cpp           includes platform/BoneSetup.h + rndobj/Rnd.h, which
#                           drags in src/gfx/GpuDevice.h (webgpu/webgpu_cpp.h).
#   CharTwistSolver.cpp     pulls the DC3-shaped char/ + GFX (GpuDevice.h) chain.
set(MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE
    SampleInst_Native.cpp
    StreamReceiver_Native.cpp
    Synth_Stub.cpp
    FxSendNative.cpp
    # AudioDevice.cpp intentionally NOT excluded — needed for V1 audio (X6).
    Joypad_Native.cpp
    File_Native.cpp
    AsyncFile_Native.cpp
    CDReader_Native.cpp
    Memory_Native.cpp
    ThreadCall_Native.cpp
    DataParser_Native.cpp
    Keygen_Stub.cpp
    MapFile_Stub.cpp
    NetworkSocket_Stub.cpp
    BoneSetup.cpp
    CharTwistSolver.cpp
    #   --- FFmpeg movie/audio (DC3 movie/ + synth shapes) ---
    # Enabling the gfx core (MILO_ENGINE_BUILD_GFX) turns on the engine's FFmpeg
    # discovery, which (FFmpeg is system-present) adds these two TUs. They are
    # NOT gfx core — FFmpegMovieImpl.h includes movie/MovieImpl.h (DC3-only; RB3
    # has movie/Movie.h, a different shape) and FFmpegAudioReader pulls
    # synth/StandardStream.h (RB3 synth shape, already excluded above). Bink/FMV
    # is off-path for the milo dump + clear-frame smoke; drop both for RB3.
    FFmpegMovieImpl.cpp
    FFmpegAudioReader.cpp)

# Build the real engine lib with the `rb3` GPU backend flavor: the rndobj-FREE
# gfx CORE (GpuDevice/PipelineManager/Screenshot/headless frame) PLUS the engine's
# BandRnd : Rnd backend (src/platform/Rnd_Wgpu_RB3.cpp — graduated from this dir).
# This replaces the old BUILD_GFX=ON + BUILD_GPU_BACKENDS=OFF pair: the `dc3`
# WgpuRnd : NgRnd backend + the 6 rndobj-coupled gfx TUs stay OFF for RB3 (they
# read DC3's 2012-era rndobj shapes). Sticky CACHE so a stale value can't linger.
set(MILO_ENGINE_BUILD_GFX ON CACHE BOOL "RB3: build the rndobj-free WebGPU gfx core" FORCE)
set(MILO_ENGINE_GPU_BACKEND rb3 CACHE STRING "RB3: BandRnd : Rnd GPU backend" FORCE)

# The gfx core links Dawn (WebGPU) + glfw. glfw3 is system-installed
# (find_package(glfw3) works out of the box); Dawn lives in the dc3 deps tree.
# Default Dawn_DIR so a bare `cmake -B build-native` finds it (override with
# -DDawn_DIR=... for a different Dawn install). Mirrors dc3-native's -DDawn_DIR.
if(NOT DEFINED Dawn_DIR)
    set(Dawn_DIR "${REPO_ROOT}/../dc3-decomp-deps/dawn/lib/cmake/Dawn"
        CACHE PATH "Path to Dawn's CMake package config")
endif()

# W1 — Emscripten/web target wiring. When configured with `emcmake`, force the
# engine's web platform layer ON and exclude the lifted *_Web.cpp sources from
# libmilo-engine.a so rb3-web (declared at the bottom of this file) can compile
# them DIRECTLY with the target's compile flags + force-included canvas-selector
# header. Building them twice (once in the library, once in rb3-web) would emit
# duplicate symbols at link time. Mirrors DC3's pattern
# (dc3-decomp/native/CMakeLists.txt:274-283).
if(EMSCRIPTEN)
    set(MILO_BUILD_WEB ON CACHE BOOL "Engine web platform layer (W1)" FORCE)
    # Exclude the lifted *_Web.cpp from libmilo-engine.a; rb3-web compiles them
    # directly via RB3_WEB_LIFTED_ENGINE_SOURCES (below). Mirrors DC3.
    # File_Web.cpp is excluded in BOTH source lists — it's DC3-shaped against
    # os/File.h (uses FileMakePathBuf + stat_buffer fields that don't exist on
    # RB3's older header). Same header-shape mismatch File_Native.cpp had for
    # rb3-native; substituted by RB3's stdio-backed NativeStdioFile
    # (native/src/native_file.cpp, gated on HX_NATIVE; stdio operates on
    # MEMFS under emcc).
    list(APPEND MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE
        File_Web.cpp
        CDReader_Web.cpp
        WebAssets.cpp
        GpuDevice_Web.cpp
        AudioDevice_Web.cpp
        ImGuiBackend_Web.cpp)
endif()
add_subdirectory(${MILO_ENGINE_PATH} ${CMAKE_BINARY_DIR}/milo-engine)

# ============================================================================
# RB3-Wii matched-fork source globs.
#   rb3-dta (milestone a) needs only: obj/utl/os/math.
#   rb3-native (milestone b) uses the SAME proven-clean obj/utl/os/math subset.
#
#   NOTE: rndobj/ and synth/ are intentionally NOT compiled into rb3-native yet.
#   A keep-going build of the full globs showed 22/64 rndobj and 7/47 synth TUs
#   do not yet compile under clang LP64 — including the core base classes the
#   loader needs (rndobj/Trans.cpp RndTransformable, rndobj/Tex.cpp, Mesh.cpp,
#   Rnd.cpp; synth/Sfx.cpp, Sequence.cpp). Causes are matched-fork native
#   bring-up gaps NOT in Wave 3's integration scope: POSIX identifier clashes
#   (`wait`/`select` shadowed by <sys/wait.h>/<sys/select.h>), MWCC MSL headers
#   (rndobj/MeshDeform.cpp -> 'MSL_Common/extras.h'), switch/case jump-over-init
#   (rndobj/Trans.cpp), `expression is not assignable` register/cast idioms
#   (rndobj/Tex.cpp), and synth/tomcrypt <angled> includes + an ObjVector.h
#   template instantiation issue. Bringing those up is matched-fork porting
#   work (Wave 1/2 territory). Until then rb3-native can load milos whose object
#   classes live in the obj/ subset; the seam + harness are complete and adding
#   rndobj/synth here is a one-line glob change once those forks are native-clean.
# ============================================================================
file(GLOB ENGINE_OBJ    ${REPO_ROOT}/src/system/obj/*.cpp)
file(GLOB ENGINE_UTL    ${REPO_ROOT}/src/system/utl/*.cpp)
file(GLOB ENGINE_OS     ${REPO_ROOT}/src/system/os/*.cpp)
file(GLOB ENGINE_MATH   ${REPO_ROOT}/src/system/math/*.cpp)
file(GLOB ENGINE_RNDOBJ ${REPO_ROOT}/src/system/rndobj/*.cpp)
file(GLOB ENGINE_SYNTH  ${REPO_ROOT}/src/system/synth/*.cpp)
# --- Boot-path globs (milestone "boot-to-song" T1): the App.cpp boot spine +
# the matched-fork subsystems its *::Init() cluster + the UIManager/menu DTA
# flow touch. ui/world/track are fully clang-clean; beatmatch/meta/movie/char/
# bandobj + band3/{meta_band,game,net_band,tour,bandtrack} are MOSTLY clean — the
# residual ~43 TUs that don't yet compile under clang LP64 are EXCLUDED below
# (NATIVE_FORK_EXCLUDE) and their symbols satisfied by weak no-op stubs in
# band3_link_stubs.s. Those 43 are matched-fork clang-bring-up gaps (STLport
# vector<T,N> idioms, switch jump-over-init, DC3-vs-RB3 header shapes) — Wave 1/2
# porting work, NOT this milestone's scope. ---
file(GLOB ENGINE_UI       ${REPO_ROOT}/src/system/ui/*.cpp)
file(GLOB ENGINE_WORLD    ${REPO_ROOT}/src/system/world/*.cpp)
file(GLOB ENGINE_TRACK    ${REPO_ROOT}/src/system/track/*.cpp)
file(GLOB ENGINE_BEATM    ${REPO_ROOT}/src/system/beatmatch/*.cpp)
file(GLOB ENGINE_MIDI     ${REPO_ROOT}/src/system/midi/*.cpp)
file(GLOB ENGINE_META     ${REPO_ROOT}/src/system/meta/*.cpp)
file(GLOB ENGINE_MOVIE    ${REPO_ROOT}/src/system/movie/*.cpp)
file(GLOB ENGINE_BANDOBJ  ${REPO_ROOT}/src/system/bandobj/*.cpp)
file(GLOB ENGINE_CHAR     ${REPO_ROOT}/src/system/char/*.cpp)
file(GLOB GAME_METABAND   ${REPO_ROOT}/src/band3/meta_band/*.cpp)
file(GLOB GAME_GAME       ${REPO_ROOT}/src/band3/game/*.cpp)
file(GLOB GAME_NETBAND    ${REPO_ROOT}/src/band3/net_band/*.cpp)
file(GLOB GAME_TOUR       ${REPO_ROOT}/src/band3/tour/*.cpp)
file(GLOB GAME_BANDTRACK  ${REPO_ROOT}/src/band3/bandtrack/*.cpp)
file(GLOB GAME_APP        ${REPO_ROOT}/src/App.cpp)

# --- DTA harness source set (milestone a): obj/utl/os/math only ---
set(DTA_FORK_SOURCES ${ENGINE_OBJ} ${ENGINE_UTL} ${ENGINE_OS} ${ENGINE_MATH})

# --- native harness source set (milestone b): obj/utl/os/math + rndobj/synth.
# rndobj/synth are now clang-LP64-clean (HX_NATIVE matched-fork bring-up), so the
# object factories they define register at static-init -> full object-graph load
# (vs the earlier header-only dump). Platform/GFX-backend TUs are filtered below. ---
set(NATIVE_FORK_SOURCES
    ${ENGINE_OBJ} ${ENGINE_UTL} ${ENGINE_OS} ${ENGINE_MATH}
    ${ENGINE_RNDOBJ} ${ENGINE_SYNTH}
    ${ENGINE_UI} ${ENGINE_WORLD} ${ENGINE_TRACK} ${ENGINE_BEATM}
    ${ENGINE_MIDI}
    ${ENGINE_META} ${ENGINE_MOVIE} ${ENGINE_BANDOBJ} ${ENGINE_CHAR}
    ${GAME_METABAND} ${GAME_GAME} ${GAME_NETBAND} ${GAME_TOUR}
    ${GAME_BANDTRACK} ${GAME_APP}
    # dsp/VibratoDetector: small standalone class (~75 lines, clang-clean) used
    # by Singer.cpp. Pulled in surgically rather than globbing all of dsp/
    # (PitchDetector/SndAnalysis are off-path and still stubbed).
    ${REPO_ROOT}/src/system/dsp/VibratoDetector.cpp)

# Shared exclusions applied to BOTH source sets.
#
# Drop platform-specific TUs — native shims (rvl_shims.cpp) replace them.
set(_FORK_EXCLUDE_REGEX_PLATFORM "_(Xbox|Win|Wii|X360|Gekko)\\.cpp$")
# Wii/GX/GPU/DX render backends — no GPU on native (engine built GFX-off).
set(_FORK_EXCLUDE_REGEX_GFX "(Dx9|Wgpu|Gpu|D3D|GX)")
# DataFlex_target.cpp is a separate yytarget:: tester lexer (Windows <io.h>);
# the real lexer is DataFlex.c. Drop the target variant.
set(_FORK_EXCLUDE_REGEX_FLEX "DataFlex_target\\.cpp$")
# Off-path Wii-platform TUs that pull unshimmed Wii SDK surface (NAND saves,
# RevoEX nhttp, DVD disc I/O, content/audio backends). The loose linker flags
# tolerate any unresolved refs they'd have defined.
set(_FORK_EXCLUDE_REGEX_WIISDK "/(BufStreamNAND|HttpWii|CDReader|WaveFile|AsyncFileCNT)\\.cpp$")
# AsyncFile.cpp / HolmesClient.cpp pull os/AsyncFileCNT.h -> sdk/RVL_SDK cnt.h ->
# ARC.h (un-shimmable cheaply). Both harnesses read synchronously via the native
# FileStream, so the async backends aren't needed.
set(_FORK_EXCLUDE_REGEX_ASYNC "/(AsyncFile|HolmesClient)\\.cpp$")
# The background permuter writes hidden scratch copies (.permuter_work_*.cpp);
# file(GLOB) matches dotfiles, so drop any dot-prefixed basename.
set(_FORK_EXCLUDE_REGEX_DOTFILE "/\\.[^/]*\\.cpp$")

foreach(_set DTA_FORK_SOURCES NATIVE_FORK_SOURCES)
    list(FILTER ${_set} EXCLUDE REGEX "${_FORK_EXCLUDE_REGEX_PLATFORM}")
    list(FILTER ${_set} EXCLUDE REGEX "${_FORK_EXCLUDE_REGEX_GFX}")
    list(FILTER ${_set} EXCLUDE REGEX "${_FORK_EXCLUDE_REGEX_FLEX}")
    list(FILTER ${_set} EXCLUDE REGEX "${_FORK_EXCLUDE_REGEX_WIISDK}")
    list(FILTER ${_set} EXCLUDE REGEX "${_FORK_EXCLUDE_REGEX_ASYNC}")
    list(FILTER ${_set} EXCLUDE REGEX "${_FORK_EXCLUDE_REGEX_DOTFILE}")
endforeach()

# NATIVE-only: residual boot-path TUs that don't yet compile under clang LP64
# (matched-fork clang-bring-up gaps — see boot-path-glob comment above). Each is
# stubbed with a weak no-op in band3_link_stubs.s so the boot links. Bringing a
# TU here clang-clean (+ removing it from this list) is the per-class work that
# advances the boot (e.g. MetaPanel/Band for the menu DTA flow). Paths are
# anchored (/Name.cpp$) so e.g. SongSort.cpp doesn't also drop SongSortByPlays.cpp.
set(_NATIVE_FORK_EXCLUDE
    # V20 (salvage V33): BandPatchMesh un-excluded — its destructor is called
    # via ObjVector<BandPatchMesh>::resize from OutfitConfig::Load (band player
    # outfit merge). With BandPatchMesh weak-stubbed, its ObjPtr members hold
    # garbage and the dtor SEGVs in ObjPtr::~ObjPtr → Object::Release → rbegin().
    # Requires the V20 #ifndef HX_NATIVE wrappers around the stlpmtx_std
    # specializations in BandPatchMesh.cpp.
    ClipDistMap DataResults
    SaveLoadManager
    Splash
    StorePackedMetadata TourPerformerLocal
    WaitingUserGate)
foreach(_tu ${_NATIVE_FORK_EXCLUDE})
    list(FILTER NATIVE_FORK_SOURCES EXCLUDE REGEX "/${_tu}\\.cpp$")
endforeach()

# Flex-generated DTA lexer (C source).
set(DTA_LEXER ${REPO_ROOT}/src/system/obj/DataFlex.c)
set_source_files_properties(${DTA_LEXER} PROPERTIES LANGUAGE C)

# Native shims (shared by both targets).
set(NATIVE_SHIMS
    ${CMAKE_SOURCE_DIR}/src/rvl_shims.cpp
    ${CMAKE_SOURCE_DIR}/src/native_link_glue.cpp
    ${CMAKE_SOURCE_DIR}/src/native_file.cpp   # stdio-backed File for the read path
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s  # weak no-op stubs for off-path symbols
)

# ============================================================================
# Shared compile/include/link helper applied to each native target.
# ============================================================================
if(NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    find_package(ZLIB)
endif()

function(rb3_configure_target tgt)
    target_compile_options(${tgt} PRIVATE ${DECOMP_FLAGS})
    target_compile_definitions(${tgt} PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
    # native include dir provides revolution/* shims + (via engine) the
    # __wrap_iter STL shim. The engine's include/ is exported BEFORE/SYSTEM by
    # milo-engine.
    target_include_directories(${tgt} SYSTEM BEFORE PRIVATE
        ${MILO_ENGINE_PATH}/include
        ${CMAKE_SOURCE_DIR}/include)         # vendored single-header httplib (cpp-httplib)
    target_include_directories(${tgt} PRIVATE
        ${CMAKE_SOURCE_DIR}/src              # native shims + revolution/* shim headers
        ${REPO_ROOT}/src
        ${REPO_ROOT}/src/system
        ${REPO_ROOT}/src/band3               # game/ tour/ meta_band/ net_band/ bandtrack/ headers
        ${REPO_ROOT}/src/network             # net/ Platform/ Protocol/ online headers
        ${REPO_ROOT}/src/system/oggvorbis
        ${REPO_ROOT}/src/system/synth/tomcrypt)  # tomcrypt's <angled> sibling includes
    target_link_libraries(${tgt} PRIVATE milo-engine)
    if(NOT EMSCRIPTEN)
        # Native-only link surface. Under emcc, threads/ZLIB/vorbis/ogg are
        # supplied by Emscripten ports (-sUSE_ZLIB=1 / -sUSE_OGG=1 / -sUSE_VORBIS=1
        # — wired in via milo_engine_apply_web_target_options).
        target_link_libraries(${tgt} PRIVATE Threads::Threads)
        target_link_libraries(${tgt} PRIVATE vorbis vorbisfile ogg)
        if(ZLIB_FOUND)
            target_link_libraries(${tgt} PRIVATE ZLIB::ZLIB)
        endif()
    endif()
    # The matched-fork has a handful of header-defined global non-inline objects
    # (sNullMicClientID, gKickAutoplay, gAvg/gSyncAvg, PollTheSplasher) that
    # duplicate when included by multiple TUs. wasm-ld accepts the same
    # --allow-multiple-definition flag GNU ld does; this lets the rb3-web link
    # succeed without rewriting the fork. Pairs with the weak no-op symbol
    # stubs that rb3-native gets via dta_link_stubs.s (omitted on web — the
    # missing_stubs.js / -sERROR_ON_UNDEFINED_SYMBOLS=0 pair handles UNRESOLVED
    # references, but only --allow-multiple-definition handles DUPLICATE ones).
    target_link_options(${tgt} PRIVATE -Wl,--allow-multiple-definition)
endfunction()

if(NOT EMSCRIPTEN)

# ============================================================================
# rb3-dta (milestone a) — minimal DTA-parse harness. Links the now-fuller
# (real, GFX-off) engine lib but compiles only the obj/utl/os/math fork subset.
# Must keep parsing 138 songs.
# ============================================================================
add_executable(rb3-dta
    ${CMAKE_SOURCE_DIR}/src/main_dta.cpp
    ${DTA_FORK_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    # W0.2.S4: dta_link_stubs.s (part of NATIVE_SHIMS) went "loud" — its weak
    # stub trampolines call the census hook __hmx_stub_first_hit, defined here.
    # rb3-dta must link this TU too (not just rb3-native) or the symbol is
    # undefined. No HX_NATIVE-specific deps (just <mutex>/<cstdio>/<vector>),
    # so this is a safe, additive one-line link addition.
    ${CMAKE_SOURCE_DIR}/src/rb3_stub_census.cpp
    # Session-telemetry recorder CORE + Tier-1 replay loader. The telemetry
    # wave's HX_NATIVE hooks in shared fork TUs rb3-dta compiles (obj/Task.cpp
    # gRB3TraceFrame/RB3ReplayDtForFrame/RB3ReplayFixedClock/RB3ReplayActive,
    # os/Debug.cpp gRB3TraceActive/RB3RecordLog, os/System.cpp RB3ReplaySeed/
    # RB3TraceSetSeed) reference these definitions, so rb3-dta must link them.
    # Both TUs are std-only (no HTTP/GLFW/render pulls; the gFrameTrace*
    # counters they extern are defined in utl/Loader.cpp, already in
    # DTA_FORK_SOURCES). Tracing/replay stay inert unless RB3_SESSION_TRACE /
    # RB3_REPLAY are set.
    ${CMAKE_SOURCE_DIR}/src/rb3_session_trace.cpp
    ${CMAKE_SOURCE_DIR}/src/rb3_replay.cpp
    # ...except rb3_replay.cpp's ONE extern into rb3_trace_taps.cpp
    # (RB3TraceCurrentSongMs), whose real TU drags band3 Game/UI types. This
    # weak stub returns the tap's exact "engine not booted" value (-1.0f).
    ${CMAKE_SOURCE_DIR}/src/rb3_trace_taps_stub.cpp
)
rb3_configure_target(rb3-dta)

# ============================================================================
# rb3-native (milestone b) — full-engine boot harness. With no argv it inits
# the minimal engine state and exits cleanly (FLOOR); with a .milo path it loads
# the scene and dumps the object tree (STRETCH). Links the real engine lib +
# RB3's obj/utl/os/math/rndobj/synth fork + the render-hook seam.
# ============================================================================
add_executable(rb3-native
    ${CMAKE_SOURCE_DIR}/src/main_native.cpp
    # Live-tunable render/camera/gem settings (one struct, env-seeded at startup
    # + name/value mutable for the future HTTP PUT /api/settings endpoint).
    ${CMAKE_SOURCE_DIR}/src/rb3_native_settings.cpp
    ${CMAKE_SOURCE_DIR}/src/rb3_render_hook.cpp
    # RB3 GPU rendering backend. The BandRnd : Rnd backend itself
    # (formerly rb3_band_rnd.cpp) has GRADUATED into the shared engine as
    # src/platform/Rnd_Wgpu_RB3.cpp, compiled there under MILO_ENGINE_GPU_BACKEND=rb3
    # (set below). What stays here are the per-decomp HARNESS drivers that exercise
    # that backend:
    #   rb3_render_tri.cpp  — triangle via engine gfx core (smoke)
    #   rb3_render_mesh.cpp — boot + load milo + drive BandRnd -> PNG
    ${CMAKE_SOURCE_DIR}/src/rb3_render_tri.cpp
    ${CMAKE_SOURCE_DIR}/src/rb3_render_mesh.cpp
    #   rb3_viewer.cpp      — standalone .milo asset renderer (--viewer / RB3_VIEWER=1):
    #                         RENDER_MESH spine + char/hair factories + CharHair sim + CLI
    ${CMAKE_SOURCE_DIR}/src/rb3_viewer.cpp
    # X6: native NativeSynth (boots miniaudio AudioDevice + binds the
    # RB3-shaped StreamReceiver factory below)
    ${CMAKE_SOURCE_DIR}/src/rb3_synth_native.cpp
    # X6: RB3-shaped StreamReceiver bridging the matched-fork synth onto
    # AudioDevice. Bound to StreamReceiver::sFactory inside NativeSynth::Init.
    ${CMAKE_SOURCE_DIR}/src/rb3_stream_receiver_native.cpp
    # RB3-shaped SampleInst (SynthSample::NewInst) for one-shot menu/UI/MIDI SFX —
    # mirrors the StreamReceiver bridge. Engine SampleInst_Native.cpp is DC3-shaped
    # (platform-excluded); strong def wins over the weak rndobj_synth_link_stubs.s no-op.
    ${CMAKE_SOURCE_DIR}/src/rb3_sampleinst_native.cpp
    # X6: RB3-shaped KeyChain (class-static, int) — provides the 12 .mogg
    # decryption keys. Engine's Keygen_Stub.cpp is a DC3-shaped namespace and
    # is platform-excluded for RB3; the matched-fork keygen_wii.cpp lives
    # outside the globbed source set. Strong defs win over the weak no-ops in
    # rndobj_synth_link_stubs.s; unblocks VorbisReader's keyIndex range check.
    ${CMAKE_SOURCE_DIR}/src/rb3_keychain_native.cpp
    # K3: vorbis_synthesis_poll shim — Harmonix's incremental decoder (Xbox
    # extension) delegates to standard vorbis_synthesis (full decode at once).
    # System libvorbis provides all ogg_*/vorbis_* symbols; this shim adds the
    # one HMX-specific entry-point that libvorbis doesn't export.
    ${CMAKE_SOURCE_DIR}/src/rb3_vorbis_poll_shim.cpp
    # native ctors for Wii/online manager globals whose real ctors live in
    # platform-EXCLUDED Wii TUs (PlatformMgr, ContentMgr — strong defs win over
    # the weak noop stubs in dta_link_stubs.s, so each global's MsgSource base
    # constructs)
    ${CMAKE_SOURCE_DIR}/src/rb3_platform_native.cpp
    # native TheNetSession (`session`): minimal offline concrete NetSession; its
    # real impl lives in the un-globbed network/ Quazal subsystem
    ${CMAKE_SOURCE_DIR}/src/rb3_netsession_native.cpp
    # native WaitingUserGate ctor/dtor/Handle so its vtable is emitted (the .cpp
    # is in _NATIVE_FORK_EXCLUDE; without this shim `new WaitingUserGate()`
    # returns a vtable-less object and `RELEASE(mWaitingUserGate)` SIGSEGVs on
    # the BandUI::Terminate path)
    ${CMAKE_SOURCE_DIR}/src/rb3_waitinguser_gate_native.cpp
    # native headless synthetic-input driver + screen-flow trace (RB3_GAME_INPUT)
    ${CMAKE_SOURCE_DIR}/src/rb3_game_input.cpp
    # OPT-IN native HACK (RB3_GUEST_PROFILE=1): fake pad-0 guest profile,
    # called late (main_hub-reached) from rb3_game_input.cpp so customize/manage-band
    # are reachable without real sign-in. Uses the REAL natively-compiled
    # TheWiiProfileMgr + TheProfileMgr. Roadmap C11; kill switch RB3_NO_GUEST_PROFILE.
    ${CMAKE_SOURCE_DIR}/src/rb3_guestprofile_native.cpp
    # Faithful native OFFLINE TheServer (online server): real vtable, GetPlayerID->0,
    # not-connected. Replaces the zeroed weak stub so profile-driven ticker/score
    # paths short-circuit instead of deref-crashing. See the TU header.
    ${CMAKE_SOURCE_DIR}/src/rb3_server_native.cpp
    # Session telemetry recorder CORE (RB3_SESSION_TRACE=<path|1>, RB3_FRAME_TRACE
    # alias). Unified NDJSON event stream (hdr/fr/in/nav/boot/song/log/mark) with
    # a string interner + bounded ring + monotonic client_seq/sid. Subsumes the
    # old rb3_frame_trace.cpp (deleted); keeps RB3FrameTraceRecord as the back-compat
    # shim so src/App.cpp's frame tap still links, and ports master's load-perf
    # attribution (gFrameTrace* fields) into the RB3_FRAME_TRACE line. See
    # docs/native/SESSION_TELEMETRY_DESIGN.md "Locked v1 contract".
    ${CMAKE_SOURCE_DIR}/src/rb3_session_trace.cpp
    # Session telemetry engine TAPS: the UIScreenChangeMsg nav sink + the
    # null-guarded song-ms helper (RB3TraceCurrentSongMs). Pulls in the engine
    # types (Game/BeatMaster/BandUI/UIScreen) the recorder core deliberately
    # avoids. The frame/input/boot taps are inline edits at App::RunOneFrame /
    # JoypadPoll / BootMark; this TU owns only the nav sink + song-ms chain.
    ${CMAKE_SOURCE_DIR}/src/rb3_trace_taps.cpp
    # Session telemetry TIER-1 REPLAY: loads a recorded trace's `in` events into
    # an ordered (frame, bits) carry-forward table (RB3_REPLAY=<path> on native;
    # ?replay=<sid> -> window.__rb3ReplayData on web). The JoypadPoll hook in
    # rb3_joypad_native.cpp asks RB3ReplayBitsForFrame(gRB3TraceFrame) for the held
    # bitmask each poll and re-drives SendButtonMessages — the SAME chokepoint the
    # `in` recorder tap fires on (the verb layer bypasses it), so the replayed
    # input is re-recorded for a record->replay->compare round-trip. See
    # docs/native/SESSION_TELEMETRY_DESIGN.md §8 "Tier 1".
    ${CMAKE_SOURCE_DIR}/src/rb3_replay.cpp
    # Frame-degradation fix (2026-06-21): periodic glibc heap trim to cap in-song
    # RSS growth from arena fragmentation (RB3NativeHeapMaintenance, called once
    # per frame from App::Run's native loop). Opt-out RB3_HEAP_TRIM_OFF.
    ${CMAKE_SOURCE_DIR}/src/rb3_heap_maint_native.cpp
    # NOTE: rb3_frame_trace.cpp (the old TASK A4 per-frame stutter tracer) was
    # DELETED by the session-telemetry merge — rb3_session_trace.cpp is now the sole
    # owner of RB3FrameTraceRecord and ports its load-perf attribution fields. The
    # gFrameTrace* counters it read are still DEFINED in src/system/utl/Loader.cpp.
    # TASK T2 (incremental-load-perf Wave 5): loading-dwell GPU warm driver +
    # reveal-frame drain pre-kick. GamePanel.cpp's HX_NATIVE arm calls
    # RB3GameWarmPollDwell during the vignette to sweep the gameplay dir roots
    # through BandRnd::WarmGpuForDir (move 97 tex + 113 mesh uploads off the
    # reveal frame) + pre-kick the Enter ForceGetLoader drain. Flag RB3_GAMEWARM_OFF.
    ${CMAKE_SOURCE_DIR}/src/rb3_gamewarm_native.cpp
    # research/13 T1 (progressive in-session texture sharpen): the I/O + gameplay-
    # gating DRIVER around the engine sharpen manager (milo-native-engine
    # RB3TexSharpen.cpp). Game::Poll (HX_NATIVE) calls RB3TexSharpenPoll once
    # gameplay is running; this TU resolves the venue + `.sharpen` sidecar, fetches
    # it low-priority async on web (local file on native), and hands the bytes to
    # the engine manager which recreates each stripped venue texture at full-res.
    # Flag RB3_PROGRESSIVE_SHARPEN (default ON). Native+web glue (HX_NATIVE).
    ${CMAKE_SOURCE_DIR}/src/rb3_texsharpen_native.cpp
    # TASK T3 (incremental-load-perf): preview-hover mogg prefetch + residency
    # primitives. The matched SongPreview/MusicLibrary HX_NATIVE arms call these to
    # warm the preview mogg async during the ~1 s debounce window and to defer the
    # blocking NewStream open until the bytes are MEMFS-resident. Web = async fetch;
    # native = no-op (host file already resident). Flag: RB3_PREVIEW_PREFETCH_OFF.
    ${CMAKE_SOURCE_DIR}/src/rb3_prefetch_native.cpp
    # REAL live input: keyboard (+ USB gamepad) -> JoypadData -> SendButtonMessages.
    # Strong JoypadInit/Poll/Reset/Terminate defs override the weak no-op stubs in
    # dta_link_stubs.s; SystemPoll(false) -> JoypadPoll() (System.cpp:673) drives it
    # every frame. Lets a player play guitar/bass/keys with the keyboard, no plastic
    # instrument — feeds BOTH menu nav and the gameplay GuitarController.
    ${CMAKE_SOURCE_DIR}/src/rb3_joypad_native.cpp
    # Fullscreen-cinematic backend (intro_movie_screen / win_credits). RB3's
    # matched Movie::Impl is the Wii Bink decoder; under HX_NATIVE the public
    # Movie lifecycle routes here. Web plays a <video> overlay; native desktop
    # skips by default (RB3_INTRO_SECS virtual-plays for headless flow tests).
    ${CMAKE_SOURCE_DIR}/src/rb3_movie_native.cpp
    # band3/bandobj/track/world milo object-class factory registration. Without
    # these, a multi-chunk milo containing a bandobj Dir subclass (e.g.
    # OverdriveMeterDir in tracksystem.milo_xbox) can't construct the class, the
    # nested-dir byte extent is mis-skipped, and the stream desyncs -> runaway
    # std::vector<Viewport>::resize -> SIGSEGV. See the TU header for the full
    # root-cause writeup.
    ${CMAKE_SOURCE_DIR}/src/rb3_game_object_factories.cpp
    # 2D bowl-imposter crowd (render-polish Fix B): strong WiiRnd::GetSharedTex
    # returning one persistent square render-target RndTex. Wires the arena/
    # festival/big_club bowl-crowd imposter path (WorldCrowd::DrawShowing 2D path)
    # to the engine's lazy mid-frame RTT (BandRnd::BeginDrawTarget). Replaces the
    # weak no-op stub in band3_link_stubs.s (strong def wins; the GetSharedTex stub
    # is removed there). Opt-out RB3_CROWD_IMPOSTER_OFF.
    ${CMAKE_SOURCE_DIR}/src/rb3_crowd_imposter_native.cpp
    # Disc base-class -> App* subclass label/inline-help factory remap so the
    # retail-disc BandLabel/InlineHelp milo objects handle the proto's set_*
    # dynamic-text messages (song/player/intro names, star rating, MOTD).
    # Injects supplemental (AppLabel)/(AppInlineHelp) objects-config entries first
    # so the ClassName()-keyed fatal config lookups still resolve (boot-safe).
    ${CMAKE_SOURCE_DIR}/src/rb3_disc_label_classes.cpp
    # embedded HTTP debug server (RB3_HTTP=1): /api/health, /api/screenshot,
    # /api/dta/eval, /api/input. Background httplib thread + a main-thread command
    # queue drained from App.cpp's HX_NATIVE frame loop (RB3HttpServerPoll /
    # *PollScreenshots). Vendored httplib.h is in native/include (added above).
    ${CMAKE_SOURCE_DIR}/src/rb3_http_server.cpp
    # main-thread command handlers + frame-loop hooks for the HTTP server (split
    # from rb3_http_server.cpp because these include decomp headers and so must
    # use the MWCC compat flags, while the httplib TU must NOT — see the
    # set_source_files_properties note after rb3_configure_target below).
    ${CMAKE_SOURCE_DIR}/src/rb3_http_handlers.cpp
    # milo-trace W9 replay API (RB3_REPLAY_API=1): /api/memory + /api/call +
    # /api/replay/info. The native-side half of milo-trace mode (c) differential
    # replay (run the SHIPPING native code vs a captured guest-BE record). Ordinary
    # clang C++ (no decomp headers, no httplib) — needs the same MS-compat-OFF
    # override as rb3_http_server.cpp (set below) because it uses normal RTTI/dlfcn.
    # NOTE: commit this entry ATOMICALLY with src/rb3_replay_api.cpp + the
    # rb3_http_server.{h,cpp} call sites (dd584cdc un-wired it while untracked).
    ${CMAKE_SOURCE_DIR}/src/rb3_replay_api.cpp
    # milo-trace W9B native-side INPUT capture (RB3_REPLAY_CAPTURE=<out.ndjson>):
    # the CAPTURE half of the inverted-direction port-fidelity campaign. Records
    # real-gameplay-distribution function-entry inputs for the high-risk leaf
    # surfaces (endian/string/hash/math) from live engine state during a headless
    # session; tools/w9_campaign.py replays them on the Wii DOL + the native port
    # and diffs. Includes decomp headers (obj/Data.h, math/Vec.h, ui/UI.h,
    # obj/Task.h) so it uses the target-wide MWCC compat flags (NOT the
    # MS-compat-OFF override rb3_replay_api.cpp needs). One exported entry point
    # (rb3rc_capture_sweep), reached over the existing /api/call endpoint.
    ${CMAKE_SOURCE_DIR}/src/rb3_replay_capture.cpp
    # C2: save/profile/settings persistence (keystone). Host-FS blob store behind
    # IPersistBackend, backing ProfileMgr's self-contained global-options +
    # profile-0 GameplayOptions serializers. Bypasses the EXCLUDED Wii
    # SaveLoadManager/MemcardMgr async state machine (never Poll()'d natively).
    ${CMAKE_SOURCE_DIR}/src/rb3_save_native.cpp
    # K5: tomcrypt AES/CTR for v0x10 .mogg decryption. The matched-fork
    # ByteGrinder::HvDecrypt + VorbisReader::DoFileRead path calls
    # rijndael_setup, rijndael_ecb_decrypt, register_cipher, ctr_start,
    # ctr_decrypt — these C sources implement them. Globs only pick up
    # *.cpp under src/system/synth/, so we list these .c files explicitly
    # (mirrors DC3's native build). Strong defs win over the weak noops in
    # rndobj_synth_link_stubs.s.
    ${REPO_ROOT}/src/system/synth/tomcrypt/aes.c
    ${REPO_ROOT}/src/system/synth/tomcrypt/crypt.c
    ${REPO_ROOT}/src/system/synth/tomcrypt/ctr.c
    # Synchronizable base (network/net): not part of the globbed source set, but
    # OvershellPanel (& other menu objects) MI-inherit it; its zeroed RTTI stub
    # crashed __dynamic_cast<UIPanel*>. Compile the real matched-fork ctor/dtor so
    # the base typeinfo/vtable are valid; its online side-effects (TheSyncStore/
    # TheNetSession/TheUserMgr) are gated #ifndef HX_NATIVE.
    ${REPO_ROOT}/src/network/net/Synchronize.cpp
    ${NATIVE_FORK_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    # weak no-op stubs for the off-path symbols rndobj/synth pull in
    # (Wii GX backend, Bink, libogg, tomcrypt, Striper, engine GPU, game globals)
    ${CMAKE_SOURCE_DIR}/src/rndobj_synth_link_stubs.s
    # weak no-op stubs for the boot-path symbols defined by the band3/bandobj/char
    # TUs that don't yet compile under clang (see _NATIVE_FORK_EXCLUDE)
    ${CMAKE_SOURCE_DIR}/src/band3_link_stubs.s
    # loud-by-default census for the band3_link_stubs.s weak trampolines (W0.2)
    ${CMAKE_SOURCE_DIR}/src/rb3_stub_census.cpp
)
rb3_configure_target(rb3-native)

# The HTTP server splits across two TUs by compiler-flag need:
#   rb3_http_server.cpp   — httplib (vendored cpp-httplib) listener thread +
#                           command queue + endpoint registration. httplib pulls
#                           <regex>/mmap shared_ptr deleters whose inline
#                           std::type_info::operator== goes UNDEFINED at link
#                           under the MWCC compat flags (-fms-compatibility
#                           changes clang's RTTI ABI). So this TU must compile as
#                           ordinary clang C++ — hence it includes NO decomp
#                           headers, only httplib + the std lib + its own header.
#   rb3_http_handlers.cpp — the main-thread command handlers + frame-loop hooks
#                           (DTA eval / screenshot readback / input verb / health
#                           snapshot). These touch decomp headers (obj/Data.h,
#                           Rnd_Wgpu_RB3.h, ui/UI.h, game/Game.h) and so need the
#                           target-wide MWCC compat flags — but never include
#                           httplib, so the RTTI-ABI clash never arises.
# Both share the std-only rb3_http_server.h (identical class layout under both
# flag sets). Re-disable MS-compat for the httplib TU only (later flags win).
set_source_files_properties(${CMAKE_SOURCE_DIR}/src/rb3_http_server.cpp
    PROPERTIES COMPILE_OPTIONS
    "-fno-ms-compatibility;-fno-ms-extensions;-fno-delayed-template-parsing")
# rb3_replay_api.cpp is the same kind of std-only / dlfcn TU — no decomp headers,
# normal RTTI ABI; compile it MS-compat-OFF too (later flags win).
set_source_files_properties(${CMAKE_SOURCE_DIR}/src/rb3_replay_api.cpp
    PROPERTIES COMPILE_OPTIONS
    "-fno-ms-compatibility;-fno-ms-extensions;-fno-delayed-template-parsing")

# ============================================================================
# rb3-tests — native convergence/regression test suite (GoogleTest).
#
# Mirrors dc3-decomp/native/tests' milo-tests target: it compiles the SAME
# object set as rb3-native (so static-init object-factory registration stays
# live — a static lib would drop unreferenced factory TUs) MINUS main_native.cpp
# (gtest_main provides main()), then links GTest. Tests live in native/tests/.
#
# DRY: instead of duplicating rb3-native's ~40-entry source list, read it back
# off the target and drop its main(). Both targets therefore always share the
# exact same engine/glue surface.
# ============================================================================
option(BUILD_TESTS "Build the rb3-native gtest convergence suite" ON)
if(BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()

    get_target_property(_RB3_NATIVE_SRCS rb3-native SOURCES)
    list(REMOVE_ITEM _RB3_NATIVE_SRCS ${CMAKE_SOURCE_DIR}/src/main_native.cpp)

    add_executable(rb3-tests
        ${CMAKE_SOURCE_DIR}/tests/test_helpers.cpp
        ${CMAKE_SOURCE_DIR}/tests/test_binstream.cpp
        ${CMAKE_SOURCE_DIR}/tests/test_subsystems.cpp
        ${CMAKE_SOURCE_DIR}/tests/test_session_trace.cpp
        ${CMAKE_SOURCE_DIR}/tests/test_bandpatchmesh.cpp
        ${CMAKE_SOURCE_DIR}/tests/test_charload5b.cpp
        ${CMAKE_SOURCE_DIR}/tests/test_overpress.cpp
        ${CMAKE_SOURCE_DIR}/tests/test_texsharpen.cpp
        ${CMAKE_SOURCE_DIR}/tests/test_texsharpen_manager.cpp
        ${CMAKE_SOURCE_DIR}/tests/test_wgsl_validation.cpp
        ${CMAKE_SOURCE_DIR}/tests/test_native_compat.cpp
        ${CMAKE_SOURCE_DIR}/tests/test_stub_census.cpp
        ${CMAKE_SOURCE_DIR}/tests/test_draw_log_golden.cpp
        ${_RB3_NATIVE_SRCS}
    )
    rb3_configure_target(rb3-tests)
    target_include_directories(rb3-tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(rb3-tests PRIVATE GTest::gtest GTest::gtest_main)

    include(GoogleTest)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tmp")
    # PRE_TEST discovery: enumerate at ctest time, not build time, so a build
    # never blocks on (or fails from) running the freshly-linked binary. The
    # engine is a process-global singleton, so serialize the in-process tests.
    gtest_discover_tests(rb3-tests
        DISCOVERY_MODE PRE_TEST
        PROPERTIES
            ENVIRONMENT "TMPDIR=${CMAKE_CURRENT_BINARY_DIR}/tmp"
            RESOURCE_LOCK "rb3_engine_singleton"
            TIMEOUT 180)
endif()

else() # EMSCRIPTEN — web target

# ============================================================================
# rb3-web (W1, clear-frame milestone) — Emscripten/WebAssembly boot harness.
#
# Mirrors rb3-native's RB3_BOOT=1 path but driven by a browser main loop
# (emscripten_set_main_loop). State machine:
#   BOOT_INIT       → WebAssetsInit + WebAssetsFetchBundle
#   BOOT_FETCHING   → poll WebAssetsAllDone
#   BOOT_ENGINE_INIT → SystemPreInit("config/band_preinit_keep.dta") +
#                      SystemInit("config/band_keep.dta") + register factories +
#                      gBandRnd.InitGpu(W, H, /*headless=*/false)
#   BOOT_GPU_WAIT   → gBandRnd.Gpu().IsReady() (async WebGPU adapter)
#   BOOT_GPU_READY  → one BeginFrame/EndFrame to commit the initial clear
#   BOOT_RUNNING    → per-frame BeginFrame/EndFrame; window.rb3FrameCount = N
#
# The lifted engine *_Web.cpp TUs are compiled DIRECTLY here (with the
# canvas-selector force-included header) — they're excluded from libmilo-engine.a
# above. The link-stub .s files used by rb3-native are x86 GAS syntax and won't
# assemble under emcc; we instead rely on the engine's missing_stubs.js pre-js
# (--pre-js, supplied by milo_engine_apply_web_target_options) which patches
# Emscripten's abort() to warn instead of crash on missing function calls.
# `-sERROR_ON_UNDEFINED_SYMBOLS=0` (also from the helper) makes the link succeed
# despite unresolved references from the matched-fork TUs.
# ============================================================================

set(RB3_WEB_LIFTED_ENGINE_SOURCES
    ${MILO_ENGINE_PATH}/src/platform/WebAssets.cpp
    # NOTE: File_Web.cpp is DC3-shaped (uses FileMakePathBuf + stat fields that
    # don't exist on RB3's older os/File.h). Same problem File_Native.cpp had
    # for rb3-native — RB3 substitutes its own stdio-backed File via
    # native_file.cpp (NativeStdioFile, gated on HX_NATIVE). Under emcc, stdio
    # operates on MEMFS, so native_file.cpp's POSIX I/O reads the assets the
    # bundle unpacks under /data/.
    ${MILO_ENGINE_PATH}/src/platform/CDReader_Web.cpp
    ${MILO_ENGINE_PATH}/src/platform/GpuDevice_Web.cpp
    ${MILO_ENGINE_PATH}/src/audio/AudioDevice_Web.cpp
    ${MILO_ENGINE_PATH}/src/gfx/ImGuiBackend_Web.cpp
)

# rb3-native-only sources that don't compile under emcc and aren't needed for
# the clear-frame smoke. Filter them OUT of the source list:
#   rb3_render_mesh.cpp     — uses POSIX backtrace()/PNG write paths that aren't
#                             portable under emcc; W1 doesn't render meshes.
#   rb3_render_tri.cpp      — also CPU-side mesh smoke; W1 just clears.
#   rb3_http_server.cpp     — vendored cpp-httplib pulls POSIX sockets + threads.
#   rb3_http_handlers.cpp   — pair of rb3_http_server.cpp.
#   rb3_synth_native.cpp    — pulls miniaudio (native AudioDevice replaces it).
#   rb3_stream_receiver_native.cpp — paired with synth.
#   rb3_vorbis_poll_shim.cpp — vorbis poll shim (system libvorbis path).
#   rb3_render_hook.cpp     — HamRenderHook glue (registers with engine); not
#                             needed for clear-frame.
#   rb3_keychain_native.cpp — MOGG keychain (not needed for clear-frame).
#   tomcrypt aes/crypt/ctr  — MOGG decryption C (not needed for clear-frame).
# rb3_band_rnd.cpp is empty (BandRnd graduated to engine) and not in the source
# list to begin with.
set(RB3_WEB_NATIVE_GLUE
    ${CMAKE_SOURCE_DIR}/src/rb3_native_settings.cpp
    ${CMAKE_SOURCE_DIR}/src/rb3_platform_native.cpp
    ${CMAKE_SOURCE_DIR}/src/rb3_netsession_native.cpp
    ${CMAKE_SOURCE_DIR}/src/rb3_waitinguser_gate_native.cpp
    ${CMAKE_SOURCE_DIR}/src/rb3_game_input.cpp
    # OPT-IN native HACK (RB3_GUEST_PROFILE=1): fake pad-0 guest profile (web HX_NATIVE).
    # Roadmap C11; kill switch RB3_NO_GUEST_PROFILE. See native source list above.
    ${CMAKE_SOURCE_DIR}/src/rb3_guestprofile_native.cpp
    # Faithful native OFFLINE TheServer (see native list above) — web is HX_NATIVE.
    ${CMAKE_SOURCE_DIR}/src/rb3_server_native.cpp
    # Frame-degradation fix (2026-06-21): RB3NativeHeapMaintenance. On web the
    # malloc_trim is compiled out (wasm dlmalloc never returns to the host), so this
    # is a no-op there, but the TU must link so App.cpp's extern call resolves.
    ${CMAKE_SOURCE_DIR}/src/rb3_heap_maint_native.cpp
    # Session telemetry recorder CORE (RB3_SESSION_TRACE=<path|1>, RB3_FRAME_TRACE
    # alias) — subsumes the deleted rb3_frame_trace.cpp and owns RB3TraceInit +
    # the RB3FrameTraceRecord back-compat shim. Web defines HX_NATIVE so the
    # recorder + the gFrameTraceActive-gated attribution counters compile; the
    # web run loop in main_web.cpp calls RB3TraceInit + RB3FrameTraceRecord
    # (App::RunWithoutDebugging's native frame loop is never entered on web).
    # Without this those symbols resolve to no-op JS link-stubs and no trace
    # file is created. std-only + #include <emscripten.h> guarded, so emcc-clean.
    ${CMAKE_SOURCE_DIR}/src/rb3_session_trace.cpp
    # Engine-side taps for the recorder (nav sink + in-song `chk` checkpoint +
    # song-ms helper). Originally kept out of the web glue during the W1 clear-
    # frame era (the full engine wasn't linked then); now that main_web.cpp boots
    # the whole App (through gameplay), the engine types these use (Game / BandUI
    # / UIScreen / Player / BeatMaster) ARE in the web link, so this compiles +
    # links under emcc. main_web.cpp calls RB3TraceEnsureNavSink +
    # RB3TraceCheckpointFrame from its run loop. Restores web `nav`/`chk` egress
    # parity with native (previously `[rb3-stub] RB3TraceEnsureNavSink`).
    ${CMAKE_SOURCE_DIR}/src/rb3_trace_taps.cpp
    # TIER-1 REPLAY loader + carry-forward input lookup (RB3ReplayInit/Active/
    # BitsForFrame/Seed). #ifdef HX_NATIVE, std-only + guarded emscripten/em_asm.h
    # (no decomp/engine headers), so emcc-clean. Was NATIVE-ONLY (omitted from this
    # web list), so the ?replay=<sid> path's whole C++ half resolved to no-op JS
    # link-stubs — rb3_pre.js fetched window.__rb3ReplayData but nothing read/drove
    # it (the "compiles but never ran in a browser" gap). main_web.cpp's run loop
    # arms it (pad-independent, the source is async) + feeds the replayed bits into
    # window._rb3Keys so the web input path drives it.
    ${CMAKE_SOURCE_DIR}/src/rb3_replay.cpp
    # TASK T2 (incremental-load-perf Wave 5): loading-dwell GPU warm driver +
    # reveal-frame drain pre-kick. Same TU as native — GamePanel.cpp's HX_NATIVE
    # arm (web defines HX_NATIVE) calls RB3GameWarmPollDwell during the vignette to
    # move the venue first-draw GPU work (97 tex + 113 mesh uploads + CPU unpack)
    # off the reveal frame into the idle dwell, killing the ~0.7-1.0 s web
    # game-content freeze. Flag RB3_GAMEWARM_OFF. See the TU header.
    ${CMAKE_SOURCE_DIR}/src/rb3_gamewarm_native.cpp
    # research/13 T1 (progressive in-session texture sharpen): the I/O + gameplay-
    # gating DRIVER around the engine sharpen manager. Game::Poll (HX_NATIVE, web
    # defines it) calls RB3TexSharpenPoll once gameplay is running; on web this TU's
    # __EMSCRIPTEN__ arm kicks the LOW-priority WebAssetsEnsureResidentAsync fetch of
    # the venue `.sharpen` sidecar and polls residency. WITHOUT this entry the web
    # build resolves RB3TexSharpenPoll to a no-op JS link-stub (ERROR_ON_UNDEFINED_
    # SYMBOLS=0) so the sharpen never runs in the browser — the native-list parity
    # miss. Depends on rb3_gamewarm_native.cpp (RB3ComputeVenueMiloPathForSharpen).
    ${CMAKE_SOURCE_DIR}/src/rb3_texsharpen_native.cpp
    # REAL live input on web: keyboard -> JoypadData -> SendButtonMessages (same
    # TU as native; its #ifdef __EMSCRIPTEN__ branch reads window._rb3Keys +
    # navigator.getGamepads()). Strong JoypadPoll etc. drive frets/strum through
    # the gameplay GuitarController, one path. See the TU header.
    ${CMAKE_SOURCE_DIR}/src/rb3_joypad_native.cpp
    # Intro cinematic on web: drives a fullscreen <video> overlay (the .bik path
    # is rewritten to the pre-transcoded .webm sidecar, streamed from
    # /api/file/videos/...). Routed from the matched Movie under HX_NATIVE. Native
    # desktop skips. See the TU header.
    ${CMAKE_SOURCE_DIR}/src/rb3_movie_native.cpp
    # TASK T3 (incremental-load-perf): preview-hover mogg prefetch + residency.
    # Web arm = async WebAssetsFetch into MEMFS (kills the cold-hover sync-XHR
    # freeze); native arm = no-op (resident=true). Called from the matched
    # SongPreview/MusicLibrary HX_NATIVE arms. Flag: RB3_PREVIEW_PREFETCH_OFF.
    ${CMAKE_SOURCE_DIR}/src/rb3_prefetch_native.cpp
    # W3a: web-only weak-null definitions for the pointer globals that the native
    # build gets from the x86 GAS .s link stubs (not assembled under emcc) — e.g.
    # TheTour / TheSessionMgr / TheSongDB. Without these they're undefined data
    # symbols → garbage addresses → App-ctor vcall traps. See the TU header.
    ${CMAKE_SOURCE_DIR}/src/rb3_web_globals.cpp
    # W2b: register band3/bandobj/track/world milo object-class factories so the
    # multi-chunk milos (main_hub, tracksystem) that embed bandobj Dir subclasses
    # load without desyncing. See the TU header for the root-cause writeup.
    ${CMAKE_SOURCE_DIR}/src/rb3_game_object_factories.cpp
    # 2D bowl-imposter crowd (render-polish Fix B): strong WiiRnd::GetSharedTex.
    # Same as the native list entry — see that TU's header.
    ${CMAKE_SOURCE_DIR}/src/rb3_crowd_imposter_native.cpp
    # Disc base-class -> App* subclass label/inline-help factory remap (set_*
    # dynamic text). Same as the native list entry — see that TU's header.
    ${CMAKE_SOURCE_DIR}/src/rb3_disc_label_classes.cpp
    ${REPO_ROOT}/src/network/net/Synchronize.cpp
    # W2a: milo load + walk + draw. The native PNG path (RunRenderMesh /
    # RenderToPng / _exit) is #ifndef __EMSCRIPTEN__-guarded; the web build only
    # compiles LoadMiloAndWalk + RenderFrame, which main_web.cpp drives per-frame.
    ${CMAKE_SOURCE_DIR}/src/rb3_render_mesh.cpp
    # W3c Part A: audio bring-up on web. These are gated #ifdef HX_NATIVE, which
    # the web build defines, so they compile under emcc. The engine's
    # AudioDevice_Web.cpp (already in RB3_WEB_LIFTED_ENGINE_SOURCES) is the output
    # device — no miniaudio on web; the AudioSource/AddSource/RemoveSource
    # interface in rb3_stream_receiver_native.cpp is shared between native and web.
    #   rb3_synth_native.cpp — NativeSynth: AudioDevice::Init(44100) + binds
    #     StreamReceiver::sFactory to RB3CreateNativeStreamReceiver.
    #   rb3_stream_receiver_native.cpp — RB3-shaped StreamReceiver/AudioSource
    #     bridge (producer = synth Poll on main thread; consumer = RenderAudio via
    #     AudioDevice::PumpAudio, also on the main thread under emcc).
    #   rb3_keychain_native.cpp — .mogg decryption keys (class-static int KeyChain).
    #   rb3_vorbis_poll_shim.cpp — HMX vorbis_synthesis_poll -> vorbis_synthesis.
    ${CMAKE_SOURCE_DIR}/src/rb3_synth_native.cpp
    ${CMAKE_SOURCE_DIR}/src/rb3_stream_receiver_native.cpp
    #   rb3_sampleinst_native.cpp — RB3-shaped SampleInst (SynthSample::NewInst) for
    #     menu/UI/MIDI one-shot SFX; same AudioSource bridge as the StreamReceiver.
    ${CMAKE_SOURCE_DIR}/src/rb3_sampleinst_native.cpp
    ${CMAKE_SOURCE_DIR}/src/rb3_keychain_native.cpp
    ${CMAKE_SOURCE_DIR}/src/rb3_vorbis_poll_shim.cpp
    # W3c Part A: tomcrypt AES/CTR for v0x10 .mogg decryption (LANGUAGE C; see the
    # set_source_files_properties block after milo_engine_apply_web_target_options
    # — they need -fno-ms-* like rb3-native + DC3-web).
    ${REPO_ROOT}/src/system/synth/tomcrypt/aes.c
    ${REPO_ROOT}/src/system/synth/tomcrypt/crypt.c
    ${REPO_ROOT}/src/system/synth/tomcrypt/ctr.c
    # B3 (web persistence): the C2 ProfileMgr save/load glue (rb3_save_native.cpp,
    # platform-agnostic — its HostFsBackend just goes unused on web) PLUS the
    # web-only IndexedDB-backed IPersistBackend (rb3_save_web.cpp, #ifdef HX_WEB).
    # main_web.cpp installs the IDB backend before the App ctor; same two keys
    # (globaloptions.bin, gameplayopts_0.bin), same exact-size Read gate.
    ${CMAKE_SOURCE_DIR}/src/rb3_save_native.cpp
    ${CMAKE_SOURCE_DIR}/src/rb3_save_web.cpp
    # guarded #ifndef HX_NATIVE for the native ALSA crash. Web also defines
    # caller, so re-defining it is safe. #ifdef HX_WEB.
)

# Trim NATIVE_SHIMS for web — keep native_file.cpp (its stdio-backed File
# operates on Emscripten MEMFS under emcc, exactly what the asset bundle
# unpacks into) since the engine's File_Web.cpp is DC3-shaped and excluded
# above. dta_link_stubs.s + the other .s files are x86 GAS and won't assemble
# under emcc — omitted; the engine's missing_stubs.js + -sERROR_ON_UNDEFINED_SYMBOLS=0
# handle the runtime side.
set(RB3_WEB_SHIMS
    ${CMAKE_SOURCE_DIR}/src/rvl_shims.cpp
    ${CMAKE_SOURCE_DIR}/src/native_link_glue.cpp
    ${CMAKE_SOURCE_DIR}/src/native_file.cpp
    # Weak zero-blob DATA twins of the .s stub files' .zero entries. The JS
    # safety net only stubs undefined FUNCTIONS; undefined DATA symbols
    # resolve to address 0 in wasm-ld (null writes/reads — see the file
    # header for the App::App gInitComplete ASan null-deref this fixes).
    ${CMAKE_SOURCE_DIR}/src/web_data_stubs.cpp
)

# Web-only fork-source filter: a handful of matched-fork TUs hit
# emcc-libc++-specific issues that don't reproduce under clang LP64 native (the
# common cause is libc++'s recent privatisation of explicit STL iterator
# constructors — the fork value-initialises iterators with 0 which the older
# libstdc++ accepted but libc++ now rejects). Each one is off the W1 clear-frame
# path; failed callers fall through to the missing_stubs.js / -sERROR_ON_UNDEFINED_SYMBOLS=0
# safety net. Bring each TU emcc-clean in a future wave to grow the web target's
# functional coverage; for W1 we just need clear-frame.
set(RB3_WEB_NATIVE_FORK_EXCLUDE
    # libc++ list::iterator value-init from 0 (now-private explicit ctor)
    Gen
    MultiMeshProxy
    # W3c Part A: VorbisReader + Synth are now RE-INCLUDED. The codec.h alloca
    # clash is fixed by the #ifndef HX_NATIVE guard in src/system/oggvorbis/codec.h
    # (the web build defines HX_NATIVE=1, so __builtin_alloca is used). emcc's
    # -sUSE_OGG=1 -sUSE_VORBIS=1 ports supply ogg_*/vorbis_* symbols; the
    # vorbis_synthesis_poll HMX entry-point comes from rb3_vorbis_poll_shim.cpp
    # (added to RB3_WEB_NATIVE_GLUE).
)
set(RB3_WEB_FORK_SOURCES ${NATIVE_FORK_SOURCES})
foreach(_tu ${RB3_WEB_NATIVE_FORK_EXCLUDE})
    list(FILTER RB3_WEB_FORK_SOURCES EXCLUDE REGEX "/${_tu}\\.cpp$")
endforeach()

add_executable(rb3-web
    ${CMAKE_SOURCE_DIR}/src/main_web.cpp
    ${RB3_WEB_LIFTED_ENGINE_SOURCES}
    ${RB3_WEB_NATIVE_GLUE}
    ${RB3_WEB_FORK_SOURCES}
    ${DTA_LEXER}
    ${RB3_WEB_SHIMS}
)

# Apply the shared MWCC-compat / include / link helper. This handles the bulk of
# the compile flags + include paths shared with rb3-native; the link side
# (-Wl,--allow-multiple-definition) is a no-op under wasm-ld but harmless.
rb3_configure_target(rb3-web)

# Engine-side helper applies emdawnwebgpu, JSPI, wasm-exceptions, missing_stubs.js
# pre-js, audio-worklet.js POST_BUILD copy, base -sEXPORTED_FUNCTIONS=["_main"],
# and -sERROR_ON_UNDEFINED_SYMBOLS=0. Asset dest is native/web/build (where
# server.py serves wasm/js from).
milo_engine_apply_web_target_options(rb3-web ${CMAKE_CURRENT_SOURCE_DIR}/web/build)

# Canvas selector — `#` is illegal in -D values, so this routes through a
# configure_file'd header force-included into every rb3-web TU.
milo_engine_set_web_canvas_selector(rb3-web "#rb3-canvas")

# RB3-specific compile defs. Note MILO_WEB_AUDIO_NS is an UNQUOTED identifier
# (used as `window.<ns>Audio` in AudioDevice_Web.cpp); the helper already
# defines HX_NATIVE=1 / MILO_DEBUG=1 / _DEBUG=1 via the rb3_configure_target
# call, but we add HX_WEB=1 and the audio namespace here.
target_compile_definitions(rb3-web PRIVATE
    HX_WEB=1
    MILO_WEB_AUDIO_NS=rb3
    # libc++ removed deprecated C++ features in C++17 mode; re-enable them so the
    # matched-fork's std::random_shuffle / std::mem_fun / std::const_mem_fun_t /
    # std::binary_function (used pervasively in band3/{game,tour,bandtrack})
    # compile. Mirrors DC3's dc3-web.
    _LIBCPP_ENABLE_CXX17_REMOVED_RANDOM_SHUFFLE
    _LIBCPP_ENABLE_CXX17_REMOVED_BINDERS
    _LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR
    _LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION
)

# speex headers live under src/system/speex/include — rb3_configure_target only
# adds the oggvorbis sibling path. dc3-native links system libspeex; rb3-web has
# no equivalent (RB3's matched-fork bundles its own speex), so we just add the
# include path. The speex .cpp implementations are off the boot path under
# matched fork, but pulling the headers lets Codec.h (used by network/) compile.
target_include_directories(rb3-web PRIVATE
    ${REPO_ROOT}/src/system/speex/include
)

# W3c Part A: tomcrypt C files (.mogg decryption). LANGUAGE C, and they need
# -fno-ms-* — rb3_configure_target applies -fms-compatibility target-wide, which
# makes <signal.h>'s __sigbits emit as a non-inline global (duplicate symbol
# across the three TUs). Mirrors rb3-native + DC3-web's tomcrypt handling.
set_source_files_properties(
    ${REPO_ROOT}/src/system/synth/tomcrypt/aes.c
    ${REPO_ROOT}/src/system/synth/tomcrypt/crypt.c
    ${REPO_ROOT}/src/system/synth/tomcrypt/ctr.c
    PROPERTIES LANGUAGE C
               COMPILE_OPTIONS "-fno-ms-extensions;-fno-ms-compatibility"
)

# Extend the engine helper's base export list with RB3's tick symbol. Later
# -sEXPORTED_FUNCTIONS / -sJSPI_EXPORTS overrides the helper's earlier value.
target_link_options(rb3-web PRIVATE
    # W2b web-asset-load wall: RB3's heavy multi-chunk milos pull large subdir
    # deps (track_shared ≈ 10MB / 60 chunks + tracksystem_meshes 1.6MB +
    # ingame_bank 1.1MB) and build a 1100+-object graph in one synchronous load.
    # Decompressing/holding that whole tree (plus the transient sync-XHR copies
    # in the JS heap) blows past the engine helper's 512MB MAXIMUM_MEMORY default
    # → "Cannot enlarge memory arrays" abort → page crash. Override to 2GB (the
    # last -sMAXIMUM_MEMORY on the wasm-ld line wins, exactly like the
    # -sEXPORTED_FUNCTIONS override just below). This is an RB3-only override —
    # the shared engine default (used by DC3, whose scenes are smaller) is
    # untouched. wasm32's address space ceiling is 4GB; 2GB leaves headroom for
    # the heavier song-load phase (W3) without risking the browser's per-tab
    # allocation limits.
    -sMAXIMUM_MEMORY=2GB
    -sEXPORTED_FUNCTIONS=[\"_main\",\"_rb3MainLoopTick\"]
    # The matched-fork's off-path TUs reference Wii-only globals
    # (TheWiiRnd, TheNetSession, TheMC, TheSynth, RndMesh::sLastCollide,
    # typeinfo for WiiContent / RndGenerator / RndMultiMeshProxy) whose strong
    # defs live in the un-globbed Wii SDK / un-compiled fork TUs. Tell wasm-ld
    # to leave unresolved references as imports rather than erroring — the
    # engine's missing_stubs.js + RB3's rb3_pre.js intercept the runtime import
    # lookups and warn (then return 0) instead of trapping. This mirrors
    # rb3-native's weak-no-op-symbol-table strategy (dta_link_stubs.s).
    # `-sERROR_ON_UNDEFINED_SYMBOLS=0` from the engine helper alone isn't
    # enough — wasm-ld still hard-errors on type references; `--allow-undefined`
    # is the explicit gate.
    "SHELL:-Wl,--allow-undefined"
    # Second --pre-js for RB3-specific abort patching. The engine's
    # missing_stubs.js installs its abort patch from onRuntimeInitialized — too
    # late for the RB3 matched-fork's static initializers that hit
    # missing-import calls during wasm init. rb3_pre.js patches Module.abort
    # immediately (before wasm init) so those early misses become console
    # warnings instead of fatal "Aborted(missing function: ...)" / wasm
    # unreachable traps.
    "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/web/rb3_pre.js"
)
if(MILO_WEB_ASYNC)
    target_link_options(rb3-web PRIVATE
        -sJSPI_EXPORTS=[\"_main\",\"_rb3MainLoopTick\",\"rb3MainLoopTick\"]
    )
endif()

# rb3-web does not link cpp-httplib / ZLIB / Threads explicitly — the engine
# helper supplies USE_ZLIB / USE_OGG / USE_VORBIS via Emscripten ports.

# ============================================================================
# Size-optimised release build. Default OFF (dev keeps -O0 / -g2 for
# debug-friendly stack traces); release mode (`scripts/web/build.sh --release`)
# now ships -O2 -fno-inline -g0 — raw wasm 16.7M -> 6.2M, brotli 2.39M -> 1.50M.
# Two -O>0 faults were root-caused + fixed to get here (boot inline-discard via
# -fno-inline, and a render-time GPU crash isolated to OutfitConfig.cpp pinned to
# -O0 — both documented below). Opt-in knobs:
#   RB3_WEB_OPT_LEVEL=Oz/Os/O3/O2/O1/O0 — -O level; defaults to O2 (release).
#                                         `--opt L` on build.sh overrides it,
#                                         always adding -fno-inline at -O>0.
#   RB3_WEB_CLOSURE=ON                  — BROKEN, see comment below
#
# Order matters in emcc: later -O / -g override earlier ones, so we append to
# both compile and link. The engine helper's -g2 stays in the command but is
# overridden by our -g0 appended later. Same for the implicit -O0.
#
# `--closure 1` minifies the JS glue. The matched-fork ships a runtime
# abort-stub patcher in native/web/rb3_pre.js that walks env.* by name and
# rewrites function bodies whose source contains "missing function". Closure
# minifies the import names, breaking the source-string match — the patch
# replaces 0 stubs and boot crashes on the first missing-stub call with
# `unreachable`. Until rb3_pre.js learns to walk the unminified import map
# (or closure is configured with --externs for the env imports), keep it off.
# ============================================================================
option(RB3_WEB_RELEASE "Size-optimised release build (strip debug info)" OFF)
option(RB3_WEB_CLOSURE "Run --closure 1 on the JS glue (BROKEN — see comment)" OFF)
# RB3_WEB_OPT_LEVEL controls the compile/link -O level when RB3_WEB_RELEASE
# is ON. Default O2 — both historical -O>0 blockers are now fixed (below).
#
# BOOT crash (the old "W4a BROKEN" note) is ROOT-CAUSED and FIXED by the
# `-fno-inline` below. It was NOT optimizer UB: 109 functions are defined
# `inline`/`FORCE_LOCAL_INLINE` in .cpp files but called cross-TU (the decomp
# matching idiom — an ODR violation C++ tolerates). At -O0 clang emits the
# linkonce_odr bodies so wasm-ld resolves them; at any -O>0 clang inlines +
# DISCARDS those bodies, so the cross-TU calls become undefined symbols, which
# `-sERROR_ON_UNDEFINED_SYMBOLS=0` silently turns into env imports that
# rb3_pre.js stubs to warn-and-return-undefined. The first such stub whose value
# matters — `SystemConfig(Symbol)` (src/system/os/System.cpp) — returns NULL on
# the boot path → deref → the "exception during App construction". `-fno-inline`
# keeps the bodies out-of-line, restoring the import table to exactly the -O0 set
# (verified: O0=339 imports, plain -O1=449, but -O1/-O2 -fno-inline back to
# 340/337; SystemConfig stub gone; App ctor passes). Source-true fix
# (HX_NATIVE-gate the `inline`, drop the flag) is the follow-up; see
# docs/native/web-perf-handoffs/03-diagnosis-results.md.
#
# RENDER crash (2026-06-10): a SECOND, distinct -O>0 fault — a headless-Chromium
# renderer "Page crashed" (GPU-process death, no WebGPU validation error) that
# fired deterministically (NOT box-contention: reproduced 3/3 at load avg 2.6,
# and with --disable-gpu-watchdog) ~78s into smoke-test.mjs, during the
# tv11/ridingincab transition-vignette char-extras draw. Isolated by a per-TU
# -O0 binary search (15 builds) to a SINGLE translation unit:
# src/system/bandobj/OutfitConfig.cpp. At -O>0 (both -O1 and -O2) it miscompiles
# the outfit-mesh deform/AO data feeding the char-extras draw, corrupting a GPU
# buffer → process death; at -O0 the venue loads cleanly. Pinned to -O0 below
# (one ~1.1k-line TU; negligible size cost). Root-causing the exact UB/codegen
# trigger so the pin can be dropped is a follow-up (see
# docs/native/web-perf-handoffs/03-diagnosis-results.md). NOT the boot bug above
# (that's -fno-inline, global); this is one TU's render-data codegen.
set(RB3_WEB_OPT_LEVEL "O2" CACHE STRING
    "emcc -O level for rb3-web release builds (O0/Oz/Os/O3/O2/O1)")
set_property(CACHE RB3_WEB_OPT_LEVEL PROPERTY STRINGS O0 Oz Os O3 O2 O1)
if(RB3_WEB_RELEASE)
    message(STATUS "rb3-web: RELEASE strip + opt build (-${RB3_WEB_OPT_LEVEL} -fno-inline -g0)")
    # -fno-inline is REQUIRED at -O>0 (cross-TU inline-discard boot fix above);
    # harmless at -O0. Kept unconditionally so flipping --opt needs no other edit.
    target_compile_options(rb3-web PRIVATE -${RB3_WEB_OPT_LEVEL} -fno-inline -g0)
    target_link_options(rb3-web PRIVATE
        -${RB3_WEB_OPT_LEVEL}
        -g0
        # Keep the wasm name section (function names only, no DWARF): release
        # wedges/crashes captured via CDP Debugger.pause show real frames
        # instead of $funcNNNNN. ~1MB raw, compresses well; no codegen impact.
        --profiling-funcs
        # NOTE: deliberately leave -sASSERTIONS=1 and -sSTACK_OVERFLOW_CHECK=2
        # from the engine helper in place — the matched-fork's missing-stubs
        # surface relies on the runtime abort-patch hooks they install.
    )
    if(RB3_WEB_CLOSURE)
        message(STATUS "rb3-web: --closure 1 enabled (verify exports post-link)")
        target_link_options(rb3-web PRIVATE --closure 1)
    endif()
    # -O>0 RENDER-CRASH WORKAROUND: OutfitConfig.cpp miscompiles at -O>0 (both
    # -O1/-O2), corrupting the char-extras outfit-mesh draw data → GPU-process
    # death at the tv11 vignette load. Pin it to -O0 (COMPILE_OPTIONS appends
    # after the target's -O${LEVEL}; emcc honors the last -O). Bisect-confirmed:
    # this one TU at -O0 (rest -O2) makes smoke-test.mjs PASS. Drop this once the
    # underlying codegen/UB is root-caused. Harmless at RB3_WEB_OPT_LEVEL=O0.
    set_source_files_properties(
        ${REPO_ROOT}/src/system/bandobj/OutfitConfig.cpp
        PROPERTIES COMPILE_OPTIONS "-O0")
    # -O>0 CTOR-MISCOMPILE WORKAROUND #2 (web song-end freeze, 2026-07-03/05):
    # MetaPerformer.cpp miscompiles at -O2 — the MetaPerformer ctor's emitted
    # call sequence is correct (PendingDataInfoC2 at this+0x2C0, verified by
    # disasm) but the member-construction writes never land: at ctor-body-start
    # mPendingData.stats.mSoloStats already reads recycled-heap garbage
    # (bisect-proven via the RB3_STATS_DBG stomp-watch; debug -O0 web and
    # native x86 are healthy; NOT fixed by the web_data_stubs.cpp addr-0 data
    # fix — re-verified 2026-07-05). First touch of the garbage vector is the
    # song-end {meta_performer trigger_song_completion} copy, whose
    # __base_destruct_at_end wedges the tab on endgame_waiting ("render
    # freezes, audio drains out, then nothing"). One TU at -O0 (rest -O2)
    # passes the full song-end round trip (scripts/web/song-end-web-test.mjs).
    # Same class as the OutfitConfig pin above (suspect: wasm-EH scaffolding
    # around the vbase-heavy ctor at -O2). Drop when the emscripten toolchain
    # is upgraded past the codegen bug and song-end-web-test.mjs still passes.
    set_source_files_properties(
        ${REPO_ROOT}/src/band3/meta_band/MetaPerformer.cpp
        PROPERTIES COMPILE_OPTIONS "-O0")
    # Diagnosis hatch (mirrors RB3_WEB_O0_GLOB): compile the named TUs with
    # UBSan (recover mode — reports stream to the browser console with
    # file:line, execution continues). This is how "release-only freeze" bugs
    # split into "UB in the TU" vs "wasm backend miscompile": if an -O0 pin
    # fixes a TU, UBSan it at -O2 and read the console. Proven on the song-end
    # freeze (Stats.h GetHopoPercent NaN->int, 2026-07-02).
    if(RB3_WEB_UBSAN_GLOB)
        file(GLOB _rb3_ubsan_srcs ${RB3_WEB_UBSAN_GLOB})
        # APPEND, not set: set_source_files_properties would silently OVERWRITE
        # a per-TU -O0 pin above (COMPILE_OPTIONS last-write-wins), putting the
        # pinned TU back at -O2 and resurrecting the very bug being diagnosed.
        foreach(_rb3_ubsan_src ${_rb3_ubsan_srcs})
            set_property(SOURCE ${_rb3_ubsan_src} APPEND PROPERTY
                COMPILE_OPTIONS "-fsanitize=undefined")
        endforeach()
        target_link_options(rb3-web PRIVATE -fsanitize=undefined)
    endif()
    # Diagnosis hatch: whole-program AddressSanitizer for the web build.
    # Catches heap OOB/UAF WRITES at the writer with a C++ stack — the tool for
    # "deterministic heap stomp visible at boot" bugs the per-TU UBSan hatch
    # can't see. Boots much slower and needs extra memory; pair with the
    # boot-only probes, not gameplay perf work.
    if(RB3_WEB_ASAN)
        # recover mode so reports stream instead of aborting on the first (the
        # Symbols*.cpp duplicate-global odr-violation would otherwise kill boot
        # before anything interesting). Runtime options can't come from ENV on
        # web (ASan init runs in __wasm_call_ctors, before ENV lands), so
        # main_web.cpp compiles in __asan_default_options under this define.
        target_compile_options(rb3-web PRIVATE -fsanitize=address -fsanitize-recover=address)
        target_link_options(rb3-web PRIVATE -fsanitize=address -fsanitize-recover=address)
        target_compile_definitions(rb3-web PRIVATE RB3_WEB_ASAN_BUILD=1)
        message(STATUS "rb3-web: ASan build (RB3_WEB_ASAN=ON)")
    endif()
    # Escape hatch for re-testing the underlying fault / further bisects:
    # cmake -DRB3_WEB_O0_GLOB="<paths-or-globs>" forces extra TUs to -O0.
    if(RB3_WEB_O0_GLOB)
        file(GLOB _rb3_o0_srcs ${RB3_WEB_O0_GLOB})
        set_source_files_properties(${_rb3_o0_srcs} PROPERTIES COMPILE_OPTIONS "-O0")
        message(STATUS "rb3-web: extra -O0 on ${RB3_WEB_O0_GLOB} (${_rb3_o0_srcs})")
    endif()
endif()

endif() # EMSCRIPTEN
