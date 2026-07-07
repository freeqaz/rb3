# Wave 14 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE14_REVIEW.md` rb3 `7f5b2160`) — **all 9
amendments adopted**; dispatched with the corrected shape below.
Parent: `execution/README.md` (Wave 13 results + Wave 14 menu). Engine pin `3b5af48`.

## COORDINATOR ACCEPTANCE (2026-07-07) — final dispatched shape

Fable review: **dispatch-with-amendments** (9). Adopted in full:

- **A1 (CRITICAL, Lane R) — the Reskin premise corrected:** `RndMeshDeform::Reskin` lives in rb3
  (`src/system/rndobj/MeshDeform.cpp:298-399`, NOT engine) and transforms the mesh's current
  POSITIONS/NORMALS in place via a per-vertex weighted MULTI-BONE blend of `offset·boneWorld`; it
  never writes weights, and it requires an authored RndMeshDeform asset — `hands_naked` is absent
  from unk610 because NO SUCH ASSET EXISTS. R2 must SYNTHESIZE the deform inputs (precedent:
  `BandPatchMesh.cpp:1408-1420`). The primitive family is still right — the weighted blend dodges
  Seam B's mixed-sign wall (Seam B applied ONE bone's delta per vertex; the blend applies ALL,
  same model as the GPU palette) — but the residual is ordinary LBS blend error: the wext gate
  stays QUANTITATIVE (≤60u), not binary-zero.
- **A2 (HIGH, Lane R) — invocation point pinned:** the reskin's source data (`RndBone::mOffset`,
  the per-mesh authored inverse binds, `Mesh.h:33-52`) is DESTROYED by the default-ON
  `RebindHeadHandsAtRest` rebake after first Poll. The reskin must run at `SetDeformation` time
  (skeleton posed at the deterministic gender-bind rest, `BandCharacter.cpp:3064-3145`) with a
  once-per-mesh latch, BEFORE the rebake consumes the authored offsets.
- **A3 (HIGH, cross-lane) — collision matrix EMPTY:** Lane R is rb3-side; CPU verts are resident
  and GPU re-upload is automatic (fingerprint/OnSync + lazy first-draw, engine `:3013`) — zero
  expected engine TU edits. "V24 compressed verts" was a misnomer (V24 = the shard-guard version
  tag, not a vert format) — the R-A data-availability risk largely dissolves.
- **A4 (MEDIUM, Lane R):** the female-bind question is ANSWERABLE cheaply: dump both genders'
  hands `mOffset` arrays pre-Poll in R1; the S2 "double mismatch" is currently pointer-level only
  — R1 must establish whether the female mesh carries female-authored offsets.
- **A5/A6 (MEDIUM, Lane U) — seam corrected + minimized:** the flush lives in
  `RB3PostProc.cpp:44`, not Rnd_Wgpu_RB3.cpp. Minimal seam = a one-line visibility change at
  `Rnd_Wgpu_RB3.h:257` + an RB3PostProc flush-only shim + the PanelDir trigger swap. FORBIDDEN: a
  base-`Rnd` virtual (would need ungranted rb3 `Rnd.h`). The red-band mechanism is CONFIRMED
  consistent with source (the else-branch performs depth+stencil clears per subsequent UI dir,
  `Rnd_Wgpu_RB3.cpp:2326-2354`); the `a5cf8d3` consume-at-top latch is safe; KEEP the gameplay
  gate on the trigger.
- **A7 (MEDIUM, Lane A) — R-C refuted from source:** dirty propagation to trans-children WORKS
  (`Trans.cpp:99-107,127-140`) and the lane's own STATUS saw `album_frame01.mesh` move. The
  revealed grey element is a SEPARATE node (group draw-membership ≠ trans-parenting). Lane A must
  identify it by TransParent-chain evidence (draw-log + parent walk), not assumption.
- **A8 (LOW, Lane R) — three ungameable mechanism gates added to R2:** (i) gender-distinct vert
  deltas (the reskin output differs male vs female — kills a shared-transform fake); (ii) offset
  provenance (post-reskin verts consistent with own-basis skinning, not a clamp); (iii)
  no-matching-TU-edit rule (the fix must not touch the wext/instrument TUs it is gated by).
- **A9 (LOW):** the "seven defaults ON" tally verified correct anchor-by-anchor.

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

## Where we are (entering Wave 14)

Wave 13 named all three remaining fixes precisely:

1. **Hands = bind-basis split; fix = per-member RESKIN.** `hands_naked` (and
   `fingernails_resource`) verts+weights are skinned against the SHARED male-bind skeleton
   (`bound`) while drawing on the per-member gender-posed skeleton (`own`); females are
   double-mismatched (female-authored verts → male bind). The offset-bake fix class is formally
   exhausted (6 measured dead cells + a proven-degenerate 7th framing). The one faithful lever:
   per-member reskin of verts+weights onto `own` via the existing `RndMeshDeform::Reskin` pipeline
   (`BandCharacter.cpp:3111-3115` uses it for the unk610 deform set, which hands_naked is absent
   from). Engine + BandCharacter work, coordinator-scoped.
2. **Focused-text = grade-exempt UI; fix = clean flush-only seam.** The wired
   `ClearDepthForOverlay`-driven trigger passes every gate except song_select, where its
   depth-clear side effect produces a VISIBLE red band on the SETLISTS row (E1 hold). A flush-only
   public entry (no depth-clear) into `Rnd_Wgpu_RB3.h/.cpp` removes it; flip `RB3_UI_POST_GRADE`
   after.
3. **Album art = whole-assembly move.** The −120u `album_art.grp` nudge works but reveals a grey
   ornate bezel/frame element that does not move with the group (+ a new left-column overlap).
   Identify that element, move the assembly together, then flip.

## Proposed Wave 14 lanes

**Lane R — RESKIN (Opus ×2; engine `RndMeshDeform`/mesh pipeline + rb3 `BandCharacter.cpp`;
the headline lane):**
- **R1 (feasibility + design, diagnosis-only):** map the Reskin pipeline: what
  `RndMeshDeform::Reskin` actually does (inputs: source/dest skeleton TransObject sets? re-maps
  bone indices? re-poses positions?), whether `hands_naked`'s vert data is CPU-resident and
  re-skinnable at load (V24 compressed verts — decode/re-encode round-trip?), what the unk610
  deform set membership means and why hands_naked is absent, and where the invocation belongs
  (RebindHeadHandsAtRest time? SyncObjects?). Answer the female question explicitly: what source
  bind do female verts need (female authored bind — does it exist as data?). Deliverable: seam +
  data-availability verdict + cost (one-time at load?) + risk list. NO fix.
- **R2 (fix, flag-first default-OFF):** implement per-member reskin per R1. GATES (pre-registered;
  instruments exist): rest-free Instrument-B invariants ~0; Tier-2 ≤1u; wext on the sighting
  protocol 95-106u → ≤60u WITHOUT freezing; guard-DROP census unchanged; crowd placement oracle
  GREEN both arms + RB3_NO_CROWD_REBIND fail-red intact; RealPathFixture + skin-golden gtests;
  lineup PASS; flag-OFF drawlog 792 byte-identical; no gloves/torso regression; before/after band
  screenshots for E1 (both genders on screen — the female is the worst case).
- **STOP-TRIPWIRE:** no offset-bake variants (class exhausted). If vert data is not re-skinnable
  (GPU-resident only / lossy V24 round-trip), report BLOCKED with the evidence — do not fake it
  with clamps.

**Lane U — UIGRADE clean seam (Opus; **COORDINATOR GRANT**: `Rnd_Wgpu_RB3.h` + the
`FlushPostProcMidFrame`/`ClearDepthForOverlay` region of `Rnd_Wgpu_RB3.cpp` (re-derive by symbol)
+ `RB3PostProc.*` + rb3 `PanelDir.cpp` trigger):**
- Add a flush-only public entry (no depth-clear); switch the PanelDir trigger to it. VERIFY: the
  song_select SETLISTS red band is GONE flag-ON (pixel-compare the row ROI vs flag-OFF) and the
  contrast metric returns to parity band; hub stays ≥2.0 (2.204 baseline); partdiff in-band; A5
  backdrop chroma unchanged; gameplay pixel-invariant; flag-OFF 792 byte-identical; DC3 zero-blast.
  Deliver fresh E1 captures (hub + song_select, both arms). Coordinator flips after E1.

**Lane A — C2b art assembly (Sonnet, game-side only):**
- Identify the grey bezel/frame element revealed by the −120u nudge (draw-log the quads in the art
  rect region flag-ON; candidates: a sibling frame mesh outside `album_art.grp`, a details-page
  element, a list-highlight frame). Determine the correct assembly (which nodes must move
  together) and re-implement the fix as one whole-assembly offset (replacing or extending
  `RB3_SS_ART_YFIX`). Also fix the new left-column overlap (the E1 hold noted the art now collides
  with the left column icon — the offset may need an X component or a smaller Z). GATES: E1
  captures vs retail (art below header, no revealed frame, no new overlaps); drawlog 792 flag-OFF
  unchanged; settle-frames (frame-count-settled capture per the C2b4 methodology finding — NOT
  wall-clock sleeps).

**Deferred:** sidebar backing quad (authored polish), bar-bleed text polarity (song_select/
partdiff), loader sufficient-fix, WHITE real-lever, 4→8 lights, W2.4.

## Process rules (carried)

Locks, checkpoints (`/tmp/wave14-checkpoints/`), commit-per-review-cycle, PLAN/STATUS per item,
append-only classjson + single coordinator regen, own build dirs, NO pin bumps/default flips by
lanes, refuted flags UNSET (incl. `RB3_HANDS_SHELL_FIX`), pgid-only process cleanup. Seven
defaults now ON (placement, black head, hands rest-capture, text floor, hub quad, chroma-preserve,
hub ticker).

## Risks / open questions for the reviewer

- **R-A (Lane R):** is `RndMeshDeform::Reskin` actually the right primitive (verify its signature
  + semantics in source — does it re-map weights or only re-pose verts)? Is hands_naked's vert
  data available CPU-side at the needed moment? What does the V24 path do to a re-skinned vert?
  Is there a female authored bind to reskin FROM, or does the female case need a different source?
- **R-B (Lane U):** is a flush-only entry actually side-effect-free on song_select (the red band
  might not be the depth-clear — verify the mechanism before building the seam), and does the
  latch-consume hardening from `a5cf8d3` interact?
- **R-C (Lane A):** the "revealed frame" — is it maybe `album_frame01.mesh` INSIDE the group
  failing to move because the nudge dirties only the group xfm (children with world-authored
  xfms wouldn't follow)? Verify how DirtyLocalXfm propagates to children before assuming a
  sibling element.
- **R-D:** Lane R and Lane U both touch engine — file collision matrix (RndMeshDeform/mesh
  pipeline vs Rnd_Wgpu_RB3.h/postproc region) and whether single-writer-per-TU forces sequencing.
