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

## Early-Return Collapses Duplicate `AutoTimer` Destructor

`AutoTimer` RAII destruction (and similar scope-bound RAII) emits ~18 instructions of timer-finalization code. An `if (cond) { ... long block ... }` shape duplicates the destructor across both the taken and not-taken paths. Inverting to early-return collapses it to one copy:

```cpp
// AutoTimer dtor runs twice (one per arm):
void LightPreset::SetFrameEx(...) {
    AUTO_TIMER();
    if (!mKeyframes.empty()) {
        // ... 100 lines of body ...
    }
}

// AutoTimer dtor runs once:
void LightPreset::SetFrameEx(...) {
    AUTO_TIMER();
    if (mKeyframes.empty()) return;
    // ... 100 lines of body ...
}
```

**Example:** `LightPreset::SetFrameEx` 86.8% → 92.8% from this alone (further +5pp via other fixes).

## Split Int Addition Changes Instruction Scheduling

A 3-way compound `int` sum schedules its arithmetic as a contiguous block. Splitting into two statements changes intermediate live ranges and lets CW interleave a pending string-pointer load (or other independent op) into the gap:

```cpp
// Single 3-way sum — string load schedules adjacent to arithmetic:
int frame = (int)b1 + bit1 + bit2;

// Split — string load schedules into the gap, matches target's interleaving:
int frame = (int)b1 + bit1;
frame += bit2;
```

**Example:** `VocalTrackDir::SetMissingMicsForDisplay` 91.8% → 100%.

## Mid-Loop Break for Last-Iteration Special Case

When the target loop body runs N times for "main" code and N-1 times for some trailing update (e.g. position increment), avoid the natural `for` form by writing `for (...) { body; ++i; if (i == n) break; trailing; }`:

```cpp
// Natural form — trailing runs N times, including the unwanted Nth:
for (int i = 0; i < n; ++i) {
    body;
    trailing_update;  // runs N times — wrong
}

// Mid-loop break — trailing runs N-1 times:
for (int i = 0; i < n; ) {
    body;
    ++i;
    if (i == n) break;
    trailing_update;
}
```

**Example:** `Tail::UpdateVerts` mid-loop restructure in the vertex/segment update.

## Pair-Local Variable Forces Stack Materialization

`make_pair(...)` + immediate push_back may leave the pair in registers, while the target materializes both fields on the stack before the branch (so the inlined push_back fast-path can reload from there):

```cpp
// Registers only — mismatches target's stack stores:
if (end - start > 0.0f)
    out.push_back(std::make_pair(start, end));

// Pair on stack — matches:
std::pair<float, float> p(start, end);
if (p.second - p.first > 0.0f)
    out.push_back(p);
```

**Example:** `VocalNoteList::GenerateLegalFreestyleSections` 84.1% → 99.9%.

## Restore Missing `MILO_ASSERT` Calls

The original source had MILO_ASSERTs that emit non-trivial codegen (`MakeString` + `Fail` — ~13 instructions). If the diff shows a missing instruction cluster matching that shape, the source is missing an assert. Inspect the target's string pool for the assert's format string:

```cpp
// Source missing assert — diff shows 13-instruction insert cluster:
if (!disk->Showing()) {
    // ... main path ...
} else {
    // empty
}

// Restored — matches target:
if (!disk->Showing()) {
    // ... main path ...
} else {
    MILO_ASSERT(disk, 0xB9);
}
```

**Example:** `SpotlightDrawer::DrawAccessories` 87.0% → 94.6% from restoring one assert.
