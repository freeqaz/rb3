# UI render forensics: `/api/uidump` + drawlog provenance

The standing UI-forensics instrument (Wave-17 R3). Answers "what authored UI
object produced this pixel / this draw, with what material/pass/depth state" in
one query pair — subsuming the ≥5 ad-hoc UI probes the campaign kept re-writing.

**Native only** (`rb3-native`, `RB3_HTTP=1`). All engine additions are in the
rb3-flavor GPU TUs; all game hooks are `#ifdef HX_NATIVE`. Rendered output is
byte-identical with the flags unset (drawlog golden 792 unchanged).

## Flags (all default-OFF)

| Flag | Effect |
|---|---|
| `RB3_DRAWLOG` | existing per-draw state ring (unchanged) |
| `RB3_DRAWLOG_PROV` | provenance sidecar: names / screen rect / render-pass idx + depth LoadOp / game-fed panel+owner scope. Implies `RB3_DRAWLOG` on. Set at boot for any join. |
| `RB3_MENU_DEPTH_CLEAR` | test-only knob: forces the Wave-14 menu-boundary re-open depth LoadOp back to `Clear` (re-manifests the SETLISTS red band for G4). |

## Endpoints

### `GET /api/drawlog[?prov=1][&roi=x,y,w,h]`
- No params → **byte-identical to the committed golden** (the standing regression
  gate; do not break this).
- `prov=1` → each draw gains `"prov": { mesh, mat, cam, trans, panel, owner,
  matColor[4], boundColor[4], rect[4], rectKind, pass, passDepthLoad }`.
  - `rectKind`: `0` exact CPU-vert bbox (glyph meshes, precise), `1` sphere
    fallback (static UI quads keep no CPU verts post-upload → near-full-viewport),
    `2` unavailable.
  - `boundColor` = effective post-binder `mu.color` (UI-text floor + ROWFIX
    applied); `matColor` = authored `mat->GetColor()`.
  - `passDepthLoad`: `Clear` / `Load` / `none` at that render pass's open.
- `roi=x,y,w,h` (implies prov) → server-side rect-intersect filter; adds
  `"matched"`, `"lastWriter"` (index into the returned draws, last in submission
  order), `"provAvailable"`.

### `GET /api/uidump[?panel=<substr>][&join=0]`
Authored scene-graph dump joined to the just-drawn frame. Walks
`TheUI.CurrentScreen()` + `mPushedScreens` → active panels → loaded `PanelDir`s
recursively (nested dirs + `mSubDirs`). Standard `{ok,data}` envelope. Per object:
`name/class/showing/drawOrder/world[12]/sphere`, plus per class `mat`
(name/color/blend/zmode) for `RndMesh`, and `label` (text, state, main+alt font
materials with **live** colors) for `UILabel`. `draws` joins the prov sidecar:
named meshes by `prov.meshName`, labels/text by `prov.scopeOwner`. When
`RB3_DRAWLOG_PROV` is unset, `draws` is `null` + `joinDisabled` note (authored
half still works).

## Killer query — `scripts/native/uidump_query.py`

```bash
# ad-hoc ROI query against a running server (RB3_HTTP + RB3_DRAWLOG_PROV):
python3 scripts/native/uidump_query.py --port 8421 --roi 12,322,880,31
# G4 retrodiction (W14 red-band LoadOp truth), self-launches two arms:
python3 scripts/native/uidump_query.py --assert-redband
# G3 retrodiction (ROWFIX main-vs-alt font split), self-launches two arms:
python3 scripts/native/uidump_query.py --assert-rowfix
```

## Worked retrodiction recipes

### The W14 red band (which pass's depth LoadOp reveals the occluded quad?)
Boot with `RB3_DRAWLOG_PROV=1`, nav to song_select. Compare a band ROI with the
`RB3_MENU_DEPTH_CLEAR` knob on vs off: the band-ROI **last writer's**
`passDepthLoad` is `Clear` when the red band shows, `Load` (shipped) when it does
not — the two ROI reports differ only in `passDepthLoad`. Three lanes (W13/W14)
converged on this; it's now one `--assert-redband` run.

### The ROWFIX main-vs-alt font split (why did the focused-row alt glyphs stay light?)
`/api/uidump` prints, per `UILabel`, BOTH the main and alt font materials with
their live colors. A label with main mat dark and alt mat light (e.g.
`gamertag.lbl`: main `[0.18]`, alt `[0.75]`) *is* the split that the W15→W16 lanes
chased with hand-written `RB3_ROWTXT_DBG`. The focused row's glyph draws also
carry precise (rectKind=0) rects + per-draw `boundColor` via `?roi=`.

## Coverage / v1 limitations (honest)
- **`BandRnd::DrawMesh` only** — `RB3Quad` 2D overlays (postproc/compose) do not
  pass through the draw log (documented in every response's `"coverage"`).
- **Instanced list-widget content + persistent overshell HUD** (per-row
  highlight/bg/dots, slot0-3 cards) are resource-loaded milos not parented under a
  screen panel dir, so the *authored* walk does not enumerate them. Their draws are
  still fully captured on the **drawlog-prov** side (mesh/rect/mat/pass/panel), so
  the ROI query works regardless; only the authored↔draw object-level join is
  partial for them. 100% of UI-cam draws still carry a **panel** scope.
- **Sphere-fallback rects** (`rectKind=1`) for static UI quads project to a
  near-full-viewport box → ROI discrimination for named UI quads leans on
  submission order + pass state, not tight rects (by design; ROI is geometric +
  pass-annotated, not per-pixel).
