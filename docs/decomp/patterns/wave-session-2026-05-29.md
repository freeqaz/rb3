# Session Notes — 2026-05-29 (Analysis-Only: Binary Unavailable)

This session ran in a fresh web container where `orig/SZBE69_B8/sys/main.dol`
is not present (CI Docker volume only available in local dev/CI environments).
No builds, objdiff, permuter, or Ghidra were possible. All content below is
pure code-reading analysis to be verified in the next binary-available session.

## Context: Recent Permuter Wins (wave-y/z/aa, 2026-05-28)

The permuter fleet completed waves y, z, and aa. Key confirmed wins:

| Function | Before | After | Pattern |
|---|---|---|---|
| `GemPlayer::Hit` | 87.9% | 96.39% | `UpcomingFretRelease()` ctor added (see below) |
| `GemPlayer::UpcomingFretRelease` vector helpers | 94.5% | 100% | Same ctor fix |
| `ChordShapeGenerator::Edge` vector helpers | 88.1% | 100% | Ctor REMOVED (all-int POD) |
| `AppLabel::SetPitch` | 95.5% | 100% | Permuter |
| `HiResScreen::Accumulate` | 95.2% | 100% | Permuter |
| `ClipDistMap::FindBestNode` | 87.4% | 92.6% | Bool materialization: explicit if/else |
| `VocalTrack::UpdateScrolling` | 79.3% | 80.0% | const bindings + 64.0 double literal |
| `RockCentral::SyncSetlists` | 85.05% | 94.54% | Permuter |

The **deep-dive target list from 2026-05-26 is partially stale**:
- `GemPlayer::Hit` is now ~96%, not 87.9%
- `VocalTrack::UpdateScrolling` is at ~80%, not its old value

## POD/Non-POD Struct Trait Pattern (New from Wave-y)

Two opposite fixes both paying off:

### Adding ctor to a struct with float fields (UpcomingFretRelease)

```cpp
// GemPlayer.h
class UpcomingFretRelease {
public:
    UpcomingFretRelease() {}   // ← ADDED empty ctor
    int unk0;
    float unk4;   // ← float field
};
```

With no ctor: POD trait → `_M_insert_overflow_aux<UpcomingFretRelease>` uses
`lwz/stw` (word copy), mismatching target's `lfs/stfs` (float-typed copy).
Adding an empty ctor makes it non-POD → float-typed copies → matches.

**When to apply**: struct has a float field AND is stored in `std::vector<>` AND
the target's `_M_insert_overflow_aux` uses `lfs/stfs` for that field.

### Removing ctor from a struct with only int fields (ChordShapeGenerator::Edge)

```cpp
// ChordShapeGenerator.h
struct Edge {
    // ← ctors REMOVED
    unsigned short v0, v1;  // only int-width fields
};
```

With user ctors: non-POD → per-field byte/halfword copies (`lhz/sth`).
Removing ctors: POD → word copies (`lwz/stw`). Target uses `lwz/stw`.

**When to apply**: struct has ONLY int-width or pointer fields AND has user-declared
ctors that the target doesn't need for behavior AND the target uses `lwz/stw` copies.

### Candidate structs to check

Run `/analyze-function` on any `_M_insert_overflow_aux<T>` that isn't 100%,
then check whether the copy sequence uses `lfs/stfs` (float field → needs ctor)
or `lwz/stw` for all-int struct with ctor (→ remove ctor).

Files with `std::vector<SomeSmallStruct>` still worth checking:
- `band3/game/CommonPhraseCapturer.h:PhraseState` — 3 int fields, has ctor
- `band3/game/BandPerformer.cpp` — `std::sort(crowdratings...)` on vector<float>
- `band3/game/Stats.cpp` — `std::sort` on `PartPercentageSorter`

## `BandPatchMesh::FindXfm` Structural Bug Analysis (56.5% match)

**Location**: `src/system/bandobj/BandPatchMesh.cpp:968`

### The bug: trivially-true condition + dead loop

```cpp
// Line 1010 — ALWAYS TRUE, this is a bug:
if (endFace == endFace) {
    float minDistSq = 1e30f;
    unsigned short *facePtr = faceBase;
    while (facePtr != endFace) {   // ← this loop NEVER runs (facePtr==endFace after phase 1)
        // find nearest edge...
    }
}
```

After the phase-1 triangle search:
- **Found case**: `endFace = faceBase` (break before advancing) → `faceBase == endFace` → phase-2 loop: `facePtr = faceBase = endFace` → loop body never runs
- **Not-found case**: `faceBase` advanced to original `endFace` → `faceBase == endFace` → same, loop body never runs

In both cases phase-2's while loop is dead. The not-found path then falls through
to `triVerts[i] = &mesh->Verts(endFace[i])` with `endFace` pointing past all
faces → out-of-bounds memory access. This explains the 56.5% match.

### Reconstructed correct structure

The correct code likely uses a saved `searchEnd` pointer so phase 1 can
distinguish "found" (endFace moved from original) vs "not-found" (still at end):

```cpp
unsigned short *faceBase = (unsigned short *)&mesh->Faces()[0];
unsigned short *searchEnd = faceBase + mesh->Faces().size() * 3;
unsigned short *endFace = searchEnd;  // starts at end; phase 1 may move it

// Phase 1: find containing triangle
{
    float zero = 0.0f;
    unsigned short *fb = faceBase;
    while (fb != searchEnd) {
        // ... cross-product winding test ...
        if (matched == 3) {
            endFace = fb;   // Found — move endFace to the triangle
            break;
        }
        fb += 3;
    }
}

// Phase 2: if no containing triangle found, find nearest edge
if (endFace == searchEnd) {   // NOT found (endFace still at original end)
    float minDistSq = 1e30f;
    unsigned short *facePtr = faceBase;  // restart from beginning!
    while (facePtr != searchEnd) {
        // ... segment-distance calc ...
        if (distSq < minDistSq) {
            minDistSq = distSq;
            endFace = facePtr;  // track nearest face
        }
        facePtr += 3;
    }
}
// endFace now reliably points to best face
```

**How to verify**: use `/analyze-function BandPatchMesh::FindXfm -u system/bandobj/BandPatchMesh`
and compare the Ghidra pseudo-C to the reconstruction above. The key signal is whether
Ghidra shows a saved "searchEnd"-equivalent variable.

## Remaining High-Value Structural Targets (2026-05-29 estimate)

| Function | Est. % | Size | Note |
|---|---|---|---|
| `VocalPlayer::Poll` | ~74% | 8.7 KB | Main vocal-scoring dispatcher |
| `VocalTrack::UpdateScrolling` | ~80% | 11 KB | Largest in-scope gap |
| `BandPatchMesh::FindXfm` | 56.5% | 3.4 KB | Structural bug (see above) |
| `Singer::PostLoad` | ~76% | small | Unknown structural issue |

`GemPlayer::Hit` is now ~96% — no longer a structural target, likely at-limit
or permuter territory for the last 4%.

## Bool Materialization Pattern (from wave-aa ClipDistMap commit)

The `ClipDistMap::FindBestNode` fix from wave-aa3:

```cpp
// Source that triggered mfcr+srwi dance:
foundBetter = (newDist < bestDist || (newDist == bestDist && newIdx < bestIdx));

// Fix: explicit if/else branches with literal true/false:
if (newDist < bestDist || (newDist == bestDist && newIdx < bestIdx)) {
    foundBetter = true;
} else {
    foundBetter = false;
}
```

The permuter finds this automatically in the 85-99% range. Apply manually only
when the permuter is blocked (e.g. the bool is inside a complex loop that the
permuter can't restructure).

## Double-Precision Constant Pattern (from VocalTrack wave-z commit)

`VocalTrack::UpdateScrolling` improved with `64.0` (double, not float) constant
for `fmul/frsp` emission. Pattern: when target has `fmul` + `frsp` where source
has single `fmuls`, try promoting the constant to double:

```cpp
// float (single fmuls):
float v = x * 64.0f;
// double (fmul + frsp double-precision lowering):
float v = x * 64.0;
```

Check nearby float arithmetic with known constants near power-of-2 values.

## Next-Session Priorities

1. **Fix `BandPatchMesh::FindXfm`** — use Ghidra to confirm the `searchEnd`
   reconstruction, then fix the condition and phase-2 loop start pointer.
   Expected gain: likely +10-20% from fixing the structural bug.

2. **Run permuter on `GemPlayer::Hit`** — now at ~96%, permuter should handle
   the last few percent of register cascades.

3. **`VocalPlayer::Poll`** — 74% is structural. Use `/compare-asm` and
   `/run_diff_inspect mode=clusters` to identify the structural gap families.

4. **Check POD/non-POD struct candidates** — `PhraseState`, `BandPerformer`
   crowd ratings sort, Stats `PartPercentageSorter`. Each needs binary analysis.

5. **Permuter sweep of system/bandobj/** — ClipDistMap was there at 87.4%;
   run `batch_auto --unit 'system/bandobj/*'` to sweep remaining 85-99% functions.
