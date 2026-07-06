# W2.6 — Foot/shoe rest-capture coverage + flag-registry cleanup — STATUS

Append-only log. Update under `flock /tmp/rb3-docs.lock`. One `## <subtask-id> — done|partial|blocked`
section per subtask with commit SHAs + blockers. Re-runs read this + `git log --grep=W2.6` and skip
done work.

Lane C (rb3-only, parallel). Engine RENDER/skinning seams READ-ONLY (same seams as W2.2:
`mNativeBonesRebound` + `RB3_GUARD_EXEMPT_REBOUND`; NO `Rnd_Wgpu_RB3.cpp` / `DrawMesh` edits).
Engine pin `6221a56` — do NOT bump (coordinator bumps once per wave).
Build dir: `native/build-agent-W2.6` (Clang). Targets `rb3-native` / `rb3-tests`.

DEFAULT-OFF for the foot/shoe fix; separate coordinator-gated flip. STOP-TRIPWIRE (PART 1): a
200-460u smear or >92u skinpos on a rebound appendage reproduces the rotation-basis failure — STOP.

## planning — done
- PLAN.md written (Opus planner). Subtasks W2.6.S1..W2.6.S4. See PLAN.md `## Subtasks`.

## W2.6.S1 — done (BRANCH = STOP / diagnosis-only)

**Build:** `rb3-native` @ engine pin `6221a56`, `native/build-agent-W2.6` (Clang) — BUILD_OK.
**Harness:** `scripts/native/hands_bind_characterize.py` x4 passes on HEAD (RELOAD/HEAD_REBIND/
SHARD_RATIO/REBIND_DRAW_SKINPOS/REBIND_DRAW_FLING/SKIN_CLAMP probes).

**Fail-red baseline reproduces (lineup-dependent).** Band lineup is randomized per boot; the
`saddleshoe_skin.2` guard-DROP appeared in 1 of 4 passes (rep3) at **4.37-4.56x DROP** — matches
W2.2's 4.73x. Canonical drop baseline committed: `W2.6/char/parsed-drop.json` (raw-drop.log local,
gitignored). No-drop lineup: `parsed-default.json`.

**PLAN's PART-1 mechanism REFUTED.** The stated cause (late lower-body bones lack a clip-free seed →
mid-clip first-resolve `why=clipPlaying` → V24 drop) does NOT occur: **0 `clipPlaying` pendings** on
HEAD AND in the W2.2 baseline's own log (both: 91 `unresolvable` + 24 `boundRebakeOff`, 0 clipPlaying).
`[REST_SEED] poll=0` already seeds 105-109 bones/member clip-free; later seeds are `added=0`. The shoe
mesh resolves distinct clip-free and **completes its rebind** (`[CHAR_MESH] rebound=1`). The DROP is a
**2/155-frame count-in transient** (W2.2: 5/190) the guard already hides — max SKINPOS 69.5u overall,
**0 fling, no ≥92u, no 200-460u tripwire**.

**Why S2 is a no-op:** the seed already covers these bones; the `RB3_HANDS_BIND_FIX`-style completion
branch is gated on `clipPlaying` which never fires; the only bind-side lever for the transient frames
is `own==bound` `RB3_BOUND_REBAKE` = the proven 200-460u rotation-basis STOP-TRIPWIRE (engine-side
draw-order/guard fix is out of PART-1's READ-ONLY scope).

**Consequence:** S2/S3 (PART-1 behavior) become NO-OPs. W2.6 lands **PART 2 (S4 flag-registry) + the
diagnosis memo** only — NO PART-1 behavior change (exit criteria 1/5 hold trivially: no flag, no code
path). **S4 should NOT introduce `RB3_FOOT_REST_CAPTURE`** (never implemented) — register only
`RB3_HANDS_BIND_FIX` + `RB3_SKEL_REBIND_FULL` + the 90 `rb3/src/system` scan-root flags.

**Deviation vs plan:** the shard is BELOW the rotation-basis tripwire (2-frame guard-hidden transient,
not >92u/200-460u); STOP holds because the targeted mechanism (clipPlaying missing-seed) is absent.
Memo: `W2.6/char/S1_DIAGNOSIS.md`. Files touched: `W2.6/char/*` artifacts only (no source).

## W2.6.S2 — done (NO-OP per S1 STOP / diagnosis-only)

**Decision:** S2 is explicitly gated "(only if S1 = FIX-VIABLE)" in PLAN.md section W2.6.S2. S1
branched to **STOP / diagnosis-only** (`W2.6/char/S1_DIAGNOSIS.md`, commit `0f3388c0`). Therefore S2
lands **no source change**: `RB3_FOOT_REST_CAPTURE` is **NOT introduced**.

**Verified before deciding (S2 implementer, Opus):**
- `git log --grep=W2.6` -> only `0f3388c0` (S1 diagnosis, artifacts-only). No prior S2 work.
- `grep -rn RB3_FOOT_REST_CAPTURE|sFootRestCapture src/` -> 0 hits (flag never added anywhere).
- `git status src/system/bandobj/` -> clean (no partial/uncommitted S2 edits to reconcile).

**Why implementing S2 would be wrong, not merely unnecessary:**
1. The PLAN's targeted mechanism (`missWhy="clipPlaying"` mid-clip first-resolve -> V24 drop) is
   REFUTED — **0 `clipPlaying` pendings** on HEAD *and* in the W2.2 baseline log. The S2 completion
   branch mirrors `RB3_HANDS_BIND_FIX`, gated on `mDriver->FirstPlaying()`; that predicate never fires
   for the shoe bones, so the branch would be **dead code**.
2. The S2 load-time pre-seed is already effectively present: `[REST_SEED] poll=0` seeds 105–109
   bones/member clip-free at the gender-bind deform pose (`added=0` on later attempts), so `rp != end`
   already; seeding "by name earlier" changes nothing about when the mesh is processed.
3. The residual DROP is a benign 2-frame guard-hidden count-in transient (2 DROP / 155 samples; max
   SKINPOS 69.5u, 0 fling, nothing >=92u or 200-460u). The only remaining bind-side lever is
   `own==bound` rebake = the proven 200-460u rotation-basis **STOP-TRIPWIRE**. Engine READ-ONLY scope
   forbids the real fix (defer-draw / lineup-aware guard).

Introducing the flag anyway would add dead, flag-guarded code the diagnosis says not to ship — scope
creep against the S1 decision gate. **Exit criteria 1 (flag-OFF byte-identical) and 5 (`HandsBindOracle`
green) hold trivially: no new flag, no new code path.**

**Handoff to S4 (PART 2):** do NOT register `RB3_FOOT_REST_CAPTURE` (never implemented). Register only
`RB3_HANDS_BIND_FIX` + `RB3_SKEL_REBIND_FULL` + the 90 `rb3/src/system` scan-root flags, per S1 memo.

**Files touched by W2.6.S2:** STATUS.md only (this section). No source, no commits beyond docs.

## W2.6.S3 — done (verify: NO-FLIP, diagnosis-only certified regression-safe)

**Premise:** S3 is gated "(only if S2 ran)". S2 = NO-OP (`2675cde6`): `RB3_FOOT_REST_CAPTURE` never
introduced, `git diff HEAD -- src/system/bandobj/BandCharacter.{cpp,h}` EMPTY. So the flag A/B gates
(Gate 2 flag-ON, Gate 5 flag-ON) are **N/A** — no flag. S3 instead certifies the diagnosis-only HEAD
(= the "flag-OFF" state, byte-identical) is regression-safe and produces the go/no-go package.

**Recommendation: NO-FLIP** (nothing to flip; PART-1 landed diagnosis-only per S1 STOP). Package:
`W2.6/char/S3_MEASURE.md`.

**Gate results (all applicable gates GREEN):**
- **Gate 1 flag-OFF byte-identical — PASS:** source diff EMPTY; drawlog splash canonical golden PASS
  (888 draws, 238 known-residual within bound, 0 unexpected); W0.5 `lineup-gate.py` PASS all layers
  (img/segA/ratioB/countC/pin, max_band_ratio 3.99); `rb3-tests` 79 pass / 3 skip(fixture) / 0 fail
  incl. `HandsBindOracle` green + `HANDS_BIND_ORACLE_PERTURB` fail-red RED (2 tests flip to FAILED).
- **Gate 2 flag-ON drop clears — N/A:** no flag (S2 NO-OP).
- **Gate 3 STOP-tripwire — PASS (inherited, no code delta):** S1 measured max SKINPOS 69.5u, 0 fling,
  nothing ≥92u, nothing 200-460u; unchanged (no source change).
- **Gate 4 negative control — PASS (trivial):** crowd/extras SKIN_CLAMP byte-identical vs S1 baseline
  (`parsed-drop.json` 35 meshes/1778 events; `parsed-default.json` 36/2753 — lineup-dep) — no source
  touches the non-rebound crowd/extras path.
- **Gate 5 W0.5 lineup (flag-ON path) — PASS:** reduces to flag-OFF=HEAD; lineup gate verdict=PASS.

**Commits:** `<S3_SHA>` (S3_MEASURE.md + this STATUS section). No source, no flag, no flip.
**Exit criteria:** 1 (flag-OFF byte-identical) + 5 (`HandsBindOracle` green + fail-red) hold; criteria
2/3/4 covered N/A or inherited-green per above. Default flip is NOT created (coordinator-gated; N/A).
