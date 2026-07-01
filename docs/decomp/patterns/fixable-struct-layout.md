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

**Tooling:** `bin/lint-link-issues` statically detects this pattern (and the multiply-defined globals one) against the band_r_wii.map symbol table. Run it before promoting a TU to `Matching`/`Equivalent`. Underlying primitive: `tools/mwcc_symbols.py find <Class> <Method>` queries the map for override symbols across const/non-const, namespace-nested, and template-instantiated variants.

## Redundant Non-Virtual Member Redeclaration (Name-Hiding) — the "phantom stub"

**The non-virtual sibling of the section above.** Same root cause (a derived class re-declares an inherited member it never actually re-implemented), different — and *sneakier* — failure mode.

**Surfaces as:** on the Wii decomp, an `undefined: 'Derived::Foo(...)'` link error referenced from a *call site* (not the vtable). On the **native/web port** it surfaces as *nothing at all at build time* — the undefined symbol resolves to a silent no-op link stub (`native/src/band3_link_stubs.s` weak `.set ..., __hmx_band3_noop_stub`; or an Emscripten abort-stub on web) that returns garbage. The bug only shows up as wrong runtime behavior.

**Pattern:**

```cpp
// Base.h
class Base {
public:
    Symbol NameToFoo(const char *);   // declared
};
// Base.cpp
Symbol Base::NameToFoo(const char *name) { /* the REAL implementation */ }

// Derived.h  --- BUG: re-declares the inherited NON-virtual method
class Derived : public Base {
public:
    static Symbol NameToFoo(const char *);  // <-- redundant: no body anywhere
};

// Derived.cpp
void Derived::DoThing(const char *cc) {
    mFoo = NameToFoo(cc);   // unqualified call — C++ name-hiding picks
                            // Derived::NameToFoo (the orphan), NOT Base::NameToFoo
}
```

Because the derived class re-declares `NameToFoo`, C++ **name-hiding** makes the inherited `Base::NameToFoo` invisible to unqualified lookup inside `Derived`. The call resolves to `Derived::NameToFoo`, which has no body → undefined symbol with the derived class's mangling.

Why it's worse than the virtual variant:
- **No vtable reference**, so the symbol is only emitted if a call site actually names it. A redeclaration with *no* call site is silently harmless — until someone adds a call.
- **`objdiff` is blind to it** (function-level matching is clean; it's a link/lookup concern).
- On the port it never link-errors — `-sERROR_ON_UNDEFINED_SYMBOLS=0` + the weak stub table swallow it. The reference bug (`BandCharacter::NameToDrumVenue`, rb3 `727cf01e`) produced drum-kit paths like `char/main/drum/gen/goth_gold_(null).milo_xbox` → 404 → the drummer's kit silently never loaded.

**Verify before fixing** (same as the virtual case, plus a name-hiding check):
- Grep `orig/SZBE69_B8/files/band_r_wii.map` for the *derived* MWCC symbol (`NameToFoo__<len><Derived>F...`). If it is **absent** but the *base* symbol (`NameToFoo__<len><Base>F...`) is **present**, the original never had a derived override — the declaration is bogus. (`scripts/analysis/phantom_stub_scan.py` automates exactly this `map[D=False B=True]` check.)
- Confirm no `.cpp` defines `Derived::NameToFoo`.
- Confirm there is at least one unqualified call site (otherwise it's latent, not active).

**Fix:** delete the redeclaration line from the derived header. The unqualified call now resolves through inheritance to `Base::NameToFoo`. This is **match-positive** — removing the orphan makes the calling function's relocation match the target (e.g. `SetTempoGenreVenue` 0 → 100%). Also drop the now-unreferenced weak stub from `native/src/band3_link_stubs.s`.

**Triage rule for `band3_link_stubs.s` / `dta_link_stubs.s` / `rndobj_synth_link_stubs.s`:** before treating a stubbed C++ method as "function not yet implemented," demangle it (`c++filt`) and check whether the *same method* is defined on a **base** class. If the derived mangling is absent from the Wii map while the base's is present, it's a phantom — delete the decl, don't implement a stub.

**Tooling:** `scripts/analysis/phantom_stub_scan.py` (`--mode stubs` ranks ACTIVE native-stub phantoms; `--mode headers` finds latent ones across any src tree, RB3 or DC3) and the `ORPHAN_NONVIRTUAL` check in `bin/lint-link-issues`.

## See Also

- [fixable-copy-ctor.md](fixable-copy-ctor.md) — explicit copy constructor disables small-struct register-return ABI
