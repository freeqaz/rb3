# Wave-Dispatch Session Notes (2026-05-18 → 19)

Patterns confirmed or freshly discovered across waves 64-70. Each is small enough
to belong with the existing `fixable-*` notes but is grouped here while still
fresh.

## 1. Reuse a parameter slot instead of declaring a new local

`GlideToNote` 90 → 100%. The target compiler reused the parameter's stack slot
where the source allocated a fresh local.

```cpp
// Source (90%): allocates a new slot for `num`
int num = std::max(1, i);
mGlideFrames = num;
mGlideFramesLeft = num;

// Source (100%): reuses the parameter's stack slot
i = std::max(1, i);
mGlideFrames = i;
mGlideFramesLeft = i;
```

Apply when objdiff shows a few extra `stw r0, ...` plus matching `lwz` reloads
that all reference the same value, written once and read twice.

## 2. `bool` → `int` return type drops MWCC bool-normalization tail

`QuestFilterPanel::AreCurrentFiltersValid` 0 → 100%.

MWCC appends `neg/or/srwi 31` to coerce any non-zero value to `1` for a `bool`
return. If the target asm does NOT include that tail, change the declaration to
`int` (the call sites still treat the return as truthy).

## 3. Double-precision literals force `lfd / fadd / fmul / frsp` lowering

`Voice::SetPan` 92.7 → 99.7%. Target had double-precision arithmetic that the
float-only source could not reproduce.

```cpp
// Source (92.7%): all-float, single-precision ops
mPan = (clamped + 1.0f) * 63.5f;

// Source (99.7%): double literals promote v to double; final frsp converts
mPan = (1.0 + v) * 63.5;
```

Reuse the parameter directly (no `clamped` local) so the same FPR holds the
value across the conditional arms — eliminates an f2/f3 cascade.

## 4. Function order in the .cpp controls auto-inlining decisions

`FIFOSampleBuffer.cpp` 0% → 13/14 at 100%.

Under `-inline noauto -ipa file`, MWCC still inlines small trivial accessors
when their definition is visible before the call site. Order matters:

- Put `getCapacity` BEFORE `ensureCapacity` → gets auto-inlined into the body.
- Put `rewind` AFTER `ensureCapacity` → call-site emits `bl rewind` instead of
  inlining.

When match% climbs after a reorder, the change actually flipped the inline
decision.

## 5. `bool` materialization as `b ? 1 : 2` defeats branchless fold

`BandLeadMeter::GetColor` blocked at 72.5% because MWCC noticed two non-zero
arms returned values differing by exactly 1, and folded them into a branchless
`srwi/addi` pair. The target uses a branchy `cmpwi/bne/li 0/blr/li 2/blelr/li
1/blr`.

Open question — none of these defeated the fold:

- `return (i < 0) + 1`
- `return ((unsigned)i >> 31) + 1`
- Three sequential `if` statements with separate `color` local

Likely needs a member access, function call, or `volatile` temp the source has
that the decomp does not.

## 6. MemcardMgr-style bitfield + dead null-check pattern

`MemcardMgr_Wii` 4 new 100% matches. Two reusable patterns:

**Bitfield at byte offset:** a single byte field holds two booleans:
- bit 0 → `IsDisableWriting()` returns `b & 1`
- bit 1 → `IsWriteMode()` returns `(b >> 1) & 1`
- Setters mask: `mFlags = (mFlags & ~1) | (val ? 1 : 0);`

**Dead address-of null check** (preserves a no-op `addic./beq` in the asm):
```cpp
if (&this->mBanner != NULL) { _MemFree(mBanner); mBanner = NULL; }
```
The compiler emits the load and branch even though the address can never be
null. Source-level UB; matches the target byte-for-byte.

## 7. Build infra: concurrent `configure.py` corrupts JSON

Multiple waves blocked with `json.decoder.JSONDecodeError: Expecting ...` on
`objdiff.json`, `compile_commands.json`, or `build/SZBE69_B8/config.json`. Root
cause: two agents run `configure.py` simultaneously; one truncates a file the
other is mid-write.

Workaround agents converged on: invoke the compiler directly via wibo, lifting
the cflags from `build.ninja`:

```bash
build/tools/wibo build/compilers/Wii/1.3/mwcceppc.exe \
    -i src/system/stlport -i src/sdk/PowerPC_EABI_Support/MSL/MSL_C \
    ... (full cflag set lifted from build.ninja) ... \
    -c <path>.cpp -o /tmp/<unit>.o
```

Then `cp /tmp/<unit>.o build/SZBE69_B8/<path>.o` and `objdiff-cli diff` against
it. Bypasses the ninja regen step entirely.

## 8. Header-edit constraint is the dominant blocker in matured units

Multiple wave-67/68 dead ends had a single fix in a header outside the function's
.cpp folder:

- **Timer.h** (Ms()/SplitMs() ordering) — blocks ~every Handle/Poll function in
  `system/meta/` and parts of `system/char/`.
- **Vec.h / Mtx.h** (Cross / LookAt fp-contract fusion) — blocks
  `CharUpperTwist::Poll` and similar inline-math users.
- **stlport** (`_M_*` vector helpers) — blocks several `_M_reallocate_map`
  variants.
- **Symbol.h** (`operator==` codegen) — blocks several `strcmp`-based loops.

These are documented as `at_limit` and skipped, but a single header sweep would
fix dozens of partial functions at once.
