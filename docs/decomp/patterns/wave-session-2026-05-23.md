# Session Notes — Waves 1–17 (May 2026)

A long parallel-Sonnet wave session burning down the 75–99% match band. ~34 confirmed 100%/99%+ wins, many large structural recoveries. The patterns below were either newly discovered or sharpened during this session. Topic-specific patterns have also been added to the relevant `fixable-*.md` files; this doc is the single-place narrative + cross-reference.

## Top discoveries

### `_Temporary_buffer<T*, T>` allocator specialization (HIGHEST IMPACT)

The original RB3 binary's STL sort path uses Milo's custom allocator (`_MemAlloc(n, 0)` / `_MemFree(p)`) inside `_Temporary_buffer<T*, T>::_M_allocate_buffer()` and its destructor, instead of `malloc` / `free`. The bundled `stl/_tempbuf.h` calls `malloc`/`free`, so any TU using `std::stable_sort` ends up mismatching.

**Fix (per-TU, in the .cpp):** add an explicit class template specialization for the concrete element type:

```cpp
#include "utl/MemMgr.h"

namespace stlpmtx_std {

template <>
inline void _Temporary_buffer<Symbol*, Symbol>::_M_initialize_buffer(
    const Symbol& __val, const __false_type&)
{
    uninitialized_fill_n(_M_buffer, _M_len, __val);
}

template <>
inline void _Temporary_buffer<Symbol*, Symbol>::_M_allocate_buffer() {
    _M_original_len = _M_len;
    _M_buffer = 0;
    if (_M_len > size_t(INT_MAX / sizeof(Symbol)))
        _M_len = INT_MAX / sizeof(Symbol);
    while (_M_len > 0) {
        _M_buffer = (Symbol*) _MemAlloc(_M_len * sizeof(Symbol), 0);
        if (_M_buffer) break;
        _M_len /= 2;
    }
}

template <>
inline _Temporary_buffer<Symbol*, Symbol>::~_Temporary_buffer() {
    _STLP_STD::_Destroy_Range(_M_buffer, _M_buffer + _M_len);
    _MemFree(_M_buffer);
}

}
```

The specialization MUST be defined before any STL sort call site in the TU. A `#define malloc(n) _MemAlloc(n, 0)` macro trick does **not** work — MWCC tokenizes template bodies at header-parse time, so macros defined later in the .cpp cannot retroactively rebind already-parsed template code.

**Wins applying this pattern:**
- AccomplishmentPanel.cpp — 4 `__stable_sort_aux<Symbol*>` instantiations 85.9% → 100% (GoalAlpaCmp, AccomplishmentGroupCmp, AccomplishmentCategoryCmp, AccomplishmentCmp)
- AccomplishmentManager.cpp — `__stable_sort_aux<Symbol*, SongDifficultyCmp>` 86.0% → 100%
- CampaignGoalsLeaderboardChoicePanel.cpp — `__stable_sort_aux<Symbol*, GoalCmp>` 86.0% → 100%
- TourDescPanel.cpp — created from scratch + specialization → the **entire sort template family** (`__stable_sort_aux`, `stable_sort`, `__stable_sort_adaptive`, `__merge_adaptive`, etc.) cascaded to 100%, **11 functions newly COMPLETE** in one edit.
- BandPatchMesh.cpp — `__introsort_loop<MeshVert**, SortByWorkVertZ>` 84.5% → 98.9% via explicit specializations for `__introsort_loop`, `__unguarded_partition`, `__unguarded_linear_insert` using direct float comparisons (`v->mVert->pos.z < pivot->mVert->pos.z`) instead of `SortByWorkVertZ::operator()` — eliminates CW's bool-materialization sequences (`mfcr`/`srwi.`/`beq`) in favor of direct `blt`/`bge` after `fcmpo`.

See also: [fixable-macros.md](fixable-macros.md#stl-allocator-specialization).

### `_vector_sized.c` reserve fix — confirmed dead end

Hypothesis: hoist `__n * sizeof(_Tp)` into a named local in `_VECTOR_IMPL::reserve()` to match the target's persistent byte-count register. **Six forms A/B-tested across a full project rebuild.** Best form gained +15 functions but regressed 11 near-100% `reserve<String>` / `reserve<ObjPtr>` / `reserve<DataEvent>` instantiations (94.4%→87.5% etc.). The two sub-problems (full register swap for low-% types, single mulli-keep for high-% types) cannot both be satisfied from `_vector_sized.c` alone. **All forms reverted.** ~25 reserve partials in the 80-94% band stay at_limit.

Recorded so future agents don't repeat the experiment. See [project_stlport_reserve_dead_end.md](../../../../.claude/projects/-home-free-code-milohax-rb3/memory/) (memory; informational).

### `lwzu` idiom via reference-cast auto-increment

When the target reads a packed byte sequence with `lwzu` (load-word-with-update — load + advance pointer in one instruction), use the C reference-cast auto-increment idiom:

```cpp
static unsigned char sKey[] = { 0x7a, 0x4d, 0x60, 0x7c, 0xFF };
const unsigned char *p = sKey;
unsigned int word = *((unsigned int *&)p)++;
```

The `*((unsigned int *&)p)++` form is the specific shape that makes MWCC emit `lwzu`. Plain `unsigned int word = *(unsigned int*)p; p += 4;` produces two instructions (`lwz` + `addi`).

**Win:** `Synth::returnMasterKey` 94.5% → 99.9%.

See [fixable-operators.md](fixable-operators.md#lwzu-via-reference-cast-auto-increment).

### `(float)(int)((unsigned)x >> 24)` for packed alpha extraction

When extracting the high byte of a packed RGBA `int` and converting to `float`, the cast sequence matters:

```cpp
// packed declared as `int`:
float alpha = (float)(int)((unsigned)packed >> 24);
```

This sequence preserves both the `srwi` (unsigned logical shift) AND the `xoris` signed-float-conversion the target uses. Dropping any cast emits a different (mismatching) instruction:
- `(float)((unsigned)packed >> 24)` → unsigned-to-float helper call.
- `(float)(packed >> 24)` → `srawi` (arithmetic shift) instead of `srwi`.
- `(float)(int)(packed >> 24)` → arithmetic shift again.

**Win:** `VocalTrackDir::ApplyFontStyle` 76% → 99.9% (the alpha-channel extraction is one of three packed colors processed per call).

See [fixable-casting.md](fixable-casting.md#packed-alpha-extraction).

### `x % 4` triggers signed-modulo rotate trick

The target's DXT decompression uses MWCC's 5-instruction signed-modulo rotate sequence (`slwi+srwi+subf+rotlwi+add`) for `x mod 4`. Source forms:

```cpp
// 5-instruction rotate trick (matches target):
int xRemainder = x % 4;

// Different codegen — mismatch:
int xRemainder = x - (x / 4) * 4;
int xRemainder = x & 3;  // works for unsigned only; signed modulo differs at negative x
```

**Win:** `RndBitmap::DxtColor` 87.1% → 93.8%.

See [fixable-casting.md](fixable-casting.md#modulo-rotate-trick).

### `ObjPtr<T>::mPtr` direct member access

`ObjPtr<T>::operator T*()` is an inline accessor. Each call site materializes a fresh load — even repeated calls inside one if-condition get separate loads. Direct `.mPtr` access reuses the same load:

```cpp
// Two loads, often with r4↔r5 swap cascading from the second:
if (unkd4 && unke0 > i) return false;
bool temp = interest != unkd4;

// One load, single register, matches:
if (unkd4.mPtr && unke0 > i) return false;
bool temp = interest != unkd4.mPtr;
```

**Win:** `CharEyes::SetFocusInterest` 90.8% → 100%.

The same shape applies to the related `OutfitConfig::InMilo` fix: `DataNode *nPtr = &n; nPtr->Type()` instead of `n.Type()` forces r3 to hold the pointer through the entire vtable chain (94.2% → 99.5%).

See [fixable-declarations.md](fixable-declarations.md#objptr-direct-mptr-access).

### `#pragma fp_contract off` to suppress `fmsubs`

When the target emits separate `fsubs` + `fmuls` but ours emits a single fused `fmsubs`, the source-level lever is `#pragma fp_contract off` around the function:

```cpp
#pragma fp_contract off
void Spotlight::CalculateDirection(RndTransformable *t, Hmx::Matrix3 &m) {
    // ... Cross() call inlines without fmsubs fusion ...
}
#pragma fp_contract on
```

**Wins:** `Spotlight::CalculateDirection` 88.1% → 93.4%, `MakeRotMatrix(Vec3, Vec3, Matrix3)` 87.6% → 94.0% (the latter via manual 6-product Cross expansion + 0.0f load as scheduling barrier).

See [fixable-fsel-fma.md](fixable-fsel-fma.md#suppressing-fmsubs-via-fp_contract).

### Pair-local variable forces stack materialization

`make_pair(...)` + immediate push_back leaves the pair in registers; the target instead materializes both fields on the stack before the branch:

```cpp
// Registers only — mismatches the target's stack stores:
if (end - start > 0.0f)
    out.push_back(std::make_pair(start, end));

// Pair on stack — matches:
std::pair<float, float> p(start, end);
if (p.second - p.first > 0.0f)
    out.push_back(p);
```

**Win:** `VocalNoteList::GenerateLegalFreestyleSections` 84.1% → 99.9%.

### Struct copy → individual field access

A `Foo f = container.front()` struct copy often inflates the stack and prevents CSE of the underlying list-node pointer. Replacing with individual field access lets CW share one load and use callee-saved registers directly:

```cpp
// 0x60+ frame, struct on stack:
ScreenParams sp = mScreens.front();
FilePath fp(FilePath::sRoot.c_str(), sp.fname);
SomeCall(sp.msecs);

// 0x40 frame, fields in callee-saved regs:
const char *fname = mScreens.front().fname;
int msecs = mScreens.front().msecs;
FilePath fp(fname);
SomeCall(msecs);
```

**Win:** `Splash::PrepareNext` 87.1% → 99.5%.

### Split BinStream `>>` chains at register-reuse points

When `bs >> a >> b >> c >> d` mismatches mid-chain, find the index where the target reloads `bs` (often returning to the `_rs` operator on a heap object). Split the chain there — the second `bs >> b` starts fresh from the cached `bs` pointer in r30 rather than threading the prior result through r28:

```cpp
// Mismatching r28↔r30 swaps in the middle:
bs >> mGradientMap >> mGradientMapOpacity >> mRefractMap >> mRefractOpacity;

// Matching:
bs >> mGradientMap;
bs >> mGradientMapOpacity >> mRefractMap >> mRefractOpacity;
```

**Win:** `RndPostProc::LoadRev` 98.4% → 100%.

### Symbol == const char* over `streq`

`Symbol::operator==(const char*)` generates the `addic.`/strcmp/cntlzw null-guard pattern the target uses. `streq(symbol.Str(), "literal")` generates a different sequence.

```cpp
// Mismatching streq path:
if (streq(plat.Str(), "pc")) ...

// Matching Symbol overload:
if (plat == "pc") ...
```

**Win:** `BandWardrobe::GetPrefab` 85.2% → 99.2% (with several other compounding fixes).

### `!streq(a, b)` over `strcmp(a, b)` for bool branches

`!streq(...)` materializes `(strcmp == 0)` as a bool via `cntlzw+srwi.`, then `!` inverts to `bne`. `if (strcmp(a, b))` emits `beq` with different surrounding scheduling.

**Win:** `OutfitConfig::InMilo` 94.2% → 99.5% (combined with the `nPtr` DataNode pattern above).

### Early-return collapses duplicate AutoTimer destructor

`AutoTimer` RAII destruction emits 18 instructions of timer-finalization code. An `if (cond) { ... long block ... }` shape duplicates that destructor in both the taken and not-taken paths. Inverting to `if (!cond) return;` collapses to one copy:

```cpp
// Duplicates AutoTimer dtor across both arms:
if (!mKeyframes.empty()) {
    // ... 100 lines ...
}

// Single dtor copy:
if (mKeyframes.empty()) return;
// ... 100 lines ...
```

**Win:** `LightPreset::SetFrameEx` 86.8% → 92.8% from this alone (further +5pp via other fixes).

### Split int addition forces scheduling

`int frame = (int)b1 + bit1 + bit2;` schedules a single sum sequence. Splitting it changes intermediate live ranges and lets CW interleave a pending string-pointer load:

```cpp
// Single 3-way sum — string load gets sandwiched mid-arithmetic:
int frame = (int)b1 + bit1 + bit2;

// Split — string load schedules into the gap, fixing 82-instruction match:
int frame = (int)b1 + bit1;
frame += bit2;
```

**Win:** `VocalTrackDir::SetMissingMicsForDisplay` 91.8% → 100%.

### Pre-declare `slots` (or any heavy callee-saved temp) before a call

When the target uses one more callee-saved register than ours, pre-declaring a temp BEFORE the relevant function call forces CW to span it across the call:

```cpp
// 6 callee-saved (r26-r31) — mismatches target's 7 (r25-r31):
const Gem &gem = gems[i];
gem.Hit(...);

// 7 callee-saved:
const Gem &gem = gems[i];
unsigned int slots = gem.Slots();  // span this across the next call
gem.Hit(..., slots);
```

**Win:** `GemManager::Hit` 87.2% → 89.6%.

### Pre-loop iterator hoist for `_M_data`

When the target loads `vector::_M_data` once and computes `end` as `data + size()`, but ours calls `.end()` each loop iteration (extra `lwz` reload), declare the begin iterator before the loop and recompute end from it:

```cpp
// Reload each iteration:
while (it != mGems.end()) { ... }

// Single load — matches:
std::vector<GameGem>::const_iterator gemBase = pGemList->mGems.begin();
while (it != gemBase + pGemList->mGems.size()) { ... }
```

**Win:** `TrackerUtils::CountGemsInSong` 93.6% → 98.3%.

### MILO_WARN argument and format must match the target exactly

Tiny string-literal differences (a missing `\n`, swapped `%s` arg order) shift `@stringBase0` offsets for **every** function in the TU. One agent fixed `CameraShot::LensSym_to_FOV` by adding a `\n` to a MILO_WARN in the *sister* function `CamShotFrame::Interp`. Always cross-reference the target asm's string pool layout.

```cpp
// Wrong arg order — different MakeString template + offset shift:
MILO_WARN("This mesh (%s // %s)", Name(), Dir()->GetPathName());

// Correct — matches target's MakeString<PCc,PCc> arg order:
MILO_WARN("This mesh (%s // %s)", Dir()->GetPathName(), Name());
```

**Win:** `RndMesh::SkinVertex` 98.4% → 100%.

### Algorithmic bug found via DC3 cross-reference

When a function is 90%+ but the diff doesn't fit any compiler-pattern shape, suspect a logic bug. DC3 (Xbox360/MSVC) is a logic reference — its asm doesn't transfer, but its source structure usually does.

**Win:** `RndConsole::OnMsg(KeyboardKeyMsg)` 93.3% → 100% — TAB completion had wrong iterator initialization (`mBuffer.begin()` should be `PrevItr(mBuffer.end(), 1)`); history-back navigation had wrong boundary checks. Both revealed by side-by-side with DC3.

### Genuine source bugs found this session (worth flagging in PR reviews)

- **`TickToMs(tickSum)` should have been `TickToMs(curPhrase.unk8)`** — VocalTrainerPanel::CopyPhrasesImp had the same arg in two consecutive calls, computing a delta-of-equal that's always zero.
- **`parts > 3` should have been `parts > 1`** — VocalTrackDir::ShowPhraseFeedback's "perfect harmony" branch was unreachable.
- **Missing `return true`** — UIManager::BlockHandlerDuringTransition had implicit UB fallthrough after the FocusPanel loop.
- **`Exit()` should have been `Enter()`** — CharEyes::Poll called the wrong vtable slot on negative DeltaSeconds.
- **`BinkSetMemory` declared with 3 args, called with 3, but takes 2** — Movie::Impl::Init.
- **`return false` in `ObjPtr<T>::Load`** — already in ObjPtr_p.h, looks like a bug, but A/B testing proved the target binary *also* returns false. Caught a near-miss agent over-fit.

## Process notes

- **Bands and yield:** 99%+ ≈ 22% conversion (mostly reloc/scheduler noise), 96–99% ≈ 12% (template register coloring walls), 92–96% ≈ 25%, 75–90% ≈ 25% with the largest per-function gains (often +10–20pp). Lower-band wins typically come from algorithmic bugs or large structural restructures.
- **Wave size:** 8 parallel Sonnet agents per wave with strictly distinct units is the sweet spot. Two agents touching the same .cpp will conflict during builds.
- **Header edits require A/B:** the `_vector_sized.c` reserve experiment validated the A/B protocol — snapshot report.json, full rebuild, per-function diff, revert if any near-100% regresses. A wave-1 agent edited `ObjPtr_p.h` on a single-function hunch and silently regressed 74 functions; the A/B protocol caught and reverted it.
- **DC3 is a logic reference, not asm reference.** DC3 was compiled for Xbox360/MSVC; its source structure is portable but its instruction sequences are not. "DC3 has it at 100%" only means the logic is correct, not that copying source will hit 100% in CW.
- **Concurrent agents are the noisy floor.** This repo has multiple parallel sessions committing during waves. Per-function results are reliable; global `matched_functions` counts include their work too.
- **`/compare-asm` skill tooling:** the `--compare-asm` flag in `scripts/analysis/diff_inspect.py` was missing all session — the skill was silently failing into manual `objdump` fallback. Fixed mid-session.

## Recurring header blockers (need approval for header edits)

- **Mtx.h** inline `Multiply`/`Normalize`/`Cross` (cr1 vs cr0 in alias check, fmsubs vs fmuls schedule) — blocks ~6+ char/world/math functions.
- **Timer.h `AutoTimer` ctor** (`mTimeLimit = limit` before `mCallback = callback`) — `50.0f` constant hoists past null-check, blocks ~5+ Poll functions.
- **`_vector_sized.c` reserve register choice** — confirmed dead-end above; ~25 reserve partials.
- **stlport templates** (`_M_start` accessor inlining, `_tempbuf.h` allocator) — partial unlocks possible via TU-local pattern (see `_Temporary_buffer` discovery above).

## Sweep follow-up (2026-05-25) — STL comparator/allocator broadening

After the original `_Temporary_buffer` and `__introsort_loop` discoveries, an 8-TU parallel-Sonnet sweep applied both patterns broadly. The keepable wins and (more importantly) the negative results clarified the prerequisites.

### Wins kept

| TU | Function | Before → After |
|---|---|---|
| `band3/meta_band/AccomplishmentDiscSongConditional.cpp` | `InqSongs` (stable_sort inlined here) | partial → **100%** |
| `system/ui/UI.cpp` | `__unguarded_linear_insert<UIResource**, Compare>` | 84.4% → **100%** |
| `system/ui/UI.cpp` | `__unguarded_partition<UIResource**, Compare>` | 95.9% → **100%** |
| `system/beatmatch/GameGemList.cpp` | `__unguarded_linear_insert<GameGem*, less<GameGem>>` | 91.5% → **100%** |
| `system/beatmatch/GameGemList.cpp` | `__introsort_loop<GameGem*, less<GameGem>>` | 89.5% → **99.4%** |
| `system/beatmatch/GameGemList.cpp` | `__unguarded_partition<GameGem*, less<GameGem>>` | 93.5% → **98.4%** |

### Pattern prerequisites confirmed by negative results

The comparator-specialization pattern is narrow. All three must hold:

1. **Integer/pointer comparator** — float compares already use `fcmpo` + `blt` directly. Float comparators (MessageTimer's `MaxSort` on `MaxMs()` float, BandPatchMesh's `SortByZ` on `.pos.z`) cannot be improved this way — residual mismatches are unfixable f0↔f1/f2 volatile-FPR swaps.
2. **Trivially-copyable element type** — non-trivial copy ctors (smart pointers like `ObjOwnerPtr<T>`) trigger hidden-pointer ABI in the generic template; a specialization with `T __val` by-value uses a different calling convention. CharClipGroup's `Alphabetically` on `ObjOwnerPtr<CharClip, ObjectDir>` confirmed this.
3. **Target binary diff must currently show `mfcr` / `srwi.` / `beq` bool-mat** in the inner loop. If the target itself was built without the specialization (CameraManager's `NameSort`, Stats.cpp's `PartPercentageSorter`), forcing one will regress.

### Operational refinements

- **Specialization order matters under IPA** — declare `__adjust_heap` spec BEFORE `__introsort_loop` spec (introsort → partial_sort → __adjust_heap; IPA pre-instantiates the generic from an earlier-seen call site).
- **`_STLP_INLINE_LOOP` = `inline`** blocks `__push_heap` specialization (CW error 10335: "illegal explicit template specialization").
- **`_M_initialize_buffer` specialization triggers error 10335** in the `_Temporary_buffer<T*, T>` allocator pattern — use the 2-specialization variant (`_M_allocate_buffer` + dtor only), as in `TourDescPanel.cpp`/`AccomplishmentManager.cpp`.
- **For `__introsort_loop` median, use scalar field temporary** (`float __a = __first->mMs`), not struct copy (`T __a = *__first`). The struct copy adds field-by-field copies the target's inline expansion doesn't have. UI.cpp `__introsort_loop` hit 100% only after accessing fields directly without binding a `UIResource *__a = *__first` local.
- **String compare** must resolve to the exact symbol the target calls — `left->mPath < right->mPath` (calls `String::operator<`) matches; `strcmp(...) < 0` does not.

See [fixable-macros.md](fixable-macros.md#stl-allocator-specialization-_temporary_buffert-t) for the full pattern reference, prerequisites, and the negative-results table (CameraManager / MessageTimer / CharClipGroup / BandPatchMesh SortByZ / Stats — do not retry).
