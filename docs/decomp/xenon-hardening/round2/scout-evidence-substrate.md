# Scout S2 — Verification evidence substrate (round 2)

**Goal:** build self-contained evidence packs so 30 judge agents can verify the
band3 ACCEPT identities WITHOUT each paying for Ghidra/pyghidra setup. Done: a
deterministic 30-pair stratified sample, dual-side (Wii + Xenon) extraction in a
single JVM session, and one `evidence/pair-<NN>.md` per pair.

## TL;DR for a judge

- 30 evidence packs at `docs/decomp/xenon-hardening/round2/evidence/pair-01.md` …
  `pair-30.md`. Each is **fully self-contained**: header (claimed identity,
  match_type, BSim sim×conf, TU, demangled Wii symbol, both addresses, body
  sizes), Xenon callee table (resolved through `matches.json` to matched Wii
  symbols), Xenon referenced strings, Xenon pseudo-C, Wii m2c decompilation
  (where available), and the Wii Bank-8 ground-truth asm (truncated to ~150 lines).
- The pack tells you exactly what to judge: **are these two functions the same
  source function compiled by two toolchains** (Wii MWCC/Gekko vs Xbox360
  MSVC/Xenon)? The strongest signal is the **resolved-callee column** — when the
  Xenon callees map (via our own matches) to the named Wii callees the Wii body
  also calls, the identity is corroborated by the call graph, independent of the
  BSim score that nominated it.

## The sample (manifest: `forensics/sample_manifest.json`)

Drawn from the **309 band3 ACCEPT non-seed** entries in
`build/SZBE69_B8/ghidra/ghidriff-xenon/vetted_identities.json`
(`category==band3 && tier==ACCEPT && 'SeedMatch' not in match_types`). That
population splits exactly as the brief states: 280 BSIM + 25
ExactInstructionsFunctionHasher + 3 SwitchSigHasher + 1 Implied Match.

Stratified, **RNG seeded with 42** (reproducible; `Random(42)`, pools sorted by
`xenon_addr` before shuffle so the draw is stable):

| stratum | n | source pool size |
|---|---|---|
| BSim sim×conf ≥ 30 | 6 | 37 |
| BSim sim×conf 20–30 | 8 | 118 |
| BSim sim×conf 15–20 | 7 | 125 |
| ExactInstructionsFunctionHasher | 5 | 25 |
| SwitchSigHasher | 3 | 3 (all) |
| Implied Match | 1 | 1 (all) |
| **total** | **30** | |

`simconf` = BSim `similarity × confidence`, joined from `matches.json`
`function_matches[].scores.BSIM` by **bare-hex address** (vetted file uses `0x`
prefix, matches.json does not — strip before joining). All 280 band3 BSIM ACCEPT
entries carry a score and all are ≥15 (the vet tool's `--min-bsim-simconf 15`
gate), so there is no <15 stratum.

The 30 sampled pairs (pair_id → Wii symbol, stratum, simconf):

```
01 RebuildSharedSongData__12MusicLibraryFv          BSIM>=30   35.6
02 SaveForEndGame__5StatsCFR9BinStream              BSIM>=30   41.1
03 UpdateState__13OvershellSlotFv                   BSIM>=30   47.2
04 Poll__7NetSyncFv                                 BSIM>=30   40.2
05 Poll__15GemTrainerPanelFv                        BSIM>=30   31.5
06 Poll__7TrackerFf                                 BSIM>=30   40.3
07 ProcessStaticLyrics__10VocalTrackF...            BSIM 20-30 21.8
08 SendSongsToMetaPerformer__17SetlistMergePanel... BSIM 20-30 23.7
09 SetGameOver__4GameFb                             BSIM 20-30 28.5
10 UpdateVocalStyle__10VocalTrackFv                 BSIM 20-30 22.3
11 ResolveSlotStates__14OvershellPanelFv            BSIM 20-30 24.0
12 UpdateGameCymbalLanes__9GemPlayerFv              BSIM 20-30 26.9
13 clear__...List_base<pair<Symbol,Symbol>>Fv       BSIM 20-30 20.8
14 Hit__4TailFv                                     BSIM 20-30 23.7
15 Unload__21CampaignSongInfoPanelFv                BSIM 15-20 17.7
16 __ct<PCc,i>__...pair<C6Symbol,8DataNode>...      BSIM 15-20 16.7
17 Load__...MainHubAdvanceMsgFR9BinStream           BSIM 15-20 16.7
18 SetLoadedPrefabChar__8BandUserFi                 BSIM 15-20 16.2
19 SaveGlobalOptions__10ProfileMgrF...              BSIM 15-20 17.5
20 __ct__14PlayerBehaviorFv                         BSIM 15-20 15.8
21 CreateController__14ChordbookPanelFv             BSIM 15-20 17.4
22 SetPrimaryMetaScore__16LocalBandMachineFi        ExactInstr  -
23 PitchNote__5LyricCFv                             ExactInstr  -
24 GlobalOptionsNeedsSave__10ProfileMgrFv           ExactInstr  -
25 GetFrameMatchType__6SingerFv                     ExactInstr  -
26 TambourineGems__17TambourineManagerCFv           ExactInstr  -
27 DifficultySortPart__12MusicLibraryCFv            SwitchSig   -
28 TrackTypeToScoreType__F9TrackTypebb              SwitchSig   -
29 ActiveScoreType__12MusicLibraryCFv               SwitchSig   -
30 GetRankData__11SingerStatsCFi                    Implied     -
```

## How the evidence was extracted (reproducible)

All scripts live under `docs/decomp/xenon-hardening/round2/forensics/`.

### 1. Sampling — `forensics/sample_manifest.json`
Inline python (re-runnable): join simconf from matches.json (bare-hex), classify
into strata, `Random(42)` draw, write manifest. The manifest carries the BSim
`{similarity,confidence}` for each BSIM pair.

### 2. Wii side
- **`forensics/extract_wii_asm.py`** — for each `wii_symbol`, `rg -F '.fn <sym>,'`
  finds the dtk-split `.s`, extracts `.fn`…`.endfn`. **26/30 found this way.**
  Output: `forensics/wii_asm/<pid>.s` + `wii_asm/wii_asm_index.json`.
- **4 symbols are not in the dtk split** (08, 13, 16, 17 — template/anon-namespace
  instances whose TU isn't fully split). They ARE real Bank-8 functions (confirmed
  in `orig/SZBE69_B8/files/band_r_wii.map`). Extracted by **address** from the
  symbolized Bank-8 ELF (`build/SZBE69_B8/ghidra/bank8_target.elf`, the DOL→Gekko
  transcode):
  ```
  llvm-objdump -d build/SZBE69_B8/ghidra/bank8_target.elf \
    --start-address=<wii_addr> --stop-address=<wii_addr+size>
  ```
  llvm-objdump resolves callee symbol names inline, so these packs are still
  self-contained. The wii_addr+size came from the CW map.
- **`forensics/wii_m2c/`** — m2c (`../m2c/m2c.py --target ppc <s> --function <sym>`)
  for all 26 dtk-split functions. Each isolates the exact target function
  (verified). The 4 objdump-only pairs have no m2c (raw asm carries them).

### 3. Xenon side — `forensics/extract_xenon_evidence.py` (THE batched JVM session)
ONE pyghidra session under the **FORK** Ghidra (12.2,
`GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra`, JDK 21,
`GHIDRA_USER_HOME=/tmp/claude/ghidra_user_round2`). It **imports the analyzed
`rb3_xenon_default_xex.gzf`** (packed DB, full analysis baked in — `analyze=False`,
zero re-analysis) into a **throwaway project under `/tmp/claude/xenon_evidence_proj`**
(the ghidriff `proj/` dir is NEVER touched). For each of the 30 `xenon_addr`s:
- decompiles to pseudo-C (`DecompInterface`)
- records body byte size + Ghidra's auto-name
- enumerates callees (`getCalledFunctions`), resolving each callee's xenon address
  through `matches.json` to the matched Wii symbol
- scans body instructions for data refs to defined string data
Output: `forensics/xenon_evidence.json`. Wall time: ~3 min including gzf import.

Exact invocation (copy-paste reproducible):
```bash
cd /home/free/code/milohax/rb3
rm -rf /tmp/claude/xenon_evidence_proj
env GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra \
    GHIDRA_USER_HOME=/tmp/claude/ghidra_user_round2 \
    JAVA_HOME=/usr/lib/jvm/java-21-openjdk \
    build/SZBE69_B8/ghidra/ghidriff-venv/bin/python \
    docs/decomp/xenon-hardening/round2/forensics/extract_xenon_evidence.py
```

### 4. Pack assembly — `forensics/build_evidence_packs.py`
Merges the manifest + wii_asm + wii_m2c + xenon_evidence into `evidence/pair-<NN>.md`.
Also writes `forensics/evidence_summary.json` (per-pair callee/string counts).
CW symbol demangling via `forensics/demangle_cw.py` (best-effort: class::method,
const qualifier, ctor/dtor, free functions; falls back to the raw symbol on
template gnarl).

## What the evidence shows (corroboration stats)

Across the 30 packs: **122 of 166 Xenon callees (73%) resolve to a matched Wii
symbol.** 14 non-leaf packs have **ALL** callees resolved; 6 are leaf functions
(0 callees — judged on body shape alone); 7 packs carry referenced strings.

Standout corroborations (the call graph independently confirms the BSim/exact pick):
- **pair-29 ActiveScoreType (SwitchSig):** Xenon side builds 10 lazy-init `Symbol`s
  and `switch(param+2)` over 10 cases; strings `'guitar' 'vocals' 'real_guitar'
  'real_bass' 'real_keys' 'pending'` — exactly the score-type symbols. m2c shows
  the identical 10-way switch + `TrackTypeToScoreType`/`GetPreferredScoreType` calls.
- **pair-08 SendSongsToMetaPerformer (BSIM 23.7):** both sides call
  `MetaPerformer::Current/SetBattle/SetSetlist/SetSongs` + a vector-equality loop +
  `__dynamic_cast`. No m2c (objdump-only) but fully self-contained from the asm.
- **pair-01 RebuildSharedSongData (BSIM 35.6):** Xenon resolves
  `GetSort__11SongSortMgr`, `PushSonglistToScreen__12MusicLibrary`,
  `UpdateSharedStatus__10SongRecord`, `GetNode__8NodeSort` — the same callees the
  Wii body uses.
- **pair-30 GetRankData (Implied, 16 B):** both compute `*this + arg*8` (Xenon adds
  a `& 0x1fffffff` 64-bit-register-narrowing mask; structurally identical).
- **pair-22 SetPrimaryMetaScore (ExactInstr):** both store arg at a fixed offset
  then call `SyncLocalMachine__14BandMachineMgr`. (Field offset differs 0x6c vs the
  m2c-inferred 0x78 — expected; different toolchain struct layout.)

## Caveats a judge should keep in mind

1. **Field offsets differ between toolchains.** Wii MWCC and Xbox360 MSVC lay
   structs out differently; do not treat an offset mismatch (e.g. 0x6c vs 0x78) as
   a disagreement. Judge on call targets, constants, string refs, and control flow.
2. **Xenon is 64-bit-register PPC.** Expect `& 0x1fffffff` / `(ulonglong)` /
   `param & 0xffffffff` narrowing artifacts in the pseudo-C that have no Wii
   counterpart — toolchain noise, not a real difference.
3. **The Xenon name is meaningless** (`Function_<addr>` / `FUN_<addr>`); the binary
   is stripped. Only the Wii symbol is ground truth.
4. **44 of 166 Xenon callees stayed unresolved** — either genuinely unmatched in
   this run or matched to a `Function_<addr>` placeholder (shown as
   _(unmatched / Function_)_). Absence of a resolved name is not evidence against;
   presence of an *agreeing* name is evidence for.
5. **m2c is decompiled from the same Bank-8 asm shown below it** — it's a reading
   aid, not independent evidence. When m2c and the asm seem to disagree, trust the
   asm.
6. The 4 objdump-only Wii packs (08/13/16/17) have **no m2c**; judge from the raw
   asm (callee names are resolved inline by llvm-objdump).

## For the next agent

- **All 30 packs are ready to judge** at `docs/decomp/xenon-hardening/round2/evidence/pair-<01..30>.md`.
  No Ghidra needed — everything (both sides + callee resolution + strings) is in
  the markdown. Dispatch one judge per pack, or batch.
- **To re-extract or extend the sample:** the four forensics scripts are
  idempotent. Bump the sample size by editing the `pick(...)` counts in the
  sampling step (keep `Random(42)` for reproducibility) and re-run steps 2-4. The
  Xenon step is the only one needing the JVM/fork-Ghidra (one session, ~3 min);
  the gzf import into `/tmp/claude/` never touches the ghidriff project.
- **Raw extraction artifacts** (for programmatic consumption, not judging):
  `forensics/xenon_evidence.json` (pseudo-C/callees/strings keyed by pair_id),
  `forensics/wii_asm/`, `forensics/wii_m2c/`, `forensics/evidence_summary.json`.
- **Known sample bias to report up:** this is the **band3 ACCEPT non-seed** stratum
  only (the brief's scope). It does NOT touch the VT-fed CAUTION tier, the
  system/network strata, or any seed. A judge's verdict here estimates band3 ACCEPT
  precision, not whole-tier precision.
- **Open question this substrate can answer (next round):** the run-3 record flags
  band3 as BSim's weakest stratum on the pessimistic dc3-BinDiff oracle (0.193).
  These 21 BSIM packs (sim×conf 15–47) are exactly the set to settle whether that
  pessimism is an oracle artifact (semantic-vs-structural disagreement) or a real
  band3 precision dip. The callee-graph corroboration in the packs already leans
  strongly toward "correct," but human/agent judgement on the 30 is the measurement.
```
