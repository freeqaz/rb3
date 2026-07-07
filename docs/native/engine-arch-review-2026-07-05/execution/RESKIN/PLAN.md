# Lane RESKIN — PLAN

**Goal (headline lane):** close the hands/fingernails **bind-basis split** shard by
re-posing each band member's `hands_naked` (+ `fingernails_resource`) vertices from the
authored (shared, male) bind basis onto that member's OWN gender-posed rest, via a
**synthesized per-vertex weighted multi-bone blend** (the Reskin primitive family) — the
one faithful lever after the offset-bake class was proven exhausted (SKEL/STATUS, 6 dead
cells + degenerate 7th).

## Stages
- **R1 (this stage) — feasibility + design, diagnosis-only.** Verify the synthesized-deform
  math end-to-end from source; dump both genders' authored `mOffset` pre-Poll (A4); confirm
  vert data availability + invalidation; price cost + mesh-instance sharing. Deliver
  FEASIBLE/BLOCKED + the precise R2 recipe. NO fix. — see STATUS.md.
- **R2 (DONE — VERDICT: REFUTED).** Implemented the per-member reskin per the R1
  recipe (flag-first, default-OFF `RB3_HANDS_RESKIN`). Builds clean, fires correctly
  (all bones resolved, gender-distinct, per-member in-place, zero engine edits). The
  pre-registered `wext` gate FAILS: flag-ON is a REGRESSION (hands_naked wext mean
  74.8 → 87.7u; target ≤60u), corroborated by E1 (larger hand shards, both genders).
  Root cause: the shard is an ANIMATION-BASIS problem (own_rest vs own_live rotation),
  not a rest-shape one — a vertex re-pose leaves it untouched and amplifies it. Same
  class as refuted `RB3_APPENDAGE_REST_ROT`/`_ASSET_REBAKE`; genuine fix is
  asset/skeleton-basis (skeleton_unshared.milo per-member bind). Kept default-OFF as
  the definitive measured dead-end. See STATUS.md. Original gate list below:
- **R2 (original spec) — fix, flag-first default-OFF.** Implement per-member reskin per the R1 recipe.
  Pre-registered gates (from WAVE14 A8): Instrument-B rest-free invariants ~0; Tier-2 ≤1u;
  wext 95-106u → ≤60u WITHOUT freeze (distinct-value freeze rider); guard-DROP census
  unchanged; crowd oracle both arms + `RB3_NO_CROWD_REBIND` fail-red; skin-golden + RealPath
  gtests; lineup PASS; flag-OFF drawlog 792 byte-identical; gender-distinct vert-delta
  histogram (ungameable); authored-offset provenance dump pre/post; NO `MeshDeform.cpp` / no
  Wii-matching-TU edit; both-gender E1 band screenshots.

## STOP-TRIPWIRE (binding)
No offset-bake variants (class exhausted). No clamp/freeze faking. If vert data were not
re-skinnable → BLOCKED with evidence. (R1 result: data IS re-skinnable — see STATUS.)

## File ranges (declared, by symbol)
- rb3 `src/system/bandobj/BandCharacter.cpp`:
  - `RebindHeadHandsAtRest()` — R1 probe insertion (top, post-`NativeCollectSkinnedMeshes`).
  - `SetDeformation()` (unk610 Reskin loop, after it) — R2 reskin call site.
- classjson append (R2 only) under flock; NO engine TU edits expected (A3).
- Leave `FxSendNative.cpp` untouched. Lane R does not touch `Rnd_Wgpu_RB3.*` / `PanelDir.cpp`
  / `SongSelectPanel.cpp` (Lanes U/A).
