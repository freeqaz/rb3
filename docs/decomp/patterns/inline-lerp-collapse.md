# Pattern: inline_lerp_collapse

**Permuter pattern**: `inline_lerp_collapse`
**File**: `decomp_synth/patterns/inline_lerp_collapse.py`
**Status**: ported from MEMORY (`feedback_inline_lerp_no_intermediate`); detection validated against a synthetic candidate. No in-tree candidates remain — the two known wins were already applied in commit `e1aea8bf`.

## What it does

Collapse N parallel per-field float lerps written in **split form** (with
intermediate `dN` / `rN` locals) into **inline form** (one expression per
field).

**Before** (split form — confuses MWCC's scheduler):

```cpp
float dX = e.x - tmpX; float rX = f * dX + tmpX; dst.x = rX;
float dY = e.y - tmpY; float rY = f * dY + tmpY; dst.y = rY;
float dZ = e.z - tmpZ; float rZ = f * dZ + tmpZ; dst.z = rZ;
```

**After** (inline form — matches target leaf-function shape):

```cpp
dst.x = f * (e.x - tmpX) + tmpX;
dst.y = f * (e.y - tmpY) + tmpY;
dst.z = f * (e.z - tmpZ) + tmpZ;
```

## Why it works

With separate `d*` and `*Res` locals, MWCC's IPA decides each is live across
other field operations and schedules loads interleaved (cross-field). The
target's leaf function does each lerp's load/compute/store in one contiguous
block per field. The inline form lets MWCC use the same uniform pipeline.

## Detection (AST)

The pattern matches sequences of **3 consecutive statements**:

1. `T local_d = <binary_expression>;` — typically `field - tmp`
2. `T local_r = factor * local_d + addend;` — fused multiply-add
3. `<lvalue> = local_r;` — store

Where:
- `local_d` is used **only** in (1) and (2)
- `local_r` is used **only** in (2) and (3)
- Contiguous triples sharing the same `factor` expression are grouped

Each group of ≥ 2 triples becomes one variant. Groups of ≥ 3 also emit a
partial-collapse variant (first N-1 triples) for cases where full collapse
over-shoots.

## Detection (asm) — relevant gate

Returns `True` when any of:
- An `fmuls`/`fadds`/`fmadds`/`fsubs`/`fmsubs` mismatch appears in `diff_ops`
- A callee-saved FPR appears in any `reg_swap_pairs` entry
- `fpr_save_delta != 0` (prologue FPR-save mismatch)

Otherwise still `True` (the AST gate of ≥ 2 triples is itself strong).

## Real wins (historical)

| Function | Before | After | Source commit |
|---|---|---|---|
| `LightPreset::SpotlightDrawerEntry::Animate` | 83.8% | 100% | `e1aea8bf` |
| `LightPreset::AnimateSpotlightDrawerFromPreset` | 95.2% | 98.8% | `e1aea8bf` |

Both fixes were applied by hand before the pattern existed; the pattern is
documented for future leaf animate functions that may be encountered.

## See also

- `temp_elimination` — single-local elimination (sibling)
- `variable_inline` — single-assignment local inline (sibling)
- `fma_reorder` — operand reordering for FMA instructions
- `docs/decomp/patterns/fixable-fsel-fma.md` — broader float-scheduling notes
