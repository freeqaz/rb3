# Fixable: Struct & Vtable Layout

Patterns where class layout (vtable shape, member offsets) drives codegen.

## virtual vs Non-virtual Affects Vtable Layout

Adding or removing `virtual` from a method changes the vtable layout, affecting all code that does virtual dispatch on that class.

**Example:** In `Rnd`, making `TestPoint` non-virtual fixed a 4-byte vtable offset that was breaking `EndWorld`.

## See Also

- [fixable-copy-ctor.md](fixable-copy-ctor.md) — explicit copy constructor disables small-struct register-return ABI
