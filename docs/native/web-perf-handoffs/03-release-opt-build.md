# 03 — Un-break optimized release wasm (P1.2)

## Problem

Release web build ships `-O0 -g0` (16 MB raw wasm / 2.3 MB brotli — both VERIFIED;
~7.5 s cold compile, claimed). `--opt` and `--closure` are documented BROKEN as of W4a
(CMakeCache confirms both release+debug pinned at `RB3_WEB_OPT_LEVEL=O0`,
`RB3_WEB_CLOSURE=OFF`). `-O2`/`-Os` should shrink + speed up both compile and runtime.
Note up front: DC3's web build also defaults to O0 for the SAME matched-fork
exception-edge brittleness (see step 5) — treat the `-O>0` fault as a shared-engine /
matched-fork codegen class, not RB3-specific.

## Recorded facts (verified in-tree, re-verified 2026-06-09)

- **build.sh plumbing**: flags declared/parsed at `scripts/web/build.sh:59-70`
  (`CLOSURE`, `OPT_LEVEL`, `--opt`, `--closure`); the actual `-D` mapping is at
  `:152-155`: `--opt L` → `-DRB3_WEB_OPT_LEVEL=L`, `--closure` → `-DRB3_WEB_CLOSURE=ON`.
  The BROKEN help-text is at `:21-24`. **VERIFIED.**
- **CMake** (`native/CMakeLists.txt:900-954`): release block (`if(RB3_WEB_RELEASE)`,
  `:940`) adds `-${RB3_WEB_OPT_LEVEL} -g0` to compile (`:942`) + link (`:943-945`);
  closure adds `--closure 1` (`:952`). **CMakeCache re-checked 2026-06-09:** both
  `native/build-web-release` AND `native/build-web` show `RB3_WEB_OPT_LEVEL=O0`,
  `RB3_WEB_CLOSURE=OFF` (release has `RB3_WEB_RELEASE=ON`, debug `OFF`; both
  `MILO_WEB_ASYNC=ON`). **VERIFIED.**
- **The W4a record** (`native/CMakeLists.txt:925-931`, quote):
  > ANY -O>0 (-O1, -Os, -Oz, -O3) currently throws an uncaught exception during App
  > construction, right after the missing-stub returns from `SystemConfig(Symbol)`. The
  > matched-fork is brittle to optimizer-driven null-pointer / exception-edge codegen
  > — same class of issue as the W3 missing-stub work.
  > Repro: build `--release --opt O1`, run smoke → "boot error — exception during App
  > construction".
  **VERIFIED** (quote matches `:925-931`; the repro line paraphrases `:929-931`).
- **Closure breakage is SEPARATE** (`CMakeLists.txt:912-918`): `native/web/rb3_pre.js`
  (`:49-70`) patches missing-stub imports inside `instantiateWasm` by walking every
  import module's functions, `String(v)`-ing each, and string-matching the function
  **source** for `'missing function'` (Emscripten's auto-generated abort-stub text);
  matches are replaced by name `modName + '.' + name`. `--closure 1` minifies the JS
  glue → the abort-stub source no longer contains the literal string → 0 stubs patched
  → `unreachable`/trap on first stub call. **VERIFIED** (mechanism is source-string
  match, not name match — corrected from the earlier "by NAME" phrasing).
- **Toolchain**: emcc **5.0.2** (`/home/free/emsdk`; `$EMSDK` unset, default `~/emsdk`).
  **VERIFIED** (`~/emsdk/upstream/emscripten/emscripten-version.txt` = "5.0.2"). JSPI
  enabled via `-sJSPI_EXPORTS` (`CMakeLists:891-895`, gated on `MILO_WEB_ASYNC=ON`).
  `-sASSERTIONS=1 -sSTACK_OVERFLOW_CHECK=2` (from the engine helper) deliberately kept in
  release — the CMake comment that says so is at `:946-948`. **VERIFIED.**
- **Sizes** (re-checked `ls -lh native/web/build/`): release `rb3-web.wasm` = 16M raw /
  2.3M brotli / 4.0M gzip; debug `rb3-web.wasm` = 28M raw / 5.3M gzip. **VERIFIED.**

## Diagnosis plan (in order; stop when root-caused)

1. Reproduce: `scripts/web/build.sh --release --opt O1` + `node scripts/web/smoke-test.mjs`.
2. Localize: rebuild with `-O1 -g2` (keep symbols!) + `-sASSERTIONS=2`; get the JS
   exception + wasm stack from smoke-test `--verbose` console capture. The known
   neighborhood is right after `SystemConfig(Symbol)` in the App ctor — likely a null
   deref / UB the optimizer now exploits (same class as the W3 missing-stub work).
3. Bisect optimization, not code: `-O1 -fno-inline`, then selectively
   `-mllvm`-level or per-TU `-O0` (CMake `set_source_files_properties`) on the
   suspect TU(s) to isolate which TU's optimized codegen faults. A per-TU `-O0`
   pragma/property on ONE file is an acceptable shipping workaround if the UB fix is
   deep.
4. Rule out JSPI interaction: temporarily disable the async path (MILO_WEB_ASYNC or
   equivalent) at `-O1` — diagnostic only.
5. ~~Check DC3: does its web build ship `-O>0`?~~ **PRE-ANSWERED (verifier,
   2026-06-09): NO.** DC3's web build also defaults to `DC3_WEB_OPT_LEVEL=O0`
   (`/home/free/code/milohax/dc3-decomp/native/CMakeLists.txt:1445`; both its
   `build-web-release` and `build-web` CMakeCache show `O0`). DC3's own comment
   (`:1439-1442`) says: "any -O>0 currently risks the matched-fork's optimizer-driven
   exception-edge codegen (**same brittleness rb3 documents**), so the default is O0".
   → This is a **SHARED matched-fork codegen class**, NOT RB3-specific UB. The bisect
   (steps 2-4) should expect the faulting TU(s) to be in shared engine / matched-fork
   code, and a fix may benefit both ports. (DC3 has no `--closure` knob — it uses the
   engine's `missing_stubs.js`, with no pre-js abort-patcher to break.)
6. Once `-O1` boots, walk up: `-Os`, `-O2`. Pick the best size/speed point (likely
   `-Os` for wire size; measure both).

## Closure (separate, optional, after -O works)

Fix `rb3_pre.js` stub patcher to be minification-proof: patch by import-table position
/ signature instead of name+source string, or emit a build-time stub-name manifest that
closure externs preserve. Lower priority than -O; skip if time-boxed.

## Acceptance

- `scripts/web/build.sh --release --opt <chosen>` exits 0; `smoke-test.mjs` green 10/10.
- Full nav to gameplay once (lib/core.mjs); audio still plays
  (web-song-preview-audio.mjs or audio-jitter-profile.mjs short run).
- Record: raw/gzip/brotli wasm sizes + cold-compile time (gpu-boot-probe.mjs cold run)
  before/after, in the commit message. Targets: brotli ≤ 2.3 MB (likely ~1.5 MB),
  cold compile well under 7.5 s.
- Root cause written into `native/CMakeLists.txt` comment (replacing the W4a BROKEN
  note) + a short doc. If the fix is a per-TU workaround, say so explicitly.
- BONUS: with `-O2`, re-run `audio-jitter-profile.mjs` gameplay capture — runtime fps
  may improve materially (feeds the P0.2/P2 decision).

## IMPLEMENTED (2026-06-09) — diagnosis phase only (investigation, no production changes)

**Status: ROOT-CAUSED + containment validated.** Full results in
[03-diagnosis-results.md](03-diagnosis-results.md). No commits — diagnosis ran
entirely in isolated dirs (`/tmp/rb3-web-opt*`, server on :8433); the deployed
`native/web/build/*` and `native/build-web*` were not touched; no tracked file
edited.

- Root cause is NOT optimizer-driven UB / JSPI / Binaryen: **109 functions
  defined `inline`/`FORCE_LOCAL_INLINE` in .cpp files with cross-TU callers**
  (decomp matching idiom; ODR violation). At -O>0 clang discards the inlined
  linkonce_odr bodies → `-sERROR_ON_UNDEFINED_SYMBOLS=0` silently turns the
  cross-TU calls into env imports → rb3_pre.js stubs them →
  `SystemConfig(Symbol)` (System.cpp:527) returns NULL `DataArray*` on the App
  boot path → deref → the recorded "boot error — exception during App
  construction".
- Measured: O0 import table 339; plain -O1 = 449 (**+110**); `-O1 -fno-inline`
  = 340 and `-O2 -fno-inline` = 337 (all 109 C++ imports gone) — **smoke-test
  PASS at both** (main_hub, 83 songs, no pageerror).
- Sizes: raw wasm 16 M (O0) → 8.7 M (O1ni) → **6.0 M (O2ni)**; brotli q9 of
  O2ni = **1.68 MB** (deployed q11 baseline 2.39 MB) — beats the ≤2.3 MB target
  with inlining disabled.
- Recommended ship: add `-fno-inline` next to `-${RB3_WEB_OPT_LEVEL}` in
  `native/CMakeLists.txt:942` + default `O2`; then the source-true fix
  (HX_NATIVE-gate the `inline` on the .cpp definition sites, list + CI gate in
  the results doc). `-flto=thin` proven on a minimal repro as the
  inlining-preserving alternative. Closure breakage unchanged/separate.
- Deviation from plan: per-TU `-O0` containment was NOT tested — unnecessary
  (the fault is not per-TU codegen; 37 defining TUs) and strictly worse than
  `-fno-inline`. Steps 4 (JSPI rule-out) and 6 (walk-up) subsumed by the O2
  PASS.

## IMPLEMENTED — SHIP attempt (2026-06-09) — boot fix landed, -O>0 default DEFERRED

Acting on the SHIP-NOW recommendation. Wired the `-fno-inline` boot fix into the
real build (`native/CMakeLists.txt` RB3_WEB_RELEASE block) + un-broke the `--opt`
knob in `build.sh`, and flowed the default through `build.sh --release` (no
`--opt` required). Built BOTH release + debug under the repo's flock lock.

**The boot crash is FIXED and verified on the real build.** `-fno-inline` at
-O>0 restores the import table to the -O0 set (measured here: O0=339, plain
-O1=449, **-O2 -fno-inline=337, -O1 -fno-inline=340**), `SystemConfig(Symbol)`
is no longer a stub, and the App ctor passes — boot reaches the same ~78 s
venue-load point as O0 (the only `[rb3-stub]` hits past boot are the known-benign
W3 set: DiscErrorMgrWii / WiiRnd / etc., zero SystemConfig). The diagnosis above
is fully confirmed.

**But a SECOND, distinct fault blocks shipping -O>0 as the default here.** Under
`node scripts/web/smoke-test.mjs` on this (loaded) box, every -O>0 build crashes
the **headless-Chromium renderer** (`[CRASH] Page crashed!`) deterministically at
the main_hub venue-vignette + char-extras load (~79 s, right after the `tv11`
transition milos / `male_extras11`), while -O0 passes:

| build | smoke runs | result |
|---|---|---|
| -O0 -fno-inline (release default) | 2 | **PASS** (main_hub, 83 songs, ~87 s) |
| -O0 (deployed debug `?debug=true`) | 1 | **PASS** (main_hub, 83 songs, 86.7 s) |
| -O1 -fno-inline | 2 | **CRASH** at ~78.9 s (renderer) |
| -O2 -fno-inline | 2 | **CRASH** at ~78.7 s (renderer) |

This is render-time, not boot: the crash is downstream of the now-fixed
SystemConfig path, and host GPUs are idle (2×24 GB, 410 MiB used) so it is not
host-OOM — it is a per-renderer GPU/process crash at the venue draw burst. The 03
diagnosis saw `-O1/-O2 -fno-inline` smoke **PASS** on a quiet isolated box; here,
under load avg ~7-8 with a rogue `python` at 281 % CPU, the optimized build's
faster boot pacing appears to hit the render burst harder. **Unresolved whether
this is box/contention pacing or a genuine -O>0 codegen issue in the render
path.** Per the task fallback rule I tried O1 -fno-inline (also verified by 03);
it crashed too (2/2), so I did NOT ship either -O>0 as default.

**Shipped:** release default **stays O0**, but with the boot fix mechanism fully
wired and ready: `-fno-inline` is unconditional (harmless at O0), `RB3_WEB_OPT_LEVEL`
defaults O0 and is overridable via `build.sh --release --opt O2`, and the
CMake/build.sh comments document the real root cause (boot) + the new render-time
blocker. So `--opt O2` produces the small `-fno-inline` build for a one-command
re-test on a quiet box.

### Sizes (raw / brotli q11 / gzip -9, deployed release wasm)

| build | raw | brotli | gzip |
|---|---|---|---|
| O0 (before, baseline) | 16,698,323 (16 M) | 2,386,970 (2.3 M) | 4,098,981 (4.0 M) |
| O2 -fno-inline | 6,229,405 (6.0 M) | 1,495,717 (1.5 M) | 2,062,617 (2.0 M) |
| O1 -fno-inline | 9,035,850 (8.6 M) | 1,876,986 (1.8 M) | 2,727,027 (2.6 M) |

(O2 -fno-inline is ~63 % smaller raw / ~37 % smaller brotli than O0 — a strong
prize once the render crash is resolved. Debug build unchanged at -O0 -g2, ~29 M.)

### Next step to actually ship -O>0

Re-run `smoke-test.mjs` against `build.sh --release --opt O2` on a QUIET box (no
other WebGPU/headless contention). If it passes, flip the `RB3_WEB_OPT_LEVEL`
default to O2 — the one-line change. If it still crashes, bisect the render path
at -O1 (the venue/vignette material upload + char-extras skinned-mesh draw is the
suspect frame) or pursue the source-true inline fix (03 §2) which lets us drop
`-fno-inline` entirely and re-evaluate whether plain `-flto=thin` (03 §3,
cross-TU-inlining-preserving) sidesteps it. Closure remains separately broken
(untouched).

## RESOLVED (2026-06-10) — O2 SHIPPED; render crash root-caused to OutfitConfig.cpp

The `-O>0` render-time GPU-process crash is **fixed and shipped**. `RB3_WEB_OPT_LEVEL`
now defaults to **O2** (`scripts/web/build.sh --release`), with one TU pinned to `-O0`.

**Refuted contention theory:** the crash reproduced **3/3 on a quiet box** (load avg
2.6) and with `--disable-gpu-watchdog` — deterministic, not box pacing. It was a genuine
`-O>0` codegen fault.

**Isolation (per-TU `-O0` binary search, ~15 web builds):** the crash is a GPU-process
death (no WebGPU validation error, no oversized texture — engine-side `[TEXDIAG]` probe
ruled out giant dims) during the `world/vignette/transition/tv11/.../ridingincab` draw of
retargeted char-extras. Bisected: engine lib is `-O0` (stable); rb3-web is `-O2`. Ruled
out in order: `-fno-strict-aliasing` (no), GPU watchdog (no), rndobj/ (no), `*_Web.cpp`
(no) → scene half (world/char/bandobj/meta/movie/ui/obj) PASS → bandobj/ → 12-file char
cluster → **`src/system/bandobj/OutfitConfig.cpp`** (single TU; `-O0` on it alone makes
smoke PASS). It miscompiles the outfit-mesh deform/AO data (`Piercing::Deform` /
`MeshAO::Apply` / `SetSkinTextures`) feeding the char-extras draw → corrupt GPU buffer.

**Fix:** `set_source_files_properties(.../OutfitConfig.cpp PROPERTIES COMPILE_OPTIONS
"-O0")` in `native/CMakeLists.txt` (one ~1.1k-line TU; negligible size cost). An
`RB3_WEB_O0_GLOB` cache var remains as an escape hatch for further bisects / root-cause.

**Validation (real brotli release build):** `smoke-test.mjs` PASS (main_hub, 83 songs,
no pageerror — past the tv11 crash point); `keyboard-to-gameplay.mjs` reaches
`game_screen` and plays (through the tv3 vignette) — no other `-O>0` crash in the
menu→gameplay path.

**Sizes (deployed release wasm, O0 baseline → O2):** raw 16.70M → **6.13M (−62%)**;
brotli q11 (wire) 2.39M → **1.51M (−37%)**; gzip 4.10M → **2.08M (−49%)**.

**Open follow-up:** root-cause the exact OutfitConfig.cpp `-O>0` codegen/UB trigger (O0 vs
O2 disasm diff of the deform path) so the per-TU `-O0` pin can be dropped. `--closure`
remains separately broken (rb3_pre.js stub-name matching). Same `-fno-inline` + (likely)
OutfitConfig-class fix should transfer to DC3 (also pinned `-O0`).
