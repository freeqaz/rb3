# BandFaceDeform::DeltaArray::AppendDeltas — Analysis

**Function**: `BandFaceDeform::DeltaArray::AppendDeltas`
**Symbol**: `AppendDeltas__Q214BandFaceDeform10DeltaArrayFRCQ211stlpmtx_std59vector<7Vector3,Us,Q211stlpmtx_std22StlNodeAlloc<7Vector3>>RCQ211stlpmtx_std59vector<7Vector3,Us,Q211stlpmtx_std22StlNodeAlloc<7Vector3>>`
**Unit**: `system/bandobj/BandFaceDeform.cpp`
**Current Match**: 67.1% (1612 bytes, 433 instructions)
**Status**: LIKELY_FIXABLE (medium confidence)

## Overview

`AppendDeltas` is a Run-Length Encoded (RLE) compression routine that encodes the vertex deformations (deltas) between two mesh frames. The function:

1. Iterates through vertex pairs comparing the "position" frame against a "base" frame
2. Quantizes 3D float deltas to signed bytes via clamping and scaling
3. Packs non-zero delta runs with a 4-byte header + 3-byte-per-vertex data format
4. Tracks statistics (total runs, average run length, max delta magnitude)

## Delta Encoding Format

```cpp
struct Delta {
    unsigned short start;  // vertex index of run start (offset 0)
    unsigned short count;  // number of vertices in run (offset 2)
    // Followed by count * 3 bytes of quantized X, Y, Z deltas
    // Layout: rec[4+0], rec[4+1], rec[4+2] = qx, qy, qz of vertex 0
    //         rec[7], rec[8], rec[9] = qx, qy, qz of vertex 1, etc.
};
```

**Memory layout**:
- Byte 0-1: `start` (unsigned short)
- Byte 2-3: `count` (unsigned short)
- Byte 4+: `count * 3` bytes of quantized deltas (signed char per component)

**Total record size**: `count * 3 + 4` bytes

## Quantization Algorithm

### Float → Signed Byte Conversion

Each float component (dx, dy, dz) undergoes:

1. **Clamping** to [-2.0f, +2.0f]:
   ```cpp
   const float maxClamp = 2.0f;
   const float minClamp = -2.0f;

   float dx = pv.x - bv.x;
   if (dx > maxClamp) dx = maxClamp;
   else if (dx < minClamp) dx = minClamp;
   ```

2. **Quantization** using scale factor **63.5**:
   ```cpp
   signed char qx = (int)(63.5 * (double)dx + 0.5);
   ```

**Why 63.5?**
- Converts [-2.0, +2.0] to approximately [-127, +127] range (signed byte bounds)
- Scale: 2.0 * 63.5 = 127.0 (nearly saturates positive byte range)
- Inverse: `float = qx / 63.5` recovers approximate delta
- The +0.5 implements banker's rounding (nearest integer)

**Precision**:
- Quantization error ≈ 1/63.5 ≈ ±0.0157 units (in original float space)
- Example: delta=0.5 → qx≈31.75→32 → recovers ≈0.504

### Zero-Delta Detection

A vertex is considered "zero-delta" if all three components quantize to 0:
```cpp
int nonZero = 0;
if ((signed char)qx != 0 || (signed char)qy != 0 || (signed char)qz != 0) {
    nonZero = 1;
}
```

Note: The cast `(signed char)qx` treats the intermediate `int` result as signed. This is critical—the quantization produces a `signed char` result directly via casting, not a separate zero-check on the float.

## RLE Encoding Strategy

The function uses a two-pass outer loop:

### First Loop: Skip Leading Zero-Delta Vertices
```cpp
while (start < pos.size()) {
    // Quantize vertex at index 'start'
    // If all components == 0, increment start and continue
    // Otherwise, break and proceed to extend run
}
```

### Second Loop: Extend Run of Non-Zero Vertices
```cpp
end = start + 1;
while (end < pos.size()) {
    // Quantize vertex at index 'end'
    // If any component != 0, increment end and continue
    // Otherwise, break (end of run)
}
```

**Result**: Runs contain contiguous non-zero-delta vertices; zero-delta regions are skipped entirely.

### Third Loop: Write Run Data
```cpp
unsigned short count = end - start;
char *rec = (char *)MemResizeElem(mData, mSize, ..., count * 3 + 4, ...);

*(unsigned short *)(rec + 0) = start;      // header[0:2]
*(unsigned short *)(rec + 2) = count;      // header[2:4]

// Write quantized deltas
for (unsigned short vi = start; vi < end; vi++) {
    int recOff = (vi - start) * 3;
    // Quantize and write rec[recOff + 4..6]
}
```

## Register & Stack Frame Issues

### Stack Frame Mismatch
- **Target**: Uses 0xd0 (208) bytes for frame
- **Base**: Uses 0xa0 (160) bytes for frame
- **Delta**: -48 bytes (29 instructions affected)

This explains many `diff_arg` mismatches on `stwu`, `stw`, `stfd`, `psq_st` instructions at function prologue.

### Callee-Saved Register Allocation
- **Target**: Saves `r20–r31` (via `_savegpr_20`)
- **Base**: Saves `r22–r31` (via `_savegpr_22`)
- **Implication**: Target uses `r20`, `r21` whereas base doesn't; affects parameter/local assignments

### Floating-Point Register Swaps (186 instructions)
Major pairs:
- `f28 ↔ f29`: 36 instructions (dominant)
- `f1 ↔ f2`: 30 instructions
- `f2 ↔ f3`: 20 instructions

These are likely due to:
1. Variable declaration order (affects compiler's register allocation)
2. Loop-carried dependencies causing different assignment strategies
3. FPU operand ordering in quantization sequences

## Control Flow Difference (1 opcode mismatch)

**Index 348**: `bdnz` vs `bne`
- **Target**: `bdnz 0x4f4` (branch if count register non-zero, decrement)
- **Base**: `bne 0x4a8` (branch if equal result non-zero)

This is a loop counter inversion:
- Target likely uses `ctr` register in `do-while(ctr != 0)` loop
- Base likely uses `cmpwi r4, 0x0; bne` condition

Suggests the inner-loop iteration pattern differs between implementations.

## Offset & Address Relocation Noise

### Offset Swaps (5 instructions)
Pairs of offsets (0x4, 0x8) are swapped in target vs. base—likely due to struct field reordering or stack slot layout differences.

### Address Relocation Noise (17 instructions, 1 lis/addi pair)
Static variable addresses (e.g., `totalRuns`, `totalLength`, `maxDelta`) relocate with different `lis` prefixes and `addi` immediates. Unfixable—due to linker/layout choices.

## m2c Pseudocode

From the target assembly (67.1% match), the approximate C++ translation is:

```cpp
void BandFaceDeform::DeltaArray::AppendDeltas(
    const std::vector<Vector3> &pos,
    const std::vector<Vector3> &base
) {
    // Validation
    if (pos.size() != base.size()) {
        MILO_FAIL("AppendDeltas pos has %d points, base has %d",
                  pos.size(), base.size());
    }

    // Static counters for debug output
    static int total = 0;
    static int totalRuns = 0;
    static int totalLength = 0;
    static float maxDelta = 0.0f;

    const float maxClamp = 2.0f;
    const float minClamp = -2.0f;

    unsigned short start = 0;
    unsigned short end = 0;

    while (end < pos.size()) {
        // Phase 1: Skip leading zero-delta vertices
        while (start < pos.size()) {
            const Vector3 &pv = pos[start];
            const Vector3 &bv = base[start];

            float deltax = pv.x - bv.x;
            float deltay = pv.y - bv.y;
            float deltaz = pv.z - bv.z;

            // Clamp each component
            float dx = (float)deltax;
            if (dx > maxClamp) dx = maxClamp;
            else if (dx < minClamp) dx = minClamp;

            float dy = deltay;
            if (dy > maxClamp) dy = maxClamp;
            else if (dy < minClamp) dy = minClamp;

            float dz = deltaz;
            if (dz > maxClamp) dz = maxClamp;
            else if (dz < minClamp) dz = minClamp;

            // Quantize with 63.5 scale and banker's rounding
            signed char qx = (int)(63.5 * (double)dx + 0.5);
            signed char qy = (int)(63.5 * (double)dy + 0.5);
            signed char qz = (int)(63.5 * (double)dz + 0.5);

            // Check if any component is non-zero
            int nonZero = 0;
            if ((signed char)qx != 0 ||
                (signed char)qy != 0 ||
                (signed char)qz != 0) {
                nonZero = 1;
            }

            if (nonZero == 0) {
                start++;
                continue;
            }
            break;  // Found first non-zero vertex
        }

        // Phase 2: Extend run while deltas are non-zero
        end = start + 1;
        while (end < pos.size()) {
            const Vector3 &pv = pos[end];
            const Vector3 &bv = base[end];

            float deltax = pv.x - bv.x;
            float deltay = pv.y - bv.y;
            float deltaz = pv.z - bv.z;

            float dx = (float)deltax;
            if (dx > maxClamp) dx = maxClamp;
            else if (dx < minClamp) dx = minClamp;

            float dy = deltay;
            if (dy > maxClamp) dy = maxClamp;
            else if (dy < minClamp) dy = minClamp;

            float dz = deltaz;
            if (dz > maxClamp) dz = maxClamp;
            else if (dz < minClamp) dz = minClamp;

            signed char qx = (int)(63.5 * (double)dx + 0.5);
            signed char qy = (int)(63.5 * (double)dy + 0.5);
            signed char qz = (int)(63.5 * (double)dz + 0.5);

            int nonZero = 0;
            if ((signed char)qx != 0 ||
                (signed char)qy != 0 ||
                (signed char)qz != 0) {
                nonZero = 1;
            }

            if (nonZero != 0) {
                end++;
                continue;
            }
            break;  // End of run
        }

        // Phase 3: Write the run record
        if (start < pos.size()) {
            unsigned short count = end - start;

            // Allocate or extend storage
            char *rec = (char *)MemResizeElem(
                mData, mSize, (char *)mData + mSize,
                0, count * 3 + 4, "BandFaceDeform.cpp"
            );

            // Write header
            *(unsigned short *)(rec + 0) = start;
            *(unsigned short *)(rec + 2) = count;

            // Write quantized delta bytes
            for (unsigned short vi = start; vi < end; vi++) {
                const Vector3 &pv = pos[vi];
                const Vector3 &bv = base[vi];
                int recOff = (vi - start) * 3;

                float deltax = pv.x - bv.x;
                float deltay = pv.y - bv.y;
                float deltaz = pv.z - bv.z;

                float dx = (float)deltax;
                if (dx > maxClamp) dx = maxClamp;
                else if (dx < minClamp) dx = minClamp;
                rec[recOff + 4] = (signed char)(int)(63.5 * (double)dx + 0.5);

                float dy = deltay;
                if (dy > maxClamp) dy = maxClamp;
                else if (dy < minClamp) dy = minClamp;
                rec[recOff + 5] = (signed char)(int)(63.5 * (double)dy + 0.5);

                float dz = deltaz;
                if (dz > maxClamp) dz = maxClamp;
                else if (dz < minClamp) dz = minClamp;
                rec[recOff + 6] = (signed char)(int)(63.5 * (double)dz + 0.5);

                // Track max delta magnitude
                float absx = std::fabs(pv.x - bv.x);
                if (maxDelta < absx) {
                    maxDelta = absx;
                }
                float absy = std::fabs(pv.y - bv.y);
                if (maxDelta < absy) {
                    maxDelta = absy;
                }
                float absz = std::fabs(pv.z - bv.z);
                if (maxDelta < absz) {
                    maxDelta = absz;
                }
            }

            // Debug output
            unsigned short recCount = *(unsigned short *)(rec + 2);
            TheDebug << MakeString(
                "   run from %d to %d waste %g \n",
                (int)start, (int)end,
                4.0f / (float)(recCount * 3 + 4)
            );

            totalRuns++;
            totalLength += count;
        }

        start = end;
    }

    // Final debug output
    int sz = mSize;
    total += sz;
    TheDebug << MakeString(
        "   is size %d total %d av runlength %g totalWaste %d md %g\n",
        sz, total,
        (float)totalLength / (float)totalRuns,
        totalRuns * 4,
        maxDelta
    );
}
```

## Key Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| `maxClamp` | 2.0f | Upper bound for float delta clamping |
| `minClamp` | -2.0f | Lower bound for float delta clamping |
| **Quantization scale** | 63.5 | Converts [-2.0, +2.0] to signed byte range |
| **Header size** | 4 bytes | Start vertex (2) + count (2) |
| **Delta size per vertex** | 3 bytes | Quantized X, Y, Z |

## Known Issues & Proposed Fixes

### Issue 1: Register Allocation (186 swaps, 43% of diff_arg)

**Symptoms**:
- FPR swaps (f28↔f29, f1↔f2, f2↔f3) dominate
- GPR swaps (r3↔r5, r4↔r5) secondary

**Root Cause**: Variable declaration order affects CodeWarrior's register allocation.

**Proposed Fix**:
1. Reorder local float variables to match expected allocation:
   - Declare quantization temporaries (dx, dy, dz) first
   - Then declare loop counters (start, end, vi)
   - Then declare component accumulation registers

2. Use explicit register hints if available (non-portable):
   ```cpp
   register float dx asm("f28");
   register float dy asm("f29");
   ```
   (Unlikely to work with MetroWorks without pragma support)

### Issue 2: Stack Frame Size (-48 bytes, 29 diff_arg)

**Symptoms**: All prologue stack saves use different offsets (stwu, stfd, psq_st).

**Root Cause**: Target uses more callee-saved GPRs (r20, r21) than base.

**Proposed Fix**:
1. Check if `r20`/`r21` are genuinely needed. If not, avoid using them.
2. If needed, ensure they are saved/restored consistently.
3. Consider if the implementation can use fewer callee-saved regs overall.

### Issue 3: Loop Counter Expression (1 opcode, bdnz vs bne)

**Symptoms**: Index 348 has `bdnz` (Target) vs `bne` (Base)

**Root Cause**: Different loop idiom:
- Target: `do { ... } while(ctr != 0)` with `bdnz` (decrement and branch on non-zero)
- Base: `while(ctr != 0) { ... }` with separate `cmpwi; bne`

**Proposed Fix**:
Ensure loop counter logic matches. The current implementation uses:
```cpp
unsigned short ctr = count;
// ...
do {
    // body
    ctr--;
} while (ctr != 0);
```

Consider rewriting as a for-loop or matching the exact loop structure of the target assembly.

### Issue 4: Quantization Temporary Storage (Multiple diff_arg in inner loops)

**Symptoms**: `stfs` / `lfs` instructions for temporary storage at stack offsets [0x18, 0x28, 0x2c]

**Root Cause**: Compiler spilling quantized component values to stack, then reloading for comparisons.

**Proposed Fix**:
1. Keep quantized values in registers across the comparison:
   ```cpp
   signed char qx = ..., qy = ..., qz = ...;
   if (qx != 0 || qy != 0 || qz != 0) { ... }  // no intermediate store
   ```

2. Avoid loading/comparing float again; just check the quantized byte.

## Uncertainties & Research Questions

1. **Quantization Round-Trip Error**: The +0.5 in `(int)(63.5 * (double)dx + 0.5)` is banker's rounding. Verify this matches the target assembly exactly (may need to check FPU rounding mode).

2. **Static Variable Initialization**: Are `total`, `totalRuns`, `totalLength`, `maxDelta` initialized to 0 globally, or do they have different initial values in the original binary? This affects the first invocation's output.

3. **MemResizeElem Semantics**: The function calls `MemResizeElem(mData, mSize, (char *)mData + mSize, 0, count * 3 + 4, ...)`. The second parameter is updated in-place. Verify that `mSize` is correctly incremented after each call.

4. **MILO_FAIL Behavior**: Does `MILO_FAIL()` return or throw? If it throws, exception handling could affect register usage.

5. **TheDebug Stream**: Does `TheDebug << MakeString(...)` allocate temporaries that affect register allocation?

6. **Floating-Point Comparison Semantics**: The clamping uses `>` and `<` on floats; no NaN handling. Assume all inputs are finite.

## Summary of Match Degradation

- **Equal**: 98 (22.6%)
- **diff_arg** (same opcode, different arguments): 194 (44.8%)
  - Offset shifts: 72
  - Register swaps: 231 (overlapping)
  - Address relocation: 7
  - Branch dests: 5
  - Unexplained: 6
- **delete**: 60 (13.9%) — removed instructions (spill elimination, opt)
- **insert**: 30 (6.9%) — added instructions
- **replace**: 48 (11.1%) — opcode changes
- **diff_op**: 3 (0.7%) — opcode mismatches (bdnz↔bne, addi↔subi, slwi↔clrlwi)

## Recommendation for Next Steps

1. **Investigate control flow** at index 348 (bdnz vs bne)—this is the primary opcode mismatch.
2. **Reorder variable declarations** to influence register allocation (f28/f29 swaps are very common).
3. **Minimize stack frame usage** by removing unnecessary temporary stores.
4. **Verify loop idiom** matches target (do-while with counter vs while with condition).
5. **Profile quantization path** to ensure no extra registers are allocated for intermediate computations.

The function is LIKELY_FIXABLE if the above structural issues are addressed. The dominant issues are compiler-level (register allocation, stack frame) rather than algorithmic.
