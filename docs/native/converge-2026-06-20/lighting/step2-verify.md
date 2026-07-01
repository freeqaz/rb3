# STEP 2 verify — FINAL adversarial gate (Opus)

**STEP-2 VERIFY agent. Independently re-derived both fixes by reading the engine
source at the worktree HEAD (`bae1aae`), rebuilding rb3-native, re-measuring
shots, and running fresh A/B captures. Worktree
`/home/free/code/milohax/rb3/.claude/worktrees/converge-lighting`; private engine
`/home/free/code/milohax/milo-native-engine-worktrees/converge-lighting`
(branch `wt-converge-lighting`).**

## Verdict: LAND_WITH_NOTES — both fixes are structurally correct + DC3-safe.

Ordered engine SHAs for the coordinator to push + pin (`MILO_ENGINE_PIN`):
1. STEP 1 (GAP 2 GX falloff): **`a360e3c3124d16d76f94b4ad80dbf1d65b042798`**
2. STEP 2 (GAP 3 impostor crowd env): **`bae1aae164e00610d7bb065e73136ff092c9613e`**

Linear history atop the current pin `5cbe855` → `a360e3c` → `bae1aae`. Engine
working tree clean; NOT pushed; main-repo pin `native/CMakeLists.txt:74` still
`5cbe855` (untouched, as required). Worktree-local pin is `bae1aae` for local
build only.

---

## STEP 1 (GX point falloff, GAP 2 arena band) — VERIFIED, default-on, DC3-safe

- **Visible win confirmed** (step1/arena02 shots, my eyes + luma): legacy arena_02
  band = near-black silhouettes (`coop_fs_b_c` bandLuma 46.1); GX = lit, readable,
  moody-spotlit forms (63.4), staying BELOW the flooded VENUE_OFF look (73.0).
  `coop_fs_all_n00`: 50.0→53.5, vs OFF 81.3. Gate "rises into spotlit range but
  below flooded" — **MET**.
- **DC3 byte-identical — confirmed structurally:**
  - `pointFalloffMode` reuses the former `_padPL[0]` slot (struct size unchanged);
    C++ (`UniformStructs.h`) and WGSL (`standard_wgsl.inc`) field order match.
  - mode-0 else-branch math = the EXACT legacy `saturate(1-d/r)^2`.
  - DC3's `WgpuRnd::WriteSceneUniforms` (`Rnd_Wgpu.cpp:1276`) does `SceneUniforms
    scene{}` + an explicit `memset(&scene,0,sizeof(scene))` → `pointFalloffMode`
    always 0 → byte-identical legacy curve. DC3 never references the field
    (grepped dc3-decomp: 0 hits). DC3 GPU backend = `dc3` (compiles `Rnd_Wgpu.cpp`,
    not `Rnd_Wgpu_RB3.cpp`).
- **NOTE — blast radius is BROADER than the plan's STEP-1 gate text claimed.**
  The plan said "directional-lit venues (festival, clubs) don't move." That is
  only partly true. STEP 1 is set on the WHOLE `world.cam` venue path, and
  RB3's character envs are heavily point-lit: the default city venue's
  `char_rooftop.env` (2× range-250 points), `street_slomo_char.env` (8 points
  114-1000u), festival `RB3_chars.env` (range-40-50 silhouette points) all get
  the new curve. So STEP 1 brightens point-lit characters in MANY venues, not
  just arena. This is consistent with the band-lit intent (and masked where a
  bright directional dominates, e.g. festival's `char_bounce` (0.5,1.0,0.64)),
  but the gate "festival/club UNCHANGED" is not literally satisfied — it's
  "minimally changed, directional-dominated." game.cam (highway) + menu + DC3 ARE
  byte-identical (mode never set there). Recommend the coordinator treat STEP 1
  as a venue-wide character-lighting change, not an arena-only one, and keep
  exposure (`sVenuePointExposure` 0.70) as the tuning knob if any venue reads hot.

## STEP 2 (impostor crowd env, GAP 3 big_club white crowd) — STRUCTURALLY VERIFIED; visible gate NOT met (by design, masked)

- **Structural fix VERIFIED (my fresh big_club_01 RB3_VENUE_PROBE run):** the
  impostor cam now enters the venue gate and reads its scoped dim env —
  `[VENUE_PROBE] env=crowd.env ambRaw=(0,0,0)` and
  `env=RB3_crowd_mesh.env ambRaw=(0.18,0.18,0.18)` now fire. Before STEP 2 the
  unnamed impostor cam missed the `world.cam`-only gate and fell to the
  hardcoded-white default. The widening works exactly as intended.
- **Objective gate "big_club crowd white% → small_club ~0" — NOT achieved on the
  VISIBLE crowd, and not achievable via STEP 2 in this build.** My crowd-region
  white% across every big_club variant is INVARIANT ~9–11%: on 9.2, off
  (`RB3_CROWD_LIGHT_OFF=1`) 10.3, greykey0 (`RB3_CROWD_GREY_KEY=0`) 10.0,
  step1legacy 10.2, fresh both-on 10.8. The STEP-2 knobs do not move it. Reason
  (confirmed): the VISIBLE big_club crowd renders via **world.cam** scoping
  `char_rooftop.env`, plus a mesh-shard white smear — NOT the impostor path STEP 2
  fixes. `RB3_VENUE_LIGHT_OFF` triples big_club crowd luma (→125) while leaving
  white% ~unchanged, proving the visible crowd is on the world.cam path.
- **game.cam (highway) + world.cam (band) byte-identical — guaranteed.** The
  discriminator excludes named cams; the grey-key softening only fires for
  `isImpostorCrowdCam` (world.cam keeps the 0.22 key verbatim — I diffed the
  branch: `greyBase = false ? CrowdGreyKey : sVenueGreyKey()` = `sVenueGreyKey()`,
  identical). Separate-boot pixel A/B is unreliable here (documented per-boot
  disco-phase non-determinism: smallclub on 20.4 vs off 92.8; sc_det r1 31.6 vs
  r2 48.8) — the byte-identity is by code inspection, which is sound.
- **Zero-light guard does NOT over-darken legit crowds.** Soft 0.10 key only on
  the impostor path; ambient-only non-crowd world.cam envs (sky/back_left/road)
  keep 0.22. Correct.
- **DC3-safe:** `Rnd_Wgpu_RB3.cpp` is RB3-only; DC3 compiles `Rnd_Wgpu.cpp`. DC3
  cannot reach this code. Confirmed.

### DISCRIMINATOR ROBUSTNESS — one real gap (low practical risk)
The `(unnamed cam && TargetTex())` discriminator is NOT provably unique by static
analysis, contrary to the impl doc's "ONLY the crowd cam" claim:
- I audited every `SetTargetTex` + `New<RndCam>` caller. **`OutfitConfig::sCam`**
  (`bandobj/OutfitConfig.cpp:388`) is created via `Hmx::Object::New<RndCam>()`
  with NO name → `mName=gNullStr` (unnamed), and `Hmx::Object::Copy` (`Object.cpp:361`)
  does NOT copy the name, so it stays unnamed after `Copy(defaultCam)`. It calls
  `SetTargetTex(diffTex)` + `Select()` (`:133,:143`). So it is ALSO
  `(unnamed && TargetTex())` — a discriminator collision the impl probe missed
  because it never visited the char-customization screen where the outfit RTT runs.
  (The "Cam.cam" the probe logged is the NAMED track-panel cam from
  `TrackPanelDir.cpp:233`, a red herring — not the outfit cam.)
- **Why it's low-risk anyway:** (1) the gate also requires `venv &&
  venv->mAmbientFogOwner`; (2) OutfitConfig composites flat textured RECTS via
  `TheRnd->DrawRect` with an explicit-color `sMat`, NOT lit `DrawMesh` geometry,
  so scene-uniform lighting has no meaningful effect on the composite; (3)
  `RB3_CROWD_LIGHT_OFF=1` opts out. `LayerDir::sCam` is also unnamed but has NO
  TargetTex → correctly excluded. TexRenderer's cam is mostly named engine cams.
- **Recommendation:** harden the discriminator to be truly unique by also
  requiring the impostor env (e.g. `venv->Name()` contains "crowd") OR exposing
  `cam==gImpostorCamera`. Not a blocker for LAND, but worth a follow-up so a
  future char-customize-during-venue path can't trip it.

## Commit isolation / revertability
- 2 clean commits, linear, atop the current pin. STEP 1 = 3 files
  (`UniformStructs.h`, `standard_wgsl.inc`, `Rnd_Wgpu_RB3.cpp`); STEP 2 = 1 file
  (`Rnd_Wgpu_RB3.cpp`).
- `git revert bae1aae` (STEP 2) then `a360e3c` (STEP 1) is clean in that order.
  Reverting STEP 1 ALONE while STEP 2 is present would textually conflict (STEP 2
  rewrote the gate block that contains STEP 1's `pointFalloffMode=` line). BUT
  each fix has a clean RUNTIME opt-out (`RB3_VENUE_POINT_FALLOFF_LEGACY=1`,
  `RB3_CROWD_LIGHT_OFF=1`), so independent rollback is achievable without git
  surgery. Satisfies "either can revert" in practice.

## Venues regressed
None observed. arena_02 improved (STEP 1). big_club_01 visible crowd unchanged
(STEP 2 masked, not regressed). game.cam/menu/world.cam byte-identical.
small_club on/off deltas are disco-phase noise, not regressions. festival_01
shot-name enumeration failed in my harness pass (shots=0, NOT a lighting
regression) — festival's char lighting is directional-dominated so STEP 1's
point-spot change is masked there per the probe data.

## Bottom line for the coordinator
- LAND both. They are correct, gated, DC3-safe (byte-identical legacy for DC3 +
  game.cam + menu + world.cam), and improve the arena band (STEP 1, visible) and
  the impostor-crowd path (STEP 2, structural).
- Carry forward TWO open items (neither a blocker): (a) big_club's VISIBLE white
  crowd is a world.cam char-extras + mesh-shard issue, route to the
  char-shard / crowd-origin workstream, NOT STEP 2; (b) tighten the STEP-2
  discriminator to be provably unique (add a crowd-env predicate) so the
  unnamed OutfitConfig RTT cam can never collide.
