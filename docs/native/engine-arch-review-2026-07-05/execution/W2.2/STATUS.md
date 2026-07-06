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

## W2.2.S1b — done (numeric bind-pose identity oracle, fail-red)
Implementer: Opus. Build dir `native/build-agent-W2.2` (Clang). Commit `03f76e11`.

**Deliverable (all NEW, no shipped-source edits):**
- `native/tests/test_hands_bind_oracle.cpp` — the falsifiable invariant BandPatchMesh
  never had (SYS-7 hole). At the captured bind pose, `offset'=meshWorld·inv(restWorld)`
  (exactly `BandCharacter.cpp:1425-1428`) composed back with the bone
  (`offset'∘restWorld`) must reproduce `meshWorld`, and a mesh-local vert must skin to
  its authored world position. Uses the SAME production engine free funcs the rebake
  calls: `Multiply(Transform,Transform,Transform)` (row-vector "apply a then b",
  `math/Rot.cpp:732-740`) + `Invert(Transform,Transform)` (`math/Mtx.h:697`), linked via
  `_RB3_NATIVE_SRCS`.
- `native/CMakeLists.txt` — one source line added to the `rb3-tests` target list
  (`:718`; verified single-line diff, no sibling lines touched — the R5 cross-diff
  merge point).
- `native/tests/goldens/w2.2-hands/README.md` — documents the real-path fixture format
  + the exact in-game dump point for the S1a probe.

**Tests (4 in suite `HandsBindOracle`):**
1. `ComposeIdentityAtBindPose` — `skin==meshWorld` AND `skin∘inv(meshWorld)==I`; honors
   `HANDS_BIND_ORACLE_PERTURB` (fail-red).
2. `SkinnedVertsMatchAuthored` — 4 mesh-local verts incl. a long-thin **R~200u fingertip**
   to exercise the `R·sin(θ)` smear; honors the perturb env (fail-red).
3. `PerturbationIsDetected` — self-contained permanent guard (fixed 0.02rad wrong basis
   MUST exceed eps), so `kMatEps`(1e-3)/`kVertEps`(5e-2u) can never be loosened into
   BandPatchMesh-style blindness.
4. `RealPathFixture` — best-effort real-numbers arm; asserts the invariant on an
   S1a-dumped fixture (`native/tests/goldens/w2.2-hands/bind_fixture.txt`, override
   `HANDS_BIND_ORACLE_FIXTURE`) when present, else `GTEST_SKIP` (Risks §R3 fallback —
   invariant still checked on synthetic transforms).

**Gate evidence:**
- Unperturbed: `ctest -R HandsBindOracle` → 3 pass / 1 skip (RealPathFixture; fixture
  not yet dumped). `ComposeIdentityAtBindPose`, `SkinnedVertsMatchAuthored`,
  `PerturbationIsDetected` GREEN.
- **Fail-red proven:** `HANDS_BIND_ORACLE_PERTURB=0.15 rb3-tests` → both invariant tests
  RED; fingertip smears **28.87u** (≈ 200·sin(0.15)=29.7u — the R·sin(θ) failure mode),
  identity residual 0.897 vs 1e-3 eps.
- **No regression:** full `rb3-tests` = **73 pass / 2 skip / 0 fail** (the +2 pass +1 skip
  are exactly the new oracle).

**PLAN deviation (recorded per protocol):** the plan/brief assumed test rotations could
be built with the engine `Matrix3::RotateAboutX/Y/Z`. Those call the engine `Sine`, which
reads `gBigSinTable` filled by `TrigTableInit()` — NOT run in the standalone gtest process
(`Sine`→0 → singular test inputs). Fix: build INPUT rotations with host libm
(`std::sin/std::cos`) via explicit orthonormal matrices; the compose/invert **under test**
remain the production engine functions (trig-init-independent). No scope change — the
invariant and fail-red are unaffected.

**Remains / handoff:** RealPathFixture is armed but SKIPs until S1a (or a follow-up) dumps
a `bind_fixture.txt` from the in-game rebake point (format in the goldens README). This is
the plan's best-effort real-path arm; the numeric core is fully covered. No blockers.

## W2.2.S1c — done (hand-closeup capture harness incl. count-in/walkon window)
Implementer: Sonnet. Build dir `native/build-agent-W2.2` (Clang, reused from S1a/S1b,
no rebuild needed — harness is a pure Python HTTP-API driver, no source edits).

**Deliverables (all NEW, no shipped-source edits):**
- `scripts/native/hand-closeup-capture.py` — boots `rb3-native` headless (`RB3_HTTP=1`),
  reuses the exact frame-timed `RB3_GAME_INPUT` nav script from S1a/`song-end-test.py`
  (deterministic: `@N:verb` timestamps, not wall-clock) to reach `game_screen`, then pins
  a venue camera shot via the native-only DTA accessors from `band-closeup-capture.py`
  (`{rb3_director_disable 1}` + `{rb3_force_shot "<name>.shot"}` + `{rb3_cur_shot}`
  readback) so every captured frame is framed on the SAME hand-visible shot instead of
  whatever the auto-director happens to be showing. Tries `HAND_CLOSEUP_SHOTS` in
  priority order (guitar/bass hand-fret closeups first, then drums/vocals/keys);
  `coop_g_cg` -> `coop_g_cg.shot` resolved on every boot tested (club-family quickplay
  venue, same as `band-closeup-capture.py`'s default). Falls back to the free-running
  auto-director (manifest `shot_pinned: false`) if no candidate resolves in an
  unfamiliar venue — never hard-fails the run over camera pinning alone.
- `docs/.../W2.2/goldens/hand-closeup/` — committed golden reference set: 5 fixed-name
  PNGs (`handcloseup_countin.png`, `handcloseup_walkon.png`, `handcloseup_play_{00,01,02}.png`)
  + `manifest.json` (per-frame label/frame/songMs/screen/requested+resolved+readback
  shot/ok). `countin` = `game_screen` entry at songMs~0 (the window the W0.5 lineup gate
  does NOT frame — memory: "thin-geo count-in shards = pose-independent skinning
  residual"); `walkon` = mid-count-in (~2.5s in, clock-gated via `autohit` polling on
  `songMs`, NOT a wall sleep); `play_*` = 3 steady-play frames past note-start.

**Gate evidence:**
- Live run: pin held every single frame — `cur_shot` == `coop_g_cg.shot` at all 5
  captures, confirmed again by an explicit pin-held check after the last frame.
- **The committed `handcloseup_walkon.png` frame already shows a visible hand/finger
  shard**: during the count-in window the guitarist's hand/fingers render as detached,
  stick-like geometry projecting away from the wrist (compare to the clean fret-hand
  in `handcloseup_play_00.png` a few seconds later, same pinned camera angle). This is
  exactly the reviewer-judged evidence layer 3 of the W2.2 four-layer exit
  (WAVE3_REVIEW Amendment B1) needs for S3/S4, and independently corroborates S1a's
  CHARACTERIZATION.md finding of a marginal head/hand-region residual in the count-in
  window (this is visual confirmation of a real, camera-visible artifact — worth S2/S3
  scrutiny even though S1a scored it MARGINAL-GRAZE numerically, not HARD-SHARD).
- **Determinism check (ran the harness twice):** both runs produced the identical
  5-label set (`countin`/`walkon`/`play_00`/`play_01`/`play_02`), the same camera pin
  (`coop_g_cg` -> `coop_g_cg.shot`, held every frame both runs), and closely matched
  `songMs` windows per label (e.g. walkon 2523.7ms vs 2583ms — within the autohit
  polling granularity of the `advance_to_songms` clock gate). Per-pixel content is NOT
  byte-identical run-to-run (`visual_diff.py` shows 86-98% differing pixels) — this
  traces to quickplay randomizing the band member/outfit lineup on each boot, the
  SAME run-to-run characteristic S1a's CHARACTERIZATION.md already documented ("quickplay
  picks different characters/outfits run-to-run ... but the CLASSES are stable"). This
  is a pre-existing property of the shared nav path, not introduced by this harness;
  labels/camera-framing/timing are the stable, comparable axis across runs, matching
  the S1c exit bar ("frame-stable shots (same labels/positions)").

**PLAN deviation (recorded per protocol):** none material. One clarification: the plan's
step 1 mentions `RB3_WALKON_SNAP_OFF` "to A/B" the walk-on window — the harness does not
add a dedicated CLI flag for it; like every other native diagnostic toggle in this repo
(`SHARD_GUARD_OFF`, `RB3_NO_HEAD_REBIND`, etc.) it is read straight from the inherited
`os.environ` into the subprocess env, so `RB3_WALKON_SNAP_OFF=1 python3
scripts/native/hand-closeup-capture.py --out <dirB>` vs an unset run gives the A/B pair
without any script-side plumbing. Not run as part of this subtask (S1c's job is the
harness + golden set, not the A/B analysis itself — that's available to S2/S3 on demand).

**Verification commands:**
```
python3 scripts/native/hand-closeup-capture.py --verbose
# re-run to a scratch dir to confirm label/camera stability:
python3 scripts/native/hand-closeup-capture.py --out /tmp/hc-run2
```

**Remains / handoff:** none blocking. The golden set is ready for S3 (`S3_MEASURE.md`
reviewer-judged visual layer) and S4 (default-flip sign-off) to reference directly, and
the `handcloseup_walkon.png` finding should be read alongside S1a's CHARACTERIZATION.md
head/hand MARGINAL-GRAZE numbers when S2 scopes its targeted fix.

Commits: `8992549c` (rb3) — `git log --grep='W2.2.S1c'`.
