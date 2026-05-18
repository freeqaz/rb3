# Fixable: Explicit Copy Constructor

## Removing Explicit Copy Constructor Enables Register-Return ABI

If a small struct (≤8 bytes) has a user-defined copy constructor, CW uses the hidden-pointer ABI (`r3` = hidden pointer, `r4` = `this`). Removing the redundant explicit copy constructor lets CW use small-struct register-return instead.

**Example:** Removing `Vector2(const Vector2&)` from `Vec.h` fixed `CamShotFrame::MaxAngularOffset` and unblocked `Spotlight::NGRadii` — both needed register-return ABI for `Vector2`.

## When to Apply

Look for functions returning `Vector2`, `Vector3`, or other small POD-like structs where the diff shows:

- Target uses small-struct return ABI (return value in `f1`/`r3` directly).
- Our build uses sret ABI (extra hidden pointer in `r3`, real args shifted).

The explicit copy constructor declaration in the header is the usual culprit — once removed, CW's implicit copy ctor (which is bodyless and ABI-friendly) takes over.

## TU-Local Inline Hmx::Object Copy Constructor

When a Vector_impl template instantiation (op=, fill_insert_aux, insert_overflow_aux) is ~50-70% and the diff shows ~190 extra instructions inline at the copy ctor site, the target inlined `Hmx::Object::Object(const Hmx::Object&)` while our build emits `bl __ct__Q23Hmx6ObjectFRCQ23Hmx6Object`.

Inlining the copy ctor globally in `Object.h` affects 383 files — too risky. **Define the copy ctor inline at file scope in just the affected `.cpp`:**

```cpp
// In PatchDir.cpp / SongSortMgr.cpp / Sfx.cpp / etc., AT FILE SCOPE before any usage:
inline Hmx::Object::Object(const Hmx::Object& o)
    : RootObject(o) {
    /* match Object.h's normal body */
}
```

The TU's templates see the inline definition and inline the copy ctor at call sites; other TUs still see the header declaration and emit `bl`.

**Wins:**
- `SongSortMgr::BuildSetlistList` 50.65% → 99.89% (commit 7d77c27f)
- `MoggClipMap` Vector_impl op= 67.9% → 95.75%, with bonus `_M_insert_overflow_aux` to 99.98% (commit 579ce5f1)
- `PatchDir` Vector_impl pair pushed up to 98.7% / 99.2%
- Pattern applies anywhere CW would inline the synthesized copy ctor for a type that inherits from `Hmx::Object`.

## Member Function Pointer to Force Out-of-Line Emission

When one TU needs to inline a method while a cross-TU caller needs `bl` to it, use this 3-part trick:

1. **Header**: Declare method only — no body, no `inline`.
2. **Implementation .cpp**: Define as `inline void Class::Method(...) { ... }` at file scope.
3. **Force standalone symbol**: `DECOMP_FORCEBLOCK(&Class::Method);` somewhere in the same .cpp.

Taking the method's address forces the linker symbol to exist. The inline-defining TU emits both the inlined call sites AND a real symbol body for cross-TU callers.

**Example:** `SystemComponent::SetParent` simultaneously inline (in `~SystemComponent`) and out-of-line (callable from `SystemComponentGroup::RegisterComponent`). Result: SetParent + ~SystemComponent + RegisterComponent all at 100% (commit 2a55df50).
