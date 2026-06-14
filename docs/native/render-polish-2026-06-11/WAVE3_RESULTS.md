# Wave 3 — Verification + C8 deep dive: RESULTS

Wave 3 ran 7 adversarial verifiers against the **composed** master build (rb3
`c011c886` / `cca1869a`, engine pin `469c550`, binary built 2026-06-11 21:00) plus a
C8 character deep dive. The workflow was **paused mid-run** (the C8 agent had finished
its work; Fable became unavailable). All 7 verify docs + the recovered
`scout-c8-rotation-basis.md` are on disk. This file synthesizes them.

## Scoreboard (composed build)

| issue | wave-2 claim | wave-3 verdict | note |
|---|---|---|---|
| highway-offset | fixed | **PASS** | now-bar centered (`verify-ui-trio` CHECK 3) |
| all-inst-crash / vocals | fixed | **PASS** (caveat) | vocals load + HUD render + play; full-song-to-end still aborts at endgame — **PRE-EXISTING + instrument-agnostic** (guitar hits it too), not a vocal regression |
| diff-grid | fixed | **PASS** | icons centered on dot rows; normal text unshifted (`verify-ui-trio` CHECK 1) |
| crowd | fixed | **PASS** | full-bodied, spread, animating; +1 residual, +1 shifted-load note |
| gem tails / colors / flicker | fixed | **PASS** ×4 | approach tails render; gems saturated + halo present; 0 dropout/blackout frames in 160; held tails refresh every frame |
| fret-held | fixed | **PASS** *but* | smasher glows correct per-slot color in `verify-ui-trio`'s venue (+38–53 brightness, opt-out = noise floor) — **yet a NEW regression appears in another venue** (white-sphere, below) |
| menu-lighting | fixed | **PARTIAL** | green slab dead + unlit/emissive at retail brightness, **but** interacts with the Part.cpp fix → revived fog wash; contrast win does not reproduce on composed build |
| neon-slab / Part.cpp | fixed | **PARTIAL** | the "slab" (street-fog particles) is gone, but the same Part.cpp sim fix **revives** other fog systems that now over-render on the menu |
| char-render | partial (expected) | **PARTIAL** | C8 deep dive now has root cause + a measured, byte-identical candidate fix (not landed) — see `scout-c8-rotation-basis.md` |
| **venue-wash** (new, from smoke frame) | — | **FAIL** | gameplay venue "pink wash" is a **lighting BLOWOUT** (soft-clip white-out of an authored red/pink moment); root cause = unbounded lighting sum in the shared standard shader; wave-2 changes **exonerated** (amplifier predates wave 2) |

**Net:** 6 of the 8 original issues verified fixed on the composed build; `char-render`
is root-caused with a fix ready to land; `menu-lighting` is partially regressed by an
interaction. Verification also surfaced **4 new/residual issues** (below).

## NEW issues surfaced by wave-3 (queued for the next wave)

1. **fret-held white-sphere (wave-2 regression, venue-dependent).** In 20th Century
   Boy / its venue, the held-fret glow renders as a **giant shaded WHITE BALL
   (~110px)** hovering above the now-bar, occluding gems — instead of the small
   per-slot colored glow it correctly shows elsewhere. 4-way attribution
   (`verify-gems`): present only with the wave-2 glow on (`RB3_FRET_GLOW_OFF=1` → 0
   frames; pre-wave-2 binary → 0). Suspect: `square_smasher_bright_*.tex` not bound
   in this venue path → the white emissive fallback (engine `8874e77`) dominates and
   the ×2 boost oversizes it; possible interplay with emissive-on-all-cams
   (`7acc22a`). **Mitigation in place:** `RB3_FRET_GLOW_OFF=1`. Fix = bind the bright
   tex per-venue and clamp scale/color; re-verify fret-held **per venue**, not just one.

2. **menu fog wash (interaction: Part.cpp × menu lighting).** `verify-menu-hub` PARTIAL:
   the Part.cpp InitParticle sim fix resurrected street-fog particle systems that were
   broken-absent (flown to y≈−11,800) in the build where menu-lighting's contrast win
   was measured. Revived fog now renders as a dense green-grey full-frame wash over part
   of the hub camera loop. Composed-build contrast = **3.9:1** (retail ~10:1), not the
   wave-2-claimed 12.5:1. Fix direction: the fog is now *physically correct* but
   *too dense/foregrounded* on the menu — tune emitter density/placement or the menu
   environ, re-baseline contrast after.

3. **venue lighting BLOWOUT (pre-existing, shared shader).** `verify-venue-wash` FAIL:
   the authored red/pink small_club moment soft-clips to white and destroys texture
   because the standard shader's lighting sum is **unbounded** (retail GX hardware
   cannot blow out this way). Root cause is the **P4 venue-light path + the non-venue
   white-flood fallback**, both of which **predate wave 2** — the wave-2 outer-halo
   bloom / unlit+emissive / mesh-cache changes are individually exonerated. Fix lives
   in the shared standard shader (clamp/tone-map the lighting sum) — engine change.

4. **endgame abort (pre-existing, instrument-agnostic).** `verify-vocals`: a full song
   played to completion aborts at the endgame/score screen, for guitar as well as
   vocals. Matches the crowd scout's earlier `ui/endgame/endgame_helpers.dta(64):
   meta_performer` SIGABRT. Not a vocal regression; blocks score-screen testing.

### Residuals / notes (non-blocking)
- **crowd:** ~6.7k residual guard drops remain (down from 63k); `crowd-shot-capture.py`
  does not actually frame the crowd in this build (verified via natural crowd-cam
  instead) — a harness gap, not a fix gap. One "shifted-load" note logged in `verify-crowd.md`.
- **gems:** miss-state tails stay colored (retail truth unknown — ref never delivered);
  approach-tail dim-alpha vs retail uncalibrated (ref missing); tiny green sparkle specks
  on held tails (cosmetic).
- **C8 IK residual:** after the C8 space fix, the remaining band garment smear is a
  **left-limb IK mispose** class (`RB3_NO_IK` A/B: 20.4 → 4.9 drops/frame) — its own
  follow-up (`scout-c8-rotation-basis.md` §5).

## C8 deep dive — headline

The "rotation-basis divergence" was **misdiagnosed**: it is a rest-bake **space**
error (rest captured in world space, including the member's stage placement, while
verts are model-space-at-origin → `R·sin θ` smear on a |placement|-length lever).
Fix = capture rest in **character space** + never capture mid-clip. Measured:
vert→bind locality 27–60u → 5–12u (== raw authored); band guard drops 25.2 → 20.4/frame;
Wii `BandCharacter.o` byte-identical. Committed on `wt-c8-deep-dive` (`41ff9e97`),
**not landed** — needs a composed-build visual sign-off first. Full detail +
measurements + the engine probe (`6a324be`) in `scout-c8-rotation-basis.md`.

## Suggested wave-4 (next "finish-off" loop)

1. **Land C8** after a composed-build before/after visual burst (byte-identical,
   strictly reduces drops). Then open the **left-limb IK** follow-up.
2. **fret-held white-sphere** — venue-aware bright-tex bind + scale/color clamp;
   re-verify per venue. (`RB3_FRET_GLOW_OFF=1` mitigates meanwhile.)
3. **venue/menu lighting** — clamp/tone-map the unbounded lighting sum in the shared
   standard shader (fixes venue blowout); tune revived menu fog density; re-baseline
   menu contrast.
4. **endgame abort** — debug `endgame_helpers.dta(64):meta_performer` (unblocks the
   score screen for all instruments).
