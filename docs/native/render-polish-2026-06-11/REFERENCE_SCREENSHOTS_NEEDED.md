# Reference Screenshots Needed — Render Polish Wave 1

Consolidated and de-duplicated from scout docs in this directory.
Priority: **P0** = blocks verification of the fix, **P1** = nice to have.

---

## [char-render] Band member rendering in gameplay

**P1 — nice to have (broken vs. not-broken is visually unambiguous without it)**

| # | What to capture | Why needed |
|---|---|---|
| CR-1 | Retail gameplay band-member closeup: venue camera cut to guitarist or vocalist, showing head and hands mid-song | Current `images/retail-screenshots/` set is HUD-centric (drum/guitar highway only). Need a ground-truth reference for the "floating teeth/eyes" bug — to confirm what a correct head+body render looks like at close range. |
| CR-2 | Retail venue-entrance walk-in scene: band walking from offstage to stage positions at song start, showing full-body leg + torso motion | Scout found "lifted/wrong legs" consistent with the bone-clamp freezing leg bones to bind pose during the walk. Need retail ground truth to judge the correct leg arc against. |

---

## [crowd] Crowd rendering in gameplay

**P1 — nice to have (crowd shard-guard drop count is the primary metric)**

| # | What to capture | Why needed |
|---|---|---|
| CW-1 | Retail small_club gameplay wide shot or crowd-cam shot (`coop_dir_crowd.shot`) showing 3D floor crowd density and idle motion | Baseline for Fix A (crowd skeleton rebind). Needed to verify that post-fix crowd bodies are spread across the floor rather than bunched as floating heads/statues. A YouTube small-club run is sufficient. |
| CW-2 | Retail arena or festival wide shot showing the 2D bowl crowd texture rows | Baseline for the latent Fix B (2D imposter pipeline). The imposter path only fires in non-force3D venues (arena/big_club/festival). Not needed until Fix C (venue bridge) lands. |

---

## [gem-polish] Highway gem and sustain tail rendering

**CR-1 is P0; the rest are P1**

| # | What to capture | Why needed |
|---|---|---|
| GP-1 | **[P0]** Retail (Wii preferred) frame of an approaching, un-hit sustain on the 5-lane guitar highway — dim tail visible extending from the gem upward before the note is hit | Required to calibrate `kTailMinAlpha` and the approach-tail width/alpha look after the mesh-cache fix. Current refs only show tails incidentally or at a distance. |
| GP-2 | Retail held sustain with active whammy close-up — pulsing / slightly widening tail + bloom amount | Target for the post-fix tail look and to set the correct bloom contribution on held tails. |
| GP-3 | Retail missed sustain showing the gray `tail_miss` tube | No native ground truth exists for the miss state; needed to verify that state independently. |
| GP-4 | Retail overdrive-deployed highway (track turns blue) | To disambiguate the bright-blue track state that appears in some captures from a lighting or tint bug. |

---

## [fret-held] Held fret glow on the now-bar

| # | What to capture | Why needed |
|---|---|---|
| FH-1 | **[P0]** Retail (Wii preferred) gameplay frame with one fret button held/lit solid on the now-bar, cropped to the smasher plate — showing exact per-slot glow color, intensity, and shape | The native now-bar is pixel-identical whether or not a fret is held (glow material is black + no texture). Need ground truth to confirm the expected additive glow color per slot (green / red / yellow / blue / orange). |
| FH-2 | **[P1]** Retail shot of a sustain being held (smasher glowing during a sustain tail) | Confirms the lit-while-held look persists through a sustain; same material as FH-1 but in the context of a held note. |

---

## [menu-lighting] Main hub (Rock City) backdrop lighting

| # | What to capture | Why needed |
|---|---|---|
| ML-1 | **[P0]** Wii hub loop captures: walking-band close-up, storefront/ARCADE-neon shot, tent/tiger-wall shot | Current hub refs are 360/PS3 only. The Wii renderer (rndwii) has no emissive TEV reference in the decomp, so a Wii capture settles whether Wii applies emissive maps at all and at what strength — critical for choosing the correct emissive-multiplier target in the port. YouTube longplay `qSRJ8HHPXzM` likely contains the loop. |
| ML-2 | **[P0]** 360/PS3 close-up of the green ARCADE storefront neon + the green fog moment (matches `/tmp/rp-menu-lighting/von_f01800.png`) | Confirms intended tube thinness of the `neon_arcade.mesh` (currently a full-frame slab) and whether the residual green fog billboards are authored or a decode artifact. |
| ML-3 | **[P1]** Hub `theater.env` / `cityscape.env` camera shots: theatre marquee and skyline | For emissive-level tuning of building windows and sign illumination after Fix 1+2 land. |

---

## Capture notes — using `../xenia` for 360 ground truth

`/home/free/code/milohax/xenia` is available and is the recommended tool for any 360-build captures (FH-1/FH-2, GP-1–GP-4, ML-2, CW-1/CW-2, CR-1/CR-2 if a 360 source is preferred over Wii).

Per-shot notes from scouts:

- **FH-1**: hold a fret button on a known frame. The scout's probe script
  `/tmp/rp-fret-held/drive_only.py <port> <bit>` (green=1, red=5, yellow=4, blue=6, orange=7)
  shows which keypress to replicate in Xenia. Crop the now-bar at approximately
  `(360, 540, 920, 700)` of a 1280×720 frame.
- **GP-1**: the Antibodies (Poni Hoax) sustain chord at ~19.68 s is a clean repro
  target (Y/B/O 3-lane, 1.44 s long). Jump to ~19.2 s in Xenia for a clean
  approach-tail frame before the note is hit.
- **ML-1**: Wii is specifically needed here (not just preferred) — the 360 hub
  lighting is already covered by the existing refs in `images/retail-screenshots/`.
  Wii longplay `qSRJ8HHPXzM` on YouTube is an alternative if Xenia Wii emulation
  is unavailable.
- **CW-2**: only needed after Fix C (venue bridge) lands and Fix B (2D imposter)
  is in scope; defer until then.
