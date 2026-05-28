# Song-load / transition "loading screen" render — investigation (V18)

**Date:** 2026-05-28
**Goal:** Get the song-load transition (the window between confirming a song and
gameplay starting) to render like retail RB3 — title/art/spinner/cinematic, not
black/frozen/missing.
**Reproducer:** the verified full song-load chain (see below).
**Screenshots:** `docs/sessions/native/screenshots/v18-load-screen/`

```
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=24000 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
  build-native/rb3-native
```

---

## TL;DR

The retail "loading screen" the player sees while a song loads is **the cinematic
transition vignette** (`tv3_b` for a video-class venue) playing over a few seconds
while `BandPreloadPanel` reads the song's MIDI/.milo/upgrade files in the
background. In retail those file reads take seconds (disc seek + content mount +
decompress), so the vignette plays in full and the transition lingers visibly.

Natively the song's files load **essentially instantly** (direct `FileStream`
reads + the `PreloadPanel::CheckFileCached` HX_NATIVE always-true patch + no disc
latency), so the whole `part_difficulty_screen → preloading_screen → tv3_b_screen
→ game_screen` chain completes in **~3 frames** (frame 451→456). The
`preloading_screen` is committed as the current screen for **~1 poll cycle** and
the `tv3_b` vignette is shown **frozen on a single static pose for ~3 frames**
(f454/f455/f456 are byte-identical) and never animates.

So nothing is "broken" / black / culled / mis-cameraed. The renderer draws the
scene correctly the whole time (≈968 meshes during the song-select+vignette,
≈307 meshes once `game_screen` commits). The gap is purely **timing**: the load is
too fast for the loading display to be visible. This is not a rendering bug; it is
a "load is instant so the loading UI flashes by" situation, and fixing it is a
deliberate UX/timing change, not a small render patch.

---

## Exact load-window frame range (this reproducer)

Screen trace (`RB3GameInputPoll`, logs every `CurrentScreen()` change; raw
transition states via `UISCREEN_DBG=1`):

| Frame | Committed screen | Notes |
|-------|------------------|-------|
| 352   | `part_difficulty_screen` | song / difficulty select committed |
| ~451  | `part_difficulty_screen` | `@450 end_override_flow` fires the `override_ended` confirm path → `{ui goto_screen preloading_screen}` |
| ~452  | **`preloading_screen`** (1 poll cycle) | `cur=preloading_screen trans=tv3_b_screen` appears for ONE state-log entry; `BandPreloadPanel` loads instantly, `on_preload_ok` → `goto_screen game_screen` → tv3 vignette |
| 453   | `tv3_b_screen` (in transition) | cinematic transition vignette |
| 456   | `game_screen` | committed; transition done by f458 (`trans=0`) |
| ~456–~590 | `game_screen` | dark venue + smasher, song-intro pre-roll, no gems yet |
| ~590+ | `game_screen` | highway + smasher fully lit; gems reach the window at song-time ~3.3 s |

**The "load window" proper is frames ~451–456** (preloading_screen + tv3_b
vignette). Everything after f456 is `game_screen` already committed.

---

## Honest per-frame visual description (sampled across the whole window)

Tight sweep (`tight_fNNNN.png`):

- **f0450 / f0451 / f0452 / f0453** — still the **song-select / part_difficulty
  screen**: the song-list rows, the difficulty card on the left ("BASS"/"GUITAR"),
  and the header reading `%S %I SONGS` / `SETLIST (2 SONGS)` (un-localized format
  token — pre-existing Localize gap, not load-specific). From f451 on, the `tv3_b`
  vignette wipes in from the right as bright diagonal motion-blur streaks.
  The `transition_fallback_panel` spinner is NOT shown (correct — in the normal
  load path `show_fallback` is FALSE; the fallback spinner only appears when the
  cinematic vignette can't play).
- **f0454 / f0455 / f0456** — the **`tv3_b` transition vignette**, held on a single
  frozen pose: a dark abstract scene with maroon/red horizontal bands over a
  teal-green reflective floor (the "video" venue transition geometry). These three
  captures are **byte-identical** → the vignette is frozen, not animating.
- **f0458 / f0460 / f0480 / f0510 / f0540** — **`game_screen`, song clock ≈0**:
  near-black. Only overshell chrome is visible — the vertical overshell slot on the
  left edge, the profile/instrument widget top-right, a "Wii Name" placeholder, and
  the sideways `SHELL_PRESS_START_TO_ROCK` token (another un-localized token).
  The gameplay highway/smasher have not appeared yet.
- **f0570 / f0600 …** — **`game_screen` gameplay**: the highway + 5-fret smasher
  bar render correctly (yellow rails, color gem-pads, blue strum bar). Song-intro
  pre-roll; gems enter the visible window a bit later (~song-time 3.3 s, as
  documented in the gameplay sessions).

Wide sweep `01_f0450 … 30_f2200` confirms the same: black-ish game_screen through
the early frames, gameplay fully lit from ~f570 on.

---

## Retail vs native gap

| | Retail (Wii disc) | Native (x86_64, direct FileStream) |
|--|--|--|
| `preloading_screen` committed | seconds (waits on disc/content load) | ~1 poll cycle |
| What plays during load | `tv3_b` cinematic vignette, full camera animation | same vignette, **frozen on 1 pose for ~3 frames** |
| Fallback spinner (`transition_fallback_panel`) | only if vignette unavailable | same (hidden in normal path) |
| Net visible duration | multi-second loading sequence | ~3 frames; the player would never perceive it |

The native renderer reproduces the retail scene content correctly — it is the
**load duration** that diverges, which collapses the loading display to a blink.

---

## Root cause (with citations)

1. **The transition chain is correct and matches retail.**
   `ui/seldiff/seldiff.dta` `override_ended` (confirm, `$cancel=FALSE`) runs
   `{ui goto_screen preloading_screen}`. `ui/loading/loading.dta`
   `preloading_screen` (a `BandScreen` with panels `meta preload_panel
   transition_fallback_panel`) runs `BandPreloadPanel::Load()`, and on
   `on_preload_ok` does `{ui goto_screen {gamemode get game_screen}}`.
   `config/vignettes.dta` wraps `preloading_screen → game_screen` (and
   `part_difficulty_screen → *`) in `DIFFICULTY_TO_GAME_TRANSITION` → `tv3_b` for a
   video-class venue. The UI state machine
   (`src/system/ui/UI.cpp` `UIManager::PollTransition` / `GotoScreenImpl`,
   `kTransitionTo → kTransitionFrom`) commits `mCurrentScreen` and draws it via
   `UIManager::Draw()` → `mCurrentScreen->Draw()`.

2. **The native load completes too fast for `preloading_screen` to linger.**
   - `src/system/meta/PreloadPanel.cpp:201` `CheckFileCached` — the HX_NATIVE block
     (lines 204-216) returns `true` on a cache miss ("files are read directly via
     FileStream, so not-in-cache does not mean missing"). So the preload's
     file-cache staging is a no-op and reports loaded immediately.
   - `BandPreloadPanel::IsLoaded()` (`src/band3/meta_band/BandPreloadPanel.cpp:40`)
     gates only on `PreloadPanel::IsLoaded() && !mLockInProgress`; the LockStepMgr
     resolves locally on the same/next frame offline.
   - So `preloading_screen->CheckIsLoaded()` flips true within ~1 poll, the
     `UITransitionCompleteMsg` fires `on_preload_ok` → `goto_screen game_screen`,
     and the `preloading_screen` is superseded before it is on screen for more than
     a frame.
   - The `UISCREEN_DBG=1` trace shows exactly this: `cur=part_difficulty_screen
     trans=preloading_screen` → `cur=preloading_screen trans=tv3_b_screen` (single
     entry) → `cur=tv3_b_screen trans=game_screen` → `cur=game_screen`.

3. **The `tv3_b` vignette is frozen, not animated**, because the transition
   resolves in ~3 frames — the `InterstitialPanel`/vignette `PropAnim` never gets
   enough poll cycles to advance, and `game_screen->CheckIsLoaded()` (gated by
   `SyncGameStartPanel` mState 0→2, which the native `NetSession::StartGame`/
   `EnterInGameState` shim in `native/src/rb3_netsession_native.cpp:115-127` drives
   synchronously) becomes true almost immediately.

4. **The `transition_fallback_panel` spinner is intentionally hidden** in the
   normal path (`loading.dta` `preloading_screen.enter` sets
   `transition_fallback_panel set_showing [show_fallback]` with `show_fallback`
   FALSE; only `start_loading_quick_anim` when the cinematic fails). Its milo
   (`ui/loading/gen/transition_fallback.milo_xbox`) is the `meta_loading_circle`
   rotating spinner + `loading.lbl`/`waiting.lbl` — a fallback, not the primary
   loading visual.

**No rendering defect:** no missing panel instancing, no frustum/DrawBudget cull,
no wrong camera, no missing asset. The scene draws (968 → 307 meshes across the
window). The only divergence is load *duration*.

---

## Fix plan (deliberate — NOT applied; left for a follow-up dispatch)

Because the load is genuinely instant, "render the loading screen like retail"
means *making the loading display visible long enough to see*. Options, in
recommended order:

1. **(Recommended, glue-layer, opt-in) Hold the `preloading_screen` for a minimum
   wall-clock duration** so the cinematic vignette plays and the loading sequence
   is perceptible — e.g. gate `BandPreloadPanel::IsLoaded()` / the
   `preloading_screen` transition on a `getenv("RB3_LOAD_HOLD_MS")` (default 0 =
   current instant behavior). This is the cleanest place: it does not touch the
   matched-fork transition machinery, it is opt-in, and it lets a reviewer
   *observe* the loading screen on demand. Candidate seam: an HX_NATIVE block in
   `BandPreloadPanel::IsLoaded()` (`src/band3/meta_band/BandPreloadPanel.cpp`) — but
   note that is a matched-fork file, so the edit must be additive `#ifdef HX_NATIVE
   … #else … #endif` with the `#else` byte-identical. A cleaner alternative is a
   per-frame hold injected from `native/src/rb3_game_input.cpp` (glue, no permuter):
   stall the synthetic `end_override_flow`→game advance and/or pump extra frames
   while `currentScreen == preloading_screen`/`tv3_b_screen` so the vignette
   animates and a screenshot sweep can capture it.

2. **(If the cinematic vignette is the desired loading visual) Verify/advance the
   `tv3_b` vignette `PropAnim`** under the held window. Once the screen lingers
   (option 1), confirm the `InterstitialPanel` camera animation actually plays
   (the frozen pose at f454-456 is only because it had ~0 frames). If it still does
   not animate with time to spare, that becomes a separate, real render/anim bug to
   chase (likely `RndPropAnim`/`UITransitionHandler` timing).

3. **(Cosmetic, separate from load) Force `show_fallback TRUE`** to render the
   `transition_fallback_panel` spinner during the held window if a literal
   "spinner + waiting label" loading screen is wanted instead of the cinematic.
   This is a content/UX choice, not a correctness fix.

4. **Out of scope for load, but visible in these shots:** the un-localized tokens
   `%S %I SONGS` and `SHELL_PRESS_START_TO_ROCK` are pre-existing Localize gaps
   (overshell + song-select), tracked separately — they are not part of the load
   screen.

### Why no fix was applied here
The task constraints allow only a *small, self-contained, clearly-correct* fix.
Making the loading screen "render like retail" is a behavioral/timing change
(introduce an artificial load hold and/or pump vignette animation frames) with a
real design decision attached (how long to hold; cinematic vs spinner). That is
not a one-liner and it touches either a matched-fork file or the synthetic-input
pacing — better done as a focused implementation dispatch with the option-1 seam
above. Documented precisely here for that follow-up.
