# Fixable: Operators & Argument Order

Patterns around operator overload selection, commutative operations, and argument evaluation.

## CW Evaluates Function Arguments Right-to-Left

When calling a function with multiple arguments, CW evaluates the rightmost argument first. This affects which vtable calls happen in which order.

```cpp
// CW evaluates MinBlur() first, then MaxBlur(), then BlurDepth()
TheDOFProc->Set(BlurDepth(), MaxBlur(), MinBlur());
```

**Example:** In `FreeCamera::Poll`, reordering DOFProc::Set arguments to `BlurDepth(), MaxBlur(), MinBlur()` matched the target's vtable dispatch order.

## Operand Order in Commutative Operations

`a * b` and `b * a` can generate different register assignments for `fmuls`. Match the target's operand order.

**Example:** In `PatchPanel::Poll`, `unk60 * mScaleVelX` vs `mScaleVelX * unk60` fixed an OFFSET_SWAP.

## Direct .Set() vs Constructor Assignment

`vec.Set(x, y, z)` avoids a temporary on the stack that constructor assignment `vec = Vector3(x, y, z)` creates.

**Example:** In `CharIKFingers`, `.Set(0.3f, -6.0f, 0.4f)` instead of `= Vector3(...)` fixed the stack layout.

## std::max with Literal First Arg Creates Anonymous Static

`std::max(0.0f, expr)` with the literal as the first `const float&` argument causes CW to allocate an anonymous static for the literal and generate a pointer-select pattern (compare, then load via pointer to either the static or a stack spill).

**Example:** In `Player::SubtractEnergy`, `SetEnergy(std::max(0.0f, mBandEnergy - f))` generated the correct stack-spill + pointer-select pattern with full stack frame, while a ternary `(x > 0 ? x : 0)` generated different codegen.
