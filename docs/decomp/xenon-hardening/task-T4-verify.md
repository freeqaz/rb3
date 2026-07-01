# Task T4 — VERIFICATION (adversarial)

**Date:** 2026-06-10
**Verifier:** Fable (adversarial)
**Implementer doc:** `task-T4-impl.md`
**Commit verified:** rb3 `da52aac04de2baf5e2b2ebbd6456507cb0e146cf` (4 files: eval_xenon_matches.py +425/-,
build_xenon_seeds.py +179/-, test_eval_xenon_matches.py +208, task-T4-impl.md)

## Verdict

**CONFIRMED.** Every load-bearing claim re-verified independently, offline, no ghidriff run, no
service touched. The two "honest deviations" (0.486 not 0.51; +24 not +41) are arithmetic facts I
reproduced, not spin. The Bank-5/Bank-8 landmine claim is real and independently confirmed — the
implementer's name-re-resolution prevented 24+ wrong seed anchors. Two caveats recorded below
(§"Findings that are NOT refutations") that the next agent should weigh; neither invalidates the
implementation.

---

## What I checked and how (all reproducible)

### 1. Commit + scope
`git show da52aac0 --stat` — exists on master, exactly the 4 claimed files, nothing else folded in.
Seeds dir confirmed gitignored (`git check-ignore build/SZBE69_B8/ghidra/xenon-seeds/seeds.json` →
ignored), so "not committed" follows repo convention; deviates from the brief's literal "stage the
regenerated seeds" but is disclosed and correct for a regenerable build artifact.

### 2. Tests — PASS, and they exercise the real paths
`python3 tools/ghidra/test_eval_xenon_matches.py` → **Ran 36 tests, OK** (re-run by me).
Read the new test bodies in the diff: `test_judge_agreement_gated_off_by_default` pushes the EXACT
mangled pair `SetFrame__8QuatKeysFff` ↔ `?SetFrame@QuatKeys@@UAAXMMM@Z` through the real
`judge_agreement` (off→`disagree`, on→`agree (arity-tolerant)`); `test_scores_field_optional_backward_compat`
and `test_min_vt_score_culls_when_scores_present` call the real `evaluate()` with scores absent/present
(absent → pair kept, culled=0; present → 2 culled, multi-type pair survives via Implied). Rejection
tests (KeylessHash different instantiation, arity gap >1, constness, method mismatch) are real
negative controls. Not strawman tests.

### 3. Byte-identity regression — CONFIRMED
```bash
build/SZBE69_B8/ghidra/ghidriff-venv/bin/python tools/ghidra/eval_xenon_matches.py \
  --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon --seeds /tmp/xenon-seeds-orig/seeds.json \
  --out /tmp/verify_t4_replay.json
```
Compared every top-level key vs the stored `eval_report.json` via sorted-keys JSON dump: **all
identical except `inputs`** (path strings only). Also verified `holdout.json` is byte-identical
between `/tmp/xenon-seeds-orig/` and the regenerated live dir (so the replay's holdout input is
unchanged — the regression isn't a coincidence of a mutated holdout).

With the LIVE 1213-seed file (default args): `seed_conflicts 1→3, scored_pairs 1458→1456,
wii_addr_resolved 1393→1391`, `precision_by_match_type` **identical** — exactly the disclosed caveat.

### 4. Alias gain — CONFIRMED, 0.486 / 0.500, and the flip set is exactly as claimed
`--credit-platform-alias` run: VT **18/37 = 0.486**, OVERALL **50/100 = 0.500**, dc3 all_entries
agree 149 [exact 136 + alias 13] (0.405), high_conf 21 [15+6] (0.309) — every number matches the doc.

Diffed the off-vs-on report entry-by-entry (same key sets — no subset games): **holdout list: 0
changed** (alias can't touch exact-addr judgments); **dc3 list: exactly 13 flips**, all
`disagree → agree (platform-alias|arity-tolerant)`. Of the 13, exactly **6 have `high_conf: true`**,
all `VTCombinedReference` — SetTex@WiiMovie↔DxMovie, SetFrame@QuatKeys (arity), CheckShotOver +
IsDownload + ComputeElbowPullAndQuat (Band↔Ham), Select@WiiPostProc↔NgPostProc. VT +6 and OVERALL +6
are fully accounted for; **"no 7th flip" is correct** (the other 7 flips are non-high-conf and
correctly stay out of the precision table). `KeylessHash<char,char>` vs `<AllocInfo,void>` stays
`disagree` on the real data.

The "Fff parse bug" reframing is correct: MWCC `Fff` genuinely = (float,float) and MSVC `MMM` =
(float,float,float) — there is no `normalize_demangled` bug to fix; crediting at the judgment layer
(not the seed join) is the right call (a relaxed seed key would fabricate seeds). The brief's named
regression test exists for the exact pair.

### 5. Other flags — smoke-verified, doc numbers exact
- `--exclude-match-types StringsRefsHasher,StrUniqueFuncRefsHasher`: scored 1458→**803**
  (filtered 655), OVERALL **0.611**, judged 72. ✓
- `--sweep-vt-score 9.5,11,13,20` on the scoreless matches.json: kept **constant 37** at every
  threshold, culled 0 — the §3 backward-compat contract behaves. ✓ (Becomes live only when T2's
  `scores` field lands.)
- `--low-trust-stub`: 139 (all) / 30 (high-conf) disagrees re-bucketed, high-conf agreement
  0.221→**0.395**. ✓

### 6. Seeds — CONFIRMED, including the landmine
- `seeds.json`: 1213 entries; the **1189 old pairs are all preserved** (set-subset check), exactly
  **24 added**, all `prov: rb3wii-direct` in `seeds_detail.json`; `stats.json` provenance
  {cpp 1058, plain 131, rb3wii-direct 24}.
- Arithmetic re-derived from source data: `unified_id_rb3wii.json` has 9301 entries; **45** pass
  sim≥1.0 ∧ conf≥0.95 (my own filter); drops 4 dup-existing-p2 + 1 no-bank8-addr = 40 added
  pre-holdout; `excluded_eval_holdout` 30→**46** (+16) ⇒ **24 net**. The brief's "+41" indeed ignored
  the 16 holdout collisions; +24 is the holdout-clean number.
- **Zero holdout overlap**: I intersected all 1213 seed `p2_addr` against the 146 holdout `addr`
  values myself → **0**.
- **Bank-5/Bank-8 landmine independently confirmed**:
  - Bank 5 ELF (`nm` on `milo-executable-library/.../band_r_wii.elf`): `8013cd10 T SetInCoda__10GemManagerFb`.
  - Bank 8 CW map: `0x8013cd10` = `RebuildBeats__8GemTrackFv`; `SetInCoda__10GemManagerFb` = `0x80134e60`.
  - The committed seed is `(0x80134e60, 0x822c1e30)` — the CORRECT Bank 8 anchor. Raw `wii_addr`
    ingestion would have seeded the wrong function. This is the single most valuable judgment call
    in the task.
- Spot-checked **5** seeds' p1 against the Bank 8 map (DeleteInstance@Quazal::Platform, socket__Fiii,
  DispatchRMCResult@_DOC_RootDO, SetVocalCueVolume@Game, GetSongCount@HeaderSortNode + SetInCoda):
  all resolve to exactly the claimed symbol. Category split ≈15 band3 / ~9-11 network TU lines
  (map has dup-addr lines; immaterial).

---

## Findings that are NOT refutations (for the next agent to weigh)

1. **All 24 new rb3wii-direct seeds are stub-sized.** Bank 8 map sizes: 4–40 bytes (13 of 24 are
   8B, e.g. `SetInCoda` = stw+blr). 32/45 of the hi-conf rb3wii pairs come from BinDiff "prime
   signature matching". This is precisely the shape class forensics §1B calls BinDiff-arbitrary
   ("pairs same-shape stubs arbitrarily"). Mitigations: (a) the trust structure is the SAME as the
   existing dc3-bindiff seeds (BinDiff vouches for the Xenon endpoint in both families; rb3wii is
   same-game, arguably stronger), (b) the brief mandated sim==1.0 ∧ conf≥0.95 with no size gate, and
   the implementer complied. BUT the impl doc's "precision should be comparable (~90%)" is an
   **unverified extrapolation**, and a wrong SEED is worse than a wrong match (it anchors cascades).
   **Recommendation:** before the next full run, either spot-validate a few of the 8-byte seeds by
   instruction shape on the Xenon side, or be ready to attribute any new VT noise clusters around
   these anchors to them.
2. **`--low-trust-stub` removes stub disagrees from the precision DENOMINATOR** (the `judged` list
   only takes buckets agree/agree_alias/disagree, and low_trust_stub is its own bucket). That is per
   the brief ("separate bucket rather than hard wrong") and default-off, and the headline 0.486/0.500
   were computed WITHOUT it — but do not quote precision-with-`--low-trust-stub` against the 0.32
   experiment bar; it's a different (more favorable) metric.
3. The generic prefix rules `("Wii","")` / `("Band","")` in `_PLATFORM_PREFIXES` are broader than the
   explicit twin table (e.g. `BandFoo`↔`Foo` unifies). Theoretical over-credit requires a
   same-method+arity+const coincidence across the renamed classes; on the real data only the 13
   audited flips occurred. Acceptable, just keep the tags audited if the credited set ever grows.

## Commands to reproduce everything
```bash
cd /home/free/code/milohax/rb3
python3 tools/ghidra/test_eval_xenon_matches.py                                  # 36/36 OK
V=build/SZBE69_B8/ghidra/ghidriff-venv/bin/python
$V tools/ghidra/eval_xenon_matches.py --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon \
   --seeds /tmp/xenon-seeds-orig/seeds.json --out /tmp/r.json                    # == stored (excl inputs)
$V tools/ghidra/eval_xenon_matches.py --run-dir ... --seeds /tmp/xenon-seeds-orig/seeds.json \
   --credit-platform-alias --out /tmp/a.json                                     # VT 0.486, OVERALL 0.500
# landmine: nm Bank5 ELF | grep 8013cd10  vs  grep RebuildBeats/SetInCoda Bank8 map
```

## For the next agent
- T4 is sound to build on. The eval is the fixed reference for T2's score sweep
  (`--sweep-vt-score`, consumer verified ready against a synthetic scores fixture).
- Open: (a) T2 must emit the §3 `scores` field before the VT floor is tunable; (b) consider a size
  sanity-check or down-weighting for the 24 stub-sized rb3wii-direct seeds before/after the next
  full run (finding #1); (c) RTTI seeds remain wired-but-OFF and unvetted — measure before enabling.
- /tmp artifacts used here (`/tmp/xenon-seeds-orig/`, `/tmp/verify_t4_*.json`) are ephemeral; the
  regeneration path is `build_xenon_seeds.py` (deterministic, seed=20260609).
