# Fixable: Macros, Pragmas, and Static Guards

Patterns controlled by compiler pragmas, macro layout, or static-variable initialization.

## #pragma pool_data off

Prevents CW's IPA from pre-loading the BSS segment base address into a callee-saved register at function entry. Critical when the target doesn't have this optimization.

**Example:** In `SetDiskError`, `#pragma pool_data off` prevented IPA from hoisting BSS base to r31, which was causing a 4-register spill cascade.

## #pragma dont_inline on/off

Controls whether CW inlines functions within the pragma scope. Be careful — `dont_inline on` can cause `MessageTimer` constructors to not inline, drastically shrinking function size.

## Static Message Guards

Function-local `static Message msg(...)` generates guard variables (`_GUARD_FuncName@msg`). These add initialization checks on every call. The guard pattern must match between source and target.

## MILO_ASSERT `#cond` Stringification → Pool Shift

**Rule:** `MILO_ASSERT(cond, line)` stringifies `cond` (Debug.h:86) — each unique condition text is a distinct TU-pool slot; reorderings cascade into register allocation. Diff target vs base string tables to find which direction the fix goes.

**Method (bidirectional — direction must be diffed first):**
```bash
mwccppc-nm -s target.o | grep '@'        # or: strings build/.../obj/foo.o
mwccppc-nm -s base.o   | grep '@'
diff
```
Then either: (a) extract inline expressions to a local (when base has the long string, target has the short one), or (b) restore inline form (when target has the long string, base over-extracted). When extracting, name the local to match simple-name conditions already pooled by other asserts in the TU.

**Why:**
- `AccomplishmentManager.cpp` — renamed `pMeta` → `pPerformer` and extracted `MILO_ASSERT(MetaPerformer::Current(), 0xA8B)` into a local: 9 fns → 100%, 95-99% band 21 → 12.
- `VocalPlayer.cpp` (counter-direction) — `LocalScorePhrase` had `unsigned int sz = i_rNewPhraseActiveParts.size(); MILO_ASSERT(sz, …)`; target had the `.size()` inline. Restoring inline form was a 26-byte shorter→longer pool string, undoing a -28 byte pool shift; cascaded 9 functions to 100% in one edit.
- `Tour.cpp` (function-order matters) — fixing the -10 delta required BOTH renaming `performer` → `pPerformer` AND moving `InitializeTour` from before `UseUsersProgress` to after `IsUnderway`. Pool entries are ordered by emission order, so function-definition order in the .cpp controls where each assert string lands. +16 fns to 100% in one commit.
- `BandCharacter.cpp` (string tail-merge variant) — `DataVariable("no_anim")` allocated a standalone 8-byte pool slot. Target binary referenced the same string as a SUBSTRING of an existing `DECOMP_FORCEACTIVE`'d `"BandCharacter.no_anim"` via `+ 14` offset. Fix: `DataVariable("BandCharacter.no_anim" + 14)`. +25 fns to 100% in one line.
- `NoteTube.cpp` (stringify-only macro variant) — when a header field name (e.g. `unk_0x30`) is off-limits but the target stringified it under a semantic name (e.g. `mWidth` that's already pooled via `DECOMP_FORCEACTIVE`), use a TU-local `#define` to rename the token at the assert site:
  ```cpp
  #define mWidth unk_0x30
  MILO_ASSERT(mWidth > 0.0f, 0xCE);
  #undef mWidth
  ```
  `#cond` stringifies the literal `mWidth > 0.0f` token (deduping with the FORCEACTIVE) while the runtime condition macro-expands to `unk_0x30 > 0.0f`. Use sparingly — header rename is cleaner if available.
- `AppLabel.cpp` (DECOMP_FORCEACTIVE pool injection) — when the cluster is driven by string ORDERING (same strings, different positions) or by dead pool entries with zero `.text` references in target, list ALL target-pool strings in target's exact emission order inside a single `DECOMP_FORCEACTIVE(TU, ...)` call. Forces MWCC to emit them in that order, including dead/unused literals. +10 fns to 100% with this one trick.

Same family as [format-string tweaks shift string pool](fixable-operators.md) and `MakeString("literal")` opening a FormatString slot.

**When to apply:** TU has a long 95-99% tail AND multiple functions share the SAME `@stringBase`/`@sda`/`@sda2` byte delta. That uniform delta IS the pool-shift signature. Without it the partials are permuter-class — assert-count alone is a poor predictor.

**Find candidates:** `python3 scripts/find_pool_shift.py [--filter PATH]` — walks `report.json`, runs objdiff with `--include-instructions` per partial, extracts Signed-arg diffs on pool-symbol instructions, groups by TU + delta. Ranks TUs where ≥3 functions share one delta. Per-TU output gives the exact byte delta to chase. (Latest scan: `docs/decomp/pool-shift-scan-2026-05-25.json`.)

## __declspec(noinline) to Defeat IPA Inlining

When `-ipa file` causes CW to inline a helper that the target left as a `bl` call, mark the helper with `__declspec(noinline)`.

**Symptom in objdiff:** caller is ~70-80%, the diff shows N extra instructions inline at the call site (often inside a switch case), and the helper itself may still be 100% but never actually called.

**Examples:**
- `__declspec(noinline)` on `nandComposePerm` → `nandGetStatus` 74.9% → 100%, `nandGetStatusCallback` 18.5% → 100%.
- `__declspec(noinline)` on `UnsetRun` → `OSSuspendThread` 74.6% → 100%, plus side effects on `SetEffectivePriority`.

Use when ~5-25 extra inline instructions in a partial caller match the body of a helper defined in the same TU. The target was likely built with the helper in a different TU or with different cflags.

## #pragma ipa on (Per-File)

When a group-level `-ipa file` cflag would cause regressions in some TUs, enable IPA per-file via `#pragma ipa on` at the top of the affected `.cpp`.

**Examples:**
- `network/Platform/BandwidthCounter.cpp` + `ProfilingUnit.cpp` — per-file pragma; group-level on `network/Platform` regressed `StringStream`.
- `system/speex/libspeex/ltp.c` — group-level on `system/speex` regressed `bits.c`.
- `system/speex/libspeex/nb_celp.c` — fixed `nb_decode` 87.9% → 100% and `nb_encode` 98.77% → 99.9%.

## TU-Local Conditional Inline Macro

When a stlport (or container) method needs to be **inline in one TU** to match the target but **out-of-line in another TU** to preserve another match.

1. In the header (e.g. `_list.c`):
   ```cpp
   #ifndef _STLP_LIST_CLEAR_INLINE
   #define _STLP_LIST_CLEAR_INLINE
   #endif

   _STLP_LIST_CLEAR_INLINE void _List_base::clear() { /* body */ }
   ```

2. In the TU that needs the inline version (e.g. `StreamTable.cpp`):
   ```cpp
   #define _STLP_LIST_CLEAR_INLINE inline
   #include "stl/_list.h"
   ```

Other TUs leave the macro undefined → they see the non-inline version and continue calling `bl _List_base::clear`.

**Example:** `StreamTable::~StreamTable` 41.6% → 100% (commit 3aa722cf) without regressing 17 previously-100% functions in `SessionSearcher_RV.cpp`.

## STL Allocator Specialization (`_Temporary_buffer<T*, T>`)

The original RB3 binary's `std::stable_sort` path uses Milo's allocator (`_MemAlloc(n, 0)` / `_MemFree(p)`) inside `_Temporary_buffer<T*, T>::_M_allocate_buffer()` and its destructor, instead of stdlib `malloc` / `free`. The shipped `stl/_tempbuf.h` calls `malloc`/`free`, so any TU calling `std::stable_sort` mismatches.

**Fix per TU:** add explicit class template specializations for the concrete element type at file scope (must appear before any sort call site):

```cpp
#include "utl/MemMgr.h"

namespace stlpmtx_std {

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

**Use the 2-specialization variant (`_M_allocate_buffer` + dtor only).** Specializing `_M_initialize_buffer` additionally triggers MWCC error 10335 ("illegal explicit template specialization") because IPA pre-instantiates it at header-include time. The working reference TUs (`AccomplishmentManager.cpp`, `AccomplishmentPanel.cpp`, `TourDescPanel.cpp`, `AccomplishmentDiscSongConditional.cpp`) all use this 2-specialization form.

A `#define malloc(n) _MemAlloc(n, 0)` macro trick does NOT work — MWCC tokenizes template bodies at header-parse time, so a `#define` in the .cpp can't rebind already-parsed templates.

**Wins:**
- AccomplishmentPanel.cpp — 4 `__stable_sort_aux<Symbol*>` instantiations 85.9% → 100% in one edit.
- AccomplishmentManager.cpp + CampaignGoalsLeaderboardChoicePanel.cpp — 86.0% → 100% each via the same specialization.
- TourDescPanel.cpp — created from scratch + this specialization cascaded the **entire** sort template family (`stable_sort`, `__stable_sort_aux`, `__stable_sort_adaptive`, `__merge_adaptive`, etc.) to 100% — **11 functions newly COMPLETE** in one edit.
- AccomplishmentDiscSongConditional.cpp — `InqSongs` to 100% (entire stable_sort path was inlined there).

## Comparator Specialization for `__introsort_loop` / Heap Sorts

When `std::sort` / `std::partial_sort` / `__introsort_loop` mismatches in the inlined comparator path (visible as `mfcr` / `srwi.` / `beq` bool-materialization), define an explicit class template specialization for the sort helpers in the .cpp using direct field comparisons instead of `Comparator::operator()`:

```cpp
namespace stlpmtx_std {

template <>
inline BandPatchMesh::MeshVert** __unguarded_partition(
    BandPatchMesh::MeshVert** __first, BandPatchMesh::MeshVert** __last,
    BandPatchMesh::MeshVert* __pivot, SortByWorkVertZ)
{
    while (true) {
        while ((*__first)->mVert->pos.z < __pivot->mVert->pos.z) ++__first;
        --__last;
        while (__pivot->mVert->pos.z < (*__last)->mVert->pos.z) --__last;
        if (!(__first < __last)) return __first;
        iter_swap(__first, __last);
        ++__first;
    }
}

// ... similar specializations for __introsort_loop, __unguarded_linear_insert ...
}
```

The direct field compare emits `cmpw`/`cmpwi` + `blt`/`bge` instead of an inlined `operator()` call followed by `mfcr` / `srwi.` / `beq` bool materialization.

### Prerequisites — when this pattern actually helps

Confirmed by a 7-TU sweep (2026-05-25): the pattern is narrow. All three conditions must hold:

1. **Comparator uses integer or pointer compares — NOT floats.** Float compares already emit `fcmpo` + `blt` directly, with no bool materialization to eliminate. The residual mismatch on float-comparator sort helpers is volatile-register (f0↔f1/f2) allocation noise that source changes cannot fix.
   - *Negative results:* MessageTimer (`MaxSort`/`ObjSort` on `MaxMs()` float), CameraManager (`NameSort` — target was compiled WITHOUT specialization), BandPatchMesh `SortByZ`. All four are at-limit.

2. **Element type must be trivially copyable.** Smart pointers like `ObjOwnerPtr<T>` have non-trivial copy ctors → MWCC passes the value via hidden-pointer ABI in the generic template, but a specialization written with `T __val` by-value uses the by-value ABI. Different calling convention → fundamentally different codegen, cannot match.
   - *Negative result:* CharClipGroup (`Alphabetically` on `ObjOwnerPtr<CharClip, ObjectDir>`).

3. **The current diff must actually show `mfcr` / `srwi.` / `beq` bool-mat in the partition/insert/heap inner loop.** If the target binary itself was compiled without the specialization (Stats.cpp `PartPercentageSorter`), forcing one will regress — the target's mismatch is something else entirely.

### Caveats

- **Specialization order matters under IPA.** `__adjust_heap` spec must appear BEFORE `__introsort_loop` spec, because `__introsort_loop` calls `partial_sort` → `__adjust_heap` and IPA will pre-instantiate the generic if the spec hasn't been declared yet.
- **`_STLP_INLINE_LOOP` = `inline`** blocks specialization (CW error 10335: "illegal explicit template specialization"). `__push_heap` is currently inline → cannot be specialized in any TU.
- **For `__introsort_loop` median computation, use a scalar field temporary**, not a struct copy: `float __a = __first->mMs;` not `GameGem __a = *__first;`. The struct copy generates field-by-field copies the target's inline expansion doesn't have. UI.cpp `__introsort_loop` hit 100% only after accessing fields directly via `(*__first)->mResourcePath` instead of binding `UIResource *__a = *__first;`.
- **Empty comparator structs (no members) may REDUCE register pressure**, sometimes regressing a function whose target was compiled with the generic call-path version (more callee-saved registers spilled).
- **String/path compares** need to resolve to the exact symbol the target calls — `left->mResourcePath < right->mResourcePath` (calls `String::operator<` → `bl __lt__6StringCFRC6String`) matches; `strcmp(left->mResourcePath.c_str(), right->mResourcePath.c_str()) < 0` does not.

### Wins from this pattern

- BandPatchMesh `__introsort_loop<MeshVert**, SortByWorkVertZ>` 84.5% → 98.9% (original discovery).
- GameGemList `__unguarded_linear_insert<GameGem*, less<GameGem>>` 91.5% → **100%**; `__introsort_loop` 89.5% → **99.4%**; `__unguarded_partition` 93.5% → **98.4%**.
- UI.cpp `__unguarded_linear_insert<UIResource**, Compare>` 84.4% → **100%**; `__unguarded_partition` 95.9% → **100%**.

### Confirmed-not-applicable (do not retry without a new approach)

Logged so future sweeps don't re-attempt these:

| TU | Comparator | Element type | Reason |
|---|---|---|---|
| `system/world/CameraManager.cpp` | `NameSort` | `CamShot*` | Target was compiled WITHOUT specialization; adding it regressed. |
| `system/obj/MessageTimer.cpp` | `MaxSort`, `ObjSort` | `EventEntry*`, `ObjEntry*` | Float comparators (`MaxMs()`) — already use `fcmpo`. Residual is f0↔f1/f2 volatile swaps. |
| `system/char/CharClipGroup.cpp` | `Alphabetically` | `ObjOwnerPtr<CharClip, ObjectDir>` | Non-trivially-copyable element → ABI mismatch (hidden-pointer vs by-value). |
| `system/bandobj/BandPatchMesh.cpp` `SortByZ` | `SortByZ` | `RndMesh::Vert*` | Float compare on `.pos.z`/.y/.x — already `fcmpo`. |
| `band3/game/Stats.cpp` | `PartPercentageSorter` | `pair<int, float>` | Residual is unfixable volatile FPR swaps from inlined `pair.second` (float) compare. Specializations are structurally correct but neutral. |
