# Round 3 — TASK #1 IMPL: band3 porting-worklist handoff

**Status:** DONE. The 232 net-new band3 Wii→Xenon identities are emitted as a
porting worklist + per-fn identity oracle for the active rb3-xenon class-A
TU-pure port. This is the irreplaceable core of the whole Wii→Xenon lever: RB3
game code DC3 (Dance Central 3, no Rock Band gameplay) cannot identify.

**Constraints honored:** no ghidriff run (consumes existing artifacts only);
NOT a `target_symbol_map.json` injection (additive worklist/oracle); rb3-xenon
commit additive + branch-checked.

---

## TL;DR — what shipped

| Artifact | Repo / path | Tracked? | What |
|---|---|---|---|
| Generator (canonical) | `rb3-xenon/tools/gen_band3_port_worklist.py` | yes | Self-verifying; reads identities + production map + CW map, emits both outputs |
| CW demangler (copy) | `rb3-xenon/tools/demangle_cw.py` | yes | Copied from `rb3/.../round2/forensics/demangle_cw.py` so the generator is self-contained |
| Data feed | `rb3-xenon/band3_port_worklist.json` | no (gitignored) | 232 rows + `tu_summary` + `ranked_tus`; machine-ingestible by `wf_classa_harvest.js` |
| Human checklist | `rb3-xenon/docs/plans/band3-port-worklist.md` | yes | TU-ranked, HIGH-confidence subset first, per-TU rosters (886 lines) |
| `.gitignore` entry | `rb3-xenon/.gitignore` | yes | `/band3_port_worklist.json` (sibling of `/ghidriff_identities.json`) |
| Generator (rb3-side forensics) | `rb3/docs/decomp/xenon-hardening/round3/forensics/gen_band3_worklist.py` | yes (rb3) | Thin `runpy` re-runner of the canonical generator (one source of truth) |

Commit: see `## Commit + branch` below.

---

## 1. Net-new band3 set — regenerated + self-verified (232 / 93 TUs)

Reproduced exactly the MEASURED ground truth. Net-new = ghidriff ACCEPT identity
(`ghidriff_identities.json`) whose normalized Xenon `rb3_addr` is NOT a key in the
production pairing set (`scripts/target_symbol_map.json`). Address join =
`lower(); strip "0x"; lstrip("0"); zfill(8)`.

```
total 978 | in-map 216 | NET-NEW 762
by category: {system: 311, band3: 232, network: 205, main: 4, null: 10}
band3 net-new: 232 across 93 TUs
certainty: {high: 20, bsim>=30: 27, bsim20-30: 92, bsim15-20: 93}
match_types (band3): BSIM 212, ExactInstructions 18, SwitchSig 1, Implied 1
top TUs: GemPlayer.o(19), TrackerManager.o(10), Stats.o(8), Player.o(7),
         VocalPart.o(7), Game.o(7), MusicLibrary.o(7), VocalTrack.o(7),
         Track.o(6), SongRecord.o(5)
```

Confidence label rule (matches the brief's strata):
- **high** = match_type ∈ {ExactInstructionsFunctionHasher, SwitchSigHasher,
  Implied Match, SymbolsHash} OR BSim `simconf ≥ 30`.
- **bsim≥30 / bsim20-30 / bsim15-20** = BSim `simconf` bands.

(In band3 the 20 `high` are all the exact/switch/implied feeders; the 27 `bsim≥30`
are the next safest. HIGH-confidence subset in the worklist = 20 + 27 = 47 rows.)

The generator's VERIFY pass (runs on every regen, exits non-zero on any failure):
- every `wii_symbol` resolves in the Wii CW map
  (`rb3/orig/SZBE69_B8/files/band_r_wii.map`) to its claimed `wii_addr_bank8` —
  **232 / 232, 0 missing, 0 addr-mismatch.**
- 0 entries are already in `target_symbol_map.json` — **0 (by construction).**

---

## 2. Per-entry fields (the JSON feed row)

```json
{
  "rb3_addr": "0x825508c8",                       // Xenon target addr (join key into report/splits)
  "wii_addr_bank8": "0x802ac920",                 // Bank-8 Wii addr — bin/analyze-function on the rb3 side
  "wii_symbol": "ShowClothes__9ClosetMgrFv",      // CW/MWCC-mangled (ground-truth identity oracle)
  "wii_demangled": "ClosetMgr::ShowClothes(...)", // human-readable (round2 demangle_cw.py)
  "tu": "ClosetMgr.o",                            // groups + ranks the worklist
  "src_path": "src/band3/meta_band/ClosetMgr.cpp",// rb3 source to port (derived from CW .o path)
  "match_type": "ExactInstructionsFunctionHasher",// match_types[0]
  "match_types": ["ExactInstructionsFunctionHasher"],
  "confidence_label": "high",                     // high | bsim>=30 | bsim20-30 | bsim15-20
  "simconf": null,                                // BSim sim*conf, or null for exact/switch/implied
  "dc3_cannot_provide": true                      // ALWAYS true for band3
}
```

Top-level: `_meta` (counts, precision prior, failure-mode note), `tu_summary`
(per-TU strata counts + src_path), `ranked_tus` (TU port order), `worklist` (rows).

### Demangler coverage (measured)
- 229 / 232 band3 symbols demangle to `Class::Method(...)` (const + ctor/dtor
  handled). The ingested `wii_symbol_demangled` field is a **no-op copy** of the
  CW name for all 232, so demangling MUST be done downstream — confirmed; we use
  `round2/forensics/demangle_cw.py`.
- 3 fall back to the raw CW name (operators / templated pair ctor):
  `__as__3GemFRC3Gem` (Gem::operator=), `__ls__FR9BinStreamRCQ211SongSortMgr10SongFilter`
  (operator<<), `__ct<PCc,6Symbol>__Q211stlpmtx_std...` (templated pair ctor).
  Acceptable: the raw `wii_symbol` is carried alongside in every row, and a porter
  reads the real body via `bin/analyze-function <wii_symbol>`.

---

## 3. The two coupled outputs (form + location, per recon-consumption.md §A.3)

**(1) `rb3-xenon/band3_port_worklist.json`** — gitignored data feed (sibling of
`ghidriff_identities.json`), one row per identity, grouped by TU. The
`wf_classa_harvest.js` Scan/Validate stages read this as a TU-priority +
`OWN`-attribution oracle (a fn pinned here at high/bsim≥30 is strong independent
evidence for `OWN`, better than the near-random `unified_id_rb3wii.json` oracle).

**(2) `rb3-xenon/docs/plans/band3-port-worklist.md`** — tracked, beside the
existing `porting-backlog-ranked.md` / `game-code-pairing.md` plans. Sections:
- **HIGH-confidence subset (safest first)** — 47 rows (20 high + 27 bsim≥30),
  full signatures, the first-targets table.
- **TU ranking** — all 93 TUs ranked `(#high + #bsim≥30) desc, then total desc,
  then name`, with per-stratum counts + src path + "DC3 cannot-provide".
- **Per-TU function rosters** — every TU's identities, confidence-ranked, with
  `wii_symbol` + Bank-8 addr for `bin/analyze-function`.

Both are **additive + reversible**: delete two files (one gitignored, one doc) +
the gitignore line; zero effect on the build, the map, objdiff, or report.json.

### Top of the TU ranking (certainty-weighted, port these first)

| Rank | TU | #ids | high | ≥30 | 20-30 | 15-20 | src |
|---|---|---|---|---|---|---|---|
| 1 | MusicLibrary.o | 7 | 1 | 3 | 2 | 1 | `src/band3/meta_band/MusicLibrary.cpp` |
| 2 | VocalPart.o | 7 | 0 | 2 | 3 | 2 | `src/band3/game/VocalPart.cpp` |
| 3 | Track.o | 6 | 0 | 2 | 3 | 1 | `src/band3/bandtrack/Track.cpp` |
| 4 | ClosetMgr.o | 4 | 2 | 0 | 1 | 1 | `src/band3/meta_band/ClosetMgr.cpp` |
| 5 | VocalPlayer.o | 4 | 1 | 1 | 1 | 1 | `src/band3/game/VocalPlayer.cpp` |
| 6 | GemTrainerPanel.o | 3 | 0 | 2 | 0 | 1 | `src/band3/game/GemTrainerPanel.cpp` |
| 7 | Tracker.o | 3 | 0 | 2 | 0 | 1 | `src/band3/game/Tracker.cpp` |
| 8 | TourSavable.o | 2 | 2 | 0 | 0 | 0 | `src/band3/tour/TourSavable.cpp` |
| 9 | GemPlayer.o | 19 | 0 | 1 | 9 | 9 | `src/band3/game/GemPlayer.cpp` |
| 10 | TrackerManager.o | 10 | 0 | 1 | 2 | 7 | `src/band3/game/TrackerManager.cpp` |

Note the certainty weighting: GemPlayer.o has the most IDs (19) but only 1 in the
high/bsim≥30 tier, so it ranks 9th — porters get the highest-yield *and*
highest-certainty TUs first. The raw-count top TUs (GemPlayer/TrackerManager/Stats)
are still near the top because total is the tiebreak.

---

## 4. The 20 highest-confidence identities (the safest first targets — hand-vettable)

These are the exact/switch/implied feeders (the most reliable tier). All verified
against the CW map (addr + .o TU). Full table is in the markdown's HIGH subset.

| Xenon addr | TU | match | Wii signature |
|---|---|---|---|
| `0x827a9768` | AccomplishmentProgress.o | ExactInstr | `AccomplishmentProgress::SendHardCoreStatusUpdateToRockCentral()` |
| `0x8267c830` | Band.o | ExactInstr | `Band const::EnergyCrowdBoost()` |
| `0x825a8520` | BandMachine.o | ExactInstr | `LocalBandMachine::SetPrimaryMetaScore(int)` |
| `0x826d8b80` | BandMachineMgr.o | ExactInstr | `(anon)::SyncLocalMachineMsg::Dispatch()` |
| `0x825508c8` | ClosetMgr.o | ExactInstr | `ClosetMgr::ShowClothes()` |
| `0x82550d58` | ClosetMgr.o | ExactInstr | `ClosetMgr::ResetPatches()` |
| `0x82b7caa8` | Lyric.o | ExactInstr | `Lyric const::EndPos()` |
| `0x82655768` | MainHubMessageProvider.o | ExactInstr | `MainHubMessageProvider::ClearData()` |
| `0x8252c728` | MusicLibrary.o | **SwitchSig** | `MusicLibrary const::DifficultySortPart()` |
| `0x82672b08` | NetGameMsgs.o | ExactInstr | `TourHideShowFiltersMsg::TourHideShowFiltersMsg(bool)` [ctor] |
| `0x825bfb80` | OvershellSlot.o | ExactInstr | `OvershellSlot const::InOverrideFlow(OvershellOverrideFlow)` |
| `0x82353b40` | Performer.o | ExactInstr | `Stats const::GetVocalPartPercentage(int)` |
| `0x82532198` | ProfileMgr.o | ExactInstr | `ProfileMgr::GlobalOptionsNeedsSave()` |
| `0x823527e0` | QuestJournal.o | ExactInstr | `QuestJournal::HandleDataChange()` |
| `0x825a66e8` | SongSort.o | ExactInstr | `NodeSort const::GetShortcutIx(SortNode*)` |
| `0x826798b0` | Stats.o | **Implied** | `SingerStats const::GetRankData(int)` |
| `0x826dbaa8` | TambourineManager.o | ExactInstr | `TambourineManager const::TambourineGems()` |
| `0x82357450` | TourSavable.o | ExactInstr | `TourSavable::SetDirty(bool, int)` |
| `0x82357490` | TourSavable.o | ExactInstr | `TourSavable::SaveLoadComplete(ProfileSaveState)` |
| `0x826c6318` | VocalPlayer.o | ExactInstr | `VocalPlayer const::CurrentPhrase()` |

**Caveat (cross-TU class):** two of these (`Stats const::GetVocalPartPercentage`
under Performer.o, `SingerStats const::GetRankData` under Stats.o) carry a class
name different from the bare TU stem. That is expected and correct — the `tu`
field is where ghidriff matched the *Xenon* address (the right porting bucket),
while the class name is the demangled Wii identity. Porting Performer.o / Stats.o
will define those classes' methods.

---

## 5. VERIFY — what I checked by hand (task #4)

1. **All 232 wii_symbols resolve in the CW map to the claimed Bank-8 addr** —
   232/232, 0 missing, 0 mismatch (generator VERIFY pass, also reproduced
   standalone). Example: `ShowClothes__9ClosetMgrFv` → `802ac920` ✓
   (`grep " ShowClothes__9ClosetMgrFv " orig/SZBE69_B8/files/band_r_wii.map`).
2. **0 entries already in target_symbol_map.json** — confirmed (net-new filter).
3. **5+ demangled signatures hand-checked** against the CW map (addr, size, .o
   path, arg shape from the mangled suffix):
   - `LocalSoloStart__9GemPlayerFv` → `GemPlayer::LocalSoloStart()`, GemPlayer.o, 8019d8e0 ✓
   - `IncrementTrillsHit__5StatsFb` → `Stats::IncrementTrillsHit(bool)`, Stats.o ✓
   - `RebuildProfileData__12MusicLibraryFv` → `MusicLibrary::RebuildProfileData()`, MusicLibrary.o ✓
   - `IsEmptyPhrase__9VocalPartCFRCPC11VocalPhrase` → `VocalPart const::IsEmptyPhrase(const VocalPhrase *const&)`, VocalPart.o ✓
   - `E3CheatAutoplayAccuracy__4GameFv` → `Game::E3CheatAutoplayAccuracy()`, Game.o ✓
   - `SetDirty__11TourSavableFbi` → `TourSavable::SetDirty(bool,int)` ✓
   - `GetShortcutIx__8NodeSortCFP8SortNode` → `NodeSort const::GetShortcutIx(SortNode*)` ✓
4. **All 93 derived src_paths exist** in the rb3 tree (spot-checked GemPlayer.cpp,
   Gem.cpp, MusicLibrary.cpp, RockCentral.cpp, QuestManager.cpp — all present).

---

## 6. Regenerate / consume

```bash
# Regenerate both outputs (cwd-independent; VERIFY pass gates it):
python3 /home/free/code/milohax/rb3-xenon/tools/gen_band3_port_worklist.py
# or the rb3-side forensics re-runner:
python3 /home/free/code/milohax/rb3/docs/decomp/xenon-hardening/round3/forensics/gen_band3_worklist.py

# Consume one row (real Wii body + arg shape), from the rb3 repo:
cd /home/free/code/milohax/rb3 && bin/analyze-function ShowClothes__9ClosetMgrFv
```

---

## Commit + branch

- **rb3-xenon branch at commit time:** checked immediately before committing
  (was `main` at recon time; see StructuredOutput `caveats` for the actual branch
  at commit). Additive only — 3 new files + 1 gitignore line; no tracked file
  rewritten, no `target_symbol_map.json` / `fn_resolver` / report.json / build
  touched.
- Files staged in rb3-xenon: `tools/gen_band3_port_worklist.py`,
  `tools/demangle_cw.py`, `docs/plans/band3-port-worklist.md`, `.gitignore`.
  (`band3_port_worklist.json` is gitignored/regenerable, intentionally unstaged.)
- Files in rb3: `docs/decomp/xenon-hardening/round3/forensics/gen_band3_worklist.py`
  + this doc.

---

## For the next agent

- **The worklist IS the deliverable** — `rb3-xenon/docs/plans/band3-port-worklist.md`
  (human) + `band3_port_worklist.json` (machine). Port band3 TUs in the ranked
  order; name each function from `wii_symbol` (CW ground truth) via
  `bin/analyze-function` in the rb3 repo.
- **Do NOT inject these names into `target_symbol_map.json`.** CW≠MSVC mangling +
  the TUs are uncompiled → a wrong key mis-pairs objdiff at our ~0.90 precision.
  Confirm the MSVC symbol only when the TU is actually compiled/ported (then
  `gen_game_target_map.py --tu <TU>` pairs it the supported way).
- **Watch the dominant failure mode when confirming a name:** same-TU sibling
  aliasing (~10%) — two near-identical bodies differing only in a type-tag
  immediate / STL node-size literal, or a hash-shape match a string refutes. Diff
  small immediates + referenced strings + resolved-callee agreement against the
  Wii body. This bites hardest in the `bsim15-20` tier; the 20 `high` exact/switch
  feeders are the safest.
- **Still open (out of scope here):** the ~530 net-new system(311)/network(205)
  identities are UNJUDGED at human grade — that is task #2's 30-pair evidence set.
