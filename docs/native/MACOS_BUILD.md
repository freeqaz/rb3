# Building `rb3-native` on macOS (Apple Silicon + Intel)

The native port uses Apple Clang, Cocoa and Metal on macOS. The RB3 Wii
decompilation build remains separate. For native game boot, use an extracted RB3
Xbox 360 ARK asset tree.

On November 23, 2026, the Apple M3 build on macOS 15.7.4 linked and ran the
minimal engine and Metal readback smoke tests. The game entry point initialized
its 1280×720 renderer. Gameplay verification needs the extracted asset tree.

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
6. **Linking** — Apple's default linker checks for missing and duplicate
   definitions. Cross-file callers use the original implementations, and the
   weak stubs for off-path platform calls use the macOS libc++ names.

The Apple build generates Mach-O weak link definitions from the existing stub
registries and compiles the Cocoa WebGPU backend as Objective-C++. The debug
HTTP server uses Darwin libc and the ordinary health, screenshot and input
routes. The ELF x86-64 replay API requires its original host ABI.

---

## Prerequisites (one-time)

```bash
# Xcode Command Line Tools
xcode-select --install

# Build tools + native link deps (zlib ships with the macOS SDK)
brew install cmake ninja pkgconf glfw libvorbis libogg googletest python
```

The shared engine must be a sibling of `rb3/`:
`.../milohax/milo-native-engine` (see CLAUDE.md "Shared Engine"). Keep its
`git HEAD` at the SHA in `native/CMakeLists.txt`'s `MILO_ENGINE_PIN`
(a mismatch only warns).

### Dawn for macOS (Metal)

If a sibling DC3 checkout has already bootstrapped Dawn, reuse it.

```bash
# From the RB3 repository root. DAWN_DIR is the install prefix.
DAWN_DIR="$PWD/../dawn-prebuilt" scripts/native/build-macos.sh
```

To build Dawn from source, use the recipe below. The Linux Dawn under `../dc3-decomp-deps` is Vulkan and will **not** link on
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

---

## Running it

Run from the RB3 repository root. macOS uses a Cocoa window by default.
Set `MILO_HEADLESS=1` when using the HTTP debug workflow without a window.

```bash
export RB3_DATA="$PWD/orig-assets/extracted"
export RB3_DTA_OVERLAY="$PWD/native/dta"

# Full-game desktop launch
RB3_GAME=1 native/build-macos/rb3-native

# Offscreen launch with the debug server on loopback
MILO_HEADLESS=1 RB3_GAME=1 RB3_HTTP=1 native/build-macos/rb3-native
# In another terminal
curl -fsS http://127.0.0.1:8080/api/health
curl -fsS -o /tmp/rb3-frame.png http://127.0.0.1:8080/api/screenshot

# Asset-independent Metal smoke test
MILO_HEADLESS=1 RB3_GPU_SMOKE=1 RB3_GPU_SMOKE_PNG=/tmp/rb3-gpu-smoke.png \
    native/build-macos/rb3-native
```

Set `RB3_HTTP_PORT` to use a different port. To inspect a single scene, pass an
absolute `.milo_xbox` path to the binary without `RB3_GAME`, or use
`RB3_VIEWER=1` with the extracted asset tree to render a scene.

### CRKD Guitar LM

Use PS3 mode (USB `12BA:0100`) for the desktop build. The game assigns each
connected CRKD to its own local pad. Click stick 2 for Start and use Select or
tilt for overdrive. The frets, strum bar, D-pad and analog whammy are mapped
from the controller's HID layout.

### Assets

`RB3_DATA` must contain the arkhelper-extracted Xbox assets (the same tree the
Linux build reads): `config/`, `songs/`, the `.milo_xbox` scenes, moggs, etc.
Reuse the tree that the Linux build reads, or run
`scripts/milo/extract_ark.sh --hdr /path/to/rb3/gen/main_xbox.hdr` with
`arkhelper` on a host where that tool runs. The ARK parts must be alongside the
header. Game boot reads `config/band_preinit_keep.dta` and
`config/band_keep.dta` from this tree.

The archive stores the shared system files under the literal `(..)/(..)`
directories. For the default extraction location, add the following symlink
from the repository root if `system` does not exist yet.

```sh
ln -s 'orig-assets/extracted/(..)/(..)/system' system
```

---

## Optional targets

- **Tests** (`rb3-tests`): GTest comes from Homebrew. The native configuration
  enables this target by default. Set `-DBUILD_TESTS=OFF` in a manual configure
  to build only the application without requiring GTest.
- **`rb3-dta`** (headless DTA parser): builds the same way, but note it is
  **currently broken on master on all platforms** — unrelated to macOS — because
  in-progress session-telemetry TUs reference `RB3LoadDet*` symbols not in
  rb3-dta's link set. `rb3-native` is unaffected.
