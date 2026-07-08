# Lane CROWD — STATUS

**Headline:** Branch **a1-refined (deep skinned-mesh vertex-load gap)** — **HANDED OFF, not
fixed.** The sv3_a shell-vignette crowd walkers ARE loaded, showing, polled, driven, and
positioned onscreen — but every crowd **body mesh loads with 0 vertices**, so they produce no
skinned draw and the center street reads empty. This is a shared-engine RndMesh geometry-load
issue, out of lane scope per A6 (would touch the read path the protected gameplay crowd oracle
depends on). No fix authored.

## Discriminator result (verdict locked before any fix; checkpoint recorded)

Ran a live-tree census of the sv3_a crowd actors by name (new read-only `rb3_crowd_census`
DTA func) at `main_hub_screen` after an 8 s dwell, plus a full-frame drawlog + center-street
ROI query. The four A5 branches resolved cleanly:

| Branch | Meaning | Verdict |
|---|---|---|
| a1 | sub-/shared-milo load failure | **REFUTED at scene level** — sv3_a + streetslomo_clips load; 8 crowd Characters + crowd_chars.grp are in the tree |
| a2 | actors absent from asset | pre-refuted (they're authored) |
| b | loaded but not drawn (showing/draw-gate) | **REFUTED** — `show=1` on all 8 |
| c | loaded but mis-posed/off-screen | **REFUTED** — `onscreen=8`, world spheres placed (r≈46-48, centers spread x≈0..81) |
| d | loaded but never animated | symptom only — `animating=0` because there are no verts to skin |
| **a1-refined** | loaded shell, but **skinned body geometry decodes to 0 verts** | **CONFIRMED (root cause)** |

Census (verbatim, all 8 identical shape):
```
crowdcensus roots=2 crowd_chars=8 showing=8 polled=8 driven=8 animating=0 onscreen=8
[CROWDCENSUS] name=crowd_female01 show=1 poll=3 drv=1 clip=- sph=-1.0,-1.1,43.6,r=46.2 mesh=6/6 show=6 vert=0
```
Per-mesh geom-owner probe (the decisive line):
```
[CROWDMESH] char=crowd_female01 mesh='female_crowd_body01_lod02.mesh' show=1 bones=20 verts=0 geomOwner=self ownerVerts=0
```
`geomOwner=self` + `verts=0` ⇒ a **genuine empty mesh**, not a proxy whose geometry lives in
another mesh. 20 bones are bound (skeleton is fine); the vertex buffer is empty.

Drawlog corroboration (full hub frame, 341 draws, 140 skinned):
- **0** crowd body meshes drawn anywhere full-frame. All 54 distinct skinned meshes are
  band-player outfits (greaserjacket / escapeartist / gloves / bikinistockings / hippyfringe /
  clearcoat / jumpsuit…). The 36 skinned draws that intersect SWEEP's center-street ROI
  `[560,460,260,180]` are the **band players'** rects overlapping that band — NOT the ambient
  crowd. (This nuances SWEEP's "zero skinned draws there": there are skinned draws in the ROI,
  but none are the missing walkers.)

## Why it's a hand-off (not a lane fix)

- The asset is intact: `char/crowd/crowd_female01.milo_xbox` is 424 KB and carries the geometry;
  native simply fails to read the skinned verts. The boot log's `--->Arvin/Diana: Skinned mesh
  needs to be re-exported: female_crowd_body01_lod02.mesh` (`RndMesh.cpp:1118`, fires when
  `gAltRev < 3 && NumBones() > 1`) corroborates a mesh-revision / read-path issue on these
  crowd milos.
- The fix lives in the **engine RndMesh binstream read path**, shared with the gameplay
  **WorldCrowd** oracle. The gameplay crowd renders the SAME crowd bodies but via
  `RndMultiMesh` instancing (`WorldCrowd::CharData::mMMesh`), a *different* geometry path that
  does carry verts — so it works today and does not exercise (or vouch for) the per-Character
  path the vignette uses. Editing the shared vert loader is exactly the A6 no-touch surface.
- Per the charter: deep asset/load gap ⇒ **narrow + report + hand off, don't force a fix.**

## Gates

| Gate | Result |
|---|---|
| E1 vs GT (structural: 3 walkers in center-street band) | Documented gap (retail has them, native draws none). No fix ⇒ no before/after E1; hand-off. |
| drawlog-792 flag-OFF byte-identical | **PASS** — only a read-only DTA func added; hub drawlog deterministic (341/341 across two boots); no rendering flag introduced. |
| batch_objdiff == baseline (touched src/system unit) | **PASS trivially** — no src/system or engine TU touched (only `native/src/rb3_http_handlers.cpp`). |
| gameplay-venue crowd oracle | **UNTOUCHED** — no `src/system/world` or engine edit; oracle rides the multimesh path. |
| RB3_NO_CROWD_REBIND dedupe A/B | Census **identical** OFF vs ON ⇒ crowd-rebind path is irrelevant here (A5 confirmed, not WorldCrowd). |

## Deliverables

- `PLAN.md`, this `STATUS.md`.
- `evidence/`: `crowd_census_verbose.txt`, `crowd_mesh_geomowner.txt` (the vert=0 proof),
  `drawlog_hub_full.json` + `drawlog_no_crowd_bodies.txt` (0 crowd bodies drawn),
  `dedupe_no_crowd_rebind_ab.txt`, `main_hub_native_baseline.png`, `pos_dump.txt`,
  `sv3_load_log.txt`.
- Tooling (reusable by Wave 24): read-only `rb3_crowd_census` DTA func
  (`native/src/rb3_http_handlers.cpp`); recon harness `scripts/native/_w23_crowd_recon.py`.

## Hand-off charter for Wave 24

Fix the skinned-mesh vertex decode for `char/crowd/crowd_*.milo` per-Character body meshes on
native (the `RndMesh` binstream read / mesh-rev handling flagged by `RndMesh.cpp:1118`). A
single engine fix restores the sv3_a vignette walkers and any other per-Character crowd draw
(and likely relates to the count-in thin-geo shard residual). Must A/B the gameplay WorldCrowd
capture (shared read path) before landing. Verify with `{rb3_crowd_census}` → expect `vert>0`
and non-empty `animating`.

---
## ERRATA (Wave-23 close-out review `45f81795`, ERRATA-C1)
The vert=0 census metric reads `mVerts` ONLY; on HX_NATIVE compressed meshes keep `mVerts` empty
BY DESIGN and draw from `mNumCompressedVerts` (rb3_render_mesh.cpp:126,455). No positive control
was run. **"Loads with 0 vertices" is therefore UNCONFIRMED as the root cause** — downgraded to
CANDIDATE. Confirmed facts: 0 crowd body draws (drawlog) + `gAltRev<3` on these meshes (Mesh.cpp:1116).
Wave-24 STEP 0 = re-census with `mNumCompressedVerts` + a band-mesh positive control BEFORE
touching the loader.
