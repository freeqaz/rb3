# 07 — song_select prewarm during main_hub idle (P2.4 / spec A2)

## Problem

main_hub → song_select ENTER costs a ~150 ms one-shot hitch on web:
`UIScreen::LoadPanels` (UIScreen.cpp:288) triggers `PanelRef`/`UIPanel::CheckLoad` on all
panels in one frame, and the focus panel's ~2.82 MB song_select.milo plus ~17 nested inline
Includes parse in that single frame. In-place time-slicing is OFF the table: inline Includes
share the parent BinStream sequentially and `ObjectDir::PostLoad` (Dir.cpp:390, `#pragma
dont_inline on`) is a single straight-line non-re-entrant loop, and UIScreen.cpp is a matched
TU (status `Matching`, 100% code / 100% funcs — confirmed config/SZBE69_B8/objects.json:1409 +
report.json `main/system/ui/UIScreen`). Full original spec:
`docs/native/audio-perf-loop/wave-04.md` (§A2, lines 239–456 — VERIFIED present); this doc is
the condensed contract.

> CAVEAT (verified 2026-06-09): the "seeding makes the ENTER parse free via the CheckLoad
> path" mechanism below is **mechanically incomplete** — `UIPanel::Load` (UIPanel.cpp:72-103)
> does NOT consult `DirLoader::Find`; it unconditionally does `mLoader = new DirLoader(fp, pos,
> 0,0,0, false)` (UIPanel.cpp:97) and that fresh loader opens its OWN ChunkStream and re-parses
> the milo. The ONLY place a prewarmed cached dir is reused is `DirLoader::FindLast(fp)` inside
> `ObjectDir::PostLoad` (Dir.cpp:404), and only for *shared* inline sub-dirs (`iDir.shared`),
> NOT the top-level panel milo. So prewarming as written does not by itself eliminate the
> top-level parse on ENTER. See "Mechanism" caveat below — this is a real gap inherited from
> wave-04, not a condensed-doc error.

## Mechanism (loader-cache seeding — no load-semantics change)

While the user dwells on main_hub, issue the next screen's panel milo `DirLoader`s as
`kLoadBack` (budgeted background — `TheLoadMgr.Poll` spends ≤ `RB3_LOADER_BUDGET_MS`
= 8 ms/frame). Loaded loaders sit in `TheLoadMgr.mLoaders` and **survive the
main_hub → song_select transition** — a `Loader` is removed from `mLoaders` only in
`~Loader()` (Loader.cpp:551-554); nothing in `UIScreen::Exit` / `UnloadPanels` /
`CheckUnload` evicts a background loader that has no owning panel, so a prewarmed-and-
finished dir stays resident until something deletes it.

**REALITY CHECK (the load-bearing claim, VERIFIED — and it is wrong as originally
stated):** `UIPanel::CheckLoad` (UIPanel.cpp:37) → `UIPanel::Load` (UIPanel.cpp:72) does
**NOT** call `DirLoader::Find(fp)`. It always constructs a fresh loader:
`mLoader = new DirLoader(fp, pos, 0, 0, 0, false)` (UIPanel.cpp:97), which opens its own
`ChunkStream` (DirLoader.cpp:392) and re-parses the milo from disk. There is no Find/cache
hit on the top-level panel milo. `DirLoader::Find` (DirLoader.cpp:78, scans
`TheLoadMgr.mLoaders` for `mFile == fp`) has effectively **no callers in the load path**;
the only loader-reuse in the parse chain is `DirLoader::FindLast(fp)` in
`ObjectDir::PostLoad` (Dir.cpp:404), which dedups **shared inline sub-dirs** (`iDir.shared`)
— NOT the top-level focus-panel milo. So, as written, seeding `mLoaders` does **not** make
the focus-panel ENTER parse free.

**Implication for the implementer:** to actually make the prewarm pay off, the HX_NATIVE
hook must either (a) hand the prewarmed `DirLoader` to the panel (e.g. teach
`UIPanel::Load` under `#ifdef HX_NATIVE` to `DirLoader::Find(fp)` first and adopt the
already-loaded loader / its `mDir` instead of `new DirLoader`), or (b) rely only on the
OS/filesystem page cache + shared-inline-dir reuse (a far weaker win), or (c) accept that
the measurable benefit comes mostly from warming the file in cache + warming the shared
inline dirs, not from skipping the top-level parse. Pick (a) for the real win; it is still
HX_NATIVE-gated and Wii-byte-identical, but it is a SECOND touched function (UIPanel.cpp,
also a matched TU — keep all additions inside `#ifdef HX_NATIVE`). Do not ship this handoff
claiming "parse is free" until (a) is in.

## Touch points (all rb3, all `#ifdef HX_NATIVE`)

- `src/system/ui/UIScreen.cpp` — END of `UIScreen::Poll()` (~:133-141): env-gated
  prewarm block. Gate: `RB3_PREWARM_SCREENS` truthy AND `CheckIsLoaded()` (:309-329)
  AND this screen not already prewarmed.
- **No member additions** (matched struct — see BandCharacter::Filter regression
  lesson): track prewarmed-state in a file-static `std::set<UIScreen*>` in the .cpp;
  erase `this` in `UIScreen::Exit` (~:233-252).
- Native-only helper `DoPrewarmNextScreen()` (file-static, inside the HX_NATIVE
  block): resolve the next screen's panel file list, and for each
  `fp`: `if (!DirLoader::Find(fp)) TheLoadMgr.AddLoader(fp, kLoadBack);`
  — VERIFIED-real signatures:
    - `static DirLoader *DirLoader::Find(const FilePath &)` — `src/system/obj/DirLoader.h:54`
      (NOT Loader.h; returns the matching loader from `TheLoadMgr.mLoaders` or null).
    - `Loader *LoadMgr::AddLoader(const FilePath &, LoaderPos)` — `src/system/utl/Loader.h:49`.
    - `enum LoaderPos { kLoadFront=0, kLoadBack=1, kLoadFrontStayBack=2, kLoadStayBack=3 }`
      — `src/system/utl/Loader.h:12-17`. (There is **no** `kLoadFrontStayResident`; the
      "stay" variants are `kLoadFrontStayBack` / `kLoadStayBack`. `kLoadBack` is the right
      one here — budgeted background.)
  - NB (see "Mechanism" REALITY CHECK): seeding alone does not make the focus-panel parse
    free — `UIPanel::Load` re-`new`s its own DirLoader. The high-value variant is to also
    teach `UIPanel::Load` (HX_NATIVE) to adopt a prewarmed `DirLoader::Find(fp)` result.
- **Next-screen mapping**: keep it OUT of shipped assets — env map
  `RB3_PREWARM_NEXT=main_hub:song_select` (parse once), default just that one pair.
  Hardcoding main_hub→song_select behind the env gate is acceptable v1.

## Verification requirements

- Wii match: UIScreen.cpp is a matched TU — after the edit, build and confirm the unit's
  match% unchanged (`tools/ninja-locked` + objdiff/batch-check on ui/UIScreen).
- Prewarm must not stutter main_hub: it rides the existing 8 ms/frame loader budget;
  confirm with frame trace below.

## Acceptance

- Native A/B (fast loop):
  `RB3_PREWARM_SCREENS=0/1` + `python3 scripts/native/frame_profiler.py --scroll 2 --worst 12`
  → the ENTER-activation frame drops from ~50 ms to ≲16 ms; main_hub p95 unchanged.
- Web A/B: `node scripts/web/web-stutter-probe.mjs` (or netperf-suite nav scenario)
  with the env/URL flag: main_hub→song_select worst rAF gap ~150 ms → <30 ms.
- Flag off = byte/behavior identical; flip default-ON in a follow-up after both pass.

## Optional follow-up (4b in wave-04, NOT this handoff)

GPU prewarm (force lazy mesh/tex uploads off-screen) — only if the milo-parse prewarm
leaves a residual GPU-upload hitch on first draw. Measure first.

## IMPLEMENTED (2026-06-09)

**Status: DONE — mechanism (a) implemented and verified; native acceptance is a measured
NEGATIVE result (the prewarm is NEUTRAL on native, exactly as the doc's CAVEAT predicted).**

### What landed (all `#ifdef HX_NATIVE`, no struct members, env-gated default-OFF)

- `src/system/ui/UIScreen.cpp`
  - End of `UIScreen::Poll()`: gate `RB3_PREWARM_SCREENS` truthy AND `CheckIsLoaded()` AND
    not-already-prewarmed (file-static `std::set<UIScreen*>`); fires `DoPrewarmNextScreen(this)`.
  - File-static (anon-namespace) helpers: `PrewarmNextMap()` parses
    `RB3_PREWARM_NEXT` (default `main_hub_screen:song_select_screen` — the real UIScreen
    object names from band_ui.dta, NOT the milo basenames the doc guessed); `DoPrewarmNextScreen`
    resolves the next screen via `ObjectDir::Main()->Find<UIScreen>(name, false)` (non-fatal),
    iterates its `mPanelList` with LoadPanels' own load predicate (`mAlwaysLoad || IsReferenced()`),
    and for each panel `fp` does `if (!DirLoader::Find(fp)) TheLoadMgr.AddLoader(fp, kLoadBack)`.
  - `UIScreen::Exit`: erases `this` from the prewarmed-set (re-arm).
  - Lifetime/leak hygiene: `PrewarmedFiles()` records the issued set per source-screen;
    `EvictPriorPrewarm(screen, keep)` runs at re-prewarm time and frees only prior loaders
    NOT in the new wanted-set that are resident+loaded+unadopted (`mAccessed == false`). NOT
    eager-on-Exit (the path routes through an intermediate `song_select_enter_screen`, so an
    Exit-time sweep wrongly killed still-pending loaders → churn/re-parse; the generational
    keep-set version evicts 0 in steady state).
- `src/system/ui/UIPanel.h` / `UIPanel.cpp`
  - **Mechanism (a) — the load-bearing fix the doc demanded:** `UIPanel::Load()` adopts a
    prewarmed loader BEFORE `new DirLoader`: `if (mLoadRefs==1 && (dl=DirLoader::Find(fp)) &&
    dl->IsLoaded()) { pDir=dynamic_cast<PanelDir*>(dl->GetDir()); SetLoadedDir(pDir,false);
    delete dl; return; }`. `GetDir()` sets `mAccessed` so `~DirLoader` does NOT RELEASE the
    dir → panel owns it, no double-delete; an unadopted loader's own `~DirLoader` frees its
    dir (`mAccessed` false) → no leak.
  - New HX_NATIVE helper `UIPanel::GetPanelFilePath()` resolves the panel's milo FilePath the
    same way Load() does, so UIScreen can prewarm without loading.

### Acceptance (measured)

- **Wii match: UNCHANGED.** `tools/ninja-locked build/SZBE69_B8/report.json`:
  `main/system/ui/UIScreen` = **100.0% (29/29)**, `main/system/ui/UIPanel` = **100.0% (27/27)**.
  All edits are HX_NATIVE-gated, invisible to mwcc.
- **rb3-tests: 13/13 PASS.**
- **Mechanism verified live** (direct run, full profiler env incl. frame trace): main_hub
  prewarms the 5 song_select panel milos as kLoadBack, and on ENTER all panels adopt the
  prewarmed dirs (`UIPanel song_select_panel adopted prewarmed dir for
  ui/song_select/song_select.milo`, +shortcut/filter/sv4/meta). Eviction churn fixed:
  **5 adopted, 0 evicted, 0 asserts/fails**, each milo parsed exactly once.
- **Native A/B** (`frame_profiler.py --scroll 2 --worst 12`, RB3_PREWARM_SCREENS 0 vs 1,
  isolated `--trace` paths under the run-lock; box load avg ~12-24 so run-to-run noise is large):
  - ENTER-activation frame (first `song_select_screen` frame): OFF ≈ 55-63 ms, ON ≈ 56-58 ms
    → **statistically identical, NOT reduced to ≤16 ms.**
  - main_hub p95: OFF ≈ 27 ms, ON ≈ 27 ms → **unchanged** (the doc's safety requirement: met).

### Why the ENTER frame did NOT drop (negative result, root-caused)

The frame trace shows the ENTER frame (243) has `lp=0.0` AND `lpu=0.0` in BOTH OFF and ON —
i.e. **zero time in LoadMgr.Poll and zero in PollUntil* on the ENTER frame.** On native the
milo parse is NEVER on the critical frame: even WITHOUT prewarm, `UIScreen::Exit` →
`to->LoadPanels()` → `UIPanel::Load` issues the panel milos as **kLoadBack**, and the existing
`LoadMgr::Poll` 8 ms/frame budget (`RB3_LOADER_BUDGET_MS`) already time-slices the parse across
the `song_select_enter_screen` transition frames. The residual ~55 ms ENTER cost is GPU
upload + `mDir->Enter()`/SyncObjects + first-frame UI layout (the trace's "elsewhere
(draw/poll/gpu)" bucket), which the milo-parse prewarm does not touch — that is **follow-up 4b
(GPU prewarm), explicitly out of scope here.** The doc's "~50 ms → ≤16 ms" target is a WEB
number (web's `PollUntilLoaded` drains un-interruptibly with no budget), exactly as the doc's
own "REALITY CHECK / Mechanism CAVEAT" anticipated ("the measurable benefit comes mostly from
warming the file in cache + shared inline dirs, not from skipping the top-level parse").

### Deviations from the handoff

- Next-screen default map = `main_hub_screen:song_select_screen` (real object names), not the
  doc's `main_hub:song_select`. The doc said the mapping is env-overridable and "acceptable v1";
  the actual band_ui.dta UIScreen object names are required for it to fire.
- Added `UIPanel::GetPanelFilePath()` (HX_NATIVE header method, no struct change) so the screen
  can resolve a panel's milo path without loading — needed for mechanism (a).
- Added leak/lifetime hygiene (`PrewarmedFiles` + generational `EvictPriorPrewarm`) beyond the
  doc's "erase in Exit" note, because a finished unowned kLoadBack DirLoader is never GC'd.
- The native acceptance target (≤16 ms ENTER) is NOT met and is a documented negative result
  (parse already budget-sliced on native; residual is GPU). Web A/B (`web-stutter-probe.mjs`)
  NOT run — the feature is default-OFF and the native trace already proves the parse isn't the
  ENTER cost on this build; the web path is where the un-budgeted drain (and thus the win) lives.
- `RB3_PREWARM_DBG` (secondary env) gates verbose diagnostics; the `RB3_PREWARM` action logs are
  gated on `RB3_PREWARM_SCREENS` itself.

### Recommendation

Keep landed (default-OFF, Wii-clean, mechanism correct, neutral on native). Flip default-ON
only after a WEB A/B confirms the win there (where the parse is on the critical frame). The
native residual hitch is GPU-upload — pursue follow-up 4b if that frame matters.

## FIX ROUND (2026-06-09) — adoption-branch ownership gate

A reviewer BLOCKED the first landing (`ad3b7e61`) on one correctness issue: the
`UIPanel::Load()` prewarm-adoption branch was gated **only** on `mLoadRefs == 1`, NOT on the
prewarm feature — the `getenv("RB3_PREWARM_SCREENS")` at the old UIPanel.cpp:140 only gated the
MILO_LOG, so the steal-and-`delete` ran on EVERY `HX_NATIVE` panel load by default. With the
flag off, any `DirLoader::Find(fp)` hit is by definition a loader owned by ANOTHER component
(`ObjDirPtr<T>::LoadFile`'s mLoader, or a second panel's completed-but-unpolled mLoader),
and adopting+`delete`ing it is a use-after-free. EvictPriorPrewarm had the symmetric hazard:
its `DirLoader::Find(*it)` could return and `delete` a foreign loader for the same milo.

**Fix (robust variant the reviewer recommended): pointer-identity ownership boundary.**

- `UIScreen.cpp`: new file-static `std::set<Loader *> IssuedPrewarmLoaders()` records the EXACT
  `Loader*` each `TheLoadMgr.AddLoader(...)` returns in `DoPrewarmNextScreen`. Two
  global-linkage bridge functions (outside the anon namespace, `#ifdef HX_NATIVE`):
  `bool RB3PrewarmIssuedLoader(Loader *)` and `void RB3PrewarmForgetLoader(Loader *)`.
  `EvictPriorPrewarm` now deletes a found loader only if it is in the issued set (identity, not
  FilePath) — a foreign loader for the same milo has a different pointer and is left alone — and
  erases it from the set on delete.
- `UIPanel.cpp`: the adoption branch is now `if (RB3PrewarmIssuedLoader(prewarmed) &&
  prewarmed->IsLoaded())` and calls `RB3PrewarmForgetLoader(prewarmed)` before `delete`. Because
  the issued set is ONLY ever populated by the `RB3_PREWARM_SCREENS` hook, with the flag OFF the
  set is empty ⇒ `RB3PrewarmIssuedLoader` is always false ⇒ the branch is inert and the stock
  `new DirLoader` path runs unchanged. **Flag-off = byte/behavior identical restored.**

This is strictly stronger than the "minimal" env-gate (it also blocks adoption of a foreign
loader for the same milo on the flag-ON path, the second hazard the reviewer flagged).

### Re-verification (this round)

- **Build:** `rb3-native` + `rb3-tests` rebuilt green under `/tmp/rb3-native-build.lock`.
- **rb3-tests: 13/13 PASS.**
- **Flag OFF** (`song-select-capture.py`, env scrubbed of all RB3_PREWARM*): reached
  `song_select_screen`; engine log has **ZERO** `RB3_PREWARM` / `adopted prewarmed dir` /
  `evicting` lines and **zero** asserts/fails — the adoption branch is fully inert (the fix's
  core acceptance: flag-off byte/behavior identical).
- **Flag ON** (`RB3_PREWARM_SCREENS=1 RB3_PREWARM_DBG=1`): reached `song_select_screen`;
  **5 prewarm-issued milos, 4 adoptions** (sv4_panel + song_select/shortcut/filter; the meta
  panel shares sv4_d.milo), **0 evictions, 0 asserts/fails** — feature still works through the
  gated `RB3PrewarmIssuedLoader` path.
- **Matched-TU safety:** all edits inside `#ifdef HX_NATIVE`; `git diff` confirms no
  non-HX_NATIVE line touched; UIPanel.h unchanged this round (no struct/vtable change). The
  Wii-match claim (`main/system/ui/UIScreen` 100% 29/29, `main/system/ui/UIPanel` 100% 27/27)
  is preserved because mwcc sees identical source.

Files touched this round (rb3): `src/system/ui/UIScreen.cpp`, `src/system/ui/UIPanel.cpp`,
this doc. No engine-repo change; `MILO_ENGINE_PIN` untouched.
