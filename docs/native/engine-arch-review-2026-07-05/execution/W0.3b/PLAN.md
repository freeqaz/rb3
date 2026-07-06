# W0.3b — Frozen-sim-clock seam + stable draw-log integration golden

**Wave:** 2 · **Phase:** 0 (regression net, ADDITIVE — completes Wave-1 W0.3) · **Planner:** Opus
**Lane refs:** `REFACTOR_PLAN.md` §Phase-1 W0.3; `06-arch-crosscut.md` §4.3 rec 2 (the
"no golden-image / per-draw-state test exists" finding) and §3 (the NativeCompat flag
registry); `01-renderer-core.md` §2b–2f (the state-leak / co-location / bind-group-collapse
classes the golden must catch). **Parent:** `execution/W0.3/PLAN.md` + `execution/W0.3/STATUS.md`
(READ the STATUS `VERIFY` section — it precisely characterizes the blocker this item closes).
**Unblocks:** W1.6.

## Objective

Wave-1 **W0.3** delivered the per-draw state-log ring, the JSON dump, the `/api/drawlog`
endpoint, the C++ comparator + fail-red gtests (`native/tests/drawlog_compare.h`,
`test_draw_log_golden.cpp`), and the Python comparator (`scripts/native/drawlog-golden.py`).
All of that is **proven correct** (S2's fail-red audit + S3's `--fail-red-audit` both go red on
a genuine co-location / translation divergence). The **one** unmet exit criterion (W0.3 exit
#6, "integration golden diffs green") is blocked by a single root cause, independently
reproduced in W0.3's VERIFY section:

> A fresh headless boot's sim clock is **wall-clock / live-audio driven**, so the splash scene's
> animation + cooperative asset streaming land at different sim states across runs. The draw
> **count** alone jitters 885–890 between two consecutive boots of the *same* scene — so the
> comparator fails on the EXACT `count` gate before it ever reaches the world/bind-group
> tolerance checks. This is NOT a comparator bug; it is boot non-determinism.

W0.3's STATUS already names the fix: a **trace-free frozen/fixed sim clock for headless boots**,
extending the existing `RB3ReplayFixedClock()` / `RB3_REPLAY_FIXED_CLOCK` machinery
(`native/src/rb3_replay.{h,cpp}`) — which today only engages when a recorded input trace is
loaded (`RB3ReplayActive()`), so a plain boot cannot use it. This item builds that seam, proves
determinism, and re-captures + wires the golden into a runnable gate.

### Faithful-reference citations (the exact seams to extend — re-grep, line numbers may shift)

- **The clock machinery to extend:** `native/src/rb3_replay.{h,cpp}`.
  - `RB3ReplayFixedClock()` (cpp ~line 486): parses `RB3_REPLAY_FIXED_CLOCK` once. Returns true
    on any non-empty non-`"0"` value. Independent of `RB3ReplayInit`.
  - `RB3ReplayActive()` (cpp ~line 456): true iff a trace was loaded. **This is the gate that
    excludes a plain boot today.**
  - `RB3ReplayDtForFrame(int)` (cpp ~line 545): the recorded per-frame sim dt; returns `0.0f`
    when there is no clock table (i.e. no trace) — so it cannot drive a trace-free boot as-is.
- **Clock SEAM 1 (menu/UI sim clock — the one that matters for splash):**
  `src/system/obj/Task.cpp` `TaskMgr::Poll()` (~line 386) and its gate helper
  `RB3TaskReplayFixedClock()` (~line 41) = `RB3ReplayFixedClock() && RB3ReplayActive()`.
  When true it advances `kTaskSeconds`/`kTaskBeats` by `RB3ReplayDtForFrame(gRB3TraceFrame)`
  via a file-static accumulator (`sReplaySeconds`, `sReplayLastFrame`) instead of the
  wall-clock `mTime.Split()` / `Timer::CyclesToMs` path. **This is the primary seam** — UI /
  splash animation is driven off `kTaskSeconds` (PropAnims, task timelines).
- **Clock SEAM 2 (in-song song-ms):** `src/band3/game/Game.cpp` (~line 1718). **Out of scope
  for W0.3b** — verified song-only (`if (TheGamePanel->unk150)`); the splash / boot capture has
  no song clock. Leave untouched.
- **The frame index the seam keys on:** `gRB3TraceFrame`, set by `RB3TraceSetFrame(frame)` in
  `App::RunOneFrame` (`src/App.cpp` ~line 551) — but **only inside `if (gRB3TraceActive)`**, so
  on a plain boot with tracing off it stays `0`. The seam's once-per-frame accumulation needs a
  frame index that advances every frame regardless of tracing. The clean, always-advancing
  source already exists: the loop counter `frame` passed to `RunOneFrame(frame)` from
  `App::RunWithoutDebugging` (`src/App.cpp` ~line 869).
- **Second non-determinism source (the loader):** `src/system/utl/Loader.cpp` uses **wall-clock**
  time-slicing (`mTimer`, `Timer::CyclesToMs`, the `RB3_LOADER_BUDGET_MS` budget, `mPeriod`
  10ms) to bound cooperative streaming per frame. At a *fixed absolute frame index*, the set of
  resident meshes therefore still varies run-to-run (how much loads per frame depends on
  wall-clock, not frame count). A frozen animation clock is **necessary but not sufficient**;
  S2 must also make per-frame loading frame-deterministic under the flag (or settle to a
  fully-drained steady state that is reproducible). W0.3/S3 empirically confirmed loader jitter
  persists across 30/400/700-frame settle windows and with `RB3_ASYNC_OPEN_OFF=1`, so "just wait
  longer" does **not** converge — the loader path must be made deterministic under the flag.
- **The flag registry:** `milo-native-engine/src/platform/NativeCompatFlags.classification.json`
  (hand-authored sidecar) → regen via `scripts/analysis/native_compat_census.py gen` →
  `NativeCompatFlags.gen.inc` + `docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md`.
  The census `scan`/`check` roots are `milo-native-engine/src` **and** `rb3/native/src` (NOT
  `rb3/src/system`) — so the new flag's `getenv` MUST live in `native/src/rb3_replay.cpp` (it
  already does, per the brief) for the census to see it and require its registration.

### Chosen design (the seam)

A new **presence-mode** env flag `RB3_FIXED_CLOCK` (free — grep-confirmed unused), class
`feature`, read once in `rb3_replay.cpp`, exposed via two new accessors:

- `bool RB3FixedClockActive()` — true iff `RB3_FIXED_CLOCK` is set (non-empty, non-`"0"`),
  parsed once (mirrors `RB3ReplayFixedClock()`'s cached-int idiom). **Independent of any trace.**
- `float RB3FixedClockDt()` — the constant per-frame sim dt in **seconds**. Default `1.0f/60.0f`
  (a real fixed timestep so animation *progresses* deterministically to a well-defined
  frame-`N` state); overridable via `RB3_FIXED_CLOCK_DT_MS` (a value knob) for tuning, and
  `0.0` is permitted (a true freeze — animation held at t=0) as a fallback lever if a nonzero
  timestep proves harder to stabilise than a frozen one.

Seam 1's gate widens from `RB3ReplayFixedClock() && RB3ReplayActive()` to
**`(RB3ReplayFixedClock() && RB3ReplayActive()) || RB3FixedClockActive()`**, and its dt source
becomes: recorded (`RB3ReplayDtForFrame`) when a trace is active, else the constant
`RB3FixedClockDt()`. The frame index the once-per-frame guard keys on is made to advance every
frame (set `RB3TraceSetFrame(frame)` whenever `gRB3TraceActive || RB3FixedClockActive()`).

**Inertness (flag off):** every new branch is taken only when `RB3FixedClockActive()` is true;
with the flag unset the seam is byte-identical to the pre-W0.3b code (the original wall-clock
`mTime.Split()` path). This is Phase-0-additive, exactly like W0.3 itself — NOT a Phase-1 MOVE.

**Determinism harness (how the golden is captured deterministically):** a *bounded, non-HTTP*
boot pins the absolute frame index —
`MILO_MAX_FRAMES=N RB3_FIXED_CLOCK=1 RB3_DRAWLOG=1 RB3_DRAWLOG_DUMP=<file> rb3-native` — so both
the sim time (`= N·dt`, fixed) AND the capture frame (`N-1`, fixed) are reproducible. The
W0.3/S3 HTTP "wait-for-scene + settle" path is inherently non-deterministic in its capture
*frame index* and is therefore NOT the gate path; it stays as a diagnostic tool.

---

## Subtasks

### W0.3b.S1 — Trace-free fixed-clock primitive + flag registration
- **id:** W0.3b.S1
- **model:** opus (API/semantics design; must not perturb the existing replay-fixed-clock path)
- **goal:** Add `RB3FixedClockActive()` / `RB3FixedClockDt()` to the replay machinery and register
  `RB3_FIXED_CLOCK` (+ `RB3_FIXED_CLOCK_DT_MS`) in the NativeCompat registry. **Additive + inert:**
  no seam is wired yet (accessors unused by production code), so this commit changes nothing
  rendered. Provides a unit-testable primitive S2 builds on.
- **exact files:**
  - `native/src/rb3_replay.h` — declare `bool RB3FixedClockActive();` and `float RB3FixedClockDt();`
    under the existing `#ifdef HX_NATIVE` block, with a doc-comment header matching the file's
    style (state the trace-free semantics + default dt + the `RB3_FIXED_CLOCK_DT_MS` override).
  - `native/src/rb3_replay.cpp` — implement both. Cache a `static int` for the presence check
    (mirror `RB3ReplayFixedClock()`'s `gReplay.fixedClock` idiom, but a NEW independent cached
    field or a new file-static — do NOT reuse `gReplay.fixedClock`, semantics differ) and a
    `static float` for the dt (default `1.0f/60.0f`; if `RB3_FIXED_CLOCK_DT_MS` parses to a
    finite `>= 0` value, use `ms/1000.0f`). Presence rule EXACTLY matches `RB3ReplayFixedClock`
    (`v && *v && strcmp(v,"0")!=0`). Under `__EMSCRIPTEN__` mirror the existing `EM_ASM_INT`
    web-global pattern (`window.__rb3FixedClock`) so the seam compiles for web (browser-run
    deferred, same as replay).
  - `milo-native-engine/src/platform/NativeCompatFlags.classification.json` — add two entries:
    `"RB3_FIXED_CLOCK": { "class": "feature", "owner": "session-telemetry/determinism",
    "faithfulStatus": "n/a: headless-determinism harness flag (off in shipping runs)",
    "default": "off" }` and `"RB3_FIXED_CLOCK_DT_MS": { "class": "feature", ... "default": "off" }`
    (value knob; `off` = absent ⇒ built-in default dt). Keep alphabetical/grouped placement
    consistent with the surrounding entries.
  - `milo-native-engine/src/platform/NativeCompatFlags.gen.inc` (REGENERATED, do not hand-edit)
  - `docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md` (REGENERATED)
- **step-by-step:**
  1. Add the two accessors to `.h`/`.cpp` (see files above). Keep them tiny + branch-predictable.
  2. Add the sidecar entries. Then regenerate:
     `python3 scripts/analysis/native_compat_census.py --selftest` (must pass first), then
     `python3 scripts/analysis/native_compat_census.py gen`, then
     `python3 scripts/analysis/native_compat_census.py check` (must exit 0 — proves registry ⊇
     every getenv incl. the two new ones, and the committed gen.inc/ledger are regen-clean).
  3. Build `rb3-native` in your OWN build dir to confirm it compiles (accessors unused → no
     behavior change).
- **verification commands:**
  - `python3 scripts/analysis/native_compat_census.py --selftest` → all pass
  - `python3 scripts/analysis/native_compat_census.py check` → **exit 0** (registry complete +
    regen-clean). Paste the exit status into STATUS.
  - `cmake --build native/build-agent-W0.3b --target rb3-native -j8` → `Built target rb3-native`.
  - Quick runtime smoke: `RB3_FIXED_CLOCK=1 MILO_MAX_FRAMES=3 rb3-native` boots without new
    output changes vs flag-off (accessors not yet consumed — expected).
- **byte-identical / inertness evidence (REQUIRED):** this commit adds only new *unused* symbols
  + sidecar/gen/ledger doc rows. Record in STATUS that `git show --stat` on the commit(s) shows
  the `.cpp`/`.h` diff is pure insertion of two new functions (no edit to any existing function
  body), and the engine-repo commit touches only the sidecar + regenerated gen.inc/ledger. No
  rendered path is reachable ⇒ inert by construction; no screenshot compare needed for S1.
- **repo/commit split:** rb3 repo commit (`.h`/`.cpp` + ledger regen under rb3 docs) prefixed
  `W0.3b:`; engine repo commit (sidecar + gen.inc) prefixed `W0.3b:`. **Do NOT bump
  `MILO_ENGINE_PIN`** (Hard Rule 3) — the soft pin builds from the engine working tree; a HEAD≠pin
  WARNING at configure is expected and correct (coordinator bumps per wave). Note the engine SHA
  in STATUS.

### W0.3b.S2 — Wire the trace-free seam + prove two boots are byte-identical
- **id:** W0.3b.S2
- **model:** opus (correctness-critical: clock seam + a second non-determinism source (loader);
  must be provably behavior-preserving when the flag is off, and provably deterministic when on)
- **goal:** Engage the frozen/fixed clock on a plain boot (no trace) via SEAM 1 + an
  always-advancing frame index, make per-frame loading frame-deterministic under the flag, and
  PROVE two consecutive fresh bounded boots under `RB3_FIXED_CLOCK` produce IDENTICAL draw-log
  captures (count EXACT + tolerance-clean) at a fixed absolute frame.
- **exact files:**
  - `src/system/obj/Task.cpp` — widen `RB3TaskReplayFixedClock()` (the gate helper, ~line 41) so
    the seam engages on `RB3FixedClockActive()` too, and add the trace-free dt branch inside
    `TaskMgr::Poll()`'s SEAM-1 block (~line 386): dt = `RB3ReplayDtForFrame(gRB3TraceFrame)` when
    `RB3ReplayActive()`, else `RB3FixedClockDt()`. Keep the file-static accumulator + once-per-
    frame `gRB3TraceFrame` guard unchanged in shape. `#include "rb3_replay.h"` already present.
  - `src/App.cpp` — in `RunOneFrame` (~line 551), advance the frame index whenever
    `gRB3TraceActive || RB3FixedClockActive()` (so the seam's once-per-frame guard works without a
    trace). Add `#include "rb3_replay.h"` guard usage if not already reachable (App.cpp already
    includes it, ~line 88). Purely additive; the `gRB3TraceActive` fast-path stays.
  - `src/system/utl/Loader.cpp` — under `RB3FixedClockActive()`, make the cooperative loader
    frame-deterministic: prefer draining the front loader to empty each frame (the existing
    `PollUntilEmpty` / unbudgeted `period = 1e30` path) OR replace the wall-clock budget with a
    fixed op-count budget — whichever yields a reproducible resident set at frame `N`. This is the
    engine-behavior half; keep it behind the flag so the shipping loader budget is untouched.
    (If root-causing shows the residual jitter is NOT loader-side but a specific engine
    animation/movie poll reading wall-clock, the corresponding seam goes in the **engine repo**
    `milo-native-engine/src` — see the "engine-side" note in the brief — and is registered/pinned
    the same way as S1. Determine empirically; do not add engine seams speculatively.)
- **step-by-step:**
  1. Wire SEAM 1's trace-free path + the frame-index advance. Build `rb3-native`.
  2. **Determinism loop (the core work):** run the bounded harness twice —
     `for i in 1 2; do MILO_MAX_FRAMES=N RB3_FIXED_CLOCK=1 RB3_DRAWLOG=1 \
       RB3_DRAWLOG_DUMP=/tmp/dl_$i.json MILO_HEADLESS=1 native/build-agent-W0.3b/rb3-native; done`
     picking `N` large enough that the target scene (splash, per W0.3/S3, or the earliest
     stable fully-loaded scene) is reached + settled. Compare the two dumps with the S2 Python
     comparator (`scripts/native/drawlog-golden.py`'s compare, or a direct diff). Iterate:
     - If **animation** state differs → confirm SEAM 1 actually drives it (log `kTaskSeconds` at
       capture; both runs must read identical seconds = `N·dt`).
     - If **count / resident-mesh set** differs → the loader budget is the culprit; apply the
       deterministic-drain change and re-run.
     - Escalating lever: `RB3_FIXED_CLOCK_DT_MS=0` (true freeze) to isolate animation vs load
       jitter; and stack the existing `RB3_GAMEWARM_OFF/RB3_TEX_PREWARM_OFF/RB3_ASYNC_OPEN_OFF`
       stabilizers only if still needed (document any that remain load-bearing).
  3. Repeat until **two consecutive runs are count-EXACT and tolerance-clean**. Record the exact
     `N`, dt, scene, and any auxiliary flags that ended up load-bearing.
- **verification commands:**
  - Determinism proof (the exit bar): two bounded fixed-clock boots →
    `python3 scripts/native/drawlog-golden.py` compare (or the S2 C++/py comparator) reports
    `passed=true` / zero divergences, with **identical `count`**. Run it **3×** (three
    independent boot-pairs) to rule out a lucky pair; paste all counts into STATUS.
  - Off-path unchanged: with `RB3_FIXED_CLOCK` UNSET, a bounded boot's behavior is unchanged —
    the seam branch is skipped. Because the boot is non-deterministic when off (the very bug),
    prove non-regression **by construction**: `git show` the diff shows every new line is inside
    an `if (RB3FixedClockActive() ...)` / widened-gate branch; the original wall-clock path is
    byte-identical. State this explicitly (this is the Phase-0-additive analogue of the MOVE
    byte-identical rule; a pixel compare on the OFF path is not meaningful here and W0.3 already
    documented why).
  - `native/build-agent-W0.3b/rb3-tests --gtest_filter='DrawLogGolden*'` still green (no test
    touched by S2, but confirm nothing regressed).
- **byte-identical / determinism evidence (REQUIRED):** in STATUS record (a) the by-construction
  OFF-path argument with the `git show` line references, and (b) the ON-path determinism proof:
  the three boot-pair counts (all equal within each pair) + the comparator's zero-divergence
  output. This is the evidence that closes W0.3 exit #6.
- **repo/commit split:** rb3 repo commit(s) prefixed `W0.3b:` for Task.cpp/App.cpp/Loader.cpp.
  If an engine-side seam proved necessary, a separate `milo-native-engine` commit prefixed
  `W0.3b:` (register any additional flag in the sidecar + regen, still no pin bump). Stage ONLY
  your own lines (Hard Rule 8) — Task.cpp / App.cpp / Loader.cpp have concurrent owners; never
  `git add -A`.

### W0.3b.S3 — Re-capture the golden + wire drawlog-golden.py into a runnable gate + STATUS green
- **id:** W0.3b.S3
- **model:** sonnet (mechanical: capture, commit artifact, add a gate mode, update docs — the
  hard design/correctness is settled in S1/S2)
- **goal:** Capture + commit the stable golden under the frozen clock, add a deterministic
  `--fixed-clock` gate mode to `drawlog-golden.py` (bounded boot → read dump → compare), turn it
  into a runnable regression gate, and flip W0.3's STATUS exit #6 to green.
- **exact files:**
  - `native/tests/goldens/drawlog/<scene>.json` — RE-CAPTURED under `RB3_FIXED_CLOCK` at the
    `N`/dt/scene S2 fixed. (Overwrite the existing `splash_screen.json` if S2 kept that scene, or
    add the new scene's golden + retire the stale one — follow S2's chosen scene.)
  - `scripts/native/drawlog-golden.py` — add a `--fixed-clock` capture mode: instead of
    HTTP wait-for-scene + settle, boot bounded non-HTTP
    (`MILO_MAX_FRAMES=N RB3_FIXED_CLOCK=1 RB3_DRAWLOG=1 RB3_DRAWLOG_DUMP=<tmp>`), then read `<tmp>`
    and compare against the committed golden with the EXISTING Python comparator (unchanged
    tolerances). `--update` in this mode writes the golden from the bounded capture. Update the
    module docstring: the routine default-diff caveat ("diagnostic, not a gate") is REPLACED for
    `--fixed-clock` mode — that mode IS a reliable unattended gate. Keep `--fail-red-audit` and
    the HTTP diagnostic path working.
  - `docs/native/engine-arch-review-2026-07-05/execution/W0.3/STATUS.md` — append a
    `## W0.3b.S3 — W0.3 exit #6 now green` note (do NOT rewrite prior entries; append under the
    docs flock) pointing at the frozen-clock harness + the new golden + the runnable gate.
  - `docs/native/engine-arch-review-2026-07-05/execution/W0.3b/STATUS.md` — S1/S2/S3 entries.
- **step-by-step:**
  1. Implement `--fixed-clock` mode in the script (reuse the comparator + tolerance constants
     verbatim; only the capture front-end changes to the bounded-boot + dump-file read).
  2. `python3 scripts/native/drawlog-golden.py --fixed-clock --update` → writes the golden.
  3. Verify the gate: `python3 scripts/native/drawlog-golden.py --fixed-clock` (fresh boot) →
     **exit 0, count match, zero divergences.** Run it 2–3× to confirm it stays green unattended.
  4. `python3 scripts/native/drawlog-golden.py --fixed-clock --fail-red-audit` → still goes red
     on a perturbed golden (proves the gate can fail); golden byte-identical on disk after
     (`git diff --stat` empty).
  5. Update both STATUS files.
- **verification commands:**
  - `python3 scripts/native/drawlog-golden.py --fixed-clock` × 3 fresh boots → all exit 0, all
    count-match. Paste the three results into STATUS.
  - `python3 scripts/native/drawlog-golden.py --fixed-clock --fail-red-audit` → red as designed,
    golden untouched.
  - `native/build-agent-W0.3b/rb3-tests --gtest_filter='DrawLogGolden*'` → unchanged (still
    9 pass / 1 skip or better).
- **evidence (REQUIRED):** STATUS records the committed golden's capture command (exact `N`, dt,
  scene, flags), the three green gate runs, and the fail-red audit — closing the loop that W0.3
  exit #6 left open.
- **repo/commit split:** single rb3 repo commit prefixed `W0.3b:` (script + golden + STATUS).
  Stage only your files.

---

## Exit criteria (measurable)

1. `RB3_FIXED_CLOCK` (+ `RB3_FIXED_CLOCK_DT_MS`) are registered in
   `NativeCompatFlags.classification.json` with `class: feature`, gen.inc + ledger regenerated,
   and `native_compat_census.py check` exits **0** (registry complete + regen-clean).
2. With `RB3_FIXED_CLOCK` **unset**, the seam is byte-identical to pre-W0.3b behavior — proven
   by construction (every new line is inside a flag-gated branch; the original wall-clock path
   unedited), documented with `git show` line references in STATUS.
3. Two (verified across **three** independent boot-pairs) consecutive fresh bounded headless
   boots under `RB3_FIXED_CLOCK` produce **count-EXACT + tolerance-clean-identical** draw-log
   dumps at the chosen fixed frame `N`.
4. A committed golden (`native/tests/goldens/drawlog/<scene>.json`) captured under the frozen
   clock; `scripts/native/drawlog-golden.py --fixed-clock` diffs **green (exit 0)** against it on
   a fresh boot, repeatably (≥3 runs), and `--fixed-clock --fail-red-audit` still goes red with
   the golden untouched on disk.
5. W0.3's STATUS exit criterion #6 is flipped to green (appended note referencing this item), and
   W0.3b's STATUS carries S1/S2/S3 done entries with SHAs + the evidence above. **W1.6 unblocked.**
6. `rb3-tests --gtest_filter='DrawLogGolden*'` remains green throughout (no W0.3 regression).
7. `MILO_ENGINE_PIN` is unchanged (Hard Rule 3); engine changes are committed in the engine repo.

## Risks / conflicts

- **Loader determinism is the real risk, not the clock.** W0.3/S3 proved settle-window length +
  `ASYNC_OPEN_OFF` do NOT converge the count → a frozen animation clock alone will likely still
  jitter because `Loader.cpp` time-slices on wall-clock. S2 MUST address the loader (deterministic
  drain / fixed op-budget under the flag) or the golden will keep jittering. Budget the bulk of
  S2's effort here. The `dt=0` true-freeze lever isolates animation-vs-load jitter during
  diagnosis.
- **Frame-index correctness.** The seam's once-per-frame accumulation assumes the frame index
  advances exactly once per `RunOneFrame`. Confirm no code path calls `TaskMgr::Poll()` twice per
  frame under the fixed clock (it would double-advance the sim time). The existing guard
  (`gRB3TraceFrame != sReplayLastFrame`) handles this iff the index is set once per frame — verify.
- **Concurrent-owner files.** `Task.cpp`, `App.cpp`, `Loader.cpp`, `rb3_replay.cpp`, and the
  engine sidecar all have concurrent owners across waves. Stage ONLY your own added lines
  (Hard Rule 8); never `git add -A` / `git commit -a`; flock the appropriate git lock
  (`/tmp/rb3-git.lock` for rb3, `/tmp/milo-engine-git.lock` for engine, `/tmp/rb3-docs.lock` for
  STATUS). NEVER `git reset/rebase/checkout--/restore` on shared trees (Hard Rule 7).
- **This wave's Lane A** (W1.2→W1.3→W1.4→W1.5→W1.7→W1.6) runs sequentially on
  `Rnd_Wgpu_RB3.cpp`. W0.3b does **not** touch `Rnd_Wgpu_RB3.cpp` (the draw-log ring landed in
  W0.3; W0.3b only touches the clock/loader/flag/script/golden), so there is no file overlap with
  Lane A. W0.3b runs in parallel with W2-TESTFIX (also no overlap). Assume prior Lane-A commits
  are already in; re-grep before editing (line numbers in this plan are approximate).
- **Engine soft-pin WARNING is expected.** After the S1 engine commit, `milo-native-engine` HEAD
  ≠ `MILO_ENGINE_PIN` → a configure-time WARNING. This is correct (do NOT bump the pin to silence
  it); the working-tree build picks up the new sidecar/gen.inc regardless.
- **Web path** compiles but is browser-run-deferred (mirror the replay-fixed-clock `EM_ASM_INT`
  pattern so `rb3-web` still links; no browser verification required in W0.3b).
