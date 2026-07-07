# PLAN R3 — UI render forensics: `/api/uidump` + drawlog provenance

**Item:** ROADMAP.md R3 / OPTIONS.md §2#3 (candidates c+d, merged).
**Planner:** Fable, 2026-07-07 (read-only research pass; every file:line below was
opened/grepped in this pass, engine @ pin `51640ff` working tree).
**Executor:** one Opus lane (or E/G split, see §7). Wave-17 slot per ROADMAP sequencing.

---

## 1. OBJECTIVE + non-goals

Build the standing UI-forensics instrument the campaign re-improvised ≥5 times:

1. **Scene-graph dump** — `GET /api/uidump`: for the active screen's panels, every
   authored UI object's name/class/show-state/draw-order/xfm/material-color/font-material,
   **joined** against the just-drawn frame's draw log → per object: drawn screen rect(s)
   or `"draws": 0`.
2. **Drawlog provenance** — extend the existing `/api/drawlog` with an opt-in
   provenance sidecar: per draw, the mesh/material/camera **names**, the owning
   Trans parent, the game-side scope (panel / text-object) that issued it, its
   projected **screen rect**, and its **render-pass index + that pass's depth LoadOp**.
3. **Killer ROI query** — given a pixel region, list the draws that intersect it in
   submission order with z/blend/pass state, name the last writer, and join each to its
   authored object. (Server does a thin `?roi=` rect filter; the joined pixel→authored-object
   report lives in a script, `scripts/native/uidump_query.py`.)

**Acceptance is retrodictive:** the instrument must reproduce, from the shipped build,
two diagnoses that each previously cost a lane (§5 G3/G4): the W14 red-band LoadOp truth
and the ROWFIX main-vs-alt font-material split.

### Non-goals

- **Anim attribution** ("which anim drove this draw"). Hooking RndAnimatable is a large
  surface for little diagnostic gain; the dump instead reports authored (rest/dir-file)
  vs live xfm so "something animated this" is readable. Revisit only if a consumer needs it.
- **Per-pixel exact last-writer.** The ROI query is geometric (bbox-intersect + submission
  order + full z/blend/pass annotations). Both validation cases are solvable at that
  precision; a depth-aware software rasterizer is out of scope.
- **Web build / Wii build / DC3 backend.** Native `rb3-native` only. All game-side edits
  `#ifdef HX_NATIVE`; all engine edits inside the rb3-flavor TUs (Wii object files
  untouched by construction).
- **No new gameplay behavior.** Pure diagnostics; every addition is flag-gated and the
  existing drawlog-golden byte shape is preserved at prov-off (§5 G1).

---

## 2. CURRENT STATE (verified against source this pass)

### 2a. The single draw choke point (load-bearing premise, VERIFIED)

- RB3-native compiles the **BandRnd backend only**: `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`
  (engine `CMakeLists.txt:315-330`) = `Rnd_Wgpu_RB3.cpp`, `RB3MeshCache.cpp`,
  `RB3MaterialBinder.cpp`, `RB3HaloPass.cpp`, `RB3PostProc.cpp`, `RB3Quad.cpp`,
  `RB3TexSharpen.cpp`. `Mesh_Wgpu.cpp` / `TransparentQueue.cpp` are in the **dc3-only**
  list (`CMakeLists.txt:305-313`, comment: "RB3-Wii cannot compile any of these").
- `RndMesh::DrawShowing()` strong def for rb3 = `Rnd_Wgpu_RB3.cpp:5468` →
  `gBandRnd.DrawMesh(this)`. Text: `RndText::DrawShowing()` (rb3
  `src/system/rndobj/Text.cpp:1722-1742`) draws each `mMeshMap` glyph mesh via
  `meshInfo.mesh->DrawShowing()` → same choke point.
- **⇒ 100% of RB3-native draws (UI quads, glyph meshes, venue, chars) pass through
  `BandRnd::DrawMesh`**, where the W0.3 drawlog ring already records
  (`Rnd_Wgpu_RB3.cpp:5283-5289`). One capture point covers everything.
- **Premise correction inherited docs carry:** W4.4-TEXTCOLOR STATUS §1 cites
  "`Mesh_Wgpu.cpp DrawMeshImmediate` snapshots mat color (line 48/230)" as the native RB3
  label path. That file is **not compiled** in the rb3 flavor; the *behavior* claim
  (per-draw material snapshot) is true of the RB3 path too (RB3MaterialBinder builds
  `MaterialUniforms` per draw), but the file citation is wrong-era/wrong-backend. This
  plan does not build on that citation.

### 2b. Existing drawlog (what we extend)

- Record shape: `RB3DrawRecord` (engine `src/platform/RB3DrawLogDebug.h:32-48`):
  pipelineHash, blend/zMode/layout/flags, targetFormat, index/tri/vert counts,
  `meshNameHash` (FNV-1a, **hash only — no name string**), `world[16]`, 4 opaque
  bind-group identity tokens. **No material name, no camera, no rect, no pass info.**
- Capture: `BandRnd::RecordDrawLog` (`Rnd_Wgpu_RB3.cpp:5324-5352`), called from
  `DrawMesh` behind `DrawLogOn()` — cached-static env gate `RB3_DRAWLOG`
  (`:5410-5419`) OR the gtest override `RB3DebugSetDrawLogEnabled`.
- Ring cleared each BeginFrame; read via `RB3DebugGetDrawLog()` (`:5413`).
- HTTP: `GET /api/drawlog` (`native/src/rb3_http_server.cpp:356-374`,
  `kCmdDrawLog` processed by `ProcessCommands` right after `RunOneFrame`) serializes in
  `rb3_http_handlers.cpp:200-` (`HandleDrawLog`) to
  `{ frame, count, draws:[ {i, name:"0x<hash>", pipe, blend, zmode, layout, fmt,
  hasDepth, alphaCut, alphaWrite, skinned, idx, tris, verts, scene, mat, obj, bone,
  world:[16]} ] }` — **unwrapped** (no ok/data envelope) because
  `scripts/native/drawlog-golden.py` diffs the body against a committed golden
  (792 draws, `--fixed-clock --canonical-order --scene splash_screen` is the standing
  regression gate cited by every recent STATUS doc).
- Glyph meshes are **unnamed** (created internally by RndText; cf. `Mesh_Wgpu.cpp:127`
  comment and `Text.cpp` mMeshMap) → `meshNameHash == 0` for ALL text draws. A
  name-join alone can therefore never attribute text; this is why the design adds a
  game-side scope stack (§3.3).

### 2c. Plumbing points available at capture time

- `DrawMesh` scope has: `mesh` (`RndMesh*` → `Name()`, `Mat()`, `TransParent()`
  (`rndobj/Trans.h:75`), `GetSphere()` (`rndobj/Draw.h:71`), CPU `Verts()`
  (`rndobj/Mesh.h:112-128`)), `mat` (`RndMat*` → `Name()`, `GetColor()`), `RndCam::sCurrent`
  (name already read for the halo gate at `Rnd_Wgpu_RB3.cpp:5261-5263`), `obj.world`.
- `viewProj` is computed CPU-side in `WriteSceneUniforms` (`Rnd_Wgpu_RB3.cpp:1380+`,
  local `SceneUniforms.viewProj[16]`) but **not retained** — `RB3SceneBinding`
  (`Rnd_Wgpu_RB3.h:56-59`) carries only the bind group + offset. A CPU copy member is
  needed for rect projection (§3.2).
- Material effective color: `RB3MaterialBinder.cpp` builds `mu.color` per draw and is
  where the UI text floor mutates it (`:160-215`; `RB3_UI_FLOOR_DBG` at `:203` dumps
  `mesh/mat/pre-floor rgb` — the exact datum uidump subsumes). Binder runs inside
  DrawMesh **before** `RecordDrawLog`, so a prov-gated scratch handoff is ordering-safe.
- Render-pass opens: `mPass = mEncoder.BeginRenderPass(&rp)` at `Rnd_Wgpu_RB3.cpp:1935,
  2246, 2298, 2348` and `RB3PostProc.cpp:112` (+ blit/RTT passes at `RB3PostProc.cpp:509`,
  `RB3HaloPass.cpp:255`). The W14 U-CLEAN mechanism lives at `RB3PostProc.cpp:75-93`:
  menu-boundary re-open uses `depthLoad = menuBoundary ? LoadOp::Load : LoadOp::Clear`
  (`:93`) under default-ON `RB3_UI_POST_GRADE` (`:249-258`).

### 2d. Game-side access for the authored dump

- `TheUI` (`src/system/ui/UI.h:200`), `UIManager::CurrentScreen()` (`UI.h:163`),
  `mPushedScreens` (`UI.h:177`), `UIScreen::GetPanelRefs()` (`UIScreen.h:67`),
  `UIPanel::LoadedDir()` → `PanelDir : RndDir` (`UIPanel.h:65`, `PanelDir.h:17`).
- Recursive dir walk precedent: the C2a census (`RB3_SS_CENSUS`,
  `src/band3/meta_band/SongSelectPanel.cpp:288-360` — `C2CensusDir` walks
  `ObjDirItr<Hmx::Object>` + `mSubDirs`, logs GRP membership/showing). uidump
  generalizes this from a panel-local stderr probe to a JSON endpoint.
- Draw order: `RndDir::mDraws` (`rndobj/Dir.h:83`) — index in that vector is the
  authored draw order uidump reports.
- HTTP command plumbing: add a `kCmd` to `rb3_http_server.h:40-53` + route; handlers run
  on the main thread between frames (same pattern as kCmdDrawLog/kCmdDtaEval).

### 2e. The ≥5 ad-hoc precedents this subsumes (kill-list)

| Precedent | What it logged | Where |
|---|---|---|
| `RB3_SS_CENSUS` (W4.3-C2a) | recursive panel-dir census: name/class/showing/group membership | `SongSelectPanel.cpp:71,288+` (stays; uidump makes it redundant for future lanes) |
| `RB3_ROWFIX_DBG` (W4.4-ROWFIX W15) | per-`UIListWidget::DrawMesh` name/verts/GeomOwner/matcolor/tex/blend/zmode | STATUS §0 (probe landed in `UIListWidget.cpp`) |
| `RB3_ROWTXT_DBG` (W4.4-TEXTCOLOR W16) | per-label text + alt-state + main/alt font-mat colors in `UILabel::DrawShowing` — **removed before commit** (STATUS §0), i.e. the next label bug re-writes it | W4.4-TEXTCOLOR/STATUS.md §0-1 |
| `RB3_UI_FLOOR_DBG` | pre-floor (mesh, mat, rgb) for likely-UI-text draws | engine `RB3MaterialBinder.cpp:203` (stays) |
| W4.3-C34 side-scripts | flip-finder / hold-label probing around label scale/wrap | `scripts/native/_c34_flip_finder.py`, `_c34_holdlabel_probe.py` |
| W4.3-C2b4 probes | `RB3_C2B_ASM_HIDE` force-hide + `C2LogGroup` group dumps | `SongSelectPanel.cpp:260-286` |
| W0.3c `RB3_DRAWORDER_TRACE` | per-flush mesh ptr + FNV hash + order (dc3 TU) | engine `TransparentQueue.cpp:66-100` (dc3-only; rb3 gets the same via prov) |

uidump's contract: **each row of this table becomes a field in one of the two JSON
endpoints**, so no future UI lane needs to re-write a bespoke probe for these data.

---

## 3. DESIGN

### 3.1 Flags (all default-OFF; classjson appends required)

| Flag | Effect | Cost when unset |
|---|---|---|
| `RB3_DRAWLOG` | existing ring capture (unchanged) | 1 cached-static branch/draw (existing) |
| `RB3_DRAWLOG_PROV` | provenance sidecar capture (implies ring on) | 1 cached-static branch/draw; scope-push hooks are the same cached branch, no allocation |
| `RB3_MENU_DEPTH_CLEAR` | test-only: force `depthLoad = LoadOp::Clear` at the menu boundary (`RB3PostProc.cpp:93`) — re-manifests the W14 red band on demand for G4 | getenv once |

Append `RB3_DRAWLOG_PROV` + `RB3_MENU_DEPTH_CLEAR` to
`NativeCompatFlags.classification.json` (existing `RB3_DRAWLOG` entries at `:504,:511`
are the template).

### 3.2 Engine half — provenance sidecar (milo-native-engine, rb3 flavor TUs only)

`RB3DrawRecord` stays byte-identical (it is a committed data contract — header says so).
Add a **parallel sidecar vector** `std::vector<RB3DrawProv>` filled only when
`RB3_DRAWLOG_PROV`, exposed via `RB3DebugGetDrawProv()` in `RB3DrawLogDebug.h`:

```cpp
struct RB3DrawProv {                  // one per RB3DrawRecord, same index
    std::string meshName;             // mesh->Name() ("" for glyph meshes)
    std::string matName;              // mat->Name()
    std::string camName;              // RndCam::sCurrent->Name()
    std::string transParent;          // mesh->TransParent() ? ->Name() : ""
    std::string scopePanel;           // top PANEL entry of the scope stack ("" if none)
    std::string scopeOwner;           // top OWNER entry (text/label/widget) ("" if none)
    float       matColor[4];          // authored mat->GetColor() at draw time
    float       boundColor[4];        // effective post-binder mu.color (floor applied)
    float       rect[4];              // projected screen bbox x,y,w,h (pixels); w<0 => offscreen/degenerate
    uint8_t     rectKind;             // 0 exact-verts, 1 sphere-fallback, 2 unavailable
    uint16_t    passIdx;              // frame-monotonic render-pass sequence number
    uint8_t     passDepthLoadOp;      // 0=Clear,1=Load,2=none at THIS pass's open
};
```

Capture details:

- **Names**: read in `RecordDrawLog`'s caller (`DrawMesh`, where `mesh`/`mat` are live) —
  extend the `RecordDrawLog` signature (internal, private) or pass a small struct.
- **Rect**: when prov on, `BandRnd` keeps a CPU copy of the last-written scene matrices —
  `float mActiveViewProjCpu[16]` + cam name, assigned inside `WriteSceneUniforms`
  (unconditional 64-byte copy is fine; gate it anyway for zero-delta discipline). Rect =
  bbox of the mesh's CPU verts (`mesh->Verts()`, exact for UI quads/glyphs) transformed
  by `world → viewProj → NDC → pixels` (viewport = current target size), corner-clamped.
  If `Verts().size() > 4096` or empty, fall back to `GetSphere()` center±radius
  (`rectKind=1`); skinned draws also use sphere (CPU verts are pre-skin). Any corner with
  `clip.w <= 0` ⇒ conservative full-viewport rect with `rectKind=1` (UI cams are behind-
  camera-safe; venue geometry precision doesn't matter for a UI instrument).
- **boundColor**: `RB3MaterialBinder` writes the final `mu.color` into a prov scratch
  (`RB3ProvNoteBoundColor(const float*)`, no-op when prov off) that `RecordDrawLog`
  consumes — this captures the UI-text floor's actual output (subsumes
  `RB3_UI_FLOOR_DBG`'s post side; the pre side is `matColor`).
- **passIdx / passDepthLoadOp**: `mProvPassSeq` incremented at every
  `mEncoder.BeginRenderPass` site listed in §2c (helper `ProvNotePassOpen(depthLoadOp)`
  called with the descriptor's depth LoadOp, or `2` when the pass has no depth
  attachment). `RB3PostProc.cpp:93` additionally honors `RB3_MENU_DEPTH_CLEAR`.
- **Scope stack** (engine-owned, game-fed): in `RB3DrawLogDebug.h`:

```cpp
// Thread-local provenance scope. Push/pop are no-ops (cached branch) unless
// RB3_DRAWLOG_PROV. kind: 0=panel, 1=owner (text/label/widget).
void RB3DrawScopePush(int kind, const char* name);
void RB3DrawScopePop(int kind);
struct RB3DrawScopeGuard { /* RAII */ };
```

  `RecordDrawLog` snapshots the current top-of-stack panel + owner into the sidecar.

### 3.3 Game half — scope hooks + `/api/uidump` (rb3 repo)

**Scope hooks** (`#ifdef HX_NATIVE`, each one line + guard object):

- `PanelDir::DrawShowing` (or `UIPanel::Draw` if that's the tighter seam — executor
  picks whichever wraps the whole panel's draw): push `(panel, mDir->Name())`.
- `RndText::DrawShowing` (`Text.cpp:1722`): push `(owner, Name())` around the mMeshMap
  loop — **this is the one hook that attributes every unnamed glyph draw**.
- `UILabel::DrawShowing`: push `(owner, Name())` so a label's glyphs attribute to the
  label, not just the inner RndText (label pushes outer, RndText pushes inner; sidecar
  keeps the innermost owner + we also emit the outer if trivially available — v1 keeps
  ONE owner slot, innermost wins, label name recoverable from the RndText's dir/name).

**`GET /api/uidump[?panel=<substr>][&join=0]`** — new `kCmdUIDump`, handler in new TU
`native/src/rb3_uidump.cpp` (walk runs on the main thread between frames, same safety
model as kCmdDtaEval). Walks `TheUI.CurrentScreen()` + `mPushedScreens` →
`GetPanelRefs()` → each loaded `PanelDir`, recursively (census pattern, incl. `mSubDirs`).
Response (wrapped in the standard ok/data envelope — this endpoint has no golden):

```json
{ "frame": 1234,
  "screens": [ { "name": "song_select_screen", "current": true,
    "panels": [ { "name": "song_select_panel", "dir": "song_select.milo",
      "objects": [
        { "name": "ml_highlight_glasstopp.mesh", "class": "RndMesh",
          "showing": true, "drawOrder": 17,
          "world": [16], "localXfm": [12],
          "sphere": {"c":[x,y,z], "r": 12.0},
          "mat": { "name": "ml_highlight_glasstop.mat",
                   "color": [1,1,1,0.08], "blend": 4, "zmode": 0 },
          "groups": ["highlight_bar.grp"],
          "draws": { "count": 1, "rects": [[512,388,896,42]],
                     "lastDrawIdx": 611, "passIdx": 3 } },
        { "name": "song_title.lbl", "class": "UILabel",
          "showing": true, "drawOrder": 22,
          "label": { "text": "25 or 6 to 4", "state": "kFocused",
                     "fontMat": "font_main.mat", "altFontMat": "font_ital.mat",
                     "fontMatColor": [...], "altFontMatColor": [...],
                     "colorOverride": null },
          "draws": { "count": 2, "rects": [[...],[...]], "via": "scopeOwner" } },
        { "name": "highlight_yellow.mesh", "class": "RndMesh",
          "showing": true, "drawOrder": 9, "draws": { "count": 0 } }
      ] } ] } ] }
```

- Per-class extraction: `RndDrawable` (showing/order/sphere), `RndTransformable`
  (world+local), `RndMesh` (mat name/color/blend/zmode via `Mat()`), `UILabel`/`RndText`
  (text, main+alt font materials + their live colors, `mColorOverride`, focus state),
  `RndGroup` (member list → inverted into each object's `groups`). Unknown classes emit
  name/class/showing only — additive schema.
- **Join** (`join=1` default): match drawlog+prov of the last completed frame — named
  meshes by FNV name-hash (`meshNameHash`), labels/text by `scopeOwner`. Requires
  `RB3_DRAWLOG_PROV=1` at boot; when prov is off, `draws` is `null` with
  `"joinDisabled": "RB3_DRAWLOG_PROV unset"` (dump half still works — it is
  authored-side only).
- Name-collision honesty: same mesh name in two dirs → both join the same hash; emit
  `"joinAmbiguous": true` when a hash matched >1 authored object (world-translation
  tiebreak is a v2 nicety, not v1).

**`GET /api/drawlog?prov=1[&roi=x,y,w,h]`** — extend the existing handler:
- `prov=0` (or absent): **byte-identical to today** (G1 gate).
- `prov=1`: each draw gains `"prov": { mesh, mat, cam, trans, panel, owner,
  matColor, boundColor, rect, rectKind, pass, passDepthLoad }`.
- `roi=`: server-side rect-intersect filter (draws whose `rect` overlaps the ROI), same
  shape, plus `"lastWriter": <index into returned draws>` (= last intersecting draw in
  submission order). Full z/blend/pass state is on every returned draw, so occlusion
  reasoning is in the reader's hands — by design (non-goal: per-pixel truth).

**`scripts/native/uidump_query.py`** — the joined killer-query CLI:
`--roi x,y,w,h [--panel P] [--png out.png]` → fetches both endpoints, prints per
intersecting draw: `#611 pass3(depth=Load) blend=srcAlpha z=disable rect=[...]
mesh=ml_highlight_glasstopp.mesh mat=... panel=song_select owner=- ← authored: showing=1
order=17 matColor=(1,1,1,0.08)`, marks the last writer, and (with `--png`) draws rect
overlays on a `/api/screenshot` capture. Also grows the two gate assertions (§5) as
`--assert-*` modes so G3/G4 are one-command replays.

### 3.4 Overhead accounting (per item spec)

- All flags unset: `DrawLogOn()` cached branch per draw (exists today); scope hooks =
  one cached-static branch each (same pattern, `Rnd_Wgpu_RB3.cpp:5410` precedent); pass-
  open helper = one branch per pass (≤ ~8/frame); `WriteSceneUniforms` copy gated. No
  allocation, no string work, rendered output byte-identical. Wii build: game hooks are
  `#ifdef HX_NATIVE` → object files byte-identical by construction.
- `RB3_DRAWLOG=1` (golden harness mode): unchanged behavior AND unchanged JSON (G1).
- `RB3_DRAWLOG_PROV=1`: ~200-400 bytes + one vert-scan per draw, diagnostic sessions only.

---

## 4. MILESTONES

**M1 — engine sidecar spike (cheapest decisive risk-retirement).**
Scope: sidecar struct + names + rect + passIdx/LoadOp capture; `/api/drawlog?prov=1`
serialization; NO game-side hooks yet (scopePanel/Owner empty).
Exit / go-no-go:
- `drawlog-golden.py --fixed-clock --canonical-order --scene splash_screen` **PASS
  (792, canonical)** with `RB3_DRAWLOG=1` and prov compiled in but off → the contract
  survives. FAIL ⇒ stop, redesign serialization isolation.
- On song_select (headless nav per `scripts/native/song-select-capture.py` pattern),
  `?prov=1` shows `ml_highlight_glasstopp.mesh` with a sane rect (overlaps the focused
  row band measured in W4.4 evidence PNGs) and `highlight_yellow.mesh` **absent** —
  rect fidelity + the "0 draws for free" claim retired on real data. Rect wildly wrong
  (e.g. degenerate for >20% of named UI draws) ⇒ go/no-go on falling back to
  sphere-only rects (`rectKind=1`) before proceeding.

**M2 — scope hooks + `/api/uidump` walk + join.**
Scope: §3.3 complete; glyph draws attribute to their RndText/UILabel; uidump joins.
Exit: on song_select, the focused row's dump names the fill quad, the frame mesh,
both font materials with live colors, and `highlight_yellow.mesh` with `draws.count=0`
(the OPTIONS "first milestone" sentence, verbatim). Unattributed draw fraction on
song_select reported; >30% unattributed UI-cam draws ⇒ add the missing hook(s) before
calling M2 done.

**M3 — ROI query + the two retrodictions (G3+G4) + `uidump_query.py`.**
Exit: both gates below GREEN with committed evidence under
`execution/R3-UIDUMP/evidence/` (per OPTIONS §4 lint: probes write under execution/,
never bare /tmp).

**M4 — subsume + document.**
Kill-list table (§2e) annotated in the STATUS doc: which field replaces each precedent;
`docs/native/` gains a short usage page (endpoint shapes + the two worked retrodictions
as recipes); classjson entries; engine commit → `MILO_ENGINE_PIN` bump in the same rb3
change per CLAUDE.md Git rules.

---

## 5. GATES (each with its fail-red demonstration)

| # | Gate | GREEN condition | Fail-red demonstration (run once, show RED) |
|---|---|---|---|
| G1 | Golden compat | `drawlog-golden.py --fixed-clock --canonical-order --scene splash_screen` PASS (792) with prov code compiled, `RB3_DRAWLOG=1`, prov OFF | temporarily emit one prov field at prov-OFF → golden comparator must FAIL (proves the gate sees serialization drift) |
| G2 | Zero-cost off + Wii-safe | flags unset: `/api/drawlog` returns `{"draws":[]}`-era behavior unchanged; `tools/ninja-locked` Wii build: zero object-file diffs (game edits are `#ifdef HX_NATIVE`); engine edits confined to rb3-flavor TUs | move one scope hook outside its `#ifdef HX_NATIVE` guard → Wii build diff appears (then revert) |
| G3 | **Retrodiction 1 — ROWFIX font split** (shipped build, `RB3_ROWFIX` default-ON since W16) | `uidump_query.py --assert-rowfix`: focused song row dump shows (a) `ml_highlight_glasstopp` drawn bright/opaque with rect over the row, (b) label with **main AND alt font materials both dark** (W16 fix), and with `RB3_ROWFIX=0` opt-out arm: fill quad color alpha≈0.08 additive + **alt font material NOT darkened** — i.e. the instrument prints the main-vs-alt split that cost W15→W16 a lane | run the same assert against an unfocused row → RED (no fill quad, no dark override), proving the assert isn't vacuously green |
| G4 | **Retrodiction 2 — W14 red-band LoadOp truth** (shipped build + test knob) | with `RB3_MENU_DEPTH_CLEAR=1`: song_select shows the red band (pixel check, threshold from W4.4 STATUS: red `[76,27,27]`-class dominance on the SETLISTS row); `uidump_query.py --roi <band rect>` names the SETLISTS selection quad as last writer **and prints its pass's `passDepthLoad=Clear`**; knob OFF: same ROI, same quad listed, `passDepthLoad=Load` — the diagnosis W13/W14 took three lanes to converge on is readable in one query pair | knob OFF pixel check shows 0% red (the shipped state) while knob ON shows the band — the knob itself is the fail-red control; additionally assert the ROI report DIFFERS between arms only in passDepthLoad |
| G5 | No regression net holes | existing suites: `rb3-tests` green; hub contrast / SETLISTS red-band 0% both-arms checks from W16 STATUS re-run unchanged with all new flags unset | n/a (standing gates; RED already defined by their own harnesses) |

---

## 6. RISKS + mitigations

1. **Golden fragility (highest).** The 792-draw golden is byte-compared; any accidental
   reorder/reshape at prov-OFF breaks the campaign's standing gate. Mitigation: sidecar
   is a *parallel* vector (RB3DrawRecord untouched); serializer takes the prov branch
   strictly after the legacy field emit; G1 fail-red proves the comparator sees drift.
   M1 runs this gate before anything else is built.
2. **Rect fidelity.** CPU-vert bbox is pre-skin and pre-shader; some UI text is
   positioned by shader/vertex tricks; `clip.w<=0` corners; RTT passes (rects are in
   render-target space, not screen). Mitigation: `rectKind` honesty byte; only screen-
   target passes (`targetFormat == framebuffer`) participate in ROI queries (filter by
   passIdx of framebuffer passes); M1 go/no-go measures degenerate-rect fraction on real
   song_select data before M2/M3 build on rects.
3. **Glyph attribution gaps.** Some text may draw outside `RndText::DrawShowing` (e.g.
   deferred/queued paths). Verified: rb3 flavor has NO TransparentQueue, so no deferred
   path exists today — but W2-era code moves could add one. Mitigation: M2 exit measures
   the unattributed fraction; the scope API is engine-side so a future queue flush can
   re-push scopes from captured tags.
4. **Panel walk vs concurrently-loading dirs.** `LoadedDir()` may be null / mid-load;
   census precedent already handles null checks. Handler runs between frames on the main
   thread (same safety envelope as kCmdDtaEval), so no torn state — but a panel unload
   between the drawn frame and the uidump command makes joins dangle by one frame.
   Mitigation: joins are by name/hash (value data), never by pointer into the drawn
   frame; document the one-frame skew.
5. **`RB3_MENU_DEPTH_CLEAR` knob touches shipped postproc.** One-line, getenv-gated,
   test-only; G5 re-runs the SETLISTS 0%-red both-arms check with the knob unset.
   If reviewers reject a knob in RB3PostProc.cpp, fallback: run G4's Clear arm via
   `RB3_UI_POST_GRADE_OFF` — **NOT equivalent** (it removes the whole menu-boundary
   flush, changing more than depth LoadOp; verified `:249-258` semantics), so the knob
   is the honest instrument; plan states this so the executor doesn't silently swap it.
6. **Premise risk owned:** this plan asserts single-choke-point draw coverage from
   `CMakeLists.txt` flavor lists + the strong `RndMesh::DrawShowing` def. If some rb3
   native TU draws via a bespoke path (`rb3_render_mesh.cpp:460` and `rb3_viewer.cpp:288`
   call `gBandRnd.DrawMesh` directly — still the same choke point; `RB3Quad.cpp` 2D
   quads do NOT go through DrawMesh), then **RB3Quad draws (DrawRect overlays, outfit
   compose) are invisible to the drawlog**. Checked: RB3Quad is used for postproc/
   compose, not authored UI objects — acceptable v1 blind spot; documented in the
   STATUS + endpoint (`"coverage": "BandRnd::DrawMesh only"`).

## 7. COST + what it unblocks

- **Cost: ~1 wave**, matching the ROADMAP row. Suggested split: single Opus lane running
  M1→M4 sequentially (M1 is a half-day spike; M2 the bulk; M3 mostly script + evidence).
  If parallelized: E-lane (engine, M1) ∥ G-lane prep (uidump walk against prov-less
  drawlog), converging at M2's join. Engine work commits to `milo-native-engine` first,
  then the rb3 commit bumps `MILO_ENGINE_PIN` (CLAUDE.md rule).
- **Unblocks (ROADMAP Wave-18 cash-in):** sidebar backing quad (authoring polish — dump
  shows its authored xfm/material vs drawn rect directly), partdiff GUITAR confirmation
  (the ROWFIX same-family sibling W4.4 explicitly deferred), and every future UI parity
  item — the UI family produced 6-7 of the 11 shipped flips, so this is the
  highest-fix-per-wave instrument in the roadmap. Also feeds R2's rule-as-harness: G3/G4
  are permanent one-command retrodiction tests that double as the instrument's own
  known-good/known-bad separation proof.

## Appendix — commands run for §2 (reproducibility)

```
grep -n "drawlog" native/src/rb3_http_server.cpp                 # :356-374 endpoint
sed -n 170,240p native/src/rb3_http_handlers.cpp                 # HandleDrawLog shape
sed -n 5230,5360p <engine>/src/platform/Rnd_Wgpu_RB3.cpp         # DrawMesh/RecordDrawLog
sed -n 1,60p <engine>/src/platform/RB3DrawLogDebug.h             # record contract
sed -n 295,330p <engine>/CMakeLists.txt                          # dc3 vs rb3 TU lists
grep -n "DrawShowing" <engine>/src/platform/Rnd_Wgpu_RB3.cpp     # :5468 strong def
sed -n 1722,1745p src/system/rndobj/Text.cpp                     # glyph draw path
grep -n "LoadOp" <engine>/src/platform/RB3PostProc.cpp           # :93 menuBoundary depth
sed -n 160,215p <engine>/src/platform/RB3MaterialBinder.cpp      # UI floor + FLOOR_DBG
grep -n "GetPanelRefs\|mPushedScreens\|TheUI" src/system/ui/{UIScreen.h,UI.h}
sed -n 288,360p src/band3/meta_band/SongSelectPanel.cpp          # C2a census walk
```
