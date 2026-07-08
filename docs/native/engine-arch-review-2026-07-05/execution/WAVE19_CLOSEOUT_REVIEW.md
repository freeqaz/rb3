# WAVE-19 CLOSE-OUT REVIEW (Fable, adversarial, results-level)

**Scope:** T1-FRAMETRACE, T2-WORLDROI, W-ISO, coordinator close-out (`bd5a2eab`, pin
`beb89e5→6e6387c`). Verified against code + evidence at rb3 `bd5a2eab` / engine `6e6387c`.
**Overall verdict: ACCEPT — all three lanes' gates are real and their fail-reds fired.
Three claims are overstated as written (F1–F3) and need rewording, not rework. No landed
code needs to change. Wave-20 menu at the end.**

Method note: every claim below was checked against the artifact named, not the STATUS
prose. Two sub-verifications were delegated (T2 evidence pack; W-ISO stats/guards) and
their file:line findings re-cited here.

---

## Findings, ranked by severity

### F1 (MEDIUM-HIGH) — T1 gate-2: "DOSE-INDEPENDENT" overstates a low-power test; the
### attribution survives, but on a different leg than STATUS foregrounds

The kickoff's key question — is the jitter knob inert, making the honest-negative
vacuous? — is **REFUTED**: the knob is live on the OFF-arm and mechanically coupled to
the measured axis:

- `RB3_LOADDET_JITTER` is parsed independently of the seam env vars
  (engine `src/platform/ThreadCall_Native.cpp:38-52`) and fires in `WorkerMain`
  (`:99`) and the drain path (`:208`). The OFF-arm (`RB3_FIXED_CLOCK=1`,
  `RB3_LOAD_DETERMINISM` unset) keeps the async worker (`LoadDetSerialize`
  requires BOTH, `:71-80`), so injected boots really did sleep 0..N µs per job.
- The worker genuinely carries load-path work the frameAssign axis measures:
  `DataLoader::LoadFile` dispatches parses via `ThreadCall(unk38)`
  (`src/system/obj/DataFile.cpp:786`) and completion is frame-logged at
  `ThreadDone` (`:830-838`).

What IS overstated (`T1-FRAMETRACE/STATUS.md:72-80`, `evidence/gate2-injection.json`):

- **N = 1 injected boot per dose, 3 controls, 5 boots total.** The envelope test
  (`scripts/native/loaddet_gate.py:603-641`) needs the single injected boot to exit a
  3-boot [min,max] at ≥3/4 checkpoints. Only a huge effect could fire. "Dose-independent"
  is a claim about effect ABSENCE that this design cannot support.
- The data actually shows a **monotone dose shift**: at every checkpoint the 20000-dose
  boot crosses ~100 frames later than the 2000-dose boot (2773→2880, 3375→3476,
  4084→4184, 4529→4629; `gate2-injection.json:8-95`). Inside the envelope, and the four
  checkpoints are correlated (≈1 effective sample) — but a doc that says
  "dose-independent" while its own evidence shows a consistent same-direction shift will
  not survive a hostile read.
- The **load-bearing leg** for the T3 attribution is elsewhere and is solid: controls
  disagree on 177/457 names at `JITTER=0` (`gate2-injection.json:5`), and gate-1 shows
  6/6 distinct sigs on all three axes even seam-ON
  (`evidence/gate1-eng_hot-seam-on.json`: frameAssign/songClock/emitTimeline all DIVERGE,
  events 722/722/… constant while assignment differs). Ambient divergence exists with the
  knob OFF → the knob is **not necessary**. That is all T3 needs.

**Correction:** reword the attribution everywhere it appears (STATUS M4,
`gate2-injection.json` attribution strings are baked, so an errata note suffices):
"jitter is NOT NECESSARY (ambient divergence at JITTER=0); the injection differential was
UNDERPOWERED (n=1/dose) to detect a contribution, and shows an un-adjudicated +~100-frame
monotone shift inside the envelope." T3 consequence unchanged: do not use the knob as the
reproduce/pin lever. **T3 is NOT built on sand — but it is built on the control-arm
observation, not the injection.**

### F2 (MEDIUM) — W-ISO: the N=30 A/B is presented as "the load-bearing proof" but is
### not statistically decisive; internal rate inconsistency; attribution not in evidence

Confirmed from `W-ISO/evidence/iso_ledger_n10_PREGUARD.json` / `iso_ledger_n10.json`:
PREGUARD 29/30 RED (deltas 29×0, 1×1 at boot 22), GREEN 30/30 (all zero). But:

- **One divergent boot in 30.** P(30/30 clean with no guard | p≈1/30) ≈ 0.36; Fisher on
  1/30 vs 0/30 ≈ 0.5. The GREEN arm alone proves almost nothing, and STATUS.md:103-104
  ("the STATISTICAL A/B … is the load-bearing proof") mis-assigns the load. The actual
  proof is (a) the deterministic redirect mechanism (`Rand.cpp:89-93`, seam-gated), and
  (b) the divergent PREGUARD boot's draws attributing to two of the four guarded
  consumers. Keep the A/B as corroboration; do not call it load-bearing.
- **Rate inconsistency:** STATUS.md:81 says "~1/30 boots"; STATUS.md:95 says "~1/10
  boots" citing the Wave-18 baseline (`wr_n10-ledger-off.json`, 1 boot delta=16 of 10).
  Both cannot be the stated rate. Also the fresh event's magnitude (delta=1) is 16×
  smaller than Wave-18's (delta=16) — same consumer family per attribution, but say so
  rather than letting the reader infer the events are identical.
- **Attribution granularity gap:** the claim "divergent boot attributes to
  `CharInterest::ComputeScore:172` (4) + `LightPresetManager::PickRandomPreset:286` (1)"
  (STATUS.md:72-74) lives only in the commit message; the evidence JSON's
  `ra_attribution` block carries just `residual_rows: []` / `verdict: CLEAN`
  (`iso_ledger_n10_PREGUARD.json:95-109`). Add the per-PC rows (or a pointer) to the
  evidence pack.
- Minor: both evidence files are named `iso_ledger_n10*.json` but contain `nBoots: 30`.

Everything else in the lane held under adversarial check: exactly 4 guards, all
function-scope, all `#ifdef HX_NATIVE`, at the review-A4 sites —
`CharClipDriver.cpp:23-29` (ctor), `CharInterest.cpp:127-133` (`ComputeScore`),
`Crowd.cpp:1217-1224` (`OnIterateFrac` only, NOT :810/:812), and
`LightPresetManager.cpp:276-283` covering BOTH :286 (probe) and :294 (shipping) — the A4
correction was honored. `capture_lints.py` `BLACK_LUMA_THRESH = 0.05` with both AM-1
bounds documented in-code (`scripts/native/capture_lints.py:35-37`), selftest 4/4;
white_regrade wiring real (imports + F2/F7/F10 refusals). R-A subtraction implemented
(`iso_ledger_gate.py:112-144`, 9-site union) and residual CLEAN in evidence. M6 artifact
supports 0 seam-OFF lines / 792 golden / 116-0 tests.

**Consequence for the Wave-20 WHITE re-grade:** the precondition is satisfiable
**because the guard is deterministic**, not because 30/30 was observed. The re-grade must
run on the guarded build and treat its entry gate (fresh eng_hot OFF-arm ledger PASS) as
mechanism-backed; a GREEN N=10 on any unguarded build is ~90%-likely luck (STATUS's own
fresh-N=10 10/10 demonstrated exactly this trap).

### F3 (LOW-MED) — T1's "10 pre-existing GPU teardown failures" is environment-scoped,
### and the close-out numbers disagree across lanes without saying so

T1 reports rb3-tests 112/122 with 10 GPU-device failures "pre-existing, flag-inert"
(`T1-FRAMETRACE/STATUS.md:103-107`). W-ISO M6, T2 CLOSEOUT, and the coordinator commit
all report **116 PASSED / 0 FAILED (+7 skipped)** on the same tree and final pin
(`W-ISO/evidence/m6_default_boot.txt:22-24`, `T2-WORLDROI/evidence/CLOSEOUT.md`,
`bd5a2eab` message). Both are honest: the SIGSEGV class is documented pre-existing
(`execution/W0.3/STATUS.md:22`, teardown SIGSEGV on bounded non-HTTP boot), and T1's
A/B (identical 10-failure set with the flag ON vs unset) is a valid inertness proof
regardless of environment. But "pre-existing" without "host/GPU-contention-conditional —
the same suite is 116/0 elsewhere this same wave" invites a false conclusion that the
suite is red at HEAD. It is not. One-line scope note needed; also the totals differ
(122 vs 123) and nobody reconciles them.

### F4 (LOW) — T2 M5 FOREARM-FLOAT: "POSE-placement" is supported, but the charter
### should not read "pose" as excluding wrong-skeleton binding

The discriminator is sound: boneRects derive from `bt->WorldXfm()` (engine `ad01ca6`,
Rnd_Wgpu_RB3.cpp — TransParent worlds, no invBind/palette tap), and the bone rects
THEMSELVES sit at the float location (y≈284-297, above heads at y≈307) with
`boneFallback=0` on every hit (`evidence/M5-forearm-float-query.json`: gloves_resource +
clearcoat_resource, owner=player3, bone_R-foreArm/foreTwist1/2/hand in `bones_in_roi`).
Instrument and pixels agree → the palette bones' WORLDS are genuinely elevated. This
excludes invBind/skin-matrix error, camera-space error, and stale owner scope (RAII per
`DrawShowing`, `Character.cpp:321`). **Not yet excluded:** the mesh's `members[]` binding
the WRONG bone objects — a shared/static-skeleton alias whose pose is legitimately
elsewhere. That is exactly the outfit-mesh class `RebindOutfitBonesToOwnSkeleton` fixed
(char-skinning saga), and `gloves_resource`/`clearcoat_resource` on a band member is the
same family. The fix charter's FIRST check should be: do these meshes' palette bones
belong to player3's own `skeleton_unshared` (and did the rebind cover them)? Then clip/
pose data. The triage's "distinct from the finger clamp family" stands either way.

### F5 (INFO) — claims that held under adversarial check (no action)

- **R4 naming box (T1):** `order_sig`/`ledger_for_arm` appear in the wave diff only
  inside an added comment (`git diff b8ab2054~1..2aca12e2 -- scripts/native/loaddet_gate.py`);
  the functions are byte-untouched; 710==710 completes as claimed.
- **Gate-1 (T1):** seam-ON N=6 all-axes DIVERGE with frameAssign event count CONSTANT
  (722×6) — divergence is assignment, not volume; OFF-arm common-window run equalizes
  songClock sample counts (2801×3) and still DIVERGEs, answering the capture-length
  objection (`evidence/gate1-*.json`).
- **Wash v2 (T1):** committed-red refusal real (`wash_v2_regrade_refusal.json`: verdict
  DEGENERATE, `instrument_validated:false`, refused ⊇ {fx_emit_win:2, light_changes_win:2}),
  and the live run honestly re-DEGENERATEs: 37 shots at 2 distinct songms/frame
  (`wash_v2_live.json`) — the `r4m4_capture.multi_capture` sweep residual is upstream
  and is a named Wave-20 prerequisite, not a lane failure.
- **T2 flag-OFF inertness:** skinned branch gated
  `skinned && skinnedPoseValid && !RB3ProvSkinSphere()` (engine `ad01ca6`); Character.cpp
  hook strictly `#ifdef HX_NATIVE` (HX_NATIVE absent from the MWCC config) so Wii
  `Character.o` unchanged; DrawShowing 98.2% documented pre-edit (register-swap residual).
- **T2 G1 RED arm is REAL, not vacuous:** RED capture has 302 rectKind:1 sphere rects
  (+3 kind:2), and spot-checked rects ([663.1,658.1,…], [618.1,555.7,…]) demonstrably
  fail intersection with the band ROI [540,310,60,160] — genuine mislocalization, so the
  GREEN(106 named)-vs-RED(0) contrast is a true known-answer test
  (`evidence/gates/red_skinned_summary.json`).
- **Cross-lane co-edit:** `native/src/rb3_http_handlers.cpp` carries BOTH hunks at HEAD
  — T2 boneRects serialization (:284-314) and T1 `RB3LoadDetSongMs` (:1056-1057). No
  clobber; T1's staged-hunk-only claim is consistent with the result.
- **Coordinator:** pin `6e6387c` in `native/CMakeLists.txt:74` and its history contains
  exactly the wave's engine commits (`ce22beb`, `ad01ca6`, `515f617` + regen); ledger
  "Total flags: 379" matches `NativeCompatFlags.gen.inc` row count (379).

---

## Coordinator must fix before README

1. **Errata note on T1 gate-2 (F1):** replace "dose-independent" framing with
   "not-necessary + underpowered injection (n=1/dose; monotone +~100-frame in-envelope
   shift disclosed)". Keep the T3 consequence.
2. **W-ISO STATUS (F2):** reconcile ~1/10 vs ~1/30; state delta=16 (W18) vs delta=1
   (fresh) explicitly; demote the N=30 A/B from "load-bearing" to corroboration of the
   deterministic mechanism + attribution; add the divergent boot's per-PC attribution to
   the evidence pack; note the `_n10` filenames hold N=30.
3. **T1 STATUS test-count scope note (F3):** the 10 GPU failures are environment-
   conditional (suite is 116/0 on the final pin elsewhere in this same wave).
4. **M5 charter line (F4):** record "first discriminator = own-skeleton vs
   shared-skeleton palette binding (RebindOutfitBones coverage)" so the fix lane doesn't
   charter a clip/pose hunt first.

## Recommended Wave-20 menu

1. **WHITE re-grade cash-in — GO** (coordinator-sequenced per §6.5). Prerequisites, in
   order: (a) fix `r4m4_capture.multi_capture` so shots span ≥5 distinct songms/frame
   values — wash v2 will (correctly) refuse anything less, as it did live this wave;
   (b) entry gate = fresh eng_hot OFF-arm ledger PASS on the **guarded** build (N=10 is
   sufficient NOW because the guard is deterministic — but record it as mechanism-backed
   per F2, and keep N=30 for any arm whose guard status is in question);
   (c) capture_lints wired (done, W-ISO M2).
2. **T3 — GO in PINNING mode; NO-GO for jitter-reproduction.** Gate-1 proves the
   frame-assignment axis is unpinned even seam-ON; gate-2's control arm proves ambient
   scheduling suffices. Since ThreadCall is already drained inline under the seam
   (`LoadDetSerialize`) and native `AsyncFile` is synchronous main-thread I/O
   (engine `AsyncFile_Native.cpp`), the residual ambient actors are the NON-loader
   threads — the HTTP-poll songms source, the audio/stream clock behind songClock, any
   remaining worker. T3 = enumerate those actors and pin them under the existing seam
   (e.g., quantize/derive songClock from the fixed sim clock when
   `RB3_LOAD_DETERMINISM` is on); acceptance gate = T1 `--timeline` all-3-axes collapse
   at N≥6 seam-ON with the OFF-arm staying RED. If enumeration shows the songClock
   source is wall-clock-fundamental, T3 exits NO-GO with the actor named — that is a
   valid close.
3. **FOREARM-FLOAT fix charter (small lane).** Discriminator order per F4: (1) palette
   bone ownership (own vs shared skeleton, rebind coverage for gloves/clearcoat), (2)
   clip/pose data for the right-arm chain. T2's `uidump_query --roi` is the before/after
   gate; keep `RB3_PROV_SKIN_SPHERE` as the RED control.
4. **N-TAIL bad-torn recapture (half lane)** — carried from Wave-18, still open.
5. **R3-WALK v1 walk gap (small)** — carried; cheaper now that `--roi` names owners/bones.
6. **Conditional tail:** W2.4 BandPatchMesh (conditional-GO per Wave-18 review — do NOT
   co-schedule with any lane touching patch meshes; two prior native breaks), 4→8 lights,
   web tail gate. Take at most one as a filler lane.

Suggested shape: 3 primary lanes (WHITE cash-in; T3 pinning; FOREARM fix) + N-TAIL/R3
as half-lanes, same plan→review→implement protocol.
