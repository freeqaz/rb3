# Harmful Patterns: Avoid These

MWCC / RB3 version of DC3's `harmful-avoid.md` — adapted from `/home/free/code/milohax/dc3-decomp/docs/decomp/patterns/harmful-avoid.md`.

Source shapes that look reasonable but actively hurt match% under MetroWorks CodeWarrior (`mwcceppc 4.3.172`, `-O4,p -inline noauto -ipa file -sdata 2 -sdata2 2`). MWCC's register allocator and inliner respond differently than MSVC's, but most of the anti-patterns from DC3 transfer directly because the underlying mechanism is the same: source choices that add IR nodes (extra deref, extra alias, extra store) survive into codegen as extra GPR/FPR pressure.

**Action:** Do NOT use these patterns. If you've already applied them and the function regressed, revert before chasing other theories.

---

## Member Aliasing

**Effect:** Typically -3% to -6%, occasionally larger when the alias spans a call.

Creating a local reference or pointer to a member variable hurts register allocation. MWCC keeps the alias alive in a callee-saved GPR (`r25`-`r31`) even when target accesses the member directly each time via `r3 + offset`.

### What NOT to Do

```cpp
// BAD - reference alias to member
Transform& xfm = mLocalXfm;
xfm.SetRotation(...);

// BAD - pointer alias
Transform* pXfm = &mLocalXfm;
pXfm->SetRotation(...);
```

### What To Do Instead

```cpp
// GOOD - direct member access; MWCC re-loads via r3 each use
mLocalXfm.SetRotation(...);
```

### Why It Hurts

Direct member access inside a method emits `lwz rN, OFF(r3)` (or `addi rN, r3, OFF` for the address) at each use. With `-O4,p` MWCC will CSE adjacent uses inside the same basic block but won't promote the alias to a callee-saved register. An explicit local reference forces the promotion, growing the prologue (`stmw`/`stwu`) and shifting register coloring downstream.

---

## Child Pointer in Loop

**Effect:** Typically -5% to -7%.

Adding an intermediate pointer for iterator dereferencing inside loops.

### What NOT to Do

```cpp
// BAD - extra child pointer occupies a callee-saved register across the loop
for (std::vector<RndDrawable*>::iterator it = mChildren.begin();
     it != mChildren.end(); ++it) {
    RndDrawable* child = *it;
    child->Draw();
}
```

### What To Do Instead

```cpp
// GOOD - direct dereference; MWCC reuses the same volatile reg per iter
for (std::vector<RndDrawable*>::iterator it = mChildren.begin();
     it != mChildren.end(); ++it) {
    (*it)->Draw();
}
```

### Why It Hurts

`auto* child = *it;` forces a live range across the inner call. MWCC then needs `child` in a callee-saved register (so it survives the call), which steals a slot from a member load or loop bound that target keeps callee-saved. Diff usually shows an `r28`↔`r29` (or similar) swap concentrated in the loop body.

---

## Iterator Address-Of (`&*iter`)

**Effect:** -3% to -5% on the calling function.

Taking the address of a dereferenced iterator (`&*it`) instead of passing the iterator directly. With stlport's `vector<T>::iterator` being a `T*` typedef this looks like a no-op — but the IR carries the deref-then-addrof round trip into codegen and the result diverges from the target's direct-pass version.

### What NOT to Do

```cpp
// BAD - extra deref/addrof round trip
std::vector<MyType> values;
std::vector<MyType>::iterator it = values.begin();
std::vector<MyType>::iterator next = it + 1;
InsertHeaderRange(&*it, &*next);
```

### What To Do Instead

```cpp
// GOOD - pass iterators directly
InsertHeaderRange(it, next);
```

### Detection

Grep the candidate file for `&*` and compare against DC3/upstream's call shape. If they pass iterators directly, drop the `&*`.

### Related

- [Child Pointer in Loop](#child-pointer-in-loop) — same family: extra iterator-shaped local hurting regalloc.
- [Iterator Dereference Caching note](fixable-declarations.md#objptrtmptr-direct-member-access) — sometimes caching the dereferenced value *helps*; verify per call site by running the diff both ways.

---

## End Iterator Explicit (`it != end` cached)

**Effect:** -0.5% to -1% in the simple case; can cascade further when MILO_ASSERT is involved.

Storing the end iterator in an explicit variable used in the loop condition.

### What NOT to Do

```cpp
// BAD - explicit end variable
std::vector<Foo*>::iterator end = mChildren.end();
for (std::vector<Foo*>::iterator it = mChildren.begin(); it != end; ++it) { ... }
```

### What To Do Instead

```cpp
// GOOD - end() called each iteration; MWCC inlines _M_finish and shares the load
for (std::vector<Foo*>::iterator it = mChildren.begin();
     it != mChildren.end(); ++it) { ... }
```

### Exception — MILO_ASSERT find pattern

The mirror case *does* help: `MILO_ASSERT(std::find(v.begin(), v.end(), x) == v.end())` recomputes `end()` twice. Hoist `auto e = v.end();` and use it on both sides of the assert. See the [Cache .end() before find-assert](../../decomp/) memory entry — pattern docs cover the loop-body case; assert-shape is the explicit exception.

---

## Constructor Zero-Init That Doesn't Exist in Target

**Effect:** -2% to -6% (size mismatch + extra float stores).

Adding explicit zero-initialization to members that the original constructor did not initialize injects extra constant loads and stores — usually `lis/lfs/stfs` for `0.0f` or `li 0; stw` for ints.

### What NOT to Do

```cpp
// BAD - adds lis/lfs/stfs for 0.0f if target doesn't initialize unk90
CharacterTest::CharacterTest(Character* theChar)
    : /* ... */, unk90(0.0f) {
}
```

### What To Do Instead

Only initialize if the target's prologue stores the value. Verify by looking at the ctor's stores in the target asm — count `stw r0, OFF(r3)` / `stfs fN, OFF(r3)` immediately after the prologue and only mirror what's there.

### Notes

When a member is documented as `unkNN` and the target ctor doesn't touch it, leaving it uninitialized is the match-correct choice. Convert to a named field later via a header rename, not by adding a spurious init.

---

## Patterns Mostly Neutral Under MWCC

Confirmed across multiple RB3 functions to have no measurable effect:

| Pattern | Effect |
|---------|--------|
| `while` loop instead of `for` (same body) | None |
| Separated increment (`++it` in body) | None |
| Iterator declared outside loop | None |
| Container alias (`std::vector<T>& v = mChildren;`) | None |
| `self = this` / `mesh = this` shim at function start | None |

These are stylistic; pick whichever reads best in the source.

---

## Key Takeaways

1. **Don't alias member transforms / vectors / matrices** — direct `m*` access is the match-correct shape.
2. **Don't add `auto* child = *it;` inside loops** — dereference at the use site.
3. **Don't cache `.end()` in a loop variable** — call `.end()` in the condition. The mirror exception is the MILO_ASSERT-find pattern.
4. **Don't zero-init members the target leaves uninitialized** — every spurious store is a frame-size delta.
5. **Loop syntax and `this`-shim choices are usually neutral** — chase real anti-patterns first.

If a "fix" you applied lifts match% by less than 0.5% and adds source noise, revert it. Stack the wins that come from the catalogued [fixable-declarations.md](fixable-declarations.md) patterns instead.

---

## See Also

- [fixable-declarations.md](fixable-declarations.md) — declaration-order and register-hoisting patterns that DO help.
- [fixable-control-flow.md](fixable-control-flow.md) — loop-shape and early-return inversions that DO help.
- [verifiable-icf.md](verifiable-icf.md) — when a "fix" causes the function to merge with another via ICF.
