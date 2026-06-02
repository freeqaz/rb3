# Wave-Dispatch Session Notes (2026-06-02)

Remote session run without build environment (orig binary + MWCC compiler unavailable).
Analysis-only mode — changes are config metadata, not verified source changes.

## Session Environment

- `orig/SZBE69_B8/sys/main.dol`: MISSING (required for `dtk dol split`)
- `build/compilers/Wii/1.3/mwcceppc.exe`: unavailable (files.decomp.dev returns 403)
- `build/tools/dtk`: downloaded successfully via GitHub
- `build/binutils`: downloaded successfully via GitHub

Without the DOL and MWCC, Tiers 1-3 (permuter/from-scratch/hand-decomp) are blocked.
Deliverable: config metadata fixes based on permuter log analysis.

## Key Finding: 11 MISSING Files Had Existing Implementations

Comparing `config/SZBE69_B8/objects.json` status against `src/` filesystem:
**14 files were marked MISSING but already had source implementations.** Of these,
**11 were verified to compile** by checking `logs/permuter/auto_2026052*/` JSON logs
(the permuter ran them on the developer's machine and got valid match% readings).

All 11 were flipped to `NonMatching`. See commit `config: flip 11 MISSING→NonMatching
files confirmed by permuter logs` for the full list.

Files skipped (NOT flipped):
- `network/Platform/BerkeleySocketDriver.cpp` — marked `// NON-DECOMPED`, intentional stub
- `system/synthwii/Synth_Wii.cpp` — only has BufFile methods (partial — 80 functions
  totaling 9960 bytes remain unimplemented; needs Ghidra)
- `sdk/RVL_SDK/src/nand/NANDErrorMessage.c` — 0-byte empty file

## Function Match Percentages (from permuter logs)

### Newly-NonMatching files — known match state

| File | Function | Match% |
|------|----------|--------|
| BandStorePanel.cpp | BandStoreShortcutProvider::Text | 96.8% |
| StoreMenuProvider.cpp | IsActive | 100% ✓ |
| StoreOfferContentsProvider.cpp | AcceptCurChecked | 100% ✓ (type_width_change) |
| StoreOfferProvider.cpp | FindOffer | 98.4% |
| StoreOfferProvider.cpp | Mat | 97.8% |
| TourDescPanel.cpp | Refresh | 96.0% |
| TourDescPanel.cpp | UpdateExtendedMesh | 95.1% |
| NintendoManagementProtocolDDL.cpp | CallGetConsoleUsernames | 94.3% |
| NintendoManagementProtocolDDL.cpp | ExtractCallSpecificResults | 0.0% ⚠ |
| PitchDetector.cpp | dump | 97.2% |
| SndAnalysis.cpp | FindCCPeak | 94.7% |
| Dxt1Compress.cpp | fancybasecolorsearch | 98.97% |
| Dxt1Compress.cpp | storedxtencodedblock | 91.74% |

The `ExtractCallSpecificResults` 0% in NintendoManagementProtocolDDL is a known issue —
the target function is 0x2DC (732) bytes but our implementation is much shorter. Needs
Ghidra to compare. The file compiles fine; objdiff will reveal the gap.

### TourDescPanel — stl template cascade

Per session notes (2026-05-23): TourDescPanel was written from scratch and the
`_Temporary_buffer<Symbol*>` specialization caused the STL sort template family to
cascade to 100% (11 functions: `__stable_sort_aux`, `stable_sort`,
`__stable_sort_adaptive`, `__merge_adaptive`, etc.). But the panel's own functions
(Refresh at 96%, UpdateExtendedMesh at 95.1%) remain nonmatching.

## SZBE69_B8 MISSING/NonMatching Status Inventory

After flips:
- **NonMatching**: 563 (+11 vs snapshot)
- **Matching**: 655 (unchanged)
- **Equivalent**: 87 (unchanged)
- **MISSING**: 571 (-11 vs snapshot)

Remaining MISSING files that still need Ghidra for implementation:
- `sdk/RVL_SDK/src/gx/GXTexture.c` — marked NonMatching in objects.json but no src/ file
- `sdk/RVL_SDK/src/os/OSLaunch.c` — same situation
- `sdk/RVL_SDK/src/os/OSPlayTime.c` — same situation
- `system/synthwii/Synth_Wii.cpp` — 80 functions, needs Ghidra decompilation
- `system/movie/Movie_Wii.cpp` — Wii BinkFile/GX, needs Ghidra
- `band3/meta_band/BandNetGameData.cpp` — network game data, needs Ghidra

## At-Limit Functions (from permuter plateau analysis)

These functions are at 99.9%+ but stuck — permuter has saturated them:

| Function | % |
|----------|---|
| TambourineManager::~TambourineManager | 99.98% |
| BandCharacter::ReplaceSubdir | 99.98% |
| CharCache::InitMe | 99.98% |
| BandWardrobe::~BandWardrobe | 99.97% |
| GamePanel::~GamePanel | 99.97% |
| VocalGuidePitch::Load | 99.97% |

These likely have a single mismatched instruction pair. Next steps per
`docs/decomp/patterns/at-limit-mwcc.md`: run `/compare-asm` and check for:
- `fcmpu` operand order flip
- `bool` materialization (`neg/or/srwi 31` vs direct compare)
- ICF (linker-merged identical function bodies)

## High-Value Unpermuted Files (never been in permuter fleet)

278 NonMatching files have never been run through the permuter. Top candidates for
structural improvements once the build environment is available:

| File | Lines | Priority | Reason |
|------|-------|----------|--------|
| band3/meta_band/Campaign.cpp | 993 | HIGH | Large, structural |
| band3/meta_band/MetaPerformer.cpp | 1774 | HIGH | Largest unpermuted file |
| band3/meta_band/NextSongPanel.cpp | 887 | HIGH | Song selection UI |
| band3/meta_band/Matchmaker.cpp | 428 | MED | Network matchmaking |
| band3/game/FocusTracker.cpp | 538 | MED | Game logic |
| band3/game/TrackerManager.cpp | 411 | MED | Game logic |
| band3/game/Performer.cpp | 425 | MED | Game logic |
| band3/game/Scoring.cpp | 366 | MED | Scoring system |
| band3/game/TrackerDisplay.cpp | 299 | MED | UI display |

## Dxt1Compress.cpp Partial Progress

The `storedxtencodedblock` function went from 91.47% to 91.74% via `comma_split`
pattern in session `auto_20260527_014547`. The improvement was found but may not
have been committed. Look for compound comma expressions in the function body
to split into separate statements.

`fancybasecolorsearch` is stuck at 98.97% (plateau). The last tried pattern was
`comparison_flip` — check for a `<` vs `<=` comparison difference in the
multi-pass color search loops.
