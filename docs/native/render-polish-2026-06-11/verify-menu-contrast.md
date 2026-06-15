# verify-menu-contrast — independent adversarial review (Wave 5)

**Verdict: CONFIRM_WITH_RESIDUALS**

Independent Opus reviewer, composed master build (HEAD `c6e6048d`, engine pin
`15ce606` which sits atop the menu-contrast commit `facaa6a`), binary
`/home/free/code/milohax/rb3/native/build-native/rb3-native` (built 2026-06-15
09:07). Own evidence under `/tmp/rp5rev-menu-contrast/`. Ports 9331–9339.

## What I checked / how

The fix is one engine commit (`facaa6a`, the landed form of the doc's worktree
`3bd4245`): in `BandRnd::WriteSceneUniforms` (engine `src/platform/Rnd_Wgpu_RB3.cpp`,
the `world.cam` + venue-light path) the three conservatively-bright floors were
lowered and made env-tunable:
- `RB3_VENUE_AMBIENT_FLOOR` 0.07 → **0.008**
- `RB3_VENUE_AMBIENT_CLAMP` 0.25 → **0.09**
- `RB3_VENUE_GREY_KEY` 0.6 → **0.22**

I confirmed the diff in the engine repo matches the doc exactly (no structural
change, same code path). My A/B is the single FIX binary, **default vs the old
floor values forced back via env** (`RB3_VENUE_AMBIENT_FLOOR=0.07 _CLAMP=0.25
_GREY_KEY=0.6`) — a clean control with no rebuild. Metrics use the implementer's
`measure.py` (= the wave-3 3×3-cell luminance-contrast method); I verified it
reads the retail hub ref at the published baseline (10.24:1 / dark-cell 0.035).

## (a) MENU HUB — CONFIRMED toward retail

50-frame hub loop, default vs old-floor, both on the FIX binary:

| metric (loop) | retail ref | OLD floor (pre-fix) | FIX default | dir |
|---|---|---|---|---|
| 3×3 contrast median | 10.24:1 | **2.32:1** | **6.77:1** | ✓ toward retail (≈2.9×) |
| contrast p75 / max | 13.5:1 | 2.99 / 7.25 | 8.34 / 11.11 | ✓ |
| frames ≥5:1 | — | **1/50** | **45/50** | ✓ |
| frames still ≤2.6:1 | — | **34/50** | **2/50** | ✓ |
| dark-cell median | 0.035 | 0.171 | **0.054** | ✓ crushed toward retail |
| dark-cell min | — | 0.081 | 0.026 | ✓ reaches retail black, no full crush |
| mean-lum median | 0.193 | 0.294 | 0.234 | ✓ toward retail |
| soft-green median | 3.4% | 3.13% | 2.85% | ✓ in-band (wave-4 fog NOT regressed) |
| hard-green median | 0% | 0% | 0% | ✓ no neon slab |

**Visual (decisive):** matched PALACE-neon hub camera —
`old_floor/hub_f0010.png` (brick + band figures lifted to a flat washed grey,
neon doesn't pop) vs `fix_default/hub_f0010.png` (deep warm-black brick, PALACE
neon glowing, figures read as dark forms). The FIX matches
`images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png` (dark backdrop + bright
neon hotspots) far better. Band close-up `fix_default/hub_f0048.png` (TIKI/ARCADE
shot): neon pops, geometry readable, NOT crushed. Wave-4 menu-lighting
(neon/signs) and wave-4 menu-fog (soft-green in-band, no green slab) are not
regressed.

**Residual (not a regression, matches the impl doc):** my loop median lands
**6.8:1**, the impl doc reports **8.4:1**, vs retail **10.24:1**. The 6.8 vs 8.4
gap is camera-loop sampling variance (50 vs 60 frames / different loop-phase
coverage) — same regime, both a strong move toward retail; the impl's 8.4 is on
the optimistic edge of the run-to-run spread but not misleading. The remaining
gap to ~10:1 is bright-side (neon hotspot strength = the wave-2 emissive/register
levers), independent of this floor lever.

## (b) CRITICAL no-regression — GAMEPLAY VENUES — CONFIRMED (with honest nuance)

The active venue is **small_club_01** (the `tv3_*_screen` names are just the entry
vignette transition; the engine log confirms `small_club_01_bank.milo` +
`ludclassic_small_club_resource` loading). I drove to gameplay twice (FIX default
vs old-floor env), 16-shot venue-camera burst each:

- Loop medians (NOT frame-matched — venue uses many cameras + disco color-wheel
  crowd light): FIX mlum **0.175** vs OLD **0.220**; crush% (L<0.02) **10.9%** vs
  **6.2%**; **std (detail proxy) 0.179 vs 0.164 — detail slightly HIGHER under
  FIX**, i.e. the unlit zones go dark but lit detail is preserved/sharpened, not
  flattened.
- The street/club venues **do darken in unlit zones** — this is the *intended*
  retail-faithful behavior, exactly as the impl claimed, and the highway (game.cam)
  is unchanged.
- Visual A/B: the authored-lit phases are carried fine — FIX `gp_A_fix/burst_01.png`
  (pink disco-light phase, band member, bright) vs OLD `gp_A_old/burst_00.png` (same
  camera but flat washed-pink-grey). FIX is more contrasty/atmospheric, NOT crushed.
  Even the darkest FIX frame (`burst_08.png`, mlum 0.120) keeps the chains/lamp/
  structure readable and the highway+gems brightly lit.
- A/B `RB3_VENUE_AMBIENT_FLOOR=0.07` cleanly restores the old (brighter, washed)
  venue, proving the lever is live and the default IS the fix.

Honest caveat: the two gameplay runs hit different camera/lighting phases so this
is a distributional, not frame-matched, comparison. No frame in either run showed
detail genuinely lost to black; the higher crush% under FIX is concentrated in the
far-from-light backdrop zones that retail also renders dark.

## (c) Score screen + walking-band outfits — CONFIRMED not crushed

- **Walking band (hub):** `fix_default/hub_f0048.png` — band/figures readable.
- **Score screen:** reached `coop_endgame_popups_screen` (score-detail fix works);
  captured FIX vs old-floor. FIX median mlum **0.493** vs OLD **0.223** (FIX
  happened to catch a brighter camera phase — the point is neither is crushed:
  crush <0.2% / nblk <0.5% under FIX, <1.3% / <6.4% under OLD). Band member +
  "7 goals completed" widget clearly lit in both (`score_fix2/score_1.png`,
  `score_old2/score_2.png`). The endgame celebration scene is well-lit; the floor
  lever does not crush it.
- **Song select:** unchanged (FIX mlum 0.305 vs OLD 0.317 = noise) — it draws
  under ui/overshell cams, not world.cam, so the venue floor never touches it.
  `gp_A_fix/01_song_select.png` renders clean (header digits / FRIEND-RANKINGS /
  album-box all fine — the separate wave-5 songselect-ui fix).

## interactionsOk = true

Full sweep menu hub → song select → gameplay (small_club) → score screen showed no
regression from the OTHER wave-4/5 brightness fixes: menu-fog soft-green stays
in-band (2.85% median, no wash), no neon green slab (hard-green 0%), venue
soft-clip / postproc-clip compose without new blowout, gems/highway bright. Score
screen reaches and is stable (score-detail fix intact).

## Residuals / notes (none are regressions)

1. Loop-median contrast 6.8:1 (mine) / 8.4:1 (impl) vs retail ~10:1 — bright-side
   gap, independent of this lever. Hence CONFIRM_WITH_RESIDUALS not plain CONFIRM.
2. Several songs SIGABRT on track load (`SongData::TrackInfo` vector OOB) —
   pre-existing, song-specific, reproduces with the old env values too; unrelated
   to lighting (also noted in the impl doc + PLAN).
3. The gameplay-venue A/B is distributional (multi-camera + disco color-wheel),
   not frame-matched; the drop-detail risk is mitigated by std staying high under
   FIX, but a frame-matched single-camera A/B would be a cleaner future gate.

## Bottom line

The menu-hub contrast is genuinely raised toward retail (~2.3→6.8:1 median,
dark-cell 0.17→0.054, neon pops against deep blacks) and the all-venue ambient-floor
lowering does NOT crush gameplay venues or the score screen — lit zones are carried
by their authored lights, unlit zones darken as retail does, detail is preserved.
The change is engine-only / Wii byte-identical by construction (rb3 commit is the
pin bump only). The only residual is the bright-side gap to ~10:1, which is a
separate (wave-2 emissive) lever. **CONFIRM_WITH_RESIDUALS.**
