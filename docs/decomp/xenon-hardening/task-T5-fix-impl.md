# T5 Fix Implementation: vet_xenon_identities.py Annotation Defects

**Date:** 2026-06-10
**Task:** Fix four annotation defects identified by the adversarial verifier (task-T5-verify.md §6–9)
**File changed:** `tools/ghidra/vet_xenon_identities.py`

---

## What Changed

### Fix 1 — rb3wii cross-check: name-level join via Bank 5 ELF (verifier §6)

**Problem:** `_rb3wii_check` compared `p1` (Bank 8 address) directly against `rb3wii.wii_addr`
(Bank 5 address). These are different address spaces, so the comparison was meaningless — 28 of
637 "contradicted" entries were actually name-level agreements (proven via the Shuttle example:
xenon `0x8269d338` → Bank8 `0x801d6120 __ct__7ShuttleFv`; rb3wii says `0x801ee270` which nm on
the Bank 5 ELF also resolves to `__ct__7ShuttleFv`).

**Fix:** `_rb3wii_check` now:
1. Looks up `rb3wii.wii_addr` in a Bank 5 ELF symbol table (loaded via `nm` at startup).
2. Compares the Bank 5 mangled name with the Bank 8 map symbol at `p1`.
3. `confirmed` iff names agree; `contradicted` iff names disagree; `absent` if either lookup
   fails (Bank5 ELF missing the address, Bank8 map missing p1).

New function `load_bank5_syms(elf_path)` runs `nm` and returns `{addr: mangled_sym}` for T
(text) symbols only. Default path: `milo-executable-library/rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.elf`.
New CLI arg `--bank5-elf` overrides the path. `vet()` now takes `bank5_syms` as an optional
kwarg (defaults to `{}`; degrades to all-absent when empty).

**File:line:** `_rb3wii_check` at line ~490; `load_bank5_syms` added before rb3wii loader section;
`vet()` signature at line ~275; `main()` at line ~620.

### Fix 2 — Category attribution: replace `_categorize_full` with eval's module extraction (verifier §7)

**Problem:** `_categorize_full` checked for `system_wii\` and `network_wii\` path prefixes which
occur 0 times in the map (the project root is `band3_wii\`, not `system_wii\`). Everything became
`band3` (79% mislabeled).

**Fix:** `_categorize_full` now faithfully mirrors `eval_xenon_matches.py`'s `categorize_tu`: it
splits on `band3_wii\` and extracts the next path component as the module name, yielding `system`,
`network`, `band3`, etc. The map's own path structure is `band3_wii\<module>\src\...`.

**File:line:** `_categorize_full` at line ~173; docstring for `parse_wii_map_index` updated to
remove the stale NOTE about system/network being undetectable.

### Fix 3 — Remove ExactMnemonicsFunctionHasher from default ACCEPT (verifier §8)

**Problem:** `ExactMnemonicsFunctionHasher` was silently included in the default `accept_types`
despite being an unmeasured cross-compiler type — contradicting the tool's own docstring ("REJECT
unless allowlisted via --accept-types") and the brief's rule.

**Fix:** Removed from `DEFAULT_TIER_CONFIG["accept_types"]`. The type now reaches `CAUTION` by
default (no reject_type entry). Still fully promotable via `--accept-types ExactMnemonicsFunctionHasher`.

**File:line:** `DEFAULT_TIER_CONFIG` at line ~60.

### Fix 4 — Emit `wii_addr` unconditionally (verifier §9)

**Problem:** `entry["wii_addr"]` was `None` when `p1` was not a map symbol start (`wii_index`
lookup miss), causing 65 entries to export `wii_addr: null`.

**Fix:** Changed to `"wii_addr": hex(p1)` unconditionally — always emits the `p1_addr` from
ghidriff, whether or not the Wii map has a symbol there.

**File:line:** Entry construction in `vet()`, second pass, at line ~384.

### Selftest updates

The selftest was updated to match all four fixes:
- `rb3wii_index` now uses synthetic Bank 5 addresses (`0x90100000`, `0x90999999`), with a
  matching `bank5_syms` dict, to test the name-bridge logic properly.
- Added explicit test for `wii_addr` always-emitted (p1 not in wii_index).
- Added tests that `ExactMnemonicsFunctionHasher` defaults to CAUTION and promotes via
  `--accept-types`.
- All `vet()` calls pass `bank5_syms`.
- Check count: 13 → 17.

---

## Validation Numbers vs Verifier Predictions

Run: `python3 tools/ghidra/vet_xenon_identities.py --matches build/.../run2-baseline-archive/...matches.json --out /tmp/vetted_identities_fixed.json`

### rb3wii cross-check (verifier §6 predictions)

| Metric | Verifier predicted | Measured |
|---|---|---|
| Flips from contradicted → confirmed | 28 | **28** ✓ |
| Remaining contradicted | 609 | **453 + 156 absent** = 609 ✓ |
| Total with rb3wii data | 637 | **637** ✓ |

Note: the 609 "remaining" split as 453 truly-contradicted + 156 newly-absent (Bank5 ELF has no T
symbol at the rb3wii wii_addr — these are low-confidence rb3wii entries pointing at non-function
addresses). The verifier's arithmetic holds: 637 − 28 = 609.

### Category counts (verifier §7 predictions)

| Category | Verifier predicted | Measured |
|---|---|---|
| band3 | 456 | **456** ✓ |
| system | 1,931 | **1,931** ✓ |
| network | 149 | **149** ✓ |
| main | 31 | **31** ✓ |
| sdk | 13 | **13** ✓ |
| unresolved | 65 | **65** ✓ |

All six category counts match exactly.

### ExactMnemonics removal (verifier §8)

4 entries moved from ACCEPT → CAUTION. ACCEPT: 1268 → **1264**.

### wii_addr null elimination (verifier §9)

65 → **0** null wii_addr entries.

### Tier summary (fixed output)

| Tier | Before fix | After fix |
|---|---|---|
| ACCEPT | 1,268 | **1,264** (−4 ExactMnemonics) |
| FILTERED_VT | 280 | **280** (unchanged) |
| CAUTION | 442 | **446** (+4 ExactMnemonics) |
| REJECT | 655 | **655** (unchanged) |

### Selftest

`SELFTEST: all 17 checks passed.`

---

## Caveats

- The 156 entries newly-absent (were contradicted) reflect genuine Bank5 ELF gaps: the Bank5
  debug build has 20,444 T symbols but rb3wii entries can point to addresses in .data, .bss, or
  SDK code not in the ELF's text section. These entries remain unevaluatable for rb3wii and are
  correctly labeled "absent". They were previously (wrongly) labeled "contradicted".
- The output was validated against the **archived** baseline (`run2-baseline-archive/`) as
  required. The live run3 experiment writes to `json/` and is untouched.
- The prior impl doc's per-category table (§"Game-code" section) is now wrong; the correct
  breakdown is in this doc and in the fixed tool's output.
- The §4 calibration (band3 new_coverage tier breakdown) is unaffected — the verifier confirmed
  it used eval_report.json categories, not the broken vet tool's category field. The calibration
  numbers from task-T5-impl.md remain valid.

---

## For the next agent

- The tool is now correct for all four refuted defects. The tiering core was always sound.
- When run3 finishes writing to `json/`, run the tool without `--matches` to generate a fresh
  `vetted_identities.json` from the live run. Expected: same tier protocol; rb3wii cross-check
  will produce different confirmed/contradicted counts if the run3 matches differ from run2.
- The `--accept-types ExactMnemonicsFunctionHasher` flag is available to promote those 4 entries
  if/when precision is measured and found acceptable.
- T6 (if planned): the "contradicted" tier at conf≥0.95 is now 20 entries (down from 53 because
  20 of the 53 were true name agreements). These 20 high-confidence contradictions are the
  genuine cross-pipeline disagreements worth investigating.
- Selftest: `python3 tools/ghidra/vet_xenon_identities.py --selftest` → 17 checks passed.
- Live run: `python3 tools/ghidra/vet_xenon_identities.py` (auto-discovers newest matches.json).
