# Fixable: Explicit Copy Constructor

## Removing Explicit Copy Constructor Enables Register-Return ABI

If a small struct (≤8 bytes) has a user-defined copy constructor, CW uses the hidden-pointer ABI (`r3` = hidden pointer, `r4` = `this`). Removing the redundant explicit copy constructor lets CW use small-struct register-return instead.

**Example:** Removing `Vector2(const Vector2&)` from `Vec.h` fixed `CamShotFrame::MaxAngularOffset` and unblocked `Spotlight::NGRadii` — both needed register-return ABI for `Vector2`.

## When to Apply

Look for functions returning `Vector2`, `Vector3`, or other small POD-like structs where the diff shows:

- Target uses small-struct return ABI (return value in `f1`/`r3` directly).
- Our build uses sret ABI (extra hidden pointer in `r3`, real args shifted).

The explicit copy constructor declaration in the header is the usual culprit — once removed, CW's implicit copy ctor (which is bodyless and ABI-friendly) takes over.
