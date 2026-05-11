# Fixable: Bool Materialization

Patterns around how MetroWorks CodeWarrior materializes boolean values into registers.

## IsLocal() vs !IsNet() Produce Different Patterns

`IsLocal()` generates `cntlzw + srwi.` (compact bool materialization), while `!IsNet()` generates `cmpwi + beq + li` (branch-based negation). Choose the one that matches the target.

**Example:** In `GemPlayer::Pass`, switching from `!IsNet()` to `IsLocal()` jumped match from 87.6% to 99.1%.

## Compound Bool Pattern

For materializing compound conditions, use explicit bool variables:

```cpp
bool rejN = A && B;
if (rejN) continue;
```

This generates the `cror` + `srwi.` pattern CW uses for compound boolean tests, rather than the cascaded short-circuit branches you'd otherwise get.

## Condition Inversion for Branch Direction

`if (x) return; body;` generates different branch polarity than `if (!x) { body; }`. CW emits `bne body; b exit` vs `beq exit; fallthrough`. Match whichever the target uses.

**Example:** In `Character::Poll`, `if (mFrozen) return;` matched the target's `bne [epilogue]` branch.
