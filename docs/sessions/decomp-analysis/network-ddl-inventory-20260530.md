# Network DDL/DDF Inventory — 2026-05-30

**Context**: Remote session without orig/SZBE69_B8/ or toolchain. Analysis-only.
Comprehensive inventory of MISSING/NonMatching network DDL files with function
sizes, patterns, and implementation roadmap for future toolchain sessions.

## Summary

| Category | Count | Total bytes |
|---|---|---|
| MISSING network DDL/DDF | 31 | ~100 KB |
| NonMatching network DDL/DDF | 8 | ~18 KB |
| Total | 39 | ~118 KB |

Overall network status: **16.13% matched** (1,278 / 4,579 functions). The
DDL/DDF files above represent the most patterned, highest-ROI subset.

---

## Tier A — DDF Registration Files (224–448 bytes each, all MISSING)

These follow a rigid 4-function template: `Register()` + `Init()` + destructor +
static initializer. The destructor and static initializer are always exactly 88
bytes (they're identical template instantiations). `Init()` size tracks how many
DO classes get registered.

| File | Text | Init size | Notes |
|---|---|---|---|
| `network/Extensions/AnyExtDDF.cpp` | 224 | 16 | Trivial; 1 registration |
| `network/Extensions/AVStreamsDDF.cpp` | 224 | 16 | Trivial; 1 registration |
| `network/Extensions/STLExtDDF.cpp` | 224 | 16 | Trivial; 1 registration |
| `network/Extensions/DupSpaceExtDDF.cpp` | 304 | 88 | 2–3 registrations |
| `network/Extensions/VoiceChatExtDDF.cpp` | 288 | 80 | 2 registrations |
| `network/Services/MatchMakingServiceDDF.cpp` | 272 | 60 | ~2 registrations |
| `network/Services/ProtocolFoundationDDF.cpp` | 272 | 60 | ~2 registrations |
| `network/Extensions/SessionClockExtDDF.cpp` | 352 | 100 | 2–3 + extra init |
| `network/net/HarmonixGameDDF_Wii.cpp` | 400 | 120 | ~3 registrations |
| `network/ObjDup/DOCoreDDF.cpp` | 448 | 164 | 4–5 registrations; central |

### Implementation Pattern

Reference: `src/network/ObjDup/DDLDeclarations.h` + existing matching EXT files.

Each DDF file needs:
1. A concrete `XXXDDLDeclarations : public DDLDeclarations` class in a header
2. `Register()` — calls `RegisterIfRequired()` (12 bytes = 1 bl + blr)
3. `Init()` — calls `DOClassesTable::RegisterDOClass(...)` for each DO class
4. Destructor — base class virtual dtor chain (template; same 88 bytes for all)
5. Static initializer `__sinit` — constructs the static `XXXDDLDeclarations` object

**No source or header exists for any of these.** All need to be created from scratch.
Requires Ghidra to identify which DO classes get registered in `Init()`.

### Quick-win: AnyExtDDF / AVStreamsDDF / STLExtDDF

Init is only 16 bytes = 1 function call. Likely `DOClassesTable::RegisterDOClass`
with one argument. Look up the DO class type registered in each via Ghidra at:
- `Init__Q26Quazal21AnyExtDDLDeclarationsFv` @ 0x800BB000
- `Init__Q26Quazal24AVStreamsDDLDeclarationsFv` @ 0x800BB720
- `Init__Q26Quazal21STLExtDDLDeclarationsFv` @ 0x800C9630

---

## Tier B — _DDL_X and _DS_X Classes (NonMatching, have source)

These files compile but don't match. Analysis of why follows.

### GameSessionDDL / RankingDDL / TournamentDDL (each 304 bytes)

```
NonMatching  304b  network/Services/GameSessionDDL.cpp
NonMatching  304b  network/Services/RankingDDL.cpp
NonMatching  304b  network/Services/TournamentDDL.cpp
```

**Root cause (high confidence):** All three call `_DDL_Competition::Add` /
`_DDL_Competition::Extract`, which are static methods in the MISSING
`CompetitionDDL.cpp`. Without that file, the linker can't resolve these symbols,
causing the .o to build with wrong relocation targets → NonMatching.

**Fix:** Implement `CompetitionDDL.cpp` first (see Tier C). Then these three
will likely flip to Matching with no additional changes.

Function sizes for each (RankingDDL as example):
```
Clone:          88 bytes  (new(__FILE__, N) Ranking)
GetGatheringType: 16 bytes  (return "Ranking")
IsA:            20 bytes  (IsEqual check)
IsAKindOf:     120 bytes  (IsEqual + parent chain)
StreamIn:       16 bytes  (bl _DDL_Competition::Add)
StreamOut:      16 bytes  (bl _DDL_Competition::Extract)
```

**Line number in Clone**: The `new (__FILE__, LINE)` call embeds the source
line number. With Ghidra, read the string at `__RTTI__Ranking::Clone` to find
the exact line. Check current files for correctness.

### DynamicGatheringDDL (464 bytes, NonMatching)

```
Clone:         128 bytes  (new(__FILE__, N) DynamicGathering — larger than others)
GetGatheringType: 16 bytes
IsA:            20 bytes
IsAKindOf:     120 bytes
StreamIn:       76 bytes  (calls _Type_buffertail::Add)
StreamOut:      76 bytes  (calls _Type_buffertail::Extract)
```

StreamIn/StreamOut call `_Type_buffertail::Add` (for a Buffer field). The 76-byte
size vs 16 bytes for StreamIn in GameSessionDDL suggests `_Type_buffertail` expands
inline. Current impl looks correct; check Clone line number + _Type_buffertail
Add/Extract signatures.

### NintendoTokenDDL (480 bytes, NonMatching)

```
Clone:         144 bytes  (new + field copy chain)
GetDataType:    16 bytes
IsA:            20 bytes
IsAKindOf:     120 bytes
StreamIn/Out not listed (must be beyond the 480-byte split end)
```

Note: `_DDL_NintendoToken` extends `_DDL_Data` (uses `GetDataType`, not
`GetGatheringType`). Clone is 144 bytes — larger than Gathering-style clones.
Likely copies string fields with assignment operators.

### ChannelMembersDDL (240 bytes, NonMatching)

```
FormatVariableValue: 52 bytes
AddSourceTo:        148 bytes  (iterates qList<VoiceChannelMember>)
CallOperationOnVars:  4 bytes  (empty)
```

The 148-byte AddSourceTo iterates a list. Current implementation looks correct
in structure. `qList` iteration is straightforward. Check for type mismatch or
missing dtor for intermediate iterators. The `_DDL_VoiceChannelMember::Add`
call may be the culprit — check if it should be `static` per the DDL pattern doc.

---

## Tier C — MISSING DDL Files (need Ghidra + from-scratch C)

### CompetitionDDL.cpp (1520 bytes, MISSING, has header)

Critical blocker — GameSession/Ranking/Tournament all depend on it.

```
Add:            196 bytes  static method
Extract:        424 bytes  static method (complex — list iteration)
Clone:           60 bytes
GetGatheringType: 16 bytes
IsA:            20 bytes
IsAKindOf:      120 bytes
StreamIn:       204 bytes  (inline Add + list iteration)
StreamOut:      432 bytes  (inline Extract + list iteration)
```

The CompetitionDDL.h reveals:
```cpp
String m_str28;      // at 0x28 after Gathering fields
// qList at 0x2c - two pointer slots (head/tail placeholders)
```

The large Extract (424) and StreamOut (432) sizes confirm list iteration.
Need Ghidra to identify the list element type (likely `String` or a small POD).
Once known, apply the `qList<T>::clear()` + `push_back` pattern from the
GatheringStatsDDL doc note.

### GatheringStatsDDL (304 bytes) / GatheringURLsDDL (304 bytes)

Both have only ONE function: `Extract`. No `Add`, no virtual methods.
These are `_DDL_X` classes used only for deserialization from the server.

Note from quazal-ddl-pattern.md: **skip** — require qList `clear()` + `push_back`
intrusive-list alloc patterns. Attempt after the qList allocation pattern is
mastered (see ChannelMembersDDL above).

### RVConnectionDataDDL (288 bytes, MISSING)

Only `Extract` (276 bytes). Similar skip note — uses complex extraction.

### RemoteLogDeviceProtocolDDL (240 bytes, MISSING)

Only `DispatchProtocolMessage` (232 bytes). Protocol message dispatch function.
Check the NintendoManagementProtocolDDL.cpp for the same pattern; it's more
complex (has a switch on method ID).

---

## Tier D — Large MISSING DDL Files (need significant Ghidra work)

| File | Text | Functions |
|---|---|---|
| `network/ObjDup/IDGeneratorDDL.cpp` | 3504 | `_DOC_IDGenerator` + DOClassTemplate<> |
| `network/ObjDup/PromotionRefereeDDL.cpp` | 3312 | `_DOC_PromotionReferee` + DOClassTemplate<> |
| `network/ObjDup/RootDODDL.cpp` | 2144 | `_DO_RootDO` + many stubs |
| `network/ObjDup/SessionDDL.cpp` | 8080 | `_DO_Session` + `DOClassTemplate<>` |
| `network/ObjDup/StationDDL.cpp` | 6992 | `_DO_Station` + `DOClassTemplate<>` |
| `network/Extensions/SessionClockDDL.cpp` | 2432 | `_DO_SessionClock` + `_DOC_SessionClock` |

These contain `DOClassTemplate<_DO_X, _DOC_RootDO>` instantiations with methods:
`SpecificUpdate`, `SpecificRefresh`, `SpecificAddDSToDiscoveryMessage`,
`SpecificExtractDSFromDiscoveryMessage`, `SpecificExtractADataset`,
`ValidCastTowards` (each 0x10–0x248 bytes). These require full Ghidra analysis.

---

## Implementation Priority Order (for toolchain session)

1. **CompetitionDDL.cpp** — unlocks GameSession/Ranking/Tournament NonMatching → Matching
2. **AnyExtDDF / AVStreamsDDF / STLExtDDF** — trivial 4-function files; verify Init() with Ghidra
3. **DupSpaceExtDDF / VoiceChatExtDDF** — slightly larger Init but same pattern
4. **MatchMakingServiceDDF / ProtocolFoundationDDF** — same DDF pattern
5. **DOCoreDDF.cpp** — largest DDF; central registration table
6. **RootDODDL.cpp** → **SessionDDL / StationDDL** — large but high-value (enable full ObjDup subsystem)

---

## Pattern Reference

- DDF files: `docs/decomp/patterns/quazal-ddl-pattern.md`
- qList intrusive-list alloc: session notes in `wave-session-2026-05-23.md`
- `_DDL_X` static Extract/Add: see `InvitationDDL.cpp`, `FriendDataDDL.cpp`
- `_DS_X` DataSet: see `RangeDDL.cpp`, `StationStateDDL.cpp`, `SessionStateDDL.cpp`
- `_DO_X` constructor: `_DO_VoiceChannel::_DO_VoiceChannel()` in `VoiceChannelDDL.cpp`
- `_DOC_X` class: see symbol map for `_DOC_SessionClock` methods
- `DOClassTemplate<X,Y>` instantiation: see `_DO_VoiceChannel` symbols in symbols.txt

---

---

## Special Case: VoiceChannelDDL.cpp (6944 bytes, NonMatching, vastly incomplete)

The current source has only 2 lines (a bss `s_uiClassID` definition and constructor).
The file requires **44 functions** across 5 class types:

| Class | Functions needed |
|---|---|
| `_DOC_VoiceChannel` | Create, Delete, GetClassNameString, ApproveFaultRecovery, ApproveEmigration, Trace, ctor (364 bytes!), dtor, IsAKindOf, DataSetsOperation (496 bytes), FormatVariableValue (176 bytes), DispatchAction, DispatchRMCCall **(1216 bytes)**, DispatchRMCResult (260 bytes), FillDupSpacesInfo, GetDatasetNameString |
| `_DO_VoiceChannel` | ctor (92 bytes), InitDOClass (88 bytes), CreateWellKnown (100 bytes), CallOperationOnDatasets (324 bytes), GetAvailableStreamReturnStub (120 bytes), HandleJoinReturnStub (104 bytes), CallHandleVoicePacket (300 bytes), CallReclaimStream (272 bytes) |
| `BasicUpdateProtocol<ChannelMembers>` | UsesSpecialInnerUpdateLoop, GetCommunicationFlags, AddToMessage (64 bytes), ExtractFromMessage (316 bytes), dtor (64 bytes) |
| `BasicUpdateProtocol<ChannelInfo>` | Same 5 functions as above but smaller |
| `DOClassTemplate<_DO_VoiceChannel, _DOC_RootDO>` | SpecificUpdate (428 bytes), SpecificRefresh (168 bytes), SpecificAddDSToDiscoveryMessage (320 bytes), SpecificExtractDSFromDiscoveryMessage (292 bytes), SpecificExtractADataset (328 bytes), ValidCastTowards (36 bytes), dtor (96 bytes) |

**Requires full Ghidra analysis.** This is one of the most complex MISSING implementations.
Start with RootDODDL.cpp → SessionDDL.cpp → StationDDL.cpp to understand the
`DOClassTemplate<>` and `_DOC_X` patterns before tackling VoiceChannelDDL.

Similarly incomplete (NonMatching but missing most body):

- `MessageBrokerDDL_Wii.cpp` (1680 bytes): needs `_DOC_MessageBroker` + `DOClassTemplate<>`
  instantiations; current source has `s_uiClassID` + constructor only.

---

## NonMatching Network Files by Size (excluding DDL)

Top 10 smallest NonMatching non-DDL files (highest-ROI for quick wins):

| File | Text | Notes |
|---|---|---|
| `network/RVPackages/JobNintendoUpdateNameByGuest.cpp` | 96 | Only has include; needs `~NintendoManagementProtocolClient` body |
| `network/ObjDup/Range.cpp` | 144 | `Range::Range()` + `Range::~Range()` — base class inits |
| `network/Plugins/RTT.cpp` | 160 | Adjust() formula looks correct; check uint size/cast |
| `network/ObjDup/DOFilter.cpp` | 192 | Need to check DOFilter.h and implementation |
| `network/Extensions/ChannelMembersDDL.cpp` | 240 | Covered above (Tier B) |
| `network/Core/Operation.cpp` | 368 | Check virtual dtor chain |
| `network/Platform/StringConversion.cpp` | 368 | Likely string conversion utilities |
| `network/Platform/Platform.cpp` | 384 | Platform-specific init stubs |

JobNintendoUpdateNameByGuest.cpp is interesting: the symbol
`__dt__NintendoManagementProtocolClient` lives in its text range (0x800F7E90,
92 bytes). The file currently only has `#include "NintendoManagementProtocolClient.h"`
with no body — the destructor implementation is literally missing.
