# Wave 14 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** DRAFT — under Fable pre-dispatch review, not yet dispatched.
Parent: `execution/README.md` (Wave 13 results + Wave 14 menu). Engine pin `3b5af48`.

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
