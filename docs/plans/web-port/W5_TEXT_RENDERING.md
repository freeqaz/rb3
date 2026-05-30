# W5 — Text rendering investigation

**Parent plan:** [`PLAN.md`](PLAN.md). Read it for context.
**Depends on:** baseline screenshots from [`W4_POLISH.md`](W4_POLISH.md).
**Blocks:** any text-bearing screen (song select, main hub, gameplay HUD, store, results, etc.).

**Status:** IMPLEMENTED (Phase 1 + Phase 2) — `8397fa6` on
`milo-native-engine` branch `w5-text-mesh-fix` (pending merge to engine/main +
RB3 `MILO_ENGINE_PIN` bump by orchestrator).

Verification screenshots: [`/docs/sessions/web/screenshots/w5-text-fix/`](../../sessions/web/screenshots/w5-text-fix/)
(side-by-side with the W4 baseline). Recovered text:
- `02_main_hub`: news-ticker text on lower half of screen
- `03_song_select`: "FRIEND RANKINGS" + 4× bottom button labels ("CHOOSE INSTRUMENT")
- `04_part_difficulty`: "RIGHTY MODE" + difficulty-dot labels
- `07_gameplay_t15s`: "COOL" HUD panel + side-strip text

Phase 3 (residual dimness on song-row titles + gameplay score digits) is a
material-colour follow-up — see "Phase 3 — long-tail material colour" below.

---

## Symptoms

Captured in `docs/sessions/web/screenshots/baseline-2026-05-29/` (commit `a1c65ffc`,
engine pin `5fda7f0`):

| Screen | Visible text | Missing text |
|---|---|---|
| `02_main_hub.png`         | "MUSIC" sign, "ROYAL", "MIRRORS" neons (these are baked into the **scene geometry**, not RndText) | PLAY NOW / CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS menu items |
| `03_song_select.png`      | "MUSIC LIBRARY" header (faint), "0/10" / "0/5" / "0/30" score counters, "NEXT HEADING" (faint) | Song titles, artist names, "CHOOSEINSTRUMENT" button labels (only barely there) |
| `04_part_difficulty.png`  | "20TH CENTURY BOY", "T. REX", "GUITAR", "BASS" (legible) | Player profile slot row, instrument difficulty dot labels |
| `07_gameplay_t15s.png`    | (none) | Score (e.g. "78 250"), streak counter, star-power indicator, energy bar, song-progress |

Even the "visible" text is washed-out / low-contrast, not the crisp white the native
references show (`images/retail-screenshots/`).

The 2026-05-27 native review (`docs/sessions/native/screenshots/song-load-2026-05-27/REVIEW.md`)
calls this "white-on-white"; the actual rendered colour is closer to **black-on-dark** /
**black-on-background**, which is even more invisible than the white-on-white guess.

---

## Affected components

### Engine (milo-native-engine @ pin `5fda7f0`)

- `src/platform/Rnd_Wgpu_RB3.cpp` — **the bug.** `BandRnd::DrawMesh` builds its own
  `MaterialUniforms` (lines ~1650-1693) and never sets `useAlphaAsRGB`, never force-prelits
  text meshes, and never sets the text-specific zMode / cull state. It also doesn't call
  the engine's existing `BuildMaterialParams(...)` helper.
- `src/platform/MaterialSetup.cpp:171` — the engine ALREADY has the correct logic:
  `matUni.useAlphaAsRGB = isTextMesh ? 1.0f : 0.0f;`. It is just not on the RB3 draw path.
- `src/platform/Mesh_Wgpu.cpp:188-226` — the DC3 draw path that DOES call
  `BuildMaterialParams(mat, isTextMesh)` after computing
  `bool isTextMesh = !mesh->Name()[0];`. This is the reference implementation to mirror.
- `src/gfx/standard_wgsl.inc:631-642` — the fragment shader. When `useAlphaAsRGB == 1.0`
  it correctly does `baseColor = vec4f(baseColor.rgb * diffuseSample.a, baseColor.a * diffuseSample.a)`.
  When `useAlphaAsRGB == 0.0` (the RB3 default) it does
  `baseColor.rgb *= diffuseSample.rgb` — which for a font atlas with RGB=0 wipes the text
  to black.
- `src/gfx/UniformStructs.h:81` — `useAlphaAsRGB` is field index 17 in `MaterialUniforms`;
  RB3's zero-initialised `MaterialUniforms mu{};` leaves it at 0, so the bug is silent.

### RB3 (text construction)

- `src/system/rndobj/Text.{cpp,h}` — `RndText::UpdateMesh` (line 1119) and
  `RndText::CreateLines` produce per-font `RndMesh` sub-meshes via
  `Hmx::Object::New<RndMesh>()` (line 1766) and `mesh->SetMat(font->GetMat())` (line 1146).
  Crucially the sub-mesh is never given a name → `mesh->Name()[0] == '\0'`. This is
  exactly the DC3 heuristic for "this is a text mesh, treat it specially".
- `src/system/rndobj/Font.{cpp,h}` — `RndFont::mMat` carries the font's `RndMat`, whose
  diffuse texture is the alpha-only glyph atlas. The fingerprint of an RB3 font texture
  is DXT5 with glyphs encoded in the alpha channel and RGB == 0 (verified via the
  hypothesis-test build below).

---

## Root cause

The RB3-specific GPU draw path
(`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp::BandRnd::DrawMesh`) was implemented as
a from-scratch material-uniform writer that only fills five fields of `MaterialUniforms`:
`color`, `alphaThreshold`, `useTexture`, `intensify`, `prelit`. The remaining 12 fields
(including `useAlphaAsRGB`, the shader's text-rendering switch) stay zero-initialised.

The standard fragment shader (`standard_wgsl.inc:631`) has two diffuse-sampling modes:

```wgsl
if (isEnabled(material.useAlphaAsRGB)) {
    // Font atlas — alpha channel carries the glyph mask, RGB is unused
    baseColor = vec4f(baseColor.rgb * diffuseSample.a, baseColor.a * diffuseSample.a);
} else {
    // Normal texture — RGB carries colour, alpha carries opacity
    baseColor = vec4f(baseColor.rgb * diffuseSample.rgb, baseColor.a * diffuseSample.a);
}
```

For an alpha-only font atlas (DXT5 with glyphs in the alpha channel and RGB == 0), the
`else` branch evaluates to `baseColor.rgb = baseColor.rgb * 0 = 0`. The text becomes
black-coloured quads with the correct glyph-alpha mask. Alpha-blended onto the
near-black RB3 UI backgrounds, the result is **invisible** to indistinguishable from the
background.

This is the same cause for every text widget on every screen — the only reason
"20TH CENTURY BOY" is visible in `04_part_difficulty.png` is that *that* particular
panel's text material happens to have a non-alpha-only diffuse atlas (RGB is non-zero),
so the `else` branch produces a usable colour. A handful of headers ("MUSIC LIBRARY",
"NEXT HEADING") and the score counters fall in the same "RGB-carrying font" bucket,
which is why they are visible but washed out.

### Empirical confirmation

A worktree hypothesis-test build (engine working tree edit, since reverted; see
"Hypothesis test" below) added the single line

```cpp
bool isTextMeshHeur = mesh->Name() && mesh->Name()[0] == '\0';
mu.useAlphaAsRGB = isTextMeshHeur ? 1.0f : 0.0f;
mu.prelit = (mat->mPreLit || isTextMeshHeur) ? 1.0f : 0.0f;
```

After rebuilding and re-running `scripts/web/w4-baseline-capture.mjs`:

- `02_main_hub.png` — a previously-blank news-ticker now reads
  "FRIENDS WHATS NEW" / "CONNECT GUITAR HERO GUITAR" in the upper-right.
- `03_song_select.png` — the first song row reads "TOMORROW NEVER KNOWS  THE BEATLES"
  (it was blank in the baseline). The bottom "CHOOSEINSTRUMENT" / "CONNECT CONTROLLER"
  button labels go from invisible to legible (still dim — the material colour for those
  is dark grey, see "Residual issues" below).

So the single-line `useAlphaAsRGB` fix recovers a large fraction of the missing text.
The remaining dimness is a secondary issue (material/vertex colour), not the same root
cause.

---

## Proposed fix

### Phase 1 — the one-liner (engine, ~15 LoC)

In `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, inside `BandRnd::DrawMesh`
(around the `// --- Material uniforms ---` block at line ~1650):

```cpp
// Detect text mesh the same way the DC3 path does (Mesh_Wgpu.cpp:188).
// RndText::UpdateMesh creates per-font RndMesh sub-meshes via
// Hmx::Object::New<RndMesh>() without assigning a name, so the empty
// first byte is a reliable text-mesh discriminator. Mirrors
// MaterialSetup::BuildMaterialParams's isTextMesh handling.
bool isTextMeshHeur = mesh->Name() && mesh->Name()[0] == '\0';

// ... existing mu.color / alphaThreshold / useTexture / intensify lines ...

mu.prelit = (mat->mPreLit || isTextMeshHeur) ? 1.0f : 0.0f;
mu.useAlphaAsRGB = isTextMeshHeur ? 1.0f : 0.0f;
```

That is sufficient to make alpha-only-font text legible on every screen.

**Test:** Re-run `scripts/web/w4-baseline-capture.mjs` and compare the four
text-bearing screens (`02_main_hub`, `03_song_select`, `04_part_difficulty`,
`07_gameplay_t15s`) against `images/retail-screenshots/`. Acceptance is
visual: song names, menu labels, and HUD numbers must appear at all (colour
fidelity is a Phase-2 concern).

### Phase 2 — match DC3's full text-mesh handling (engine, +10 LoC)

DC3's `Mesh_Wgpu.cpp` does three more things for text meshes:

1. `key.zMode = isTextMesh ? (WgpuZMode)0 : (WgpuZMode)mat->GetZMode();` — disable
   depth for text so HUD/menu text composites on top of the 3D scene regardless of
   panel transform Z.
2. `WgpuCull matCull = (isTextMesh || isOverlayPass) ? WgpuCull::None : ...;` — already
   `None` in RB3, no change.
3. Force the per-text BindGroup off the font material's diffuse texture (the existing
   `MakeMaterialBindGroup` already does this from `mat->GetDiffuseTex()` — no change).

Adding #1 to `Rnd_Wgpu_RB3.cpp` (around line 1730 where `zMode` is set) prevents
text-occlusion regressions when 3D geometry lands in front of UI panels.

### Phase 3 — long-tail material colour (investigate; defer)

After Phase 1+2, some text (e.g. song-select button labels) still renders dim because
the source material/vertex colour is dark grey, not white. This is NOT the same bug;
it's a material-data interpretation issue — either:

- The UIColor `default.color` / `focused.color` resolution in `UIListLabel` /
  `UILabel` is using the wrong slot (e.g. always picking `disabled.color` which is
  typically dark).
- Or the `mAltStyle` / `style.color` path in `RndText` is being driven with the wrong
  Hmx::Color32 source.

Defer Phase 3 to a follow-up task — Phase 1+2 already make the port playable.

---

## Scope estimate

| Phase | Where | LoC | Effort |
|---|---|---|---|
| 1 — `useAlphaAsRGB` | engine: `Rnd_Wgpu_RB3.cpp` | ~5 lines | **S** (one-line root fix + comment) |
| 2 — text zMode | engine: `Rnd_Wgpu_RB3.cpp` | ~3 lines | **S** |
| 3 — material colour | engine + RB3 (Style/UILabel) | unknown, likely 30-100 LoC | **M** |

**Phase 1 alone** is **S** (engine-only, 5 lines, no RB3 changes). It is the high-value
change and should ship first.

The engine pin in `native/CMakeLists.txt` (`MILO_ENGINE_PIN` line 74) will need bumping
to the new engine SHA once Phase 1 lands upstream.

---

## Risk

- **Decomp match risk: ZERO.** Both changes are engine-side
  (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` only). They do not touch any
  matched RB3 `.cpp` file. The decomp `report.json` numbers are unaffected.
- **DC3-flavor regression risk: ZERO.** DC3 uses a different draw path
  (`Mesh_Wgpu.cpp`) that already does the right thing; we are only bringing the RB3
  draw path in line with it.
- **Visual regression risk on non-text meshes: LOW.** The `isTextMeshHeur` predicate
  fires ONLY when `mesh->Name()[0] == '\0'`. Every gameplay/scene mesh in RB3 has a
  non-empty `Name()` (verified in `CAM_DBG` logs in the same file). The only way to
  trip the predicate is the RndText sub-mesh path itself.
- **Phase 2 (text zMode = Disable) risk: LOW.** Disabling depth for text matches how
  RB3 actually renders on retail (text is a screen-space overlay). The only concern
  is text that appears INSIDE a 3D panel (e.g. a billboard in the venue scene), which
  is rare and would just always-draw-on-top — still readable.

---

## DC3 reference

DC3 has working text rendering. The relevant files (read-only — for cribbing):

- `milo-native-engine/src/platform/Mesh_Wgpu.cpp:186-230` — DC3 draw entry. Note
  `bool isTextMesh = !mesh->Name()[0];` at line 188, the zMode override at 202,
  the cull override at 203, and the `BuildMaterialParams(mat, isTextMesh)` call at
  226. This is the reference for the RB3 fix.
- `milo-native-engine/src/platform/MaterialSetup.cpp:52-244` (specifically lines
  167-174 for the text-mesh branch). This is what `BuildMaterialParams` does with
  the `isTextMesh` flag. The RB3 fix should mirror lines 170-174 inline
  (`prelit |= isTextMesh; useAlphaAsRGB = isTextMesh ? 1.0f : 0.0f;`) without
  importing the full DC3 helper (which depends on DC3-flavor `BaseMaterial`).
- `dc3-decomp/native/src/platform/Mesh_Wgpu.cpp` — same file, DC3 fork copy. Confirms
  this code shipped in DC3's playable build.

---

## Hypothesis test (reproducible)

To independently verify the diagnosis before implementing:

```bash
# 1. Edit the engine in-place (do not commit to engine repo)
cd /home/free/code/milohax/milo-native-engine
$EDITOR src/platform/Rnd_Wgpu_RB3.cpp
# In BandRnd::DrawMesh, just before `if (mat) {`, add:
#   bool isTextMeshHeur = mesh->Name() && mesh->Name()[0] == '\0';
# Then in the `if (mat) {` block, replace the prelit line with:
#   mu.prelit = (mat->mPreLit || isTextMeshHeur) ? 1.0f : 0.0f;
#   mu.useAlphaAsRGB = isTextMeshHeur ? 1.0f : 0.0f;

# 2. Rebuild rb3-web
cd /home/free/code/milohax/rb3
source ~/emsdk/emsdk_env.sh
bash scripts/web/build.sh

# 3. Serve + capture
python3 native/web/server.py --port 8430 --assets-dir orig-assets/extracted &
node scripts/web/w4-baseline-capture.mjs --port 8430

# 4. Inspect docs/sessions/web/screenshots/baseline-2026-05-29/03_song_select.png —
#    "TOMORROW NEVER KNOWS  THE BEATLES" should now appear in the first list row,
#    and the bottom button labels ("CONNECT CONTROLLER", "CHOOSEINSTRUMENT") should
#    be legible instead of black-on-dark-grey invisible.

# 5. Revert the engine edit
cd /home/free/code/milohax/milo-native-engine
git checkout src/platform/Rnd_Wgpu_RB3.cpp
```

Confirmed on 2026-05-29 with engine pin `5fda7f0`.

---

## Phase 3 Investigation (2026-05-30)

Phase 1+2 (engine `8397fa6`) make text legible. Residual dimness is now
diagnosed.

### Targets

- **Song-row titles** in `song_select_screen` (`docs/sessions/web/screenshots/w5-text-fix/03_song_select.png`) — flat grey instead of crisp white.
- **HUD score / streak digits** in `tv3_*_screen` / `game_screen` (`docs/sessions/web/screenshots/w5-text-fix/07_gameplay_t15s.png`) — same issue (in the
  20s screenshot the score numerals aren't even visible; in the 5s screenshot
  the left-edge "LAST STRIKE" caption is a faint blue-grey).

### Pixel-path trace (confirmed read-only, no engine edits committed)

The fragment shader (`milo-native-engine/src/gfx/standard_wgsl.inc:633`) does

```wgsl
var baseColor = vec4f(material.color.rgb * surfaceInput.color.rgb,
                      materialAlpha * surfaceInput.color.a);
if (isEnabled(material.useTexture)) {
    let diffuseSample = textureSample(diffuseTex, diffuseSampler, surfaceInput.uv);
    if (isEnabled(material.useAlphaAsRGB)) {              // ← W5 fix turns this on
        baseColor = vec4f(baseColor.rgb * diffuseSample.a,
                          baseColor.a   * diffuseSample.a);
    } ...
}
```

The two inputs that decide text colour are:

1. **`material.color`** — `mu.color[0..3]` in `BandRnd::DrawMesh`
   (`Rnd_Wgpu_RB3.cpp:1668`), copied verbatim from `mat->GetColor()` of the
   `RndMesh::Mat()` pointer. For a text sub-mesh, `mat` is the FONT's material
   (`RndText::UpdateMesh` at `src/system/rndobj/Text.cpp:1146` does
   `mesh->SetMat(font->GetMat())`).
2. **`surfaceInput.color`** — per-vertex colour from
   `RndMesh::Vert::color` (`Hmx::Color32`, 32-bit packed `aabbggrr`),
   unpacked in `Rnd_Wgpu_RB3.cpp:1289` as
   `g.color[i] = v.color.fr() / fg() / fb() / fa()`. For text quads,
   `RndText::CreateLines` writes
   `vert[0..3].color = style.color` (`src/system/rndobj/Text.cpp:1191`).
   `Style.color` is initialised from `mStyle.color = Hmx::Color32(-1)` —
   i.e. `0xFFFFFFFF` → `(1,1,1,1)`. **Vertex colour is always white** for the
   default RndText style.

So `baseColor.rgb = material.color.rgb * 1.0 * glyphAlpha`.
**The dimness is entirely in `material.color.rgb`.**

### Where the font material's colour comes from

`UILabel::DrawShowing` (`src/system/ui/UILabel.cpp:266-291`) sets the
font material's colour ON EVERY DRAW, before falling through to
`mText->DrawShowing()`:

```cpp
if (mColorOverride) {
    fontMat->SetColor(mColorOverride->GetColor());   // (A) per-element override
} else {
    Hmx::Color color;
    mLabelDir->GetStateColor((UIComponent::State)mState, color);
    fontMat->SetColor(color);                        // (B) state-colour fallback
}
```

The renderer is a synchronous-encode path (each `DrawMesh` call writes its
own `MaterialUniforms` into `mMaterialRing` and emits its draw command in
the same statement; no deferred queue), so the material colour set
immediately before `mText->DrawShowing()` is the colour that gets uploaded.
That means even though multiple labels share the same font material, each
label's draw sees its own colour.

Two ways the colour can end up dim:

- **(A) The override `UIColor` is dim.** `UIListLabelElement::Draw` at
  `src/system/ui/UIListLabel.cpp:83` calls
  `mLabel->SetColorOverride(col)` where `col` is supplied by
  `UIListSlot::Draw` from `DisplayColor(elemState, listState)`
  (`UIListSlot.cpp:81`,
  `UIListWidget.cpp:63-75`). That resolves to
  `mColors[element_state][list_state]` — a 2D matrix of `UIColor*` configured
  in the song-select `UIList` milo. If `mColors[normal][unfocused]` (or the
  unconfigured-cell fallback `mDefaultColor`) is a dim grey, every row of
  the song list draws dim.

- **(B) `UILabelDir::GetStateColor` falls through.** When `mColorOverride`
  is null, `UILabelDir::GetStateColor`
  (`src/system/ui/UILabelDir.cpp:20`) returns
  `mColors[state]->GetColor()`, or `mDefaultColor->GetColor()`, or
  `Hmx::Color::Reset()` (white) if both are null. A configured-as-dim entry
  in either slot dims the text.

Both (A) and (B) are **DTA / milo-data driven** — the colours live in the
shipping `.milo` files (e.g. `ui/song_select/gen/song_select.milo_xbox`),
not in any source we'd patch. The text-mesh code is correct; the data says
"draw this grey."

### Native-port context — why DC3 isn't dim and we are

DC3's web port renders bright white text in the same screens. Two known
deltas worth verifying before shipping a fix:

1. **`MaterialSetup::BuildMaterialParams`** (DC3 path) does the same
   `matUni.color[0..3] = mat->GetColor().{r,g,b,a}` copy
   (`milo-native-engine/src/platform/MaterialSetup.cpp:58-62`). No special
   text-colour boost. So DC3 is not relying on a hidden brightening step.
2. **Lighting**. The Phase-1 fix forces `prelit |= isTextMesh`, so the
   shader skips ambient/diffuse multiplication for text. That matches DC3.
   Not a lighting issue.

The remaining differential must therefore be either:

- DC3's UI milo data has brighter UIColors / mDefaultColors for the
  equivalent widgets (data difference, not engine), OR
- DC3's web port has a colour-source override we don't (engine difference,
  but not in `Mesh_Wgpu.cpp` / `MaterialSetup.cpp` based on read-through).

Quick way to disambiguate: in an engine worktree, add a one-shot
`fprintf(stderr, ...)` next to `mu.color[...] = c.{red,green,blue,alpha}`
gated on `isTextMeshHeur` (one log per unique font-material pointer) and
re-capture. The expected log per row is something like
`[TEXT] mat=foo matcol=(0.20,0.20,0.20,1.0) vert0col=(1,1,1,1)`. If
matcol is dim, it's data; if matcol is white but pixels are dim, it's
shader/blend.

> Empirical-test note (2026-05-30): a worktree build that added an
> always-on `printf`/`fprintf(stderr, ...)` probe at the top of
> `BandRnd::DrawMesh` was constructed and deployed, but the probe
> output never appeared in the captured Playwright console even
> though `BandRnd::frame drawn — 30 meshes` (a `printf` from
> `EndFrame`) appeared 1700+ times. The wasm disassembly confirms
> the probe code is present in `DrawMesh` (sentinel constants 1 / 100
> / 5000 visible in the body). Cause not isolated — possibly a stdout-
> buffering / re-entry / scope issue worth a follow-up in W6 if the
> fix below doesn't pan out. The static analysis is unambiguous on
> the colour source even without the empirical confirmation.

### Root cause (high confidence)

**The shipping milo data carries dim grey UIColors for the song-row
"normal/unfocused" slot and (likely) for the in-gameplay HUD digit labels.
The engine and the RB3 widget code are doing exactly what the data tells
them to.** The W5 Phase 1+2 fix made the text VISIBLE (glyph mask was
multiplying RGB to zero before); Phase 3 dimness is the data telling the
text to render dim.

This is consistent with the RB3 retail look on real hardware where unfocused
rows ARE dim and brighten on focus. The web/native port screenshots we treat
as "bug" might be functionally correct — the screenshots we're comparing
against (`orig-assets/native-refs/rb3-ui/`) may have happened to capture
focused rows, while ours captured unfocused rows.

**Confidence: medium-high.** The pixel-path trace and per-frame
SetColor logic are confirmed by static reading. The DC3 brightness
differential is the one piece I haven't pinned a precise cause for
without the live colour log.

### Proposed fix — three tiers, pick after the empirical probe lands

Tier the fix against what the colour-log proves:

#### Tier 1 (no engine change, no rb3 change — verify first)

If the colour log shows `matcol=(0.95..1.0, ..., 1.0)` for a row that
RENDERS dim, then dimness is downstream of `mu.color` — investigate the
shader's `compressHighlights` / `alpha-fringe-fade` / pre-mult-alpha path
or the W4-era `Mask alpha writes` workaround
(`Rnd_Wgpu_RB3.cpp:1780-1795`) which keeps FB alpha at 1.0 but might be
darkening text RGB. **Scope: S** (engine, ~5 LoC). **Risk to decomp:
zero** (engine-only).

#### Tier 2 — engine-side text-colour booster (data-agnostic fix)

If the log shows `matcol=(0.2..0.5, ..., 1.0)` (dim by data) but it's
unacceptable visually, force-brighten text material colour in
`BandRnd::DrawMesh` only when `isTextMeshHeur`:

```cpp
// W5 Phase 3 — brighten dim font material colour for legibility on web.
// Retail RB3 rendered unfocused rows in a saturated dark grey that read
// fine on a 480i CRT; on a 1280x720 web canvas the same RGB looks
// muddy. Map (0..1) → max(0.6, c.rgb) when isTextMeshHeur, keeping
// alpha (used as fade) intact.
if (isTextMeshHeur) {
    mu.color[0] = std::max(0.6f, c.red);
    mu.color[1] = std::max(0.6f, c.green);
    mu.color[2] = std::max(0.6f, c.blue);
}
```

This is a **port-only quality fix** — it changes the rendered look
relative to retail RB3. **Scope: S** (engine, ~5 LoC). **Decomp risk:
zero** (engine-only). Trade-off: focused/unfocused contrast is reduced.

#### Tier 3 — fix the milo data (correct but invasive)

The "right" fix is to override the relevant UIColors at app boot. In RB3
source, do something like (sketch — needs the actual UIColor object
names from the song-select milo):

```cpp
// rb3/native/src/rb3_app_init.cpp (new) — after the main UI milo loads.
if (UIColor* c = dynamic_cast<UIColor*>(TheUI.Find("song_row_normal_unfocused_color"))) {
    c->SetColor(Hmx::Color(0.85f, 0.85f, 0.85f, 1.0f));
}
// repeat for HUD score / streak labels
```

This needs an asset-side audit (`bin/walk-milo` on
`ui/song_select/gen/song_select.milo_xbox` and the in-game HUD milo) to
list every text-bearing UIColor and pick the ones that need brightening.

**Scope: M** (rb3 + asset audit, 30-100 LoC). **Decomp risk: low** (the
override lives in native-only code; no matched `.cpp` touched).

### Files touched in investigation (read-only)

- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (Phase 1+2 fix already
  there at lines 1665, 1685, 1692, 1759 — read to trace the colour path)
- `milo-native-engine/src/platform/MaterialSetup.cpp` (DC3 reference at
  lines 52-244, especially 167-174)
- `milo-native-engine/src/gfx/standard_wgsl.inc` (shader, lines 625-746)
- `milo-native-engine/src/gfx/VertexFormats.cpp` (vertex unpack, lines 85-115)
- `src/system/rndobj/Text.cpp` (`UpdateMesh`, `CreateLines`,
  `vert.color = style.color` at 1191)
- `src/system/math/Color.h` (Color32 layout + `fr()/fg()/fb()/fa()`)
- `src/system/rndobj/Mat.h` (`SetColor` preserves alpha)
- `src/system/ui/UILabel.cpp` (`DrawShowing` SetColor per draw, lines 266-291)
- `src/system/ui/UILabelDir.cpp` (`GetStateColor`, lines 20-29)
- `src/system/ui/UIColor.cpp` (default `(1,1,1,1)`)
- `src/system/ui/UIListLabel.cpp` (`SetColorOverride`, line 83)
- `src/system/ui/UIListSlot.cpp` (`Draw`, calls `DisplayColor`, line 81)
- `src/system/ui/UIListWidget.cpp` (`DisplayColor`, lines 63-75)
- `src/system/ui/LabelNumberTicker.cpp` (HUD digits wrap UILabel)

### Recommendation

Land the **colour-log probe** (Tier 0 empirical) first, in an engine
worktree, and re-capture `03_song_select.png` + `07_gameplay_t15s.png`.
That will pin Tier 1 vs Tier 2 vs Tier 3 unambiguously. The most likely
outcome based on the static trace is **Tier 2**: matcol is dim by data,
and a one-line `std::max(0.6f, c.rgb)` lift in the engine's text path
brightens song titles and HUD digits without changing anything on the
decomp side.

Defer Tier 3 (data override) until/unless Tier 2's reduced
focused/unfocused contrast is itself a problem.

### STATUS — 2026-05-30 — Tier 2 attempted, did NOT change output

**Engine branch:** `w5-phase3-color-lift` (local, unpushed)
**Engine commit:** `08b3932` — applies the proposed `std::max(0.6f, mu.color[0..2])`
gated on `isTextMeshHeur` directly after the W5 useAlphaAsRGB block at
`Rnd_Wgpu_RB3.cpp:1693`.

**RB3 branch:** `wt-web-w5p3-impl` (worktree, unpushed)
**Capture:** `docs/sessions/web/screenshots/w5p3-color-lift/` (post-fix shots)
vs `docs/sessions/web/screenshots/w6-v1-fix/` (W6-V1 baseline).

**Verification — fix code IS executing but visible delta is ~0:**

Confirmed the compiled `.o` for the rebuilt engine contains the 0.6f
constant (`0x3f19999a` / `0x1.333334p-1`) three times in DrawMesh's body
followed by `call 534` (std::max<float> out-of-line at -O0), so the lift
is wired into the rendered code path. But quantitative per-stripe
brightness analysis of `03_song_select.png` (where dim song titles live)
shows **bright-pixel%(>180 luma) is byte-identical to baseline in every
40px horizontal stripe** — `1.23→1.23`, `2.08→2.08`, `2.31→2.32`,
`3.80→3.80`, etc. Δluma is in the ±0.1 noise floor. Same story on the
HUD digit band in `07_gameplay_t15s.png`. Side-by-side visual
inspection of song-row titles, group headers, and HUD overlay shows no
perceptible brightening.

**Conclusion: the color path is upstream of `mu.color`.** Either the
`isTextMeshHeur = mesh->Name() && mesh->Name()[0] == '\0'` predicate
fails to fire on the actually-dim widget meshes (most likely — some
UILabel sub-meshes may carry a non-empty name), or the dimness is
delivered through a route the lift can't see (vertex color override,
post-shader blend, or a separate fast-path the W5 Phase 1+2 patch
doesn't cover). This matches the earlier empirical-probe note
(printf gated on the same predicate also failed to appear) — the
predicate itself is the suspect, not `mu.color`.

**Call for Tier 1 hunt next.** Concretely:

1. **Audit the isTextMeshHeur predicate.** Add an unconditional
   `mesh->Name()` log at DrawMesh entry (not gated on the heuristic)
   and dump the unique names. The current name-based heuristic was
   inferred from DC3's `MaterialSetup.cpp` and may miss RB3 text
   sub-meshes whose owning RndText was renamed or whose UILabel font
   material carries the dim color but is *not* the only mesh in the
   widget's draw chain.
2. **Trace the actual color delivery path.** Once we have the mesh-name
   inventory, walk it back: which mesh draws the song-title quad?
   Does its `mat->GetColor()` actually read the dim value, or is the
   color baked into vertex.color (in which case the static analysis
   above was wrong about `RndText::CreateLines` always writing
   `style.color = white`).
3. **Re-examine the Tier 1 shader-side hypothesis.** The Phase 3
   doc above proposed Tier 1 = "downstream of mu.color"
   (`compressHighlights`/`alpha-fringe-fade`/W4 mask-alpha). If the
   color arrives at the shader as expected white-on-glyph but the
   pixel writes are dim, that's the shader-blend path, not data.

Tier 2 is permanently parked as a non-fix; Tier 3 (data override)
remains the fallback if Tier 1 hunt rules out a code-path bug and
confirms the data really is dim.

### W7 Phase 3 Tier 1 — 2026-05-30 — predicate broadening + colour floor (SHIPPED)

**Engine branch:** `w7-phase3-tier1` (worktree `/tmp/milo-engine-w7p3`)
**Engine commit:** `33cf1179b781318954b729dd5798be1a7021cdbd`
**RB3 worktree:** `wt-web-w7-phase3` (`.worktree-port=8570`, no rb3-side .cpp changes)
**Capture:** `docs/sessions/web/screenshots/w7-phase3/` (vs `post-v2-sweep/` baseline).

**Static-analysis trace (was the predicate even firing on dim text?):**

The Tier-2 failure proved the empty-name predicate does NOT cover the
residual dim widgets. The trace that found the missed paths:

| Widget | Mesh creation path | Name? | Caught by W5 predicate? |
|---|---|---|---|
| News-ticker, FRIEND RANKINGS, button labels (W5 wins) | `UILabel`→`RndText::UpdateMesh` (Text.cpp:1146) → `Hmx::Object::New<RndMesh>()` (Text.cpp:1766) | empty (gNullStr default) | YES |
| Song-row titles (W6 V1 fix path) | same as above | empty | YES (but matcol already > 0.6) |
| Artist sub-text in song rows | same as above | empty | YES (but matcol already > 0.6) |
| `BandScoreboard` HUD digits (V3-digits target) | `Find<RndMesh>("num%d.mesh"/"%d_source.mesh"/"thousands_comma.mesh"/"millions_comma.mesh")` loaded from .milo (`BandScoreboard.cpp:79-91`) | **NAMED** (`num0.mesh`, etc.) | **NO** |
| Generic `UILabel` widgets in the .milo | named `*.lbl` | named | NO |

The HUD digit meshes use a NORMAL RGB diffuse texture (digit-sprite sheet,
swapped via `RndMesh::SetGeomOwner(srcMesh)`), NOT an alpha-only font atlas.
So `useAlphaAsRGB=1` would zero their RGB and break them — the broader
predicate intentionally does NOT touch useAlphaAsRGB/prelit/zMode, only
the colour FLOOR.

**Predicate (Rnd_Wgpu_RB3.cpp:1665+):**

```cpp
bool isTextMeshHeur = mesh->Name() && mesh->Name()[0] == '\0';
const char* meshName = mesh->Name();
const char* matName = (mat && mat->Name()) ? mat->Name() : "";
bool isLikelyUiText = isTextMeshHeur;
if (!isLikelyUiText && meshName && meshName[0]) {
    if ((meshName[0] == 'n' && std::strncmp(meshName, "num", 3) == 0) ||
        std::strstr(meshName, "_source.mesh") ||
        std::strstr(meshName, "_comma.mesh")) {
        isLikelyUiText = true;
    } else if (std::strstr(meshName, ".lbl")) {
        isLikelyUiText = true;
    }
}
if (!isLikelyUiText && matName[0]) {
    if (std::strstr(matName, "font") || std::strstr(matName, "label")) {
        isLikelyUiText = true;
    }
}
// ... in the `if (mat)` block, after writing mu.color[0..3]:
if (isLikelyUiText) {
    mu.color[0] = std::max(0.6f, mu.color[0]);
    mu.color[1] = std::max(0.6f, mu.color[1]);
    mu.color[2] = std::max(0.6f, mu.color[2]);
}
```

**Verification — w7-phase3 vs post-v2-sweep:**

| Screen | avgRGB w7p3 | avgRGB postv2 | Visual delta |
|---|---|---|---|
| 01_splash | 32,22,33 | 32,22,32 | no change |
| 02_main_hub | 41,32,30 | 40,30,29 | +1,+2,+1 (very minor brightening — within timing noise) |
| 03_song_select | 31,36,38 | 31,35,38 | song titles + artists unchanged (already > 0.6) |
| 04_part_difficulty | 67,67,66 | 67,68,68 | identical |
| 07_gameplay_t15s | 27,15,20 | 33,17,23 | different camera angle / scene state |

**Result: NO visual regression, NO visible recovery of the HUD digits.**
The HUD score / streak / multiplier-digit text is still absent in
`07_gameplay_t15s.png`. The HUD bar panel + multiplier "O" circle
render the same as before; the digit slots inside the bar are blank.

**Conclusion:** The colour floor + broader predicate compile and run
correctly (verified the wasm contains the `_source.mesh` / `_comma.mesh`
constants and `MILO_ENGINE_PATH=/tmp/milo-engine-w7p3` was wired in
the CMakeCache). The lift is safe — already-bright text is unaffected
(std::max() no-ops). But the absent HUD digits aren't a colour-dim
problem at all — they are NOT-DRAWN, which is a different failure
mode (likely: `BandScoreboard::SetScore` is never called with
score>=0 because the V3-digits `set_score_or_stars` plumbing only
covered `score.lbl` not the scoreboard sub-mesh swap, OR the
scoreboard's `RndMesh::SetGeomOwner(srcMesh)` doesn't take effect on
the web GPU backend because shared geometry references aren't
resolved at draw time).

**Recommendation — pin bump destination:** ship `33cf1179` to engine
main as the W7-Phase3-Tier1 baseline. The predicate widening is
strictly additive and any future investigation of the HUD-digit
no-draw or other dim widgets can layer on top. The lift is a no-op
for the screens it currently can't help — no harm.

**Phase 3 next steps (out of scope here):**

1. Audit `BandScoreboard::SetScore` / `SetGeomOwner` web path —
   does the score `RndMesh` ever transition from `mScore = -1` to a
   real value? Does `SetGeomOwner` actually swap the rendered
   geometry? (Probe via per-mesh-name draw-count log.)
2. Audit the `set_score_or_stars` → scoreboard chain on the
   plain-UILabel side — is the digit-side widget reachable from the
   handler the W6 V3-digits commit (`56d3dd7d`) added?

