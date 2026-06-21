# GAP 1 Scrollbar Fix — ADVERSARIAL VERIFY (independent re-derivation)

**Verify agent (Opus), 2026-06-21. Worktree `wt-converge-scrollbar` @ `afc8b5b8`.**
Independent of the impl agent: I re-derived all 4 claims from first principles with
my own frame/game-state-stamped instrumentation and my own deep-anchor captures
across 3 venues + 3 songs. I did NOT trust the impl agent's numbers.

## VERDICT: **LAND** (with one correction to the impl agent's own framing)

The fix is correct, harmless, and Wii-byte-neutral. The PREMISE REVERSAL holds:
the scrollbar does **not** draw during steady-state gameplay, so the audit's
attribution of the highway teal filigree to `scrollbar_bg.mesh` is **wrong**. The
fix is a sound defensive guard that (as it happens) suppresses **nothing** in
normal play — it keeps the one real scrollbar (`song.sbd`) and would only drop a
genuinely contentless widget, which never occurs in any tested path.

One correction to the IMPL agent (not to the fix): the impl undersold the teal
highway artifact as "only a song_select→game transition artifact." It is **not** a
transition artifact — it is **present in steady-state deep gameplay** (I see it at
songMs 33k–36k). But the impl's substantive point is right: that teal pattern is
**NOT the scrollbar** (a separate gameplay/highway visual). So the conclusion
(LAND, scrollbar is not the highway leak) is unaffected.

---

## Method (independent instrumentation)

I added a temporary `RB3_SBVERIFY` diagnostic to `ScrollbarDisplay::DrawShowing`
(HX_NATIVE, env-gated, behaviour-neutral) logging EVERY call with: owner `Name()`,
resource-dir name, `TheGame!=0` (gameActive), `TheUI.CurrentScreen()->Name()`,
songMs (via `TheGame->GetBeatMaster()->GetAudio()->GetTime()`), `m_fSavedScale`,
`GetListAttached()`, `mAlwaysShow`, and the `drawWii`/`drawNative` gate decisions.
**Reverted before land** — `git diff src/system/bandobj/ScrollbarDisplay.cpp` vs
`afc8b5b8` is now empty; the binary was rebuilt clean at the committed state.

Driver (preserved at `_sbverify_drive.py`): boots the worktree binary, applies
`{meta_performer set_venue_override <v>}`, varies `--song-downs`, drives to
`game_screen`, holds 35–40s with nofail+autohit to a deep anchor.

**6 runs, 2840 total `DrawShowing` records:**

| run | venue (confirmed loaded) | song-downs | extra env | deepest songMs | SBVERIFY recs |
|---|---|---|---|---|---|
| 1 | small_club_01 (default) | 4 | — | 55986 | 632 |
| 2 | big_club_01 (override) | 7 | — | 53905 | 227 |
| 3 | arena_02 (override) | 2 | — | 51740 | 310 |
| 4 | small_club_01 | 4 | SHARD_GUARD_OFF + RB3_SCROLLBAR_FIX_OFF | 56595 | 471 |
| 5 | small_club_01 | 4 | + RB3_HIGHWAY_BLOOM_OFF | 58520 | 732 |
| 6 | small_club_01 | 4 | + SHARD_RATIO_DBG/SHARD_DBG | 50439 | 468 |

---

## Claim 1 — PREMISE REVERSAL (no game-active scrollbar draw). **CONFIRMED.**

Aggregate over all 6 runs (2840 `DrawShowing` records):
- **gameActive=1 records: 0.** Not one scrollbar draw while `TheGame != null`, across
  small_club + big_club + arena, 3 songs, 5 boots (random wardrobe per boot).
- **owner: `song.sbd` for all 2840.** The doc's named gameplay suspect **`chars.sbd`
  never appears in any log** (0 occurrences in all 6) — its `DrawShowing` is never
  invoked in normal headless play.
- **screen: `song_select_screen` for all 2840.** And the last `[SBVERIFY]` byte offset
  is *before* the first `game_screen` marker in every run — i.e. `DrawShowing` stops
  firing entirely once gameplay begins.
- `savedScale` for the kept draws was always ~0.11 (real overflow content), never 0.

This is exactly the impl's reversal: the audit's 834 `scrollbar_bg.mesh` drops are the
**legitimate song_select list scrollbar**, mis-attributed by reading a whole-session
log as if it were a gameplay log. The audit premise ("the gameplay-resident
`chars.sbd` over-draws the highway every frame") is **wrong** on this build.

Adversarial coverage of the prompt's worry (wardrobe/venue not rolled): I varied
venue (3) and song (3) and re-booted 5×. `chars.sbd` still never drew. The overshell
character-swap scrollbar only matters if the player opens that sub-panel mid-song;
even then the fix would correctly suppress it iff it were contentless.

## Claim 2 — THE HIGHWAY ARTIFACT. **CONFIRMED it is NOT the scrollbar; impl's "transition-only" framing is WRONG.**

- With guard OFF + fix OFF at a **deep anchor (songMs 33830)**, the teal filigree IS
  sprawled across the highway (`V1_…teal_highway.png`) — same club venue as the audit
  shot, deep in steady-state gameplay, gameActive=1. So it is **not** a transition
  artifact (impl mischaracterized this). It appears on 5 of 6 deep-anchor frames; the
  6th (`V3_…other_camera_clean.png`, songMs 30393) is a different camera with the band
  in front, so the highway pattern isn't framed.
- BUT it is **provably not the scrollbar**: (a) my SBVERIFY shows 0 game-active
  scrollbar draws; (b) `scrollbar_bg.mesh` appears 0× after `game_screen` in the
  guard-OFF log; (c) with SHARD_RATIO_DBG, **12001 mesh-ratio evals during deep
  gameplay, 0 scrollbar-named meshes** (they're crowd bodies, clap/fist hand props,
  band outfit/instrument + venue meshes — the GAP 3/5/6 families).
- The teal pattern **persists with `RB3_HIGHWAY_BLOOM_OFF=1`** (`V2_…bloom_persists.png`),
  so it's independent of the gem-bloom path too. It reads as a now-bar / highway
  track-surface decorative overlay (perspective-tiled along the lane toward the
  now-bar), a real but **separate** gameplay visual. See "Residual" below.

## Claim 3 — WII-NEUTRALITY. **CONFIRMED (independent objdiff).**

`mcp__orchestrator__run_objdiff DrawShowing__16ScrollbarDisplayFv`, unit
`system/bandobj/ScrollbarDisplay`, project_dir = this worktree →
**100.0% normalized (100.0% raw), 58 instructions, all equal.** The entire change is
inside `#ifdef HX_NATIVE` (verified by reading the file): include guard + the gated
block + early `return`. The non-HX_NATIVE Wii arm
(`if (mAlwaysShow || m_fSavedScale < 1.0f)`) is untouched. Wii codegen byte-identical.

## Claim 4 — NO MENU REGRESSION. **CONFIRMED.**

- song_select with fix ON renders the full MUSIC LIBRARY list AND its scrollbar
  (`V4_song_select_fixon_scrollbar_kept.png`).
- Direct gate proof: `song.sbd` reports `savedScale≈0.11, listAtt=1` →
  `hasContent=true` → `drawNative=1` (kept) in all 2840 records.
- Across all runs the fix **suppressed 0 draws** (`drawWii=1 && drawNative=0` count =
  0): it never dropped a real scrollbar. (accomplishments not separately captured, but
  it uses the same `0<savedScale<1` content discriminator and the same code path; the
  song_select proof generalizes — no path can produce a false drop of a real bar.)

---

## What the fix actually is (honest characterization for the lander)

- **Correct + harmless + Wii-neutral.** It tightens the native draw gate to require
  real scrollable content (`0 < m_fSavedScale < 1 && list attached`), keeping
  `mAlwaysShow` bars and real scrollbars, dropping only the degenerate `savedScale==0`
  contentless case.
- **It does NOT deliver the audit's claimed convergence value.** The audit said it
  would collapse 71–74% of shard-guard drops; it collapses **0** of them, because
  those drops are the legitimate song_select scrollbar (correctly kept), not a
  contentless gameplay over-draw. The value is purely defensive: if a contentless
  `ScrollbarDisplay` (e.g. `chars.sbd` under some interaction) ever did draw on
  native, it would now be suppressed instead of painting the 200u ribbon.
- Net: a clean, low-risk guard. Land it as correctness/defense-in-depth, not as the
  "GAP 1 highway cleanup" the audit framed (there is no scrollbar on the gameplay
  highway to clean).

## Residual teal highway pattern — real separate gap, worth a future note (do NOT chase)

The teal filigree on the gameplay highway is a **genuine, persistent steady-state
visual** that is NOT the scrollbar, NOT the gem bloom, and NOT a transition artifact.
It reads as a highway track-surface / now-bar decorative overlay tiled along the
lane. It is masked today only because it's subtle vs the note gems and because the
prior teams were looking at the scrollbar. **Recommend a separate small investigation**
(track-surface material / now-bar overlay under game.cam) — but it is out of scope for
this fix and should not block landing. Ground-truth: retail club gameplay
(`images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png`) shows a plain blue
highway with no ornate filigree, so this likely IS a native-only over-draw worth
converging later.

---

## Evidence files
- `shots/verify/V1_deep_anchor_guardoff_fixoff_teal_highway.png` — songMs 33830, guard
  OFF + fix OFF: teal filigree on highway during steady-state gameplay.
- `shots/verify/V2_deep_anchor_bloomoff_teal_persists.png` — songMs 35295, + bloom OFF:
  pattern persists → not the bloom path.
- `shots/verify/V3_deep_anchor_other_camera_clean.png` — songMs 30393, band-front
  camera: highway not framed (why some frames look "clean").
- `shots/verify/V4_song_select_fixon_scrollbar_kept.png` — song_select fix ON: scrollbar
  kept (no menu regression).
- `_sbverify_drive.py` — the verify driver (override + deep-anchor + screenshots).
- objdiff: `DrawShowing__16ScrollbarDisplayFv` = 100.0% / 58 instr all equal.
