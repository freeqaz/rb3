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

**Commits:** `8df42648` (S3_MEASURE.md + this STATUS section). No source, no flag, no flip.
**Exit criteria:** 1 (flag-OFF byte-identical) + 5 (`HandsBindOracle` green + fail-red) hold; criteria
2/3/4 covered N/A or inherited-green per above. Default flip is NOT created (coordinator-gated; N/A).

## VERIFY — partial (PART 1 confirmed correct; PART 2/S4 was never done — real gap)

**Verifier build:** own dir `native/build-agent-W2.6-verify` (Clang, default build type —
CAUTION: `-DCMAKE_BUILD_TYPE=RelWithDebInfo` produces a **build-breaking link failure**
unrelated to W2.6 — `TimeConversion.cpp`'s `MsToTick`/`TickToMs` are defined `inline` in
the `.cpp` and get dropped by the optimizer when not ODR-used in that TU at higher opt
levels, so `SystemConfig`/`TickToMs`/`MsToTick`/`Lyric::Width`/`SongDB::IsInCoda`/
`Game::GetActivePlayers` all go unresolved; default (no `CMAKE_BUILD_TYPE`) build is clean.
Not a W2.6 regression — pre-existing decomp code, not touched by this item — flagging for
whoever hits it next).

### PART 1 (foot/shoe rest-capture) — RE-DERIVED, CONFIRMED CORRECT

- `git diff`/`git log` on `BandCharacter.{cpp,h}`: **zero W2.6 source changes**, confirms
  S1/S2's "no flag introduced" claim literally (no `RB3_FOOT_REST_CAPTURE`/`sFootRestCapture`
  anywhere in `src/`).
- Re-ran `hands_bind_characterize.py --single default` **3x** (own build): all three
  reproduce the S1 characterization pattern — **0 `clipPlaying` occurrences** in any raw
  log (confirms the PLAN's targeted mechanism is genuinely absent, not just missed once);
  `saddleshoe_skin.2` shard ratio is **lineup-dependent** (0.8x-3.99x across my 3 runs, one
  run reproducing a near-4x graze) and never printed a literal `DROP` suffix in my samples;
  the worst SKINPOS across runs was 69.5u (matches S1's number), 0 fling, nothing >=92u,
  nothing in the 200-460u tripwire band. This is consistent with — and independently
  reproduces the substance of — S1's STOP verdict: the drop is a benign, rare, sub-tripwire
  transient, not the `clipPlaying`-missing-seed mechanism the PLAN hypothesized.
- `rb3-tests`: **79 pass / 3 skip(fixture) / 0 fail** (matches STATUS's claim exactly).
  `HandsBindOracle`: 3 pass/1 skip green; `HANDS_BIND_ORACLE_PERTURB=1` flips
  `ComposeIdentityAtBindPose` + `SkinnedVertsMatchAuthored` to FAILED — fail-red confirmed
  live, not vacuous.
- `drawlog-golden.py --fixed-clock --canonical-order --scene splash_screen`: **PASS**
  (888 draws, 264 known-residual within bound — residual count varies run-to-run per the
  known W0.3d eye-jitter flake, non-blocking, still PASS).
- `lineup-gate.py`: **PASS** all layers (img/segA/ratioB/countC/pin), max_band_ratio=3.34
  this run (S3 reported 3.99 — both within the golden bound; run-to-run variance expected,
  lineup is randomized).
- **Verdict: S1/S2/S3's STOP/no-flip conclusion holds up under independent re-derivation.**
  Nothing here should be reopened or force-fixed.

### PART 2 (S4 flag-registry cleanup) — NEVER EXECUTED. This is a real gap, not a nit.

**Finding:** STATUS.md has no `## W2.6.S4` section at all — S1/S2/S3 all say "handoff to S4"
but no S4 ever ran. Confirmed via `git log --grep=W2.6` (only S1/S2/S3 commits) and
`git log -- scripts/analysis/native_compat_census.py` (last real commit is `W0.6`, nothing
W2.6-authored until my fix below).

**What I found in the working tree before touching anything:**
- rb3 `scripts/analysis/native_compat_census.py` had an **uncommitted** diff adding the
  `"game" = src/system` scan root + a `gameOnly` summary bucket, with a code comment
  attributing it to "W2.6.S4" — but no STATUS.md record, no commit, and step 2/3 of S4
  (classify the flags, regen `gen.inc` + ledger) never happened. This looks like an
  interrupted/orphaned S4 attempt.
- `census check` **FAILS**: 86 unregistered flags (confirmed by direct run), including both
  headline flags `RB3_HANDS_BIND_FIX` and `RB3_SKEL_REBIND_FULL` that S1/S3's own handoff
  notes said must be registered. `classification.json` has **zero** entries for
  `RB3_HANDS_BIND_FIX`/`RB3_SKEL_REBIND_FULL`/`RB3_FOOT_REST_CAPTURE` (grep-confirmed).
  `--selftest` is unaffected (14/14, uses fixture roots) — matches the plan's expectation,
  but is not a substitute for the real registration.
- **Live collision confirmed, exactly as PLAN.md's Risks section warned:** the engine
  working tree (`milo-native-engine`) currently has an uncommitted `RB3_PLACEMENT_CONTRACT`
  entry in `NativeCompatFlags.classification.json` + regenerated `gen.inc` (230->231 flags,
  feature 2->3) — this is Lane A's (W2.1's) own WIP registering their own flag, sitting live
  in the shared file right now, alongside their uncommitted `Rnd_Wgpu_RB3.cpp` edit.
  `docs/.../NATIVE_COMPAT_LEDGER.md` has a matching uncommitted diff (same 230->231 delta) —
  also Lane A's, not W2.6's. **I left both completely untouched** (hard rule 8 spirit: don't
  fix up a sibling lane's in-flight work).

**What I did:** committed only the safe, isolated, rb3-only piece — the census.py
scan-root + `gameOnly` bucket addition (`a537c2a3`), re-verified via `--selftest` (14/14)
and a real `scan` (86 game-only flags, matches PLAN's "~90" estimate; the ~4-flag gap is
because the plan scoped "game" to `src/system` only, not `src/band3` — band3 has ~21 more
files with `getenv()` calls that remain out of census scope; that's a plan-scoping
boundary, not a bug in this fix).

**What I deliberately did NOT do:** author the ~92 `classification.json` entries (the two
headline flags + ~90 game flags) or run a real `gen` (which by default writes directly into
the **live, currently-active** engine `NativeCompatFlags.gen.inc` + would need to merge
against Lane A's in-flight `RB3_PLACEMENT_CONTRACT` addition in the same file). This is
squarely the collision this item's own PLAN.md Risks section anticipated and already
prescribed a mitigation for (author content, hand the engine-side commit to the
coordinator at pin-bump, sequenced after all lanes' engine flags are in) — attempting it
myself right now, mid-wave, with Lane A's WIP uncommitted in the same file, would risk
clobbering a concurrent teammate's work. Classifying ~90 flags by the plan's own rubric is
also substantive content-authorship, not a small fix.

**Exit criteria status (PLAN.md "Exit criteria"):**
- 1-6 (PART 1): **HOLD**, re-verified above.
- 7 (`census check` exits 0): **FAILS** (86 unregistered).
- 8 (3 headline flags classified + ledger regenerated+committed): **NOT DONE.**

### Recommendation to coordinator

W2.6 is **NOT fully done**. PART 1 (the behavior/diagnosis half) is solid and needs no
further action. PART 2 (S4) needs a follow-up pass, sequenced **after** Lane A's engine
flag registrations land (per this item's own Risks section), to:
1. classify `RB3_HANDS_BIND_FIX` + `RB3_SKEL_REBIND_FULL` + the 86 `game`-only flags
   (do NOT add `RB3_FOOT_REST_CAPTURE` — never implemented, per S1/S3) in
   `NativeCompatFlags.classification.json`,
2. run a real `gen` once (picks up Lane A's `RB3_PLACEMENT_CONTRACT` + W2.3's flag +
   these ~88 entries together, one clean regen — avoiding the "two lanes clobber gen.inc"
   failure mode),
3. commit `gen.inc` (engine) + `NATIVE_COMPAT_LEDGER.md` (rb3) together,
4. confirm `census check` exits 0 and `--selftest` stays 14/14.

**Files touched by this verify pass:** `docs/.../W2.6/STATUS.md` (this section),
`scripts/analysis/native_compat_census.py` (commit `a537c2a3`, rb3-only, no engine touch).
**Build dir used:** `native/build-agent-W2.6-verify` (not cleaned up — leave for
coordinator inspection or teardown at wave close).
