# 03 — Diagnosis: the universal `-O>0` App-ctor boot exception (RESULTS)

**Date:** 2026-06-09. Investigation-only run of the plan in
[03-release-opt-build.md](03-release-opt-build.md). **Root-caused.** No production
code was changed; everything below ran in isolated build/serve dirs
(`/tmp/rb3-web-opt`, `/tmp/rb3-web-opt-serve`, port 8433). The deployed
`native/web/build/{release,debug}` and `native/build-web*` dirs were not touched.

## TL;DR

The W4a "matched-fork is brittle to optimizer-driven null-pointer /
exception-edge codegen" theory is **wrong**. There is no optimizer-exploited UB,
no JSPI interaction, no Binaryen bug. The fault is a **link-layer symbol-resolution
class**:

> **109 functions are defined with `inline` (or `FORCE_LOCAL_INLINE`) in .cpp
> files but called from OTHER TUs.** That is an ODR violation C++ tolerates
> silently. At `-O0` clang still emits the `linkonce_odr` bodies, so wasm-ld
> resolves the cross-TU calls and everything works. At any `-O>0` clang inlines
> the bodies into their in-TU callers and **discards the (legally discardable)
> definitions** — the cross-TU references become undefined symbols, which
> `-sERROR_ON_UNDEFINED_SYMBOLS=0` silently converts into **wasm env imports**,
> which `rb3_pre.js` patches into warn-and-return-undefined stubs at runtime.
> The first stub on the boot path whose return value matters —
> `SystemConfig(Symbol)`, defined `inline` at `src/system/os/System.cpp:527` —
> returns a NULL `DataArray*`; the caller dereferences it; the resulting error is
> caught by `main_web.cpp:644`'s `catch (...)` → *"boot error — exception during
> App construction"*.

**Containment validated:** `-O1 -fno-inline` (and `-O2 -fno-inline`, see below)
restores the import table to exactly the `-O0` set and **smoke-test PASSES**
(main_hub reached, 83 songs, no pageerror).

## Evidence chain

1. **Repro (verbatim W4a symptom).** Isolated `-O1` build boots to
   `BOOT_APP_CTOR`, then:
   ```
   [rb3-stub] env._Z17GetSystemLanguage6Symbol
   [rb3-stub] env._Z12SystemConfig6Symbol
   RB3 Web: boot error — exception during App construction
   ```
   (`/tmp/rb3-web-opt-smoke.log`). Note `GetSystemLanguage` is stubbed at `-O0`
   too (benign, returns a Symbol nobody derefs); **`SystemConfig` is stubbed ONLY
   at `-O1`** — that's the tell.

2. **Import-table diff is the smoking gun.** Parsing the wasm import sections
   (`/tmp/wasm_imports.py`):
   - deployed `-O0` release: **339** imports
   - isolated `-O1` build: **449** imports — **110 new**, of which 109 are C++
     mangled symbols (`/tmp/missing-syms{,-demangled}.txt`; the 110th is
     `js_audio_min_ring_depth`, a *resolved* EM_JS import from
     `milo-native-engine/src/audio/AudioDevice_Web.cpp:255` — benign).

3. **Every one of the 109 maps to the decomp's inline-in-.cpp matching idiom.**
   65 are single-line `inline` definitions in .cpp (e.g.
   `src/system/os/System.cpp:527` `inline DataArray *SystemConfig(Symbol)`,
   `src/system/ui/UIList.cpp:241` `inline UIList *UIList::ChildList()`,
   `src/system/rndobj/Anim.cpp:24` `inline float RndAnimatable::FramesPerUnit()`);
   the remaining 44 all use `FORCE_LOCAL_INLINE` on the preceding line (e.g.
   `src/system/utl/HxGuid.cpp:29-33` `HxGuid::IsNull`,
   `src/band3/meta_band/ProfileMgr.cpp:640`, `src/system/obj/DataArray.cpp:414`
   `DataArray::FindArray(Symbol, Symbol)`, `src/band3/game/Defines.cpp:121`).
   `FORCE_LOCAL_INLINE` (`src/decomp.h:4`) = `_Pragma("push") _Pragma("force_active on") inline`
   — on MWCC the pragma keeps the out-of-line copy (why the Wii build links);
   on clang the pragmas are no-ops and only the `inline` keyword remains.
   ~37 distinct defining TUs; top: `system/utl/TimeConversion.cpp` (6),
   `system/ui/UIList.cpp` (6), `system/ui/UIListState.cpp` (5),
   `system/os/System.cpp` (3).

4. **Minimal repro** (3 lines, no game code):
   ```cpp
   // a.cpp:  struct A { int f(); };  inline int A::f() { return 42; }  + in-TU use
   // b.cpp:  cross-TU call a.f()
   ```
   `emcc -O0` link: OK (`A::f` emitted as weak `W`). `emcc -O1` link:
   `wasm-ld: error: undefined symbol: A::f()` — the EXACT class. rb3-web never
   sees this hard error because of `-sERROR_ON_UNDEFINED_SYMBOLS=0`
   (`milo-native-engine/CMakeLists.txt:580`), which downgrades it to a runtime
   import. `-flto` and `-flto=thin` both resolve the minimal repro.

5. **Containment proof on the real build.**
   - `-O1 -fno-inline -g2 -sASSERTIONS=2`: import table = **340 = O0's 339 + the
     benign EM_JS one. All 109 gone.**
   - `node scripts/web/smoke-test.mjs --port 8433` → **PASS** — main_hub_screen
     reached, song DB populated (83 songs), no pageerror (87 s wall;
     `/tmp/rb3-web-o1ni-smoke{,.log}`).
   - `-O2 -fno-inline`: import table **337** (O0's set minus 3 DCE'd + the benign
     EM_JS one; zero C++ missing imports) and smoke-test **PASS** (main_hub, 83
     songs, no pageerror, 88 s wall; `/tmp/rb3-web-o2ni-smoke{,.log}`).
   - In the passing runs the only `[rb3-stub]` hits are the known-benign W3 set
     (PlatformMgr/WiiRnd/Movie/Net…) — zero `SystemConfig` stub hits.
   - The O1 run's `main_hub.png` renders normally (menu, band characters,
     overshell).

6. **Why "-O>0 throws an exception" looked like UB:** the stub returns
   `undefined` → wasm coerces to 0 → `SystemConfig()` "returns" NULL →
   `DataArray*` deref → error object unwinds into `main_web.cpp:644 catch (...)`.
   With 700+ benign MEMFS `ErrnoError`s thrown during FS init, a
   pause-on-all-exceptions CDP probe is useless noise — the import-diff method
   above is the right tool (and is build-cheap: no boot needed).

## Sizes (incidental but useful)

| build | raw wasm | brotli |
|---|---|---|
| deployed release `-O0 -g0` | 16,675,064 (16 M) | 2,386,041 (q11) |
| isolated `-O1 -g2(compile)/-g0(link)` | 9.0 M | — |
| isolated `-O1 -fno-inline` (no name section) | 9,028,990 (8.7 M) | 2,120,826 (q9; q11 will be smaller) |
| isolated `-O2 -fno-inline` (no name section) | 6,227,480 (6.0 M) | 1,677,009 (q9; q11 will be smaller) |

So even the *containment* build (inlining disabled!) is ~46% smaller raw and
beats the O0 brotli target. Runtime perf at `-O1 -fno-inline` vs `-O0` is
untested here — measure with `audio-jitter-profile.mjs` / `gpu-boot-probe.mjs`
per the acceptance section of 03-release-opt-build.md.

## Classification

**Cross-TU `inline`-definition discard (ODR violation) surfacing as silent
missing wasm imports** — a *link/symbol* issue, not codegen UB. The
`-sERROR_ON_UNDEFINED_SYMBOLS=0` + rb3_pre.js abort-patch pipeline (needed for
the genuinely-missing W3 stubs) is what makes it *silent* and boot-time instead
of a hard link error. DC3 pins O0 for the same reason and uses the identical
idiom — the fix transfers.

## Recommended fix (for the implementation agent)

In order of cost:

1. **Ship now (one line):** add `-fno-inline` to the `RB3_WEB_RELEASE` compile
   options next to `-${RB3_WEB_OPT_LEVEL}` in `native/CMakeLists.txt:942`, and
   flip the default `RB3_WEB_OPT_LEVEL` to `O2` (or `Os` after measuring).
   Verified working at O1 (smoke PASS) and O2 (import table clean). Replace the
   W4a comment block (`:925-936`) with the real root cause. Cost: loses
   user-code inlining (libc/system libs are prebuilt and unaffected); still keeps
   all other -O2 scalar/vectorize/DCE wins. This is strictly better than O0 on
   both axes.
2. **Source-true fix (removes the flag):** make the `inline` keyword vanish on
   the native fork for .cpp-local definitions:
   - `src/decomp.h`: define `FORCE_LOCAL_INLINE` as empty (no `inline`) under
     `HX_NATIVE`/non-MWCC — covers 44 of the 109 with zero Wii impact (macro
     expands to exactly today's text for MWCC).
   - Sweep the 65 bare-`inline` .cpp definition sites (exact list:
     `/tmp/missing-syms-demangled.txt`, definition map method in this doc §3)
     to a `DECOMP_LOCAL_INLINE` macro with the same gating.
   - **Gate:** the wasm import-table diff (script in this doc) must show zero
     C++ env imports added vs the `-O0` build; then drop `-fno-inline`.
   - **Hazard:** if any swept function is a file-local helper duplicated under
     the same name in two TUs, external linkage will collide at link (good —
     it surfaces; make those `static`).
   - Note the 109 list is what `-O1` happened to reach; the sweep should cover
     all `^inline`-in-.cpp definitions (362 candidate lines found), or keep the
     import-diff gate in CI.
3. **Alternative to (2):** `-flto=thin` resolves the class wholesale (proven on
   the minimal repro) *and* restores cross-TU inlining — but is untested on the
   full 28 MB link (build time, JSPI/EH interaction). Evaluate only after (1)
   ships.
4. **Closure remains a separate, known breakage** (rb3_pre.js source-string
   stub matching) — untouched by this diagnosis; see 03-release-opt-build.md.

### Gotchas for the implementer

- **CMake de-duplicates repeated link options**: appending a second `-g2` after
  the release block's `-g0` gets deduped against the engine helper's earlier
  `-g2`, so `-g0` still wins (observed: `link.txt` order
  `-sASSERTIONS=1 … -g2 … -O2 -g0 -sASSERTIONS=2`). If you want symbolized
  release stacks, use `"SHELL:-g2"` or `-gsource-map` for the later occurrence.
- Reconfigure note: this diagnosis injected flags via
  `-DCMAKE_PROJECT_INCLUDE=/tmp/rb3-web-opt-inject.cmake` with a
  `cmake_language(DEFER)` hook — handy for flag A/B without touching the tree.

## Artifacts

- `/tmp/rb3-web-opt/` — isolated emcc build dir (final state: `-O2 -fno-inline`)
- `/tmp/rb3-web-opt-inject.cmake` — deferred flag-injection file
- `/tmp/rb3-web-opt-serve/` — isolated deploy (server.py copy, port 8433)
- `/tmp/wasm_imports.py` — wasm import-section lister (the diagnosis tool)
- `/tmp/imports-{o0,o1,o1ni}.txt`, `/tmp/missing-syms{,-demangled}.txt`
- `/tmp/rb3-web-opt-smoke.log` (O1 FAIL repro),
  `/tmp/rb3-web-o1ni-smoke{,.log}` (O1 -fno-inline PASS + screenshot),
  `/tmp/rb3-web-o0-smoke.log` (O0 baseline)
- `scripts/web/_opt-exc-probe.mjs` — CDP pause-on-exception probe (kept for
  reference; superseded by the import-diff method)
