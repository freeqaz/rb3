# Session Analysis — 2026-06-01

## Environment State

This session ran in an isolated container **without the toolchain** (no `orig/SZBE69_B8/sys/main.dol`, no `mwcceppc`/`wibo`). As a result, no builds or objdiff verifications were possible. This doc captures analysis findings for the next toolchain-equipped session.

## Project State Summary (as of last toolchain run ~2026-05-31)

- **Functions**: ~78.46% matched (estimated from wave trends)
- **AT_LIMIT**: 226 functions (up from 216 after Wave K)
- **Workable**: ~1,185 functions in 80–99.9% band
- **Recent wins**: Singer.cpp declaration reorder (2026-05-31, 95dc9c8)
- **Last wave**: Wave K (2026-05-28): 2 wins (Color::MakeColor 85.1→86.7%, MakeHSL 93.6→93.8%)

## Key Finding: CharClipDisplay Cache-Repeated-Call Candidate

**File**: `src/system/char/CharClipDisplay.cpp`, `SetStartEnd` function (lines 82–86)

```cpp
int width = TheRnd->Width();                          // line 82: load cached width
float textOffset = unk64 + unk14 + margin;
unkc = unk1c - (((float)width * 0.5f - textOffset) * zoomRange) / (float)width;
unk10 = ((((float)width - margin) - textOffset) * zoomRange)
        / (float)TheRnd->Width() + unkc;              // line 86: REDUNDANT CALL
```

Line 86 calls `TheRnd->Width()` when `width` was already cached on line 82. This is the
`cache_repeated_call` pattern. The fix would be replacing `TheRnd->Width()` on line 86 with `width`.

**Caveat**: This function already has `__declspec(noinline)` and `#pragma fp_contract off`, suggesting
prior analysis. The second call might be intentional if the target binary also makes a second vtable
call here. **Verify with objdiff before committing**.

## Finding: BandPatchMesh::FindXfm AT_LIMIT Structural Issue

`BandPatchMesh::FindXfm` (56.5%) at line 1010 has `if (endFace == endFace)` — a self-comparison
that is always true. This means the "find closest face" second loop
(`while (facePtr != endFace)`) never executes, because `facePtr == endFace` immediately.

However, per Wave F7 notes: this function is **AT_LIMIT** due to `Vec.h`/`Mtx.h`/`psq_*` FPR
cascade, not the control flow issue. The self-comparison may match the target binary's compiled
behavior. **Do not attempt to fix without first verifying with `run_diff_inspect mode=clusters`.**

## Finding: BandPatchMesh Unit — All Sub-100% Functions AT_LIMIT

Per Wave F7 (2026-05-28): all 14 sub-100% functions in `system/bandobj/BandPatchMesh` are
permuter-class due to Vec.h/Mtx.h/psq_* FPR cascade. **Do not re-sweep** without new
manual structural analysis.

## Deep-Dive Target State (Updated)

Per deep-dive-targets-2026-05-26.md + wave docs:

| Function | Unit | % | AT_LIMIT? | Notes |
|---|---|---|---|---|
| `VocalPlayer::Poll` | band3/game/VocalPlayer | ~74% | No | Explored in sessions past; Duff-device find_if. Still has room. |
| `VocalTrack::UpdateScrolling` | band3/bandtrack/VocalTrack | ~80% | No | 11KB, largest in-scope gap. Multiple deque + template ops. |
| `GemPlayer::Hit` | band3/game/GemPlayer | ~88% | No | Core gameplay. 4.8KB. Most likely wins from store/compound patterns. |
| `Singer::PostLoad` | band3/game/Singer | ~76% | No | Load-time setup. `95dc9c8` improved related functions. |
| `SaveLoadManager::Poll` | band3/game/SaveLoadManager | ~88% | No | Big dispatcher, natural follow-up to SetState work. |

## What Wave K Left Behind

Wave K conclusion notes that `cache_repeated_call`, `store_then_compound_add`, and `demorgan_guard`
scanner gates are too loose — they fire on `asm_signal_match` but the real issue is an FPR cascade.
The scanner correctly finds these patterns in source but they don't translate to wins.

**Implication**: Pattern-scan-driven sweeps on units with FPR cascade issues will continue to produce
0 wins. The remaining wins are in the deep-dive dispatcher functions (semantic/structural analysis
with `run_diff_inspect mode=clusters`).

## Pool-Shift Scan State

From `docs/decomp/pool-shift-scan-2026-05-26-v4.json`, in-scope units with pool shift signals:

| Unit | Delta (bytes) | Affected Fns | Status |
|---|---|---|---|
| `system/char/CharEyes` | -49 | 9 | Wave I2: 1 win (Highlight), pool shift unfixed |
| `system/beatmatch/SongParser` | -15 | 5 | Not yet swept |
| `system/bandobj/OutfitConfig` | +50 | 5 | DECOMP_FORCEACTIVE attempt failed (Wave I); AT_LIMIT risk |
| `band3/game/BandUserMgr` | -5 | 4 | Small delta, may be noise |
| `system/utl/NetCacheMgr` | +19 | 4 | Not yet swept |

**Highest-value next pool-shift target**: `system/beatmatch/SongParser` (delta=-15, 5 fns,
not yet attempted). Use `scripts/find_pool_shift.py --filter system/beatmatch/SongParser`
to get per-function diff details.

## Toolchain Setup Instructions (for next session)

The toolchain downloads automatically via `tools/ninja-locked` but requires:
1. `orig/SZBE69_B8/sys/main.dol` — the target binary (not distributed with repo)
2. The environment must fetch `build/compilers/Wii/1.3/mwcceppc.exe` via download_tool.py

The download_tool.py downloads tools from GitHub releases (configured in `build.ninja`).
Once `orig/` is present, `tools/ninja-locked` will:
1. Download dtk, wibo, compilers, binutils, sjiswrap
2. Run dtk split to generate per-object asm targets
3. Build all source files and run objdiff

## Recommended Next Steps (with toolchain)

1. **Pool shift SongParser**: Run `scripts/find_pool_shift.py --filter system/beatmatch/SongParser`,
   then fix string pool ordering.
2. **CharClipDisplay cache fix**: Change `TheRnd->Width()` on line 86 to `width`; verify with objdiff.
3. **VocalTrack::UpdateScrolling deep dive**: Use `run_diff_inspect mode=clusters` to map diff
   clusters to source regions; fix one cluster at a time.
4. **Singer family**: Additional declaration reorders following the pattern in `95dc9c8`.
5. **batch_auto sweep after tooling fixes**: Wave J fixed the scanner; Wave K confirmed 158
   `asm_signal_match` hits. Run fresh `batch_auto` on units identified in those hits.
