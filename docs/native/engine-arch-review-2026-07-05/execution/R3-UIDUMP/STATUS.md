# R3-UIDUMP — STATUS (Wave 17 Lane U)

**Verdict: DONE — M1→M4 complete, all gates GREEN.** Standing UI-forensics
instrument shipped: `GET /api/uidump` (authored scene-graph joined to the drawn
frame) + `GET /api/drawlog?prov=1&roi=` (provenance sidecar) + the killer query
`scripts/native/uidump_query.py`. New flags default-OFF; render byte-identical
when off (drawlog golden 792 unchanged). Usage: `docs/native/uidump-forensics.md`.

Resumed as SOLE owner after the duplicate-Lane-U collision (the other instance's
M1 engine work was already committed + rebased onto Lane S — inherited, not
redone). See `DUPLICATE_LANE_COLLISION.md`.

## Commits
- Engine `753ed20` (on engine **main**, rebased onto Lane S `04651e1`): RB3_DRAWLOG_PROV
  sidecar + RB3_MENU_DEPTH_CLEAR knob + classjson rows (default-OFF).
- rb3 `8feded73` — M1 game-side `/api/drawlog ?prov=1 + ?roi=`.
- rb3 `a08bfc61` — M2 scope hooks (Text/UILabel/PanelDir DrawShowing) + `/api/uidump`.
- rb3 `31d388e4` — M3 `uidump_query.py` + G3/G4 evidence.
- rb3 (this) — M4 docs.

**Pin NOT bumped** (coordinator bumps `MILO_ENGINE_PIN` 51640ff→753ed20 once at
close-out). This lane builds against the engine worktree via `-DMILO_ENGINE_PATH`.

## Gates (full detail: `evidence/GATES.md`)
| Gate | Verdict |
|---|---|
| G1 golden compat (792, prov-off byte-identical; re-run after M2 hooks) | GREEN + fail-red RED |
| G2 zero-cost off + Wii-safe (HX_NATIVE hooks, rb3-flavor TUs) | GREEN by construction |
| G3 ROWFIX main-vs-alt font split printed + focused-row glyph boundColors | GREEN + non-vacuous |
| G4 W14 red-band LoadOp truth (Clear vs Load, one query pair) | GREEN + knob fail-red |
| G5 no-regression net (golden 792, links clean) | GREEN |

## Kill-list — how each ad-hoc precedent is now subsumed (plan §2e)

| Precedent probe | Field(s) that replace it |
|---|---|
| `RB3_SS_CENSUS` (recursive panel-dir census: name/class/showing/group) | `/api/uidump` per-object `name`/`class`/`showing`/`drawOrder`; recursive panel+subdir walk built in |
| `RB3_ROWFIX_DBG` (per-widget name/verts/matcolor/tex/blend/zmode) | drawlog `prov.mesh`/`matColor`/`boundColor`/`blend`/`zmode` + uidump `mat.{color,blend,zmode}` |
| `RB3_ROWTXT_DBG` (per-label text + main/alt font-mat colors) | uidump `label.{text,state,fontMat,fontMatColor,altFontMat,altFontMatColor}` (G3 datum) |
| `RB3_UI_FLOOR_DBG` (pre-floor mesh/mat/rgb) | prov `matColor` (pre) + `boundColor` (post-floor) |
| W4.3-C34 flip/hold-label side-scripts | uidump `label` fields + `world`/`sphere` per object |
| W4.3-C2b4 group/force-hide probes | uidump per-object `showing` + recursive walk |
| W0.3c `RB3_DRAWORDER_TRACE` (mesh ptr + hash + order, dc3-only TU) | drawlog `prov.pass`/`passDepthLoad` + submission order (rb3 gets it via prov) |
| W13/W14 red-band lane convergence (3 lanes) | `uidump_query.py --assert-redband` (G4): one query pair prints the pass depth LoadOp truth |

Future UI lanes should query these endpoints instead of writing a bespoke probe.

## Coverage / v1 caveats (honest — carry forward)
- **`BandRnd::DrawMesh` only**; `RB3Quad` 2D overlays are invisible to the drawlog
  (each response tags `"coverage": "BandRnd::DrawMesh only"`).
- **Instanced list-widget rows + the persistent overshell HUD** are resource-loaded
  milos, not parented under a screen panel dir → NOT in the authored walk. Their
  draws ARE fully captured on the drawlog-prov side (mesh/rect/mat/pass/panel);
  only the authored↔draw *object* join is partial for them. 100% of UI-cam draws
  carry a **panel** scope; label owner scope works (verified on gamertag.lbl et al).
  Consequence for G3: the per-song-row alt-arm A/B is not auto-demonstrated (needs
  an alt-styled + named instanced row label); the split itself IS reproduced.
- **Sphere-fallback rects** (`rectKind=1`) for static UI quads are near-full-
  viewport → ROI leans on submission order + pass state, not tight rects (by design).

## Follow-ups (Wave-18 candidates, not blockers)
1. Reach instanced list-widget element milos + the overshell UI root so the
   authored walk enumerates per-row quads/labels (closes the G3 per-song-row A/B
   and the "names the fill quad" authored-side gap).
2. World-translation tiebreak for name-hash join ambiguity (`joinAmbiguous`).
3. Optional: RB3Quad overlay capture for DrawRect/compose coverage.
