# Pair 18 verdict — CORRECT (high confidence)

**Identity:** Wii `SetLoadedPrefabChar__8BandUserFi` (Bank 8 `0x80162c30`) == Xenon `0x8266ecb0`
**Match type:** BSIM, sim 1.0 / confidence 16.152 (simconf 16.152), stratum BSim 15–20.

## Verdict: CORRECT, confidence HIGH

The Xenon function at `0x8266ecb0` is compiled from the same Harmonix source as
`BandUser::SetLoadedPrefabChar(int)`.

## Decisive evidence

The Wii body is a 3-call composition thunk:
`SetChar(this, GetDefaultPrefab(GetPrefabMgr(), arg))` — a 76-byte function
(`0x80162c30..0x80162c78`, size `0x4c` from CW map line 7525).

The Xenon pseudo-C is the identical skeleton with matching data-flow:
```c
uVar1 = FUN_82540840();            // GetPrefabMgr()
uVar1 = FUN_826fbef0(uVar1, param_2);   // mid(PrefabMgr*, int)
Function_8266EAF8(param_1, uVar1);      // SetChar(this, CharData*)
```
Xenon body size = **76 bytes** → size ratio exactly **1.0** (well inside the
expected 1.0–2.5x MSVC/Wii band; here it's a pure pass-through thunk so 1.0 fits).

### Callee corroboration (the strongest signal)
Confirmed against `matches.json` (joined by bare-hex Wii addr from the CW map):

| call # | Wii callee | Wii addr | Xenon addr | match | BSim sim/conf |
|---|---|---|---|---|---|
| 1 | `GetPrefabMgr__9PrefabMgrFv` | 0x80344f10 | 0x82540840 | ✓ exact | 1.0 / 8.573 |
| 2 | `GetDefaultPrefab__9PrefabMgrCFi` | 0x803468f0 | 0x826fbef0 | (see below) | — (unmatched) |
| 3 | `SetChar__8BandUserFP8CharData` | 0x80162a40 | 0x8266eaf8 | ✓ exact | 0.139 / -9.915 |

Calls #1 and #3 resolve **exactly** to the two callees the Wii body uses.

### The "TrackName" callee is a resolution artifact, not a contradiction
The evidence pack labels callee #2 (`0x826fbef0`) as `TrackName__8SongDataCFi`.
That label is **spurious** and does NOT indicate a wrong match:
- `GetDefaultPrefab__9PrefabMgrCFi` (Wii 0x803468f0) is **absent from
  matches.json** (it was never matched). With the true callee unmatched, BSim
  attached the Xenon callee `0x826fbef0` to `TrackName` (Wii 0x80652830) at a weak
  **sim 0.748 / conf 10.796** — a classic collision between two tiny index
  accessors (`GetDefaultPrefab(int)` 144 B vs `TrackName(int)` 20 B, both
  small `this+index` getters).
- The middle Xenon call receives `GetPrefabMgr()`'s return (a `PrefabMgr*`) as its
  first arg plus the int — i.e. `GetDefaultPrefab(PrefabMgr* this, int)`.
  `TrackName` would need a `SongData* this`, which is not what flows in. The
  argument plumbing only fits `GetDefaultPrefab`.

### TU-layout consistency
Xenon `0x8266eaf8` (SetChar) and `0x8266ecb0` (SetLoadedPrefabChar) are adjacent,
mirroring the Wii BandUser.o ordering (SetChar 0x80162a40 → SetLoadedPrefabChar
0x80162c30, CW map lines 7524–7525). `SetLoadedPrefabChar` is the unique Wii
function with this exact GetPrefabMgr→GetDefaultPrefab→SetChar skeleton.

## Wrong-match signals: NONE
No strings on either side (so no disjoint-string conflict). Control flow,
arity, data-flow chaining, and 2/3 callee identities all agree; size ratio 1.0.
The single discrepant callee label is explained by an unmatched true callee, not
by a semantic conflict.

## For the next agent
- This pair is a clean CORRECT. It also surfaces a reusable lesson for the
  callee-resolution column: when a Xenon callee resolves to a **low-BSim**
  (sim≈0.7) tiny accessor while a structurally-expected callee is **unmatched**,
  treat the label as a probable collision and judge by argument plumbing, not the
  name. Consider gating callee-resolution display on a min BSim sim (e.g. ≥0.85)
  to avoid these misleading low-confidence labels in future packs.
- `GetDefaultPrefab__9PrefabMgrCFi` (Wii 0x803468f0) is a candidate to seed as
  Xenon `0x826fbef0` if pair-18 is accepted — but verify against `TrackName`
  first, since matches.json currently claims `0x826fbef0 == TrackName`.
