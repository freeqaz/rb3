# T5 Verification: vet_xenon_identities.py (vetted-identity export)

**Verifier:** Fable (adversarial), 2026-06-10
**Claim under test:** `docs/decomp/xenon-hardening/task-T5-impl.md`, commit rb3 `d17d5e55`
**Verdict: PARTIAL** — tool exists, runs offline, selftests, and reproduces every calibration
number exactly; but two load-bearing parts of the deliverable are broken: (1) the rb3wii
cross-check compares **Bank 8 addresses against Bank 5 addresses** so its `confirmed/contradicted`
labels are semantically meaningless (proven: real name-level agreements are labeled
"contradicted"), and (2) **79% of entries are mislabeled `band3`** (the category helper is a
regression of the eval's, despite the impl doc claiming it is *more* accurate).

---

## What I checked and how (all offline)

### 1. Commit + hygiene — CONFIRMED
- `git -C rb3 show d17d5e55 --stat`: exactly 2 files (`tools/ghidra/vet_xenon_identities.py` 744
  lines NEW, `docs/decomp/xenon-hardening/task-T5-impl.md` NEW). Focused, no unrelated staging.

### 2. Selftest — CONFIRMED
`python3 tools/ghidra/vet_xenon_identities.py --selftest` → `SELFTEST: all 13 checks passed.`
Read the fixture (vet_xenon_identities.py:585-740): it genuinely exercises every tier rule
(SeedMatch/ExactInstr→ACCEPT, coherent 3-member VT cluster→FILTERED_VT, scattered/singleton
VT→CAUTION, SRH/StrUnique→REJECT, rb3wii confirmed/contradicted/absent, `--accept-types`
override, `min_vt_score` low→REJECT / high→pass). The synthetic fixture tests the same `vet()`
path the CLI uses — not a parallel code path.

### 3. Reproducibility — CONFIRMED
Re-ran the tool to `/tmp/t5_revet.json`. Summary identical to the committed artifact and the
impl doc: ACCEPT 1,268 / FILTERED_VT 280 / CAUTION 442 / REJECT 655; rb3wii confirmed 0 /
contradicted 637 / absent 2,008. Tier arithmetic is internally consistent with the matches.json
type histogram (every match has exactly one type: 1186 Seed + 1 SymbolsHash + 63 ExactInstr +
4 ExactMnem + 3 SwitchSig + 11 Implied = 1,268 ACCEPT; 610+45 = 655 REJECT; 722 VT = 280+442).
No duplicate p2 in the VT pool (so the `vt_tier_map` keyed-by-p2 design is safe on this data).

### 4. Calibration vs scout 4 §5 — CONFIRMED (exactly)
Using eval_report.json's `wii_category=='band3'` new_coverage set (325 addrs):
tier breakdown `{ACCEPT:16, FILTERED_VT:55, CAUTION:74, REJECT:180}`; ACCEPT decomposes to
12 ExactInstructions + 4 Implied/SwitchSig (0 ExactMnemonics-only); REJECT = 174 SRH + 6
StrUnique. Matches scout (12/56/73/174/6/4) within the allowed ±1 drift.
- `AccomplishmentManager.o`: 6 non-seed VT entries, xenon_spread 2,782,784 B → **all CAUTION** ✓
- `BandScreen.o`: 4 non-seed VT entries, xenon_spread 936 B → **all FILTERED_VT** ✓
- 45 band3 new_coverage VT entries present in rb3wii, all labeled `contradicted` ✓ (label
  reproduced — but see §6: the label itself is unsound).
- Impl-doc confidence-slice figure "38 ACCEPT-tier contradictions at conf≥0.95" — reproduced
  (53 contradicted at conf≥0.95, 38 of them ACCEPT-tier).

Note: this calibration is valid **despite** defect §7 because it filters by the EVAL's category
field, not the vet tool's own broken one.

### 5. CLI overridability — CONFIRMED
`--tier-config` (JSON merge), `--accept-types` (union into accept set), `--min-vt-score`
(reads PLAN §3 `scores.VTCombinedReference.product`; no-op on the pre-T2 artifact since the
field is absent — verified the gate path in code at :285-290 and :409-414 and via selftest).
Post-T2 reuse without code change is plausible as claimed.

---

## 6. REFUTED — the rb3wii cross-check is cross-address-space garbage (load-bearing)

`unified_id_rb3wii.json`'s `wii_addr` values are **Bank 5** (debug-ELF) addresses; ghidriff's
`p1_addr` are **Bank 8** (target map) addresses. The tool compares them directly
(vet_xenon_identities.py:421-431) and so can essentially never produce `confirmed`.

Evidence:
- Only **574/9,301** rb3wii `wii_addr` values are function starts in the Bank 8 map at all
  (36/392 at conf≥0.95).
- Decisive single example: ghidriff matched xenon `0x8269d338` → Wii `0x801d6120`
  `__ct__7ShuttleFv` (Bank 8 map line 9521). rb3wii says the same xenon addr is
  `Shuttle::Shuttle()` at `0x801ee270`. `nm` on the Bank 5 ELF
  (`milo-executable-library/rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.elf`):
  `801ee270 T __ct__7ShuttleFv`. **Both pipelines agree on the identity**; the tool reports
  `contradicted`.
- At scale (joining rb3wii `wii_addr` → Bank 5 ELF symbol → exact mangled-name compare with
  ghidriff's Bank 8 `wii_symbol`): **28 of the 637 "contradicted" entries are exact name-level
  AGREEMENTS** (i.e. true confirmations mislabeled), including **20 of the 53** at rb3wii
  conf≥0.95 (38%). Within the headline "45 band3 VT entries all contradicted": 2 are name
  agreements (`Hit__8GemTrackFfii` FILTERED_VT, `SendResumeNoScoreGameNetMsg__9GamePanelFf`
  CAUTION) — so the scout-4 finding this reproduces ("all disagree → confirms VT FP rate") is
  itself tainted, and the impl doc's "Notable finding … now verified at scale" is overstated.
- The impl doc's rationalization of `confirmed = 0` ("different algorithm paths → different Wii
  addresses … expected, not a sign of a bug") is **wrong**: it is a fixed address-space offset
  (different bank/build), not pipeline disagreement.

Fairness note: the address-based join was specified by the BRIEF and scout 4 (the implementer
inherited the defect). But the implementer observed the 0/637 confirmed/contradicted split —
a screaming anomaly — and rationalized instead of investigating. As shipped, the
`rb3wii_check` field is **anti-informative** (true agreements labeled `contradicted`) and any
consumer acting on it would discard correct identities.

Cheap fix (stays T4-independent — exact mangled-name join, no normalization): translate
rb3wii `wii_addr` through the Bank 5 ELF symbol table to a mangled name, compare to the Bank 8
map symbol at `p1` (or translate to the Bank 8 address via the map name index). My replay
above is exactly this and ran in seconds.

## 7. REFUTED — category attribution: 2,080/2,645 entries (79%) mislabeled `band3`

`_categorize_full` (vet_xenon_identities.py:173-194) checks path prefixes `system_wii\` /
`network_wii\` which occur **0 times** in `band_r_wii.map` (grep), and returns `band3` for
ANYTHING containing `band3_wii\` — but `C:\hproj\band3_wii\` is the **project root** for ALL
game/engine/network source. True distribution (recomputed with the eval's `categorize_tu`,
eval_xenon_matches.py:227-253, which correctly extracts the `<module>` path component):
**456 band3 / 1,931 system / 149 network / 31 main / 13 sdk / 65 unresolved** — vs the tool's
2,536 band3. Consequences:
- The summary's `per_category` block and the impl doc's game-code table are wrong (e.g.
  "REJECT band3 = 655" — really spread across system/network/band3; "FILTERED_VT = 280 (all
  band3)" — really mostly system).
- The impl doc's design-decision claim ("the eval's version … loses network/system; T5 uses a
  more complete version") is **exactly backwards** — the inlined copy is a regression of the
  helper it claims to improve.
- Does NOT affect: tier assignment (TU clustering uses the TU **basename**, which is computed
  correctly), the §4 calibration (used eval categories), or the rb3wii join.

## 8. Deviation — ExactMnemonicsFunctionHasher silently added to default ACCEPT

The brief's ACCEPT list: SeedMatch, ExactInstructions, SymbolsHash, Implied, SwitchSig.
The default config (:61-68) adds `ExactMnemonicsFunctionHasher` — an UNMEASURED type
cross-compiler (absent from eval_report's `precision_by_match_type`), contradicting the brief's
own rule ("any future unmeasured type … REJECT unless allowlisted via --accept-types") and the
script's OWN docstring (:16 says ExactMnemonics → CAUTION unless in accept_types). Magnitude:
4 matches promoted to ACCEPT. Small, but it quietly dilutes the tier whose whole point is
"measured-high-precision only".

## 9. Minor defects
- **65 entries export `wii_addr: null`** (incl. 2 ACCEPT-tier ExactInstructions matches) when
  `p1` is not a map symbol start (:334) — the identity datum the export exists for is dropped
  even though ghidriff's `p1_addr` is known. Emit `hex(p1)` unconditionally.
- Impl doc confidence-slice numbers partially wrong: "512 have rb3wii_confidence < 0.90" →
  measured 570; "219 have confidence < 0.80" → measured 550. (The "38 ACCEPT-tier @ ≥0.95"
  figure IS correct.) Not load-bearing.
- `find_matches_json` multi-candidate comment says "most-recently-modified" and the code does
  that correctly; fine.

---

## Verdict

**PARTIAL.** Confirmed: the script exists (commit d17d5e55, clean), runs offline against the
real artifacts, the selftest passes and exercises the real code path, the tier protocol matches
the spec thresholds, and every required calibration number reproduces exactly (12/4/55/74/174/6;
AccMgr→CAUTION @2.78MB; BandScreen→FILTERED_VT @936B; 45 band3 VT in rb3wii all labeled
contradicted; CLI overrides work). Refuted: (a) the rb3wii cross-check — a headline feature —
compares Bank 8 vs Bank 5 address spaces, making `confirmed/contradicted` labels meaningless
and actively misleading (28/637 "contradicted" incl. 20/53 high-conf are exact name-level
agreements; proven via Bank 5 ELF nm join); the impl doc's "expected, not a bug" explanation is
false; (b) the category field mislabels 79% of entries as band3, and the impl doc claims the
opposite of the truth about its accuracy. The ACCEPT-tier list also deviates from the brief
(unmeasured ExactMnemonics included, 4 matches). The tiering core is sound; the annotations a
consumer would use for game-code triage are not.

## For the next agent
- Read: this doc; `task-T5-impl.md`; the tool `tools/ghidra/vet_xenon_identities.py`.
- Fix 1 (small, high value): rb3wii cross-check must bridge Bank5→Bank8. Exact join recipe
  that needs no T4 normalization: `nm` the Bank 5 ELF (path in CLAUDE.md / §6 above) →
  `{addr: mangled}`; `confirmed` iff Bank5-mangled(rb3wii.wii_addr) == Bank8-map-symbol(p1).
  My replay measured: 637 current "contradicted" → 28 confirmed / 609 contradicted under the
  fixed rule (20/53 flip at conf≥0.95).
- Fix 2 (one-liner-ish): replace `_categorize_full` with the eval's `categorize_tu` module
  extraction (`rest.split("\\",1)[0]` after `band3_wii\`); delete the dead
  `system_wii\`/`network_wii\` branches. Then regenerate `vetted_identities.json` and re-state
  the per-category table in the impl doc.
- Fix 3: drop `ExactMnemonicsFunctionHasher` from default accept (or measure it first);
  emit `wii_addr` from `p1` unconditionally.
- The §4 calibration does NOT need redoing after these fixes (it never used the broken fields).
- Re-verify commands are embedded in each section above; everything ran in seconds, offline.
