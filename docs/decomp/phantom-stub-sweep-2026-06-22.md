# Phantom Link-Stub Sweep — 2026-06-22

Cross-repo sweep for the "phantom link-stub" bug class, triggered by the
`BandCharacter::NameToDrumVenue` fix (rb3 `727cf01e`). Covers RB3 (`rb3/`) and
the shared-engine DC3 decomp (`../dc3-decomp/`).

## The bug class

A **phantom stub** is an undefined symbol that exists only because a derived
class **re-declares** a member function that a base class already declares **and
implements**. C++ name-hiding makes an unqualified call inside the derived class
bind to the body-less *derived* declaration → undefined symbol with the derived
mangling.

Two variants, same root cause (redundant derived re-declaration of an inherited
member), different failure mode:

| Variant | How it breaks | Detector |
|---|---|---|
| **Virtual** (`virtual Foo();` re-declared) | derived vtable references `Derived::Foo` unconditionally → **link error** at vtable emission (even with no call site) | `bin/lint-link-issues` → `ORPHAN_VIRTUAL` (pre-existing) |
| **Non-virtual** (name-hiding) | only the *call site* references `Derived::Foo`; on Wii it link-errors, but on the **native/web port** it resolves to a **silent no-op stub returning garbage** | `bin/lint-link-issues` → `ORPHAN_NONVIRTUAL` (**new, this sweep**) |

The non-virtual variant is the sneaky one: `objdiff` is blind to it, it never
link-errors on the port (`-sERROR_ON_UNDEFINED_SYMBOLS=0` + the weak-stub tables
swallow it), and it only shows up as wrong runtime behavior. The reference bug
produced drum-kit paths like `char/main/drum/gen/goth_gold_(null).milo_xbox` →
404 → the drummer's kit silently never loaded.

Full pattern writeup: [patterns/fixable-struct-layout.md](patterns/fixable-struct-layout.md).

## Detection — the decisive signal

For a suspected `Derived::method`:

1. `Derived`'s header **declares** `method` (member decl, not a definition).
2. A transitive **base** `B` **defines** `method` out-of-line (`B::method` in a `.cpp`).
3. `Derived` does **not** define `method` itself.
4. **(ground truth)** the MWCC symbol `method__<mangled Derived>` is **absent**
   from the Wii map while `method__<mangled B>` is **present**.

Check (4) is the discriminator that separates a real phantom from a genuine
"function not yet implemented." Validated end-to-end on the reference bug:
`find_override('BandCharacter','NameToDrumVenue')` → `[]` (empty), while
`find_override('BandCharDesc','NameToDrumVenue')` → the real `BandCharDesc.o`
symbol.

## Tooling produced

- **`bin/lint-link-issues` → new `ORPHAN_NONVIRTUAL` check** (in
  `tools/lint_link_issues.py`). The non-virtual sibling of `ORPHAN_VIRTUAL`,
  reusing the same map + impl-index machinery plus a new inheritance-graph
  parser. Brace-matched class attribution (no cross-class contamination), gated
  on map-absence + base-present for precision. Scope-filtered (skips
  `network/`, `sdk/`, `system/rndwii/`, `system/os/`, etc.). Run before
  promoting a TU. `bin/lint-link-issues --selftest` locks in the regression.
- **`scripts/analysis/phantom_stub_scan.py`** — the broad cross-repo net.
  `--mode stubs` ranks the *active* native-stub phantoms (RB3); `--mode headers`
  finds latent ones across any `--src` tree (RB3 or DC3, `--map` optional). Uses
  a one-pass impl-index (≈2–6 s per repo). Coarser than the linter (it
  over-matches macro/inline/virtual decls), so its output is an agent-verified
  candidate list, not a verdict.

## Methodology

Mechanical scan → ground-truth verification by fan-out (ultracode workflow):
each candidate gets one agent that reads the *real* source (header decl,
virtual-ness, inline/macro body, `.cpp` definitions, Wii-map symbols, call
sites) and classifies it; any `genuine_phantom` then faces an independent
skeptic prompted to refute it. Only survivors count.

## Results

### RB3 — `bin/lint-link-issues --check nonvirtuals`

- In-scope **and** whole-tree (`--all`): **0** `ORPHAN_NONVIRTUAL` (after the
  `NameToDrumVenue` fix). Map gate validated against the known bug.
- Adversarial cross-check: the broad scanner's 32 in-scope non-virtual
  candidates were each verified against ground truth — **0 genuine phantoms**.
  Breakdown: 19 macro/inline (`REGISTER_OBJ_FACTORY_FUNC` → `static Register()`,
  inline `back()`), 5 not-a-method (scanner matched call sites like
  `da->AddRef()`), 8 virtual-variant (already covered by `ORPHAN_VIRTUAL`). The
  one `very_high` (`MidiSectionLister::SetMidiReader`) was a scanner error — the
  class has no such declaration; it only *calls* the inherited inline-virtual
  `MidiReceiver::SetMidiReader`, which binds correctly.
- **Conclusion:** `BandCharacter::NameToDrumVenue` was the only non-virtual
  phantom in RB3. The linter now guards against regressions.

### DC3 — `phantom_stub_scan.py --mode headers` + fan-out verify

DC3's native port links with `-undefined dynamic_lookup` + generated C-symbol
stubs (`engine_stubs_generated.cpp`), so a C++ phantom there fails at *runtime*,
not link — the header-driven detector is the right tool (its map is MSVC-mangled,
so the map gate is off; candidates are confidence=`high`).

- 3 non-virtual candidates, **0 genuine phantoms** (each fan-out-verified):
  - `DxRnd::BeginDrawing` — actually `virtual` (vtable dispatch, not name-hiding)
    **and** out-of-scope (`rnddx9/` D3D9 renderer, replaced by the port).
  - `Flow::SetName` — scanner false match: the `SetName` belongs to the *nested*
    struct `Flow::DynamicPropertyEntry` (fully implemented at `Flow.cpp:572`);
    `Flow` itself has no such decl and correctly inherits `Object::SetName`.
  - `basic_ios::imbue` — `stlport/` (out of scope), and it has a real
    out-of-line body (`_ios.c:68`) — intentional name-hide-and-*extend*, the
    opposite of a phantom.
- **Conclusion:** no phantom stubs in DC3's in-scope code; the shared Milo engine
  is clean of this pattern.

## Takeaways

- The non-virtual phantom is a **port-specific silent failure mode** the existing
  link linter didn't cover; it now does.
- When triaging an entry in `band3_link_stubs.s` / `dta_link_stubs.s` /
  `rndobj_synth_link_stubs.s`, demangle it and check whether the same method is
  implemented on a **base** class before treating it as "not yet implemented."
  If the derived mangling is absent from the Wii map but the base's is present,
  delete the declaration — don't write a stub.
