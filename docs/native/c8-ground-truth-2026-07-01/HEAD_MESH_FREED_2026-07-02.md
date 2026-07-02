# "Floating eyes and teeth" — head.mesh CPU geometry freed (2026-07-02)

**Symptom (user report, web):** band characters render with floating eyes and
teeth — no face/head. Reproduced IDENTICALLY on native (band-closeup harness,
guitarist pinned shots): body/arms/clothing/hair/eyes/teeth/tongue render, the
face skin is absent. This is a **second, separate** issue from the flesh-skin
texture composite fixed in `372baf7b` (RESOLUTION.md) — that fix is working
(arms/hands flesh-toned, `head_skin_diffuse_output` composites).

## Root cause — a Wii-only memory optimization frees geometry native still needs

The band face is `head.mesh` (per-member, dir `outfit`). After the face-shape
deform bakes, the char pipeline frees the mesh's CPU-side geometry:

- `BandCharacter::SetDeformation()` (BandCharacter.cpp:2283) creates a
  `CharMeshCacheMgr` with `Disable(!mInCloset)` — **disabled during gameplay**.
- `DeformHead()` syncs `head.mesh` into that manager; each `MeshCacher`
  destructor then runs `mMesh->SetKeepMeshData(false)` →
  `RndMesh::SetKeepMeshData` clears `mVerts`/`mFaces`/`mPatches`
  (Mesh.cpp:1251). Same pattern in `SyncObjects`' `RndMeshDeform` sweep
  (BandCharacter.cpp:1789).

On **Wii** this is pure memory savings — the deformed mesh already lives in GX
display lists. The **native WGPU backend re-reads CPU verts every draw**
(`Rnd_Wgpu_RB3.cpp` DrawMesh: `if (nf <= 0) return;`), so a freed mesh is
submitted every frame and silently skipped → head never draws → rigid
eyes/teeth (own meshes) float in place. Proof: `RB3_HEADMAT_DBG=1` census —
all 4 band `head.mesh` at `nf=0 nv=0` in broken runs; the one run with a
visible head was a character whose deform path skipped (bald prefab), keeping
its geometry.

Why it *looked* tied to `372baf7b`: lineup randomness. A/B runs with
`RB3_SKIN_FIX_OFF=1` also produced headless characters (p7off) once probes
existed. The wave-3 decomp sweep (`4a49b1a4`, BandHeadShaper) was checked:
match-only, behavior-identical.

## Fix (landed with this doc)

`RndMesh::SetKeepMeshData` (src/system/rndobj/Mesh.cpp), `HX_NATIVE`-only:
refuse to free CPU mesh data (early-return on `keep==false`), leaving
`mKeepMeshData` true so flag-gated readers (`OutfitConfig::MeshAO::Apply`,
`BandPatchMesh`) stay consistent with the data actually present. Wii path
byte-identical (`SetKeepMeshData__7RndMeshFb` still 100%). Opt-out
`RB3_MESH_FREE=1` restores the freeing for A/B. Memory cost: a few hundred KB
of kept verts per band member — irrelevant on desktop/web.

**Verified** (band-closeup harness, pinned `coop_g_*` shots): heads render
fully — scalp, ears, nose, beard, glasses sit ON a face — and `head.mesh`
draws with `head_naked.mat` sampling the *painted* `head_skin_diffuse_output`
RT (`hasTex=1`), i.e. the `372baf7b` composite now reaches the face too.
Verified on web release build post-rebuild (all four members have heads,
`/tmp/face-web-fixed`). Shard guard: 0 band drops.

## Tools added

- Engine `RB3_HEADMAT_DBG=1` (milo-native-engine `77eb428`): one-shot-per-mesh
  census of every DrawMesh submission (mesh/dir/mat/diffuse/blend/alphaCut/
  color) + EMPTY-geometry census + head.mesh early-out traces.
- rb3 `RB3_SKINFIX_DBG=1`: logs every MatSwap the skin-fix rebind loop touches.

## Follow-ups (unchanged from RESOLUTION.md)

Dark/flat face shading, glowing eyes, and the `RndTexBlender::DrawShowing`
norm/wrinkle stub are separate open items; faces here are dark-but-present,
matching the Dolphin ground truth's dark rim-lit look more closely now.
