# W-ISO — Venue-path consumer isolation + capture-lint hardening (PLAN)

**Lane:** Wave-19 Lane I (W-ISO). **Role of this doc:** executable plan for a Fable
reviewer then an implementer. **Author:** Opus plan-author.
**Charters (binding):** `RETROSPECTIVE/OPTIONS.md` §6.5 (rb3 `c4395043`); `WAVE19_KICKOFF.md`
Lane I (`d93fa894`); `WAVE19_REVIEW.md` amendments **A4/A5/A6** + answer **R-A** (`fb1f00e5`);
§4 lints 3/7/9 (BINDING). Engine pin `beb89e5`, NOT bumped by this lane.
**Tree state:** rb3 HEAD `fb1f00e5`. All anchors below re-derived **by symbol at this HEAD**.

**NAMING BOX (review F9, binding):** this lane operates on the **frame-assignment TIMING**
substrate only insofar as isolating per-consumer stream position removes count-variance from
the shared gRand stream. It does **not** touch R4's ledger `order` axis (already 10/10). No
doc or code comment in this lane may conflate the two.

---

## 0. Shape in one paragraph

Four venue-path gRand consumers reach the shared `gRand` stream with **boot-varying draw
counts** on the eng_hot (engaged-venue) path, so the R4 ledger **stream** axis cannot reach
10/10 there — which VOIDs the Wave-18 WHITE re-grade (`R4-M4/evidence/venue-path-divergent-consumers.md`,
`postAnchorDelta [16,0,16]`). This lane routes each consumer's draws onto its own isolated
per-tag stream using the **existing** `RB3LoadDetRedirect` RAII guard (one `#ifdef HX_NATIVE`
line per function, the CharEyes precedent), removing them from the shared count so the
post-anchor stream position becomes boot-invariant. Separately it extracts the review's F2/F7/F10
capture disciplines into a new shared `scripts/native/capture_lints.py` and wires
`white_regrade.py` to it. **This lane needs ZERO Rand.{h,cpp} edits, ZERO new engine flags, and
ZERO classjson rows** (verified §3.3) — the guard type and the seam gating already exist.

---

## 1. Anchors — re-derived BY SYMBOL at HEAD `fb1f00e5`

All four consumers draw exclusively through the **free functions** `RandomInt`/`RandomFloat`
(`src/system/math/Rand.cpp:186-212`), which are exactly what `RB3LoadDetRedirect` redirects
(`RB3_LOADDET_REDIR`, Rand.cpp:176-180). Guards go at **enclosing-function scope** (A4), matching
`CharEyes::NextLook` (`src/system/char/CharEyes.cpp:524-527`: `#ifdef HX_NATIVE` +
`RB3LoadDetRedirect _detEyes("chareyes");`).

| # | Consumer symbol (enclosing fn) | fn start | draw site(s) | draw call | tag |
|---|---|---|---|---|---|
| 1 | `CharClipDriver::CharClipDriver(Hmx::Object*,CharClip*,int,float,CharClipDriver*,float,float,bool)` — the **primary** ctor | `CharClipDriver.cpp:10` | `:62` | `RandomFloat(0, mClip->Range())` | `charclip` |
| 2 | `WorldCrowd::OnIterateFrac(DataArray*)` — Fisher-Yates shuffle | `Crowd.cpp:1216` | `:1234` | `RandomInt() % (i + 1)` | `crowditer` |
| 3 | `CharInterest::ComputeScore(...)` | `CharInterest.cpp:124` | `:172` | `RandomFloat(-0.25f, 0.25f)` | `charinterest` |
| 4 | `LightPresetManager::PickRandomPreset(Symbol)` | `LightPresetManager.cpp:275` | `:286` (probe branch) **and** `:294` (shipping) | `RandomInt(0, count)` | `lightpreset` |

**Anchor corrections applied vs the kickoff (A4):**
- #4: the kickoff cited `LightPresetManager.cpp:286`, which is the **`RB3_BOOTRNG_PROBE` branch
  copy** (`sBootRngProbe()` guarded, `:280-292`). The live shipping draw on graded (probe-OFF)
  boots is **:294**. Function-scope placement covers BOTH; both branches draw exactly once, so
  guarding at `PickRandomPreset` scope is correct regardless of probe state. **Verified in tree.**
- #2: `Crowd.cpp` has OTHER `RandomInt` draws at `:810`/`:812` (color-palette pick, a *different*
  method). A4: those were **not attributed** — **do NOT guard them.** Scope to `OnIterateFrac` only.
  **Verified in tree** (`:810/:812` are outside the `OnIterateFrac` body `:1216`+).
- #1: line `:62` sits in the **primary** ctor (`:10`), NOT the copy ctor (`:73`, which draws no
  random). **Verified in tree.**

**Draw-count evidence (from `R4-M4/evidence/venue-path-divergent-consumers.md`):** per-boot draws
CharClipDriver `[8,0,8]`, OnIterateFrac `[7,0,7]`, CharInterest `[4,5,4]`, LightPreset `[1,0,1]`;
spread `8+7+1 = 16 = observed postAnchorDelta`. (CharInterest spread is ±1, honoring F3's
"within ±1" tolerance.)

---

## 2. Milestones

### M1 — capture_lints.py FIRST (new file, conflict-free) + self-test

**Rationale (A6):** Lane I lands `capture_lints.py` as its FIRST checkpoint commit so Lane F can
import it for wash v2; new file = zero merge surface with F.

Create `scripts/native/capture_lints.py` extracting the review's F2/F7/F10 disciplines as pure
functions (no side effects, no boot). Exact API the implementer builds:

- `refuse_refinish_for_grade(is_refinish: bool, is_graded: bool) -> None` — **F2**: raises
  `CaptureLintError` if a `--refinish` (rebuilt-from-disk) run is used to emit a graded verdict.
  (`--refinish` is a crash-recovery finisher only; provenance for a *verdict* must be live boots.)
- `partition_black_frames(rows, luma_key="mean_luma", thresh=<T>) -> (kept, excluded)` — **F7**:
  splits luma≈0 frames out of the hi_frac population, mirroring the existing `mid_sat==nan`
  all-black rule (`white_regrade.py:218-220`). Threshold `<T>` derived from the committed
  zero-chroma exemplar `R4-M4/evidence/wr_n10_ON02_WHITE_zerochroma.png` (implementer measures its
  `mean_luma`; pick `T` just above it, documented in the file). Disclosure: return the excluded
  set so the caller can commit it (never silently drop).
- `attempt_disclosure(attempts: int, captured: int, discarded_reasons: list) -> dict` — surfaces
  the overshoot-discard survivorship (`capture_arm` loops to `n*4` attempts; §6.5's
  "attempt/retry counts disclosed").
- `safe_json_dump(obj, path)` — **F10**: `json.dump(..., allow_nan=False)` + a pre-pass that
  raises `CaptureLintError` naming the offending key path on any bare `NaN`/`Inf` (so an invalid
  dump fails loud instead of emitting non-standard JSON).
- `CaptureLintError(Exception)`.

**`--selftest` (the lane's own fail-red for the lint module, §4 lint 3):** fire each lint on a
known-GOOD and known-BAD fixture and assert separation:
1. `safe_json_dump({"x": float("nan")}, ...)` → RAISES (bad); `{"x": 1.0}` → writes (good).
2. `refuse_refinish_for_grade(True, True)` → RAISES; `(True, False)` and `(False, True)` → pass.
3. `partition_black_frames` on `[{mean_luma:0.0},{mean_luma:0.5}]` → excludes exactly the first.
4. `attempt_disclosure(12, 10, [...])` → `{attempts:12, captured:10, discarded:2, ...}`.
Print `SELFTEST 4/4 PASS` / exit non-zero on any miss. Commit `--selftest` transcript to
`W-ISO/evidence/capture_lints_selftest.txt`.

**M1 exit:** file exists; `python3 scripts/native/capture_lints.py --selftest` → `4/4 PASS`;
transcript committed. **Fail-red:** each of the four lints demonstrably RAISES on its known-BAD
input in the same transcript (a lint that never fires is instrument-blind).

### M2 — wire white_regrade.py to capture_lints (I owns this file's wiring, A6)

Edit `execution/R4-M4/white_regrade.py` (module-load pattern already used there,
`_load(...)`, :48-60):
1. `cl = _load("capture_lints", os.path.join(NSCR, "capture_lints.py"))`.
2. In `main()`, after arg parse: `cl.refuse_refinish_for_grade(a.refinish, is_graded=not a.validate)`
   — a `--refinish` full re-grade now REFUSES (F2). (`--refinish --validate` still allowed;
   `--refinish` for the two-arm verdict path is the banned combination.)
3. Replace the three `json.dump(...)` calls (:187, :280, :281-282) with `cl.safe_json_dump(...)`
   (F10).
4. Before computing `off_hi`/`on_hi` means (:216-217), route rows through
   `cl.partition_black_frames`; commit the excluded-frame list into `result["excluded_black"]`
   and disclose counts in the printout (F7). **Do NOT change the mid_sat nan-exclusion already
   at :219-220** — F7 is the luma sibling of that exact rule, added alongside it.
5. Thread `cl.attempt_disclosure(attempts, len(rows), ...)` out of `capture_arm` into the result
   dict (record `attempts` — currently discarded at `capture_arm` :86-88).

**Constraint:** this is a **wiring** edit — no change to verdict thresholds (G1a/G1b), the VOID
semantics, the eng_hot regime, or the `--validate` cap. Behavior on a clean full run is
byte-identical EXCEPT the new disclosure fields + the two now-enforced refusals.

**M2 exit:** `white_regrade.py --validate --n 3` still runs green on the existing binary and its
JSON now round-trips through a strict (`allow_nan=False`) parser. **Fail-red:** temporarily inject
a `NaN` into a row and confirm `safe_json_dump` refuses (don't commit the injection; note it in
STATUS).

### M3 — the four isolation guards (rb3-side, `#ifdef HX_NATIVE`, one line each)

For each anchor in §1, insert at **enclosing-function-body top** (before the first statement,
after the opening brace / member-init list), verbatim CharEyes form:

```cpp
#ifdef HX_NATIVE
    // R4 (Wave 19, W-ISO): <consumer> draws onto the isolated "<tag>" stream
    // (venue-path M1 divergent, spread <N>). Inert when the seam is off.
    RB3LoadDetRedirect _det<Name>("<tag>");
#endif
```

Exact sites (line numbers at HEAD `fb1f00e5`; implementer re-confirms with the symbol, not the
number, since M2 edits are in a different file and don't shift these):

| # | File | insert after | tag | guard var |
|---|---|---|---|---|
| 1 | `src/system/char/CharClipDriver.cpp` | ctor body opens (`:22` `... mPlayMultipleClips(multclips) {`) | `charclip` | `_detClip` |
| 2 | `src/system/world/Crowd.cpp` | `OnIterateFrac` body opens (`:1216`) | `crowditer` | `_detCrowdIter` |
| 3 | `src/system/char/CharInterest.cpp` | `ComputeScore` body opens (`:124`) | `charinterest` | `_detInterest` |
| 4 | `src/system/world/LightPresetManager.cpp` | `PickRandomPreset` body opens (`:275`, before the `count==0` early return) | `lightpreset` | `_detPreset` |

**Include:** each TU already calls `RandomInt`/`RandomFloat` → `math/Rand.h` is already in scope
(where `RB3LoadDetRedirect` is declared, under `#ifdef HX_NATIVE`). If the redirect symbol fails
to resolve, add a direct `#include "math/Rand.h"` — see ASSUMPTION-A.

**M3 exit:** `cmake --build native/build-native --target rb3-native` links clean (guards compile
under HX_NATIVE). No behavior asserted yet — that is M4.

### M4 — EXIT gate: eng_hot OFF-arm ledger **stream** axis 10/10 (BINDING)

Build a lane-owned harness `execution/W-ISO/iso_ledger_gate.py` that imports **white_regrade.py's
`capture_arm`** (eng_hot + seam env, `seam_env`/`wd.ARMS["eng_hot"]`, RB3_LOADDET_ATTRIB=1
already on) and **loaddet_gate.py's `grade_external_logs`** (both as modules, the `_load` pattern
— NO edits to loaddet_gate.py, which is F's file). It captures **N=10 OFF-arm** eng_hot boots
under the seam and grades their OWN logs (A2/F6 VOID discipline: never discard-and-rerun a
failing boot).

- **Why a lane script, not `white_regrade --validate`:** `--validate` caps N at 3
  (`white_regrade.py:174`); EXIT needs N=10. The lane script reuses `capture_arm` unchanged and
  does not perturb white_regrade's `--validate` semantics.
- **PASS:** `grade_external_logs(...)["summary"]["stream"] == "10/10"` AND `nParsed==10` AND every
  boot's `axes.stream.value` (postAnchorDelta) `== 0`. This makes the Wave-18 VOID precondition
  satisfiable — the deliverable of the lane.
- Commit `iso_ledger_gate` JSON + the 10 boot log paths' grade summary to
  `W-ISO/evidence/iso_ledger_n10.json`.

**FAIL-RED (BINDING, spelled out):** re-run `iso_ledger_gate.py` against a binary built at
`HEAD~1` (i.e. the four M3 guards absent) — or with the guards present but the seam OFF then
attribute — and show the **same eng_hot OFF-arm stream axis FAILS** (`stream < 10/10`,
`postAnchorDelta` spread ≈16, reproducing the committed `[16,0,16]` baseline at N=3, expected to
widen at N=10). Concretely the implementer captures BOTH:
- `W-ISO/evidence/iso_ledger_n10.json` (guards IN → 10/10, GREEN), and
- `W-ISO/evidence/iso_ledger_n10_PREGUARD.json` (guards reverted → RED, stream fails).
The delta between them is the lane's proof the isolation is load-bearing. (Build the pre-guard
binary in a scratch worktree or `git stash` **in a worktree** — NEVER `git stash` the main repo,
per CLAUDE.md. Cleanest: `tools/setup-worktree.sh w-iso-preguard`, build there.)

### M5 — G3 Wii-match inertness on all four touched units (BINDING)

The guards are `#ifdef HX_NATIVE`; MWCC never sees `HX_NATIVE`, so the match build is
byte-identical. G3 is a **regression check**, not a hope. Run:

```
batch_objdiff over one representative symbol per touched unit:
  main/system/char/CharClipDriver     (e.g. __ct__14CharClipDriverF...)
  main/system/world/Crowd             (e.g. OnIterateFrac__9WorldCrowdF...)
  main/system/char/CharInterest       (e.g. ComputeScore__12CharInterestF...)
  main/system/world/LightPresetManager(e.g. PickRandomPreset__18LightPresetManager...)
```

**M5 exit:** every touched unit's representative symbol == baseline (100%/COSMETIC per
`batch_objdiff`'s deterministic classifier; no REAL_DIFF). Commit the batch_objdiff verdict JSON
to `W-ISO/evidence/g3_batch_objdiff.json`. **Fail-red is structural:** if ANY unit shows
REAL_DIFF, the guard leaked out of `#ifdef HX_NATIVE` → STOP and fix the ifdef (a REAL_DIFF here
is the exact failure this gate exists to catch).

### M6 — flag-OFF byte-identical (native) + drawlog inertness

Confirm on a normal native boot (no `RB3_LOAD_DETERMINISM`): the guards are inert
(`RB3LoadDetStream` returns `NULL`, `sDetRedirect` stays `NULL`, one predicted-not-taken branch —
Rand.cpp:77-79,89-92). Evidence: a default-flags boot produces `0` `[LOADDET]` isolation effect
and the standard drawlog is unchanged. **M6 exit:** default boot health-check green + no new
`[LOADDET]` stream lines. (This is the native analog of G2; the Wii/MWCC analog is M5.)

---

## 3. R-A — bounded re-attribution (max 2 iterations; the anti-whack-a-mole rule)

Per **WAVE19_REVIEW R-A** (binding): the EXIT gate IS the completeness oracle — a missed fifth
consumer FAILS M4's 10/10. If M4 fails at N=10:

- **Iteration 2 (ONE only):** feed the **10 failing boots' own logs** (they already carry
  `[LOADDET] attrib frame= pc= off= sym= draws=` records; `RB3_LOADDET_ATTRIB=1` is on in
  `seam_env`) through `loaddet_gate.attribution_table` / `resolve_offsets` over the post-anchor
  window `[anchor, anchor+300]`. One pass names the COMPLETE residual PC set for those boots
  (addr2line → symbol). Isolate whatever it names with the same one-line guard. Re-run M4.
- **STOP rule (binding):** if N=10 still fails after iteration 2, the residual is presumptively
  **NOT** a per-consumer stream-position axis — it is draw-count-within-consumer variation driven
  by frame assignment, which is **Lane F/T1's domain**. Do NOT add a third guard round. Report the
  residual **priced**: name the residual PCs + per-boot spread, hand the failing boots' logs to
  T1's attribution (via STATUS + checkpoint), and record the M4 result as `HELD-residual-to-T1`.
  This is the R-A convergence guarantee, not a failure.

N=3 (how R4-M4 found the original four) under-samples rare-divergence consumers like LightPreset
(`[1,0,1]`); N=10 is the correct attribution width — hence the EXIT itself is at N=10.

### 3.3 Flags / classjson — NONE this lane (verified)

- **No new engine flag.** The four guards ride the **existing** `RB3_FIXED_CLOCK &&
  RB3_LOAD_DETERMINISM` seam (Rand.cpp:78, 90). Verified: `RB3LoadDetRedirect`/`RB3LoadDetStream`
  already exist and are already seam-gated at HEAD.
- **No classjson row.** Tag streams are created lazily at runtime (`RB3LoadDetStream`, Rand.cpp:80-83,
  `std::map<std::string,Rand*>`); there is no registry file to append. Reseed is generic
  (`RB3ReseedGRandAtAnchor` iterates the whole map, Rand.cpp:64-66). Verified: the four new tags
  (`charclip`/`crowditer`/`charinterest`/`lightpreset`) need zero config.
- **No `Rand.{h,cpp}` edit** (A5: that file is Lane F's this wave). Verified: the guard struct and
  the four free-fn redirect hooks are already in place; the lane only *constructs* the guard.

---

## 4. What I VERIFIED in the tree vs ASSUMPTIONS

**Verified at HEAD `fb1f00e5`:**
- All four anchors by symbol + enclosing function + draw call + exact line (§1); the :286-vs-:294
  probe/shipping split; the Crowd `:810/:812` non-anchor exclusion; the CharClipDriver primary-vs-copy
  ctor split.
- `RB3LoadDetRedirect` exists, is `#ifdef HX_NATIVE`, redirects the four free functions, is
  seam-gated, nests safely, is reset generically (Rand.cpp read in full).
- The CharEyes precedent form (`CharEyes.cpp:524-527`) and the four existing R4-M2 guard sites
  (Part.cpp ×2, CameraShot.cpp, Sequence.cpp, CharEyes.cpp).
- All four TUs compile into rb3-native (CMake source lists / build objects).
- objdiff unit names for all four (§2 M5).
- `white_regrade.py` structure, its `--refinish`/`--validate` modes, its three `json.dump` sites,
  its mid_sat-nan exclusion, its eng_hot seam env, its N=3 validate cap.
- `loaddet_gate.py` public entry points (`grade_external_logs`, `attribution_table`,
  `resolve_offsets`, `parse_boot_log`) — the EXIT-grade and R-A-attribution surfaces.
- Hazard: only `native/src/rb3_session_trace.cpp` is dirty in rb3 (the A3 linkage fix) — NOT a file
  this lane touches; NEVER stage it. `Rand.*` is clean.

**ASSUMPTIONS (reviewer: attack these):**
- **ASSUMPTION-A (low risk):** `math/Rand.h` is in direct or transitive scope in each of the four
  TUs (they already call `RandomInt`/`RandomFloat`, declared in that header). If the redirect
  symbol fails to resolve at M3 build, add an explicit `#include "math/Rand.h"`. Cheap to falsify
  at build time.
- **ASSUMPTION-B (THE RISKIEST — the whole lane's load-bearing bet):** the four Wave-18-attributed
  consumers are the **complete** eng_hot gRand-divergent set at N=10. The original attribution was
  N=3, which under-samples rare-divergence consumers (LightPreset already showed `[1,0,1]`). A
  fifth consumer that diverges only on some boots would pass N=3 attribution yet FAIL the N=10
  EXIT. **Mitigation is structural, not hope:** the EXIT gate is the completeness oracle and R-A
  (§3) is the bounded, convergent response — attribute the failing boots' own logs once, isolate,
  re-test; stop after iteration 2 and price the residual to T1. This assumption does not need to be
  true for the lane to terminate correctly; it only affects whether M4 passes in one round or two.
- **ASSUMPTION-C (low risk):** guard-at-function-scope over-captures nested draws (a callee of the
  ctor that itself draws gRand gets redirected too). This is SAFE for the EXIT (nesting restores
  the outer stream on scope exit; over-capture only removes MORE variance from the shared count).
  It could in principle capture a draw that "belongs" to another consumer — but since the goal is
  shared-stream 10/10, that is still correct. Flagged so the reviewer knows it is intentional.
- **ASSUMPTION-D (low risk):** the eng_hot capture regime reproduces the venue-path draws at N=10
  the way it did at N=3 (`wr_val_OFF_0{1,2,3}`). The `wd.ARMS["eng_hot"]` forced-hot env is
  deterministic under the seam; if a boot fails to engage the venue, its log shows 0 venue draws
  and is a capture-failure (retried), not a ledger failure — the existing `capture_arm` overshoot
  filter already handles this class.

---

## 5. Collision statement vs the other two Wave-19 lanes

Per WAVE19_REVIEW A5/A6 and the kickoff engine-writer note:

- **T1-FRAMETRACE (Lane F)** owns `scripts/native/loaddet_gate.py` and
  `execution/R4-M4/wash_cosample.py` **and `src/system/math/Rand.{h,cpp}`** this wave. **W-ISO does
  NOT edit any of them.** W-ISO only *imports* `loaddet_gate.py` as a module (read-only, via
  `_load`) for the M4 grade + R-A attribution, and *constructs* the already-existing
  `RB3LoadDetRedirect` (no Rand.* source change).
- **T2-WORLDROI (Lane P)** is the SOLE engine writer (prov sidecar, `Rnd_Wgpu_RB3.cpp`). **W-ISO
  writes NO engine code** — all four guards are rb3-side `src/system/**` decomp TUs, and the
  scripts are rb3-side Python.
- **W-ISO (Lane I)** owns, exclusively: the four consumer TUs' guard lines
  (`CharClipDriver.cpp`, `Crowd.cpp`, `CharInterest.cpp`, `LightPresetManager.cpp`), the NEW
  `scripts/native/capture_lints.py`, the wiring of `execution/R4-M4/white_regrade.py`, and the NEW
  `execution/W-ISO/iso_ledger_gate.py` + `W-ISO/evidence/**`.
- **Ordering (A6):** W-ISO lands `capture_lints.py` as its FIRST commit (M1, new file, conflict-free)
  so F can import it for wash v2. If F reaches wash v2 before M1 lands, F stubs the import and
  re-wires — one line either way; not W-ISO's concern.
- **Gate-change requests to F's files:** if M4/R-A needs a `loaddet_gate.py` change, W-ISO writes
  the request into its checkpoint/STATUS and continues; the coordinator folds it. W-ISO never edits
  `loaddet_gate.py` or `wash_cosample.py`.

---

## 6. Commit sequence (each under flock `/tmp/rb3-git.lock`, stage ONLY named files)

1. **M1:** `scripts/native/capture_lints.py` + `W-ISO/evidence/capture_lints_selftest.txt` +
   `W-ISO/PLAN.md` (this file) + `W-ISO/STATUS.md`.
2. **M2:** `execution/R4-M4/white_regrade.py` (wiring only).
3. **M3:** the four consumer TUs (one commit, "venue-path consumer isolation").
4. **M4/M5/M6:** `W-ISO/iso_ledger_gate.py` + `W-ISO/evidence/{iso_ledger_n10.json,
   iso_ledger_n10_PREGUARD.json, g3_batch_objdiff.json}` + STATUS update.

NEVER `git add -A`. NEVER stage `native/src/rb3_session_trace.cpp` or the engine's
`FxSendNative.cpp`. No pin bump, no default flip.

## 7. Process invariants (carried)

Headless `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, frame-count settling, pgid-only cleanup
(NEVER pkill by name). Builds under `/tmp/rb3-native-build.lock` or an own build dir. Checkpoint
`/tmp/wave19-checkpoints/W-ISO-plan.json` (plan) → `W-ISO.json` (impl) check-first,
write-before-return, update at each milestone. Evidence committed under `W-ISO/evidence/` or it
doesn't exist (§4 lint 7).
