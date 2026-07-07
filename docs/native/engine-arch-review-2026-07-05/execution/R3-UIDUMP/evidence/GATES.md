# R3-UIDUMP — gate results (Wave 17 Lane U)

All gates run on the shipped build (`native/build-agent-W17-UIDUMP`, engine
worktree `wave17-uidump-lane-U` @ `753ed20`, on top of Lane S `04651e1`).
Reproduce: `python3 scripts/native/uidump_query.py --assert-redband|--assert-rowfix`,
`python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order --scene splash_screen`,
`python3 scripts/native/_uidump_m2_probe.py`.

## G1 — golden compat (RB3DrawRecord byte-shape preserved at prov-OFF) — GREEN
- `drawlog-golden.py --canonical-order` PASS, **792 draws**, prov compiled-in but
  OFF (`RB3_DRAWLOG=1`). Re-run AFTER the M2 scope hooks (Text/UILabel/PanelDir
  DrawShowing) still PASS 792 — the hooks are no-ops at prov-off.
  Evidence: `G1-golden-prov-off.txt`, `M2/g1_regression_after_M2.txt`.
- Fail-red control: `--fail-red-audit` → 4 drift classes RED (count/bind-collapse/
  world-xfm/mesh-identity) + order-permutation GREEN. Evidence: `G1-failred-audit.txt`.

## G2 — zero-cost off + Wii-safe — GREEN (by construction)
- All game-side scope hooks are `#ifdef HX_NATIVE`; the engine sidecar lives in
  rb3-flavor TUs only (Rnd_Wgpu_RB3 / RB3PostProc) — Wii object files untouched.
- Flags default-OFF; at prov-off the drawlog JSON is byte-identical (G1) and the
  scope push/pop are one cached-static branch each (no allocation, no strings).

## G3 — ROWFIX main-vs-alt font-material split (shipped build) — GREEN
`uidump_query.py --assert-rowfix` (RB3_ROWFIX default-ON arm + RB3_ROWFIX=0 arm):
- The instrument PRINTS, in one `/api/uidump` query, a label exhibiting the
  main-vs-alt font-material split — **`gamertag.lbl`** `"<alt>G</alt> Player 1"`:
  main `Pentatonic_Hub(5_00)4x.mat` = **[0.18,0.18,0.18]** (dark), alt
  `instrument_icons.mat` = **[0.75,0.75,0.75]** (light). This is the exact datum
  (main dark / alt NOT darkened) that cost W15→W16 a lane via hand-written
  `RB3_ROWTXT_DBG`/`RB3_ROWFIX_DBG` probes.
- The focused song row's glyph draws are captured with **precise (rectKind=0)**
  rects + per-draw `boundColor` (drawlog `?roi=`): `#211 Pentatonic_Hub … bound
  [0.06,0.05,0.02]`, `#212 Pentatonic_Regular_fadeout … bound [0.06,0.05,0.02]`.
- Fail-red / non-vacuous: only 1 of 3 both-font labels crosses the ≥0.3 split
  threshold (a single-tone label is NOT reported as a split).
- Evidence: `G3/{g3_summary.json, rowfix{ON,OFF}_labels.json,
  rowfix{ON,OFF}_focusrow_glyphs.json, rowfix{ON,OFF}_song_select.png}`.
- **Honest caveat (v1 coverage):** an automatic ON-vs-OFF A/B of the alt-font
  darkening on an arbitrary *song* row is NOT demonstrated, because (a) the row
  must be alt-styled and (b) list-widget row labels are instanced/unnamed → absent
  from the authored walk (see STATUS §coverage). The split itself is reproduced;
  the per-song-row arm A/B is a documented v1 limitation, not a regression.

## G4 — W14 red-band LoadOp truth (shipped build + RB3_MENU_DEPTH_CLEAR knob) — GREEN
`uidump_query.py --assert-redband` (one band ROI, reused verbatim across arms):
- **knob ON** (`RB3_MENU_DEPTH_CLEAR=1`): song_select SETLISTS band is RED
  (redFrac **3.45%** in the band ROI) and the band-ROI **last writer's pass reports
  `passDepthLoad=Clear`**.
- **knob OFF** (shipped): redFrac **0.12%** (band gone) and the SAME last-writer
  quad (`bg_shadow02.mesh`, panel `slot3`) reports **`passDepthLoad=Load`**.
- The two ROI reports differ **only in `passDepthLoad`** (same quad) — exactly the
  W13/W14 diagnosis (three lanes to converge) readable in one query pair. The knob
  is its own fail-red control: OFF is the shipped, band-free state.
- Evidence: `G4/{g4_summary.json, knob{ON,OFF}_roi.json, knob{ON,OFF}_song_select.png}`.
- Note: the literal "SETLISTS selection quad" is not always the named last writer —
  with rectKind=1 sphere-fallback rects on instanced UI quads, submission order puts
  the overshell slot shadow last; the diagnostic datum is the **pass depth LoadOp**,
  which is exact and arm-differentiating (per plan non-goal: ROI is geometric +
  pass-annotated, not per-pixel).

## G5 — no-regression net — GREEN
- G1 golden 792 canonical PASS with all new flags unset (above).
- rb3-native links clean; the M2 hooks touch shared draw-path TUs but the golden
  is byte-identical at prov-off.
