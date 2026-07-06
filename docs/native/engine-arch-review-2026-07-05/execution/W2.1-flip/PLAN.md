# W2.1-flip — Flip-readiness package for `RB3_PLACEMENT_CONTRACT`

**Wave 5, Lane A, item 2** (after `W0.3d-fix`, before `W3.1a`). Planner: Opus.
Engine pin at plan time: `609efb7` (brief) — engine HEAD in the shared tree has since
advanced to `1fd2bfc` (W0.6b landed concurrently). **Do NOT bump the pin.**

## Objective

Prepare **everything** needed to ship the crowd/drum-kit-at-one-point placement fix
(`RB3_PLACEMENT_CONTRACT`, the SYS-1 fix landed in Wave 4 default-OFF), but **do NOT change
the default and do NOT re-golden.** The coordinator flips the default and re-goldens after
reviewing the Dolphin A/B and human sign-off (E1). This item's exit is
`exitReached = 'ready-for-flip'`.

The four deliverables (WAVE5_KICKOFF COORDINATOR ACCEPTANCE + WAVE5_REVIEW, authoritative):

1. **Flag mechanism (A3)** — the current read is presence-truthy so a naive default flip ships
   with no opt-out. Refactor to a registered `RB3_PLACEMENT_CONTRACT_OFF` opt-out such that the
   coordinator enables-by-default with a **one-line** change and the opt-out disables it (fail-red).
   Leave the **effective default OFF** (coordinator flips).
2. **Drum oracle (A5)** — W2.1's placement oracle only checks `kind=="crowd"`. Implement the
   reserved `kind=="drum"` assertion (drummer prop-mesh bone/waypoint world ≠ origin AND consistent
   with the drummer's placement). Wire into `placement-gate-capture.py` / `test_placement_oracle`.
   Fail-red on the current (default-OFF) build.
3. **UI-placement regression gate (A2)** — pre/post-flip default-build A/B with A/A pairs on
   BOTH song_select (scrollbar thumb) AND main_hub (hub bar — `highlight_main`/`highlight_pattern`
   live in main_hub, NOT song_select). Prove flag-ON does **not** regress the UI-placement family
   (expectation: byte-identical UI, since the contract arm excludes those meshes).
4. **Dolphin A/B package (E1)** — camera-pinned gameplay A/B (≥2 captures per flag state, A/A
   protocol) vs `dolphin-shots/gp_*.png` + retail, written to `.../W2.1-flip/dolphin-ab/` for
   coordinator human-eyes review.

**NO subsumption step (A1) — VERIFIED in source.** The contract arm explicitly excludes the UI
hacks and the branch chain is mutually exclusive (see citations) — there is **no double-apply**.
The name-scoped hacks stay **co-active/retained**. Do not attempt to prove them "subsumed" or
"no-op": the contract is provably vertex-invariant and can never fix the vertex-broken
hub-bar/scrollbar meshes — that is a permanent property, not open work.

## Faithful citations (re-grepped 2026-07-06; use these, not the stale doc line numbers)

- **Contract read (presence-truthy), engine `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`:**
  - `:2874-2875` — `static int sPlacementContract = -1; if (sPlacementContract < 0) sPlacementContract = getenv("RB3_PLACEMENT_CONTRACT") ? 1 : 0;`
  - `:2878-2879` — `bool placementContractArm = sPlacementContract && skinned && !scrollbarThumb && !hubBarPlacement;` (**the exclusion — hacks are NOT double-applied**).
  - `:2884` — `if (placementContractArm) contractMeshWorld = mesh->WorldXfm();`
  - `:2885-2909` — the mutually-exclusive `obj.world` if/else chain: `scrollbarThumb` (`:2885`) → `hubBarPlacement` (`:2887`) → general skinned w/ contract inside (`:2892-2900`) → non-skinned (`:2902`); `worldInvTranspose` at `:2909`.
  - Additional `placementContractArm` uses (palette Half-B) at `:3266`, `:4022` — the flag read is a **single static**; refactoring the read at `:2874-2875` is the only change needed (all sites read `placementContractArm`, which derives from `sPlacementContract`).
- **UI hacks (rb3, relocated by W1.7), `native/src/rb3_render_hook.cpp`:** `RB3_NO_HUB_BAR_PLACEMENT_FIX`
  read `:77` → `p.hubBarPlacement`; `RB3_SCROLLBAR_THUMB_FIX_OFF` read `:101` → `p.scrollbarThumb`.
  `RB3_NO_CROWD_REBIND` is the load-path rebind at rb3 `src/system/world/Crowd.cpp:932` (not the draw path).
- **Crowd placement probe (the S1 oracle reference), rb3 `src/system/world/Crowd.cpp`:** `curChar->SetWorldXfm(spXfm)` at `:403`; `RB3_PLACEMENT_PROBE`-gated fprintf at `:421-427` emitting `RB3_PLACEMENT_PROBE crowd inst=%u x= y= z=`.
- **Band-member (incl. drummer) placement, rb3 `src/system/bandobj/BandConfiguration.cpp:43-70`:**
  `SyncPlayMode()` — for each of 4 slots `mXfms[i].mWay->SetLocalXfm(curtargxfm.xfm)`, resolve
  `bchar = TheBandWardrobe->FindTarget(targName,...)`, `bchar->Teleport(mXfms[i].mWay)`. The
  W2.5 `HX_NATIVE` unresolved-waypoint `MILO_WARN` is at `:54-66`. **This is the drum reference
  site** — `mXfms[i].mWay->WorldXfm().v` is the faithful band-member placement the drum kit is
  bone-attached to. rb3-only, `HX_NATIVE`-guarded, Wii-inert.
- **Oracle, rb3 `native/tests/placement_oracle.h`:** `PosedInstance.kind` ("crowd"|"drum", `:87-92`);
  `RunPlacementOracle` filters `if (p.kind == "crowd")` at `:~230` (**drum reserved but dropped**);
  drawn set = skinned draws' `obj.world[12..14]`; checks (A)ref-sanity/(B)coverage/(C)span/(D)clusters.
- **Test, rb3 `native/tests/test_placement_oracle.cpp`:** synthetic fail-red + `PlacementOracle.RealCaptureSpansBowl` live gate (env `RB3_PLACEMENT_DRAWLOG` + `RB3_PLACEMENT_PROBE_LOG`).
- **Capture harness, rb3 `scripts/native/placement-gate-capture.py`:** boots headless (`RB3_DRAWLOG` + `RB3_PLACEMENT_PROBE` + `RB3_FIXED_CLOCK`), navigates to gameplay, pins a wide shot, GETs `/api/drawlog`, extracts probe lines, runs the oracle.
- **Flag registry, engine `src/platform/NativeCompatFlags.classification.json`:** `RB3_PLACEMENT_CONTRACT` at `:91` (`class:feature, default:off, read:presence`); `RB3_SCROLLBAR_THUMB_FIX_OFF:59`, `RB3_NO_HUB_BAR_PLACEMENT_FIX:69`, `RB3_PLACEMENT_PROBE:180`. Convention for opt-outs: `..._OFF` (cf. `RB3_TRACK_LIGHT_OFF:42`, `RB3_VENUE_LIGHT_OFF:43`).
- **Ground truth (brief path was stale):** Dolphin gameplay shots at
  `docs/native/c8-ground-truth-2026-07-01/dolphin-shots/gp_*.png` (`gp_00.png` shows the crowd
  spread house-left); retail drum-kit position `images/retail-screenshots/fandom_gameplay_drums.png`,
  guitar `.../fandom_gameplay_guitar.png`. Camera hooks `rb3_force_shot` / `rb3_director_disable`
  registered at `native/src/rb3_http_handlers.cpp:910-911`.
- **Lane predecessors already landed:** W0.3d-fix (rb3 `b9bd33f3`, `src/system/rndobj/Utl.cpp:192`
  deterministic tiebreak under `RB3_FIXED_CLOCK`, opt-out `RB3_DRAWSORT_DETERMINISTIC_OFF`);
  W0.6b (engine `1fd2bfc`, rb3 `3937514b`).

## Flip-mechanism design (S1) — the coordinator's future one-line flip

The read must become a **default + opt-out** pair so the coordinator flips with one line and the
shipped default keeps a demonstrable fail-red. Recommended shape at `Rnd_Wgpu_RB3.cpp:2874-2875`:

```cpp
// Effective default OFF this item (coordinator flips kDefaultOn to 1 at E1 sign-off).
static const int kPlacementContractDefaultOn = 0;   // <-- coordinator's ONE-LINE flip -> 1
static int sPlacementContract = -1;
if (sPlacementContract < 0) {
    if (getenv("RB3_PLACEMENT_CONTRACT_OFF"))      sPlacementContract = 0;   // opt-out wins
    else if (getenv("RB3_PLACEMENT_CONTRACT"))     sPlacementContract = 1;   // legacy opt-in kept
    else                                           sPlacementContract = kPlacementContractDefaultOn;
}
```

Properties this must satisfy (verify each):
- **Effective default OFF this item:** with no env set and `kPlacementContractDefaultOn == 0`,
  `sPlacementContract == 0` — byte-identical to today's default-OFF path.
- **Coordinator flip = one line:** flip `kPlacementContractDefaultOn` `0 → 1`.
- **Opt-out works post-flip:** with `kPlacementContractDefaultOn == 1`, `RB3_PLACEMENT_CONTRACT_OFF`
  set → `sPlacementContract == 0` → contract inactive (placement oracle RED = the fail-red).
- **Legacy opt-in preserved:** `RB3_PLACEMENT_CONTRACT` still enables it (all existing capture
  scripts + the W2.1 oracle harness set this — must keep working unchanged).
- **Opt-out precedence:** `_OFF` beats `RB3_PLACEMENT_CONTRACT` when both set (fail-red must win).

## Drum-oracle design (S2)

- **Reference (probe):** at `BandConfiguration::SyncPlayMode` (rb3, `HX_NATIVE` + `RB3_PLACEMENT_PROBE`
  gated), after `Teleport`, emit `RB3_PLACEMENT_PROBE drum inst=<slot> x= y= z=` from
  `mXfms[i].mWay->WorldXfm().v` for each **resolved** band member. This is the faithful band
  placement the drum kit (and other bone-attached props) hang off. Reuse the existing
  `RB3_PLACEMENT_PROBE` flag — **no new flag, no classification.json edit for S2.**
- **Assertion (`kind=="drum"` branch in `placement_oracle.h`):** distinct from crowd's spread test.
  A single kit is not a spread bowl, so assert:
  - (a) **reference non-origin** — ≥1 drum reference with `Radius > originRadius`; else INCONCLUSIVE
    (this venue placed the band at origin — neither pass nor bug).
  - (b) **drawn consistency** — ≥1 **skinned** draw whose `obj.world[12..14]` is non-origin
    (`Radius > originRadius`) AND within `drumEps` of a drum reference. On the current default-OFF
    build every skinned `obj.world` is identity → **zero non-origin skinned draws → RED**
    (`kDrumAtOrigin`). Flag-ON, the bone-attached kit prop carries `meshWorld` near the drummer → GREEN.
  - The RED/GREEN discriminator is build-independent and does not depend on tight eps tuning: the
    flag-OFF build has **zero** non-origin skinned draws, so any reasonable `drumEps` goes RED;
    `drumEps` only needs to be generous enough (kit sits offset in front of the drummer) to admit
    the prop flag-ON. Recommend `drumEps` ≈ a few band-space units; tune against a real flag-ON capture.
- **VERIFY the kit prop is `skinned` in the drawlog.** The W2.1 verifier reported bone-attached
  instrument props among the 28/79 non-origin flag-ON meshes, so they are in the contract/skinned
  arm — but confirm on a live flag-ON capture before relying on the drawn-consistency check. If the
  kit prop is NOT flagged `skinned`, fall back to the drummer-body draw or file the drum draw
  predicate as debt (do NOT silently drop the drum kind — WAVE5_REVIEW A5).
- **Wiring:** extend `placement-gate-capture.py` to run BOTH crowd (`RealCaptureSpansBowl`) and a
  new drum gtest (`PlacementOracle.RealCaptureDrumPlaced`) over the same capture; synthetic
  fail-red tests in `test_placement_oracle.cpp` (drum-at-origin frame → RED, drum-at-drummer → GREEN,
  drum-ref-at-origin → INCONCLUSIVE).

## Subtasks

### W2.1-flip.S1 — Flag mechanism: `RB3_PLACEMENT_CONTRACT_OFF` opt-out (effective default STILL OFF)
- **model:** opus
- **goal:** Refactor the presence-truthy read into a default+opt-out pair so the coordinator can
  flip default-ON with a one-line change and `RB3_PLACEMENT_CONTRACT_OFF` disables it (fail-red).
  Leave the effective default OFF. Register the opt-out in classification.json (append-only, flock,
  NO regen).
- **files:**
  - engine `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (`:2874-2875` read only)
  - engine `milo-native-engine/src/platform/NativeCompatFlags.classification.json` (append 1 row)
- **steps:**
  1. Replace the `:2874-2875` read with the default+opt-out shape above; `kPlacementContractDefaultOn = 0`.
     Keep the legacy `RB3_PLACEMENT_CONTRACT` opt-in working; `_OFF` takes precedence.
  2. Under `flock /tmp/milo-engine-classjson.lock`, **append** a `RB3_PLACEMENT_CONTRACT_OFF` row
     (`class:feature, owner:render/placement, read:presence, default:off`, faithfulStatus noting
     "opt-out for the SYS-1 placement contract; disables it when the contract is default-ON").
     **Do NOT run gen.inc regen** (coordinator regens once at wave end).
  3. Under `flock /tmp/milo-engine-git.lock`, commit engine changes (prefix `W2.1-flip:`), staging
     ONLY these two files.
- **verify:**
  - Build engine in own dir: `cmake -B native/build-agent-W2.1-flip -S native -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ && cmake --build native/build-agent-W2.1-flip --target rb3-native`.
  - **Default byte-identity:** no env set → canonical splash draw-log golden green
    (`python3 scripts/native/drawlog-golden.py --canonical-order --fixed-clock` → 888) and
    `scripts/native/lineup-gate.py` PASS all layers (proves flag-OFF path unchanged).
  - **Legacy opt-in still enables:** `RB3_PLACEMENT_CONTRACT=1 python3 scripts/native/placement-gate-capture.py --bin native/build-agent-W2.1-flip/rb3-native --tests native/build-agent-W2.1-flip/rb3-tests` → oracle GREEN (unchanged from Wave 4).
  - **One-line-flip + opt-out fail-red rehearsal (do NOT commit the flip):** locally set
    `kPlacementContractDefaultOn = 1` in a scratch build, then
    `RB3_PLACEMENT_CONTRACT_OFF=1 ... placement-gate-capture.py --expect-red` → exit 0 (opt-out
    forces the oracle RED). Revert the scratch edit; the committed default stays `0`.
  - census still exit 0: `python3 scripts/analysis/native_compat_census.py check` (row present, no regen).
- **fail-red/staging:** effective default OFF; MOVE-xor-CHANGE (this is a CHANGE commit, default-neutral);
  classification.json append-only under flock, no regen.

### W2.1-flip.S2 — Drum oracle (`kind=="drum"`) + probe + fail-red
- **model:** opus
- **goal:** Implement the reserved drum-kind assertion (drummer bone/waypoint world ≠ origin AND
  drawn kit consistent with it), a `RB3_PLACEMENT_PROBE`-gated drum reference at `SyncPlayMode`, and
  wire both into the capture harness + tests. Fail-red on the current default-OFF build.
- **files:**
  - rb3 `src/system/bandobj/BandConfiguration.cpp` (drum probe in `SyncPlayMode`, `HX_NATIVE`+`RB3_PLACEMENT_PROBE`)
  - rb3 `native/tests/placement_oracle.h` (drum-kind branch + `kDrumAtOrigin`/`kDrumRefAtOrigin`)
  - rb3 `native/tests/test_placement_oracle.cpp` (synthetic drum fail-red + `RealCaptureDrumPlaced` live gate)
  - rb3 `scripts/native/placement-gate-capture.py` (run the drum gtest over the same capture)
  - (NO classification.json edit — reuses `RB3_PLACEMENT_PROBE`)
- **steps:**
  1. Add the `HX_NATIVE`+`RB3_PLACEMENT_PROBE` drum emission after `Teleport` in `SyncPlayMode`
     (`mXfms[i].mWay->WorldXfm().v`, `kind=drum inst=<slot>`), mirroring the Crowd.cpp probe. Wii-inert.
  2. Add a `RunDrumOracle` (or extend `RunPlacementOracle` with a `kind` param) implementing (a)+(b)
     above; new `FailureKind`s; INCONCLUSIVE when no non-origin drum reference.
  3. Synthetic tests: drum-at-origin frame → RED; drum-drawn-at-drummer → GREEN; ref-at-origin →
     INCONCLUSIVE (committed fail-red demonstrations). Add live `RealCaptureDrumPlaced`.
  4. Extend `placement-gate-capture.py` to run the drum gtest and report RED/GREEN alongside crowd.
  5. **VERIFY** on a flag-ON capture that the kit prop draws with `skinned=true`; if not, adjust the
     drawn predicate (see design note) and record the finding in STATUS.md.
  6. Commit under `flock /tmp/rb3-git.lock` (prefix `W2.1-flip:`), staging ONLY these rb3 files.
- **verify:**
  - `cmake --build native/build-agent-W2.1-flip --target rb3-tests`; synthetic drum tests pass
    (`--gtest_filter=PlacementOracle.*Drum*`).
  - **Fail-red (current default-OFF build):** `python3 scripts/native/placement-gate-capture.py
    --bin ... --tests ...` → drum oracle **RED** (kit at origin) — exit-1 is the gate working.
  - **GREEN flag-ON:** `RB3_PLACEMENT_CONTRACT=1 ... placement-gate-capture.py` → drum oracle GREEN.
- **fail-red/staging:** rb3-only, `HX_NATIVE`-guarded (Wii compile untouched); probe default-OFF;
  no engine/classification edit; drum RED on current build is the required proof.

### W2.1-flip.S3 — UI-placement regression gate (song_select scrollbar + main_hub hub bar, A/A)
- **model:** sonnet
- **goal:** Prove flag-ON does NOT regress the UI-placement family. Default-build pre/post-flip A/B
  with A/A pairs on BOTH song_select (scrollbar thumb) and main_hub (hub bar). Since the contract arm
  excludes these meshes (`:2878-2879`), the expectation is byte-identical UI — verify it.
- **files:**
  - rb3 `scripts/native/` (new `w21flip-ui-ab.py` capture/compare harness; may reuse
    `song-select-capture.py` + `placement-gate-capture.py` nav + `visual_diff.py`)
  - artifacts → `docs/native/engine-arch-review-2026-07-05/execution/W2.1-flip/ui-ab/`
- **steps:**
  1. Capture song_select (scrollbar thumb on the right-edge track) and main_hub (yellow highlight bar
     behind the focused item) at fixed camera/nav, `RB3_FIXED_CLOCK`.
  2. For each screen: A/A pair flag-OFF (2 caps) + A/A pair flag-ON (`RB3_PLACEMENT_CONTRACT=1`, 2 caps).
     Compute region-diff. **Thresholds (S3-measured):** scrollbar broken ≈11% / thumb at screen-center
     vs ≈2% A/A noise; flag-ON diff must sit in the A/A-noise band (no shift toward the broken signature).
  3. Negative controls: `HandsBindOracle` + `lineup-gate.py` PASS both flag states.
  4. Write a PASS/FAIL summary + the annotated images to `ui-ab/`.
- **verify:** flag-ON minus flag-OFF region diff ≤ A/A noise on both screens (hub bar NOT visible in
  song_select — main_hub capture required); controls green.
- **fail-red/staging:** READ-ONLY on source; uses the committed default build + `RB3_PLACEMENT_CONTRACT=1`;
  no default change. Scripts + doc artifacts only.

### W2.1-flip.S4 — Dolphin gameplay A/B package (E1, for coordinator sign-off)
- **model:** sonnet
- **goal:** Produce the camera-pinned gameplay A/B package the coordinator eyes before flipping:
  ≥2 captures per flag state, A/A protocol (the pre-existing A/A-variable pink bloom wash confounds
  single-frame judgment), vs `dolphin-shots/gp_*.png` + retail. Package only — do NOT flip.
- **files:**
  - rb3 `scripts/native/` (new `w21flip-dolphin-ab.py`; reuse `placement-gate-capture.py` nav +
    `rb3_director_disable`→`rb3_force_shot` camera-pin recipe)
  - artifacts → `docs/native/engine-arch-review-2026-07-05/execution/W2.1-flip/dolphin-ab/`
- **steps:**
  1. Boot to gameplay, `rb3_director_disable 1` then pin a wide venue shot (crowd + drum kit
     in-frustum), `RB3_FIXED_CLOCK`.
  2. Capture 2× flag-OFF + 2× flag-ON (`RB3_PLACEMENT_CONTRACT=1`) at the **same** pinned shot
     (A/A pairs prove the pink-bloom wash is flag-independent).
  3. Lay out side-by-side vs `docs/native/c8-ground-truth-2026-07-01/dolphin-shots/gp_00.png`
     (crowd spread house-left) + `images/retail-screenshots/fandom_gameplay_drums.png` (drum position).
  4. Run the crowd + drum oracle on the flag-ON capture and record GREEN as the numeric companion.
  5. Write `dolphin-ab/README.md` stating: package produced, oracles GREEN, **coordinator human-eyes
     sign-off is the flip trigger** — agent did NOT flip.
- **verify:** ≥2 captures per flag state present; A/A pairs; oracle GREEN companion; comparison
  images vs gp_00 + retail drum present.
- **fail-red/staging:** package-only; NO default change, NO re-golden; the agent's exit is
  "package produced + oracle green", never "flip done".

## Exit criteria (exitReached = `ready-for-flip` — NOT a default change)

1. **Flag mechanism (S1):** engine builds; `RB3_PLACEMENT_CONTRACT_OFF` opt-out wired with opt-out
   precedence and legacy opt-in preserved; **effective default STILL OFF**; default (no-env) path
   byte-identical (canonical splash 888, lineup PASS); coordinator can flip via one line
   (`kPlacementContractDefaultOn 0→1`); opt-out fail-red rehearsed GREEN; `RB3_PLACEMENT_CONTRACT_OFF`
   appended to classification.json under flock (NO regen); census exit 0.
2. **Drum oracle (S2):** `kind=="drum"` assertion + probe implemented; drum oracle **RED** on the
   current default-OFF build and **GREEN** flag-ON; synthetic fail-red committed; wired into
   `placement-gate-capture.py`.
3. **UI regression gate (S3):** flag-ON vs flag-OFF region diff within A/A noise on BOTH song_select
   (scrollbar) and main_hub (hub bar); negative controls green — no UI-placement regression.
4. **Dolphin A/B (S4):** package in `dolphin-ab/` — ≥2 captures per flag state, A/A protocol, vs
   `gp_*.png` + retail, oracle-GREEN companion.
5. **Default STILL OFF; no re-golden; flag-OFF path byte-identical.** Coordinator flips + re-goldens
   after E1 sign-off.

## Files touched (coordinator cross-diffs)

- engine `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (S1: `:2874-2875` read only)
- engine `milo-native-engine/src/platform/NativeCompatFlags.classification.json` (S1: append 1 row, no regen)
- rb3 `src/system/bandobj/BandConfiguration.cpp` (S2: drum probe, HX_NATIVE)
- rb3 `native/tests/placement_oracle.h` (S2: drum-kind branch)
- rb3 `native/tests/test_placement_oracle.cpp` (S2: drum tests)
- rb3 `scripts/native/placement-gate-capture.py` (S2: drum gtest wiring)
- rb3 `scripts/native/w21flip-ui-ab.py` (S3: new)
- rb3 `scripts/native/w21flip-dolphin-ab.py` (S4: new)
- docs (artifacts): `.../W2.1-flip/ui-ab/`, `.../W2.1-flip/dolphin-ab/`, `STATUS.md`, `PLAN.md`

**NOT touched:** `src/App.cpp` (forbidden); the flip's default line (coordinator); the committed
drawlog goldens (coordinator re-goldens after flip); sibling `FxSendNative.cpp`; `gen.inc` regen
(coordinator).

## Risks / conflicts

- **Lane A sequential:** `W0.3d-fix` (DONE, rb3 `b9bd33f3`) → **W2.1-flip** → `W3.1a`. S1's engine edit
  is at `Rnd_Wgpu_RB3.cpp:2874-2875`; W3.1a edits `WriteSceneUniforms` (`:1160-1458`) in the SAME
  file. Functions are disjoint but the shared engine tree means **S1 must land before W3.1a starts
  engine edits** (standing same-file rule). Coordinate ordering with the coordinator.
- **classification.json is multi-lane (flock + append-only):** S1 (this item), W3.1a, and W0.6b all
  write `NativeCompatFlags.classification.json`. Append rows under `flock /tmp/milo-engine-classjson.lock`,
  **NO gen.inc regen by any lane** — the coordinator does ONE regen + reconciliation at wave end
  (W2.6's live-clobber failure mode). S1 appends exactly one row.
- **The flip breaks the committed drawlog golden (888→792):** this is EXPECTED and is the
  coordinator's re-golden AFTER the flip — NOT this item. Because this item leaves the default OFF,
  the golden stays green throughout; do NOT touch it.
- **Drum kit `skinned` assumption:** the drawn-consistency check assumes the bone-attached kit prop
  is flagged `skinned` in the drawlog (W2.1 verifier evidence supports it). S2 must VERIFY on a live
  flag-ON capture; if false, adjust the drawn predicate and record it — do NOT silently drop the drum
  kind (WAVE5_REVIEW A5).
- **Do NOT flip the default and do NOT re-golden.** Exit is `ready-for-flip`; the coordinator flips
  the one line + re-goldens after human-eyes Dolphin sign-off (E1).
- **Ground-truth path in the brief was stale:** Dolphin gp shots are under
  `docs/native/c8-ground-truth-2026-07-01/dolphin-shots/`, not the engine-arch-review subtree.
