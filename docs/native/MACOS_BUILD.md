# Building `rb3-native` on macOS (Apple Silicon + Intel)

Status: **BUILDABLE-PENDING-MAC-VERIFY** (2026-07-13).

The CMake wiring below was added by mirroring the **proven `dc3-native` macOS
recipe** (DC3 shares this exact engine) and was **regression-tested on Linux**
(rb3-native still configures, compiles, and runs). It has **not** been compiled
on real macOS hardware from this repo yet — the dev host is Linux and cannot
produce a macOS binary. Everything here is correct-by-construction; the
Mac-only steps you must run are called out in the last section.

Turnkey script: [`scripts/native/build-macos.sh`](../../scripts/native/build-macos.sh)
(`scripts/native/build-macos.sh --help`). This doc is the manual/explanatory
version.

---

## Why it works with so few changes

The native app compiles the shared `src/` matched-fork + the shared
`../milo-native-engine` with the **host clang** (NOT MWCC — the wibo/mwcceppc
toolchain is only for the Wii asm-match target and is irrelevant here). The
platform pieces are already cross-platform:

| Component | macOS backend | Status |
|---|---|---|
| GPU (WebGPU via Dawn) | **Metal** | Engine has the full `__APPLE__` surface path in `src/gfx/GpuDevice.cpp` + `src/gfx/MetalSurface.mm` (CAMetalLayer). **Done.** |
| Windowing / input (GLFW) | Cocoa | `find_package(glfw3)`; Homebrew glfw. |
| Audio (miniaudio) | CoreAudio | Automatic — miniaudio picks CoreAudio on Darwin. |
| Engine + game C++ | Apple clang / libc++ | Platform-independent. |
| Signal backtrace (`execinfo.h` / `backtrace`) | libSystem | Exists on macOS — no guard needed. |
| `malloc_trim` heap trim | n/a | Already guarded `#if defined(__GLIBC__)...` → no-op on macOS. |

`native/src/**` has **no** other Linux-only surface (no `/proc`, epoll,
`sysinfo`, `pthread_setname_np`, `<linux/*>`; `dlfcn.h` and `clock_gettime`
are portable). All macOS work lives in **`native/CMakeLists.txt`**, inside
`if(APPLE)` guards — Linux is untouched.

### What the APPLE guards do (in `native/CMakeLists.txt`)

1. **Compat flags** — the Linux `__GNUC_STDC_INLINE__` / `__GCC_ATOMIC_*`
   defines (libstdc++/GCC-15 workarounds) are skipped on macOS (libc++ needs
   none). Instead macOS gets `-include stdarg.h -D_VA_LIST_T` to fix the
   `-fms-compatibility` `__builtin_va_list` conflict with the SDK va_list.
2. **`enable_language(OBJCXX)`** — so the engine's `MetalSurface.mm` (added to
   `libmilo-engine.a`) compiles. The engine relies on the consumer to enable it.
3. **Homebrew paths** — `include_directories`/`link_directories` for
   `/opt/homebrew` (Apple Silicon) and `/usr/local` (Intel).
4. **Frameworks** — `Cocoa IOKit Metal QuartzCore` linked to each native target
   (mostly redundant with Dawn/glfw INTERFACE deps, explicit to cover
   `MetalSurface.mm`).
5. **Dawn_DIR default** — the Linux `../dc3-decomp-deps/dawn` (Vulkan) is not
   usable on macOS; the APPLE default is `~/dc3-deps/dawn/lib/cmake/Dawn`
   (overridable with `-DDawn_DIR=`).
6. **Linker flags** — GNU `-Wl,--allow-multiple-definition` → Apple ld64
   `-Wl,-ld_classic -Wl,-undefined,dynamic_lookup -Wl,-multiply_defined,suppress`.

No engine changes are needed: the pinned engine SHA (`MILO_ENGINE_PIN` in
`native/CMakeLists.txt`) already contains `MetalSurface.mm` and the Metal
surface path.

---

## Prerequisites (one-time)

```bash
# Xcode Command Line Tools
xcode-select --install

# Build tools + native link deps (zlib ships with the macOS SDK)
brew install cmake ninja glfw libvorbis libogg
```

The shared engine must be a sibling of `rb3/`:
`.../milohax/milo-native-engine` (see CLAUDE.md "Shared Engine"). Keep its
`git HEAD` at the SHA in `native/CMakeLists.txt`'s `MILO_ENGINE_PIN`
(a mismatch only warns).

### Building Dawn for macOS (Metal) — one-time, ~15 min

The Linux Dawn under `../dc3-decomp-deps` is Vulkan and will **not** link on
macOS. Build a Metal Dawn once:

```bash
git clone https://dawn.googlesource.com/dawn && cd dawn
cp scripts/standalone.gclient .gclient
gclient sync        # needs depot_tools on PATH (or: brew install depot_tools)

cmake -B build-metal -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \        # or x86_64 on Intel
    -DDAWN_ENABLE_METAL=ON \
    -DDAWN_ENABLE_VULKAN=OFF \
    -DDAWN_ENABLE_DESKTOP_GL=OFF -DDAWN_ENABLE_OPENGLES=OFF \
    -DDAWN_BUILD_SAMPLES=OFF -DTINT_BUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX=$HOME/dc3-deps/dawn
cmake --build build-metal -j$(sysctl -n hw.ncpu)
cmake --install build-metal
```

This installs the CMake package config at `~/dc3-deps/dawn/lib/cmake/Dawn`,
which is the APPLE default. (This is the same Dawn DC3's macOS build uses; if
you already built it for DC3, reuse it via `-DDawn_DIR=`.)

> Dawn's CMake version should match what the engine expects. If the engine's
> `find_package(Dawn)` or the WebGPU headers mismatch, rebuild Dawn from the
> revision DC3/RB3's Linux `dc3-decomp-deps/dawn` was built from (check that
> tree's `args.gn`/version), or align `imgui`'s Dawn-API tag.

---

## Configure + build

```bash
cd rb3/native
cmake -B build-macos -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DDawn_DIR=$HOME/dc3-deps/dawn/lib/cmake/Dawn
cmake --build build-macos --target rb3-native -j$(sysctl -n hw.ncpu)
```

Or just: `scripts/native/build-macos.sh` (does all prereq checks + the above).

Expected output: `rb3/native/build-macos/rb3-native` (a Mach-O arm64/x86_64
executable).

The `undefined symbol` link warnings for a handful of store/network/PlatformMgr
symbols are **expected** (weak stubs / out-of-scope Wii backends), same as on
Linux; `-undefined,dynamic_lookup` tolerates them.

---

## Running it

The harness is **headless-first**, driven over an HTTP debug API — identical to
the Linux workflow (`scripts/native/song-end-test.py`,
`scripts/native/song-select-capture.py`). It needs the extracted **Xbox** ARK
assets; point `RB3_DATA` at their root (mirrors the Linux default
`rb3/orig-assets/extracted`).

```bash
export RB3_DATA=/path/to/rb3/orig-assets/extracted

# Full game boot + HTTP debug server (default :8080; RB3_HTTP_PORT overrides):
RB3_GAME=1 RB3_HTTP=1 ./build-macos/rb3-native
#   curl localhost:8080/api/health
#   curl -o shot.png localhost:8080/api/screenshot
#   curl -XPOST localhost:8080/api/input -d 'green'      # nav verbs
#   curl 'localhost:8080/api/dta/eval?...'               # live engine state

# Or dump a .milo scene tree (no assets needed beyond the file):
./build-macos/rb3-native /abs/path/to/scene.milo_xbox
```

Other modes (env-gated, see `native/src/main_native.cpp`): `RB3_VIEWER=1`
(standalone `.milo` renderer), `RB3_GPU_SMOKE=1` (clear-color PNG),
`RB3_RENDER_MESH=1`. Windowed vs offscreen follows the engine's GLFW path; the
default debug flow renders offscreen and reads back via `/api/screenshot`.

### Assets

`RB3_DATA` must contain the arkhelper-extracted Xbox assets (the same tree the
Linux build reads): `config/`, `songs/`, the `.milo_xbox` scenes, moggs, etc.
Extraction is not part of this build — reuse the tree the Linux/web builds
already use, or extract with the repo's ARK tooling. `.mogg` decryption keys are
supplied natively (`rb3_keychain_native.cpp`); prefer Xbox assets (audio/Bink).

---

## Optional targets

- **Tests** (`rb3-tests`): `brew install googletest`, then add `-DBUILD_TESTS=ON`
  (GTest via Homebrew). Inherits the same APPLE wiring.
- **`rb3-dta`** (headless DTA parser): builds the same way, but note it is
  **currently broken on master on all platforms** — unrelated to macOS — because
  in-progress session-telemetry TUs reference `RB3LoadDet*` symbols not in
  rb3-dta's link set. `rb3-native` is unaffected.

---

## Mac-only steps YOU must still verify

Everything below is unverifiable on the Linux dev host:

1. **Build Metal Dawn** (above) and confirm `find_package(Dawn)` succeeds — the
   biggest unknown. Dawn CMake-package/version drift is the most likely failure.
2. **`-ld_classic`** — deprecated in the newest Xcode (16+). If the linker
   rejects it, drop it and rely on `-undefined,dynamic_lookup` alone, or add
   `-Wl,-no_warn_duplicate_libraries`. The duplicate-symbol tolerance
   (`-multiply_defined,suppress`) is what needs the classic linker.
3. **Apple clang `-fms-compatibility` edge cases** — the decomp headers lean on
   MSVC-compat parsing; the `stdarg.h`/`_VA_LIST_T` workaround handles the known
   va_list clash, but other Apple-clang-vs-Linux-clang differences may surface.
4. **libc++ vs libstdc++** — a handful of STL idioms in the matched fork could
   differ; the `include/` `__wrap_iter` shim is libc++-shaped already (good).
5. **Run one scene / one song** end-to-end and capture a screenshot to confirm
   the Metal path renders (acceptance in NATIVE_PORT_ROADMAP.md Phase 6).

Report back the first compile/link error if any — most will be a missing brew
dep, a Dawn version mismatch, or an `-ld_classic` rejection, each with a quick fix.
