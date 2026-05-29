# RB3 Native — Status & Next Goals (V34 status review)

> **UPDATE 2026-05-29 — items below are PARTLY STALE. Verified at HEAD `1df5db08`
> (see `N2_N4_N7_VERIFY_RESULT.md`):**
> - **N1 MeshIB** — DONE (engine `4077997`).
> - **N2 void/wall camera cuts** — DONE (`83780743`, V36, gate `RB3_CAM_FALLBACK_OFF`).
>   `f0700`/`f3400` now framed, not void.
> - **N3 menu/hub void** — DONE (`3600647a`, gate `RB3_MENU_VOID_FIX_OFF`).
> - **N4 grey occluder + SAVE panel** — DONE (`9a8a5479`, gate `RB3_NO_DETAILS_FIX`).
>   SAVE panel gone; grey box now the blank-album "?" placeholder. Remaining is
>   DATA only (real album art needs `ui/image/` in extraction).
> - **N6 `%S %I SONGS`** — DONE (`895e0ec6`).
> - **N7 perceptible load screen** — HONEST NEGATIVE / deferred. The load is instant
>   AND the `tv3_*` transition renders a black fade on native (no content to
>   animate); a frame-hold would only show more black. Blocked on vignette-world
>   rendering (shared root with N3). See result doc.
>
> **Still genuinely open:** vignette-world rendering (unblocks N7 + N4-data +
> N3-residual), N5 crowd slivers (deep CharServo), N8 hit/flame FX, N9 teardown
> SIGSEGV, N10 audio host output, N11 empty save-arrays.

**Authored:** 2026-05-28 (status & visual-review subagent, Opus, READ-ONLY on
source — no code edits, no build, no commit).
**Build reviewed:** `native/build-native/rb3-native` at HX_NATIVE salvage state
(post-`f8a3a379` matched-fork re-apply; engine pinned `cfaaa5bc`). V19–V33
fixes verified present in-tree (`Mtx.h:640`, `Rot.cpp:485`,
`BandDirector.cpp` EnterVenue/DrawShowing blocks all intact).
**Evidence:** fresh full sweep `screenshots/v34-status-review/`
(`01_f0007`…`11_f1400` boot→gameplay) + a clean 9000-frame deep-song sweep
`screenshots/v34-status-review/deep/` (`f1800`…`f8600`, exit 0), plus a full
read of the V18–V33 session docs.

**Deep-song confirmation (anti-cherry-pick):** the deep sweep proves gems
stream the **entire** song (`f1800`→`f3400`→`f5200`→`f8600` all show gems on the
highway — no mid-song collapse; the V17 nofail fix holds deep) and the HUD stays
top-center the whole song. It also shows the **void/wall camera cuts recur
mid-song** (`f3400` is near-total black behind the highway) — confirming N2 is a
real, repeating tell, not an intro-only artifact. The vertical "PRESS START" on
the side overshell slot (`f3400`) is the *correctly-localized* string (V28 HX_PC
locale scope verified intact in-tree) — NOT a raw-token leak.

---

## 0. Headline

The port is in genuinely good shape: **boot is reliable, every phase is
recognizable as RB3, and the in-song view (highway + venue + band + crowd +
cinematic camera + top-center HUD) reads clearly as Rock Band 3.** The big
remaining 1:1 tells are (1) a high-volume engine WebGPU validation bug
(cosmetic to render, catastrophic to the log — 195K errors/run), (2) menu/hub
backdrops cut off into black void above the framed content, (3) "void" camera
cuts that frame black/wall during a song, (4) two un-localized token leaks
(`%S %I SONGS`, and historically `SHELL_PRESS_START_TO_ROCK`), (5) crowd-shot
character slivers (residual IK/servo), and (6) a teardown SIGSEGV + audio-output
gap on this host. None of these block the core experience; they are the polish
between "clearly RB3" and "1:1."

---

## 1. Per-phase 1:1 fidelity assessment (honest, skeptical)

Recognizability % = "would a fan instantly recognize this as that RB3 screen,
and how close to retail does it look." Cited frames are from
`screenshots/v34-status-review/`.

### Boot / main hub — ~70%
- `01_f0007`: the RB3 main-hub **rooftop city** backdrop renders (purple neon
  cityscape, drum kit + amp on the rooftop) — recognizable. BUT the **top ~40%
  of the frame is pure black void** and the right third fades to black: the hub
  3D backdrop only fills a band across the lower-middle. Retail fills the whole
  frame with the animated rooftop scene. This is the same class of
  black-void-above-content tell that recurs in the menus.
- `02_f0025` / `03_f0050`: the player-select overshell slides in
  ("NO PROFILE / SIGN IN", "CONNECT CONTROLLER / CHOOSE INSTRUMENT") over the
  hub. Text legible; layout plausible. The "MENU/MENT" header glyphs look
  garbled/overlapping (`MENU` + `INSTRUMENT` colliding) — a label/z or
  text-layout artifact.
- Tell: the void above the backdrop + garbled overlapping header text.

### Main menu (PLAY NOW / QUICKPLAY) — ~80%
- `04_f0120` / `05_f0200`: the "**BABOON NEST**" neon-sign main-hub backdrop
  with the menu list ("PLAY NOW / QUICKPLAY / START A ROAD CHALLENGE", "PER…"
  tab). This is unmistakably the RB3 hub menu and looks good — colored neon
  art, a tiger/eye sign on the right. Highest-fidelity menu phase.
- Tell: still some black void around the edges; menu-card chrome frames are
  thin/absent vs retail's framed panels (long-standing COSMETIC item).

### Song-select / Music Library — ~65%
- `06_f0280`: "MUSIC LIBRARY" header + filter rows (`0/10`, `0/5`, `0/30★`)
  over a blue-grey list. Recognizable as the song browser. BUT a **large opaque
  grey occluder box** sits over the right half of the song list (the documented
  "grey occluder over Music Library" UI-layering bug), and a half-drawn
  "SAVE / Are y[ou]…" panel overlaps the right edge. The overshell card is at
  bottom-left.
- Tell: the grey occluder + overlapping SAVE panel are the dominant defects;
  the list itself reads correctly.

### Loading / song-select-to-game transition — ~55%
- `07_f0360`: the seldiff/loading view shows the difficulty card
  ("MIGHTY MICOS… BASS") and a "SETLIST" header — but the top-left reads the
  raw format token **`%S %I SONGS`** (the open TEXT_TOKENS Token-3 leak). Per
  `LOAD_SCREEN_RENDER.md` the load itself is ~3 frames (instant on native), so
  the `tv3_b` cinematic vignette never animates — there is effectively no
  perceptible loading screen. Not a render bug (the scene draws); a load-is-too-
  fast timing gap + the token leak.
- Tell: raw `%S %I SONGS`; no visible/animated loading sequence.

### In-song venue / band / crowd (cinematic frames) — ~75%
- `08_f0500`: a genuinely strong frame — the **small_club_01** wooden club
  interior from a cinematic angle, a **dense purple-lit crowd** in the
  foreground, **band members on the lit stage**, stage props/posters. This is
  clearly RB3's small club and the V19–V23 venue/character/camera work shows.
- Caveats (consistent with the docs): band/crowd characters texture via the
  simple path but outfit RTT compositing (`BandPatchMesh`) is a no-op, and
  residual crowd-shot character slivers persist (V24/V26 IK/servo residual).

### Gameplay highway — ~85% (the strongest phase)
- `10_f1100`: gem highway with correctly-colored GRYBO gems on a yellow-railed
  fretboard, the 5-color strike plate at the base, a large **teal guitar
  closeup** (the guitarist's instrument) in the foreground, the **BandScoreboard
  HUD top-CENTER** (score `0` + star-meter disc — V31 centering landed), venue
  behind (dark club, sign, dartboard). Reads immediately as RB3 guitar
  gameplay.
- `11_f1400` / deep `f1800`: down-highway, gems streaming toward the strike
  plate in sync, HUD top-center, venue behind. Clean, no shards over the
  highway (V26 fixed the dominant shards). This is the best-looking phase.
- Tell: score reads `0` (correct for headless `nofail` no-hit); no hit/flame FX
  (no synthetic note-hits); the venue background occasionally cuts to black on a
  void cut (see camera below).

### Camera cinematography — ~70%
- The director cuts between distinct shots (intro venue pan, wide stage,
  instrument closeups, down-highway), and `08_f0500` proves a great framed shot.
- BUT `09_f0700` is a clear **void cut**: the camera frames only two slivers of
  pink/wooden stage-wall at the top against pure black — no band, no stage,
  mostly black. This is the documented V24 Defect-2 / void-cut gap (intro pan
  dwells on wall + gameplay cuts whose frustum misses the venue). It recurs and
  is the most visible camera tell.
- Tell: intermittent void/wall cuts; closeups occasionally still mis-frame.

### HUD (score / star / multiplier) — ~85%
- Score plate + star meter render top-CENTER every gameplay frame (V31 + V30
  determinism). Score `0` is correct for the no-hit headless run. Multiplier
  popup correctly hidden (single-player x1). `solo_percent.lbl` shows `0%` not
  raw `%d%%` (V29).
- Tell: V31 only zeroed the anchor x; y/z polish vs retail is unverified
  (V31.1). No live score tick (needs synthetic note-hits). Cannot confirm
  digit-rollup behavior headless.

### Recognizability scorecard

| Phase | % | Dominant remaining tell |
|---|---|---|
| Boot / main hub | 70 | black void above hub backdrop; garbled overshell header text |
| Main menu | 80 | edge void; missing menu-card chrome |
| Song-select / Music Library | 65 | grey occluder box + overlapping SAVE panel |
| Loading transition | 55 | raw `%S %I SONGS`; no perceptible/animated load screen |
| In-song venue/band/crowd | 75 | outfit RTT no-op; residual crowd-shot character slivers |
| Gameplay highway | 85 | (strongest) no hit/flame FX; void cuts behind |
| Camera cinematography | 70 | void/wall cuts; some closeups mis-frame |
| HUD | 85 | y/z polish unverified; no live score (headless) |

---

## 2. Prioritized additional-work list (the new goal)

Ranked by visible 1:1 impact / cost. Layer tags: (a) matched-fork
`src/system|band3/**` (permuter-owned, additive `#ifdef HX_NATIVE` only,
re-apply if wiped), (b) engine `milo-native-engine/src/**`, (c) glue
`rb3/native/src/**`.

### N1 — MeshIB WriteBuffer size not a multiple of 4  ·  effort S  ·  Sonnet  ·  concurrent-safe
- **What's wrong:** the engine logs **195,146** WebGPU validation errors in one
  run: `WriteBuffer([Buffer "MeshIB"], data, (N bytes)) — Size (N) is not a
  multiple of 4` for every mesh whose index byte-count is odd-mod-4 (6, 90, 186,
  3126, 10650, 14958…). Rendering visually survives (Dawn tolerates it) but it
  is a real WebGPU spec violation and the log is unusable for debugging.
- **Retail behavior:** N/A (engine correctness). Index uploads must be 4-aligned.
- **Root cause + file:** `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:1206-1213`.
  The buffer is created at `padded = (isz+3)&~3` (correct), but
  `WriteBuffer(ibuf, 0, indices.data(), isz)` writes the **unpadded** `isz`. Fix:
  write `padded` bytes (the buffer is already allocated to `padded`; back the
  `indices` vector with a few pad bytes or memset a padded staging copy) so the
  WriteBuffer size is 4-aligned. One-line-ish.
- **Layer:** (b) engine. **Regression risk:** very low (the buffer is already
  the padded size; this only changes the upload length). Shared with any other
  `Rnd_Wgpu_RB3.cpp` owner — serialize engine edits.
- **Why high:** zero subjective judgment, tiny, and it un-spams the log for
  every future investigation (force-multiplier, like G_HTTP was). Front-load it.

### N2 — Void / wall camera cuts during a song  ·  effort M  ·  Opus  ·  serialize on BandDirector.cpp
- **What's wrong:** `09_f0700` and the V24-documented `f1400` cuts frame pure
  black or a sliver of stage wall — the active shot's frustum contains no venue
  geometry, and the pre-gameplay `coop_intro_venue` pan dwells on the wall.
- **Retail behavior:** every cut frames the band/stage/crowd; the camera never
  rests on void.
- **Root cause + files (per V24 Defect 2):** `BandDirector::DrawShowing` V22
  cam-follow + the intro-pan shot's authored `mWorldOffset`. Two fixes: (a) bias
  `coop_intro_venue`'s framing toward stage center; (b) add a venue-visibility
  test in the V22 cam-follow so a shot whose frustum contains no venue geometry
  falls back to the highway-locked `game.cam`. Matched-fork `BandDirector.cpp`.
- **Layer:** (a). **Regression risk:** medium — must not regress the V22/V23
  cinematography an independent review called "genuinely like RB3"; gate behind
  an env opt-out and A/B. **Concurrency:** owns `BandDirector.cpp` (serialize vs
  N5/any venue work). Highest *visible-during-play* camera tell.

### N3 — Menu/hub backdrop black void (top + edges)  ·  effort M–L  ·  Opus  ·  mostly isolated
- **What's wrong:** the main-hub rooftop (`01_f0007`) and menu backdrops fill
  only a lower-middle band; the top ~40% and right edge are pure black. Retail
  fills the frame with the animated hub scene.
- **Retail behavior:** full-frame animated rooftop/hub backdrop behind the menu.
- **Suspected root cause:** the menu 3D backdrops are cosmetic vignette worlds
  under `world/vignette/` — exactly the dirs the `IsDeferredVenueProxy` gate in
  `WorldInstance::SyncDir` (`src/system/world/Instance.cpp:351-358`) still SKIPS,
  and the `ObjectDir::PostLoadInlined` inlined-cached-shared resolution gap
  (DIVERGENCE hack #2). The in-song venue got bridged (V19 force-load) but the
  *menu vignette backdrops* were never un-deferred — so they instance only
  partially, leaving void. Verify: does the hub backdrop draw fewer meshes than
  retail / is its world only partially instanced?
- **Layer:** (a) matched-fork `Instance.cpp` + `Dir.cpp` (the long-pole
  structural fix the roadmap's G_VENUE named). **Regression risk:** medium-high
  (touches the proxy-instancing core). **Concurrency:** isolated to the obj/world
  tree. The single biggest menu-fidelity gap; also retires DIVERGENCE hacks
  #2a/#2b and the InterstitialPanel `Exiting` short-circuits.

### N4 — Song-select grey occluder + overlapping SAVE panel  ·  effort M  ·  Opus  ·  isolated UI
- **What's wrong:** `06_f0280` shows a large opaque grey box over the right half
  of the Music Library list and a half-drawn "SAVE / Are you…" panel intruding
  on the right edge.
- **Retail behavior:** clean full-width song list, no grey occluder, no stray
  save dialog.
- **Suspected root cause:** UI layering / panel-visibility — a panel (gamercard /
  save-prompt / a list-detail pane) is left `SetShowing(true)` or drawn out of
  z-order in the headless flow (same class as the V25 `draw_order.grp`
  visibility issue, but in the meta_band music-library screen). Trace the
  music_library screen's child panels' showing state. The SAVE panel is likely
  `NativeSaveLoadStub`-adjacent (DIVERGENCE Pattern 1).
- **Layer:** (a)/(c). **Regression risk:** low-medium. **Concurrency:** isolated
  to the song-select UI path. Most visible song-select tell.

### N5 — Crowd/extras character slivers (residual IK/servo)  ·  effort M–L  ·  Opus  ·  serialize char-math
- **What's wrong:** small character slivers still flicker in crowd-cinematic
  shots (`f0500`-class frames); the V24 engine guard (extent-ratio drop) is
  still load-bearing to suppress the worst.
- **Retail behavior:** clean crowd/extras meshes.
- **Root cause (per V26/V32):** after the V21 (`Mtx.h` Multiply) and V26
  (`Rot.cpp` MakeRotQuat half-angle) fixes killed the dominant body explosions,
  the residual is (a) crowd/extras **hand-IK target placement** — `CharIKHand`
  orients correctly but aims at a target proxy resolved ~300u away (trace
  `CharIKHand` `mTargets` proxy resolution, analogous to V23's closeup-target
  proxy fix), and (b) tiny-bind face features / held-props (eyebrows/goatee/clap/
  lighter). NOTE V32 found `CharIKHand::Poll` is never called in the phases that
  render the crowd in some build states — re-verify the call path first.
- **Layer:** (a) `char/CharIKHand.cpp` + `BandWardrobe.cpp` proxy resolution.
  **Regression risk:** low (improves correctness). **Concurrency:** serialize vs
  any other char-math owner. Once fixed the V24 engine guard becomes redundant.

### N6 — `%S %I SONGS` setlist/filter token leak  ·  effort S  ·  Sonnet→Opus confirm  ·  isolated
- **What's wrong:** `07_f0360` (and the seldiff setlist title) renders the raw
  printf token `%S %I SONGS`.
- **Retail behavior:** "N SONGS" / "SETLIST (N SONGS)".
- **Root cause (per TEXT_TOKENS Token-3, OPEN):** `setlist_title.lbl` in
  `seldiff.milo_xbox` has milo-default `text_token = set_list_named_title`;
  `update_named_setlist_label` either never fires (`mp->HasSetlist()` false after
  `ResetSongs()`) or fires with a `$setlist` arg `SuperFormatString` can't
  consume. Trace `SelectDifficultyPanel::Enter()`'s second call. The FILTER-view
  variant (Token-2) was already fixed (`ViewSetting.cpp` node-order swap) — this
  is the SETLIST-title sibling.
- **Layer:** (a) `meta_band/ViewSetting.cpp` / `SelectDifficultyPanel` or
  asset-side. **Regression risk:** low. **Concurrency:** isolated. Cheap visible
  win.

### N7 — Loading screen made perceptible  ·  effort M  ·  Opus  ·  isolated (glue)
- **What's wrong:** native load is ~3 frames so `preloading_screen` + the
  `tv3_b` vignette flash by frozen — no perceptible loading sequence (the user's
  original headline ask). Not a render bug (LOAD_SCREEN_RENDER.md root-caused it
  as timing).
- **Retail behavior:** multi-second cinematic vignette / loading display.
- **Root cause + fix:** hold `preloading_screen` a minimum wall-clock duration so
  the vignette animates — opt-in `RB3_LOAD_HOLD_MS` (default 0). Cleanest seam:
  glue per-frame hold in `rb3_game_input.cpp` while `currentScreen ==
  preloading_screen/tv3_b_screen` (no matched-fork churn), then confirm the
  vignette PropAnim advances.
- **Layer:** (c) glue (+ maybe verify (a) anim). **Regression risk:** low
  (opt-in). **Concurrency:** `rb3_game_input.cpp` (serialize vs other input-verb
  owners).

### N8 — Gameplay hit / flame FX  ·  effort M  ·  Opus  ·  serialize input + engine
- **What's wrong:** no hit-burst/flame FX on note-hits; strike-plate flare is
  static. Partly a synthetic-input gap (headless `nofail` never "hits").
- **Retail behavior:** colored flame bursts on hit; star-power flames.
- **Root cause + files:** need (i) a synthetic note-hit verb in
  `rb3_game_input.cpp`, then (ii) confirm `GuitarFx`/gem_mash hit FX + particle
  draw in the engine. `gem_mash0..5` meshes already load.
- **Layer:** (c) + (a) `GuitarFx.cpp` + maybe (b) engine particles. **Regression
  risk:** low-medium. **Concurrency:** serialize `rb3_game_input.cpp` + engine.
  Lower visible priority than the above.

### N9 — Teardown SIGSEGV (intermittent)  ·  effort M  ·  Opus  ·  isolated
- **What's wrong:** ~1-in-N runs SIGSEGV at a high address during/after a full
  song teardown (V23 gap #2 / V24 watch item); majority exit clean.
- **Retail behavior:** clean shutdown.
- **Root cause:** unknown; high-address fault (not the V23 ReplaceRefs null-deref
  class). Run the full song under ASan to localize; suspect a char/proxy lifetime
  (`HxNoteFreedAddr` ring guard exists for the `CharBonesObject` virtual-base
  teardown case). **Layer:** (a)/(c). **Regression risk:** low. Robustness, not
  visual — do opportunistically.

### N10 — Audio output on this host  ·  effort S  ·  Sonnet  ·  isolated (env/host)
- **What's wrong:** ALSA `default` PCM has no openable output (HDMI-only host) →
  `ma_device_init` -401 → audio silently skipped. Gameplay/clock unaffected.
- **Fix:** route to an HDMI card or `MILO_AUDIO_BACKEND=pulseaudio` (accepting the
  startup race, mitigated by the V27 GPU warm-up) or a virtual sink. Host/env
  config, not engine code. **Layer:** (c)/env. Low visual priority.

### N11 — `%S %I SONGS` sibling + `[objects]/[visibles]/[xfms]` empty save-arrays  ·  effort L  ·  Opus  ·  isolated
- **What's wrong (V31.2):** the `1_player_wide` track-config milo loads with
  EMPTY `[objects]/[visibles]/[xfms]` save-arrays (size=1 header only), so the
  authored single-player HUD/track layout can't apply from data — V31 bridges
  one anchor in C++ instead. Determine whether (a) authored-empty, (b) wrong milo
  path, or (c) the native binstream `PostLoad` drops them. If (c), fixing it
  would let the authored layout supersede the V31/V12 C++ bandaids generally.
- **Layer:** (a) binstream/PostLoad. **Regression risk:** medium (engine load
  path). Deep root-cause; deprioritize behind the visible items but it would
  retire multiple HX_NATIVE layout bandaids if (c).

### Concurrency-at-a-glance

| Item | Layer | File scope | Runs concurrent with |
|---|---|---|---|
| N1 MeshIB | (b) | `Rnd_Wgpu_RB3.cpp` | everything except other engine edits |
| N2 void cuts | (a) | `BandDirector.cpp` | N3,N4,N6,N7,N10 (NOT N5 if it touches BandDirector) |
| N3 menu void | (a) | `Instance.cpp`/`Dir.cpp` | N2,N4,N6,N8 |
| N4 occluder | (a)/(c) | song-select UI | N2,N3,N6 |
| N5 char slivers | (a) | `CharIKHand`/`BandWardrobe` | serialize char-math; else parallel |
| N6 token | (a) | `ViewSetting`/seldiff | all |
| N7 load hold | (c) | `rb3_game_input.cpp` | serialize input verbs |
| N8 FX | (c)+(a)+(b) | input + GuitarFx + engine | serialize input + engine |

---

## 3. Recommended next 3 dispatches

1. **Dispatch A — N1 MeshIB 4-align (Sonnet, ~1hr).** Engine one-liner at
   `Rnd_Wgpu_RB3.cpp:1213`: upload `padded` bytes, not `isz`. Verify the 195K
   WebGPU errors drop to zero and rendering is unchanged (re-run the v34
   reproducer, diff a screenshot). **Front-load — it un-spams every later run's
   log.** Isolated engine edit; no conflict.

2. **Dispatch B — N3 menu-vignette / hub backdrop black void (Opus).** The
   long-pole structural item: un-defer the `world/vignette/` menu backdrops in
   `WorldInstance::SyncDir` + fix the `ObjectDir::PostLoadInlined`
   inlined-cached-shared resolution (DIVERGENCE hack #2). Highest menu-fidelity
   payoff and retires multiple documented hacks. Isolated to the obj/world tree.
   Start early (it's the biggest).

3. **Dispatch C — N2 void/wall camera cuts (Opus).** Add a venue-visibility
   fallback to the V22 `BandDirector::DrawShowing` cam-follow (drop to
   `game.cam` when the shot frustum has no venue) + bias the intro pan toward
   stage center, both behind an env opt-out so the V22/V23 cinematography is
   A/B-protected. Owns `BandDirector.cpp`. Highest visible-during-play camera
   tell.

(Then, as a cheap Sonnet wave alongside: **N6** `%S %I SONGS` setlist-title
token + **N10** audio-output host config. **N4** song-select occluder and **N5**
crowd char slivers slot in after B/C as independent Opus dispatches.)

---

## Appendix — verification notes

- Boot reached gameplay on the v34 reproducer (mesh counts 173–315/frame during
  the song; gems flowing; HUD top-center). Consistent with SALVAGE_V33's
  173–261 band and the documented 3/3 clean-exit reliability at this build.
- The 195K MeshIB WebGPU errors are the dominant log content; everything else is
  drowned out — N1 is a real workflow tax, not just cosmetic.
- HX_NATIVE blocks V19a/V21/V23/V26 verified present in-tree at review time.
  Permuter-wipe remains the standing risk (SALVAGE_V33: ~32% of blocks wiped
  between sessions) — the proposed `scripts/native/audit_hx_native_blocks.sh`
  (SALVAGE_V33 §C) is still worth building.
