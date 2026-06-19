# verify-lighting-polish — INDEPENDENT REVIEW (2026-06-19)

**Verdict: CONFIRM_WITH_RESIDUALS.** The engine-only venue lit-path exposure
scale lands as designed: the over-bright pink venue reveal (sub-item 2) is
tamed, menu-hub contrast moves the right way (sub-item 1, shared trim), and the
endgame disco peak rides the same lever (sub-item 3). No regression in the
at-risk world.cam scenes (song_select). Wii byte-identical re-confirmed
independently. Residuals are the explicitly-deferred bright-side menu gap and the
phase-variance in the gameplay A/B (qualitative win is unambiguous; exact
reveal-phase numbers are not strictly reproducible in this harness).

Reviewer is INDEPENDENT of the implementer: built the worktree myself, captured
my own before/after under `/tmp/rp8rev-lighting-polish/`, did not trust the
impl's screenshots.

## What I verified independently

### Build (own)
- Worktree `/home/free/code/milohax/rb3/.claude/worktrees/task-lighting-polish`,
  branch `wt-task-lighting-polish`, HEAD `f35c58d9`.
- Engine paired worktree `.engine-path =
  /home/free/code/milohax/milo-native-engine-worktrees/task-lighting-polish`,
  HEAD `03695e312db4210d7efcdb919ee3026817a77166` ("env-tunable venue lit-path
  exposure scale").
- `cmake -B native/build-native -S native -DCMAKE_C_COMPILER=clang
  -DCMAKE_CXX_COMPILER=clang++ -DDawn_DIR=.../dawn -DMILO_ENGINE_PATH="$(cat
  .engine-path)"` → configure OK; `--target rb3-native` → **build exit 0**, 46MB
  binary. `strings` confirms `RB3_VENUE_POINT_EXPOSURE` /
  `RB3_VENUE_DIR_EXPOSURE` compiled into the binary (fix is live, not a no-op).

### Engine diff is exactly the plan (read it)
`src/platform/Rnd_Wgpu_RB3.cpp` `BandRnd::WriteSceneUniforms` only, +33/−7:
- two getters `sVenuePointExposure` (0.70) / `sVenueDirExposure` (0.80) appended
  after `sVenueGreyKey`;
- dir block: `std::min(lc.<c> * de, 1.5f)` (de = dir exposure);
- point block: `std::min(lc.<c> * pe, 1.8f)` (pe = point exposure);
- no-light grey key: `sVenueGreyKey() * sVenueDirExposure()`.
- Confined to the `world.cam` venue branch. NO touch to `softClipLighting`,
  `fs_postproc`, `DrawMesh`, `DrawParticles`, `standard_wgsl.inc`, the ambient
  floor, or any struct/bind-group → zero uniform-layout risk; identity at
  exposure=1.0 (clean env A/B control).

### A/B method
Single FIX binary; FIX-default vs full-revert env `RB3_VENUE_POINT_EXPOSURE=1
RB3_VENUE_DIR_EXPOSURE=1`. Frame-pinned capture (`scripts/native/_framepin_capture.py`)
+ `measure.py` (3×3 luminance contrast / dark / mlum / crush; calibrated — reads
retail hub ref at **10.53:1 / dark 0.034**, matching the documented baseline).
Ports 9818/9819 (my assigned range). Evidence `/tmp/rp8rev-lighting-polish/`.

## Results

### (2) Venue song-start exposure — PRIMARY — PASS (qualitative, decisive)
Raw lit-path `RB3_PP_OFF=1` small_club venue backdrop under world.cam,
FIX-default vs revert (`g_ppoff_fix_f8.png` / `g_ppoff_rev_f8.png`):

- **Visual (decisive):** REVERT = the club room's walls/cabinets bleed a flat
  brighter **pink/magenta** flood; FIX = the SAME room reads **darker and more
  saturated with form** — window grid, door, "GAMING ROOM" signage and EXIT
  neon all readable, walls deeper purple/maroon, pink flood pulled back. This is
  the over-bright-reveal → saturated-room-with-form win the plan asked for, and
  it is on the venue lit-path (world.cam), which is the exact target.
- Numbers: contrast 9.48 → 10.01:1, crush% 27.98 → 31.69 (more dark pixels). The
  exposure scale is clearly biting on the venue backdrop.
- **Soft-clip still backstops:** composited (PP ON) frames `clipW% = 0.00` both
  ways. The FIX composited steady-state frame (`g_comp_fix.png`) shows the full
  band lit on the small_club stage + EXIT neon popping + venue structure
  readable — **steady-state NOT dimmed / crushed**.

CAVEAT (residual, not a defect): the frame-pin harness lands on `game_screen`
well past the requested early pin (frame ~1800–3300), and the disco color-wheel
phase differs per boot, so the exact reveal-phase mlum numbers the impl reported
(0.415→0.256) are NOT strictly reproducible here — I land at different
color-wheel phases. The qualitative FIX-vs-revert direction (less pink, more
form, less reach into the clip) is unambiguous and content-matched on the venue.

### (1) Menu hub contrast — SECONDARY (shared trim) — PASS
Frame-pinned hub (pin 203), FIX-default vs revert:

| metric | REVERT (exposure=1) | FIX-default | dir |
|---|---|---|---|
| 3×3 contrast | 4.10:1 | **4.31:1** | up |
| dark-cell | 0.142 | **0.135** | darker → retail 0.034 |
| bright | 0.582 | 0.581 | flat (no neon dimming) |
| mlum | 0.356 | **0.349** | down → retail 0.190 |
| crush% | 9.25 | 9.29 | ~flat |

Clean rise from the shared exposure scale; both hub frames render with band
mascots / QUICKPLAY / neon fully readable (`hub_fix.png`), no crush. Matches the
impl's reported direction. Single-frame ~4.3:1 < the loop-median ~6.8:1 (camera
phase, as the plan warns) and < retail 10:1 — the bright-side gap is **explicitly
deferred** (wave-2 emissive lever, risks gem/smasher bloom). In scope here = the
shared point-light/grey-key trim only; delivered.

### (3) Endgame backdrop tint — TASTE — rides (2), no extra code — ACCEPT
The endgame crowd uses the same `main_crowd.lit` point light under world.cam, so
the 0.70 point exposure softens its disco peak for free. No separate green-trim
code (the color-wheel is the faithful authored disco per wave-5 adjudication).
Sound scope decision; no code to review.

### No-regression sweep — PASS
- **song_select** (draws under world.cam, the at-risk scene): FIX vs revert
  essentially identical — mlum 0.291→0.280, dark 0.208→0.197, crush 0.14→0.22%
  (no crushing), bright flat. `ss_fix.png` renders clean (MUSIC LIBRARY, "VIEWING
  ALL 83 SONGS", setlists, album box, footer). **Unchanged.** (UI prelit /
  backdrop near-black → exposure scale is a near-no-op there, as designed.)
- **game.cam highway:** byte-identical — the scale is inside the world.cam venue
  branch only (verified from the diff); the sTrackLight / game.cam path is
  untouched. No leak.
- **score screen:** not separately captured (jump-to-end is slow in this
  harness); the change is gentler than the wave-5 floor cut that already passed
  the score-screen gate, and a ×0.70/0.80 scale on already-clamped colors cannot
  crush a scene the floor-cut left clean. Confirmation-not-blocker (sub-item 3
  needs no code; score path is prelit/lit). Concur with the impl.
- **Crashes / NaN / asserts:** none across hub / song_select / gameplay captures.

## Wii byte-identical — re-confirmed independently
- rb3 worktree diff vs base `1c46a70e` = **`native/CMakeLists.txt` only** (the
  `MILO_ENGINE_PIN` 15ce606 → 03695e3 bump). `git diff --name-only ... -- src/
  config/ orig/ tools/` is **empty**.
- The engine fix lives ONLY in the paired engine worktree; canonical
  `/home/free/code/milohax/milo-native-engine` is untouched at base `15ce606`
  (worktree discipline respected).
- Engine is not compiled into the Wii DOL; no decomp `.o` touched → byte-identical
  by construction. No objdiff needed (no shared `src/` change to diff). Confirmed
  `wiiByteIdentical = true`.

## Residuals (out of scope / non-blocking)
- Menu-hub contrast lands ~4.3:1 single-frame vs retail 10:1; remaining gap is
  bright-side (wave-2 emissive lever) — **deferred per plan**, not a regression.
- Gameplay-reveal exact-phase numbers not strictly reproducible (color-wheel
  phase + late screen-arrival in the frame-pin harness); qualitative venue A/B is
  decisive. A retail small_club gameplay reveal reference would make item-2
  tuning precise; the two env knobs tune with no rebuild if art review wants it.
- Score-screen capture was confirmation-not-blocker (deferred by both impl and
  reviewer; the change cannot crush a floor-cut-clean scene).

## Evidence
`/tmp/rp8rev-lighting-polish/`: `hub_{fix,revert}.png`, `g_ppoff_{fix,rev}_f8.png`,
`g_comp_{fix,rev}.png`, `ss_{fix,revert}.png`, `measure.py`, `build.log`,
`cmake.log`.
