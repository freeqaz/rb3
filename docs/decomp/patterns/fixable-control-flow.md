# Fixable: Control Flow

Patterns around how loops, conditionals, and scopes are structured at the source level.

## switch vs if/else Chain

CW generates different branch patterns for `switch(x)` vs `if/else if` chains. Switch produces `cmpwi+beq` sequences to specific case bodies; if/else produces cascading `bne` skips.

**Example:** In `SetDiskError`, converting if/else to `switch(err)` matched the target's branch structure.

## Early Return Inversion

```cpp
// Target uses: guard → return, then body
if (mDiskError == kFailedChecksum || mDiskError == err) return;
// vs:
if (mDiskError != kFailedChecksum && mDiskError != err) { body; }
```

These generate different branch directions. Match the target.

## do-while vs while Loop Structure

`do {} while` and `while {}` generate different loop entry patterns. `while` adds an initial branch to the condition check.

**Example:** In `CharLipSync::PlayBack::Poll`, changing `do {} while` to `while` matched the loop structure.

## Small Constant-Bound Loops Are Fully Unrolled at -O4,p

`for (int i = 0; i < N; i++)` where N is a small compile-time constant (≤6-8) gets fully unrolled by CW `-O4,p`, generating N copies of the loop body with no branch.

**Example:** In `ChordbookPanel::ChordComplete`, `for (int i = 0; i < 6; i++)` with a bit-check body was unrolled to 6 individual `andi./beq` sequences (63 instructions total).

## Nested Scope for Destructor Ordering

Wrapping a temporary in a nested `{ }` scope forces its destructor to run at the closing brace, which can match the target's instruction sequence.

**Example:** In `GamePanel::UpdateLatency`, `{ FilePath path; dir = LoadObjects; }` placed the destructor before `Find`, matching the target.

## vector::erase Generates Unrolled Copy

`vec.erase(begin, end)` generates a `std_vec_range_assert` call plus an unrolled 8-element copy loop, which can account for hundreds of bytes of function code.

## STL __find 4-Wide Unrolled Search with CTR Loop

CW's STL `__find` for random-access iterators uses a 4-wide trip count: `(int)(last - first) >> 2` with `for (; count > 0; --count)` generating `srawi.` → `mtctr` → `bdnz` (hardware CTR loop). To match, manually write the unrolled search pattern with bit-shift trip count.

**Example:** In `SingerStats::SetPartPercentage`, manual 4-wide unrolled search with `goto done` on match generated the exact `mtctr`/`bdnz` pattern from CW's `__find` specialization.
