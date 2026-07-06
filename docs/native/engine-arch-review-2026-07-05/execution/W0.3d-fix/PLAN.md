# W0.3d-fix — Land the async-loader/object-list draw-order determinism patch

**Planner:** Opus. **Date:** 2026-07-06. **Lane:** A, item 1 (rb3-only). **Commit prefix:** `W0.3d-fix:`.
**Parent:** `execution/WAVE5_KICKOFF.md` (COORDINATOR ACCEPTANCE amendments A4/B1 are binding) +
`WAVE5_REVIEW.md` (Fable, file:line evidence) + `README.md` (hard rules 1–8).
**Engine pin:** `609efb7` — do NOT bump. This item is **rb3-only**; it touches **no engine file**.

## Objective

Land the staged, verifier-proven W0.3d part-(b) patch that makes native draw-submission ORDER
deterministic, behind a **registered opt-out `RB3_DRAWSORT_DETERMINISTIC_OFF`** for a landed
fail-red. This must land **before** the W2.1 flip so the coordinator's single post-flip re-golden
captures a stable exact-order baseline (Fable A4: `W0.3d-fix → flip → ONE re-golden`).

### Root cause (authoritative, from W0.3d STATUS — do NOT re-diagnose)

`std::sort(mDraws.begin(), mDraws.end(), SortDraws)` (rb3 `src/system/rndobj/Dir.cpp:104`,
`Group.cpp:166`, `BandCrowdMeter.cpp:153`; `Rnd.cpp:1130` uses `list.sort`) is **not stable**, and
`SortDraws` (`src/system/rndobj/Utl.cpp:162-179`) breaks a same-`GetOrder()`/different-material tie
with a **raw pointer compare `mat1 < mat2`** (`:175`). On native the `ThreadCall` worker pthread
parses milo DTA concurrently with the main thread, so material heap addresses — and thus this
compare — vary run-to-run under scheduler load (glibc per-thread arenas; NOT tamed by `setarch -R`).
That is mechanism-2 of the W0.3b/c flake: byte-identical binaries disagree on draw ORDER run-to-run
(~33% flake in the W0.3b 15-run sweep; up to 354-draw divergence). The draw *multiset* is invariant
(same 888 draws) — only the order permutes. Fix = replace the pointer tie-break with a total order
by **material NAME** (falling through to the existing unique per-dir `strcmp(draw1->Name(),…)` at
`:177` when material names tie), so `std::sort`'s output is independent of both input order and heap
address.

### Design ruling — Design (1): harness-scoped tie-break + opt-out (RECOMMENDED, MINIMAL)

The staged `W0.3d-fix.patch` (verified: 30/30 identical order under `nproc` contention; 25/0
additions-only; `--fixed-clock --canonical-order` PASS 3/3 against the committed golden; `git apply
--check` clean against the live file today) already gates the name tie-break on
`RB3FixedClockActive()` (so it is **inert unless `RB3_FIXED_CLOCK` is set** → every non-harness
native run and the entire Wii/MWCC build stay byte-identical). Per Fable **B1** ("adopt the staged
patch's own recommendation: registered opt-out for a landed fail-red") the ONLY delta from the
staged patch is to AND-in a new opt-out:

```
if (RB3FixedClockActive())                       →   if (RB3FixedClockActive() && !RB3DrawSortDeterministicOff())
```

This is the lowest-risk path and it fully satisfies the lane's purpose: the golden and the gate
always run under `--fixed-clock` (which sets `RB3_FIXED_CLOCK=1`), so determinism engages exactly
where the re-golden needs it, while **flag-OFF (no `RB3_FIXED_CLOCK`) is byte-identical to today**
(satisfies the standing "default-OFF behavior changes" hard rule cleanly — zero blast radius outside
the harness). The registered opt-out `RB3_DRAWSORT_DETERMINISTIC_OFF` (default-ON) provides the
landed fail-red: under `--fixed-clock`, setting it restores the `mat1 < mat2` pointer path → order
flake returns.

**Design (2) — native-wide determinism (drop the `RB3FixedClockActive()` gate, tie-break always on
except opt-out) is explicitly NOT this item's scope.** It would additionally remove the SYS-3
render-order instability in normal play (Wave-2 strategic note) but changes the default native draw
order everywhere and breaks the committed exact-order splash golden for all runs, not just harness
ones. Flagged in Risks for the coordinator to elect in a later item; do NOT implement it here.

## Faithful citations (re-grepped 2026-07-06 on `master` — cited-line-numbers verified)

- `src/system/rndobj/Utl.cpp:162-179` — `SortDraws`. Pre-existing HX_NATIVE null-guard `:163-167`;
  `GetOrder()` compare `:169-170`; **the target pointer tie-break `return mat1 < mat2;` at `:175`**;
  existing unique per-dir name tie-break `:177`.
- `src/system/rndobj/Utl.cpp:1-6` — include block; the staged patch adds `#include "rb3_replay.h"`
  under the existing `#ifdef HX_NATIVE` at `:2-3`.
- Sort callers (why non-stable `std::sort` + a tie makes order input-order-dependent):
  `Dir.cpp:104`, `Group.cpp:166`, `BandCrowdMeter.cpp:153` (`std::sort`), `Rnd.cpp:1130`
  (`std::list::sort`). All consume `::SortDraws`.
- `native/src/rb3_replay.cpp:516-536` — `RB3FixedClockActive()` parse-once idiom (env
  `RB3_FIXED_CLOCK`, web arm via `window.__rb3FixedClock`); `:512` the cached `int gFixedClockActive`.
  **Template for the new `RB3DrawSortDeterministicOff()` helper.**
- `native/src/rb3_replay.h:79` — `bool RB3FixedClockActive();` decl; add the new decl adjacent.
- Staged patch: `docs/native/engine-arch-review-2026-07-05/execution/W0.3d/W0.3d-fix.patch`
  (hunk `@@ -172,6 +173,30 @@`, additions-only 25/0). Do not `git apply` it verbatim — it lacks the
  opt-out; hand-apply the adapted form (Design 1) in S1.
- Gate tool: `scripts/native/drawlog-golden.py` — `--fixed-clock --canonical-order`
  (multiset comparator, `compare_canonical()`), `--determinism-check N`, `--fail-red-audit`.
  Golden `native/tests/goldens/drawlog/splash_screen.json`; residual sidecar
  `splash_screen.fixedclock-residual.json` (per-name-eps, W0.3d.S1).
- Classification: `milo-native-engine/src/platform/NativeCompatFlags.classification.json`
  (schema at `:3`; `_OFF` opt-out exemplars `:42-66` all `"default": "on"`).
- `scripts/analysis/native_compat_census.py` — `check` / `--selftest` (informational here; the
  wave-end `gen.inc` regen is the coordinator's, NOT ours).

## Subtasks

### W0.3d-fix.S1 — Add the opt-out helper + apply the adapted SortDraws tie-break
- **model:** opus
- **goal:** Land the deterministic material-name tie-break in `SortDraws`, gated on
  `RB3FixedClockActive() && !RB3DrawSortDeterministicOff()`, plus the new parse-once opt-out helper.
  One CHANGE commit (behavior change behind flag; MOVE-xor-CHANGE respected).
- **files (exact):**
  - `native/src/rb3_replay.h` — add `bool RB3DrawSortDeterministicOff();` decl next to `:79`.
  - `native/src/rb3_replay.cpp` — add the parse-once helper (env `RB3_DRAWSORT_DETERMINISTIC_OFF`,
    cached `int`, dual-arm: `std::getenv` on native / `EM_ASM_INT` `window.__rb3DrawSortDeterministicOff`
    on `__EMSCRIPTEN__`), mirroring `RB3FixedClockActive()` at `:516`. Default OFF (deterministic ON).
  - `src/system/rndobj/Utl.cpp` — add `#include "rb3_replay.h"` under the `#ifdef HX_NATIVE` at `:2`;
    insert the tie-break block inside `if (mat1 != mat2) {` (`:174`) exactly as the staged patch but
    with the condition `if (RB3FixedClockActive() && !RB3DrawSortDeterministicOff())`.
- **steps:**
  1. Add helper decl + def (copy the `RB3FixedClockActive` structure; new namespace-local cache int).
  2. Hand-apply the adapted Utl.cpp hunk (do NOT `git apply` the staged patch verbatim — it omits
     the opt-out). Keep the `mat1 ? mat1->Name() : ""` null-safety, the material-name compare, and
     the fall-through to `strcmp(draw1->Name(), draw2->Name())`. The `#else`/Wii path stays
     `return mat1 < mat2;`.
  3. Build in your OWN dir.
- **verify cmds:**
  - `cmake -B /home/free/code/milohax/rb3/native/build-agent-W0.3d-fix -S /home/free/code/milohax/rb3/native -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++`
  - `cmake --build /home/free/code/milohax/rb3/native/build-agent-W0.3d-fix --target rb3-native rb3-tests -j"$(nproc)"`
  - `git diff --numstat src/system/rndobj/Utl.cpp` → additions-only (0 deletions on the existing
    lines; the `mat1 < mat2` line stays). All new lines under `#ifdef HX_NATIVE` → Wii byte-identical.
- **staging/commit:** `flock /tmp/rb3-git.lock`; stage ONLY the three files above; commit
  `W0.3d-fix: deterministic SortDraws material-name tie-break behind RB3_DRAWSORT_DETERMINISTIC_OFF`.
  Never `-A`/`-a`. Do NOT touch `src/App.cpp`, engine files, or sibling lanes' files.

### W0.3d-fix.S2 — Register the opt-out in classification.json (append-only, NO regen)
- **model:** sonnet
- **goal:** Add one APPEND-ONLY row for `RB3_DRAWSORT_DETERMINISTIC_OFF` so the census recognizes it;
  do NOT run `gen.inc` regen (coordinator regens ONCE at wave end — shared with the flip + W3.1a +
  W0.6b; W2.6's live-clobber failure mode).
- **files (exact):** `milo-native-engine/src/platform/NativeCompatFlags.classification.json`.
- **steps:**
  1. Under `flock /tmp/milo-engine-classjson.lock`, append (append-only — do not reorder/rewrite
     existing rows) a row keyed `RB3_DRAWSORT_DETERMINISTIC_OFF`:
     `{ "class": "feature", "owner": "render/determinism", "faithfulStatus": "n/a: deterministic SortDraws material-name tie-break, active under RB3_FIXED_CLOCK, default-ON; opt-out restores Wii mat-pointer order (draw-order flake) for W0.3d-fix fail-red", "default": "on" }`.
  2. Commit in the ENGINE repo under `flock /tmp/milo-engine-git.lock`, staging ONLY
     `NativeCompatFlags.classification.json` (leave sibling `FxSendNative.cpp` untouched):
     `W0.3d-fix: register RB3_DRAWSORT_DETERMINISTIC_OFF opt-out (classification only, no regen)`.
- **verify cmds:** `python3 -c "import json; json.load(open('/home/free/code/milohax/milo-native-engine/src/platform/NativeCompatFlags.classification.json'))"` (valid JSON);
  `python3 scripts/analysis/native_compat_census.py --selftest` (informational). Do NOT run `gen`.
- **note:** class is **feature** per the brief (not workaround), default-ON.

### W0.3d-fix.S3 — Verify determinism ≥15/15, fail-red, and multiset invariance
- **model:** opus
- **goal:** Prove the exit criteria on the S1 build; record numbers in STATUS.md. Verification only —
  no source edits.
- **verify cmds (build-agent-W0.3d-fix binary):**
  - **Determinism (exit gate):**
    `python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order --determinism-check 15 --bin native/build-agent-W0.3d-fix/rb3-native`
    → **15/15 identical** canonical multiset across fresh boots (run under `nproc` contention, e.g.
    `stress-ng --cpu "$(nproc)"` in the background, to expose the flake the fix removes). Confirm
    canonical draw **count == 888**.
  - **Multiset invariance vs committed golden:**
    `python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order --bin …` → PASS
    (order may differ from the committed exact-order file — that is the point; the canonical
    comparator is order-insensitive and stays green).
  - **Fail-red (opt-out restores flake):** under the same contention, capture ≥8 boots with
    `RB3_DRAWSORT_DETERMINISTIC_OFF=1 RB3_FIXED_CLOCK=1` → observe **≥2 distinct exact-order draw
    logs** (primary proof: the pre-fix flake returns), and/or the `--canonical-order` correspondence
    gate grazes RED on some runs. Default (opt-out unset) → 1 order / 15/15 green. Also run
    `python3 scripts/native/drawlog-golden.py --fixed-clock --fail-red-audit` → non-zero exit
    (golden untouched on disk).
  - **rb3-tests unchanged:** `ctest --test-dir native/build-agent-W0.3d-fix -R DrawLog` or
    `native/build-agent-W0.3d-fix/rb3-tests --gtest_filter='*DrawLog*'` → 9 pass / 1 skip (unchanged).
- **staging:** verification only; append results to STATUS.md under `flock /tmp/rb3-docs.lock`.

## Exit criteria (measurable)

1. `drawlog-golden.py --fixed-clock --canonical-order --determinism-check 15` → **15/15 identical**
   under CPU contention; canonical count **exactly 888**.
2. `--fixed-clock --canonical-order` vs committed golden → **PASS** (multiset/SET byte-identical;
   order permutation allowed and expected).
3. **Fail-red landed:** `RB3_DRAWSORT_DETERMINISTIC_OFF=1` under `--fixed-clock` + contention →
   order flake returns (≥2 distinct exact-order logs across ≥8 boots); default → 1 order.
   `--fail-red-audit` exits non-zero.
4. **Flag-OFF byte-identity:** all new logic under `#ifdef HX_NATIVE`; `git diff --numstat` on
   `Utl.cpp` is additions-only with the `mat1 < mat2` line intact → Wii/MWCC build unchanged; a
   non-harness native run (no `RB3_FIXED_CLOCK`) keeps `mat1 < mat2`.
5. `rb3-native` + `rb3-tests` build clean in `build-agent-W0.3d-fix`; `*DrawLog*` gtests 9 pass/1 skip.
6. `RB3_DRAWSORT_DETERMINISTIC_OFF` registered in `classification.json` (append-only, valid JSON);
   **no `gen.inc` regen run by this lane** (coordinator regens once at wave end).

**Note (handoff to coordinator, per Fable A4):** this item's exit is the **canonical-order** green.
The committed **exact-order** `splash_screen.json` will no longer match under the deterministic order
— that re-capture is the coordinator's single post-flip re-golden step (`W0.3d-fix → flip → ONE
re-golden`), NOT part of this item.

## Files touched (coordinator cross-diff)

- `rb3` `src/system/rndobj/Utl.cpp` (S1)
- `rb3` `native/src/rb3_replay.h` (S1)
- `rb3` `native/src/rb3_replay.cpp` (S1)
- `milo-native-engine` `src/platform/NativeCompatFlags.classification.json` (S2, append-only, no regen)
- docs: this `PLAN.md` + `STATUS.md` (under `flock /tmp/rb3-docs.lock`)

**Explicitly NOT touched:** any engine `.cpp`/`.h` (esp. `Rnd_Wgpu_RB3.cpp` — Lane A items 2/3 own
it), `src/App.cpp`, `NativeCompatFlags.gen.inc` (coordinator-regen), sibling `FxSendNative.cpp`,
`build-native`/`build-web*`/`build-coord-*`.

## Risks / conflicts

- **Lane A sequencing (hard):** `W0.3d-fix → W2.1-flip → W3.1a` sequential. This item **must land
  first** — the flip's re-golden requires the deterministic order. Coordinator gates the flip on
  this item's canonical-order green.
- **classification.json is multi-lane:** the flip, W3.1a, and W0.6b all write the same JSON +
  `gen.inc`. Mitigation: **`flock /tmp/milo-engine-classjson.lock`, APPEND-ONLY rows, NO `gen`** here
  (S2). Coordinator does the single reconciling regen at pin-bump. Do not race the regen.
- **Design-scope risk:** Design (1) leaves native-wide render-order instability (SYS-3) unfixed
  outside the harness — deliberate (out of scope, lower blast radius). If the coordinator later wants
  native-wide determinism (Design 2: drop the `RB3FixedClockActive()` gate), that is a separate item
  with its own exact-order-golden re-capture and UI/visual gating; do NOT fold it in here.
- **Fail-red fragility:** the flake is scheduler-dependent; S3 must run under real CPU contention
  (background `stress-ng`/`nproc` load) or the pre-fix flake may not surface, making the fail-red
  look vacuous. Record the contention method used.
- **Wii byte-identity:** the ONLY guard against regressing the MWCC build is that every new line sits
  under `#ifdef HX_NATIVE`. S1 must verify `git diff` shows no change outside those guards.
