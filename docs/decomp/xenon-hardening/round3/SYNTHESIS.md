# Round 3 — SYNTHESIS + adversarial verification (2026-06-23)

Final synthesis pass (Opus; Fable was unavailable). Round 3 **consumed** the validated
Wii→Xenon identity lever (no ghidriff run). Two deliverables: (#1) the band3 porting
worklist, (#2) the first human-grade measurement of the system/network identities. This
doc adversarially verifies #1 and judges #2, then records the handoff decision.

Read first for context: `../../xenon-identity-porting-OVERVIEW.md` (durable reference),
`../../xenon-precision-hardening-2026-06-10.md` (session record; see its `## ROUND 3
RESULTS`), `recon-consumption.md` (the conventions + recipe both tasks consumed),
`task-band3-worklist-impl.md` (#1 impl), `evidence-systemnetwork.md` (#2 evidence build).

---

## VERDICT TABLE

| Claim | Status | Evidence |
|---|---|---|
| #1 worklist commits exist + additive | **CONFIRMED** | rb3-xenon `f064c2d` (4 files, +1362, map/build untouched); rb3 `854489dc` |
| #1 232 set genuinely net-new | **CONFIRMED** | band3 ∩ NOT target_symbol_map = 232; 0 already in map |
| #1 genuinely band3 | **CONFIRMED** | all 232 source category = band3 |
| #1 confidence labels match rule | **CONFIRMED** | 0/232 mismatches vs re-derived certainty |
| #1 nothing injected into map | **CONFIRMED** | `git show --name-only f064c2d` omits target_symbol_map.json |
| #1 demangled signatures sane | **CONFIRMED** | 20 HIGH vetted; 232/232 CW-map VA resolve; 93/93 src_path exist |
| #2 sys/net precision measured | **0.967 overall (29/30)** | system 0.933, network 1.000 |
| #2 handoff bar | **≥ 0.85 → hand off a worklist** | HIGH+BSim≥30 = 11/11 = 1.000 |
| #2 lone wrong = sibling aliasing | **CONFIRMED** (pair-15) | vtable-slot 0x44 (Init) vs 0xc (Empty), re-verified from Wii asm |

---

## 1. ADVERSARIAL VERIFICATION OF #1 (band3 worklist) — could not refute

### Commits exist + additive
- **rb3-xenon `f064c2d`** (branch `main` at verify time), `docs(band3-worklist): net-new
  Wii->Xenon band3 porting worklist + identity oracle`. `git show --name-only` →
  exactly: `.gitignore`, `docs/plans/band3-port-worklist.md`, `tools/demangle_cw.py`,
  `tools/gen_band3_port_worklist.py` (4 files, +1362 lines). **`target_symbol_map.json`
  is NOT among them** — the map's last-touching commits are unrelated porter work
  (`9abd8e5`, `142c0c5`, `39dd399`). Additive + reversible confirmed.
- **rb3 `854489dc`** (branch `xenon-round3-recon`, the round-3 workflow branch — NOT
  master), `docs(xenon-round3): band3 porting-worklist impl handoff + forensics
  generator`. Both commits reachable; the StructuredOutput's claimed SHAs/branches are
  accurate.

### Re-derivation (independent, reproduced EXACTLY)
```
total identities 978 | already-in-map 216 | NET-NEW 762
net-new by category: band3 232 · system 311 · network 205 · main 4 · null 10
band3 net-new: 232 across 93 distinct TUs
certainty: high 20 · bsim>=30 27 · bsim20-30 92 · bsim15-20 93
match_types[0]: ExactInstructionsFunctionHasher 18 · SwitchSigHasher 1 · Implied 1 · BSIM 212
```
Matches the MEASURED ground truth and the impl doc to the entry.

### The five adversarial checks (all PASSED)
1. **Genuinely net-new** — re-joined the 232 worklist `rb3_addr`s against the *live*
   `target_symbol_map.json`: **0 already in map**. (The map grew 13,023→13,199 since
   recon via concurrent porters; the 216-in-map figure is against the live map, band3=232
   is invariant.)
2. **Genuinely band3** — every one of the 232 worklist rows traces to a source identity
   with `category == band3` (0 leakage from system/network/main).
3. **Confidence labels match the certainty rule** — re-derived `cert(e)` from
   `match_types` (ExactInstr/SwitchSig/Implied/SymbolsHash → high) + `bsim_simconf`
   bands (≥30 / 20–30 / 15–20) and compared per-row to the stored `confidence_label`:
   **0/232 mismatches.**
4. **Nothing injected into target_symbol_map** — confirmed via the file-list of `f064c2d`
   (above) and the map's git history.
5. **Demangled signatures sane** — the 20 HIGH rows demangle to clean `Class::Method`
   (e.g. `ClosetMgr::ShowClothes()`, `TourSavable::SetDirty(bool,int)`,
   `MusicLibrary const::DifficultySortPart()`). 3 documented fallbacks (`Gem::operator=`,
   a templated pair ctor, an `operator<<`) carry the raw CW name. The two cross-TU-class
   rows the impl flagged (`Stats::GetVocalPartPercentage` under Performer.o,
   `SingerStats::GetRankData` under Stats.o) are correctly handled: `tu` = the Xenon
   porting bucket (confirmed against the CW obj-path column), class name = the Wii identity.

### Strongest correctness anchor (the impl's VERIFY pass, independently reproduced)
**All 232 `wii_symbol`s resolve in the CW map (`orig/SZBE69_B8/files/band_r_wii.map`) to
their claimed `wii_addr_bank8` — 232/232, 0 missing, 0 mismatch.** (My first pass scored
0/232 by parsing the wrong map column — the CW map is `sectoff size VA fileoff align
symbol`; the VA is column 2. Re-parsed correctly = 232/232. The impl was right; my parse
was wrong — noted so the next agent does not repeat it.) All 93 derived `src_path`s exist
and are faithful to the CW obj-path column. This means a porter using `bin/analyze-function
<wii_symbol>` lands on the right Bank-8 body for every entry.

**#1 verdict: VERIFIED. I could not refute any claim.** The worklist is a sound additive
targeting/porting oracle, correctly NOT injected into the production map.

---

## 2. JUDGE OF #2 (system/network precision) — measured per stratum

30-pair stratified sample (15 system + 15 network), human-judged. Per-pair verdicts:
29 correct, 1 wrong (pair-15), 0 uncertain. (16 of 30 also have standalone judgment files
under `judgments/`; their verdicts agree with the full inline set.)

### Precision by stratum (counts)
| Stratum | system | network | combined |
|---|---|---|---|
| high (Exact/SwitchSig/Implied) | 3/3 | — (network has 0 high) | **3/3 = 1.000** |
| BSim ≥ 30 | 4/4 | 4/4 | **8/8 = 1.000** |
| BSim 20–30 | 4/4 | 6/6 | **10/10 = 1.000** |
| BSim 15–20 | 3/4 (0.750) | 5/5 | **8/9 = 0.889** |
| **category total** | **14/15 = 0.933** | **15/15 = 1.000** | **29/30 = 0.967** |

- **HIGH + BSim ≥ 30 slice: 11/11 = 1.000** (the safe core).
- The **only** error is in the **weakest BSim 15–20** band, system category.

### The one wrong (pair-15) — sibling aliasing, independently re-verified
Claim: Wii `Init__11TrackWidgetFv` (0x807995c0) == Xenon `0x827bb4f0`. **WRONG.** The
Xenon body forwards through member-vtable **slot 0xc**; Wii `Init` forwards through
**slot 0x44** (`build/SZBE69_B8/asm/system/track/TrackWidget.s`: Init = `lwz r12,0x44(r12)`).
Slot 0xc on Wii is `Empty()` — the sibling thunk `Empty__11TrackWidgetFv` (0x80799710)
uses `lwz r12,0xc(r12)` *verbatim*. BSim (sim×conf 15.142, the lowest stratum) collapsed
two 20-byte `mImp->virtual()` forwarders differing ONLY in the vtable-slot immediate. The
true partner is `TrackWidget::Empty`. This is round-2's exact dominant failure mode, and
the judge identified it correctly. I re-ran the asm grep and confirm: Init=0x44, Empty=0xc.

### Sibling-alias clusters handled correctly
The recon flagged the Quazal `Trace__…ProbeList`/`…Probe`/`…ContactInfo` siblings
(pairs 23/24/25) and the BandList `Conceal`/`Reveal` pair (08/14) as the in-sample alias
risk. The judges did NOT alias them — e.g. pair-23 (StationProbeList::Trace) is pinned by
its single resolved callee `StationProbe::Trace` (the List→element-Trace semantic), and
the adjacent `Update` sibling is explicitly ruled out by call mechanism + arity. All four
of those risk pairs judged correct on real discriminators (resolved callees / immediates),
not shape alone.

### Why higher than band3's 0.900
The strongest signal in sys/net is **resolved-callee agreement** (75% of 92 Xenon callees
resolved to matched Wii symbols), and Quazal netcode is rich in distinctive named callees
(GetCurrentContext, ValidOperation, Trace, FormatString ctors) + source-path strings
(pair-30 references `TransportSignatureGenerator.cpp`). band3 game code has fewer such
anchors, so its sample leaned more on shape and aliased more. The 0.967 is real but n=30;
the BSim 15–20 cell (n=9, one wrong) is the noisy one — treat its true precision as
"~0.85–0.90 with wide CI", not a hard 0.889.

---

## 3. HANDOFF DECISION

Measured **0.967 ≥ 0.85** → **recommend a system/network worklist on the band3 model.**

- **Form:** additive, same as band3 — a gitignored `sysnet_port_worklist.json` feed +
  a tracked TU-ranked `docs/plans/sysnet-port-worklist.md`. **NOT** a
  `target_symbol_map.json` injection (CW≠MSVC mangling; many TUs uncompiled; a wrong key
  mis-pairs objdiff). The same generator parameterizes to `category ∈ {system, network}`
  trivially.
- **Confidence cut:** surface **all four strata**, but flag **BSim 15–20 as
  "confirm-on-consume"** (that band holds the lone miss; sibling-aliasing risk). The
  **HIGH + BSim ≥ 30 core** (system 22 high + 57 ≥30; network 32 ≥30 — network has 0 high)
  is the safe-to-trust slice to hand off first.
- **Priority:** **second** behind band3. band3 is the irreplaceable core (DC3 cannot
  provide RB3 gameplay); much of system/network is shared engine + Quazal netcode where
  DC3 BinDiff also helps, so the marginal value is lower even though precision is higher.

This is **not** the `<0.70 keep-as-fn_resolver-only` outcome, nor the
`0.70–0.85 HIGH/BSim≥30-only` outcome — the full-strata worklist is warranted, with the
weak tail flagged rather than dropped.

---

## RISKS / honest caveats

1. **n=30 sample, BSim 15–20 cell is n=9 with 1 miss.** 0.889 in that cell has a wide CI
   (~0.52–0.98 at 95%). The headline 0.967 is solid; the weak-tail point estimate is not.
   Confirm BSim 15–20 entries per-fn when a porter actually consumes them.
2. **Sibling aliasing is the *only* failure mode and it is unfixed in the pipeline.** It
   bit pair-15 (sys/net) exactly as it bit pairs 13/16/29 (band3). The round-3 priority-3
   "sibling-aliasing vet check" (diff vtable-slot / type-tag / node-size immediates on
   near-identical same-TU bodies) is still NOT built — it would have caught every wrong
   pair across both rounds. Highest-leverage next investment.
3. **`band3_port_worklist.json` is gitignored/regenerable** — if a consumer needs it
   pinned, regenerate via `rb3-xenon/tools/gen_band3_port_worklist.py` (VERIFY pass gates
   it). The generator + markdown are tracked, so this is intentional, not lost work.
4. **Production map drift** (13,023→13,199 since recon) means future net-new re-derivations
   will shrink slightly as porters absorb more identities — band3=232 is stable today but
   re-derive against the live map, do not trust a stale count.
5. **The ~530 sys/net identities are now judged but NOT yet emitted as a worklist** — that
   is the recommended next deliverable (§3), not done in this round.

---

## For the next agent

- **#1 is done and verified.** The band3 worklist (`rb3-xenon/docs/plans/band3-port-worklist.md`
  + `band3_port_worklist.json`) is a sound porting oracle. Port band3 TUs in the ranked
  order; name each fn from `wii_symbol` via `bin/analyze-function` in the rb3 repo. Do
  NOT inject into `target_symbol_map.json` — confirm the MSVC symbol only when the TU is
  actually compiled (`gen_game_target_map.py --tu <TU>`).
- **#2 is measured (0.967) and gates a sys/net worklist.** Build it next: parameterize
  `gen_band3_port_worklist.py` to `category ∈ {system, network}`, emit the same two
  coupled outputs, flag BSim 15–20 confirm-on-consume. ~530 identities (system 311 +
  network 205).
- **Build the sibling-aliasing vet check** (round-3 priority #3, still open) — it is the
  single highest-leverage precision lever: it would have caught all 4 wrong pairs across
  rounds 2–3 (every miss is a near-identical same-TU body differing only in a vtable-slot
  / type-tag / node-size immediate). Run it over the BSim 15–20 tier first.
- **Don't repeat my CW-map-column mistake:** the map is `sectoff size VA fileoff align
  symbol` — the virtual address (what `wii_addr_bank8` should equal) is **column 2**, not
  column 0 (column 0 is the section-relative file offset). Parsing col 0 gives a spurious
  100% mismatch.
- **Settled negatives (do not re-open):** DC3 same-ISA harvest (3 net-new at strict conf —
  mined out), VTCombinedReference as an ACCEPT source (permanently CAUTION), name injection
  into `target_symbol_map.json` (harmful at ~0.90 precision).
