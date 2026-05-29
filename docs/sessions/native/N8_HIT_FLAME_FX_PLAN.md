# N8 — Gameplay hit / flame FX — implementation plan

**Authored:** 2026-05-29 (READ-ONLY planning subagent, Opus). No source edited,
no build, no commit. This doc is the only write.

**Item:** N8 from `STATUS_AND_NEXT_GOALS.md` §2 — "no hit-burst / flame FX appear
on note-hits; the strike-plate flare is static."

---

## 1. Root-cause / current-state analysis

**N8 is (a) PURELY A SYNTHETIC-INPUT GAP.** The native engine already renders
hit-FX particles. The reason no flames fire is simply that the headless run
never *hits* a gem: `nofail` keeps gems flowing but no input "plays" them, so
`GemSmasher::Hit()` / `hit.trig` never fires. Once a synthetic hit path runs,
the existing matched-fork → engine FX chain draws the flames. Confidence: HIGH
on the input side, MEDIUM-HIGH that the particles render visibly (the render
path is wired and complete; not yet eyeball-verified firing in a song).

### The hit-FX render chain (verified end-to-end, all present in-tree)

1. **Hit registration (gameplay):** `GemPlayer::Hit()`
   (`src/band3/game/GemPlayer.cpp:346-455`) on a successful gem →
   `mTrack->Hit(ms, gem_id, hitFlags)` (`:426`) + `mStatCollector.HitGem` (`:446`,
   score/streak ticks, `mGemStatus->mHits++`).
2. `GemTrack::Hit` (`src/band3/bandtrack/GemTrack.cpp:747`) →
   `GemManager::Hit` (`GemManager.cpp:1245` `gem.Hit()`, `:1257`
   `mNowBar->Hit(...)`).
3. `NowBar::Hit` → `GemSmasher::Hit()` / `HitChord()`
   (`src/band3/bandtrack/NowBar.cpp:104,113,162`).
4. **FX trigger:** `GemSmasher::Hit()` (`GemSmasher.cpp:84-87`) fires the
   `hit.trig` / `smash.trig` **EventTrigger** resolved in the smasher ctor
   (`GemSmasher.cpp:16-18`). The `gem_mash0..5` flame meshes + particle systems
   are children of this dir, animated by that trigger.
5. **EventTrigger is fully functional on native:** `EventTrigger::Trigger()`
   runs `LaunchParticles()` on its `mPartLaunchers`
   (`src/system/rndobj/EventTrigger.cpp:581-583`) + its anims. The only
   `HX_NATIVE` block (`:773-777`) is a harmless `wait`→`Symbol("wait")` rename;
   no FX no-op.
6. **Particles render on native:** `RndParticleSys::DrawShowing()`
   (`src/system/rndobj/Part.cpp:887-906`) has an additive `#ifdef HX_NATIVE`
   block (`:899-905`) calling `DrawParticlesBillboard(this)`, implemented in the
   engine at `milo-native-engine/src/platform/Part_Wgpu.cpp:144-319` — a real
   camera-facing billboard renderer (per-particle quad, UV tiling, blend-mode
   mapping, texture bind). So flame/burst particles *do* draw in the WebGPU
   layer once a system has active particles.

### The "audio" GuitarFx is a red herring

`src/band3/game/GuitarFx.{cpp,h}` drives `FxSend` audio effects (wah/reverb),
NOT the visual flame. The visual hit-burst is the `GemSmasher` / `gem_mash` /
`RndParticleSys` path above. N8's "flame FX" = the gem_mash particles.

### Why no flames today: the autoplay/hit switch is off

The clean retail seam for "the game hits gems for you" is **autoplay**
(kiosk/E3-demo mode). `Player::SetAutoplay(bool)` (`Player.h:100`, pure virtual)
→ `GemPlayer::SetAutoplay` (`GemPlayer.cpp:1388-1391`) → `mMatcher->SetAutoplay`
→ `BeatMatcher::SetAutoplay` (`BeatMatcher.cpp:374-377`) → `mWatcher->SetCheating(true)`
→ `TrackWatcherImpl::mCheating = true`. With `mCheating` set,
`TrackWatcherImpl::CheckForAutoplay()` (`TrackWatcherImpl.cpp:418-473`, called
every `Poll`, `:137`) calls `HitGem()` (`:452`) on each gem as it reaches the
strike window → `SendHit` → the full chain above. The headless run never sets
this, so `mCheating` stays false and no gems are ever played.

`Game::E3CheatAutoplayAccuracy()` (`Game.cpp:1041-1055`) is the exact in-engine
template: iterate `TheBandUserMgr->GetUserFromSlot(slot)->mPlayer`,
`SetAutoplay(true)`. `Game::DebugCycleAutoplay()` (`:1057-1127`) does the same
via `gPlayerStates`. There is also a DTA handler `{<player> set_auto_play 1}`
(`GemPlayer.cpp:2822`).

---

## 2. Explicit files-to-edit list

**Only ONE file is edited. N8 is glue-only.**

### (c) GLUE — `rb3/native/src/rb3_game_input.cpp`  *(shared-file — see dep-graph note)*

A new `autohit` verb (mechanism in §3). Exact edit sites — **these line ranges
are the dependency-graph contract for any batch item that also touches this file
(N7 load-hold shares it):**

| Edit | Current line range | What changes |
|---|---|---|
| Add `kVerbAutohit` to `enum VerbKind` | `:141` | append one enumerator |
| Add `kVerbAutohit` parse case in `ParseScript` | inside `:194-300` (new `else if (action == "autohit")` near the `nofail` case `:242-249`) | push a `Verb{kind=kVerbAutohit}` |
| `VerbReady` case for `kVerbAutohit` | inside `switch` `:482-532` (add case before the closing `:532`) | ready when `TheGame && !GetActivePlayers().empty()` |
| `VerbName` case | `:536-545` | return `"autohit"` |
| `DispatchVerb` case | `:549-557` | call `ExecAutohit()` |
| `ExecVerb` (HTTP path) parse | `:562-605` (add `if (verb == "autohit")` near `:571`) | call `ExecAutohit()` |
| New `ExecAutohit()` helper | new fn near `ExecNoFail` `:450-459` | the actual SetAutoplay loop |
| New includes | top `:21-46` | `#include "game/Game.h"`, `"game/Player.h"` (BandUser.h/BandUserMgr.h already included `:34-35`) |
| Per-frame re-arm (optional, §3) | new gated block in `RB3GameInputPoll` near the other per-frame fixes `:716-935`, gated `RB3_AUTOHIT` | re-assert autoplay on newly-spawned players |

**No matched-fork (a) edits.** **No engine (b) edits.** The FX render path
(`Part.cpp` HX_NATIVE block + `Part_Wgpu.cpp`) and EventTrigger are already in
place — N8 only needs to make hits happen.

---

## 3. Fix approach

### (i) Synthetic note-hit verb — `autohit` (the one mechanism)

Add an `autohit` verb (env gate `RB3_AUTOHIT` only needed if we add the
per-frame re-arm — the scripted one-shot verb is itself opt-in by being in the
script, so no separate gate is required for the basic case).

`ExecAutohit()` mirrors `Game::E3CheatAutoplayAccuracy` (`Game.cpp:1041-1055`)
exactly — the retail kiosk path, not a new mechanism:

```
void ExecAutohit() {
    if (!TheGame) { MILO_LOG("RB3 input: autohit FAILED: no TheGame\n"); return; }
    int n = 0;
    for (Player *p : TheGame->GetActivePlayers()) {   // Game.h:178
        if (p) { p->SetAutoplay(true); n++; }         // Player.h:100 (virtual)
    }
    MILO_LOG("RB3 input: autohit enabled on %d active player(s)\n", n);
}
```

`SetAutoplay(true)` → matcher `SetCheating(true)` → `CheckForAutoplay` fires
`HitGem` per gem at the strike window → `GemSmasher::Hit()` → `hit.trig`
particles. It also drives `FretButtonDown` for slot-press animation
(`GemPlayer.cpp:391-416`) and ticks score (`mStatCollector.HitGem`,
`mGemStatus->mHits++`). One verb gets BOTH the flames AND a live score tick (a
bonus visible win vs the static `0`).

**Readiness predicate** (`VerbReady` case `kVerbAutohit`): fire only once
`TheGame != nullptr && !TheGame->GetActivePlayers().empty()` — i.e. the song is
live and the synth user has been picked up as an active player (which only
happens after `track:guitar`). Place the `autohit` verb in the script AFTER
`nofail`, e.g. `@520:autohit`.

**Optional per-frame re-arm** (`RB3_AUTOHIT=1`): if players are rebuilt
mid-song (`Game::ReconcilePlayers`), a one-shot verb could miss the new player.
A cheap guarded block in `RB3GameInputPoll` that re-asserts `SetAutoplay(true)`
on any active player whose `IsAutoplay()` is false keeps flames firing all song.
Default OFF; the one-shot verb is the baseline.

### (ii) FX / particle render fix — NONE REQUIRED

The render path is already complete (`Part.cpp:899-905` →
`Part_Wgpu.cpp:144-319`). No engine edit. If verification shows particles do
*not* visibly fire, the fallback investigation is whether the `gem_mash` dir's
`RndParticleSys` is being `Poll()`'d (the `DrawShowing` `unke8 > 1` gate at
`Part.cpp:891` needs Poll to have run) and whether `LaunchParticles` is wired
on the `hit.trig` `mPartLaunchers` — but no code change is *planned*; this is a
watch-item, not a scoped edit.

---

## 4. Regression risks

- **Must not break the existing gameplay loop / gem streaming.** `autohit` only
  flips `SetAutoplay(true)` on already-active players — it does not touch gem
  spawn, the clock, or `nofail`. Gems still stream identically; they now get
  *played* instead of *passed*.
- **`nofail` interaction:** autoplay hitting every gem keeps the crowd meter up
  anyway, so `nofail` becomes belt-and-suspenders — harmless, keep both.
- **Score is no longer `0`:** intended (a visible improvement), but any
  screenshot baseline that asserted "score reads 0" must be updated. The HUD
  digit-rollup now exercises (previously untested headless).
- **Shared file `rb3_game_input.cpp`:** the new verb is additive (new enum
  value + new cases). The risk is a *merge* collision with N7 (load-hold) which
  edits the same `ParseScript` / verb-dispatch regions. Serialize or hand-merge
  these two — they are the only two items touching this file. The enum append
  and the new `switch` cases are append-only and conflict-light.
- **`TheGame->GetActivePlayers()` lifetime:** only called when `TheGame` non-null
  AND list non-empty (the readiness gate), so no null/empty deref.
- Permuter-safe: glue lives in `rb3/native/src/**` (clang, not permuter-owned).

---

## 5. Verification recipe

Reproducer (same as N8 brief, with `@520:autohit` appended; needs the deep
9000-frame run so gems reach the strike window — first reach ~song-time 3.3s):

```
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
MILO_MAX_FRAMES=9000 \
MILO_SCREENSHOT_DIR=/abs/dir(mkdir -p first) \
MILO_SCREENSHOT_FRAMES=1100,1200,1400,1800,2400 \
RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail,@520:autohit" \
/home/free/code/milohax/rb3/native/build-native/rb3-native
```

**Prove the hits register (log, deterministic):**
- `RB3 input: autohit enabled on N active player(s)` (N>=1).
- Score in the HUD ticks above `0` across the captured frames (vs the no-hit
  run's static `0`). `mGemStatus->mHits++` / `StatCollector::HitGem` running.

**Prove FX fire (visual, frames):** capture a tight burst of consecutive frames
right as gems cross the strike plate (e.g. `MILO_SCREENSHOT_FRAMES=1100..1130`
sampled every ~3 frames once a gem is at the plate) — a flame/burst should flash
at the smasher for 1-3 frames per hit, then fade. Particle billboards are short-
lived, so dense sampling near the strike line is required.

**A/B (the decisive test):** run the *identical* script with vs without the
trailing `@520:autohit` into two dirs. Diff: the autohit run shows flame bursts
+ rising score; the baseline shows static plate + score `0`. This isolates N8's
effect from all other rendering.

---

## 6. Honest assessment

**N8 is achievable as glue-only and is the cheapest "real win" of the open
list** — one new verb in `rb3_game_input.cpp`, zero matched-fork/engine churn,
and it unlocks BOTH the hit flames AND a live ticking score (a second visible
improvement the status doc lists as a separate HUD gap).

**The one honest caveat:** I verified the *render path exists and is wired*
(`Part.cpp:899` HX_NATIVE → `Part_Wgpu.cpp` billboard renderer; EventTrigger
functional) but I did NOT observe a flame actually drawn in a song (read-only).
There is a residual risk the `gem_mash` particle systems are not `Poll`'d in the
gameplay phase, or that `hit.trig` has no `mPartLaunchers` wired on this asset
(the flame could be a flip-book mesh anim rather than `RndParticleSys` — if so
it draws through the normal mesh path, which already works, so it would *still*
appear). Either way the hit *registers* and the score *ticks* deterministically
from the log — so even in the worst case (particles silently no-Poll'd) the
realistic minimum win is **"hits register + score ticks + slot-press
animation"**, with the flame burst as the high-probability bonus. Recommend
shipping the verb, then doing the dense-sampling visual A/B to confirm the
flames; if they do not appear, file a narrow follow-up on `gem_mash` Poll wiring
rather than expanding N8's scope.
