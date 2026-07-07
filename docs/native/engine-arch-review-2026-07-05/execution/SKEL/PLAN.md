# Lane SKEL — S-S2 (fix, flag-first default-OFF) — PLAN

Wave 13, KEY=SKEL, STAGE=S-S2. Engine pin `44716f4`. Charter: implement the two-half fix
(un-share + gender-pose) at the S-S1-named seam, band-side scoped, flag-first default-OFF.
STOP-TRIPWIRE (binding): "if the seam degenerates into any dead cell OR the un-shared bone
resolves-but-doesn't-animate → STOP, verdict BLOCKED with evidence."

(S-S1 mechanism-study PLAN preserved in git history at commit 8f73b6fa.)

## Objective (as dispatched)
Register a new default-OFF flag and implement seam A (per-member gender-posed authored bind)
and/or seam B (per-vertex shell re-pose) from S-S1, in `RebindHeadHandsAtRest`
(`BandCharacter.cpp:1595-1725`, re-derived by symbol), then pass the pre-registered gates.

## Files inspected (line ranges re-derived BY SYMBOL on current tree, HEAD 8f73b6fa)
- `src/system/bandobj/BandCharacter.cpp`:
  - `RebindHeadHandsAtRest()` `:1253`; default distinct rest capture
    `NativeCharSpaceRestXfm(own)` `:1656`; pass-B bake
    `Multiply(mesh->WorldXfm(), invRest, mesh->BoneOffsetAt(b))` `:1725`; rebound flag `:1752`.
  - `RB3_HANDS_SHELL_FIX` (own-live + bound-rest) branch `:1481-1507`, header `:1316-1339`.
  - `RB3_APPENDAGE_ASSET_REBAKE` (bound-live + bound-rest) branch `:1523-1554`.
  - `SetDeformation()` `:3064`; `RndMeshDeform::Reskin` per-member reskin call `:3111-3115`.
  - `FilterSubdir` 2026-06-06 note (the original "un-share + gender-pose" statement) `:3915-3937`.
- `src/system/bandobj/BandCharDesc.cpp`: `GetDeformClip()` `:59-64` — gender bind = runtime CharClip.
- `src/band3/meta_band/AssetTypes.cpp`: `male_hands_naked`/`female_hands_naked` `:250-256`.
- engine `src/platform/Rnd_Wgpu_RB3.cpp`: palette build (skin = `BoneOffsetAt(i) * boneWorld`)
  `:3299-3305`; INSTR_B/wext/IK_SHARD instruments `:4394-4983` — READ-ONLY (renderer untouched).

## Declared edit range (pre-registered before editing)
Intended: new getenv-gated branch inside `RebindHeadHandsAtRest` `:1462-1707` + a NativeCompatFlags
classification append. **NOT EXECUTED** — the feasibility gate below tripped the STOP-TRIPWIRE
before any source edit. No file in this range was modified this stage.

## Feasibility gate (run FIRST, per STOP-TRIPWIRE) → TRIPPED → BLOCKED
Provenance-independent proof that seam A degenerates + seam B is out of scope; both from source +
the S-S1 APD_DIAG hard data + the committed `d016ce66` measurement. See STATUS.md for the full
argument. Result: no non-degenerate, in-scope implementation exists → verdict **BLOCKED**, no
source change, no flag registered (registering a byte-identical-to-default flag would be a fake).

## Deliverable
STATUS.md (BLOCKED verdict + degeneration proof + APD_DIAG evidence + named real fix = engine
per-member reskin), checkpoint `/tmp/wave13-checkpoints/S-S2.json`, docs-only commit.
