# Task T2 — Adversarial verification of the rb3-xenon ingest (Fable)

**Date:** 2026-06-11
**Verifier:** Fable 5
**Verdict:** **CONFIRMED** — every load-bearing claim independently reproduced; refutation attempts failed.
**Implementer doc verified:** `docs/decomp/xenon-hardening/round2/task-T2-ingest.md`

---

## What was verified (all offline; no ghidriff/Ghidra; port-8001 untouched)

### 1. Commits exist and match claims

| Repo | SHA | Branch | Contents |
|---|---|---|---|
| rb3 | `6a4779b2` | master | `tools/ghidra/ingest_ghidriff_accepts.py` (399 lines) + `task-T2-ingest.md` — nothing else |
| rb3 | `6793c59a` | master | doc-only (3-line SHA update to task-T2-ingest.md) |
| rb3-xenon | `7bdae6c` | main | `tools/fn_resolver.py` (+83/−2) + `.gitignore` (+1) — nothing else |

### 2. Gate math — reproduced from the injected verdicts

Sources: `docs/decomp/xenon-hardening/round2/forensics/sample_manifest.json` (30 pairs)
+ `forensics/judge_verdicts.json`.

- BSim strata in manifest: `BSIM>=30`:6 + `BSIM 20-30`:8 + `BSIM 15-20`:7 = **21** pairs.
- Verdict summary: bsim correct=19, wrong=2 → **19/21 = 0.9048 ≥ 0.85 → FULL gate**. ✓
- Judged-WRONG pairs map to exactly the claimed xenon addrs:
  - pair-13 → `0x82518de0` (BSIM 20-30, stlport List_base clear)
  - pair-16 → `0x824e51e0` (BSIM 15-20, pair ctor float sibling)
  - pair-29 → `0x8233afb0` (SwitchSig, ActiveScoreType)

### 3. Independent re-derivation → exact set equality

I re-applied the filter rules from scratch against the immutable archive
(`build/SZBE69_B8/ghidra/ghidriff-xenon/run3-archive/vetted_identities.json`,
md5 `dbc440b6b2b67b964b208a7c17af625e` — matches PLAN's recorded value, archive intact):

```
8,527 total → 2,207 ACCEPT → −1,210 SeedMatch-only → 997 non-seed
            → −9 sdk → −7 null wii_symbol → −3 judged-WRONG = 978
```

The resulting set of `(xenon_addr, wii_addr, wii_symbol)` triples is **set-equal**
(978 = 978, zero diff) to `/home/free/code/milohax/rb3-xenon/ghidriff_identities.json`.
Breakdown matches: BSIM 913 / ExactInstr 54 / Implied 8 / SwitchSig 3;
categories system 438 / band3 306 / network 216 / null 14 / main 4.
Every entry has exactly one match type (no multi-type combos → no classification ambiguity).

### 4. Count assertions on the output file (all pass)

- sdk: **0**; SeedMatch-only: **0**; judged-WRONG addrs: **0**; null wii_symbol: **0**.
- Schema: all 978 entries have exactly the 10 specified fields; tier=="ACCEPT" and
  source=="ghidriff-run3" everywhere; both addr fields 0x-prefixed lowercase.
- BSIM simconf: min **15.0024**, max 104.41, mean 24.22 (matches claim); 395 entries in
  [15,20) → conservative gate would keep 518 BSIM + 65 non-BSIM = **583** (claim verified).
- non-BSIM entries all have `bsim_simconf: null`. 0 duplicate `rb3_addr`.

### 5. matches.json join integrity

Joined all 978 entries against the archived
`run3-archive/json/...ghidriff.matches.json` (8,527 function_matches) by
**(p1_addr, p2_addr)** pair: 0 missing joins, **0 simconf mismatches**
(bsim_simconf == scores.BSIM.similarity×confidence to <0.01), **0 p1_name mismatches**
(wii_symbol_demangled == p1_name for all 978).

Note: a naive p2-addr-only join shows 4 "mismatches" — these are Xenon addrs with TWO
rows in matches.json (the ACCEPT row + a stale sub-threshold BSIM/SeedMatch row, e.g.
`0x827a6378` has both a SeedMatch row → `__ct__6HxGuidFv` and the ingested Implied row →
`Clear__6HxGuidFv`). The implementer joined by the correct row. Not a bug.

### 6. Bank-8 ground truth — FULL-population map check (stronger than required)

Parsed `orig/SZBE69_B8/files/band_r_wii.map` (81,684 addressed symbols) and joined every
ingested `wii_addr_bank8`: **978/978 agree** (0 not-in-map, 0 symbol mismatches). This
proves the addresses are genuinely Bank 8. The 5 required spot-checks (incl. pair-22
`SetPrimaryMetaScore__16LocalBandMachineFi` @ `0x802d6050`, map line 13861) all pass.

### 7. fn_resolver T4b — live behavior + before/after diff

`cd /home/free/code/milohax/rb3-xenon && python3 tools/fn_resolver.py resolve 0x825a8520 0x82586258 0x8276e798 0x82260000`

- `0x825a8520` → best = `[ghidriff_wii_b8] conf=0.94 SetPrimaryMetaScore…` (ExactInstr) ✓
- `0x82586258` → ranking `target_symbol_map(0.97) > gameid_crossval(0.95) > ghidriff_wii_b8(0.93, BSIM simconf 40.17) > rb3wii_bindiff(0.85)` ✓ — bonus cross-check: the ghidriff Wii symbol `Poll__7NetSyncFv` independently agrees with the established MSVC name `?Poll@NetSync@@QAAXXZ`.
- `0x8276e798` → best = ghidriff_wii_b8 conf=0.94 ✓
- `0x82260000` (non-ingested seed) → no ghidriff entry; T3 `dc3_content_match` 0.95 App::~App and T6 `rb3wii_bindiff` 0.85 still resolve exactly as claimed.
- **Before/after proof:** ran the parent-commit (`7bdae6c~1`) fn_resolver on the same
  addrs — output diff is EXACTLY the one added ghidriff_wii_b8 block; everything else
  byte-identical (purely additive change).
- TIER_ORDER: `…"fuzzy_pairs", "ghidriff_wii_b8", "bindiff_dc3"…` (fn_resolver.py ~line 700) ✓; wired into `resolve_all` ✓; loader returns `{}` gracefully when the gitignored file is absent ✓.

### 8. Non-clobber + hygiene

- `scripts/target_symbol_map.json`: working-tree md5 `4a6b2f826e855c8845c3d9f078729859`
  == `git show HEAD:` md5 → byte-identical, untouched, and matches the claimed value.
- `git check-ignore -v ghidriff_identities.json` → `.gitignore:55` in the
  regenerable-indexes block ✓; `git status` clean for all tracked files touched.
- `unified_id_rb3wii.json` NOT extended ✓ (Bank-5 space untouched).
- Ingest script re-runnable: `--gate full --dry-run` reproduces 978 + the identical
  breakdown and prints "Assertions: all passed"; conservative branch (simconf≥20) and
  blocked branch present in `tools/ghidra/ingest_ghidriff_accepts.py:141-157,242-255`.

---

## Minor findings (none refuting; doc-level only)

1. **Caveat-5 in task-T2-ingest.md is muddled and its explanation is wrong** (final
   number right). The SwitchSig non-seed ACCEPT pool is **5**: 3 kept + 1 judged-WRONG
   (`0x8233afb0`) + 1 **sdk** (`__wpadCertWork` @ `0x828dbf98`) — not "minus 1 null_sym"
   as the doc speculates. Output count 3 is correct.
2. **One duplicate wii_addr_bank8**: `0x827bb4f0` (BSIM, simconf 15.14) and `0x827bb458`
   (Implied) both → `Init__11TrackWidgetFv` @ `0x807995c0`. At most one Xenon addr can be
   the exact pairing — inherent ~0.93-tier noise, not an ingest bug. Consumers keying by
   wii addr should be aware; fn_resolver keys by rb3_addr (0 dups there).
3. **85 of the 978 ingested xenon addrs overlap the current holdout.json** (now 158
   entries). No leak today — the eval oracle reads only ghidriff matches.json, never
   rb3-xenon files. BUT: if any future seed-builder ingests `ghidriff_identities.json`
   as a seed source, those 85 would contaminate holdout recall. Guard required then.
4. **PLAN.md STATUS table is stale**: it says "T2 NOT RUN", but the T4 audit commit
   (`0d3afded`) predates T2's commits (Jun 11 10:28). Any round-2 record refresh should
   mark T2 COMPLETE (this doc is the evidence).
5. **Confidence-table ambiguity in the brief** ("ExactInstr/Implied 0.94/0.90" vs
   "SwitchSig 0.90"): the implementer used Implied=0.94 per the scout's section-5 code,
   which the brief designates as the verbatim design source. Defensible; documented in
   the commit message. Implied covers only 8 entries.
6. **Expected ~985 vs actual 978**: the brief's estimate didn't account for the 7
   null-wii_symbol entries, which the brief itself requires excluding. 978 is correct.

## Refutation attempts that failed

- Recomputed gate precision over the wrong stratum set (all 30 → 0.900; non-BSim → 0.889):
  the implementer correctly used the 21-pair BSIM stratum, per the manifest's stratum field.
- Hunted for SeedMatch leakage / sdk smuggling / WRONG-addr survival: 0/0/0 in output.
- Hunted for Bank-5 contamination: full 978-entry map join says all Bank 8.
- Hunted for fabricated simconf values: all 978 reproduce from the archived matches.json.
- Hunted for clobbered consumers: target_symbol_map byte-identical; old-vs-new fn_resolver
  diff purely additive; unified_id_rb3wii untouched.
- Checked archive integrity: run3-archive vetted md5 matches PLAN's planner-recorded value;
  both archived json files present.

## For the next agent

- T2 is DONE and verified. The exploitable artifact is
  `/home/free/code/milohax/rb3-xenon/ghidriff_identities.json` (978 entries, gitignored,
  regenerate with `python3 tools/ghidra/ingest_ghidriff_accepts.py --gate full`).
- If you build a future seed set from ghidriff_identities.json, EXCLUDE holdout addrs
  first (85-entry overlap measured above) or holdout recall becomes a lie.
- When writing the round-2 session record update, fix PLAN.md's stale "T2 NOT RUN" row
  and the caveat-5 explanation in task-T2-ingest.md (SwitchSig pool = 3+1 wrong+1 sdk).
- The TrackWidget::Init dup (finding 2) is a cheap candidate for a one-pair evidence-pack
  judgment if anyone extends the judged sample.
- Verifier left no files in rb3-xenon (temp old-resolver copy removed; untracked
  `auto_*.obj` / `global_fuzzy_pairs.json` in rb3-xenon pre-existed this task).
