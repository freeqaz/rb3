# WHITE-fix — STAGED scene-side patch (Lane A owns the TUs; DO NOT apply this wave)

**Verdict:** the engaged-venue WHITE over-exposure is **SCENE-SIDE** — the near-white is
baked into the scene render (`standard_wgsl.inc` fragment output) *before* the composite
samples it. The Wave-8 chroma-preserve composite fix cannot recover it because the scene's
**per-channel** highlight compression + sRGB encode drives hot lit surfaces to zero-chroma
white; a composite operating on an already-white (chroma≈0) pixel has nothing to restore.

**Fence:** Lane B (me) owns `RB3PostProc.*` + `rb3_postproc.wgsl.inc`. The fix belongs in
`Rnd_Wgpu_RB3.cpp` (Lane A) + the **DC3-shared** `standard_wgsl.inc` / `UniformStructs.h`.
Per WAVE9_REVIEW A5, Lane B **stages** the patch (W3.3 precedent). This file is the design +
exact hunks for the coordinator / Lane A to review and land in Wave 10. **It has NOT been
applied.**

---

## Root mechanism (source-derived + measured)

`standard_wgsl.inc` `fs_main` produces the venue backdrop color as:

1. `litTerm = softClipLighting(ambient + Σ diffuse)` — **per-channel** Reinhard soft-clip,
   knee 1.0 / ceiling 1.05 (`:609`).
2. `finalColor = baseColor.rgb * litTerm + specular` (`:850`).
3. `finalColor = compressHighlights(finalColor)` — **per-channel** tanh rolloff toward 1.0,
   knee 0.9 (`:621`, called `:878`).
4. `finalColor = linearToSrgb(finalColor)` (`:886`) — sRGB OETF pushes mid-highs up
   (0.85→0.94).

Steps 1+3 are **per-channel**: each channel independently approaches its ceiling, so a
bright *colored* pixel loses chroma → white. On the hottest venue moments (many overlapping
point/dir lights on a pale surface) all three channels clamp near 1.0 → a flat white region
whose chroma is **gone before the composite runs**. Measured: the raw scene (`RB3_PP_OFF=1`,
no composite) reads **more** blown than the composited output (see STATUS.md discriminator).

The correct fix is a **luminance-preserving** highlight compression on the venue path:
compress the *luminance* and scale RGB uniformly, so hue/saturation survive the rolloff and a
hot venue moment stays **bright-but-colored** (the retail/GX look) instead of clipping white.
Scoped to the venue (world.cam) via a new `SceneUniforms.venueHighlightLumaMode` gate so
DC3 + menus + game.cam keep today's per-channel path byte-identical.

---

## Patch (3 files, all Lane-A / shared-contract)

### 1. `src/gfx/UniformStructs.h` — repurpose one existing pad (size stays 656, no static_assert change)

```diff
     float numPointLights;         // f32
     float pointFalloffMode;       // f32 — 0 (default/DC3) = legacy saturate(1-d/r)^2 ...
-    float _padPL[2];
+    // WHITE-fix (Wave 9): 0 (default/DC3/game.cam/menus) = per-channel compressHighlights
+    // (byte-identical); 1 = luminance-preserving highlight compression (chroma survives the
+    // rolloff) — set ONLY by RB3's world.cam venue-light upload. Mirrors pointFalloffMode.
+    float venueHighlightLumaMode; // f32
+    float _padPL1;                // f32
```

DC3 zero-blast: DC3's scene-uniform fill never writes `venueHighlightLumaMode` (leaves the
default-0 zeroed struct), so DC3's rendered scene is byte-identical. This is a **pad rename**
(no layout/size change); `static_assert(sizeof(SceneUniforms)==656)` still holds.

### 2. `src/gfx/standard_wgsl.inc` — mirror the field + add the luma-preserving path

Shader struct (mirror the C++ rename, `standard_wgsl.inc:80-82`):

```diff
     pointFalloffMode: f32,
-    _padPL2: f32,
+    venueHighlightLumaMode: f32,
     _padPL3: f32,
```

Add a luma-preserving variant next to `compressHighlights` (`~:621`):

```diff
 fn compressHighlights(color: vec3f) -> vec3f {
     let knee = vec3f(kHighlightCompressionKnee);
     return select(
         color,
         knee + (vec3f(1.0) - knee) * tanh((color - knee) / (vec3f(1.0) - knee)),
         color > knee
     );
 }
+
+// WHITE-fix (Wave 9): luminance-preserving highlight compression for the VENUE path.
+// Compress the LUMINANCE with the same tanh shoulder, then scale RGB uniformly so
+// hue/saturation survive — a hot venue moment stays bright-but-colored instead of the
+// per-channel path flattening every channel to 1.0 (white). Identity below the knee, so
+// correctly-exposed venue frames are unchanged.
+fn compressHighlightsLuma(color: vec3f) -> vec3f {
+    let luma = dot(color, vec3f(0.2126, 0.7152, 0.0722));
+    if (luma <= kHighlightCompressionKnee) { return color; }
+    let k = kHighlightCompressionKnee;
+    let rolled = k + (1.0 - k) * tanh((luma - k) / (1.0 - k));
+    return color * (rolled / max(luma, 1e-4));
+}
```

Gate the call site (`~:878`):

```diff
-    finalColor = compressHighlights(finalColor);
+    if (scene.venueHighlightLumaMode > 0.5) {
+        finalColor = compressHighlightsLuma(finalColor);
+    } else {
+        finalColor = compressHighlights(finalColor);
+    }
```

DC3 / menus / game.cam pass `venueHighlightLumaMode == 0` → the `else` branch → **byte-
identical** to today. The `if` branch is inert for every non-venue draw.

> **Optional (stronger):** for full chroma fidelity also make `softClipLighting` (`:609`,
> applied to the lighting SUM at `:850`) luminance-based on the same gate. Staged as a
> follow-on; the `compressHighlights` change above is the dominant term (it is the last
> per-channel step before sRGB) and is sufficient to move the WHITE class → colored on the
> reproducer. Land `compressHighlights` first, re-measure, add `softClipLighting` only if a
> residual persists.

### 3. `src/platform/Rnd_Wgpu_RB3.cpp` — set the gate on the world.cam venue upload

New flag accessor next to `sVenuePointFalloffGx()` (`~:1167`), default-OFF:

```cpp
// WHITE-fix (Wave 9): luminance-preserving venue highlight compression (keeps hot venue
// moments colored instead of clipping white). Default-OFF; opt-in RB3_VENUE_WHITE_GUARD.
static bool sVenueWhiteGuard() {
    static int v = -1;
    if (v < 0) { const char* e = getenv("RB3_VENUE_WHITE_GUARD"); v = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return v != 0;
}
```

Set it in the ENGAGED venue branch, right next to `s.pointFalloffMode` (`:1460`):

```diff
         s.pointFalloffMode = sVenuePointFalloffGx() ? 1.0f : 0.0f;
+        s.venueHighlightLumaMode = sVenueWhiteGuard() ? 1.0f : 0.0f;
```

Everywhere else `SceneUniforms` is value-initialized (`SceneUniforms s{}` / memset),
`venueHighlightLumaMode` stays 0 → per-channel path. Flag-OFF: `sVenueWhiteGuard()==0` →
field is 0 even on the venue path → **byte-identical** (drawlog 792).

---

## Gates the Wave-10 implementer must run (inherits WASH-fix's set)

- **Reproducer fail-red:** `venue_light_off` arm (flood) + this fix ON — WHITE-class rate and
  `hi_frac`/`mid_sat` on the reproducer vs OFF (paired continuous delta; expect mid_sat ↑,
  hi_frac ↓, colored where OFF is white). Plus the **natural** engaged WHITE boot
  (`disc1_default_03`-class shot) re-captured with the fix.
- **flag-OFF byte-identical:** `drawlog-golden.py --fixed-clock --canonical-order` → 792.
- **DC3 zero-blast — NOTE:** unlike the Wave-8 composite fix, this touches the **shared**
  `standard_wgsl.inc` + `UniformStructs.h`. The diff is **non-empty but behaviorally inert**
  for DC3 (gate field 0 → per-channel branch; pad rename, size 656 held). The coordinator
  must accept behavioral-zero-blast here, or the change must be re-homed. `milo-engine-tests`
  198/0/2 (build-tests are dc3-backend and don't compile the RB3 TU, but DO compile
  `UniformStructs.h` — the `static_assert` guards the size).
- **lineup-gate.py** default build PASS.

## Rejected alternatives (documented so Wave 10 doesn't re-explore)

- **Composite-side fix (my fence):** REFUTED. The scene already tonemaps to [0,1]
  (`compressHighlights` + sRGB) and clamps at the RGBA8Unorm intermediate write
  (`RB3PostProc.cpp:155`); no HDR headroom survives to a float intermediate, and no grade on
  a chroma≈0 white pixel can restore color. `RB3_PP_LUMA_CEILING` (Wave-7) and chroma-preserve
  (Wave-8) both hit this wall.
- **Blunt venue exposure cut in `Rnd_Wgpu_RB3.cpp` only** (lower `sVenueDirExposure` /
  `sVenuePointExposure`): dims the WHOLE venue to fix a localized blowout and regresses the
  A2/A3/A4 glow-campaign lighting tuning. NOT recommended.
