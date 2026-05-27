# lwzu_idiom

**Pattern file**: `scripts/permuter/patterns/lwzu_idiom.py`
**Status**: B3a (new; proven on `Synth::returnMasterKey` 94.5% -> 99.9%).

## What

Collapse a separate load + pointer bump into the single reference-cast
post-increment that MWCC's `lwzu` / `lhzu` / `lbzu` recognizer needs:

```cpp
// Pattern matches (emits lwz + addi today):
unsigned int word = *(unsigned int*)p;
p += 4;

// Pattern emits this (matches target's lwzu):
unsigned int word = *((unsigned int *&)p)++;
```

Supported widths and their bump constants:

| C type            | sizeof | target opcode |
|-------------------|--------|---------------|
| `unsigned int`    | 4      | `lwzu`        |
| `unsigned long`   | 4      | `lwzu`        |
| `unsigned short`  | 2      | `lhzu`        |
| `unsigned char`   | 1      | `lbzu`        |

The pattern also accepts `p += sizeof(unsigned <T>)` on the bump line.

## Why this moves match%

MWCC only emits the load-with-update form for this exact source idiom.
Every other "obviously equivalent" shape (`*(uint*)p` + `p += 4`, casted
parens, etc.) emits a separate `lwz` followed by `addi`. That extra
`addi` shows up as either a real diff op or — more often — a register
allocation cascade because the bump uses a temporary the target's pipeline
never needed.

## Asm signal

`relevant()` returns True whenever any diff op pair has `lwzu` / `lhzu` /
`lbzu` on one side but not the other. The signal is unambiguous: those
opcodes have no other source spelling. Priority is 0.8 — strong gate.

## When the pattern fires

- AST: two adjacent named children of any `compound_statement` where:
  - The first is `<lhs>? = *(unsigned <T>*)<ptr>;` (assignment, declaration
    initializer, or bare expression) with a simple identifier as `<ptr>`.
  - The second is `<ptr> += K;` where `K` matches `sizeof(unsigned <T>)`
    (4, 2, or 1), expressed as a literal or `sizeof(unsigned <T>)`.
- Variants per function are bounded at 6.
- Mismatched widths (`unsigned int` cast with `p += 2`) are skipped.

## Repo footprint

Already applied in tree:

```
src/system/synth/Synth.cpp                  Synth::returnMasterKey
src/sdk/RVL_SDK/src/usbmic/usbmic.c         (manual)
```

Scan with `python -m scripts.permuter.pattern_scan --patterns lwzu_idiom`.

## Cross-reference

- MEMORY: `feedback_lwzu_idiom.md`
- Original proof: `Synth::returnMasterKey` 94.5 -> 99.9%
- Related: `fixable-operators.md` (the manual writeup), `feedback-pragma-pool-data-off.md`
  (sister codegen-control pattern)
