# Wave 19 — Kickoff (determinism/instrument wave: W-ISO PRIMARY ∥ T1 ∥ T2)

**Author:** coordinator. **Status:** DRAFT — for Fable pre-dispatch review.
Parent: `execution/README.md` (Wave 18 results + Wave 19 menu) + `WAVE18_CLOSEOUT_REVIEW.md` +
**`RETROSPECTIVE/OPTIONS.md` §6 (rb3 `c4395043`) — the lane charters, BINDING as written.**
Engine pin `beb89e5`. TWELVE defaults ON.

## COORDINATOR ACCEPTANCE (<pending review>)

_To be filled from `WAVE19_REVIEW.md`._

- **Hazard note:** engine tree carries uncommitted `M FxSendNative.cpp` (concurrent audio
  work) — never stage it. rb3 tree carries other agents' uncommitted
  `native/src/rb3_session_trace.cpp` — never stage it.

## Shape

Executes `OPTIONS.md` §6's suggested Wave-19 shape verbatim: the Wave-18 PRIMARY follow-through
(venue consumer isolation) in parallel with the two top-ranked gen-2 instrument builds. NO
default flips this wave (T2's sidecar extension and T1's tracer are default-OFF instruments;
the WHITE re-grade itself is Wave-20 cash-in per §6.5). T3 (pinning mode) is explicitly NOT
co-waved with T1 per §6. The reviewer's job: check these lane scopings against the §6 charters
(which are already Fable-authored) and the current tree, not re-review §6 itself.

## Lanes

**Lane I — W-ISO: venue-path consumer isolation + capture-lint hardening (Opus; executes
`OPTIONS.md` §6.5):**
Extend the R4 pattern (RB3LoadDetStream per-tag isolation, one scoped-guard line per site) to
the four Wave-18-attributed venue-path consumers: `CharClipDriver.cpp:62`,
`WorldCrowd::OnIterateFrac` (Crowd.cpp:1234 Fisher-Yates), `CharInterest.cpp:172`,
`LightPresetManager.cpp:286` — re-derive every anchor BY SYMBOL. All behind the existing
`RB3_FIXED_CLOCK && RB3_LOAD_DETERMINISM` opt-in; flag-OFF byte-identical; G3 Wii-match
(batch_objdiff == baseline on touched units). Plus the §6.5 hardening: extract shared
`scripts/native/capture_lints.py` (no `--refinish` into graded runs; black-frame/luma-0
exclusion with disclosure; attempt disclosure; no bare NaN in JSON) and wire `white_regrade.py`
+ `wash_cosample.py` to it. EXIT: eng_hot OFF-arm ledger stream axis 10/10 (the Wave-18 VOID
precondition satisfiable) + fail-red shown (isolation reverted → stream fails again). The
WHITE re-grade itself is NOT this lane (Wave-20 cash-in, coordinator-sequenced).

**Lane F — T1 frame-timeline tracer, ATTRIBUTION mode (Opus; executes `OPTIONS.md` §6 T1):**
The "loaddet-for-time": per-frame ledger of async-completion→frame assignment + per-tag
emission counts + songMs↔frame join, as three new gradeable axes
(`frameAssign`/`songClock`/`emitTimeline`) in `loaddet_gate.py`, PIE-stable keys via the
existing addr2line path. Target = the LW-1/ThreadCall completion seam (W0.3b already pins
queue drain under fixed clock, `Loader.cpp:622,729` — bytes-arrival frame is what varies).
Wash co-sampler v2 folded in as first consumer (per-frame join + midrank AUC), with the
BINDING fail-red: v2 must REFUSE the committed F1 two-cluster `wash_natural.json`. NO pinning
mode this wave (that is T3, Wave-20-conditional on this lane's attribution). F9 naming box
carried: this lane targets FRAME-ASSIGNMENT TIMING, explicitly not R4's already-10/10
loader-order axis.

**Lane P — T2 world-cam ROI provenance (Opus; executes `OPTIONS.md` §6 T2; SOLE engine
writer):**
Extend the R3 prov sidecar with skinned-pose bboxes computed from the mitten pre-pass's
existing CPU-side composed palette (`Rnd_Wgpu_RB3.cpp:3752-4009` region — re-derive by
symbol) + per-bone sub-rects + a world owner scope hook, so
`uidump_query.py --roi` answers "which mesh/bone/owner drew this" for WORLD-cam draws.
Default-OFF, classjson append under lock. Fail-red per §6: the documented v1 sphere-rect
blindness as the RED baseline + a disjoint-ROI negative control. VALIDATION (production
smoke, not a fix): one query run on a burst_08-class frame that names the FOREARM-FLOAT
structure's mesh/bone/owner — triage output only; any fix is a future charter.

_Engine writers: Lane P only (T2 sidecar region). Lane F is rb3-side (Loader/ThreadCall +
loaddet probe + scripts). Lane I is rb3-side (Rand tags at 4 sites + scripts). Declared rb3
file overlap between I and F: `scripts/native/loaddet_gate.py` (I adds nothing there; F adds
axes) and `wash_cosample.py` (I wires capture_lints; F rewrites the join) — the reviewer
should either bless a landing order or move the wash_cosample v2 wiring wholly into F._

## Process rules (carried) — VERBATIM per KICKOFF_TEMPLATE.md

Locks: rb3 `/tmp/rb3-git.lock`; engine `/tmp/milo-engine-git.lock`; classjson
`/tmp/milo-engine-classjson.lock` (append-only, single coordinator regen at close-out);
builds `/tmp/rb3-native-build.lock` or own dir. Checkpoints
`/tmp/wave19-checkpoints/<lane>.json` — check-first, write-before-return, update every
milestone. PLAN/STATUS under `execution/<KEY>/`. Evidence committed under
`execution/<KEY>/evidence/` or it doesn't exist. New flags default-OFF; NO default flips, NO
pin bumps by lanes (coordinator, ONCE, at close-out). Refuted flags UNSET. TWELVE defaults
stay ON. Headless `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, frame-count settling, pgid-only
cleanup (NEVER pkill by name).

## Pre-dispatch checklist — the ten §4 lints

- [x] **1. Matrix-relative + pointer-verified bone claims** — N/A for I/F (no bone claims);
  P's per-bone sub-rects derive from the already-shipped mitten palette compose.
- [x] **2. Split by population** — F's timeline axes graded per-boot and per-tag; I's ledger
  per-axis; P's ROI output per-mesh/per-bone.
- [x] **3. No unvalidated oracles** — F's wash v2 has the BINDING F1-refusal fail-red; P has
  the sphere-rect RED baseline + negative control; I's exit has the isolation-reverted
  fail-red.
- [x] **4. Shipped-flag contradiction grep** — done: no lane claim contradicts the twelve
  defaults; F/I stay under the existing opt-in seam flags.
- [x] **5. Grants** — all three are instrument/measurement lanes with wide read; writes
  scoped per lane above.
- [x] **6. Option table before 2nd fix attempt** — no fix attempts this wave (P's
  FOREARM-FLOAT run is triage-only by charter).
- [x] **7. Evidence committed** — carried verbatim.
- [x] **8. Flag hit-counts on negatives** — I: per-site tag hit-counts in the ledger; F:
  per-axis sample counts; P: prov row counts per capture.
- [x] **9. Flavor-membership grep** — I's four sites + F's Loader/ThreadCall TUs to be
  verified compiled into rb3-native before edits (lane step 0).
- [x] **10. Instruments before fixes** — the wave's shape (re-grade and pinning are Wave-20
  cash-ins).

## Risks / open questions for the reviewer

- **R-A (Lane I):** are the four venue-path sites really the complete eng_hot set, or should
  the lane re-run the Wave-18 attribution AFTER isolation to catch a next-layer residual
  (attribution → isolate → re-attribute loop, bounded at 2 iterations)?
- **R-B (Lane F):** is the LW-1/ThreadCall completion seam the right attribution point per
  the W0.3b/W0.3d evidence trail, and are the three §6 axis definitions gradeable from one
  boot log without a second instrumentation pass?
- **R-C (Lane P):** does the mitten pre-pass palette actually cover ALL world skinned draws
  (it's gated to band hand meshes today) — does T2 need its own palette tap, and if so is the
  declared engine region still the right one?
- **R-D (I∥F overlap):** bless a landing order for `loaddet_gate.py`/`wash_cosample.py` or
  re-scope (see Lanes note).
