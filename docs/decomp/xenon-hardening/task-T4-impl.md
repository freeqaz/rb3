# Task T4 — Eval/oracle hygiene + direct rb3wii seed ingestion (IMPLEMENTED)

**Date:** 2026-06-10
**Author:** T4 implementer (Opus)
**Status:** DONE. All offline verification passed. Files: `tools/ghidra/eval_xenon_matches.py`,
`tools/ghidra/test_eval_xenon_matches.py`, `tools/ghidra/build_xenon_seeds.py`,
regenerated `build/SZBE69_B8/ghidra/xenon-seeds/` (gitignored build artifacts).

**Read first:** `PLAN.md` (§3 score schema, §4 T4 rationale, rejection list),
`scout-failure-forensics.md` (§1B stub oracle, §1D V1/V2 alias+arity), `scout-recall-levers.md`
(§2.3 rb3wii seeds, §4 stratification).

---

## TL;DR results (measured, not estimated)

| Item | Result |
|---|---|
| Eval regression (all features OFF) | **byte-identical** to stored `eval_report.json` |
| VT judged precision, `--credit-platform-alias` ON | **0.324 → 0.486** (12→18 correct / 37) |
| OVERALL judged precision, alias ON | 0.440 → **0.500** |
| The 6 flips | 5 platform twins + 1 arity (QuatKeys::SetFrame Fff↔MMM) — all named pairs |
| Seeds: rb3wii-direct added | **+24 net** (1189 → **1213**); 15 band3 + 9 network |
| Holdout overlap in new seeds | **0** (asserted) |
| Test suite | **36/36 pass** (16 existing + 20 new) |

**Honest deviations from the task's stated expectations** (both explained below, with evidence):
1. VT precision came to **0.486, not ~0.51**. The forensics "~0.51" was an estimate of "(12+7)/37";
   the actual high-conf judged set has exactly **6** flippable pairs, giving 18/37 = 0.486. There is
   no 7th. (The other arity/alias cases — DataThisPtr::Replace, CharServoBone::DoRegulate,
   CameraManager::NumCameraShots, FixedSizeSaveable::LoadStd, WiiDOFProc, BandSongMgr, etc. — are
   real and now credited, but they are **not high-confidence bindiff pairs**, so they don't enter the
   judged precision. They do clean up the `all_entries` disagree noise.)
2. Seeds added **+24, not +~41**. The scout's "41" was "rb3wii pairs not in current seeds" but did
   **not** subtract the **16** that are in the eval **holdout** (which MUST be excluded). 45 hi-conf
   − 4 already-seeds − 16 holdout − 1 unresolvable = **24**. This is the correct, holdout-clean count.
3. **CRITICAL: the rb3wii `wii_addr` field is Bank 5, not Bank 8** — ingesting it raw would have
   produced WRONG seeds. I re-resolve by NAME to the Bank 8 address instead. See §3.

---

## 1. What changed and why

### `tools/ghidra/eval_xenon_matches.py`

All new behaviour is **default-OFF** so the report is byte-identical to legacy unless a flag is set.

- **Platform-alias + arity crediting** (forensics §1D V1/V2). New helpers near the top
  (`eval_xenon_matches.py:94-200` block): `_PLATFORM_PREFIXES` (`:94`), `_CLASS_ALIAS_PAIRS`,
  `_strip_platform_prefix`, `_class_tokens_alias`, `_scope_aliases`, `_credit_aliased`,
  `ARITY_TOLERANCE=1`. `judge_agreement(..., credit_aliases=False)` (`:314`) now returns a tagged
  verdict `agree (platform-alias)` / `agree (arity-tolerant)` / `agree (platform-alias+arity-tolerant)`
  when the keys differ ONLY by a Wii↔Xbox class rename and/or an arity drift ≤1, with same method +
  constness. `is_agree()` / `verdict_bucket()` (`:297`, `:303`) classify verdicts. CLI:
  `--credit-platform-alias`.
  - The mechanism is conservative: a rename is credited only when, after prefix normalization,
    `scope+method+arity(±1)+constness` ALL line up. `KeylessHash<char,char>` vs
    `KeylessHash<AllocInfo,void>` (a genuine different instantiation) is **rejected** — verified by
    test and on the real data (stays `disagree`).
  - **Why this lives in the eval, NOT in `normalize_demangled`:** both manglings parse CORRECTLY —
    QuatKeys::SetFrame genuinely is `(float,float)` on Wii and `(float,float,float)` on Xbox. There
    is no `normalize_demangled` parse bug. Relaxing the seed join key would FABRICATE wrong seeds
    (PLAN.md keeps seeds strict, `--min-sim` ≥ 1.0). So crediting is a judgment-layer concern only.
- **`--exclude-match-types`** (default ""): drops a match type before scoring. A pair survives if it
  carries any non-excluded type; pairs whose entire type-set is excluded are dropped and counted in
  `totals.filtered_excluded_match_types`. Triage loop `:436-475`.
- **`--min-vt-score` + `--sweep-vt-score`** (PLAN.md §3 contract): consumes the OPTIONAL
  `scores.VTCombinedReference.product` field per match. Below the floor → the VT type is stripped
  (pair dropped if VT was its only type), counted in `totals.vt_below_min_score_culled`. Field
  ABSENT → filter inactive (legacy matches.json replays byte-identically). `parse_sweep()` /
  `run_vt_sweep()` (`:815`, `:834`) print precision/yield per threshold and exit (no report written).
- **`--low-trust-stub` / `--stub-size-max 88`** (forensics §1B): a BinDiff `disagree` on a Wii
  function ≤ `stub_size_max` bytes is bucketed `low_trust_stub` instead of hard-wrong (BinDiff pairs
  identical `return Symbol(...)` shapes arbitrarily). `parse_wii_map_index` now also captures the CW
  `size` column (`:290`). dc3 (b) block `:543-567`.
- **`--stratify`** (scout 4 §4): emits `precision_by_match_type_by_category`
  (band3/system/network/sdk/main) — the aggregate VT 0.324 hides ~0% band3 vs 40-54% engine.
- All extra report keys are emitted **only when their feature is active** (`_agree_stats` conditional,
  `totals` conditional adds, `precision_by_match_type_by_category` only with `--stratify`) — this is
  what preserves byte-identity.

### `tools/ghidra/build_xenon_seeds.py`

- **Direct rb3wii ingestion** (`_ingest_rb3wii_seeds` `:456`, called at `:714`). Reads
  `rb3-xenon/unified_id_rb3wii.json`, filters `sim≥1.0 ∧ conf≥0.95`, re-resolves each `wii_name`
  to its **Bank 8** address via the existing 1:1-unique `wii_by_key` normalize-join (NOT the file's
  Bank 5 `wii_addr` — see §3), dedups against existing pairs by both endpoints, tags
  `prov="rb3wii-direct"`. Holdout exclusion + range checks apply downstream in the shared pipeline.
  `_normalize_rb3wii_name` (`:430`) fixes the file's `_const`→` const` and `,_`→`, ` rendering so
  `normalize_demangled` parses constness/arity right.
- **Provenance** on every seed (`prov` in `seeds_detail.json`; `seeds_by_provenance` in `stats.json`).
- **RTTI seeds** (`_ingest_rtti_seeds` `:502`), behind `--rtti-seeds` (default OFF). Joins
  `unified_id_rtti.json` `dc3_name` → Wii Bank 8 addr via the MSVC normalize-join, skipping
  `{scalar/vector deleting destructor}`. Verified runs (adds 170 at sim/conf 0.8) but stays OFF and
  **unvetted** — its confidence is lower than the sim=1.0 default; measure precision before using.
- New CLI: `--rb3wii`, `--rb3wii-min-sim`, `--rb3wii-min-conf`, `--rtti-seeds`, `--rtti-ids`.
- `DEFAULT_RB3WII`, `DEFAULT_RTTI` path defaults (`:80-81`).

### `tools/ghidra/test_eval_xenon_matches.py`

+20 tests (36 total). New classes `TestPlatformAliasCrediting` (alias table, the exact
**QuatKeys Fff/MMM arity regression**, both-tags, the rejection cases incl. KeylessHash, default-off
gating), `TestExcludeAndScoreFilters` (exclude-types, **scores-field-optional backward-compat**,
min-vt-score culling with scores present, low-trust-stub bucketing, stratify), `TestParseSweep`.

---

## 2. Verification protocol + FULL output

### (1) Unit tests — `python3 tools/ghidra/test_eval_xenon_matches.py`
```
Ran 36 tests in 0.004s
OK
```
Includes the required cases: alias-credit, the exact `SetFrame__8QuatKeysFff` ↔
`?SetFrame@QuatKeys@@UAAXMMM@Z` Fff/MMM arity pair, exclude-types, scores-field-optional
backward-compat.

### (2) Eval regression — byte-identical with all features OFF
```bash
build/SZBE69_B8/ghidra/ghidriff-venv/bin/python tools/ghidra/eval_xenon_matches.py \
  --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon --out /tmp/eval_replay.json
diff build/SZBE69_B8/ghidra/ghidriff-xenon/eval_report.json /tmp/eval_replay.json   # EMPTY
```
Result: `raw diff exit: 0  lines: 0` AND sorted-keys semantic diff = 0 lines. (Re-confirmed against
the original 1189-seed backup after seed regen: byte-identical excluding only `inputs.seeds` path.)

### (2b) Eval with alias+arity ON — VT precision rises, named pairs flip
```bash
... eval_xenon_matches.py --run-dir <run> --credit-platform-alias --stratify
```
```
--- (b) DC3 (bindiff) agreement ---
  all_entries: agree 149 [exact 136 + alias 13] / disagree 219 / unjudgeable 35  (agreement 0.405)
  high_conf:   agree 21  [exact 15  + alias 6]  / disagree 47  / unjudgeable 20  (agreement 0.309)
--- (d) precision proxy ---
VTCombinedReference   37   18   19   0.486      # was 12/37=0.324
OVERALL               100  50   50   0.500      # was 44/100=0.440
```
The **6 high-conf flips** (all named in the task / forensics §1D):
| verdict | Wii symbol | DC3 name |
|---|---|---|
| agree (platform-alias) | `SetTex__8WiiMovieFP6RndTex` | `?SetTex@DxMovie@@…` |
| agree (arity-tolerant) | `SetFrame__8QuatKeysFff` | `?SetFrame@QuatKeys@@UAAXMMM@Z` |
| agree (platform-alias) | `CheckShotOver__11BandCamShotFf` | `?CheckShotOver@HamCamShot@@…` |
| agree (platform-alias) | `IsDownload__16BandSongMetadataCFv` | `?IsDownload@HamSongMetadata@@…` |
| agree (platform-alias) | `Select__11WiiPostProcFv` | `?Select@NgPostProc@@…` |
| agree (platform-alias) | `ComputeElbowPullAndQuat__14BandIKEffector…` | `?…@HamIKEffector@@…` |

`KeylessHash<char,char>` ↔ `KeylessHash<AllocInfo,void>` correctly **stays disagree** (different
template instantiation, not a rename).

### (2c) Other features (smoke-verified on the real run dir)
- `--exclude-match-types StringsRefsHasher,StrUniqueFuncRefsHasher`: drops 655 noise pairs
  (scored 1458→803), removes them from the precision table, OVERALL 0.440→0.611, judged 100→72.
- `--sweep-vt-score 9.5,11,13,20`: kept constant at 37 (scores ABSENT in this matches.json ⇒ filter
  inactive — exactly the backward-compat contract; the constant-`kept` printout makes it obvious).
- `--low-trust-stub`: moves 139 (all) / 30 (high-conf) tiny-stub disagrees into `low_trust_stub`;
  high-conf agreement 0.221→0.395.

### (3) Seeds — regenerated count + zero holdout overlap + spot-check
```bash
build/.../ghidriff-venv/bin/python tools/ghidra/build_xenon_seeds.py --spot 0
```
```
final_seeds: 1213                     # old 1189 + 24
rb3wii_total: 9301  rb3wii_filtered: 45  rb3wii_added: 40   # 40 resolved; 16 later dropped by holdout
seeds_by_provenance: {dc3-bindiff-cpp: 1058, dc3-bindiff-plain: 131, rb3wii-direct: 24}
rb3wii drops: {rb3wii_dup_existing_p2: 4, rb3wii_no_bank8_addr_for_name: 1}
excluded_eval_holdout: 46             # was 30 (+16 rb3wii ∩ holdout)
```
Asserts (script): **holdout overlap = 0**; all 1189 old seeds preserved (0 lost), exactly 24 added;
p1 & p2 unique (the builder's own `assert` at `build_xenon_seeds.py:754-755` also passes).

The 24 split **15 band3** (GemManager, GemRepTemplate, Band, BandUserMgr, DirectInstrument, Game,
GameConfig, Performer, Stats, TrainerPanel, VocalPart, VocalPlayer, MusicLibrary, SongSort) **+ 9
network** (Quazal). Spot-check of 5 (Bank 8 `p1` → CW map name):
```
0x80021320 -> DeleteInstance__Q26Quazal8PlatformFv      = Quazal::Platform::DeleteInstance()      ✓
0x8002a3a0 -> socket__Fiii                                = socket(int,int,int)                     ✓
0x800a6ff0 -> DispatchRMCResult__Q26Quazal11_DOC_RootDO…  = Quazal::_DOC_RootDO::DispatchRMCResult…  ✓
0x80134e60 -> SetInCoda__10GemManagerFb                   = GemManager::SetInCoda(bool)             ✓
0x80180b70 -> SetVocalCueVolume__4GameFf                  = Game::SetVocalCueVolume(float)          ✓
```
All Bank-8 addresses resolve to the exact function the seed claims.

---

## 3. THE BANK-5/BANK-8 LANDMINE (most important finding for the verifier)

`rb3-xenon/unified_id_rb3wii.json`'s `wii_addr` is in the **Bank 5 DWARF ELF** address space (the only
Wii build BinDiff could run against), **NOT** the Bank 8 target ghidriff diffs against (`p1` =
`bank8_target.elf`). The two builds have ~2010-vs-mid-2009 layout drift (see MEMORY / CLAUDE.md
"bank divergence"). PROOF:
- rb3wii says `GemManager::SetInCoda` is at `wii_addr=0x8013cd10`.
- In the **Bank 8 map**, `0x8013cd10` = `RebuildBeats__8GemTrackFv` — a DIFFERENT function.
- The real Bank 8 `SetInCoda__10GemManagerFb` lives at `0x80134e60`.
- In the **Bank 5 ELF** (`nm`), `0x8013cd10` IS `SetInCoda__10GemManager` — confirming Bank 5.

⇒ Ingesting the raw `wii_addr` would have seeded ghidriff with WRONG anchors that point it at the
wrong Bank 8 functions, polluting every downstream cascade. The implementation **ignores `wii_addr`
entirely** and re-resolves each `wii_name` → Bank 8 addr via the same 1:1-unique normalize-join used
for the DC3 seeds. This makes rb3wii ingestion immune to the bank drift.

**Consequence for the task's literal spot-check instruction:** "spot-check 5 new seeds' Wii addresses
against band_r_wii.map (GemManager::SetInCoda wii=0x8013cd10)" — `0x8013cd10` does NOT name
SetInCoda in the Bank 8 map; that address is the Bank 5 one. The correct Bank 8 anchor is
`0x80134e60`, which IS what the seed contains. The literal check would FAIL only because the task
quoted the Bank 5 address; my Bank-8-resolved seed is correct.

---

## 4. Known caveats

- **VT 0.486, not 0.51.** Exact, not an estimate. 6 high-conf flips exist; there is no 7th. The
  forensics doc's 0.51 was a rough "(12+7)/37". I report the measured 0.486 honestly.
- **+24 seeds, not +41.** The +41 ignored holdout overlap. +24 is the correct holdout-clean number.
  Net effect on the next run is small (scout-recall-levers ranks rb3wii a "SMALL IMPACT" lever); its
  value is the **15 band3 anchors** the DC3 oracle cannot reach, not volume.
- The new seeds are **cross-compiler BinDiff at sim=1.0** — "likely correct", not "certain" (different
  ABI). They are gated at the same threshold as the DC3 seeds and re-resolved 1:1-unique by name, so
  precision should be comparable (~90%). 1 (`Quazal::MessagingClient::RegisterMessagingNotificationHandler`)
  was un-joinable to a Bank 8 addr (`rb3wii_no_bank8_addr_for_name`) and dropped — likely inlined/
  absent in Bank 8.
- **RTTI seeds stay OFF and unvetted.** sim/conf=0.8 is below the strict bar; measure precision (e.g.
  via a held-out subset) before enabling. The path is wired (`--rtti-seeds`) and runs clean (+170).
- The eval's `--credit-platform-alias` and the seed regen are **independent**; the 24 new seeds do NOT
  change the eval regression (they'd appear as SeedMatch on the NEXT run; against the OLD matches.json
  they shift scored_pairs 1458→1456 as 2 become seed-conflicts, OVERALL unchanged).
- Score-export (the `scores` field) is produced by **T2** (ghidriff side). Until T2 lands, `--min-vt-score`
  / `--sweep-vt-score` are no-ops on real data (verified: constant kept). The consumer is ready.

---

## For the verifier — exactly what to re-check

1. **Tests:** `python3 tools/ghidra/test_eval_xenon_matches.py` → expect `Ran 36 tests OK`. The
   Fff/MMM regression is `TestPlatformAliasCrediting.test_judge_agreement_gated_off_by_default`
   (off→disagree) + `test_credit_aliased_arity_tolerant`. Backward-compat is
   `TestExcludeAndScoreFilters.test_scores_field_optional_backward_compat`.
2. **Regression (must be byte-identical):**
   ```bash
   build/SZBE69_B8/ghidra/ghidriff-venv/bin/python tools/ghidra/eval_xenon_matches.py \
     --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon --seeds /tmp/xenon-seeds-orig/seeds.json \
     --out /tmp/r.json     # OR regenerate the 1189-seed set if /tmp backup is gone
   ```
   then compare all keys except `inputs` to `build/SZBE69_B8/ghidra/ghidriff-xenon/eval_report.json`.
   NOTE: the live `seeds.json` is now 1213; use the original 1189 set for an exact match (the 24 new
   seeds add 2 seed-conflicts otherwise — metrics identical, but scored_pairs 1458→1456).
3. **Alias gain:** add `--credit-platform-alias` → VT precision must read **0.486** (18/37), OVERALL
   0.500, and the 6 pairs in §2b must show `agree (platform-alias|arity-tolerant)`.
4. **Seeds:** `... build_xenon_seeds.py --spot 0` → `final_seeds: 1213`, prov `rb3wii-direct: 24`,
   holdout overlap 0. Re-verify the §3 Bank-5/Bank-8 claim with `nm` on the Bank 5 ELF vs the Bank 8
   map for `0x8013cd10` if you want to confirm the landmine independently.
5. **Still open (not T4):** the `scores` field producer is **T2**; once it lands, run
   `--sweep-vt-score 9:14:0.5` to pick the VT operating point offline (the consumer is verified ready
   against a scores-present synthetic fixture in `test_min_vt_score_culls_when_scores_present`).

**Commits:** see StructuredOutput (rb3 repo, branch master). Seeds dir is gitignored (build artifact),
regenerated on demand — not committed.
