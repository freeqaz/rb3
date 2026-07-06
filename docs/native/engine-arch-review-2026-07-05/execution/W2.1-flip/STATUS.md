# W2.1-flip — STATUS

Append-only log (implementers/verifiers), under `flock /tmp/rb3-docs.lock`. One
`## <subtask-id> — done|partial|blocked` section per subtask with commit SHAs + blockers.
Re-runs read this + `git log --grep=W2.1-flip` and skip done work.

## Plan — done
- PLAN.md written by Opus planner (2026-07-06). Objective: flip-readiness package for
  `RB3_PLACEMENT_CONTRACT` (S1 opt-out mechanism, S2 drum oracle, S3 UI regression gate, S4 Dolphin
  A/B). Exit = `ready-for-flip`; coordinator flips + re-goldens after E1 sign-off. NO subsumption
  step (A1 verified: contract arm excludes UI hacks at `Rnd_Wgpu_RB3.cpp:2878-2879`, no double-apply).
  W0.3d-fix predecessor confirmed landed (rb3 `b9bd33f3`).

<!-- implementers append below -->

## W2.1-flip.S1 — done
- **Commit (engine):** `dbf2758` — `W2.1-flip: RB3_PLACEMENT_CONTRACT default+opt-out mechanism (effective default STILL OFF)`.
  Staged ONLY my 2 files (`src/platform/Rnd_Wgpu_RB3.cpp`, `src/platform/NativeCompatFlags.classification.json`);
  sibling `FxSendNative.cpp` left uncommitted/untouched. Engine pin NOT bumped.
- **Mechanism (Rnd_Wgpu_RB3.cpp read, was :2874-2875):** replaced the presence-truthy read with the
  default+opt-out shape from PLAN.md S1. `kPlacementContractDefaultOn = 0` (effective default STILL OFF —
  coordinator flips 0->1). Precedence: `RB3_PLACEMENT_CONTRACT_OFF` -> 0 (wins) -> legacy `RB3_PLACEMENT_CONTRACT`
  -> 1 (opt-in kept) -> else `kPlacementContractDefaultOn`. All downstream sites read `placementContractArm`
  (derived from `sPlacementContract`), so this single-static read is the only change (:2878-2879, :3266, :4022 untouched).
- **classification.json:** appended ONE `RB3_PLACEMENT_CONTRACT_OFF` row (class:feature, owner:render/placement,
  default:off, read:presence) under `flock /tmp/milo-engine-classjson.lock`, APPEND-ONLY, JSON re-validated.
  **NO gen.inc regen** (coordinator regens once at wave end).
- **Build:** `native/build-agent-W2.1-flip` (clang) — `rb3-native` + `rb3-tests` both exit 0.
- **Verify — all properties GREEN:**
  - Default byte-identity (committed build, no env): `drawlog-golden.py --canonical-order --fixed-clock` -> **PASS 888 draws**;
    `lineup-gate.py` -> **PASS** all layers (img/segA/ratioB/countC/pin).
  - Legacy opt-in preserved: `RB3_PLACEMENT_CONTRACT=1 placement-gate-capture.py` -> **ORACLE GREEN** (exit 0).
  - One-line-flip rehearsal (scratch build `kPlacementContractDefaultOn=1`, NOT committed, source reverted clean +
    scratch dir removed): no-env -> **ORACLE GREEN** (flip mechanism works); `RB3_PLACEMENT_CONTRACT_OFF=1 --expect-red`
    -> **ORACLE RED**, exit 0 (opt-out fail-reds post-flip).
  - Opt-out precedence: `RB3_PLACEMENT_CONTRACT=1 RB3_PLACEMENT_CONTRACT_OFF=1` (committed default-OFF build)
    `--expect-red` -> **ORACLE RED**, exit 0 (`_OFF` beats the opt-in).
- **DEVIATION (expected, per HARD RULES + PLAN Risks):** `native_compat_census.py check` exits **1** — but ONLY on
  gen.inc/ledger staleness, NOT a real regression. `RB3_PLACEMENT_CONTRACT_OFF` (mine) AND
  `RB3_DRAWSORT_DETERMINISTIC_OFF` (prior landed lane W0.3d-fix `b9bd33f3`) are both flagged "getenv flag not in
  registry" because `gen.inc` has not been regenerated. Census was ALREADY red before my change (W0.3d-fix's flag).
  gen.inc regen is DEFERRED to the coordinator's single wave-end regen (HARD RULE). classification.json (source of
  truth) has my row and is valid JSON; census will pass after the coordinator regens. This is the documented
  multi-lane append-only/no-regen state — PLAN.md S1 "census exit 0" is stale w.r.t. that protocol.
- **Exit:** `ready-for-flip` mechanism half. Coordinator flips `kPlacementContractDefaultOn 0->1` (one line) after E1
  sign-off, then re-goldens (888->792). Committed default STILL OFF; flag-OFF path byte-identical.

## W2.1-flip.S2 — done
- **Commit (rb3):** `c49fddb4` — `W2.1-flip: drum-kind placement oracle + SyncPlayMode probe + fail-red (S2)`.
  Staged ONLY my 4 rb3 files (`src/system/bandobj/BandConfiguration.cpp`, `native/tests/placement_oracle.h`,
  `native/tests/test_placement_oracle.cpp`, `scripts/native/placement-gate-capture.py`). NO engine edit, NO
  classification.json edit (reuses the existing `RB3_PLACEMENT_PROBE` flag, per PLAN S2). Pin NOT bumped.
- **Probe (BandConfiguration.cpp `SyncPlayMode`):** `HX_NATIVE`+`RB3_PLACEMENT_PROBE`-gated `fprintf` inside the
  `if (bchar)` block after `Teleport`, emitting `RB3_PLACEMENT_PROBE drum inst=<slot> x= y= z=` from
  `mXfms[i].mWay->WorldXfm().v` per resolved slot (the faithful band-member waypoint the kit hangs off).
  Mirrors the Crowd.cpp `:403` probe. Wii-inert (`#ifdef HX_NATIVE` guards includes + body), default-OFF, no
  behavior change. Added `<cstdio>`/`<cstdlib>` under an `HX_NATIVE` include guard.
- **Oracle (`placement_oracle.h` `RunDrumOracle`):** distinct from crowd's spread test. (a) reference non-origin:
  >=1 drum ref with `Radius>originRadius` else INCONCLUSIVE (`kDrumRefAtOrigin`); (b) drawn consistency: >=1
  **skinned** draw with `Radius>originRadius` AND within `drumEps` of a ref else RED (`kDrumAtOrigin`).
  Build-independent discriminator: the default-OFF build has ZERO non-origin skinned draws (all identity), so any
  reasonable `drumEps` goes RED. New `OracleOptions.drumEps=12.0`; two new `FailureKind`s.
- **KIT-SKINNED VERIFICATION (PLAN S2 step 5 — REQUIRED):** on a live flag-ON capture the drawlog has **108
  non-origin skinned draws** (vs **0** flag-OFF), confirming the band/kit props ARE in the skinned/contract arm.
  The 4 band waypoints sit at radius 35.5..147.4; nearest non-origin skinned draw per ref = **0.00 / 3.50 / 4.82 /
  18.02** units — 3 of 4 comfortably inside `drumEps=12.0` (>=1 required). So the kit-prop-skinned assumption HOLDS
  and no drawn-predicate fallback was needed; `drumEps=12.0` is well-calibrated with margin (matches at 0-5u).
- **Tests (`test_placement_oracle.cpp`):** 6 synthetic drum tests — `ParsesDrumProbeLog`, `DrumAtOriginIsRed`
  (fail-red demo), `DrumDrawnAtDrummerIsGreen`, `DrumRefAtOriginIsInconclusive`, `DrumNoRefsInconclusive`,
  `DrumToleratesPropOffset` — plus the `RealCaptureDrumPlaced` live gate. Full suite: **12 pass / 2 live-skip**.
- **Harness (`placement-gate-capture.py`):** now extracts drum refs from the WHOLE log (deduped — SyncPlayMode
  fires at band setup, BEFORE the crowd per-frame capture window) + crowd from the window; new `--gate
  {crowd,drum,both}` (default `crowd`, backward-compatible); runs+reports BOTH oracles per capture.
- **Verify — all GREEN:**
  - Synthetic drum tests: 6/6 pass.
  - **Live default-OFF (required fail-red):** drum oracle **RED** (`posed=4 far=4 matched=0 skinnedDraws=231`,
    nearest -1.0 = zero non-origin skinned draws). `--gate drum --expect-red` -> **exit 0** (fail-red confirmed).
  - **Live flag-ON (`RB3_PLACEMENT_CONTRACT=1`):** drum oracle **GREEN** (both crowd + drum OK), `--gate drum` exit 0.
  - **Default byte-identity:** no-env `drawlog-golden.py --canonical-order --fixed-clock` -> **PASS 888** (my
    probe is behavior-neutral; rebuilt rb3-native unchanged on the default path).
- **DEVIATION (none material):** none. No classification.json / engine edit; probe reuses `RB3_PLACEMENT_PROBE`.
- **Exit:** S2 `ready-for-flip` half complete. The drum gate is now a hard flip gate — RED on the committed
  default-OFF build, GREEN flag-ON. Coordinator flips (S1's one line) + re-goldens after E1 sign-off.

## W2.1-flip.S3 — done
- **Commit (rb3):** `84c30275` — "W2.1-flip: UI-placement regression gate (song_select scrollbar +
  main_hub hub bar, A/A) — S3". Staged ONLY my files (new `scripts/native/w21flip-ui-ab.py` +
  `W2.1-flip/ui-ab/*` artifacts). No source edit (READ-ONLY on engine/rb3 per PLAN). Pin NOT bumped.
- **Harness (`scripts/native/w21flip-ui-ab.py`):** boots the committed default build (own
  `native/build-agent-W2.1-flip/rb3-native`) 4x under `RB3_FIXED_CLOCK`: 2x flag-OFF (no env) +
  2x flag-ON (`RB3_PLACEMENT_CONTRACT=1`), navigating splash -> main_hub_screen (screenshot) ->
  song_select_screen depth 0 (screenshot). Computes a REGION-cropped pixel diff (not full-frame —
  full-frame is dominated by the animated char-preview-panel boot-random pose + venue-backdrop
  parallax, ~10-20% unrelated churn) over each UI element's own bounding box:
  - `HUB_BAR = (68,208,336,247)` — interior of the yellow highlight_main/highlight_pattern
    bar behind the focused main_hub item (tightened from an initial (55,195,350,260) pass that
    picked up ~16% A/A noise from the busy animated backdrop bleeding in at the corners).
  - `SCROLLBAR = (880,95,912,175)` — visible (non-album-art-occluded) sliver of the song_select
    scrollbar track + thumb at scroll depth 0.
  Reports A/A noise (OFF1-OFF2, ON1-ON2) and A/B diff (OFF1-ON1, OFF2-ON2 — max taken), PASS iff
  A/B stays within A/A-noise + 3pp margin AND below a 6% hard ceiling AND well clear (<5.5%) of the
  known ~11% "broken" (thumb-relocated-to-screen-center) signature from W2.1 S3's
  `SCROLLBAR_THUMB_FIX_OFF` refutation experiment.
- **Result: OVERALL PASS.** Both screens came back byte-identical in-region across all 4 captures:
  `aa_off=0.00% aa_on=0.00% ab_1=0.00% ab_2=0.00%` on BOTH mainhub and songselect — i.e. zero
  measurable pixel churn even at the A/A (same-flag, reboot-to-reboot) level inside these tight
  crops, and zero shift under the flag flip. This matches the mechanical expectation from S1/
  WAVE5_REVIEW A2: the contract arm explicitly excludes these meshes
  (`placementContractArm = sPlacementContract && skinned && !scrollbarThumb && !hubBarPlacement`,
  `Rnd_Wgpu_RB3.cpp:2878-2879`) via a mutually-exclusive if/else chain (`:2885-2909`), so flag-ON
  cannot touch UI-placement pixels by construction — this run empirically confirms it holds at the
  pixel level, not just in the branch logic. Visual spot-check (annotated PNGs) confirms the crops
  land on the intended elements (yellow "PLAY NOW" highlight bar; scrollbar thumb sliver next to
  the "RANDOM SONG" row) and are not degenerate/black frames.
- **Negative controls (PLAN step 3) — both GREEN:**
  - `HandsBindOracle` (`rb3-tests --gtest_filter=*HandsBind*`): 3/3 pass (ComposeIdentityAtBindPose,
    SkinnedVertsMatchAuthored, PerturbationIsDetected), 1 skip (RealPathFixture, no dumped
    fixture, pre-existing/unrelated). Flag-independent (pure math oracle, no env read).
  - `lineup-gate.py --bin native/build-agent-W2.1-flip/rb3-native`: PASS all layers
    (img/segA/ratioB/countC/pin) with no env (flag-OFF) AND with `RB3_PLACEMENT_CONTRACT=1`
    (flag-ON) — max_band_ratio 3.82 (OFF) vs 3.20 (ON), both within golden bound; no shard/sliver
    regressions either state.
- **Artifacts:** `docs/native/.../W2.1-flip/ui-ab/` — 4 raw + 4 annotated screenshots, 4 engine
  logs, `summary.json` (regions/thresholds/results/verdicts/overall=PASS).
- **DEVIATION (none material):** PLAN.md anticipated reusing `song-select-capture.py` +
  `placement-gate-capture.py` nav + `visual_diff.py`; instead wrote a self-contained nav (reusing
  `keyboard-to-gameplay.py`'s wait_screen/press/screenshot helpers directly) since the required
  nav (splash -> main_hub -> song_select, no scrolling) was simpler than either existing script's
  full path. Also swapped the perceptual `visual_diff.py` comparator for a direct region-cropped
  raw pixel diff (PIL/numpy) — more precise for a fixed, tight, pre-calibrated bounding box than a
  perceptual-hash-based whole-image comparator, and avoids re-deriving region semantics through an
  indirect tool. No plan-scope change, no source touched.
- **Exit:** S3 `ready-for-flip` half complete — no UI-placement regression from flag-ON,
  demonstrated on both screens with A/A discipline. Coordinator can rely on this as one of the
  four flip-readiness deliverables (S1 mechanism + S2 drum oracle + S3 UI gate + S4 Dolphin A/B).
