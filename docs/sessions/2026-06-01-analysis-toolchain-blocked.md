# Session 2026-06-01 — Analysis (build blocked: orig/ missing)

**Branch**: `remote-decomp/20260601_120243`  
**Outcome**: No match% improvements — `orig/SZBE69_B8/sys/main.dol` absent, blocking all builds.

---

## Environment Status

| Component | Status |
|-----------|--------|
| `orig/SZBE69_B8/sys/main.dol` | **MISSING** — entire `orig/` directory absent |
| Compiler (`mwcceppc` via wibo) | Downloadable but blocked by missing DOL |
| Ghidra MCP (`ghidra.local:8001`) | DNS unresolvable in this environment |
| DC3 reference (`/home/free/code/milohax/dc3-decomp/`) | Not mounted |
| `build/SZBE69_B8/report.json` | Not built — all analysis tools blocked |
| `decomp_synth` module | Not installed (not in venv) |

**Blockers in priority order:**
1. Copy `orig/SZBE69_B8/` into the container before starting (the DOL is gitignored by design)
2. Ensure `ghidra.local:8001` is resolvable (needed for Tier 1 analysis and Tier 2 from-scratch)
3. `dc3-decomp/` mounted at `/home/free/code/milohax/dc3-decomp/` for engine reference

---

## Static Analysis Findings (no build required)

### Objects.json Status (config/SZBE69_B8/objects.json)

| Category | Matching | NonMatching | Equivalent | MISSING | Total |
|---|---:|---:|---:|---:|---:|
| `system/` | 292 | 267 | 67 | 19 | 645 |
| `band3/` | 158 | 117 | 15 | 14 | 304 |
| `sdk/` | 114 | 79 | 2 | 287 | 482 |
| `network/` | 87 | 86 | 3 | 251 | 427 |
| `lib/` | 1 | 0 | 0 | 11 | 12 |

**Priority areas breakdown (core engine + game):**

| Directory | Matching | NonMatching | MISSING |
|---|---:|---:|---:|
| band3/bandtrack | 5/13 | 8 | 0 |
| band3/game | 32/74 | 42 | 0 |
| band3/meta_band | 110/179 | 56 | 13 |
| system/bandobj | 23/60 | 37 | 0 |
| system/beatmatch | 31/46 | 15 | 0 |
| system/char | 26/61 | 35 | 0 |
| system/math | 15/20 | 5 | 0 |
| system/meta | 24/30 | 6 | 0 |
| system/obj | 10/17 | 7 | 0 |
| system/rndobj | 31/64 | 32 | 1 |
| system/utl | 50/75 | 25 | 0 |
| system/world | 7/16 | 9 | 0 |

---

### Pattern B: `TheDebug << "literal"` without MakeString (Pattern B2)

These NonMatching files have bare string literals in `TheDebug <<` chains. Adding `MakeString(...)` wraps shifts the stack frame by ~0x82C (see `makestring-wrap-literal.md`).

| File | Approx. count |
|------|--------------|
| `src/system/rndobj/MeshDeform.cpp` | 12 |
| `src/band3/meta_band/SaveLoadManager.cpp` | 5 |
| `src/system/rndobj/Line.cpp` | 5 |
| `src/band3/game/VocalPart.cpp` | 4 |
| `src/system/utl/Cache_Wii.cpp` | 4 |
| `src/system/utl/Locale.cpp` | 2 |
| `src/system/utl/Rso_Utl.cpp` | 2 |
| `src/system/utl/CacheMgr_Wii.cpp` | 2 |
| `src/system/utl/Loader.cpp` | 1 |
| `src/system/utl/MemMgr.cpp` | 1 |
| `src/system/utl/NetCacheMgr.cpp` | 1 |
| `src/system/utl/HttpWii.cpp` | 1 |

**Asm tell**: `stwu r1, -0xXXX` vs `stwu r1, -0x(XXX+0x830)` — stack difference of ~0x82C in prologue signals where to apply this.

**Action**: Run `python -m decomp_synth.pattern_scan --patterns makestring_wrap_literal` after build is available.

---

### Pattern C: Missing `#pragma pool_data off` before BEGIN_HANDLERS/BEGIN_PROPSYNCS

The following NonMatching rndobj files have `BEGIN_HANDLERS`/`BEGIN_PROPSYNCS` without the pragma. Matching/Equivalent rndobj files that use the same macros do have them (e.g., `Tex.cpp:577`, `Mat.cpp:372`).

High-confidence candidates for `#pragma pool_data off/reset` wrapping around the handler block:

| File | Macro location | Current status |
|------|---------------|----------------|
| `system/rndobj/Line.cpp` | line 782 (HANDLERS), 812 (PROPSYNCS) | NonMatching |
| `system/rndobj/Mesh.cpp` | line 1241 (HANDLERS) | NonMatching |
| `system/rndobj/MeshAnim.cpp` | line 264 (HANDLERS), 273 (PROPSYNCS) | NonMatching |
| `system/rndobj/MeshDeform.cpp` | line 238 (PROPSYNCS), 396 (HANDLERS) | NonMatching |
| `system/rndobj/Morph.cpp` | line 176 (HANDLERS), 224 (PROPSYNCS) | NonMatching |
| `system/rndobj/MultiMesh.cpp` | line 250 (HANDLERS), 396 (PROPSYNCS) | NonMatching |
| `system/rndobj/PropAnim.cpp` | line 794 (HANDLERS), 854 (PROPSYNCS) | NonMatching |
| `system/rndobj/Trans.cpp` | line 430 (HANDLERS), 640 (PROPSYNCS) | NonMatching |
| `system/rndobj/Env.cpp` | line 246 (HANDLERS), 373 (PROPSYNCS) | NonMatching |
| `system/rndobj/EnvAnim.cpp` | line 113 (HANDLERS), 119 (PROPSYNCS) | NonMatching |
| `system/rndobj/LitAnim.cpp` | line 100 (HANDLERS), 119 (PROPSYNCS) | NonMatching |
| `system/rndobj/Lit.cpp` | line 176 (HANDLERS), 183 (PROPSYNCS) | NonMatching |
| `system/rndobj/Console.cpp` | line 414 (HANDLERS) | NonMatching |
| `system/rndobj/ScreenMask.cpp` | line 114 (HANDLERS), 120 (PROPSYNCS) | NonMatching |
| `system/rndobj/PartAnim.cpp` | line 186 (HANDLERS), 193 (PROPSYNCS) | NonMatching |

**Caution**: Only add the pragma where asm shows mismatched BSS-base pooling (see `pragma-pool-data-wrap.md`). These are candidates that need objdiff verification before committing.

---

### Pattern: Operator= without `return *this` (Pattern B1)

Checked all `operator=` in NonMatching system/band3 files — all properly include `return *this;` or fall off the end intentionally (MWCC match).

**One edge case**: `src/system/rndobj/Mesh.cpp:406` (`RndMesh::VertVector::operator=`) has `return *this;` inside `#ifdef HX_NATIVE` only. The non-native build path falls off — this is the INTENTIONAL match (MWCC also falls off). Confirmed correct.

**network/Platform** `Time.cpp` and `DateTime.cpp` have trivially-short `operator=` without returns. These are in a deprioritized area (Wii-specific transport) and the trivial body makes the register-allocation issue moot.

---

## Tier 1: Permuter Sweep Targets (for next session)

When `report.json` is available, run these sweeps in order:

```bash
# Highest density of workable functions
python3 -m scripts.permuter.batch_auto --unit 'system/rndobj/*' --min-pct 85 --max-pct 99.99 --workers 4 --limit 90 --json
python3 -m scripts.permuter.batch_auto --unit 'system/char/*'   --min-pct 85 --max-pct 99.99 --workers 4 --limit 90 --json
python3 -m scripts.permuter.batch_auto --unit 'system/bandobj/*' --min-pct 85 --max-pct 99.99 --workers 4 --limit 90 --json
python3 -m scripts.permuter.batch_auto --unit 'band3/game/*'    --min-pct 85 --max-pct 99.99 --workers 4 --limit 90 --json
python3 -m scripts.permuter.batch_auto --unit 'band3/meta_band/*' --min-pct 85 --max-pct 99.99 --workers 4 --limit 90 --json
```

The permuter fleet last ran on 2026-05-27 — may have 0 wins on some units (already saturated). Rotate through units per the CLAUDE.md instructions.

Last batch check (2026-05-27, auto_20260527_043305): 27 results, 0 with delta (already AT_LIMIT or saturated).

---

## Tier 2: SDK/Network From-Scratch (for next session)

### SDK (GX) — NonMatching, HIGH-YIELD

These GX files are NonMatching (have partial implementations). Permuter + targeted hand edits after Ghidra analysis:

| File | Status |
|------|--------|
| `sdk/RVL_SDK/src/gx/GXAttr.c` | NonMatching |
| `sdk/RVL_SDK/src/gx/GXBump.c` | NonMatching |
| `sdk/RVL_SDK/src/gx/GXFifo.c` | NonMatching |
| `sdk/RVL_SDK/src/gx/GXFrameBuf.c` | NonMatching |
| `sdk/RVL_SDK/src/gx/GXMisc.c` | NonMatching |
| `sdk/RVL_SDK/src/gx/GXPixel.c` | NonMatching |
| `sdk/RVL_SDK/src/gx/GXTev.c` | NonMatching |
| `sdk/RVL_SDK/src/gx/GXTexture.c` | NonMatching |

SKIP: `GXInit.c` (MISSING, big state machine), `GXPerf.c` (MISSING).

### Quazal DDL MISSING Files with Headers

These MISSING files have headers but NO cpp yet. The `_DO_*` pattern requires understanding what DataSets each registers — need Ghidra for the constructor.

| File | Header available | Pattern |
|------|-----------------|---------|
| `network/ObjDup/RootDODDL.cpp` | `RootDODDL.h` | `_DO_RootDO : DuplicatedObject` |
| `network/ObjDup/SessionDDL.cpp` | `SessionDDL.h` | `_DO_Session : RootDO` |
| `network/ObjDup/StationDDL.cpp` | `StationDDL.h` | `_DO_Station : RootDO` |
| `network/Extensions/SessionClockDDL.cpp` | `SessionClockDDL.h` | `_DO_SessionClock : RootDO` |

The `_DO_*` virtual method bodies (especially `CallOperationOnDatasets`) need binary analysis to determine which DataSet classes they register. Use Ghidra on the RB3 debug ELF before attempting.

### Large MISSING DDL files (no headers available)

287 MISSING SDK files and 250+ MISSING network files have no headers in-tree. These need either:
- Ghidra analysis to discover the class layout
- Reference from public Quazal/RVL_SDK documentation

---

## Recent AT-LIMIT functions (audit 2026-05-26)

These are documented as AT-LIMIT — don't re-attempt:

| Symbol | % | Reason |
|--------|---|--------|
| `__ct__14BoxMapLightingFv` | 98.33 | `psq_l`/`ps_*` SIMD cascade |
| `NextName__FPCcP9ObjectDir` | 97.97 | `_savegpr_N` span mismatch |
| `Enter__14BandStorePanelFv` | 99.98 | Virtual-inheritance vbase resolution |
| `HandleEventResponse__15SaveLoadManagerFP9LocalUseri` | 99.78 | Switch-range MWCC heuristic |
| `ContentDone__11BandSongMgrFv` | 99.78 | `std::pair<float,float>` slot swap |
| `AddUpgradeData__14SongUpgradeMgrFPC28BandSongUpgradeSystemMessage` | 99.57 | r4/r5 regswap cluster |

FIXED in the 2026-05-26 audit: `BuildSetlistTree__11SetlistSortF...` → 100% (bogus `(SongSortNode *)` cast removed, commit-landing confirmed).

---

## What to do in the next session

1. **FIRST**: Ensure `orig/SZBE69_B8/` is present before starting. Run `sha1sum orig/SZBE69_B8/sys/main.dol` to confirm.
2. Run `tools/ninja-locked build/SZBE69_B8/report.json` to regenerate progress report.
3. Start Tier 1 sweeps immediately — load is low, no fleet running.
4. Use the pool_data candidates in Pattern C above as quick experiments after permuter saturates.
