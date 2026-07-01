# verify-first-frame-flash — independent adversarial review (WAVE 5)

**Verdict: CONFIRM_WITH_RESIDUALS**

Reviewer: independent Opus adversarial pass on the composed master build
(rb3 `c6e6048d`, engine pin `15ce606`, binary `native/build-native/rb3-native`).
The first-frame-flash fix landed as engine commit **`c064ff4`** ("tame
gameplay-entry venue postproc white/pink first-frame flash") — the impl doc's
worktree commit `8db8b3a` cherry-picked onto the wave-5 pin. Soft-clip block
(`ppCeil=0.97`/`ppKnee=0.82`/`ppRolled`) and the `RB3_PP_OFF` gate are both
compiled into the shipped binary (verified via `strings`).

## What the fix is

A Reinhard soft-clip on the **venue POSTPROC composite OUTPUT** in `fs_postproc`
(`Rnd_Wgpu_RB3.cpp`, immediately before the final `return clamp(...)`), knee 0.82
→ ceiling 0.97. Opt-out `RB3_PP_OFF=1` disables the whole composite (renders
direct). Tunables `ppCeil`/`ppKnee`.

## Independent evidence (NOT trusting the impl numbers)

### Genuine pre-fix A/B (built a real BEFORE binary)

Built a true pre-fix binary in a paired throwaway worktree by reverting **only**
the 6-line soft-clip block (`strings` confirms no `ppCeil/ppKnee`, retains
`RB3_PP_OFF`). Ran an **interleaved N=6 batch on the same harness**, three configs
fired in parallel per round to beat the boot/venue lottery: `BEFORE` (pre-fix
composite), `AFTER` (default, soft-clip), `PP_OFF` (composite disabled).
Metric: `clipW` = % pixels ≥250 in **all** channels (the pink/white wash class),
over a ~30-frame burst at the `game_screen` flip.

| config | blowout >5% | >1% | meanPeak clipW | maxPeak clipW |
|---|---|---|---|---|
| **BEFORE** (pre-fix composite) | **3/6** | **6/6** | 8.88% | **32.7%** |
| **AFTER** (default soft-clip) | **0/6** | **0/6** | 0.39% | **0.42%** |
| `PP_OFF` (no composite) | 0/6 | 0/6 | 0.51% | 0.83% |

**(a) First-frame venue blowout — CONFIRMED gone.** Every pre-fix boot blows out
(6/6 >1%, 3/6 >5%, max 32.7%); the fix takes it to **0/6** with max peak 0.42%
(below the impl's claimed <0.55%). Visual decisive: `BEFORE_peak_blowout_clipW32.png`
= whole venue washed to a flat pink/white field, only highway/gems readable —
the exact user symptom. `AFTER_hot_reveal_readable.png` (the hottest AFTER boot,
peak lum 200) = strongly red-lit but **fully detailed** (band char, venue objects,
highway all readable), no white wash.

The bisection holds independently: `PP_OFF` (composite off) is also clean, so the
blowout is structurally in the intermediate→composite path, not a clear-color
transient (this corrects the wave-4 `verify-venue-blowout.md` clear-color framing,
as the impl claims).

### (b) Steady-state / song-select luminance — UNCHANGED (no dimming)

Settled-frame (last 10 of burst) mean luminance: **AFTER 51.0 vs BEFORE 46.6**
(AFTER if anything slightly *higher*; per-boot venue variance dominates, both span
~32–77). The soft-clip is verified mathematically **identity below the knee**
(in 0.0/0.4/0.8/0.82 → out unchanged; asymptote → 0.9676 = ppCeil < pure white),
so correctly-exposed venue/menu/song-select frames are untouched. The residual
AFTER clipW (~0.4%) is the UNGRADED HUD/highway/gem layer drawn after the
mid-frame flush — correctly unaffected by the composite clip.

Menu sweep (clipW%, lum, darkW%): main_hub `0.0, 93.3, 8.9` ; song_select
`0.0, 80.1, 2.2`. Both crisp and readable (`AFTER_main_hub.png`,
`AFTER_song_select.png`) — neon venue art visible, menu text sharp, song list +
album-art fallback box clean. The pure-white sliver drops (clipW 0.39→0.0) is
imperceptible, matching the impl claim.

### (c) Interaction sweep — no regression from the composed wave-4/5 fixes

The wave-5 menu-contrast change lowered the ambient floor for ALL venues. Swept
menu hub → song select → gameplay (lit venue) → score: **the hub is NOT
crushed-dark** (lum 93, healthy), song-select clean, gameplay venues lit (not
crushed, not blown). **18/18 batch boots reached `game_screen` (`ok:true`), zero
crashes / SIGSEGV / SIGABRT / asserts** across all run logs. `interactionsOk=true`.

## Residual (why CONFIRM_WITH_RESIDUALS, not plain CONFIRM)

The fix robustly **bounds the output** so the wash is impossible, and it does so
without dimming — that is fully confirmed. But the underlying *cause* (the native
lit-path running the song-start lighting reveal hotter than the Wii GX backdrop)
is NOT addressed and the impl honestly flagged this as unresolved. Consequence:
on hot-lit venues the first-frame reveal is still visibly **bright/saturated**
(e.g. a strong red tint at peak lum ~200; `AFTER_hot_reveal_readable.png`) — just
**readable instead of washed-to-white**. This is a structural exposure residual,
not a regression, and is the same neighborhood as the open venue-lighting
exposure-tuning backlog item. The soft-clip is the correct, robust bound for it.

## Wii match-neutrality

WGSL/engine-only change (`Rnd_Wgpu_RB3.cpp`, platform/gfx — not compiled into the
Wii DOL). No `src/band3` / `src/system` edits in the landing commits. Byte-identical
by construction.

## Evidence

`/tmp/rp5rev-first-frame-flash/` — `agg.py`/`steady.py` (analysis), `batch/*.out`
(raw per-boot frame data), `evidence/{BEFORE_peak_blowout_clipW32, AFTER_hot_reveal_readable,
AFTER_main_hub, AFTER_song_select}.png`. Throwaway BEFORE worktrees torn down.
