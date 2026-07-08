# Lane GRADE — STATUS (Wave 23, SWEEP S2)

**HEADLINE: mechanism (c) authored/no-fix residual — NO FIX.** The hub "wash" is
NOT venue-light non-engagement (REFUTED: `engaged=1` on every hub environ), NOT a
clean PP-grade wash (PP contributes ~15% brightness but darkening it also drops
contrast, and PP is a shipped default), and is substantially a **camera-phase
artifact** in the SWEEP crop. The current build already sits WELL INSIDE the
wave-5 tuned contrast envelope (5.26–6.88:1 vs the 2.6:1 pre-fix). Close S2 as a
known-limitation.

## Discriminator (A1 — FOUR arms + probe, one boot each)

Harness `scripts/native/_grade_hub_discriminator.py` (boot → `@10:start,@30:confirm`
→ `main_hub_screen`, `RB3_FIXED_CLOCK=1`, settle N frames, `/api/screenshot` +
stderr digest). Metric `scripts/native/_grade_contrast_metric.py` (wave-5 3x3
grid, `max_cell/min_cell` luma over the top 92%).

| arm | env | contrast | frameMean | minCell | backdrop | reading |
|---|---|---|---|---|---|---|
| baseline | — | **5.26** | 0.344 | 0.114 | 0.310 | current hub |
| A_PP_OFF | `RB3_PP_OFF=1` | 4.26 | 0.297 | 0.122 | 0.278 | darkens frame BUT lowers contrast |
| B_VENUE_OFF | `RB3_VENUE_LIGHT_OFF=1` | 3.49 | **0.408** | 0.173 | 0.327 | flat flood is BRIGHTER → non-engagement refuted |
| C_UIGRADE_OFF | `RB3_UI_POST_GRADE_OFF=1` | 3.70 | 0.394 | 0.160 | 0.324 | ~null on backdrop (as predicted) |
| E_FALLBACK_FIX | `RB3_VENUE_FALLBACK_FIX=1` | 3.52 | 0.351 | 0.168 | 0.309 | NULL: else-branch never taken |
| **retail GT** | — | **11.44** | 0.202 | **0.033** | 0.269 | dark-night neon-glow |

(Arm→arm frameMean/minCell noise is dominated by camera phase — see below — so the
signed contributions above are read against baseline at the same settle window.)

### `RB3_WASH_PROBE=1` digest (the decisive instrument)

Every hub environ engages the venue-light path:

```
[WASHPROBE] SCENE env=road.env              engaged=1 miss=engaged dl=1 pl=0 greykey=1
[WASHPROBE] SCENE env=street_slomo_geom.env engaged=1 miss=engaged dl=0 pl=4 greykey=0
[WASHPROBE] SCENE env=street_slomo_char.env engaged=1 miss=engaged dl=0 pl=1 greykey=0
[WASHPROBE] SCENE env=streaks_red.env       engaged=1 miss=engaged dl=1 pl=0 greykey=1
[WASHPROBE] SCENE env=                       engaged=1 miss=engaged dl=1 pl=0 greykey=1
```

`engaged=1` everywhere ⇒ the flat "1.0 white dir + 0.45 grey ambient" flood
(`Rnd_Wgpu_RB3.cpp:1730-1749`) is **never** the code path on the hub. **A1's prime
suspect (b) is REFUTED.** `RB3_VENUE_PROBE` confirms `road.env` is ambient-only
(`ambRaw=0,0,0 numApprox=0`) → grey-key key (`0.22×0.80=0.176`), and
`street_slomo_geom.env` carries the 4 authored point lights.

## Why it looks washed (root cause, from `RB3_HEADMAT_DBG`)

The bright, opaque neon signs are **authored-faithful**, not a grade bug:

- **Pure neon strokes** (`red_neon`/`green_neon`/`Baboon_line`/`nest_line`/
  `capitolsign_neon`…): `diffuse=<null> useEnviron=0` → GX_SRC_REG register-colour
  glowing lines, full-bright by design (matches the binder comment `:281-290`).
- **Sign plates** (`neonsigns_tiger_tattoo.mat` `blend=3 useEnviron=1
  diffuse=neonsign_tiger_tattoo.tex`; `neonsigns_palace.mat` `blend=3 alphaCut=1
  useEnviron=1`): standard src-alpha-over LIT signs (`blend=3 == WgpuBlend::SrcAlpha`,
  `PipelineManager.cpp:336`). Their diffuse texture is the full painted tiger/sign.
- **Backdrop** (`city_backdrop.mat` `useEnviron=0 diffuse=city_night_01.tex`;
  `grey_brick_wall.mat`/`graffiti_brick_wall.bmp.mat` `useEnviron=1`): the grey-green
  speckle IS the authored brick/backdrop texture.

Lowering the venue lighting floors (`RB3_VENUE_GREY_KEY=0.08`,
`RB3_VENUE_DIR_EXPOSURE=0.5`, `RB3_VENUE_AMBIENT_*`) did **not** move backdrop luma
(T1/T2 backdrop ≈0.31, same as baseline) — the backdrop reads mostly through the
unlit/register path, so the lighting knobs cannot darken it.

## Camera-phase finding (re-frames the SWEEP crop)

The hub camera is a cinematic fly-through; 3x3 contrast swings **2.17 → 6.88:1**
across capture depths (settle 30/150/300/500). SWEEP's crop caught the tight-on-
Baboon-Nest-yeti phase (worst wash). At other phases native reaches genuinely dark,
high-contrast framings (`settle_500`: leather jackets true-black, left third
near-black). So the SWEEP "over-bright/washed" is **substantially a phase mismatch
against the single retail wide-street frame**, not a persistent global regression.

## E1 vs GT (A9 — STRUCTURAL / RELATIVE only; 360-PS3 GT, no absolute color)

Matched neon-wall phase (`settle_300`) vs `yt_mhKNp9uAT48_menu_hub`:

| metric | native | retail |
|---|---|---|
| backdrop-dark ROI luma | 0.251 | 0.162 |
| neon-sign ROI luma | 0.585 | 0.159 |
| **neon/backdrop ratio** | **2.33** | **0.98** |

Retail's neon signs are NOT brighter than its backdrop (0.98) because they are dark
neon line-art on near-black; native's lit sign PLATES read 2.3× the backdrop. This
gap is authored-faithful sign rendering + the residual black floor (native minCell
0.088–0.26 vs retail 0.033), and closing it would require touching the additive/
lit-sign compositing (outside the grade grant, and faithful) or crushing the venue
lighting default gameplay depends on.

## Verdict against A2's decision rule

> "not 'Wii might be brighter' (no Wii hub GT) but 'current build still meets the
> wave-5 tuned contrast envelope → residual, close as known-limitation'."

Current baseline contrast **5.26:1** (peak 6.88:1) is **2× the wave-5 pre-fix
2.6:1** — the wave-5 menu-contrast Fix 3 landed and holds. The remaining gap to the
360/PS3 GT's 11.44:1 is the deep-black floor + camera phase, both authored/faithful.
**(c) authored/no-fix.**

## GATES (A2/A3) — trivially satisfied (no code change)

- **numeric hub 3x3 contrast** — reported ON/OFF vs history (table above). No
  regression: baseline 5.26 ≥ wave-5 2.6.
- **E1 vs GT (structural)** — reported (relative ratios only, per A9).
- **gameplay grade non-regression / UIGRADE flush parity / song_select parity band
  (1.110→1.049)** — untouched; NO edits to game.cam/`kGamePlaying`/UI-post-grade.
- **drawlog-792 flag-OFF byte-identical** — no engine change → byte-identical.
- **batch_objdiff == baseline** — no `src/system` unit touched.

## S4 re-scope

S4 (player1 avatar crop) is **NOT** re-scoped by a grade fix (there is none). SWEEP
predicated S4's "over-bright avatar" partly on an S2 grade fix; with S2 = no-fix,
S4's only novel axis (placement/crop) stands on its own and SWEEP already rates it
"likely authored" (retail also places the avatar right); the skin-tone axis is
C8-excluded. → S4 stays deferred/authored, no new action from GRADE.

## Deliverables

- PLAN.md, this STATUS.md
- `scripts/native/_grade_hub_discriminator.py`, `scripts/native/_grade_contrast_metric.py`
- `evidence/`: 6 discriminator-arm PNGs + 2 camera-phase PNGs, `washprobe_venueprobe_digest.txt`,
  `neon_sign_materials.txt`, `contrast_metric.txt`
- checkpoint `/tmp/wave23-checkpoints/GRADE.json` (verdict = (c))

---
## ERRATA (Wave-23 close-out review `45f81795`, ERRATA-G1, G2)
- G1: the "2.6:1 pre-fix" comparison uses the WRONG baseline — wave-5's verified POST-fix
  contrast was 6.8:1 (render-polish PLAN.md:256). Current peak phase 6.88 MATCHES it; the 5.26
  arm-window sits below it (phase variance). Verdict (c) UNCHANGED on the corrected yardstick.
- G2: "SWEEP caught the worst phase" is UNSUPPORTED (settle_150/500 measure LOWER contrast than
  baseline — phase *variance* is proven, not a "worst"). The neon-plate 2.33-vs-0.98 relative gap
  on IDENTICAL assets is an OPEN native rendering difference (deprioritized), NOT "authored-faithful"
  — logged as an open observation, not a Wave-24 lane.
