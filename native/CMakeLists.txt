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
set(MILO_ENGINE_PIN "cfaaa5bc3ddbaaed84f9db6fe6d9517f77e8729c"
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
    BandPatchMesh
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
find_package(Threads REQUIRED)
find_package(ZLIB)

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
    target_link_libraries(${tgt} PRIVATE milo-engine Threads::Threads)
    target_link_libraries(${tgt} PRIVATE vorbis vorbisfile ogg)
    if(ZLIB_FOUND)
        target_link_libraries(${tgt} PRIVATE ZLIB::ZLIB)
    endif()
    # Off-path symbols (rendering/audio/Bink/MIDI/RSO/Wii managers + their
    # vtables/typeinfo) are satisfied by the weak no-op stubs in
    # dta_link_stubs.s — calling one harmlessly returns, reading one yields a
    # valid non-null pointer. Preferable to --unresolved-symbols=ignore-all,
    # which leaves null symbols that crash when a global ctor calls one at
    # static-init time.
    target_link_options(${tgt} PRIVATE -Wl,--allow-multiple-definition)
endfunction()

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
    # X6: native NativeSynth (boots miniaudio AudioDevice + binds the
    # RB3-shaped StreamReceiver factory below)
    ${CMAKE_SOURCE_DIR}/src/rb3_synth_native.cpp
    # X6: RB3-shaped StreamReceiver bridging the matched-fork synth onto
    # AudioDevice. Bound to StreamReceiver::sFactory inside NativeSynth::Init.
    ${CMAKE_SOURCE_DIR}/src/rb3_stream_receiver_native.cpp
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
