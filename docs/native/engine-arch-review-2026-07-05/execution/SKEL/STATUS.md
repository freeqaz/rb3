# Lane SKEL — S-S2 — STATUS — VERDICT: **BLOCKED** (both named seams degenerate / out of scope)

Flag-first fix stage. The dispatched two-half fix (un-share + gender-pose) at the S-S1-named
offset-bake seam is **provably degenerate**; the only non-degenerate lever (per-member reskin) is
an ENGINE mesh-data change outside this band-side flag-first lane. Per the binding STOP-TRIPWIRE
("if the seam degenerates into any dead cell → STOP, verdict BLOCKED with evidence"), no source
was changed, no flag was registered, and no default/pin was touched.

(The S-S1 mechanism-study STATUS is preserved in git history at commit `8f73b6fa`.)

## TL;DR

S-S1 proved BOTH halves of the 2026-06-06 fix statement are ALREADY realized at runtime:
`Find(name) → own` is **per-member** (un-share half done) **and gender-posed** (gender-pose half
done — male `bone_R-index01` own=109.5°, female own=120.1°). The residual is neither half: it is a
**native-port bind-basis split** — the mesh keeps a SHARED embedded bind skeleton
(`bound = mesh->BoneTransAt`, one instance across all members, male-bind) distinct from the
per-member gender-posed DRAW skeleton (`own`). No `RebindHeadHandsAtRest` offset bake can reconcile
that split, and the per-dominant-bone vertex re-pose (seam B) tears at the multi-bone knuckle
blends where the shard lives. The faithful fix is a per-member **reskin** of the verts+weights onto
`own` — engine `RndMeshDeform` territory, not a band-side offset bake.

## Feasibility gate (STOP-TRIPWIRE, run before any edit)

### Seam A — "un-share the embedded bind `bound` per member + gender-pose it so `boundRest==ownRest`, then bake `off = meshWorld·inv(ownRest)`" → DEGENERATE (provenance-independent)

The GPU palette (engine `Rnd_Wgpu_RB3.cpp:3299-3305`) is `skin[b] = BoneOffsetAt(b) · own->WorldXfm()`.
After the DEFAULT distinct repoint (`SetBone(b, own)`, `:1707`) the palette reads **`own`** and
`BoneOffsetAt` — it **never reads `bound` again**. Therefore:

1. Un-sharing / re-posing `bound` is **invisible to the render** unless the *bake* is switched to
   read `bound`'s rest. Seam A's "un-share bound" half alone = byte-identical to default.
2. If the bake reads a gender-posed `boundRest` **copied from `own`** →
   `off = meshWorld·inv(ownRest)` = **byte-identical to the DEFAULT path** (the shipped shard). No fix.
3. If the bake reads the **shared authored `boundRest`** (129° basis) →
   `off = meshWorld·inv(boundRest)` drawn on `own` = **exactly `RB3_HANDS_SHELL_FIX`** = the measured
   **6th dead cell**, commit `d016ce66` ("W2.8g B-S2: … own-live+bound-rest regresses").

There is no third value for a "gender-posed bound rest": the gender bind is a **runtime `CharClip`**
(`BandCharDesc::GetDeformClip`, `BandCharDesc.cpp:59-64`) whose only static realization is `own`
after `SetDeformation`. There is **no separate gender-posed bind SKELETON asset** to re-pose `bound`
to. So seam A's second half has **no data source distinct from `own`** and collapses onto the
default (no-op) or the shell-fix (measured dead). Degenerate either way — **cannot register a
non-fake flag** for it (a flag byte-identical to default is a fake, and the `boundRest=129°` variant
is `d016ce66` re-run).

### Seam B — "per-vertex shell re-pose `v' = ownRest·inv(boundRest)·v` per dominant bone at load" → GENUINE LEVER but OUT OF SCOPE and cannot close the shard

- It IS a new lever (mutates `vLocal`, outside the exhausted offset-bake class). But the per-bone
  `own`-vs-`bound` gaps have **MIXED SIGNS** and reach ~35° (APD_DIAG, below): a vertex weighted
  across two bones — the **knuckle blend zones, exactly where the R·sin(θ) far-verts smear** —
  cannot be reconciled by any single per-vertex transform. Picking one dominant bone's
  `ownRest·inv(boundRest)` delta **tears** the other bone's blend contribution. This is the same
  "irreducible with any single live bone" barrier the six dead cells hit, extended to per-vertex
  single-bone re-pose.
- It changes the REST SHAPE (moves verts) → risks the rest-free INSTR_B `isoDistort` (non-rigid)
  invariant and gloves/torso regressions.
- It is a **mesh-data / reskin edit** = engine `RndMeshDeform` territory (`SetDeformation`
  `:3111-3115` `RndMeshDeform::Reskin`), **not** a band-side `RebindHeadHandsAtRest` offset bake →
  outside this lane's scope and the flag-first charter.

## The APD_DIAG hard evidence (S-S1 log `/tmp/wave13-skel-s1/gameplay.log`)

`bound` (mesh embedded bind) is SHARED across members; `own` (draw bone) is per-member + gender-posed;
`bakedRest.ang == ownNow.ang` (default is coherent *w.r.t. own*); the per-bone `own`-vs-`bound` gap
is **mixed-sign and up to ~35°** — a genuine per-member skeleton pose difference, not a uniform
gender rotation:

| bone | `bound` (shared) | own p0 (male) | own p1 (female) | gap sign |
|---|---|---|---|---|
| `bone_R-index01`       | 103.0° | 109.5° | 120.1° | own **>** bound (+6 / +17) |
| `bone_L-index01`       | 142.1° | 121.6° | 119.6° | own **<** bound (−22) |
| `bone_R-middlefinger03`| 129.9° | 106.0° | 119.7° | own **<** bound (−10 / −24) |
| `bone_L-middlefinger03`| 112.6° | 147.6° | 127.2° | own **>** bound (+15 / +35) |

`bound` pointer identical across players (e.g. `bone_R-middlefinger03` = `0x…730140` for p0 AND p1);
`own` distinct per player. Mixed signs ⇒ no single rotation reconciles all bones ⇒ seam B tears.

## Root cause (named for the faithful engine fix)

Native-port artifact: on Wii a mesh's bind skeleton and its draw skeleton are ONE object.
On native, `hands_naked` retains a SHARED embedded bind skeleton (`bound`) distinct from the
per-member gender-posed draw skeleton (`own`). The verts+weights encode inter-bone geometry from
`bound`'s (shared, male) bind; `own`'s per-member (gender) skeleton has different relative bone
rest orientations. Even a perfect bind-to-`own` repoint + `inv(ownRest)` bake (= the DEFAULT) leaves
the authored vert→bone relationships referencing `bound`'s geometry, so **multi-bone blends tear**.
**Faithful fix = re-skin the verts+weights against `own`'s skeleton** — precisely what
`RndMeshDeform::Reskin` (`:3111-3115`) does for the deform-set meshes (`unk610`); `hands_naked` is
NOT in that set. This is an ENGINE reskin-pipeline change, not an offset bake.

### New breadcrumb for that engine fix
`male_hands_naked` / `female_hands_naked` are **separate gendered assets**
(`AssetTypes.cpp:250-256`, `ClosetMgr.cpp:455-460`). The female member loads `female_hands_naked`
(female-authored verts) but its bind-bone `bound` still resolves to the SHARED (male-bind) skeleton
instance (identical pointer to the male member's `bound`), while its draw bone `own` is the
per-member female gender-posed skeleton → a **DOUBLE mismatch** (female verts / male-bind `bound` /
female-draw `own`). This is why the shard is "worst for the female." The eventual reskin fix must
rebind female verts to the female per-member skeleton.

## Interaction vs the two default-ON rebinds (unchanged — nothing edited)
`RebindOutfitBonesToOwnSkeleton` (`:1101`, torso-scoped) and `RebindHeadHandsAtRest` (`:1253`, hands
writer) are untouched; the shipped `RB3_NO_SKIN_CLAMP` renderer fling-clamp remains the mitigation.

## Gates
- **Not run** (no fix): the fix gates (wext ≤60 without freeze, Tier-2 joint ≤1u, invOff
  provenance, W2.1 crowd oracle + `RB3_NO_CROWD_REBIND` fail-red + guard-DROP census, skin-golden
  gtests, lineup, drawlog-792) require a landed behavior change. The degeneration proof establishes
  that any implementable seam-A variant is **byte-identical to default** (a no-op — drawlog-792
  trivially unchanged) or reproduces the `d016ce66` regression; there is no non-degenerate flag to
  gate. Before/after band screenshots: there is no "after" (no behavior delta); the current default
  (clamp-on) render is characterized in the S-S1 evidence.
- No source changed; **no new flag registered** (nothing non-degenerate to gate). Refuted flags
  left UNSET (`RB3_HANDS_BIND_FIX`, `RB3_HANDS_POSEAWARE`, `RB3_APPENDAGE_REST_ROT`,
  `RB3_APPENDAGE_ASSET_REBAKE`, `RB3_HANDS_SHELL_FIX`, `RB3_SKEL_REBIND_FULL`;
  `RB3_LOAD_DETERMINISM` stays unset). Six shipped defaults untouched. No pin bump, no default flip.
  Engine `FxSendNative.cpp` audio edit left intact. Process cleanup by PGID only.
- Staged only my own files under `flock /tmp/rb3-git.lock`.

## Consequence for the campaign
The offset-bake fix class for the hands shard is **exhausted** (6 measured dead cells + this proven
degeneration of the 7th "two-half" framing). The lane should be **retired at the band-side offset
seam**. The only remaining faithful lever is an **engine per-member reskin** of `hands_naked`
(and `fingernails_resource`) onto `own`'s skeleton via the `RndMeshDeform::Reskin` pipeline —
a distinct, engine-scoped work item (coordinator to lane it separately if pursued). Until then,
`RB3_NO_SKIN_CLAMP` (default-off = clamp on) remains the shipped mitigation.

## Evidence
- `/tmp/wave13-skel-s1/gameplay.log` (S-S1 APD_DIAG run; regenerable).
- rb3 commit `d016ce66` (measured 6th dead cell).
- Source: `BandCharacter.cpp` `:1253/1481-1554/1656-1752/3111-3115/3915-3937`,
  `BandCharDesc.cpp:59-64`, `AssetTypes.cpp:250-256`; engine `Rnd_Wgpu_RB3.cpp:3299-3305`.
- Checkpoint: `/tmp/wave13-checkpoints/S-S2.json`.
