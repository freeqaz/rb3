# rb3-native macOS build — status (2026-07-13)

Outcome: **BUILDABLE-PENDING-MAC-VERIFY**

Goal: make `rb3-native` buildable on macOS so the user can test on their Mac.
The Linux dev host cannot compile/run a macOS binary, so the deliverable is a
correct-by-construction tree + a turnkey recipe, with the Linux build proven
non-regressed.

## What I changed

All edits are in **`native/CMakeLists.txt`**, every macOS path inside
`if(APPLE)` (6 guards added). Mirrors the proven `dc3-native` recipe
(dc3-decomp/native/CMakeLists.txt), which builds this same shared engine on
macOS.

1. **Compat flags** — `__GNUC_STDC_INLINE__` / `__GCC_ATOMIC_*` moved into a
   `NOT APPLE` branch (Linux/libstdc++ only); APPLE gets
   `-include stdarg.h -D_VA_LIST_T` for the `-fms-compatibility` va_list clash.
2. **`enable_language(OBJCXX)`** on APPLE (for the engine's `MetalSurface.mm`).
3. **Homebrew** include/link dirs (`/opt/homebrew` + `/usr/local`).
4. **Frameworks** `Cocoa IOKit Metal QuartzCore` on each native target.
5. **Dawn_DIR** default made platform-conditional (macOS →
   `~/dc3-deps/dawn/lib/cmake/Dawn`, not the Linux Vulkan Dawn).
6. **Linker** `--allow-multiple-definition` → ld64
   `-ld_classic -undefined,dynamic_lookup -multiply_defined,suppress` on APPLE.

New files:
- **`scripts/native/build-macos.sh`** — turnkey build (prereq checks + configure
  + build + run hints). `--help`, `--reconfigure`, `DAWN_DIR`/`BUILD_DIR`/`TARGET`
  env overrides. `bash -n` clean.
- **`docs/native/MACOS_BUILD.md`** — full recipe (Dawn-on-Mac, brew deps,
  configure/build/run, assets, Mac-only verify list).

No `native/src/**` code changes were needed — audited clean for macOS:
`execinfo`/`backtrace` exist on macOS; `malloc_trim` already `__GLIBC__`-guarded;
`dlfcn`/`clock_gettime` portable; no `/proc`, epoll, sysinfo, `pthread_setname_np`,
`<linux/*>`.

No engine changes: the pinned engine SHA (`2ea8e343`, = current HEAD) already
contains `src/gfx/MetalSurface.mm` and the `SurfaceSourceMetalLayer` `__APPLE__`
path in `GpuDevice.cpp`. **No pin bump.**

## Linux regression — VERIFIED GREEN

Configured a fresh isolated build dir and built the deliverable target:

```
cmake -B build-macosguard-verify -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DDawn_DIR=.../dc3-decomp-deps/dawn/lib/cmake/Dawn
cmake --build build-macosguard-verify --target rb3-native
```

- Configure: clean (engine gfx ON, backend `rb3`, Dawn/glfw/ZLIB/GTest found).
- Build: **`rb3-native` links** → 49 MB ELF x86-64, runs (prints usage + stub
  census, exit 0).

`rb3-dta` fails to link (`RB3LoadDet*` undefined) — **pre-existing on master,
NOT my change**: verified it fails identically in the untouched `build-native`
dir with the old cached config. Cause: another agent's uncommitted
session-telemetry WIP (`M native/src/rb3_session_trace.cpp` in git status)
references `RB3LoadDetFrameTap`/`gRB3LoadDetAttribOn` from Rand.cpp +
rb3_session_trace.cpp, TUs rb3-dta compiles, without adding
`rb3_loaddet_probe.cpp` to rb3-dta's link list. Out of scope here.

## Dawn-on-Mac solution

Build a Metal-enabled Dawn from source once (`DAWN_ENABLE_METAL=ON`,
Vulkan/GL OFF), install to `~/dc3-deps/dawn` (the APPLE `Dawn_DIR` default);
overridable via `-DDawn_DIR=`. Same Dawn DC3's macOS build uses — reuse if
present. This is the single biggest Mac-side unknown (CMake-package/version drift).

## Remaining Mac-only-verifiable (user runs on Mac)

1. Build Metal Dawn; confirm `find_package(Dawn)` resolves.
2. `-ld_classic` may be rejected by Xcode 16+ ld — fallback documented (drop it).
3. Apple-clang `-fms-compatibility` edge cases beyond the va_list fix.
4. libc++ vs libstdc++ STL idiom differences in the matched fork.
5. Run one `.milo` scene / one song e2e + screenshot to confirm Metal renders.

## Commit

Landed on `master` after the Linux regression passed (see repo log for SHA):
`native/CMakeLists.txt`, `scripts/native/build-macos.sh`,
`docs/native/MACOS_BUILD.md`, this file. Staged only my files (concurrent agents
active). The disposable `build-macosguard-verify/` dir is not committed.
