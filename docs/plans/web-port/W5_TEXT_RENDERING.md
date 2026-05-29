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
