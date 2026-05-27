# Stub Campaign — 2026-05-26

After ~10 sweep waves moved the project from **75.54% → 78.46% functions / 59.58% → 62.22% code**, the source-fixable mid/large partial surface (≥1200B, 40-90%, not in AT_LIMIT) is exhausted. Remaining productive paths:

1. **Permuter runs** (autonomous, already churning in `logs/permuter/auto_*`) — grinds 95-99.9% scheduling/regswap last-mile
2. **From-scratch stub fills** on in-scope `<50%` functions — the target of this campaign

## Wave-Based Sweep Process (refined over 10 waves)

The same process used for partial-fix waves applies to stub fills, with adjustments noted below.

### Dispatch shape

- **Parallel Sonnet agents** (4-8 per wave), each on a **distinct TU** to avoid concurrent header/source collisions
- **Worktrees** (`isolation: "worktree"` Agent param) for *invasive* changes (headers, struct layout, shared STL specializations)
- **Direct-on-master** for surgical .cpp edits inside a single TU
- **Hard time-boxes**: ≤45 min per agent, ≤2 attempts per candidate, **commit-on-win** discipline. Prior sweep agent ran 1.7h without reporting and hit weekly quota mid-write — never again.
- **Build with `tools/ninja-locked`**, NEVER bare `ninja` (concurrent corruption risk)

### Tooling commands the agent must use

| Tool | When |
|---|---|
| `bin/analyze-function SYMBOL` | First step — ghidra + m2c + objdiff in one shot |
| `bin/analyze-function -u UNIT SYMBOL` | When symbol exists in multiple TUs |
| `bin/decompile -u UNIT SYMBOL -c SRC_FILE` | m2c with C++ context types |
| `build/tools/objdiff-cli diff -u UNIT SYMBOL --format json-pretty -o /dev/stdout` | Final verification |
| `python3 scripts/dc3_compare.py --filter UNIT/` | Find DC3 100%-match sister fn for engine code |
| `bin/find-inlining-gaps` | Detect qualified-call inline opportunities (3 modes) |
| `bin/sweep-candidates` | Discover next batch of candidates |
| `bin/lint-mangled-paste` | Catch m2c mangled-name leakage before commit |
| `bin/lint-link-issues` | Catch missing/duplicate symbols pre-link |
| `bin/orchestrate` | Wraps `mcp__orchestrator__*` tools |
| `scripts/find_pool_shift.py` | Find MILO_ASSERT pool-shift fix candidates |
| `scripts/orchestrator/classify_equivalent.py` | LINKED audit — promote NonMatching@100% → Matching |
| `tools/mwcc_symbols.py` | MetroWorks mangling table lookup |

### Slash-command skills

`/analyze-function`, `/ghidra-decompile`, `/compare-asm`, `/stack-layout`, `/progress`, `/dc3-pair`, `/refactor-staff`, `/pcode-inspect`, `/struct-check`, `/permute` — see `CLAUDE.md` for descriptions.

### Stub-fill methodology (per `feedback_setupgems_pattern`)

For a 0% (or low) from-scratch fill:

1. `bin/analyze-function` → ghidra pseudo-C + m2c output + ground-truth asm
2. Pull all MILO_ASSERT / MILO_WARN / MILO_LOG strings from the asm (`@stringBase0` references) — they pin function shape and assertion line numbers
3. Check `scripts/dc3_compare.py --filter UNIT/` for a DC3 sister implementation (logic skeleton only — DC3 was MSVC, NOT asm-matchable)
4. **First pass target**: 60-90% from a clean source draft (`SetupGems` got 87.5% from this alone)
5. **Iterate**: fn-order, bitfield names, declaration order, materialization patterns, pool placement → typical 90-95%
6. Beyond 95% is permuter-class — hand off to background runs

### A/B protocol for header edits

1. Snapshot `report.json` before
2. Apply header change
3. Full rebuild with `tools/ninja-locked`
4. Per-function diff via `objdiff-cli` for every TU that includes the header
5. **Revert immediately if ANY 95%+ function regresses** (one 100%→79% costs more than +5pp on three 70% functions)

### What NOT to touch (AT_LIMIT skip list)

Functions in these TUs cannot improve via source edits (inline-asm `psq_` FPR cascades, prologue-level locks, etc.):

- `src/system/rndobj/Part.cpp` — Multiply asm
- `src/system/char/CharHair.cpp` — StrandMultiply asm
- `src/system/char/CharForeTwist.cpp`, `CharIKHead.cpp`, `CharUpperTwist.cpp` — psq_ asm
- `src/system/char/CharCuff.cpp` — psq_ asm
- `src/system/world/BandIKEffector.cpp` — 160B stack delta from inline Normalize
- `src/system/math/Geo.cpp` — Mtx.h Normalize-block cascade
- `src/system/os/Timer.h` — header-lock blocks every meta Handle/Poll fn at 99.x%
- `src/system/world/BandFaceDeform.cpp` — face deform asm
- `src/system/synth/IIRFilter.cpp`, `DrumMixDB.cpp` — synth asm
- `src/system/synth/Utl.cpp` (CacheWav variants), `src/system/midi/DisplayEvents.cpp`, `src/system/rndobj/PropKeys.cpp` — header-locked

See `feedback-inline-asm-fpr-cascade` memory for grep recipe.

### Out-of-scope categories (skip wholesale — replaced by native port)

- `src/sdk/*` (RVL_SDK, MSL, DWC, NW4R)
- `src/system/rndwii/*` (GX renderer → OpenGL/Vulkan/Metal)
- `src/system/os/*` (Wii OS calls)
- `src/network/Platform/*`, `WiiIpStack`, `BerkeleySocketDriver`, DWC matchmaking
- `src/lib/{zlib,vorbis,speex,binkwii}` — use upstream
- Any `*_Wii.cpp` (`Movie_Wii`, `CustomSplash_Wii`, `Synth_Wii`, `FXWii`, `Mic_Wii`, `MemcardMgr_Wii`, `BandMemcardAction_Wii`, `OvershellProfileProvider_Wii`, `HttpWii`, `UsbWii`)
- Wii-specific gameplay glue: `JoinInvitePanel`, `WiiFriendsList`, `WiiFriendsProvider`, `WiiProfilePanel`, `WiiInvitationsProvider`
- `RockCentral`, `BandNetGameData` — RB3-specific online networking; *conditionally valuable* only if port ships online multiplayer (Nintendo WFC shut down 2014). Defer.

## Stub-Campaign Target Inventory (post-wave-10)

After filtering out-of-scope categories from the 692 `<50%` in-scope functions, the genuinely tractable stub-campaign surface is **small**:

| Function | Unit | Size | Match | Notes |
|---|---|---|---|---|
| `ShiftedDotProduct` | `system/dsp/SndAnalysis` | 988B | 48.1% | Pure DSP math, no platform deps. **Top target.** |
| `Synth::DrawMeter` | `system/synth/Synth` | 900B | 51.7% | Synth utility, no Wii deps |
| `RndLine::_Vector_impl<Point>::operator=` | `system/rndobj/Line` | 2760B | 32.0% | STL template cluster |
| `RndLine::_Vector_impl<Point>::_M_fill_insert_aux` | `system/rndobj/Line` | 2224B | 47.5% | STL template cluster |
| `operator>><RndLine::Point>(BinStream&, ...)` | `system/rndobj/Line` | 1664B | 44.3% | STL BinStream |
| `RndLine::SetNumPoints` | `system/rndobj/Line` | 2720B | 65.9% | Already worked once (9babd6dd) |
| `PresenceMgr::_Vector_impl<Symbol>::_M_fill_insert_aux` | `band3/game/PresenceMgr` | 700B | 0% | STL Symbol vector |
| `OutfitConfig::_Copy_Construct<OldColorOption>` | `system/bandobj/OutfitConfig` | 516B | 0% | STL template |
| `OutfitConfig::__uninitialized_move<OldColorOption*>` | `system/bandobj/OutfitConfig` | 552B | 0% | STL template |
| `InterpVertData<Hmx::Color32, GetVertColor>` | `system/rndobj/MeshAnim` | 1576B | 83.3% | Template family (3 instantiations) |
| `InterpVertData<Vector3, GetVert{Normal,Point}>` | `system/rndobj/MeshAnim` | 1020B×2 | 89.0% | Template family |
| `VoiceBeat::Analyze` | `system/synth/VoiceBeat` | 2168B | 89.7% | Synth analyzer |
| `RndMeshDeform::Reskin` | `system/rndobj/MeshDeform` | 2768B | 87.9% | Mesh deform (verify not in AT_LIMIT TU) |

Larger from-scratch fills (>4KB, harder, lower priority): the user explicitly stopped `VocalTrack::UpdateScrolling` on the previous attempt, so size bigger than ~3KB is the upper bound for stub-campaign dispatch.

## Session Wins (Waves 1-10)

Tracked in commit log. Highlights:

- **Big stub fills**: `GemPlayer::Hit` (3.8→80.2%), `VocalPlayer::Poll` (1.6→72.7%), `SaveLoadManager::SetState` (5.5→93.0%), `SongParser::HandleRGGemStop` (5.5→93%), `NoteTube::DrawToPlate` (59→93.3%), `MemMgr` stats trio (0→100% ×3)
- **Pool-shift sweeps**: `SaveLoadManager` (+17), `AccomplishmentManager`, `MusicLibrary`, `QuestFilterPanel`, `MemMgr`
- **STL specialization sweep** (the macro pattern): TourDescPanel (11 fns to 100%), AccomplishmentPanel (4 fns to 100%), UIResource, GameGemList
- **LINKED audits**: ~146 unit promotions across 3 phases
- **Real source bugs found via DC3 cross-ref**: `CharLookAt`, `GemTrackDir`, `CharCuff` (`mShape[2]`→`mShape[1]`), `InlineHelp`

## Process Refinements (lessons from waves)

- Time-box hard. The 1.7h-no-report agent that hit weekly quota was the worst incident — agents must commit on win and stop on ≥2 failed attempts.
- Read `MEMORY.md` skip lists before dispatch (waste prevention).
- Pattern docs in `docs/decomp/patterns/` are read by EVERY agent — keep them tight (rule + one-line why + one example), no narrative.
- `Co-Authored-By` lines are forbidden in commits (project convention).
- `git stash` in main repo is forbidden — use worktrees.
- AT_LIMIT findings go to `mcp__orchestrator__report_result`, NOT to memory (don't pollute the index).
