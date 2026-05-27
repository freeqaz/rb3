# return_this_op_assign

**Pattern file**: `scripts/permuter/patterns/return_this_op_assign.py`
**Status**: B1 (new; manually proven on `FileMerger::Merger::operator=` 64.6% -> 100% in commit `f1998277`).

## What

Append `return *this;` to any reference-returning `operator=` whose body
lacks any return statement:

```cpp
T &Foo::operator=(const Foo &other) {
    mA = other.mA;
    mB = other.mB;
    // pattern adds:
    return *this;
}
```

The transformation is semantically benign on its own — the missing return
is undefined behavior under `[stmt.return]/2` when the return type is
non-void — but it pins `this` to `r3` at exit, which **dramatically**
changes register allocation throughout the body.

## Why this moves match%

When the body has no return, the MWCC ABI doesn't constrain `this` to live
in `r3` across the function. The register allocator then steals `r3` for
temporaries; `this`-comparisons frequently land in `r4`, cascading
`r3<->r4` swaps everywhere a member is touched. Adding `return *this;`
forces `this` to stay in `r3` until exit and the cascade collapses.

## Asm signal

Liberal — every diff with `mr`, `addi`, `cmpw`, `cmplw`, `cmpwi`, `cmplwi`
on `r3`/`r4`, or any callee-saved reg-swap pair, qualifies. The AST gate
(ref-return + name ends in `operator=` + no existing return statement) is
strict enough that the asm gate doesn't need to be.

Symbol hint: mangled `__as__*` always boosts priority (MWCC mangles
`operator=` as `__as`).

## When the pattern fires

- AST: `function_definition` whose top declarator is `reference_declarator`
  (return type `T&`), the declared name ends in `operator=`, and the body
  contains zero `return_statement` nodes anywhere.
- One variant per matching function.

## Repo footprint

5 hit functions at time of authoring (Gem.cpp's `operator=` correctly
skipped because it returns `(Gem &)g`):

```
src/network/Platform/DateTime.cpp    DateTime::operator=
src/network/Platform/Time.cpp        Time::operator=                  (x2)
src/system/rndobj/MatAnim.cpp        RndMatAnim::TexKeys::operator=
src/system/rndobj/Mesh.cpp           RndMesh::VertVector::operator=
```

Scan with `python -m scripts.permuter.pattern_scan --patterns return_this_op_assign`.

## Cross-reference

- MEMORY: `feedback_return_this_op_assign.md`
- Original proof: commit `f1998277` (FileMerger::Merger::operator= 64.6% -> 100%)
- Related: `fixable-copy-ctor.md`, `fixable-declarations.md`
