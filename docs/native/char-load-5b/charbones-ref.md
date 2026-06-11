# char-Load 5b — CharBonesSamples / CharBones::Bone lane (the "string-len overflow")

**Lane:** `CharBonesSamples::Load` / `LoadHeader` / `LoadData` / `ReadCounts`
(`src/system/char/CharBonesSamples.cpp`) **AND** `operator>>(BinStream&, CharBones::Bone&)`
(`src/system/char/CharBones.cpp:1352`).

**Headline conclusion: THIS LANE IS ALREADY BYTE-CORRECT ON NATIVE. The gate
naming it is STALE.** No source change is needed in `src/system/char/*` for this
lane. The fix is to delete the over-broad gate in `BandHeadShaper::Init` (or
narrow it to the CharClip lane if that lane is found still-broken). Verified
empirically: with the gate disabled, **both** `char/main/head/{male,female}/head.milo_xbox`
load fully and cleanly through the real `RB3_GAME` boot — **zero "String chars … > 512",
zero MILO_FAIL, exit 0, boot reaches the frame loop.**

---

## 1. What the "string-len overflow" actually is

`CharBones::Bone` is `{ Symbol name; float weight; }` (CharBones.h:60-66). The
target body of `operator>>` (verified 100% match, Bank-8 Ghidra +
`bin/analyze-function __rs__FR9BinStreamRQ29CharBones4Bone`):

```
__rs__9BinStreamFR6Symbol(d, (Symbol *)b);   // read Symbol name
ReadEndian__9BinStreamFPvi(d, b + 1, 4);      // read float weight
```

`operator>>(Symbol&)` → `BinStream::ReadString(buf, 0x200)` (BinStream.cpp:42-49):

```c
void BinStream::ReadString(char *c, int i) {  // i = 0x200
    int a;
    *this >> a;                       // 4-byte length, via ReadEndian
    if ((unsigned int)a >= i)
        MILO_FAIL("String chars %d > %d", (unsigned int)(a + 1), i);  // <-- THE OVERFLOW
    Read(c, a);
    c[a] = 0;
}
```

So "String chars 23173 > 512" = `ReadString` read a 4-byte length `a` that came
out as a huge/garbage value (`>= 0x200`). The historical symptom string is even
quoted verbatim in the LoadData V38 comment (CharBonesSamples.cpp:567,578).

## 2. The length read is NOT an endian bug — the primitive is already correct

`*this >> a` → `BS_READ_OP(int)` → `BinStream::ReadEndian` (BinStream.cpp:164-182).
ReadEndian is **already HX_NATIVE-correct**:

```c
void BinStream::ReadEndian(void *data, int bytes) {
    Read(data, bytes);
#ifdef HX_NATIVE
    if (!mLittleEndian) SwapData(data, data, bytes);   // swap when FILE is BE, on LE host
#else
    if (mLittleEndian)  SwapData(data, data, bytes);   // Wii (BE host) swaps when FILE is LE
#endif
}
```

For `head.milo_xbox` the stream's `mLittleEndian` is set correctly to **false**:
- `ChunkStream::Eof()` detects `.milo_xbox` → `SetPlatform(kPlatformXBox)` (ChunkStream.cpp:263).
- `SetPlatform` → `mLittleEndian = PlatformLittleEndian(kPlatformXBox)` (ChunkStream.cpp:116).
- `PlatformLittleEndian(kPlatformXBox)` = **false** (System.cpp:191-197 — only PC/None are LE; Xbox360 is big-endian).

Therefore every `bs >> int/float/short` in this lane (numBones, the Symbol
length-prefix, weight, mCounts[], mCompression, mNumSamples, Vector3/Quat
components) is byte-swapped BE→LE correctly on native. The string-length field
itself reads correctly.

Also relevant: the milo **chunk header** (first 0x810 bytes — magic `afde beca`
= 0xCABEDEAF, chunkInfoSize, numChunks) is stored **LITTLE-ENDIAN on disk for
both xbox and wii** (confirmed via xxd of both fixtures), and the `ChunkStream::Eof`
header byteswaps are `#ifndef HX_NATIVE` (Wii-only) — already correct. Only the
decompressed **payload** is platform-BE, and that is what ReadEndian handles.

## 3. Real root cause of the historical overflow: a LoadData stream DESYNC — ALREADY FIXED (V38)

The overflow was never in the length read's *value*; it was a **stream
position** desync. The extracted assets are big-endian **cached `.milo_xbox`**
files whose Xbox/PS3 Save path stores `CharBonesSamples` sample data in a
**PADDED** layout: each uncompressed POS/SCALE `Vector3` is followed by a pad
float (16 bytes on disk), and each per-sample block is rounded up to a 16-byte
boundary. The Wii-native `LoadData` reads a tightly-packed stream; reading the
unpadded layout against the padded file leaves the stream N bytes short → the
*next* `bs >> mZeros` (a `vector<Bone>`) or the next `CharBones::Bone` name read
lands on garbage → `ReadString` sees length 23173 → MILO_FAIL.

This was fixed in **commit `902bcf37` (2026-05-29)** — `CharBonesSamples::LoadData`
has an `#ifdef HX_NATIVE` "cached & xbox/ps3" branch (CharBonesSamples.cpp:558-686)
that consumes the per-Vector3 pad float and the per-sample 16-byte realignment
padding. Its own comment (lines 574-583, "V38") documents that **removing the +4
pad immediately re-triggers `String chars 23173 > 512`** — i.e. with the pad
read in place the stream stays aligned. (DC3's equivalent fix is the
`SwapBE_Section` bulk-read-then-swap in dc3 CharBonesSamples.cpp:112-272; RB3
instead reads element-wise via `bs >> *p`, which already swaps per-element.)

## 4. Why the gate is STALE — timeline

| commit | date | what |
|---|---|---|
| `65892986` | 2026-05-27 18:57 | **Added the gate** `_tmp0/_tmp1 = false` in `BandHeadShaper::Init`, with the "CharBonesSamples.cpp:457 / CharBones.cpp:1354 string-len overflow" comment. |
| `902bcf37` | 2026-05-29 02:05 | **V38 LoadData padding fix** that resolved that exact `String chars 23173 > 512` desync. |

The gate was put up ~1.3 days **before** the fix that made the path work, and was
never re-evaluated.

## 5. EMPIRICAL PROOF (decisive)

Temporarily replaced the gate with `if (getenv("RB3_HEADSHAPE_TEST")==0) _tmp0/1=false;`
in the **main** repo, rebuilt `rb3-native`, ran the real boot, then hand-reverted
to pristine (git diff empty) and rebuilt.

- `MILO_HEADLESS=1 RB3_GAME=1 RB3_HEADSHAPE_TEST=1 ./native/build-native/rb3-native`
  → both `char/main/head/male/head.milo` and `…/female/head.milo` load fully
  (meshes instantiate; only benign "Skinned mesh needs to be re-exported" RndMesh
  content NOTIFYs, which fire AFTER the full CharClip/CharBonesSamples parse).
- Overflow/desync hit count across the whole boot: **0**
  (`grep -icE "String chars|chars [0-9]+ > 512|can't load.*CharBonesSample|d.rev > 12"` → 0).
- Boot proceeds: `App constructed → Run() → frame loop → 5 frames → exit cleanly` (exit 0).

Header-only direct load also clean: `./native/build-native/rb3-native
orig-assets/extracted/char/main/head/{male,female}/gen/head.milo_xbox` → milo rev 28,
52/54 objects, exit 0.

## 6. objdiff match-neutrality (LIVE char/* functions)

No char/* source change is proposed, so match% is untouched. For the record,
current baselines (already include the existing `#ifdef HX_NATIVE` LoadData branch,
which is excluded from Wii codegen):

- `operator>>(BinStream&, CharBones::Bone&)` — **100.0% normalized**
- `CharBonesSamples::LoadData(BinStream&)` — **100.0% normalized**

## 7. Concrete fix for the fix-step

**Primary (this lane):** No change to `src/system/char/*`. The lane is correct.

**The unblock** lives in `src/system/bandobj/BandHeadShaper.cpp` (already 100%
HX_NATIVE-gated — NOT a live-objdiff function, so removing the gate is
match-neutral by construction):

- Remove `_tmp0 = false;` at line ~137 and `_tmp1 = false;` at line ~156 (and the
  stale comment block at 131-137), OR narrow the gate so it ONLY suppresses the
  CharClip lane if the parallel `CharClip::Load` lane is confirmed still-broken.
- **Coordinate with the CharClip lane** before flipping unconditionally: the
  empirical RB3_GAME test above exercised BOTH `mFull.Load`/`mOne.Load`
  (CharBonesSamples) AND `CharClip::Load` (the `bs >> mZeros` + version path) on
  the real head milos and **all of it passed** — strong evidence the CharClip
  lane is ALSO already fixed for these assets. The fix step should confirm with
  the CharClip lane's doc, then remove the gate.

**Gating verdict:** the unblock edit is in already-HX_NATIVE-gated bandobj code →
**match-neutral / no HX_NATIVE gate needed in char/***. The Wii build is
byte-identical (the `#ifdef HX_NATIVE` body of `BandHeadShaper::Init` is excluded
from Wii codegen entirely).

## Key file:line references
- `src/system/utl/BinStream.cpp:42` `ReadString` (the overflow MILO_FAIL)
- `src/system/utl/BinStream.cpp:51` `operator>>(Symbol&)` → ReadString(buf,0x200)
- `src/system/utl/BinStream.cpp:164` `ReadEndian` (HX_NATIVE-correct swap)
- `src/system/os/System.cpp:191` `PlatformLittleEndian` (Xbox=false)
- `src/system/utl/ChunkStream.cpp:116/263` SetPlatform → mLittleEndian for `.milo_xbox`
- `src/system/char/CharBonesSamples.cpp:459` `Load` / `:482` `LoadHeader` / `:554` `LoadData` (V38 padded-read fix 558-686) / `:466` `ReadCounts`
- `src/system/char/CharBones.cpp:1352` `operator>>(BinStream&, Bone&)`
- `src/system/bandobj/BandHeadShaper.cpp:137,156` — the STALE gate to remove
