# R3 — rndobj Divergence: RB3 (Wii-lineage) vs DC3 (Xbox-lineage)

Recon lane R3 for the DC3 Render Bridge study. Read-only comparison of the
render-facing engine classes in `src/system/rndobj` (+ `rndwii`/`rnddx9`
backends) between RB3 and DC3, plus the DC3 native GPU layer, to determine what
a bridge layer must translate.

Paths (absolute):
- RB3 engine: `/home/free/code/milohax/rb3/src/system/rndobj`, `.../rndwii`
- DC3 engine: `/home/free/code/milohax/dc3-decomp/src/system/rndobj`, `.../rnddx9`
- DC3 native GPU: `/home/free/code/milohax/dc3-decomp/native/src/{gfx,platform}`

## Summary

The two render stacks share **class names and the DTA/Object property surface**,
but diverge at the two lowest, most bridge-critical layers: (1) **vertex/material
data model** — RB3 is Wii GX fixed-function (48-byte packed vert, no tangent, TEV
diffuse+emissive material); DC3 is Xbox-360 shader-era (96-byte full-float vert +
tangent, `BaseMaterial` with normal/specular/rim/environ-cube/detail/fur/meta);
and (2) **binary ABI** — the Xbox math types pad `Vector3` to 16B (Transform
0x40 vs Wii 0x30) and DC3's `RndDrawable` base replaces Wii's bit-packed subclass
flags with a plain bool + `mClipPlanes`, so every transform/drawable-derived
class has different member offsets and vtable shape. **Pointer-level sharing of
render objects between the two is impossible; the bridge must be a per-object
translation adapter, not a pass-through.** DC3's native GPU path additionally
hard-assumes **Xbox 360 encodings** for compressed vertices (tangent+binormal
present) and textures (BE-DXT byte-swap + Xbox tiling) — both wrong for Wii
`.milo` assets.

## Findings

### 1. RndMesh::Vert — structurally different (ADAPTER required)
- RB3 `Vert` is 48 bytes: `pos`(0x0) `norm`(0xc) `boneWeights` **Vector4_16_01**
  (0x18, "the hate format" — 16-bit packed) `color` **Hmx::Color32** (0x20,
  packed 32-bit) `uv`(0x24) `boneIndices short[4]`(0x28). **No tangent.**
  `rb3/src/system/rndobj/Mesh.h:62-87`.
- DC3 `Vert` is 96 bytes: `pos`(0x0) `norm`(0x10) `boneWeights` **Vector4**
  (0x20, full float4) `color` **Hmx::Color** (0x30, full float RGBA) `tex`(0x40)
  `boneIndices short[4]`(0x48) **`tangent` Vector4 (0x50)**.
  `dc3-decomp/src/system/rndobj/Mesh.h:54-83`.
- Note the field stride also differs because Xbox pads `Vector3` to 16B (norm at
  0x10 not 0xc). Bridge must repack every vertex: expand 16-bit bone weights →
  float4, unpack Color32 → float RGBA, and **synthesize tangents** (Wii has none).

### 2. Compressed-vertex stream — structurally different; DC3 assumes Xbox packing
- RB3 stores a Wii-encoded blob: `mCompressedVerts`(0x114)/`mNumCompressedVerts`
  (0x118) + GX triangle strips `mStriperResults` (0x100) and
  `GXColor mBoxLightColorsCached[6]`(0x120). `rb3/src/system/rndobj/Mesh.h:366-371`;
  strip machinery `CacheStrips`/`CreateStrip` at `Mesh.h:187,200`.
- DC3's compressed vertex is `struct CompressedVertex_Xbox { float pos xyz; int
  packed color; uint mNormal; uint mTangent; uint mBinormal; uint mBoneIndices;
  uint mBoneWeights; }` — explicitly `_Xbox`, and **carries tangent + binormal**.
  `dc3-decomp/src/system/rndobj/MeshVertCompress.h:6-16`.
- Consequence: DC3's native unpackers `UnpackCompressedVertices` /
  `UnpackCompressedSkinnedVertices` are documented "**Unpack Xbox 360 compressed
  vertices**" (`dc3-decomp/native/src/gfx/VertexFormats.h:43-49`), and
  `MeshGpuCache.cpp:272` skips tangent generation because "compressed meshes
  already have tangent data from the **original Xbox vertex stream**". Feeding a
  Wii `mCompressedVerts` blob to this path decodes to garbage. A Wii bridge must
  never reach DC3's compressed path — it must expand to the uncompressed
  `RndMesh::Vert` (or straight to DC3's `GpuVertex`) and let MikkTSpace generate
  tangents (`MeshGpuCache.cpp:271-273` does exactly this for uncompressed meshes).

### 3. RndMat / BaseMaterial — structurally different (MAPPING + missing-data)
- RB3 `RndMat : Hmx::Object` is a GX-TEV fixed-function material: `mColor`,
  `mTexXfm`, `mDiffuseTex`, `mEmissiveMap`+`mEmissiveMultiplier`,
  `mRefractNormalMap`, plus TEV-era bitfield toggles
  (`mIntensify/mUseEnviron/mPreLit/mPerPixelLit/mScreenAligned`) and 8-bit-packed
  `mBlend/mTexGen/mTexWrap/mZMode/mStencilMode/mShaderVariation`.
  `rb3/src/system/rndobj/Mat.h:119-355`. There is **no normal/specular/rim/cube**
  map on RB3's `RndMat`.
- DC3 `RndMat : BaseMaterial` (`dc3-decomp/src/system/rndobj/Mat.h:56`). All the
  render params live in `BaseMaterial` (size 0x1f8) and cover the full Xbox-360
  shader-era feature set: `mNormalMap`(0xf8) + `mDeNormal`/`mNormDetailMap`/
  tiling/strength, `mSpecularRGB`/`mSpecular2RGB`/`mSpecularMap`/`mAnisotropy`,
  `mRimRGB`/`mRimMap`/`mRimLightUnder`, `mEnvironMap`(RndCubeTex)/falloff/specmask,
  `mFur`(RndFur), `mShaderVariation` (incl. `kShaderVariationWorldProjection`),
  world-projection blends, `mBloomMultiplier`.
  `dc3-decomp/src/system/rndobj/BaseMaterial.h:96,218-373`.
- DC3 `RndMat` also owns a data-driven `MetaMaterial` (`mMetaMaterial` 0x1f8,
  `dc3-decomp/src/system/rndobj/Mat.h:180`) — a template/param system RB3 lacks
  entirely. Bridge mapping: RB3's fields map to the corresponding `BaseMaterial`
  fields, and the DC3-only maps default to null/off (Wii assets never author
  them). `mBlend`/`mZMode`/`mTexWrap`/`mTexGen` enum values are numerically
  identical across both (`Mat.h`/`BaseMaterial.h` enum bodies match), so the
  translate is field-copy + default-fill, not value-remap.

### 4. RndTex / RndBitmap — same-shape data model, divergent texel encoding
- `RndTex` and `RndBitmap` are near-identical in field semantics; both encode
  pixel format in `mBitmap.mOrder` with the **same DXT scheme** (`mOrder & 0x38`,
  `& 1` = RGBA vs BGRA) — the comment block is byte-identical in both headers
  (`rb3/.../Bitmap.h:60-64` == `dc3-decomp/.../Bitmap.h:60-64`). DC3 adds a
  leading `Hmx::CRC mName`(0x0) to `RndBitmap` and `unk2c`(Hmx::CRC) +
  `kRegularLinear`/`AlphaCompress` to `RndTex`, shifting offsets, but the pixel
  container is the same shape (`rb3/.../Tex.h:139-159`, `dc3-decomp/.../Tex.h:156-177`).
- The real divergence is the **platform texel layout**: RB3's `WiiTex` stores a
  `GXTexFmt mFormat`(0x88) and decodes GX-tiled formats (CMPR/I8/RGB5A3/RGBA8);
  `rb3/src/system/rndwii/Tex.h:12,33`. DC3's native texture path assumes **Xbox
  360** encoding (see Finding 6). Verdict: texture bridge = **decode-to-RGBA on
  the RB3 side, then hand linear RGBA to the GPU uploader** — reuse RB3's Wii
  decode; do not route Wii bitmaps through DC3's byte-swap/untile.

### 5. RndDrawable / Transform ABI — structurally different base (offset ripple)
- RB3 `RndDrawable` packs subclass state into a 4-byte bitfield in the base
  (`mShowing:1 ... mForceNoQuantize:1 ... particle/text flags`), size 0x20,
  `mOrder` at 0x1c; `rb3/src/system/rndobj/Draw.h:122-171`.
- DC3 `RndDrawable` uses a plain `bool mShowing`(0x8), adds `ObjPtrVec<
  RndTransformable> mClipPlanes`(0x24) and a `virtual DrawShadow(...)` vtable
  slot; size 0x28; `dc3-decomp/src/system/rndobj/Draw.h:52-54,126-139`.
- Combined with the Xbox math-type padding (Wii `Transform` = 0x30 with 12-byte
  `Vector3`; DC3 `Transform` = 0x40 with 16-byte padded `Vector3` — derivable
  from Cam: RB3 `mInvWorldXfm`0x90→`mLocalProjectXfm`0xC0 = 0x30 stride vs DC3
  0xc0→0x100 = 0x40, `rb3/.../Cam.h:77-78`, `dc3-decomp/.../Cam.h:98-99`), **every
  transform/drawable-derived class has different offsets and a different vtable**.
  This is the hard blocker against sharing render-object instances between the
  two engines: they cannot be the same C++ object.

### 6. DC3 native GPU layer hard-assumes Xbox 360 encodings (file:line)
The renderer the bridge would target bakes in Xbox conventions:
- `dc3-decomp/native/src/gfx/VertexFormats.h:14` — `GpuVertex` mandates a
  `tangent[4]`; `:43-49` unpackers labeled "Unpack **Xbox 360** compressed
  vertices".
- `dc3-decomp/native/src/platform/MeshGpuCache.cpp:272` — assumes compressed
  meshes "already have tangent data from the original **Xbox** vertex stream".
- `dc3-decomp/native/src/gfx/TextureConvert.h:36-44` — `ByteSwapDXT` ("16-bit
  byte-swap for **Xbox BE DXT** data"), `UntileMilo` ("Untile Milo's custom tiled
  layout (mOrder & 4)"), `SwapBGRAtoRGBA`. These target Xbox 360 BE-DXT + tiling,
  not Wii GX CMPR.
- `dc3-decomp/native/src/platform/RndTex_Native.cpp:1-18` — DC3's own
  `RndBitmap::Load` was in fact derived "Based on RB3 reference:
  rb3/src/system/rndobj/Bitmap.cpp:1018", confirming the load path is portable;
  only the post-load platform decode is Xbox-specific.

### 7. NG subclass family — present in DC3, absent in RB3
- DC3 ships "Next-Gen" (Xbox360/PS3) render subclasses with no RB3 counterpart:
  `Rnd_NG`, `Mat_NG`, `Lit_NG`, `Env_NG`, `PostProc_NG`, `DOFProc_NG`, `Fur_NG`,
  plus `Shader/ShaderMgr/ShaderProgram/MetaMaterial/Ribbon/Shockwave/Spline/
  VelocityBuffer/OcclusionQueryMgr/TexProc` (`ls dc3-decomp/src/system/rndobj`).
  RB3's only platform variant is `DOFProc_Wii` and the fixed-function
  `ShaderOptions`. DC3 is a genuinely more capable render feature set; this is
  the part of the gap that a bridge to DC3's renderer could actually *close*.
- Both `Rnd` singletons expose a parallel virtual interface but with divergent
  vtable shape (DC3 adds pure `Clear()`, `NumDrawPasses/BeginDrawPass/EndDrawPass`,
  `Push/PopClipPlanesInternal`, `ClearDepthForOverlay`, `GetDefaultTexBitmapOrder`,
  and returns `RndTex*`/`RndCam*` where RB3 returns `int`;
  `rb3/.../Rnd.h:88-118` vs `dc3-decomp/.../Rnd.h:95-226`). The concrete renderer
  (`WgpuRnd`) is per-port and is **not** a shared object — each native port owns
  its own `Rnd` subclass, so the bridge does not marshal `Rnd` itself.

### 8. RndCam / RndLight / RndPostProc / RndEnviron — same-shape + divergent-fields
- `RndCam`: same semantics; DC3 adds precomputed `mViewProjMatrix`/
  `mInvViewProjMatrix` (Hmx::Matrix4, `dc3-decomp/.../Cam.h:138-139`) and a
  `float mAspectRatio` where RB3 has `mUnknownFloat` (`rb3/.../Cam.h:105`);
  offsets shifted by ABI. Bridge = field map.
- `RndLight`: same semantics; DC3 adds `mCubeTexture`(RndCubeTex, `dc3-decomp/
  .../Lit.h:82`) for point-light cube projection; RB3 has none. Field map +
  default-null.
- `RndPostProc`: DC3 header carries ~66 offset-annotated members vs RB3's ~56
  (`grep -c '// 0x'`), plus a `PostProc_NG` subclass — DC3 exposes more
  color-correction/bloom params. Divergent-fields.
- `RndEnviron`: DC3 has an `Env_NG` NG subclass; RB3 does not. Divergent.

## Per-class divergence table

| Class | Verdict | Bridge action |
|---|---|---|
| `RndMesh::Vert` (uncompressed) | Structurally different | Adapter: repack 48B→96B/GpuVertex, expand 16-bit weights + Color32, synth tangent |
| Compressed vertex stream | Structurally different | Adapter: **never** use DC3's Xbox unpacker; expand Wii blob → uncompressed verts |
| `RndMat`/`BaseMaterial` | Structurally different | Mapping table + default-fill (normal/spec/rim/cube/fur/meta = null) |
| `RndTex`/`RndBitmap` (data model) | Same-shape | Pass-through fields; enum/mOrder scheme shared |
| Texture texel encoding | Structurally different | Decode Wii GX → RGBA on RB3 side; bypass DC3 ByteSwapDXT/UntileMilo |
| `RndDrawable` base + Transform ABI | Structurally different | No object sharing; per-object marshal only |
| `RndCam` | Same-shape + divergent-fields | Field map (+ recompute viewproj on DC3 side) |
| `RndLight` | Same-shape + divergent-fields | Field map + null cube tex |
| `RndPostProc`/`RndEnviron` | Divergent-fields (+NG subclass) | Field map; NG features default-off |
| `Rnd` singleton | Structurally different vtable | Not marshaled — per-port concrete `WgpuRnd` |

## Implications for the bridge

1. **The bridge cannot be pointer-level pass-through.** The ABI divergence
   (Transform 0x30 vs 0x40, `RndDrawable` bitfield vs bool+clipplanes, shifted
   vtables) means an RB3 `RndMesh*` is not a valid DC3 `RndMesh*`. A bridge that
   "shares the scene graph" is not feasible without recompiling the RB3 render
   classes against DC3's math/base-class ABI — which would fork RB3's matched
   engine. So option-space is really: (a) **per-object translation** into DC3's
   render classes / GPU intermediates, or (b) **port DC3's GPU *passes* onto
   RB3's own object model** (reuse DC3's `gfx/` pass code, keep RB3's RndMesh/
   RndMat, write an RB3-flavored VertexFormats/TextureConvert).
2. **The natural seam is DC3's `GpuVertex`/`GpuVertexSkinned` + a material-param
   struct**, not the `RndMesh`/`RndMat` classes. DC3's `VertexFormats::Unpack*`
   already converts `RndMesh::Vert` → `GpuVertex`; an RB3 variant of that
   unpacker (reading RB3's 48-byte packed Vert, generating tangents via the same
   `mikktspace.c` DC3 already vendors) yields the exact buffer DC3's pipelines
   want. This strongly favors option (b): the reusable, genuinely-ahead DC3 code
   is `native/src/gfx` (BloomPass/DofPass/ShadowPass/PostProcPass/PipelineManager/
   TransparentQueue), which is largely object-model-agnostic once fed GpuVertex +
   a resolved material. Recommend the architect scope the bridge at the GPU-pass
   layer, not the rndobj-class layer.
3. **Wii asset encodings must be handled on the RB3 side and never routed through
   DC3's Xbox decoders.** Concretely: DC3's `UnpackCompressedVertices`,
   `ByteSwapDXT`, and `UntileMilo` are Xbox-360-specific and will corrupt Wii
   `.milo` data. The bridge feeds DC3 only *decoded* geometry (uncompressed
   float verts) and *decoded* textures (linear RGBA) — both of which RB3's native
   engine already produces for its own WGPU path.
4. **DC3's material model is a superset.** Every RB3 material field maps cleanly
   into `BaseMaterial`; the DC3-only fields (normal/specular/rim/environ-cube/
   detail/fur/meta, `mBloomMultiplier`, NG shader variations) simply stay at
   defaults for Wii assets. This means the bridge *gains* headroom (per-pixel
   lighting, bloom, shadows) without needing RB3 assets to author it — the main
   payoff argument for the whole study.
5. **Tangents are the one piece of data Wii assets lack** that DC3's shaders
   want. MikkTSpace generation (already in DC3's `MeshGpuCache`) covers static
   meshes; skinned/dynamic meshes that skip tangent regen (HamRibbon-style) will
   fall back to flat/normal-only shading — acceptable, and RB3's existing path
   has the same limitation.

## Confidence + what I could not verify

- **High confidence** on the structural divergences: all cited from header field
  offsets and the DC3 native `gfx`/`platform` source, read directly.
- **Verified by comment/label, not by running code**: the "Xbox 360" nature of
  DC3's compressed-vertex and texture paths (MeshVertCompress.h struct name,
  VertexFormats/MeshGpuCache/TextureConvert comments). I did not build or execute
  either engine (lane is read-only, no builds), so I could not confirm at runtime
  that a Wii blob actually corrupts through DC3's unpacker — the conclusion is
  from the format definitions.
- **Not investigated (out of R3 scope, belongs to R1/R2)**: whether
  `../milo-native-engine` (RB3's own native engine, the actual consumer) already
  contains a Wii-aware VertexFormats/TextureConvert — I inspected DC3's native
  tree and the RB3 *matched* `src/`, not RB3's native engine repo. The
  `#ifdef HX_NATIVE` hooks in RB3's `Mesh.h:176`/`Tex.h:49` show RB3 already has
  a WebGPU `DrawShowing`/`MakeDrawTarget` path in milo-native-engine; how close
  its GpuVertex is to DC3's `GpuVertex` (and thus how much of DC3's `gfx/` could
  drop in) is the key follow-up the architect needs from R1/R2.
- **Exact Transform/Vector3 sizes** are inferred from Cam member-offset strides,
  not from a direct `sizeof`; the 0x30-vs-0x40 stride is unambiguous in the data
  but I did not open `math/Mtx.h` in both repos to confirm the padding cause.
