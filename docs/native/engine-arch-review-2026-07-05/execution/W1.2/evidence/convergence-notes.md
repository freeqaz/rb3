# W1.2.S4 — Convergence analysis: RB3MeshCache vs shared `gfx`/`platform` copies

Doc-only. Satisfies the brief's "diff to prove identity, else record the delta"
requirement. **Overall verdict: NOT provably identical on either axis — keep RB3's
version verbatim; convergence is a later CHANGE commit, out of W1.2 (MOVE-only) scope.**

Sources compared (engine repo `milo-native-engine`, at W1.2 HEAD `6f9d340`):
- RB3 (this item): `src/platform/RB3MeshCache.{h,cpp}` — the just-extracted TU.
- Convergence target A (unpack): `src/gfx/VertexFormats.{h,cpp}` — the shared
  `VertexFormats::Unpack*` family (385 lines, rndobj-coupled TU RB3 excludes).
- Convergence target B (entry/cache): `src/platform/MeshGpuCache.{h,cpp}` — the DC3
  backend's `GpuMeshData` + `EnsureMeshUploaded` (365 lines, `MILO_ENGINE_GPU_BACKEND=dc3`).

The two source sets are **mutually exclusive at configure time** (`RB3MeshCache.cpp` in
`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`, `MeshGpuCache.cpp`/`VertexFormats.cpp` in the DC3
set), so there is no ODR clash today and convergence is an *optional* future unification,
not a correctness requirement.

---

## Axis 1 — `RB3UnpackMeshVerts` vs `VertexFormats::Unpack{Static,Skinned,Compressed,CompressedSkinned}Vertices`

**Verdict: NOT identical.** Divergence is deeper than the PLAN's stated expectation —
there are FIVE independent divergence points, at least one of which (compressed vertex
colour channel order) is a *behavioral* difference that would change pixels if either side
were swapped for the other. Concrete points:

1. **Signature / allocation model.** RB3 is ONE function: `RB3UnpackMeshVerts(owner, bool
   skinned, vector<GpuVertex>&, vector<GpuVertexSkinned>&)` — it `resize()`s the out vector
   itself and dispatches static/skinned + uncompressed/compressed internally, returning the
   count (or −1 for no geometry). VertexFormats is FOUR functions with a
   `T* out, int maxVerts` contract (`std::min(NumVerts(), maxVerts)`) — the CALLER
   pre-allocates and clamps. Merging requires picking one allocation contract.

2. **Compressed-record decode strategy (structural).** RB3 reinterpret-casts the blob to
   `struct XboxCVert { int pos[3]; int color; int uv; int norm; int tan; int b0; int b1; }`
   (36 B, `static_assert`) and passes each `int` field through `Be*` helpers that
   `__builtin_bswap32` then `memcpy` to the target type. VertexFormats deliberately AVOIDS
   the type-pun: it reads each field by BYTE OFFSET (`LoadBE32(rec + kCV_*)`, an
   `enum CompressedVertexOffset`) into a host-endian word, with an explicit code comment
   that the old struct-cast "silently truncated the FLOAT position members to int, zeroing
   every compressed position" for its `CompressedVertex_Xbox` (whose pos members are
   `float`). RB3's `XboxCVert` sidesteps that by declaring `int pos[3]` so no truncation
   occurs — but the two reach the same floats via structurally different, non-substitutable
   code (one is portable-by-byte, the other is cast+bswap).

3. **Compressed vertex COLOUR channel order — REAL behavioral divergence.**
   RB3 `BeColor` (after bswap32): `R=(v>>16)`, `G=(v>>8)`, `B=(v>>0)`, `A=(v>>24)`
   — i.e. treats the word as `0xAARRGGBB`.
   VertexFormats `UnpackColor_BE`: `R=(v>>0)`, `G=(v>>8)`, `B=(v>>16)`, `A=(v>>24)`
   — treats it as `0xAABBGGRR`. **R and B are SWAPPED between the two.** RB3's own code
   comment records that this mapping was deliberately changed (prior code read R from the
   low byte, matching VertexFormats) and that the vertex-colour application is now gated on
   `RndMat::mPreLit` in the shader. So the two are NOT interchangeable: swapping in
   VertexFormats' unpacker would re-introduce the R↔B swap on every prelit compressed mesh.
   *Whichever is "more correct" is a CHANGE decision, not a MOVE.*

4. **Uncompressed `RndMesh::Vert` member-access model.** RB3 reads `owner->mVerts` directly
   and uses `v.color.fr()/fg()/fb()/fa()` (Color32-packed accessors), `v.uv`,
   `v.boneWeights.GetX()`, `v.boneIndices[]`. VertexFormats reads via the `owner->Verts(i)`
   accessor and uses `v.color.red/green/blue/alpha` (float members), `v.tex.x/y`,
   `v.boneWeights.x`. These imply the two backends compile against DIFFERENT effective
   `RndMesh::Vert` shapes (RB3: packed Color32 `color` + `uv`; DC3: float-channel `color` +
   `tex`). A shared unpacker would first need a single agreed Vert accessor set.

5. **Tangent handling.** For UNCOMPRESSED verts both force `tangent=(1,0,0,1)`, BUT
   VertexFormats then runs **MikkTSpace** tangent generation in the caller
   (`EnsureMeshUploaded` → `ComputeMikkTangents`, first-upload only) to replace that
   placeholder with real tangents; RB3 leaves the hardcoded `(1,0,0,1)` (no MikkTSpace on
   the RB3 path at all). For COMPRESSED verts, VertexFormats extracts the bitangent SIGN
   from the DEC4N w-bit (`UnpackDEC4N_Sign_BE` → `tangent[3] = ±1`); RB3 hardcodes
   `tangent[3]=1.0f` and only unpacks the xyz. So compressed-mesh tangent HANDEDNESS
   diverges (RB3 always +1; VertexFormats data-driven).

Bone weight/index bit math (`UDEC4N`/`UBYTE4`) and the UV `FLOAT16_2`/half-float decode ARE
equivalent between the two — but they are the only pieces that are, and they cannot be
shared in isolation. **Kept verbatim.**

## Axis 2 — `RB3MeshEntry`/`sMeshGpu` vs `GpuMeshData`/`sMeshGpuData` (`MeshGpuCache`)

**Verdict: NOT identical.** RB3's entry is a strict superset built for the browser-WebGPU
backpressure/leak problem; DC3's is a plain VB/IB record plus viewer/text metadata. The two
also use different invalidation *models*, so they are not a field-rename apart. Concrete
divergence:

| Concern | `RB3MeshEntry` / `sMeshGpu` | `GpuMeshData` / `sMeshGpuData` |
|---|---|---|
| Buffers + core | `vbuf`,`ibuf`,`indexCount`,`skinned`,`uploaded` | `vertexBuffer`,`indexBuffer`,`numIndices`,`numVertices`,`skinned`,`uploaded` |
| Invalidation | **Fingerprint** (`ownerKey`,`fpVerts`,`fpFaces`,`fpSkinned`) **+ owner-gen** (`fpOwnerGen` vs `sGeomSyncGen[owner]`) — auto-detects position-only owner rewrites (sustain-tail) | Explicit `InvalidateGpuMesh()` call (drops `uploaded` on `SetGeomOwner` swap); no count/position fingerprint |
| Second map | `sGeomSyncGen` (per-owner geom generation), bumped in `OnSync` | none — single side table |
| L1 vertex cache | `cachedSkinnedVerts` (bind-pose skinned verts, re-read every frame by the V24 shard guard) | none |
| Per-instance uniforms | `std::vector<UniformSlot>` free-list (`objUB/BG`,`boneUB/BG`,`matUB/BG` + `matKey`/`matDiffuseView`/`matEmissiveView` material-BG invalidation) + `frameSeen`/`nextSlot` lazy per-frame reset | none (DC3 builds bind groups elsewhere; no per-instance slot cache in the entry) |
| Viewer/text metadata | none in the entry | `depthBias` (viewer), `debugLabel` (`std::string`, text meshes) |
| API surface | `CleanupGpuMesh`, `LookupGeomSyncGen`, buffer/BG create counters | `Ensure/Cleanup/Invalidate/ClearMeshGpuCache`, `SetMeshDebugLabel/DepthBias`, `MeshLabel`, `GetMeshGpuData`, frame-stats |

The dominant difference is the **`UniformSlot` per-instance free-list** — RB3's fix for the
same `RndMesh` drawn N times per frame with different object/material/bone state under
browser submit-queue backpressure (see the extensive RB3MeshEntry comment). DC3 has no
analog. Plus the L1 skinned bind-vert cache and the `sGeomSyncGen` owner-generation
invalidation are RB3-only. `RndMesh::OnSync` also differs: DC3's only clears `uploaded`;
RB3's ALSO bumps `++sGeomSyncGen[this]`. **Kept verbatim.**

---

## Deferred convergence path (OUT of W1.2 scope — a later CHANGE wave)

Lane doc `01-renderer-core.md` §5a names the eventual target: a common `MeshUploadCache`
interface both backends implement, wrapping the VB/IB upload + fingerprint. This is a
**CHANGE**, not achievable as a MOVE, because it must first reconcile the hard divergences
above — none of which W1.2 may touch (the byte-identical gate would fail, and it is exactly
the regression class the memory notes warn against):

1. **One vertex-unpack.** Pick a single allocation contract (`vector&` vs `out[]+max`) and a
   single `RndMesh::Vert` accessor model, and RESOLVE the compressed-colour R↔B channel
   order (Axis 1 pt 3 — one side has to change pixels; requires the visual/lineup gate) and
   the tangent policy (MikkTSpace + DEC4N bitangent sign vs RB3's hardcoded `(1,0,0,1)`).
2. **One entry with backend extensions.** A shared VB/IB + fingerprint core, with the
   RB3-only `UniformSlot` free-list, L1 skinned cache, and `sGeomSyncGen` owner-generation
   invalidation kept as an RB3 specialization (they exist for a browser-WebGPU constraint
   DC3's native Dawn path does not have), and DC3's `depthBias`/`debugLabel` as a DC3
   specialization.
3. **Gate it under the draw-log golden** (once W0.3b lands its frozen sim clock) plus the
   lineup gate, since it changes generated output by construction.

A related in-item follow-up already recorded in S3: routing `DrawMesh`'s inline upload block
through `RB3EnsureMeshGpu` (dedupe within RB3) — also a CHANGE, also deferred to the same
gated wave.

**Bottom line:** both axes are measurably divergent; W1.2 correctly relocated RB3's version
verbatim and did NOT converge. Convergence remains a future gated CHANGE.
