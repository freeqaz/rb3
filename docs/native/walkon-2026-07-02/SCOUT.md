# Song-start "walk-on" band corruption — root-cause scout (2026-07-02)

**Symptom** (native master @ `f0a95910`, post-BandPatchMesh-revert; reproduces
identically on web): for the first ~0–7 s of gameplay (the count-in window)
band members render wrong — a multi-body "knot" near center stage / the drum
kit, one body floating HORIZONTALLY with legs stretched out, claw/overextended
limb poses, feet off the floor. By ~6–9 s everyone is correct. Evidence:
`/tmp/regress-0702/native-songstart/shot_05.png` (songMs≈0),
`shot_07.png` (3 s), clean by `shot_10/11.png` (7–8 s); web
`/tmp/regress-0702/web-crowd/g_003s_1.png` (worst) vs `g_006s_2.png` (clean).

Reproduced live 6× today with `scripts/native/char-burst-capture.py`
(ports 8770–8779; logs `/tmp/rb3-charburst-877*.log`, screenshots
`/tmp/walkon-probe*`, `/tmp/walkon-p{0,1,3}*`, `/tmp/walkon-rebind`) and a
pos-dump probe (`/tmp/walkon-posprobe.py` → `/tmp/rb3-posprobe-8773.log`).

---

## 1. What SHOULD drive the band at song start (mechanism map)

There is **no literal "walk-on" clip system**. Band-member animation during
gameplay is driven ENTIRELY by camera shots; the song-start window is the
venue-authored **INTRO shot**:

| Step | Code |
|---|---|
| Venue enter picks the intro shot | `src/system/bandobj/BandDirector.cpp:185-197` (`allow_intro_shot` → `PickIntroShot()` → `set_intro_shot`) ; `PickIntroShot` :905 |
| Intro category chosen by script | `orig-assets/extracted/world/world_objects.dta:4433-4454` — `get_intro_category`: `INTRO_VENUE` (first song), `INTRO_MULTI` (later songs in a setlist), `INTRO_QUICK` (replay). Camera categories list: `world/camera_cats.dta:2` |
| Count-in clock armed | `set_intro_shot` DTA (`world_objects.dta:4464-4474`) → `{beatmatch set_intro_real_time -(shot total_duration_seconds)}` → `Game::SetIntroRealTime` (`src/band3/game/Game.cpp:629-634`): `TheTaskMgr.SetSeconds(-introDur)`, `mRealtime=true`. The TaskMgr seconds run **-introDur → 0 in real time** during the intro; audio `songMs` reads 0. `GamePanel::StartGame` gates on `HasIntro()` (`src/band3/game/GamePanel.cpp:305-310`). NOTE `Game.cpp:1758-1762`: while `mRealtime`, `mSongPos`/beats are NOT recomputed — the **beat clock is effectively frozen** through the count-in (Wii and native alike). |
| Shot start drives the characters | `BandCamShot::StartAnim` (`src/system/bandobj/BandCamShot.cpp:325-383`): for each authored `Target {mTarget, mXfm, mAnimGroup, mTeleport, mFastForward}` it (a) `TeleportTarget` (`:311`, `SetLocalXfm` + `teleport_char` msg) and (b) sends `play_group` |
| `play_group`/`teleport_char` message handlers | DTA `world_objects.dta:3885-3915` → `{$char play_group $grp TRUE FALSE …}` / `{$char cam_teleport}` → `BandCharacter::OnPlayGroup`/`OnCamTeleport` (`src/system/bandobj/BandCharacter.cpp:2868`, `:2908`) |
| Group → clip | `BandCharacter::PlayGroup` :2340 → `SetState` :2364 → `PlayMainClip` :225 (finds `CharClipGroup` by name in the member's clip dir; the ONLY C++ caller of `play_group` is BandCamShot::StartAnim — grep-verified) |
| Intro anim-group flags | `BandWardrobe::AddDircut` (`src/system/bandobj/BandWardrobe.cpp:758-800`): flag `0x400` = "Intro" anim groups; warns `"%s intro camera looking for non-intro anim group"` |
| Shot end | `BandCamShot::EndAnim` teleports targets **back** to their cached pre-shot transforms (`BandCamShot.cpp:484-495`); DTA `shot_over` (`world_objects.dta:3702-3719`) → for `INTRO_*` categories → `pick_new_shot` (regular song-anim shot rotation takes over) |

For the small-club venues the intro shot is `coop_intro_venue.shot` /
`coop_smallclub_intro_venue.shot` (per-run VOIDCUT_DBG logs), whose authored
target anim groups are plain **`stand`** (guitar/bass/mic) and **`sit`**
(drums) realtime idles — the venue milo carries `stand`×many + `INTRO_VENUE` +
`coop_intro_venue.pst` (strings dump of
`world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox`). Realtime
idle clips advance on the real-time task queue, so they animate during the
count-in even with beats frozen.

**Before the venue**: during song load the game plays a *transition vignette*
on the band characters — for this profile the tv11 cab ride
(`world/vignette/transition/tv11/a/{getincab,ridingincab,leavecab}`), plus
shell vignettes (sv3/sv4/sv8). Clip groups `player0…player3`, clips
`playerN_{m,f}` — clipType `vignette`. These are seated/lounging poses authored
for the cab set. (`/tmp/rb3-posprobe-8773.log` load lines; BAND_ANIM probes.)

## 2. What actually happens natively in 0–7 s (measured)

Shot timeline (VOIDCUT_DBG, run 8772): `coop_intro_venue.shot` is active for
the whole `songMs==0` window (~6–7 s real time), then `coop_all_f*` etc. The
**broken window == exactly the intro-shot / count-in window**; frames go clean
within ~0.5–1.5 s of the song clock starting.

Per-member live state (BAND_ANIM_PROBE + `bone_pelvis.mesh`, 4 targeted runs
8770/8777/8778/8779 — one member each; note the probe's shared `frameCt % 30`
throttle aliases `BAND_ANIM_PROBE='*'` to slots 0/2 only, gcd(30,4)=2):

* **Pre-game (song load)**: every member plays tv11/shell VIGNETTE clips
  (`player0_f`, `player1_f`, `player3_m`…), often a *different slot's* group
  (retarget). Pelvis positions are vignette-space: e.g. player3 pelvis frozen
  at `(-58.5, -17.2, 21.3)` (downstage-left, **z≈21 = seated/cab height**)
  for ~2500 frames while `grp='(none)' clip='(none)'` (driver holding the last
  vignette pose).
* **Intro-shot start**: within ~10–15 frames the probed member ALWAYS received
  the authored group and its pelvis landed on its stage root — player3
  `sit/idle_d_0*` pelvis `(14.2, 146.3, 41.9)`; player1 `stand/stand_idle_norm_*`
  pelvis `(-70, 81, 52)`; player0 `stand/stand_realtime_idle_*` `(69, 52, 49)`;
  player2 `stand→extreme_closeup / ms_idle_crowdinteract_rt_01` `(-13, 35, 51)`.
* **Roots** (`{rb3_pos_dump}` verbose, 0.5 s cadence through the window): all
  4 band roots SPREAD and stable the entire time — `player0 (69,52,13)`,
  `player1 (-70,81..88,13)`, `player2 (-10..8,-6..31,13)`, `player3
  (14.4,146.1,13.2)`. The knot is **not** root placement.
* **Yet the RENDERED frames stay broken through the count-in** in the same
  sessions (e.g. run 8774: player3 verified `sit`@kit by ~0.8 s, but the 3–4 s
  screenshot still shows the center knot + horizontal/inverted member +
  claw poses + a large black "fur ball" occluder + white thin-geo shard
  clusters). The shard clusters persist a few seconds past the count-in
  (known accepted thin-geo residual); the knot/floater/claw poses clear at
  ~clock start.
* No `"could not find group"` / `"no clip w. flags"` NOTIFYs in any run — the
  clip groups resolve.

### Interpretation (ranked)

**H1 (leading) — stale-vignette pose blended/held through a frozen count-in.**
The gameplay band enters the venue still *posed by the cab/shell vignette
clips* (measured above). When the intro shot re-groups everyone, the
transition out of that wildly-wrong pose is not a clean snap:
`OnCamTeleport → Teleport()` only resets sims (hair/IK — `Character.cpp:485`,
`CharIK*.cpp`, `CharHair.cpp:470`); the CharClipDriver **blend** from the old
clip is beat-based, and during the count-in the beat clock is frozen
(`Game.cpp:1758` — `mSongPos` only recomputed when `!mRealtime`). On Wii the
"from" pose is a sane stage idle (everything preloaded and posed long before
the first visible frame), so a frozen blend is invisible; natively the "from"
pose is a cab-seat/lying vignette pose → a **mid-blend grotesque held for the
whole count-in**: horizontal floater (seated/lying pose residue — vignette
pelvis z≈21–40 vs standing ≈50), claw arms, overextended limbs, and the pile
of half-posed bodies+outfits near center reading as a knot. It completes/snaps
right when beats start advancing (`songMs > 0`) — exactly the observed
resolution time.

**H2 — outfit/instrument child-dir lag.** `mOutfitDir`/`mInstDir` are separate
`Character`s with their own drivers (`BandCharacter.cpp:440-446, 529-536`);
the pelvis probe only measured the member's main skeleton. If the OUTFIT
driver keeps the vignette clip longer than the main driver, the visible body
(mostly outfit skin meshes) poses wrong while the probed pelvis reads correct.
Discriminator: extend BAND_ANIM probe to `mOutfitDir->GetDriver()` clip name.

**H3 — per-session flakiness of the shot's target resolution.** Each probed
run showed its probed member healthy; sessions vary (different intro shots,
different load timing). A run where `CreateTargetCache`/`FindTarget`
(`BandCamShot.cpp:335-345`) misses a member at StartAnim would leave that
member on the raw vignette clip until the NEXT shot (~7 s later) — matching
the worst frames (`/tmp/regress-0702`, web `g_003s_1.png`). Supported by run
8775 showing `grp='(none)' clip='player0_f'` persisting ~380 frames into the
game window. Discriminator: log a line in `BandCamShot::StartAnim` per target
(resolved? played?) for the intro shot.

The big black "fur ball" at center and the white radiating shard clusters are
the **known** thin-geo skinning residuals (hair/strings basis mismatch — see
`BandCharacter.cpp` C7/C8 comments and the inst-strings rebind `2f393eaa`),
made severe by the extreme vignette-residue poses; they are a *consequence*
during this window, not the driver. SKEL_REBIND_PROBE run 8776 confirmed no
outfit rebinds occur during gameplay (all rebinding completed pre-game), so
the wave-08 rebind machinery is NOT the live culprit.

## 3. Is the 0–7 s window visible on Wii? (Dolphin ground truth)

Partially hidden, and the pose corruption itself does not exist there:

* Dolphin ground truth
  `docs/native/c8-ground-truth-2026-07-01/dolphin-shots/gp_00.png` shows the
  Wii intro window: a **dark, tight, backstage/door framing with the band as
  near-black silhouettes** — the INTRO_VENUE pan deliberately dwells on venue
  walls/door with the band tiny, peripheral and unlit (cf. the native VOIDCUT
  comment: "the intro pan… dwells on the stage wall with the band
  tiny/peripheral", `BandDirector.cpp:406-416`).
* Natively, TWO deviations expose the window instead: (a) the V36 VOIDCUT
  fallback re-points the venue draw-cam at the last band-framing camera when
  the intro pan is "void" (`BandDirector.cpp:340-450`), and (b) the venue
  lighting bridge renders the stage bright where Wii shows silhouettes. So the
  native experience is "camera+lighting show a moment the real game frames
  away" **on top of** a real native-only pose defect (the vignette residue).
  On Wii the members simply stand/sit in their authored idles during the pan,
  so even a direct look would be fine.

## 4. Recency

**Pre-existing, not a recent regression.** Evidence:

* The evidence frames are post-`f0a95910` (today's BandPatchMesh revert), and
  the corruption reproduces on freshly-built HEAD.
* `git log --since=2026-06-05` on the walk-on driver files
  (`BandCamShot.cpp`, `BandDirector.cpp` shot path, `BandCharacter.cpp`)
  shows only the June char-skinning/rebind + venue-bridge work; nothing in the
  last 3 weeks touched intro-shot/play_group/vignette handoff.
* The 2026-06-11 triage note ("intro-cinematic … shards ≠ broken gameplay")
  already describes intro-window artifacts being dismissed.
* Root mechanism (async native load → gameplay band still wearing vignette
  clips at venue entry + count-in beat freeze) has existed since native
  gameplay + async loading came up.

Confidence: medium-high (not bisected; per instruction stating confidence
instead).

## 5. Fix proposal (ranked, smallest-faithful first)

All rb3-side (`HX_NATIVE`), no engine work needed for the core fix; the
thin-geo shard/hair residual is a separate, known engine-level limitation.

1. **Snap out of vignette state at gameplay entry (small, faithful).** In
   `BandCharacter` at the first gameplay `PlayGroup` (or in `Enter()` when the
   previous clipType was `vignette`): clear the stale driver
   (`mDriver->Clear()` — handler exists, `CharDriver.cpp:644`) and/or force
   the new clip to start with NO BLEND + `SetTeleported(true)` so hair/IK
   reset. Wii-faithful because on Wii the pre-intro pose is already the stage
   idle — the blend-from-vignette simply cannot occur there; suppressing it
   native-only is a timing-parity fix, match-neutral (HX_NATIVE block).
   Opt-out env (e.g. `RB3_NO_VIGNETTE_SNAP=1`). ~15 lines.
   *Verify*: char-burst run — knot/floater/claw window should vanish; also
   web spot-check once.
2. **Instrument + harden intro-shot target delivery.** Add a one-line
   diagnostic in `BandCamShot::StartAnim` (target resolved? group played?) to
   catch the H3 per-member miss; if misses occur, retry unresolved intro-shot
   targets on subsequent Polls during the intro (native-only). Small; do it
   together with (1) since it shares the verification harness.
3. **(If H2 confirmed) mirror the snap to `mOutfitDir`/`mInstDir` drivers** —
   same treatment as (1) on the child dirs.
4. **Camera/lighting parity for the window (cosmetic, optional).** Keep the
   intro pan's authored framing for the venue backdrop during INTRO_* shots
   (the tight-40u intro special-case already exists — `BandDirector.cpp:416`;
   consider not falling back to a band-framing cam during the intro at all)
   and/or respect the intro shot's dark lighting. This hides the window the
   way Wii does; do NOT rely on it as the fix — web/native wide shots at 3 s
   (post-intro) must also look right.
5. **Load-gating parity (bigger, only if 1–3 leave stragglers).** Ensure the
   wardrobe clip loads (`BandWardrobe::StartClipLoads`,
   `BandDirector::HarvestDircuts` `BandDirector.cpp:989`) complete before
   `GamePanel` leaves loading — the Wii-blocking behavior. Touches the load
   pipeline; higher regression risk to load-perf work — last resort.

### Repro / verification harness

```bash
# knot window capture (screenshots + engine log)
python3 scripts/native/char-burst-capture.py --port 8770 --shots 10 \
  --interval 1.0 --out /tmp/walkon --extra-env "VOIDCUT_DBG=1"
# per-member clip+pelvis probe (NOTE: '*' aliases to slots 0/2 — probe one member)
... --extra-env "BAND_ANIM_PROBE=player3 BAND_ANIM_BONE=bone_pelvis.mesh"
# roots through the window
python3 /tmp/walkon-posprobe.py 8773   # POS_DUMP_VERBOSE {rb3_pos_dump} every 0.5s
```

Probe gotcha worth fixing while in there: `BAND_ANIM_PROBE`'s shared static
`frameCt % 30` throttle (`BandCharacter.cpp:489`) can only ever fire on 2 of
the 4 members when matching all (gcd(30,4)=2) — make the counter per-member or
use a %29 modulus.
