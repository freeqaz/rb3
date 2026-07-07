# Wave 14 — Pre-dispatch Review (Fable adversarial pass)

**Reviewer:** Fable subagent (2026-07-07). **Input:** `WAVE14_KICKOFF.md` (draft), README Wave-13
results table + Wave-14 menu, `WAVE13_KICKOFF.md` (acceptance) / `WAVE13_REVIEW.md`, STATUS docs
(`SKEL/`, `UIGRADE/` G-S1+G-S2+G-TRIGGER, `W4.3-C2b4/`), plus direct source verification:
rb3 `src/system/rndobj/MeshDeform.{h,cpp}`, `src/system/rndobj/Mesh.h`,
`src/system/rndobj/Trans.{h,cpp}`, `src/system/rndobj/Group.cpp`, `src/system/rndobj/Rnd.h`,
`src/system/bandobj/BandCharacter.cpp`, `src/system/char/CharMeshCacheMgr.h`,
`src/system/ui/PanelDir.cpp`; engine @ pin `3b5af48`: `src/platform/Rnd_Wgpu_RB3.{h,cpp}`,
`src/platform/RB3PostProc.{h,cpp}`, `src/platform/NativeCompatFlags.classification.json`.

## VERDICT: **dispatch-with-amendments**

The three-lane structure is right and every lane is aimed at a real, named residual. The wave's
false premise is in the headline lane, as it has been three waves running: **Lane R's load-bearing
sentence — "per-member reskin of verts+weights onto `own` via the existing
`RndMeshDeform::Reskin` pipeline" — is wrong in three specific ways** (Reskin never touches
weights; there is no "pipeline" for `hands_naked` to be put through — the deform inputs must be
SYNTHESIZED because the authored asset doesn't exist; and `RndMeshDeform` is an rb3 Wii-matching
TU, not engine code). None of these kill the lane — the direction survives, and source
verification actually strengthens it in three places the kickoff doesn't know about (the authored
bind basis exists as per-mesh numeric data; the ideal invocation point already exists at
`SetDeformation`; GPU propagation is free). But R1 dispatched on the kickoff's wording would spend
its budget looking for a membership switch that doesn't exist. Lane U's mechanism claim checks
out against source, with a grant-phrasing correction (the flush lives in `RB3PostProc.cpp`, and
the minimal seam is a one-line header visibility change — no `Rnd_Wgpu_RB3.cpp` edit, and
definitely no new base-`Rnd` virtual, which would need ungranted rb3 files). Lane A's R-C
hypothesis is refuted by the Trans source: dirty propagation to trans-children works; the real
distinction the lane needs is group-membership ≠ trans-parenting.

---

## Amendments

### A1 (CRITICAL, Lane R) — `RndMeshDeform::Reskin` does not do what the kickoff says; correct the charter before dispatch

Verified semantics (rb3 `src/system/rndobj/MeshDeform.cpp:298-399` — **rb3, not engine**):

- Reskin is a CPU **linear-blend re-pose of the mesh's current vertex positions and normals, in
  place**. Per bone it builds `xfms[i] = mBones[i].offset · ExportWorldXfm(bone_i)`
  (`:308-317`); per vertex it blends those transforms with the vert's weights, normalizes by
  total weight, optionally premultiplies `mMeshInverse` (`:329-364`), then overwrites
  `v.pos` and rotates/renormalizes `v.norm` (`:366-395`).
- **It never writes weights.** The weights it consumes are the RndMeshDeform's OWN authored,
  8-bit-quantized table (`mVerts`, `VertArray` — `:320-341`), not the mesh's skin weights
  (`RndMesh::Vert::boneWeights`/`boneIndices`, `Mesh.h:78-86`), and nothing in the function
  mutates either set. The kickoff's "reskin of verts+weights onto `own`" is half false; so is
  the review-question framing "re-derives per-vertex positions/weights in the NEW basis" — it
  TRANSFORMS old positions; it derives nothing from scratch.
- **There is no pipeline for `hands_naked` to be "put through."** `unk610` is populated only when
  a loaded object's class is `RndMeshDeform` (`BandCharacter.cpp:2974-2978`); `hands_naked` is
  absent because **no authored RndMeshDeform asset exists for it**, not because of a membership
  flag. R2 must SYNTHESIZE the deform inputs — bone list, per-bone offsets, per-vert weights —
  from the mesh's own CPU-resident skin data. Precedent exists: `BandPatchMesh.cpp:1408-1420`
  creates `Hmx::Object::New<RndMeshDeform>()` at runtime and copies weights in.

**Why this is still the right primitive (the mixed-sign question, answered precisely):** Seam B
died because it applied ONE transform per vertex (dominant bone) and the per-bone `own`-vs-`bound`
deltas are mixed-sign up to ~35° — one bone's delta tears the other's blend contribution. Reskin
does not have that wall: it applies a **per-vertex weighted blend of the per-bone transforms**
(the same blend model the GPU palette uses), so a knuckle vertex weighted across two bones gets a
smooth interpolation of both deltas. With per-bone transforms set to the bind-migration delta
(authored-bind offset composed with `own`'s gender-rest world) and the existing default rebake
(`off = meshWorld·inv(rest_own)`) left in place, the palette and the geometry then reference the
SAME basis: at `own`-rest every skin matrix collapses to `meshWorld`, so the re-posed verts render
exactly as re-posed, and animation deltas act on a basis-consistent shape. What remains is NOT
guaranteed-zero smear — it is ordinary LBS interpolation error at the blend zones (the same class
as candy-wrapper) — so the gate must be the pre-registered **quantitative** target (wext 95-106u →
≤60u + Instrument-B invariants), not a binary "reconciled". Amend R1's deliverable to state the
primitive as: *"synthesized per-vertex weighted-blend re-pose from the authored bind to `own`'s
gender rest, plus the existing offset rebake"* — and to evaluate whether to reuse
`RndMeshDeform::Reskin` at all versus doing the blend directly in BandCharacter native code with
the mesh's own 16-bit weights (routing through `VertArray::AppendWeights` requantizes weights to
8-bit, `MeshDeform.cpp:124-135` — a pointless fidelity loss when the mesh weights are already
CPU-resident).

### A2 (HIGH, Lane R) — the reskin's source data is DESTROYED by the default-ON rebind; sequencing is load-bearing

The only numeric record of the authored bind basis is the per-mesh inverse-bind set
`RndBone::mOffset` (`Mesh.h:33-47`, loaded from the mesh file: `bs >> bone.mBone >> bone.mOffset`
`:49-52`; authored bind world = `inv(mOffset_b)·meshWorld`). The default-ON
`RebindHeadHandsAtRest` Poll-time bake **overwrites** those offsets with
`off = meshWorld·inv(rest_own)` (`BandCharacter.cpp:1700-1725` region, two-pass apply). A reskin
that runs after the first Poll reads rebaked offsets and silently degenerates (delta ≈ the
identity family = no-op, or the 6th-dead-cell values). **Require R1 to pin the invocation point
BEFORE the first Poll-time bake and to name where the authored offsets are captured.** The natural
point already exists: `SetDeformation()` (`BandCharacter.cpp:3064-3145`) is where the `unk610`
deforms already Reskin, the skeleton is posed at the one deterministic weighted gender-bind rest
(`:3075-3110`), and the native rest-capture + rebind re-arm run immediately AFTER it
(`:2420-2455`, `NativeCaptureRestPoseAfterDeform`). Idempotence: the existing deforms use the
`mDeformed` latch (`MeshDeform.cpp:300-302`) because outside the closet the `CharMeshCacheMgr` is
disabled (`mgr->Disable(!mInCloset)`, `:3089`; `MeshCacher` restores originals only when enabled,
`CharMeshCacheMgr.h:10-17`) — a synthesized hands reskin needs the same once-per-mesh latch or a
`SyncObjects` re-run (mid-song merge) will compound the transform.

### A3 (HIGH, Lane R + cross-lane R-D) — the lane is rb3-side, not engine-side; that inverts the collision matrix

- `RndMeshDeform` lives in rb3 `src/system/rndobj/MeshDeform.cpp` — a **Wii-matching shared TU**.
  `BandCharacter.cpp` likewise. The kickoff's lane header ("engine `RndMeshDeform`/mesh pipeline")
  and its R-D framing ("Lane R engine mesh/deform TUs") are both mislocated.
- **The fix likely needs ZERO engine TUs.** CPU verts are resident and consumed by the native
  renderer every frame (the V24 shard guard re-reads bind-pose verts, engine
  `Rnd_Wgpu_RB3.cpp:2694-2711`); the GPU vertex buffer is created lazily at first draw
  (`:3013`) and invalidated by vert fingerprints + the `RndMesh::OnSync` `uploaded=false` dirty
  signal (`:2707-2710`) — and Reskin's own `cb->SyncMesh` path fires `RndMesh::Sync`
  (`CharMeshCacheMgr.h:20`). A load-time reskin lands before the first upload anyway.
- Consequences: (i) rule for R2 — **do not edit `MeshDeform.cpp`** (match-sensitive TU; do the
  synthesized blend in BandCharacter HX_NATIVE code). If an edit there proves unavoidable, the
  gate is objdiff match% unchanged on the TU, on top of Wii byte-identity. (ii) The Lane R / Lane
  U file overlap is EMPTY on the expected path (R: rb3 `BandCharacter.cpp` [+classjson append
  under flock]; U: engine `Rnd_Wgpu_RB3.h` + `RB3PostProc.*` + rb3 `PanelDir.cpp`). No sequencing
  needed. (iii) If R2 discovers a genuine engine need (e.g. the skinned-vert unpack cache
  misbehaving after mutation), that is a coordinator STOP + sign-off, and any `Rnd_Wgpu_RB3.*`
  edit sequences AFTER Lane U per single-writer-per-TU law — pre-register this in the lane prompt
  rather than discovering it mid-wave.

### A4 (MEDIUM, Lane R) — the female question is answerable from data R1 must dump, and the "double mismatch" is currently a pointer-level claim only

S2's female evidence (`SKEL/STATUS.md` breadcrumb) is that `bound` POINTERS are shared — a
name-resolution fact. Whether a **female authored bind exists as data** is a separate, numeric
question the kickoff leaves rhetorical: the gendered mesh files (`male_hands_naked` /
`female_hands_naked`, `AssetTypes.cpp:250-256`) each carry their OWN `RndBone::mOffset` array. R1
must dump the hands-bone `mOffset` values for both genders **pre-Poll** (before the A2 rebake
destroys them) and compare:
- If they DIFFER → the female authored bind exists per-mesh; the reskin source is solid and the
  "female verts → male bind" double-mismatch framing is confirmed at the data level.
- If they are IDENTICAL → the female mesh was authored against the male bind; the double-mismatch
  framing collapses into the single bind-basis split, the reskin source is the one shared authored
  bind, and the target basis is still the member's own gender rest — the fix statement simplifies
  rather than dies.
Either way the numbers go in the R1 STATUS; do not let R2 inherit the question.

### A5 (MEDIUM, Lane U) — grant phrasing is mislocated; the minimal seam is smaller than the kickoff grants, and one shape must be explicitly forbidden

- `FlushPostProcMidFrame` is NOT in `Rnd_Wgpu_RB3.cpp`: it was moved to **`RB3PostProc.cpp:44`**
  (file-header note `:10-13`), which the kickoff already grants. `ClearDepthForOverlay` is at
  `Rnd_Wgpu_RB3.cpp:2307-2354` on today's pin. The grant as written ("the
  FlushPostProcMidFrame/ClearDepthForOverlay region of Rnd_Wgpu_RB3.cpp") should be re-stated:
  **the expected edit set is `Rnd_Wgpu_RB3.h:257` (make the private `FlushPostProcMidFrame`
  public, or add a public flush-only wrapper next to it) + `RB3PostProc.{h,cpp}` (a free-function
  shim, e.g. `RB3MenuUIFlushOnly()`) + `PanelDir.cpp` (swap the call)** — `Rnd_Wgpu_RB3.cpp`
  likely needs NO edit at all (keep it granted as contingency only; it is the same TU A9/Wave-13
  law cares about).
- **Forbid the base-`Rnd`-virtual shape.** PanelDir currently reaches the engine two ways: the
  base virtual `Rnd::ClearDepthForOverlay()` (rb3 `rndobj/Rnd.h:124`) and plain `extern`
  declarations (`PanelDir.cpp:24-25` — `RB3UIPostGradeActive`/`RB3SetMenuUIFlushPending`, no
  header include). A new base virtual would require editing rb3 `src/system/rndobj/Rnd.h` — a
  Wii-matching header OUTSIDE the grant. The extern-free-function pattern is already established
  in this exact file; use it.
- Keep the **gameplay gate** on the new trigger unchanged (`PanelDir.cpp:168-172`): a flush-only
  entry still MOVES the gameplay venue-flush point if an overshell PanelDir draws before
  `TrackPanel::Draw` — the gate is what preserved gameplay pixel-invariance in G-TRIGGER
  (flush counts OFF 395 / ON 402, all TrackPanel).

### A6 (MEDIUM, Lane U) — the red-band mechanism claim is CONSISTENT with source; state what is proven vs pre-registered

Verified: the wired trigger fires on **every** menu `PanelDir::DrawShowing` (`PanelDir.cpp:170-172`).
The FIRST call per frame (venue pending) takes the flush path; every SUBSEQUENT call falls into
`ClearDepthForOverlay`'s else-branch, which suspends and re-opens the current pass with
`depthLoadOp = Clear` + `stencilLoadOp = Clear` (+ bind-group rebind and `mLastSceneCam` reset,
`Rnd_Wgpu_RB3.cpp:2326-2354`) — i.e. flag-ON inserts **inter-UI-dir depth/stencil clears that
flag-OFF never has**, on exactly the screen (song_select) that layers a 3D char preview + album
art between UI dirs. That is a mechanism fully consistent with the E1 red band and the
1.110→1.049 metric drop. A flush-only entry provably removes that delta: after the first flush,
subsequent calls no-op entirely (`mPostProcFlushed` early-return, `RB3PostProc.cpp:56`), and the
latch cannot dangle because `a5cf8d3` consumes it unconditionally at the TOP (`:55` region,
verified in source) — set-then-no-op-call pairs are safe, and gameplay never sets it. What is NOT
yet proven is that the visible band is ONLY the depth clear (venueGrade=false grading of
song_select's venue is also new vs flag-OFF, by design); the kickoff's pre-registered gates cover
this correctly — keep both: (i) SETLISTS-row ROI pixel-compare flag-ON vs flag-OFF (band GONE),
(ii) contrast back inside the PP_OFF-parity band [1.06,1.17] (expected ≈1.11). Re-run hub ≥2.0
(2.204 baseline) and the A5 backdrop-chroma ROIs on the SAME binary.

### A7 (MEDIUM, Lane A) — R-C is answered by source: dirty propagation WORKS; the real distinction is draw-membership vs trans-parenting; start from the C2b4 STATUS, not against it

- `DirtyLocalXfm()` → `SetDirty()` (`Trans.h:150-152`, `:98-102`) → `DirtyCache::SetDirty_Force`
  recurses over children caches (`Trans.cpp:99-107`); world xfms recompute lazily via
  `WorldXfm_Force` = `Multiply(mLocalXfm, mParent->WorldXfm(), mWorldXfm)` (`Trans.cpp:127-140`).
  Even constrained children follow the parent's motion (`kParentWorld` copies parent world;
  `kLocalRotate` composes position). **There is no "world-authored child that ignores a parent
  nudge" mode in Trans.** A node fails to follow only if (a) it is a GROUP MEMBER but not a
  TRANS-child — `RndGroup::mObjects` is a draw list (`Group.cpp:21,38`), and group membership
  does NOT imply trans-parenting — (b) it has a `kTargetWorld` dynamic constraint to another
  target (`Trans.cpp:221-228` region), or (c) code rewrites its xfm per frame.
- The kickoff's R-C hypothesis ("the nudge dirties only the group xfm; children with
  world-authored xfms wouldn't follow") is therefore NOT a real mechanism, and the lane's own
  STATUS already observed the opposite empirically: `album_frame01.mesh` moved with the group
  "as one rigid unit," and the non-mover is "a separate, unrelated always-present background
  decoration" (`W4.3-C2b4/STATUS.md`, C2b diagnosis). The kickoff's Lane A premise sentence
  ("reveals a grey ornate bezel/frame element that does not move with the group") is the
  coordinator's E1 observation of that SEPARATE element — reconcile the two in the lane prompt so
  the agent doesn't re-litigate propagation.
- Amend Lane A's first action to: draw-log the art-rect region flag-ON, then for each candidate
  quad dump its **TransParent chain** (`TransParent()`, `Trans.h:75`) and constraint. The
  whole-assembly fix = nudge the trans-ROOTS of every element that must move (or re-parent them
  under one group) — and carry the STATUS's projection note (this panel's camera is off-axis, so
  a local-Z nudge projects diagonally; the new left-column overlap may need an X/Y-aware offset,
  not just a smaller Z). Settle-frames methodology (frame-count, not wall-clock) carried as a gate.

### A8 (LOW, Lane R gates) — add the ungameable mechanism gates; the listed set is achievable and fail-red but symptom-heavy

The pre-registered set (Instrument-B ~0, Tier-2 ≤1u, wext ≤60u without freezing, guard-DROP census,
crowd oracle both arms + `RB3_NO_CROWD_REBIND` fail-red, gtests, lineup, flag-OFF drawlog 792,
gloves/torso control, both-gender E1) is achievable and can fail red — keep all of it (the crowd
gates are cheap insurance even though Wave-13 evaporated the crowd risk). Add three mechanism-level
checks that cannot be satisfied by a clamp or a freeze: (i) post-reskin per-vertex delta histogram
is NONZERO and **differs male vs female** (proves gender-distinct source data was actually
consumed); (ii) authored-offset provenance dump pre/post (proves A2's ordering held — the reskin
read authored `mOffset`, and the final baked offsets equal `meshWorld·inv(rest_own)` per member);
(iii) NO edits to `MeshDeform.cpp` or any Wii-matching TU without an objdiff match%-unchanged
proof (per A3). Note the wext gate's freeze-detector rider from Wave-13 carries (distinct wext
values ≤3 = freeze = FAIL even if ≤60u).

### A9 (LOW, process) — the "seven defaults ON" tally is VERIFIED correct

Placement (`RB3_PLACEMENT_CONTRACT`, README:249), black head (W2.7, default-ON at landing,
README:262), hands rest-capture (`RebindHeadHandsAtRest`, README:131), text floor (engine
`a94762f`, README:315), hub quad (rb3 `cda3b326`, README:316), chroma-preserve (FIX-H2, engine
`a320f9d`, README:343), hub ticker (`RB3_HUB_TICKER_YFIX`, engine classjson `ebe90f4`,
README:488/491). Seven. Refuted-flags-UNSET list consistent with `SKEL/STATUS.md` (incl.
`RB3_HANDS_SHELL_FIX`). No amendment — recorded so the tally has a verified anchor.

---

## Lane-by-lane assessment

**Lane R — right direction, wrong mechanism description; dispatch only with the corrected
charter (A1-A4).** The Wave-13 S1/S2 evidence chain (bind-basis split, shared `bound` pointers,
mixed-sign per-bone gaps, offset-bake class exhausted) is internally consistent with a
weighted-blend re-pose fixing the smear — S2's own Seam-B analysis even anticipates it ("the
faithful fix is a per-member reskin of the verts+weights onto own"), and the source verification
strengthens feasibility beyond what the kickoff claims: the authored bind exists as per-mesh
numeric data (`RndBone::mOffset`), the invocation point exists at the gender-rest-posed
`SetDeformation`, CPU verts are resident and mutable with automatic GPU re-upload, and the cost is
a one-time (latched) CPU pass over a few thousand verts at load. But the kickoff's literal
instruction — put `hands_naked` through the existing Reskin pipeline — is unexecutable as written
(no authored deform asset; Reskin doesn't touch weights; "V24 compressed verts" conflates the
engine's shard-guard version tag with vert storage — the only quantized vert fields are the 16-bit
weights, positions are plain floats, no decode/re-encode round-trip exists). The two-agent shape
(R1 diagnosis-only, R2 flag-first) is right; Opus ×2 justified; STOP-TRIPWIRE (no offset-bake
variants, no clamp-faking) carried and correct.

**Lane U — mechanism verified, smallest lane, dispatch with the A5 seam shape.** The
depth-clear-side-effect theory is consistent with source (A6), the flush-only fix provably removes
the flag-ON-only inter-dir clears, the latch hardening composes safely, and the gates are already
pre-registered with baselines. The one real trap is grabbing more surface than needed: no base-Rnd
virtual, no `Rnd_Wgpu_RB3.cpp` edit on the expected path, keep the gameplay gate. Coordinator
flips `RB3_UI_POST_GRADE` only after the song_select E1 re-capture passes both pre-registered
checks.

**Lane A — dispatchable once R-C is settled per A7.** The C2b −120u calibration and the
settle-frames methodology are solid inherited assets. The lane must reconcile the
kickoff-vs-STATUS wording (the revealed element is a SEPARATE node, not a propagation failure)
and ground the assembly in TransParent-chain evidence before re-nudging. Sonnet is the right
tier; game-side only, no collisions.

**Cross-lane:** with A3, the R/U file sets are disjoint (R: rb3 `BandCharacter.cpp`; U: engine
`Rnd_Wgpu_RB3.h` + `RB3PostProc.*` + rb3 `PanelDir.cpp`; A: rb3 `SongSelectPanel.cpp`). The only
shared engine artifact is `NativeCompatFlags.classification.json` — append-only under the
established flock, as carried. Any Lane-R engine escalation is a coordinator STOP and sequences
after Lane U (single-writer-per-TU).

---

## Direct answers to the kickoff's risk questions

**R-A (is Reskin the right primitive?):** It is the right primitive FAMILY — per-vertex
weighted-blend re-pose — but not as described. Verified semantics: it transforms current positions
and rotates normals in place via a per-vertex weighted blend of `offset·boneWorld` transforms
(`MeshDeform.cpp:298-399`); it re-maps nothing, re-derives nothing, and never writes weights. For
`hands_naked` the deform inputs must be synthesized (no authored RndMeshDeform exists — that is
WHY it's absent from `unk610`); the cleanest implementation does the same blend directly with the
mesh's own 16-bit skin weights in BandCharacter native code (avoids the 8-bit `AppendWeights`
requantization and avoids touching the matching TU). Vert data: CPU-resident (`RndMesh::Vert`,
float pos/norm) and mutable at load; no compressed round-trip — "V24" is the engine shard-guard
version tag, not a vertex format; GPU propagation is automatic (fingerprint/OnSync invalidation,
lazy first-draw upload). Female source bind: exists (if anywhere) as the female mesh's own
`RndBone::mOffset` array — a data question R1 must answer with a pre-Poll dump (A4), because the
default-ON rebake destroys those offsets after first Poll (A2). And the reason reskin does not hit
Seam B's mixed-sign wall: Seam B applied ONE bone's delta per vertex; the blend applies ALL
weighted bones' deltas per vertex — the same interpolation model the GPU palette uses, so the
geometry and palette become basis-consistent at `own`-rest by construction. The residual is
ordinary LBS blend error, which is what the quantitative wext/Instrument-B gates are for.

**R-B (is flush-only side-effect-free on song_select?):** The red band's proposed cause is
consistent with source: flag-ON inserts per-UI-dir depth/stencil clears via the else-branch
(`Rnd_Wgpu_RB3.cpp:2326-2354`) that flag-OFF never executes, on the one screen that layers 3D
content between UI dirs. A flush-only entry removes exactly that delta (subsequent calls no-op on
`mPostProcFlushed`), and the `a5cf8d3` consume-at-top makes repeated latch-set/no-op-call pairs
safe with no dangle into gameplay. Not yet proven: that the band isn't partly the (intended)
venueGrade=false grade itself — which is why the E1 ROI pixel-compare + parity-band re-measure
stay the flip criteria. The grant should be re-shaped per A5 (header one-liner + RB3PostProc shim
+ PanelDir swap; no base-Rnd virtual; `Rnd_Wgpu_RB3.cpp` contingency-only).

**R-C (does the group nudge move `album_frame01.mesh`?):** Yes, if (and only if) it is a
TRANS-child: `SetDirty` recursion + lazy `local×parentWorld` recompute is verified working
(`Trans.cpp:99-107`, `:127-140`); no "world-authored child" exception exists. The lane's own
STATUS observed it moving as one rigid unit with the picture. The revealed grey element is
therefore a SEPARATE node (group-membership ≠ trans-parenting is the distinction that matters —
`RndGroup::mObjects` is a draw list), and the fix is to identify every element of the visual
assembly by TransParent chain and move the trans-roots together — not a different transform op,
and not a propagation bug (A7).

**R-D (collision matrix + gates):** Empty on the expected path once A3's relocation is adopted —
Lane R is rb3-side (`BandCharacter.cpp`), Lane U's engine writes are `Rnd_Wgpu_RB3.h` +
`RB3PostProc.*`, Lane A is `SongSelectPanel.cpp`; the only shared file is the classjson under
flock. Single-writer-per-TU is preserved without sequencing; a Lane-R engine escalation is the
one event that re-creates a conflict and is pre-gated as a coordinator STOP. Gates: achievable and
fail-red as listed, with A8's three ungameable mechanism additions for Lane R (gender-distinct
vert deltas, offset provenance, no-matching-TU-edit rule) and the freeze-detector rider carried.
The seven-defaults tally is verified (A9).

---

## Source appendix (verified at review time)

- rb3 `src/system/rndobj/MeshDeform.cpp`: `Reskin` `:298-399` (xfm build `:308-317`, weighted
  blend `:329-364`, pos/norm overwrite `:366-395`, `mDeformed` latch `:300-302`); `VertArray`
  8-bit weights `:124-135`; class is plain Hmx::Object with authored `mMesh`/`mBones`/`mVerts`
  (`MeshDeform.h:14-73`).
- rb3 `src/system/rndobj/Mesh.h`: `RndBone { mBone, mOffset }` `:33-52`; `Vert` — float
  pos/norm, `Vector4_16_01 boneWeights` (0x18), `short boneIndices[4]` (0x28) `:60-86`;
  `SyncMeshCB` `:386-399`.
- rb3 `src/system/bandobj/BandCharacter.cpp`: `SetDeformation` `:3064-3145` (gender clip pose
  `:3075-3110`, `mgr->Disable(!mInCloset)` `:3089`, unk610 Reskin loop `:3111-3115`);
  unk610 population `:2974-2978`; post-SetDeformation rest capture + rebind re-arm `:2420-2455`;
  V24 two-pass rebake note `:1449-1453`; GeomOwner palette note `:1432-1448`.
- rb3 `src/system/char/CharMeshCacheMgr.h`: `MeshCacher` cache/restore `:7-66` (`SyncMesh` →
  `mMesh->Sync(mFlags|0xA0)` `:20`).
- rb3 `src/system/bandobj/BandPatchMesh.cpp`: runtime RndMeshDeform synthesis precedent
  `:1408-1420`.
- rb3 `src/system/rndobj/Trans.{h,cpp}`: `DirtyLocalXfm`/`SetDirty` `Trans.h:98-102,150-152`;
  `DirtyCache::SetDirty_Force` recursion `Trans.cpp:99-107`; `WorldXfm_Force` composition
  `Trans.cpp:127-140`; constraints `:131-137` + `ApplyDynamicConstraint` `:221` region.
- rb3 `src/system/rndobj/Group.cpp`: `mObjects` draw-list membership `:21,38` (no trans coupling).
- rb3 `src/system/rndobj/Rnd.h`: base virtuals `DoPostProcess` `:115`, `ClearDepthForOverlay`
  `:124` (empty default).
- rb3 `src/system/ui/PanelDir.cpp`: extern-decl pattern `:24-25`; wired trigger `:146-174`
  (gameplay gate `:168-170`, latch+ClearDepthForOverlay `:170-172`).
- engine `3b5af48` `src/platform/Rnd_Wgpu_RB3.cpp`: `ClearDepthForOverlay` `:2307-2354`
  (flush path `:2317-2321`, else-branch depth+stencil clear re-open `:2326-2354`); L1 unpack
  cache + invalidation contract `:2694-2711` ("V24 shard guard re-reads bind-pose verts EVERY
  frame"; invalidation = owner/fpVerts/fpFaces/fpSkinned + OnSync `uploaded=false`); lazy VB
  create `:3013`; V24 guard `:4290` region.
- engine `src/platform/Rnd_Wgpu_RB3.h`: `FlushPostProcMidFrame` declared PRIVATE `:257`
  (private block from `:198`).
- engine `src/platform/RB3PostProc.cpp`: `FlushPostProcMidFrame` `:44-107` (consume-at-top
  `a5cf8d3` `:55`, early-returns `:56-59`, `venueGrade=!menuBoundary` composite `:73`,
  framebuffer re-open `:78-101`); `DoPostProcess` `:108-118`; moved-TU note `:10-13`.
- engine `src/platform/NativeCompatFlags.classification.json` + README flip anchors per A9.
- Engine pin verified: rb3 `native/CMakeLists.txt:74` = `3b5af488…` = engine HEAD (`3b5af48`,
  Wave-13 regen, 354 flags); `a5cf8d3` is its parent — the kickoff's "engine + pin 3b5af48"
  framing is current.
