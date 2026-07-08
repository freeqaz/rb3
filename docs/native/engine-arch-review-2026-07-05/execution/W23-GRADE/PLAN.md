# Lane GRADE — PLAN (Wave 23, SWEEP S2)

**Bug (SWEEP S2):** main_hub reads opaque-bright / washed grey-green *daytime* vs
retail's dark-night neon glow (`yt_mhKNp9uAT48_menu_hub.png`, 360/PS3 — RELATIVE
metrics only per A9). Neon signs + mats are correct/present; it is the GRADE /
LIGHTING.

**Mechanism candidates (A1, from `WAVE23_REVIEW.md` Q1):**
- **(a) PP grade wash** — `RB3PostProc.cpp` fs_postproc; grade also DESATURATES
  (wgsl `:222` mid_sat). Arm: `RB3_PP_OFF=1`.
- **(b) venue-light non-engagement / regression on the hub env** — the hub
  backdrop rides the shipped world.cam VENUE-LIGHT path (`Rnd_Wgpu_RB3.cpp:1573`
  `sVenueLightEnabled && world.cam && venv->mAmbientFogOwner`). If it stops
  engaging, frames fall to the flat "1.0 white dir + 0.45 grey ambient" flood
  (`:1730-1749`) = the wash. **PRIME SUSPECT.** Arm: `RB3_VENUE_LIGHT_OFF=1`
  (if hub look does NOT change → venue path not engaging on hub).
- **(c) authored / no-fix** — still meets the wave-5 tuned contrast envelope →
  residual, close as known-limitation (NOT "Wii brighter"; no Wii hub GT).
- **(d) UI post-grade** — `RB3_UI_POST_GRADE_OFF=1`. DEDUPE; expected NULL on the
  backdrop (its direction re-washes UI text, can't darken the backdrop).

## STEP 0 — discriminator (FOUR arms + probe, ONE boot each)

Harness: `scripts/native/_grade_hub_discriminator.py` (boots main_hub, fixed
clock, settles to a deterministic frame, screenshot + stderr log).

| arm | env | expectation |
|---|---|---|
| baseline | (none) | current washed hub |
| A_PP_OFF | `RB3_PP_OFF=1` | grade contribution (desat/tone) removed |
| B_VENUE_OFF | `RB3_VENUE_LIGHT_OFF=1` | if UNCHANGED → venue path not engaging (prime) |
| C_UIGRADE_OFF | `RB3_UI_POST_GRADE_OFF=1` | expected NULL on backdrop (dedupe) |
| D_WASH_PROBE | `RB3_WASH_PROBE=1 RB3_VENUE_PROBE=1` | reads engagement + grey-key digest |
| E_FALLBACK_FIX | `RB3_VENUE_FALLBACK_FIX=1` | if it ALONE darkens → flood is flat-default fallback |

**A2 numeric gate:** 3x3 grid mean-luma CONTRAST metric (max_cell/min_cell) on
each arm's hub screenshot, reported vs history (2.6:1 pre-fix → ~10:1 target).
This decides regression-vs-residual NUMERICALLY.

## VERDICT (checkpoint BEFORE any fix)

(a) PP grade wash | (b) venue-light non-engagement/regression on the hub env |
(c) authored/no-fix (residual, known-limitation).

## THE FIX (only if (a) or (b); flag-first default-OFF)

If venue-light isn't engaging on the hub → make it engage (or fix the fallback
flood) match-neutrally, new knob default-OFF. **A3 guardrail:** NO edits inside
game.cam / kGamePlaying-gated branches (shipped gameplay grade + venue-lighting +
UI-post-grade defaults stay intact).

## GATES (A2/A3)

- numeric hub 3x3 contrast ON/OFF vs history
- matched-frame E1 vs GT (STRUCTURAL: contrast ratio, neon-vs-backdrop; NOT
  absolute luminance)
- gameplay matched A/B (grade non-regression) + UIGRADE flush-count parity
- song_select parity band (RB3_UI_POST_GRADE 1.110→1.049 must not worsen)
- drawlog-792 flag-OFF byte-identical
- batch_objdiff == baseline on touched src/system units

## Re-scope note
Report whether the grade finding re-scopes SWEEP S4 (player1 avatar crop) — S4
is "probably a non-bug once S2 grade is addressed" per SWEEP.
