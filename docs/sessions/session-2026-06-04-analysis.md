# Session Analysis — 2026-06-04

## Environment State

Same as the three previous remote sessions (2026-05-30, 2026-06-01, 2026-06-02): **toolchain absent**.

| Resource | Status |
|---|---|
| `orig/SZBE69_B8/sys/main.dol` | MISSING — required for `dtk split` |
| `build/compilers/Wii/1.3/mwcceppc.exe` | MISSING — required for builds |
| `build/SZBE69_B8/` | MISSING — no `.o` files, no `asm/`, no `report.json` |
| Ghidra MCP (`http://ghidra.local:8001`) | Not reachable |
| `/home/free/code/milohax/m2c/` | Not present |
| `/home/free/code/milohax/dc3-decomp/` | Not present |
| `decomp.db` | Not present |
| `struct_db.sqlite` | Present (2381 classes, 7727 members, 1521 inheritance rows) |

Tiers 1–3 (permuter sweeps, from-scratch decomp, hand-edit) are all blocked.
Deliverable: analysis and updated next-steps for the next toolchain-equipped session.

---

## Project State (as of last successful toolchain run ~2026-06-02/03)

From `config/SZBE69_B8/objects.json`:

| Status | Count |
|---|---|
| Matching | 655 |
| NonMatching | 563 |
| Equivalent | 87 |
| MISSING | 571 |

The 571 MISSING files are almost exclusively out-of-scope (sdk/, network/ObjDup, network/Services, lib/binkwii, etc.). The only in-scope MISSING files are Wii-specific (BandMemcardAction_Wii, WiiFriendsList, WiiProfilePanel, etc.) and need Ghidra for implementation.

From commit `79d55e3` (2026-06-03): **Decomp: 62.8830% code / 31,949 functions matched** (updated vs CLAUDE.md's stale 59.58% / 31,163).

From session notes (2026-06-01): **~226 AT_LIMIT functions**, ~1,185 workable (80-99.9%).

Last permuter activity: Wave K (2026-05-28), 2 wins. Logs show last `batch_auto` run was `auto_20260527_043305` (0 improvements out of 25 functions — fleet saturated).

---

## Patterns Verified as Already Fixed

This session confirmed all previously-documented patterns have been applied:

### operator= return *this

All `operator=` implementations in NonMatching `.cpp` files have been verified:
- `MatAnim.cpp`, `Mesh.cpp` — have `return *this;` ✓
- `BandFaceDeform.cpp`, `BandIKEffector.cpp`, `BandPatchMesh.cpp` — have `return *this;` ✓
- `Character.cpp`, `CharClip.cpp`, `CharEyes.cpp` — have `return *this;` ✓
- `SpotlightDrawer.cpp` — has `return *this;` ✓

Header-level fall-off-the-end operator= (TransformNoScale, StlNodeAlloc, MemAllocator, TickedInfo) are intentionally gated behind `#ifdef HX_NATIVE` per commit `79d55e3`.

### Gem.cpp `return (Gem &)g;`

Confirmed: Gem.cpp line 55 returns the argument `g` rather than `*this`. Per `return-this-op-assign.md`, this was **correctly skipped** by the automated scanner — it has an explicit return (different case from the "missing return" pattern). The cast-to-argument may match the original MWCC codegen. **Do not change without assembly verification.**

### MISSING files with source implementations

Confirmed: Only `BerkeleySocketDriver.cpp` (intentional stub, `// NON-DECOMPED`) and `Synth_Wii.cpp` (80 functions remain, needs Ghidra). No new MISSING→NonMatching flips are available without Ghidra.

---

## Pool Shift Analysis

The pool shift scan (`pool-shift-scan-2026-05-26-v4.json`) shows these in-scope candidates:

| Unit | Delta | Functions | Status |
|---|---|---|---|
| `system/movie/Movie` | -143 | 19 | Wii-specific (Movie_Wii.cpp); skip |
| `system/bandobj/OutfitConfig` | +50 | 5 | Wave I: DECOMP_FORCEACTIVE attempt failed; likely AT_LIMIT |
| `system/char/CharEyes` | -49 | 9 | Wave I2: 1 win (Highlight); pool shift persists |
| `system/utl/NetCacheMgr` | +19 | 4 | Not yet swept |
| `system/beatmatch/SongParser` | -15 | 5 | Not yet swept — **highest value** |
| `band3/game/BandUserMgr` | -5 | 4 | Small delta, may be noise |

### SongParser Pool Shift — Recommended Investigation

Five functions affected by the -15 byte delta (our pool is 15 bytes shorter than target):

1. `IsPartTrackName` — uses "PART", "HARM"
2. `UpdateReadingState` — uses "SongParser::UpdateReadingState in wrong state"
3. `AnalyzeTrackList` — uses `MILO_WARN` format strings including "%s: bad track name: '%s'"
4. `ShouldReadTrack` — uses "BEAT", "SongParser::ShouldReadTrack in wrong state"
5. `HandleRGGemStop` — uses multi-format strings

**With toolchain**: Run `scripts/find_pool_shift.py --filter system/beatmatch/SongParser` to get per-function diff details. Look for a missing MILO_ASSERT condition string (around 15 chars) or a slightly different error message.

The existing `DECOMP_FORCEACTIVE(SongParser, "mMeasureMap")` at line 90 handles the primary pool anchor. The -15 gap is elsewhere in those 5 functions.

### NetCacheMgr Pool Shift

Functions affected: `NeedsToDownload`, `IsDownloading`, `Poll`, `IsSafeToDelete`.
Delta +19: our pool has 19 extra bytes. May be a duplicate string or incorrect MILO message.

---

## Deep-Dive Target Status (Updated)

From session-2026-06-01-analysis.md + wave docs, these remain the highest-value hand-decomp targets:

| Function | Unit | Notes |
|---|---|---|
| `VocalTrack::UpdateScrolling` | bandtrack/VocalTrack | ~80%, 11KB, largest in-scope gap. Deque + template ops. |
| `VocalPlayer::Poll` | band3/game/VocalPlayer | ~74%, Duff-device find_if pattern. |
| `GemPlayer::Hit` | band3/game/GemPlayer | ~88%, 4.8KB, most likely store/compound wins. |
| `Singer::PostLoad` | band3/game/Singer | ~76%, load-time setup. |
| `SaveLoadManager::Poll` | band3/game/SaveLoadManager | ~88%, big dispatcher. |

**Important caveat from Wave K** (2026-05-28): `cache_repeated_call`, `store_then_compound_add`, and `demorgan_guard` scanner gates are too loose — they fire on `asm_signal_match` but the real issue is FPR cascades. Pattern-scan alone won't find the remaining wins; use `run_diff_inspect mode=clusters` for structural analysis.

---

## CharClipDisplay Cache-Repeated-Call (Pending Verification)

From session-2026-06-01-analysis.md finding (not yet acted upon):

```cpp
// src/system/char/CharClipDisplay.cpp, SetStartEnd (lines 82-86):
int width = TheRnd->Width();                          // cached
float textOffset = unk64 + unk14 + margin;
unkc = unk1c - (((float)width * 0.5f - textOffset) * zoomRange) / (float)width;
unk10 = ((((float)width - margin) - textOffset) * zoomRange)
        / (float)TheRnd->Width() + unkc;              // REDUNDANT vtable call
```

If the target only calls `Width()` once, changing line 86's `TheRnd->Width()` → `width` would help.
**Requires assembly verification before committing.** The function has `__declspec(noinline)` and `#pragma fp_contract off` from prior analysis.

---

## Permuter Fleet State

The permuter fleet is saturated. Last run: 2026-05-27. Summary:

- Wave K (2026-05-28): 2 wins
- 226 functions AT_LIMIT (up from 216 after Wave K)
- Most recent `batch_auto` runs return 0 improvements (queue exhausted)

Wave E target list (`docs/plans/wave-e-targets.md`) has 50 ranked functions. Top actionable targets after fleet saturation:

| Rank | Symbol | Unit | Match% | Pattern |
|---|---|---|---|---|
| 2 | `HandleRGGemStart` | beatmatch/SongParser | 80.03% | member_readback + store_then |
| 4 | `SimulateZeroTime` | char/CharHair | 80.12% | switch_case_reorder (verify: may be false positive) |
| 9 | `BuildBeam` | world/Spotlight | 80.58% | bitpack_or_reorder |
| 13 | `MeasureLengths` | char/CharIKHand | 81.35% | demorgan_guard |
| 14 | `TestMesh` | bandobj/BandHeadShaper | 81.45% | positive_branch_invert |
| 16 | `Poll` | char/CharForeTwist | 81.82% | demorgan_guard |

**Note**: Wave H identified that the AST scanner has a function-granularity bug (scanning file-level instead of function-level). These hits may be false positives. Verify with `run_diff_inspect mode=diagnosis` before editing.

---

## NintendoManagementProtocolDDL.cpp — ExtractCallSpecificResults (0%)

From PR #16 session notes: `ExtractCallSpecificResults` is at 0% — target is 0x2DC (732) bytes but
current implementation is ~120 lines. The function body looks complete for the known switch cases
(0x8001: no-op, 0x8002: string list extraction). The missing 600+ bytes must be additional cases.

**With Ghidra**: Run `/analyze-function -u network/RVPackages/NintendoManagementProtocolDDL ExtractCallSpecificResults__Q26Quazal37NintendoManagementProtocolClientFPQ26Quazal7MessagePQ26Quazal23ProtocolCallContext` to see missing cases. The file already compiles; this is an implementation gap.

---

## Recommended Next Steps (with toolchain)

### Immediate (first 30 min)
1. **Pool shift SongParser**: `scripts/find_pool_shift.py --filter system/beatmatch/SongParser` → identify the missing 15-byte string → add DECOMP_FORCEACTIVE or fix message text.
2. **CharClipDisplay::SetStartEnd**: Run `bin/analyze-function -u system/char/CharClipDisplay SetStartEnd__16CharClipDisplayFff`, check whether target calls Width() once or twice, apply fix if once.

### Short-term (next session)
3. **VocalTrack::UpdateScrolling deep dive**: `run_diff_inspect mode=clusters` to map 20% gap to source regions.
4. **GemPlayer::Hit deep dive**: Same approach, identify FPR cascade vs structural issues.
5. **Permuter re-sweep after pool fixes**: Once SongParser pool is fixed, the 5 affected functions become permuter targets.
6. **NetCacheMgr pool shift**: Investigate the +19 delta (extra bytes in our pool).
7. **Wave H H1 fix** (function-granularity bug in pattern_scan): Fixes false positives in AST scanner, unlocks cleaner permuter targeting.

### Medium-term
8. **ExtractCallSpecificResults Ghidra analysis**: Missing switch cases in NintendoManagementProtocolDDL.
9. **CharEyes pool shift (-49)**: 9 functions still affected; first attempt (Wave I2) got 1 win.
10. **BandNetGameData.cpp stub**: 7 functions — needs Ghidra for constructor/destructor bodies.

---

## CLAUDE.md Progress Update

CLAUDE.md reports 59.58% code / 75.54% functions (31,163 / 41,254) but this is stale.
Commit `79d55e3` (2026-06-03) shows **62.8830% code / 31,949 functions**. Update CLAUDE.md when
next toolchain session produces a fresh `build/SZBE69_B8/report.json`.
