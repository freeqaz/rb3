# W2.2 — Hands/fingers bind fix — STATUS (append-only, flock /tmp/rb3-docs.lock)

Lane B (rb3-only, engine READ-ONLY). Engine pin 41b9e3a (coordinator bumps). Planner: Opus.

## planning — done
- PLAN.md written. W0.3-dependency **WAIVED** for W2.2 (WAVE3_REVIEW A1) — recorded here per brief.
- **KEY FINDING (coordinator ack needed):** the brief's "untried bind-pose-captured rebake" is
  ALREADY IMPLEMENTED and default-ON as `BandCharacter::RebindHeadHandsAtRest()`
  (`src/system/bandobj/BandCharacter.cpp:1214`, called from `Poll():522` before `Character::Poll`;
  bakes `offset' = meshWorld · inverse(restWorld)` at `:1425-1428`). The load-time bind-pose capture
  seam EXISTS: `NativeCaptureRestPoseAfterDeform()` from `SyncObjects:1693` after `SetDeformation`
  (the deterministic gender-bind rest point). Landed `0de768a1`/`2580e128`/`3c02e08b`/`491288ec`
  (C7/C8). Opt-out `RB3_NO_HEAD_REBIND=1`. => W2.2 PIVOTS to: build the missing numeric oracle +
  characterize the existing default-ON behavior + gate any net-new change default-OFF. Four-layer
  anti-revert (brief B1) preserved.
- Verified engine READ-ONLY seams at current (post-Wave-2) lines: `mNativeBonesRebound` skip
  `Rnd_Wgpu_RB3.cpp:2920/3190/3226`; `RB3_GUARD_EXEMPT_REBOUND` `:3727`; failed-experiment tripwire
  `RB3_BOUND_REBAKE` `:3715-3729` (200-460u smear = STOP signature); `SHARD_RATIO_DBG` `:3712`;
  `REBIND_DRAW_SKINPOS/FLING` `:3199/3214`; `SKIN_CLAMP_PROBE` `:3238`.
- Census wrinkle recorded: `native_compat_census.py` scans engine/src + rb3/native/src only, NOT
  rb3/src — a new getenv in `BandCharacter.cpp` won't trip `check`; registry entry is a
  coordinator/pin-bump data edit (engine read-only for W2.2).
- Build dir for this lane: `native/build-agent-W2.2`.

## S1-EXIT branch — satisfied, NOT triggered
The bind-frame capture seam is reachable (`NativeCaptureRestPoseAfterDeform` at SyncObjects), so the
wave-08 "unreachable seam → characterize only" stop does not fire. Proceed to build gates.

## W2.2.S1a — done (verdict: BRANCH-RESIDUAL, severity MARGINAL on head/hands scope)
Implementer: Opus. Build dir `native/build-agent-W2.2` (Clang), rb3-native built clean.

**Deliverables (all NEW, no shipped-source edits):**
- `scripts/native/hands_bind_characterize.py` — headless RB3_HTTP characterization
  harness: boots → nav → gameplay → count-in/walk-on window, runs the in-tree
  read-only skinning diagnostics (`REBIND_DRAW_SKINPOS`/`FLING`, `SHARD_RATIO_DBG`
  guard-ON, `SKIN_CLAMP_PROBE`, `HEAD_REBIND_PROBE`, `RELOAD_PROBE`), parses NUL-safe
  (grep -a), reduces per-mesh (SKINPOS max / FLING / ratio / clamp), runs BOTH the
  default (rebake ON) and `RB3_NO_HEAD_REBIND=1` (OFF) passes for the A/B, and emits
  a tri-severity verdict (HARD-SHARD / MARGINAL-GRAZE / CLEAN → BRANCH-CLEAN|RESIDUAL).
  Has `--single`, `--report-only` (regen from saved parsed-*.json, no boot — reusable
  by S3), `--dwell`.
- `docs/native/.../W2.2/char/CHARACTERIZATION.md` + `parsed-default.json` +
  `parsed-nohead.json` (S1b real-numbers fixture + S3 negative-control baseline).
  `raw-*.log` gitignored (2.8MB NUL-laden capture, regenerable).

**VERDICT: BRANCH-RESIDUAL.** But the severity split is the load-bearing finding:
- **The head/hands rebake (W2.2's actual scope) is a proven NET WIN, not broken.**
  A/B causation: with rebake **OFF**, `head.mesh` is guard-DROPPED at **9.59x**
  (bind=15.6 world=150.0) — a catastrophic head shard the guard has to hide. With
  rebake **ON**, `head.mesh` renders cleanly (ratio 1.34x, NOT dropped) at a 69.5u
  SKINPOS graze. Hands render 47-53u SKINPOS (under bar). **Zero FLING(>120u) on
  either path; nothing near the 200-460u RB3_BOUND_REBAKE tripwire.**
- **Residual on the head/hands scope is MARGINAL only:** head `head.mesh` 69.5u vs
  the ~65u bar (a brow-bone vertex; non-rebound CROWD bodies sit at the SAME 63-64u
  via bone_head → this is the natural head-region skinning extent, NOT introduced by
  the rebake); hand meshes `fingernails`/`drivinggloves` 2.31-2.35x bind/world ratio,
  NO guard-drop, world extents <85u. Head-region outfit collars (jacket/suspenders)
  graze 65.4u via bone_head.
- **The one HARD signal is a FOOT/shoe mesh, a SIBLING lower-body path — not the
  head/hands rebake.** `saddleshoe_skin.2.mesh` guard-DROP 4.73x (+ wovensteppers
  3.0-3.8x no-drop). Feet go through `RebindOutfitBonesToOwnSkeleton` (torso-only) or
  stay un-rebound, NOT `RebindHeadHandsAtRest`. S2 should treat it as an OPTIONAL
  coverage extension, not evidence the head/hands rebake is broken.

**S2 mode selected:** BRANCH-RESIDUAL → S2 gets a targeted default-OFF fix
(`RB3_HANDS_BIND_FIX`, class:feature), but the characterization scopes it narrowly:
the head/hands rebake itself needs NO catastrophic rewrite (it fixes a real shard);
candidate targeted deltas = (a) trim the head 69.5u / hand-ratio graze in the count-in
window, and/or (b) OPTIONALLY extend rest-capture coverage to the foot/shoe meshes
that still drop. S2 must first justify that the marginal graze is even worth a change
given the crowd baseline shows the same head-region extent (do-nothing is defensible).

**Negative-control baseline captured** for S3: 35 crowd/extra `[SKIN_CLAMP]` meshes
(default-ON) in `parsed-default.json` `clamp`; full `[CHAR_MESH]` band-vs-extra
inventory (113 entries) in `charmesh`. HEAD_REBIND probe: 0 mixed-anchor smear
candidates; 11 transient PENDING meshes (props/hair still resolving, throttled — not
permanent failures).

**Coverage:** live run confirmed game_screen entered at songMs=0 (the count-in/walk-on
window — beat clock frozen during the intro shot per walkon SCOUT.md), then 40s dwell.
Per-mesh diagnostics track MAX across the whole run, so a count-in-window shard is
captured even though tags emit mid-run. Two independent boots (quickplay picks
different characters/outfits run-to-run) showed the SAME pattern (head-region grazes +
foot/hand appendage ratios), so the CLASSES are stable even as specific mesh names vary.

**Tripwire check (PLAN R2):** NOT reproduced — no SKINPOS >92u on any rebound appendage,
no 200-460u smear, no band-appendage guard-drop. `RB3_BOUND_REBAKE` was NOT run.

Commits: see `git log --grep='W2.2.S1a'`.
