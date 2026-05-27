# Fixable: Inline Boundary

MWCC / RB3 version of DC3's `fixable-inline-boundary.md` — adapted from `/home/free/code/milohax/dc3-decomp/docs/decomp/patterns/fixable-inline-boundary.md`.

Patterns where moving a function definition between an inline location (header) and an out-of-line location (.cpp) changes downstream codegen — sometimes by eliminating a `bl` (when MWCC inlines), sometimes by *adding* one (when the function is no longer visible to the inliner), and sometimes by triggering an ICF merge that influences register allocation in unrelated callers.

MWCC's three relevant knobs:

- `-inline noauto` (game code in `band3/`, `network/`, `system/`) — only header-inline or explicitly `inline` functions are candidates.
- `-inline auto` (libraries: `zlib/`, `vorbis/`, `speex/`, SDK) — MWCC may inline anything it can see.
- `-ipa file` — interprocedural analysis pushes out-of-line definitions across same-TU call sites. Across TUs, only header-inline definitions are visible.

This makes RB3's inline boundaries sharper than MSVC's: an inline ctor in a header is *always* inlined per-caller-TU, an out-of-line ctor is *never* inlined cross-TU.

---

## Inline Constructor in Header vs Out-of-Line in .cpp

**Impact:** +5-10% (typical: trivial sort-class or Hmx::Object subclass ctor at ~91% → 100%)
**Time:** 2 minutes

### Symptom

A bodyless or init-list-only ctor is at 91-95% match. Mismatch is near function entry — extra `stwu`/`stmw`, an unexpected `bl __ct__...`, or a different `_savegpr_NN`.

### Why It Works

Out-of-line ctors emit `bl __ct__<Class>F<args>` at every call site. Inline ctors expand the init-list directly — usually a couple of `stw`/`stfs` for base members and a `lis`/`stw` for the vtable. Target was almost certainly built with the trivial ctor visible in the header.

### Fix

```cpp
// BEFORE - out-of-line
// SongSortByDiff.cpp
SongSortByDiff::SongSortByDiff() : SongSort(by_diff) {}
SongSortByDiff::~SongSortByDiff() {}

// AFTER - inline in header
class SongSortByDiff : public SongSort {
public:
    SongSortByDiff() : SongSort(by_diff) {}
    ~SongSortByDiff() {}
};
```

### When It Hurts

If the body is non-trivial (>3 statements, calls non-inline helpers), inlining can worsen every caller. Restrict to bodyless / init-list-only ctors.

---

## Inline Comparator's `operator()` for std::sort / `__unguarded_partition`

**Impact:** +30-50% on the sort-template instantiation, often cascading the whole `__introsort_loop` family.
**Time:** 5 minutes

### Symptom

A sort helper instantiation sits at 50-70%. Diff shows `bl __cl__<Cmp>...` (or `bl <Cmp>::operator()`) where target has inlined `cmpw`/`cmpwi` + `blt`/`bge` with no call.

### Why It Works

stlport's sort helpers call the comparator via `operator()` from inside a header template. With `-inline noauto`, MWCC inlines only what it sees at parse time. An out-of-line `bool Cmp::operator()(...) const` is invisible to the instantiation, so a `bl` is emitted.

### Fix

```cpp
// BEFORE - out-of-line in StoreOffer.cpp
bool SortCmp::operator()(const StoreOffer* a, const StoreOffer* b) const {
    return a->Compare(b, mSort) < 0;
}

// AFTER - inline in StoreOffer.h
class SortCmp {
    Symbol mSort;
public:
    SortCmp(Symbol s) : mSort(s) {}
    bool operator()(const StoreOffer* a, const StoreOffer* b) const {
        return a->Compare(b, mSort) < 0;
    }
};
```

### Related

If inlining `operator()` is impossible (cross-TU constraints) or insufficient (target uses direct field compares without going through `operator()` at all), see [fixable-macros.md: Comparator Specialization for `__introsort_loop` / Heap Sorts](fixable-macros.md#comparator-specialization-for-__introsort_loop--heap-sorts). The specialization writes a direct field compare into the `.cpp`, emitting `cmpw`/`blt` without any comparator call.

---

## `__declspec(noinline)` to Defeat `-ipa file`

**Impact:** Variable. Typically lifts a caller from 70-80% to 100% by restoring the `bl` to a helper.
**Time:** 5 minutes

### Symptom

A function is 70-85% and the diff shows N extra instructions inlined inside the body that correspond to a same-TU helper. The helper itself may still be at 100% but is never actually called from this function in the target.

### Why It Works

With `-ipa file` MWCC inlines a same-TU helper across a call site even when the helper is defined out-of-line. Target was usually compiled with the helper in a different TU. `__declspec(noinline)` blocks IPA from threading the body through.

### Fix

```cpp
// BEFORE - IPA inlines nandComposePerm into nandGetStatus
static u32 nandComposePerm(...) { ... }

// AFTER - forced bl
static __declspec(noinline) u32 nandComposePerm(...) { ... }
```

Use when 5-25 extra inline instructions in a partial caller match the body of a same-TU helper. See [fixable-macros.md](fixable-macros.md) for the cross-TU mirror (`#pragma ipa on` / per-file IPA).

---

## Member-Function-Pointer Trick: Force Inline AND Out-of-Line

**Impact:** Recovers cross-TU `bl` while keeping intra-TU inlining.
**Time:** 10 minutes

### Symptom

One TU's caller needs the body inlined; a different TU's caller needs a real `bl`. Pure header-inline kills the second; pure out-of-line kills the first.

### Fix (three parts)

1. **Header** — declare only, no body, no `inline`.
2. **Implementation .cpp** — define as `inline void Class::Method(...) { ... }` at file scope.
3. **Force standalone symbol** — `DECOMP_FORCEBLOCK(&Class::Method);` somewhere in the same .cpp.

Taking the method's address forces the linker symbol to exist. The defining TU emits both inlined call sites and a real out-of-line body for cross-TU callers.

**Example:** `SystemComponent::SetParent` — inline within `~SystemComponent`, out-of-line for `SystemComponentGroup::RegisterComponent`. All three at 100% (commit `2a55df50`).

See also [fixable-copy-ctor.md: Member Function Pointer to Force Out-of-Line Emission](fixable-copy-ctor.md#member-function-pointer-to-force-out-of-line-emission).

---

## Qualified Call to Force Inline Across vtable

**Impact:** +5-50% on a caller; cascades when sibling call sites share the pattern.
**Time:** 2 minutes per call site.

### Symptom

Caller shows `lwz r12, OFF(r12); mtctr; bctrl` (vtable dispatch) where target shows direct calls and inlined references to string literals from the callee's body (e.g. `@stringBase0+0x123` for an assert message inside the callee).

### Why It Works

`obj->Method(args)` resolves through the vtable — MWCC can't inline because it doesn't know the dynamic type. `obj->Class::Method(args)` is static dispatch, so MWCC inlines the body when visible. Inlined asserts then land in the *caller's* TU string pool.

### Fix

```cpp
// BEFORE - vtable dispatch, no inline
mDir->LoadFile(filename, ...);

// AFTER - static dispatch, inline candidate
mDir->ObjectDir::LoadFile(filename, ...);
```

### Detection

`bin/find-inlining-gaps --mode qualified-call` emits row-per-row candidates with header path and fix suggestion. Diagnostic signature: `@stringBase` references present in target but absent in base, located near a `bctrl`.

---

## Inline-Boundary Cascade (ICF Merge of Out-of-Line Accessor)

**Impact:** Variable. Fixes a downstream caller's regalloc swap (5-50 instructions) by triggering an ICF merge.
**Time:** 30 minutes — symptom is in a *different* function.

### Symptom

`Caller::Foo` has a residual regalloc swap (e.g. `r28`↔`r29` across many instructions) that won't budge despite source fixes. The function called inside is a trivial accessor like `GetMotdFreq()` defined inline in a header.

### Why It Works

Identical bodies get folded by `mwldeppc`. When two trivial out-of-line accessors fold, the call site emits `bl <merged_address>`. Our inline-in-header version emits the body directly (`lbz rN, OFF(r3)`). The presence/absence of the `bl` changes the call site's register-pressure profile and shifts MWCC's coloring of surrounding callee-saved registers, dissolving what looks like unrelated regalloc noise.

### Fix (When Diagnosed)

```cpp
// BEFORE - header inline
class RockCentral {
public:
    int GetMotdFreq() const { return mMotdFreq; }
};

// AFTER - declaration only + .cpp body
class RockCentral { public: int GetMotdFreq() const; };
// RockCentral.cpp
int RockCentral::GetMotdFreq() const { return mMotdFreq; }
```

### Detection

Hard to spot from the affected function alone. Signs: target's matching version still emits `bl` to the accessor (verify via `/compare-asm`); residual mismatches are register swaps not logic; symbol-address sweep finds another function with the same body merged at that address. See [verifiable-icf.md](verifiable-icf.md) for the verification half.

### When It Doesn't Help

If the accessor's body is unique (nothing else folds with it), moving out-of-line just adds a `bl` without ICF. The source of the swap is elsewhere.

---

## Notes on MWCC vs MSVC Differences

- **No `__chkstk` analog.** MWCC doesn't probe the stack on large frames; frame-size deltas come from spills, not CRT probes.
- **No `??_B` static guards.** MWCC emits `__sinit_<file>_cpp` / `__sterm_<file>_cpp` rather than per-symbol guards, so DC3's "static guard slot ripple" symptom doesn't appear. The closest analog is the `MILO_ASSERT` string pool shift — see [fixable-macros.md](fixable-macros.md#milo_assert-cond-stringification--pool-shift).
- **`-inline noauto` is the default for game code**, so MWCC won't silently inline an out-of-line helper unless `-ipa file` lets it. Inline-boundary patterns above are deterministic: the choice usually is the choice.
- **Diagnosis aid** — when you suspect an inline-boundary issue but can't pin the direction, run the source permuter on the affected function.

---

## See Also

- [verifiable-icf.md](verifiable-icf.md) — linker ICF as a verifiable pattern.
- [fixable-macros.md](fixable-macros.md) — `__declspec(noinline)`, `#pragma dont_inline`, `#pragma ipa on`, TU-local conditional inline macros, comparator template specialization.
- [fixable-copy-ctor.md](fixable-copy-ctor.md) — TU-local inline `Hmx::Object` copy ctor; member-function-pointer trick.
- [fixable-declarations.md](fixable-declarations.md#stlport-direct-member-access-vs-trivial-accessor) — workaround when a trivial accessor refuses to inline under `-inline noauto -ipa file`.
