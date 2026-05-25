# Fixable: Struct & Vtable Layout

Patterns where class layout (vtable shape, member offsets) drives codegen.

## virtual vs Non-virtual Affects Vtable Layout

Adding or removing `virtual` from a method changes the vtable layout, affecting all code that does virtual dispatch on that class.

**Example:** In `Rnd`, making `TestPoint` non-virtual fixed a 4-byte vtable offset that was breaking `EndWorld`.

## Redundant Virtual Override Declaration (Link-Time)

**Surfaces as:** `undefined: 'Derived::Foo(Arg)' Referenced from 'Derived::__vt' in <DerivedTU>.o` when promoting `<DerivedTU>` to link from source.

**Pattern:** the derived class's header re-declares an inherited virtual method as if it were an override:

```cpp
// Base.h
class Base {
public:
    virtual void Foo(Arg) { /* inline body */ }
};

// Derived.h  --- BUG: declares an override that doesn't exist
class Derived : public Base {
public:
    virtual void Foo(Arg);  // <-- redundant: no body anywhere in source
};
```

The original game's `Derived` did NOT override `Foo` — its vtable slot inherits `Base::Foo`. But because the derived class re-declares `Foo` as virtual, MWCC emits the derived's vtable referencing `Derived::Foo` as a separate symbol. There is no body → link fails.

`objdiff` cannot detect this — function-level matching is clean, and the missing symbol only surfaces at the link step. So a unit can sit at fuzzy ~99% with all functions cosmetic, get promoted to `Equivalent`, and still fail to link with this signature.

**Verify before fixing:**
- Look at the target binary's `__vt__<Derived>` (Ghidra MCP, `bin/analyze-function __vt__<DerivedMangled>`, or grep `orig/SZBE69_B8/files/band_r_wii.map`). If slot N's symbol is `Base::Foo` rather than `Derived::Foo`, the original never overrode it.
- Check the base class header — if `Base::Foo` has an inline body (or out-of-line definition with extern linkage), inheriting it is the intended behavior.

**Fix:** delete the `virtual void Foo(Arg);` line from the derived class header. The vtable now correctly inherits from base. No source body needed, no other change required.

**Cascade unblock:** removing a redundant decl in a *parent* class can unblock multiple derived TUs. Example: `StartTransitionMsg` had a redundant `virtual void Dispatch();`; removing it unblocked `BandUI` (which instantiates `NetPushScreenMsg` and `NetPopScreenMsg`, both transitively inheriting through `StartTransitionMsg`).

**Distinct from** multiply-defined globals (file-local sentinel needs `static`) and key-function vtable placement (covered in the linking-sweep memory note).

## See Also

- [fixable-copy-ctor.md](fixable-copy-ctor.md) — explicit copy constructor disables small-struct register-return ABI
