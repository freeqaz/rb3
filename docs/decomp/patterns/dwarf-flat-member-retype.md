# Fixable: Flat `float unkN` Members That Are Really Aggregates (DWARF Retype)

When a header declares a run of flat `float unkN` members that were, in the
original source, a `Vector2` / `Vector3` / `Hmx::Color` / `Transform` sub-object,
MWCC generates the *wrong copy shape*. Retyping the flat floats back to the real
aggregate (recovered from the Bank-5 DWARF) flips our codegen to match the
target. This is a real, landed match lever — but a **rare** one. Read the base
rate at the bottom before you go hunting: across the whole DWARF sweep (~35
structs) it fired exactly **once**.

## The lever

A struct copy / element-fill of a POD aggregate lowers differently depending on
whether the compiler sees the members as one flat scalar blob or as typed
sub-aggregates:

- **Flat `float unkN`** members → MWCC **block-copies** the region with integer
  `lwz` / `stw` word moves (it treats the copy as an opaque byte span).
- **Typed `Vector3` / `Vector2` / `Color`** members → MWCC **member-copies**,
  emitting float `lfs` / `stfs` per component (it copies each sub-object with its
  own type's copy).

The target was compiled from source with the real aggregate types, so it uses
`lfs` / `stfs`. Our flattened header makes MWCC use `lwz` / `stw`. Retyping the
`unkN` floats back to the aggregate (from the Bank-5 DWARF member names/types)
makes MWCC emit the float-move shape and the copy region snaps into alignment.

This bites hardest inside STL container growth helpers — `_M_fill_insert_aux<T>`
and `_M_insert_overflow_aux<T>` — because those instantiate `T`'s copy over the
new element(s). `T = RndLine::Point` (below) is the worked example.

## Diagnostic signature (objdiff level)

Look for a **run of `replace` diffs inside a struct-copy region** where the two
sides use different memory-op *classes*:

- **target `lfs` / `stfs`  vs  base (ours) `lwz` / `stw`** → we flattened an
  aggregate the original kept typed. **Retype our header** (the common case).
- the **inverse** (target `lwz` / `stw` vs ours `lfs` / `stfs`) → the original
  flattened something we typed; rare, but the same tool surfaces it.

The run is concentrated in a copy helper: stlport `_M_fill_insert_aux` /
`_M_insert_overflow_aux` instantiations, `operator=`, or a hand copy ctor.

### Counter-signature (do NOT retype)

If the copy loop uses **`lwz` / `stw` on BOTH sides**, the struct is *already
member-faithful* and a retype changes nothing — the residual is permuter-class
regalloc/spill, not a type-flattening bug.

- `RndMeshDeform::BoneDesc` (g10): the value-copy loop is `lwz`/`stw` on both
  sides at the Transform matrix-word offsets (`li r22, 0xc / 0x18 / 0x24 / 0x30
  / 0x3c / 0x48 / 0x54 / 0x60`). Both `Transform` members were already typed;
  the 89–90% residual on its `_M_*_aux` instantiations is a whole-bank
  callee-saved renumbering (r18↔r27 …) driven by a `bool` param spill — pure
  permuter-class. A retype was proven to change nothing.

The source permuter surfaces this signature as a steering hint
(`decomp-synth` `flat_struct_copy`) so agents stop permuting and run the retype
tool instead — but only for the `lfs/stfs`-vs-`lwz/stw` split direction, never
the both-sides-`lwz/stw` case.

## Worked example: `RndLine::Point` (`34c4033c`)

`RndLine::Point`'s tail was nine flat floats. The Bank-5 DWARF (die `0x4d7386`,
cu `0x4d701a`, size `0x50`) shows the real members: `v` (`Vector3`) @0,
`c` (**`Color`**, 16B in Bank 5) @0x10, `cam` (`Vector3`) @0x20,
`base`/`dir`/`delta` (`Vector2`) @0x30/0x38/0x40. Bank 8 packed the 16-byte
`Color` down to a 4-byte `Color32`, which pulls the whole tail forward, so on our
Bank-8 layout the nine floats are `cam`(@0x10) + `base`(@0x1c) + `dir`(@0x24) +
`delta`(@0x2c).

Before (`34c4033c^:src/system/rndobj/Line.h`):

```cpp
class Point {
public:
    Point() : v(0, 0, 0), c(-1) {}
    Vector3 v; // 0x0
    Hmx::Color32 c; // 0xc
    float unk0; // 0x10
    float unk1; // 0x14
    float unk2; // 0x18
    float unk3; // 0x1c
    float unk4; // 0x20
    float unk5; // 0x24
    float unk6; // 0x28
    float unk7; // 0x2c
    float unk8; // 0x30
};
```

After (`34c4033c:src/system/rndobj/Line.h`):

```cpp
class Point {
public:
    Point() : v(0, 0, 0), c(-1) {}
    Vector3 v; // 0x0
    Hmx::Color32 c; // 0xc
    Vector3 cam;   // 0x10 - view-space position (Bank 5 DWARF: "cam")
    Vector2 base;  // 0x1c - cam projected onto the view plane (x/y, z/y)
    Vector2 dir;   // 0x24 - screen-space direction to the next point
    Vector2 delta; // 0x2c - perpendicular half-width offset
};
```

Retype mapping: `cam → {unk0,unk1,unk2}`, `base → {unk3,unk4}`,
`dir → {unk5,unk6}`, `delta → {unk7,unk8}`.

Because the members became typed aggregates, the `*(Vector3 *)&pt->unk0` /
`*(Vector2 *)&pt->unk7` **reinterpret casts in `Line.cpp` were deleted** in favor
of direct member access (`pt1->cam.x`, `Subtract(pt1->cam, pt1->delta, …)`), and
a hand alias struct (`_RndLineW3 unkA`) went away. `_M_fill_insert_aux<RndLine::Point>`
went **95.7% → 99.6%**. Full-rebuild regression gate across all units: **1 up /
0 down** (the copy helper is the only site that changes; the aggregate members
are laid out at the same offsets, so nothing else moves).

## The tool: `scripts/analysis/flat_member_retype_scan.py`

The tool separates the buckets programmatically so nobody hand-audits 35 structs
again. It parses our headers (`tools/struct_db.py :: parse_header`) against the
Bank-5 DWARF (streamed from the debug ELF) and classifies each candidate struct.

```bash
python3 scripts/analysis/flat_member_retype_scan.py --selftest      # hermetic, no ELF/git needed
python3 scripts/analysis/flat_member_retype_scan.py --filter Point --unit system/rndobj
python3 scripts/analysis/flat_member_retype_scan.py --filter LightParams_Spot
python3 scripts/analysis/flat_member_retype_scan.py --json /tmp/retype_scan_full.json
python3 scripts/analysis/flat_member_retype_scan.py --refresh       # rebuild the DWARF cache
python3 scripts/analysis/flat_member_retype_scan.py --min-run 3     # min consecutive scalar run
```

The first real run streams ~11M `readelf` lines (minutes; progress to stderr) and
caches the resolved DWARF registry; later runs are instant. `--refresh` rebuilds.
`--unit` restricts by unit path, `--filter` by class-name substring, `--json`
dumps machine-consumable evidence for every struct.

### The five verdicts

| Verdict | Meaning | Action |
|---|---|---|
| `RETYPE_CANDIDATE` | flat scalars name/position-match a DWARF aggregate | **confirm against Bank 8, then retype** |
| `ALREADY_TYPED` | the aggregate members are already declared as aggregates | nothing to do (permuter-class residual, if any) |
| `GENUINE_SCALARS` | the flat floats really are separate scalars in the DWARF | rename-only at most; no match lever |
| `DIVERGENT_CAUTION` | Bank-5 layout diverges too far to map safely | skip; don't guess offsets |
| `NO_DWARF` | the struct has no Bank-5 DWARF entry at all | no evidence; skip |

### Honesty model (read this before editing a header)

The tool is a **candidate generator, not an authority.** The only DWARF is the
**Bank-5 proto** ELF (~mid-2009); the target is **Bank 8** (~2010), and the two
banks genuinely diverge:

- Bank-5 `Vector3` is `0x10` (16-byte, 16-aligned); Bank-8 `Vector3` is `0xc`
  (12-byte, 4-aligned). Bank-5 offsets are **not** copyable to Bank 8.
- members were added / dropped / reordered per bank (see the negative buckets).

So a `RETYPE_CANDIDATE` is confirmed **only by Bank-8 evidence** before a header
edit lands:

1. the objdiff copy region actually shows the `lfs/stfs`-vs-`lwz/stw` split;
2. anchored offset deltas check out (the `scripts/analysis/struct_confirm.py`
   Bank-8 confirmation layer — the exact mold this tool follows);
3. the full-rebuild regression gate is net-positive (1 up / 0 down for Point).

This mirrors `scripts/analysis/struct_layout_audit.py` +
`scripts/analysis/struct_confirm.py`: generate candidates cheaply, confirm
against Bank 8 before touching a header.

## The negative buckets, with sweep receipts

The whole point of the tool is that **retypes are rare**. Every struct below was
hand-audited during the DWARF sweep and produced **no** retype. Trust the tool's
bucket instead of re-auditing.

### `ALREADY_TYPED` — the aggregate is already declared correctly

- `CharIKFingers::FingerDesc` (g2, f3): its `Vector3 mDestPos`/`mDestForwardVector`/
  `mCurForwardVector` and `ObjPtr` bones were already typed; `_M_fill_insert_aux<FingerDesc>`
  @97.27% is permuter-class (ObjPtr copy-ctor scheduling), not a flattening bug.
- `RndMeshDeform::BoneDesc` (g10): both `Transform` members already typed; the
  copy loop is `lwz/stw` on both sides (the counter-signature above).

### `GENUINE_SCALARS` — the flat floats really are separate scalars

- `BoxMapLighting::LightParams_Spot` (g1): the seven flat floats are genuine
  cone/distance cache values, not a hidden vector. Bank 8 in fact *flattened*
  its Bank-5 `CachedData` sub-struct inline **and dropped `mTipOffsetSqr`** — a
  divergence *away* from aggregation, the opposite of a retype opportunity.
- `VocalFramePartData` / `VocalScoreCache` / `VocalPhrase` (g4, g5): all-scalar
  score/pitch caches; DWARF confirms every member is a standalone float/int/bool.
- `GemPlayer` / `Player` / `Stats` (g7): whole structs are scalars (anchored by
  already-named fields on both sides); no sub-struct anywhere.
- `RndParticleSys::Burst` / `RndFont::CharInfo` / `RangeSection` (g9): 4-scalar
  aggregates (triangle-envelope, glyph metrics, pitch min/max) — genuinely flat.

### `DIVERGENT_CAUTION` — Bank-5 layout can't be mapped safely

- `CharEyes` (g3): Bank-5 `0x1d0` vs Bank-8 `~0x160`; disjoint member sets
  (Bank-5-only `mViewDirBone`; ours-only `mViewDirection`), a 32-byte unexplained
  tail. Offsets don't line up; every `Vector3` we have is already typed anyway.
- `Tail` / `GemManager` / `VocalTrack` (g6): proven cross-bank shifts (`Tail`
  has a +4 global shift; `GemManager` even differs in the `mGems` vector width;
  `VocalTrack` members start at a shifted base). No confident mapping.
- `OverdriveTracker` (g7): Bank-5 is a much smaller, differently-structured class
  (no map/multiplier map); trailing scalar patterns don't line up.

### `NO_DWARF` — no Bank-5 entry exists

- `CharClipDisplay` (g3), `Tail::SlideInfo` (g6), `OverdriveTimeTracker` (g7),
  `FreestylePanel` (g8), `BandPatchMesh::MeshPair` (g10): grepping the dump for
  the type name returns zero hits. No evidence → don't guess.

## Base rate: run the tool first

Across the full sweep — ~35 structs, 10 groups plus 3 Fable cases —
**`RndLine::Point` was the only true retype.** Everything else was already-typed,
genuinely scalar, bank-divergent, or DWARF-absent. That is exactly why the tool
exists: a `RETYPE_CANDIDATE` verdict is worth a Bank-8 confirmation and a header
edit; every other verdict tells you to stop and move on, without re-reading the
DWARF by hand.

## Related

- [fixable-struct-layout.md](fixable-struct-layout.md) — vtable/member-offset
  layout patterns (the other class of layout-driven codegen).
- [at-limit-mwcc.md](at-limit-mwcc.md) — the counter-signature case (already
  member-faithful → permuter-class / source-immune triage).
- [permuter-roi.md](permuter-roi.md) — when the residual is regalloc, not typing.
- `scripts/analysis/struct_layout_audit.py` / `scripts/analysis/struct_confirm.py`
  — the candidate-generator + Bank-8 confirmation pair this tool is modeled on.
