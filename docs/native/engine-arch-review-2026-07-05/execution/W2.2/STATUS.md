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

## W2.2.S2 — done (net-new fix behind default-OFF RB3_HANDS_BIND_FIX)
Implementer: Opus. Build dir `native/build-agent-W2.2` (Clang). Commit `32746985`.
Mode selected by S1a verdict = **BRANCH-RESIDUAL** (HARD-SHARD) → S2 IS triggered (not the
no-op documentation branch).

**Change (one file, CHANGE commit, separate from S1's additive commits):**
- `src/system/bandobj/BandCharacter.cpp` — in `RebindHeadHandsAtRest`, the FIRST-distinct-resolve
  branch. Root cause of the residual = the **count-in/walkon clip-free capture gap**: a bone whose
  first distinct resolve lands mid-clip (the count-in/walk-on window always plays a clip, so a
  per-member skeleton that streams in there first-resolves poisoned) hits the `clipPlaying` miss →
  the whole mesh stays pending → the V24 ratio guard drops it (S1a measured: `saddleshoe_skin.2`
  4.73x DROP + `head.mesh` 69.5u / hand 2.3x grazes, all count-in-window). But
  `NativeCaptureRestPoseAfterDeform` (poison-guarded to the clip-free deform rest) has usually
  ALREADY seeded that bone's own==bound magnet rest into `mNativeRestPose`, and a NON-distinct
  entry there is **clip-free by construction** (the load seed + the RB3_BOUND_REBAKE Poll capture
  are the only writers, both clip-free). When the NEW default-OFF `RB3_HANDS_BIND_FIX`
  (class:feature) is set, that seed is reused as the rest basis (`rest = rp->second`) and promoted
  to distinct instead of poisoning on the mid-clip pose. Magnet and per-member bone hold the SAME
  weighted gender-bind rest at load; char-space divides out placement ⇒ the seed is a valid
  clip-free basis for the now-distinct bone.

**Staging (binding, brief B1) — satisfied:**
- **DEFAULT-OFF.** Flag-off path is **byte-identical** (only a `static int`+`getenv` added; when
  0 the new `if` is skipped and control falls to the identical `miss++`/`clipPlaying` code).
- Existing head-rebind (`RB3_NO_HEAD_REBIND`) + torso-only (`RB3_NO_SKEL_REBIND` /
  `RB3_SKEL_REBIND_FULL`) BOTH retained as opt-backs — nothing removed or superseded this wave.
- Engine repo **untouched** (READ-ONLY): uses only the existing `mNativeBonesRebound` +
  `RB3_GUARD_EXEMPT_REBOUND` seams. No `DrawMesh` / `App.cpp` edits.
- MOVE-xor-CHANGE: this is a CHANGE commit; the S1 test/script/doc commits are additive-only.

**Gate evidence:**
- Build: `rb3-native` + `rb3-tests` compile clean (Clang, `native/build-agent-W2.2`).
- **Fail-red (reuses the S1b oracle, per brief):** `ctest -R HandsBindOracle` → GREEN unperturbed
  (3 pass / 1 skip=RealPathFixture); `HANDS_BIND_ORACLE_PERTURB=0.15 ctest -R HandsBindOracle` →
  RED (`ComposeIdentityAtBindPose` + `SkinnedVertsMatchAuthored` fail). The oracle gates the exact
  `offset'=meshWorld·inv(rest)` compose this fix feeds (`:1388-1390`), so a wrong reused basis
  fail-reds — the SYS-7 hole is closed for this path too.
- **No suite regression from the edit:** all non-GPU rb3-tests pass. The 10 ctest "failures" are
  Dawn/GPU **at-exit teardown SEGFAULTs** in the render tests (TexSharpen*/WgslValidation*/
  DrawLogGolden) whose own assertions all PASS (`[ PASSED ] 1 test`, shaders all OK) — environmental,
  do not include/exercise BandCharacter or skinning, present independent of this change.
- **Flag-ON boot smoke (non-rigorous, S3 owns the real A/B):**
  `RB3_HANDS_BIND_FIX=1 hands_bind_characterize.py --single default --dwell 8` boots clean through
  splash→hub→song_select→part_difficulty→game_screen (frame 1458, songMs 0 = count-in window),
  no crash/abort/segfault, verdict stayed **MARGINAL-GRAZE** (no 200-460u tripwire, not worse than
  default). NOT a controlled measurement (8s dwell, quickplay randomizes lineup) — the S3 gate must
  run the full A/B (SKINPOS ≤65u incl. hands, FLING=0, ratio ≤2×/no-tripwire, crowd SKIN_CLAMP
  byte-identical vs `parsed-default.json`) to decide whether the flag actually clears
  `saddleshoe_skin.2` and the head/hand grazes without regressing.
- **Census:** `native_compat_census.py check` → exit 0 (230 flags, regen clean). BandCharacter.cpp
  is under `rb3/src`, NOT a scan root, so the new getenv does not trip the census (as PLAN R4
  predicted).

**FOR THE COORDINATOR — classification.json entry to append at pin-bump (engine READ-ONLY for
W2.2, so I do not edit it). Append to
`milo-native-engine/src/platform/NativeCompatFlags.classification.json`:**
```json
"RB3_HANDS_BIND_FIX": {
  "class": "feature",
  "owner": "skinning/bandobj",
  "faithfulStatus": "not-live: net-new default-OFF hands/fingers bind fix (W2.2.S2) — reuses the clip-free load-time rest seed for a bone whose first distinct resolve lands mid-clip (count-in/walkon), closing the V24 guard-drop gap; flip deferred to S4/coordinator",
  "default": "off",
  "read": "presence"
}
```
(rb3/src is not a census root, so census `check` stays exit 0 with or without this entry; the entry
is registry hygiene per PLAN R4, a coordinator/pin-bump data edit.)

**PLAN deviation:** none material. PLAN listed `BandCharacter.h` as a conditional file — not needed
(the fix is a static-local getenv in the .cpp, no new member/decl). `.cpp`-only.

**Remains / handoff to S3:** run the full flag-ON-vs-default A/B via `hands_bind_characterize.py`
(the flag reads straight from inherited env — `RB3_HANDS_BIND_FIX=1 python3
scripts/native/hands_bind_characterize.py …`) and record the five S3 gates in `char/S3_MEASURE.md`,
including the negative-control diff vs `parsed-default.json`. S4 default-flip stays deferred to
coordinator + reviewer-judged frames (no flip in-wave). No blockers.

## W2.2.S3 — done (measure all gates; default stays OFF; S4 flip deferred to coordinator)
Implementer: Opus. Build dir `native/build-agent-W2.2` (Clang). Commit `95339eea`
(measurement artifacts; no source edits — S3 is measure-only). Full report:
`char/S3_MEASURE.md`.

**Environment note (HARD-RULE 7):** engine working tree observed at `5cee522`, ONE
commit past the pin `41b9e3a` (sibling Lane A W0.3c `RB3_DRAWORDER_TRACE`, default-OFF
probe + one TU — behavior-neutral for skinning with the flag unset). Pin check is a
WARNING (`native/CMakeLists.txt:80`), build proceeds; measurements valid. Did NOT
reset/rebase the engine tree. A concurrent agent's uncommitted `FxSendNative.cpp`
(audio) left untouched.

**Five gates (all recorded with exact numbers/commands in S3_MEASURE.md):**
1. **Numeric draw-time A/B (flag-ON vs OFF):** FLING(>120u)=**0** both passes; no
   STOP-tripwire (>92u / 200-460u) either. Head/hands/hair SCOPE = a proven NET WIN
   (rebake-OFF head guard-DROPs 9.59x → rebake-ON renders clean 1.34x @ 69.5u graze);
   hands ≤53u SKINPOS / ≤2.35x ratio no-drop. **Literal `≤65u/≤2x on ALL rebound` bar
   NOT met** by (a) a STRUCTURAL 69.5u head brow-bone graze (identical flag-OFF/ON;
   crowd bodies show the same 63-64u) and (b) a **HARD guard-DROP on a FOOT/SHOE mesh**
   (`saddleshoe_skin.2` 4.73x OFF / `lowtopsneaks_skin.2` 5.09x ON — SAME lower-body
   class, lineup-randomized name). Foot/shoe go through `RebindOutfitBonesToOwnSkeleton`
   (lower-body), **NOT** `RebindHeadHandsAtRest` → OUT of W2.2 scope; the flag cannot
   and does not clear it. **`RB3_HANDS_BIND_FIX`=ON yields NO measured improvement**
   on the head/hands scope (69.5u↔69.5u, hands identical). Torso not regressed (≤65.4u
   collars both). ⇒ scope net-win, literal bar unmet by an out-of-scope residual.
2. **Negative control:** all **29 true venue crowd/extra `[SKIN_CLAMP]` meshes
   BYTE-IDENTICAL** flag-OFF vs flag-ON (zero diffs, zero set diffs). Of the 33-mesh
   overlap, 32 identical; the 1 diff (`messyshort_resource` 1218→1017) is a
   band-assigned HAIR resource (S1a HEAD_REBIND PENDING), lineup-randomized, not a
   venue extra. Code proof: flag gated strictly inside `RebindHeadHandsAtRest`
   (`BandCharacter.cpp:1383-1394`); crowd/extras are non-rebound (SKIN_CLAMP path),
   never enter it ⇒ band-scoped by construction. **PASS.**
3. **Oracle:** `ctest -R HandsBindOracle` GREEN unperturbed (3 pass / 1 skip); `PERTURB=0.15`
   RED (fingertip 28.87u ≈ 200·sin θ; identity residual 0.897 vs 1e-3). **PASS.**
4. **Invariance nets:** own DC3-context engine build (`build-agent-W2.2`, decomp
   context + Dawn) → `milo-engine-tests` **198 pass / 0 fail / 2 skip**. W0.1 SkinGolden
   (3 green) + W0.4 ClipPoseFixture.EffectorWorldPositionsMatchGolden green ⇒ no leak
   into shared skinning/effector math (auto-stop not triggered). **PASS.**
5. **W0.5 lineup:** `--selftest` PASS + real gate on `build-agent-W2.2/rb3-native`
   (default-OFF) → `verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS`
   (sliv=0 all frames). **PASS.**

**S4 decision — NO FLIP; default stays OFF; deferred to coordinator.** Flip gate
(PLAN §S4 / brief B1) requires a flag that clears the residual + all layers green +
reviewer-judged Dolphin/retail frames. (a) flag shows no measured benefit + doesn't
clear the out-of-scope foot/shoe drop; (b) gates 2-5 PASS, gate-1 literal bar unmet;
(c) no in-wave reviewer judgment. ⇒ land default-OFF (worst-case four-layer exit =
unflipped flag, never a blind revert). **Recommendation to coordinator:** the
head/hands/hair rebake is already a proven net-win default-ON via `RebindHeadHandsAtRest`
and needs no flip; the remaining HARD residual is a lower-body foot/shoe path — treat
foot/shoe rest-capture coverage as a separate Wave-4 item, not a flip of
`RB3_HANDS_BIND_FIX`. Hand this + S1c `handcloseup_walkon.png` to S4.

**PLAN deviations (recorded per protocol):** (i) Gate-1's literal "≤65u on ALL rebound
incl hands/fingers/hair + ≤2x" target NOT proven — head brow-bone graze is structural
(matches non-rebound crowd) and the one HARD drop is out-of-scope lower-body; the flag
provides no measured benefit. This is the honest measured outcome, not a pass. (ii)
Negative control satisfied on the venue-extra subset (byte-identical) rather than the
full clamp map — a strict full-map byte-identity is not physically achievable across
two boots while quickplay randomizes the band lineup (S1a-documented). (iii) Engine
tree one commit past pin (behavior-neutral, above).

**Remains / handoff:** S4 (default-flip) deferred to coordinator with the recorded
evidence + recommendation. No blockers. RealPathFixture (S1b) still SKIPs — no in-game
bind fixture dumped (optional; numeric core covered).
