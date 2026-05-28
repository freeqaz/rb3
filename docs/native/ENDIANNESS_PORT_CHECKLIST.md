# Endianness Port Checklist — Serialized Bitfields

Per-struct shim checklist for the native (x86/ARM, little-endian) port. Scope:
the 17 HIGH-risk bitfield structs flagged by
`scripts/analysis/endianness_audit.py --risk high`. Analysis only — no source was
modified.

## 1. The MSB/LSB bitfield hazard (read this first)

The Wii/PS3/360 build was compiled with MWCC (and equivalents), which lay out C
bitfields **MSB-first**: the first declared field occupies the *high* bits of the
storage word. A modern host compiler (clang/gcc/MSVC on x86/ARM) lays bitfields
out **LSB-first**: the first declared field occupies the *low* bits. So if a
bitfield struct's *raw storage word* is ever persisted (saved to disk, embedded
in a `.milo` blob, packed into store metadata, or sent over the wire), reading
that same word back on a little-endian host decodes the fields into the **wrong
positions** — even after the engine byte-swaps the storage word. `ReadEndian`
fixes byte order within the word; it does **not** fix the bit *positions* within
that word. The hazard is invisible to the decomp match% metric: the Wii build is
big-endian, so it matches the target perfectly. Corruption only manifests on a
little-endian host.

The decisive question for each struct is therefore **not** "does it contain
bitfields" but "**is the packed storage word itself the unit of serialization?**"

- If each field is read/written as **its own scalar** (`bs >> intLocal; member =
  intLocal;`), the packed word never crosses the wire. The on-disk value is a
  plain integer that `ReadEndian` byte-swaps correctly, and the host's bitfield
  layout is purely an internal runtime detail. **SAFE — no port action.**
- If the **whole word** is the serialized unit (raw blob `memcpy`/pointer-cast,
  or `bs << *(int*)&flags`), the bit positions are baked into the file in MSB-first
  order and a little-endian host mis-decodes them. **NEEDS BIT-REMAP.**

## 2. Per-struct classification

Classes: **A** = SAFE (field-by-field), **B** = NEEDS BIT-REMAP (word is the
wire unit, no fix exists), **C** = COVERED BUT VERIFY (an `EndianFix()` already
re-extracts bits), **D** = RUNTIME-ONLY (false HIGH; never persisted).

| Struct | File | Class | Affected word / fields | What the port must do |
|---|---|---|---|---|
| RndMat | system/rndobj/Mat.h | **A** | flag word @0xAC–0xAD; mBlend/mZMode/mTexGen/mTexWrap/mStencilMode/mShaderVariation/mIntensify/mUseEnviron/mPreLit/mAlphaCut/mAlphaWrite/mCull/mPointLights/mFog/mFadeout/mColorAdjust/mScreenAligned/mRefractEnabled | Nothing. Every persisted bit read via `LOAD_BITFIELD`/`LOAD_BITFIELD_ENUM` as its own scalar (Mat.cpp:67,69-77,80,83,124,174,184-191,211,221,262). mDirty/mPerPixelLit/unk_0xAD_* not persisted. |
| MatPerfSettings | system/rndobj/Mat.h | **A** | mRecvProjLights/mRecvPointCubeTex/mPS3ForceTrilinear | Nothing. `LOAD_BITFIELD(bool, ...)` per field (Mat.cpp:32-35). |
| RndText | system/rndobj/Text.h | **A** | mFixedLength/mDeferUpdate/mTextMarkup | Nothing. `bs >> fixedLength` int (Text.cpp:174,185); `LOAD_BITFIELD(bool, mTextMarkup)` (Text.cpp:199). mDeferUpdate/unk124* are runtime. |
| RndDrawable | system/rndobj/Draw.h | **A** | 32-bit flag word @0x8; only mShowing persisted | Nothing. `bs >> bool; mShowing = ...` (Draw.cpp:115-117). All other 31 bits are runtime caches owned by subclasses (and where a subclass persists one, e.g. mTextMarkup, it does so field-by-field). |
| EventTrigger | system/rndobj/EventTrigger.h | **A** | mEnabledAtStart (others runtime) | Nothing. `LOAD_BITFIELD(bool, mEnabledAtStart)` (EventTrigger.cpp:461). mEnabled/mWaiting/mTriggered/unkdf are runtime state. |
| PropKeys | system/rndobj/PropKeys.h | **A** | mKeysType/mInterpolation/mPropExceptionID/unk18lastbit (mLastKeyFrameIndex runtime) | Nothing. Each read via `bs >> iVal; member = iVal;` / `bs >> b` (PropKeys.cpp:190-191,196/204,215-216,221-222). |
| UILabel | system/ui/UILabel.h | **A** | mMarkup/mUseHighlightMesh/mAltStyleEnabled | Nothing. `LOAD_BITFIELD(bool, ...)` per field (UILabel.cpp:128,181,185). |
| CamShot | system/world/CameraShot.h | **A** | mLooping/mUseDepthOfField (others runtime) | Nothing. `bs >> bitfield_bool; member = ...` (CameraShot.cpp:613-614,623-624,652-653). mPS3PerPixel/mShotOver/mHidden/unk120* are runtime. |
| CamShotFrame | system/world/CameraShot.h | **A** | mBlendEaseMode/mUseParentNotation/mParentFirstFrame | Nothing. `int b; bs >> b; member = b` / bool locals (CameraShot.cpp:898-900,964-966,980-982). |
| TrackWidget | system/track/TrackWidget.h | **A** | mWideWidget/mAllowRotation/mAllowShift/mAllowLineRotation/mWidgetType/mCharsPerInst/mMaxTextInstances | Nothing. `LOAD_BITFIELD(bool, ...)` + `bs >> u; member = u` (TrackWidget.cpp:72,89,135,138,104-119). mActive/mMaxMeshes runtime/derived. |
| BandCamShot::Target | system/bandobj/BandCamShot.h | **A** | mForceLod/mTeleport/mReturn/mSelfShadow/unk1/unk2/mHide | Nothing. `operator>>` reads each via a char/int local (BandCamShot.cpp:82-83,94-95,118-119,123-132,156-162). |
| StorePackedSong | system/meta/StoreOffer.h | **C** | unka:9 @0xa, unk10:9 @0x10 | VERIFY/port `StorePackedSong::EndianFix()` (StorePackedMetadata.cpp:2067). It already re-extracts bits from raw bytes — see §3 for the LE-host adjustment. |
| StorePackedRanks | system/meta/StoreOffer.h | **C** | 12-byte blob, 9×10-bit ranks | VERIFY/port `StorePackedRanks::EndianFix()` (StorePackedMetadata.cpp:935). Reference example of correct bit-remap; see §3. |
| StorePackedOfferBase | system/meta/StoreOffer.h | **C** | bytes @0x0,0x1 (mIsRBN..mVocalParts) + mRanks | VERIFY/port `EndianFixBase()` (StorePackedMetadata.cpp:2087). |
| StorePackedRBNOffer | system/meta/StoreOffer.h | **C** | byte @0x43 (mSubGenre:7/mLanguage:3/unk44:5) | VERIFY/port `StorePackedRBNOffer::EndianFix()` (StorePackedMetadata.cpp:2113) → EndianFixBase + mIsRBN set. (byte @0x43 itself currently has no per-bit remap; see §3 note.) |
| StorePackedPage | system/meta/StorePackedMetadata.h | **C** | bytes @6,@7 (unk6p0/unk6p1/mDefaultSort/mHasOffers/unk6p3) | VERIFY/port `StorePackedPage::EndianFix()` (StorePackedMetadata.cpp:960). |
| ObjPtrList | system/obj/ObjPtr_p.h | **D** | mSize:24 / mMode:8 — **never serialized** | Nothing. `Load()` writes `int count` + object-name strings then rebuilds the list; the `mSize:24/mMode:8` word is pure runtime container bookkeeping (ObjPtr_p.h:576-604). Downgrade from HIGH — the `Load` signal was a coincidence. |

**Tally: A=11, B=0, C=5, D=1.**

## 3. Patterns / generalized fixes

### Pattern A — field-by-field is already endian-safe (do nothing)
The engine's standard `.milo` serialization macros guarantee safety:

```c
// src/system/obj/ObjMacros.h:497
#define LOAD_BITFIELD(type, name) { type bs_name; bs >> bs_name; name = bs_name; }
#define LOAD_BITFIELD_ENUM(type, name, enum_name) \
        { type bs_name; bs >> bs_name; name = (enum_name)bs_name; }
```

`bs >> bs_name` dispatches the typed `BinStream::operator>>` (BinStream.h:181-209),
which `ReadEndian`s **one scalar**. The packed bitfield word is never the wire
unit; the bit *positions* are an internal post-read assignment that the host
compiler is free to lay out however it likes. **Every `.milo` Rnd/UI/track object
in scope uses this idiom (or the equivalent `bs >> local; member = local;`), so
all 11 are class A.** `SAVE_OBJ` (ObjMacros.h:367) is an `MILO_ASSERT(0)` stub —
`.milo` objects are load-only at runtime, so only the read path matters.

Rule for the port: keep loading bitfields one scalar at a time. Do **not**
"optimize" a Load by reading the whole flag word as one `int` — that would
*introduce* the MSB/LSB bug.

### Pattern C — the StorePacked* blob path (the reference for "doing it right")
Store metadata files are pulled from a server as a fixed-wire-format blob and
mapped in by **pointer-cast, not BinStream** (e.g.
`mSongs = (StorePackedSong*)loc12c`, StorePackedMetadata.cpp:305;
`mPage = (StorePackedPage*)buffer`, :617; `mOffers = (StorePackedRBNOffer**)dataStart`,
:474), then `EndianFix()` is called **unconditionally** (no platform gate). So the
MSB-first bit packing *is* baked into the file, and `EndianFix` is the bit-remap.

`StorePackedRanks::EndianFix` (StorePackedMetadata.cpp:935) is the canonical
correct shim — it reads raw bytes and re-assembles each 10-bit field by hand:

```c
unsigned char buf[12]; memcpy(buf, this, 12);
rank = ((buf[0] & 0x3F) << 4) | (buf[1] >> 4);  mGuitar = rank;   // MSB-first decode
rank = ((buf[1] & 0x0F) << 6) | (buf[2] >> 2);  mVocals = rank;
...
```

Note it does **bit re-extraction**, not merely a byte swap. That is exactly what
the port needs. **The LE-host catch:** the *input* side (the `buf[i]` byte reads
and the `<<`/`>>`/mask arithmetic) is endian-neutral and already correct, but the
*output* side — `mGuitar = rank` — stores into a **host-native LSB-first
bitfield**. On the Wii the destination layout happens to match the MSB-first
order EndianFix reconstructs; on a little-endian host the same assignment lands
the value in the wrong bits. Port options, in order of safety:

1. **Preferred — decode into a POD mirror struct, not a bitfield.** Replace the
   packed bitfield members with plain `u16`/`u32` fields (no `:N`) and keep the
   `EndianFix` byte arithmetic verbatim. With no host bitfields involved, layout
   is unambiguous and the existing decode "just works" on any endianness.
2. **Or — write a host-endian-aware bit accessor.** Provide `GetField(word,
   shift, width)` / `SetField(...)` helpers that always index bits MSB-first
   regardless of host, and route every blob-bitfield read through them. This
   localizes the hazard to one audited primitive.

Whichever is chosen, apply it uniformly to all five class-C structs
(StorePackedSong/Ranks/OfferBase/RBNOffer/Page). Watch the structs that have a
declared bitfield with **no** corresponding remap line: e.g. `StorePackedRBNOffer`
declares `mSubGenre:7 / mLanguage:3 / unk44:5` at byte 0x43 but its `EndianFix`
(StorePackedMetadata.cpp:2113) only calls `EndianFixBase()` + sets mIsRBN — byte
0x43 is consumed as a single byte by `SubGenre()`/`Language()` accessors, which on
a host with LSB-first packing would read the wrong sub-fields. Verify those
accessors against the wire byte during the port.

### General port rule
"Byte-swapping is not bit-swapping." Any persisted bitfield must be decoded by
explicit bit arithmetic from defined byte offsets (the StorePacked* approach), or
loaded field-by-field as scalars (the `.milo` approach). Never `memcpy`/pointer-cast
a foreign-authored blob directly onto a host bitfield struct and assume a word
byte-swap suffices.
