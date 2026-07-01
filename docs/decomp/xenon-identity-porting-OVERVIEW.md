# Wii→Xenon identity porting — overview

**What this is.** A durable reference for the cross-compiler function-identity recovery
effort that ports symbol identities from the **RB3 Wii debug build** (Bank 8,
MWCC/Gekko, fully symbolized via the CodeWarrior map) onto the **RB3 Xbox 360 build**
(MSVC/Xenon, stripped, ~65k functions). The Xbox 360 build is what the
`rb3-xenon` decomp effort matches against, and it has no symbols of its own — so any
function we can confidently name from the Wii side is a name the Xenon port would
otherwise have to recover by hand.

This effort ran as four ghidriff/BSim experiment rounds (2026-06-10 → 06-11) plus an
ingest into `rb3-xenon`. **This document consumes the existing artifacts; it does not
re-run the pipeline.** It is written to be self-contained — a newcomer or the
`rb3-xenon` team can act on it without reading the round docs, though those are linked
for depth.

Deeper records (read in this order for the full arc):
- Session record / runs 1–4 + ROUND 2 digest: [`xenon-precision-hardening-2026-06-10.md`](xenon-precision-hardening-2026-06-10.md)
- Round-1 plan + rejected levers: [`xenon-hardening/PLAN.md`](xenon-hardening/PLAN.md), synthesis [`xenon-hardening/SYNTHESIS.md`](xenon-hardening/SYNTHESIS.md)
- Round-2 (human judging + ingest + VT refutation): [`xenon-hardening/round2/SYNTHESIS.md`](xenon-hardening/round2/SYNTHESIS.md), plan [`xenon-hardening/round2/PLAN.md`](xenon-hardening/round2/PLAN.md)

---

## 1. What we did

**The pipeline.** We forked [ghidriff](https://github.com/clearbluejar/ghidriff) (a
Ghidra-based binary-diffing tool) and Ghidra's BSim subsystem to diff the two RB3
binaries. The Wii Bank-8 DOL is transcoded into a symbolized Gekko ELF; the Xenon XEX
is exported as a Ghidra `.gzf` (`build/SZBE69_B8/ghidra/ghidriff-xenon/rb3_xenon_default_xex.gzf`,
12.2-format — opens only under the fork dist). ghidriff runs a cascade of correlators,
each proposing function↔function pairs: exact-instruction/switch-signature/symbol
hashers, an **Implied-Match** transitive closure, **VTCombinedReference** (matches by
shared vtable/reference graph), and **BSim** top-K nearest-neighbor over decompiled
feature vectors. We seed the cascade with known-true pairs harvested from prior
identity work (`unified_id_rb3wii.json`, re-resolved to Bank-8 addresses) and reserve a
held-out set of known pairs to measure recall and precision blind.

**The hardening, runs 1→4.** Run 1 measured catastrophic precision (judged **0.440**
overall, VT 0.324, the string hashers 0.000) and a 2h42m `decomp_correlate` stage. We
then (T1) added a global 1:1-uniqueness gate to the string-reference hashers, (T2)
exported per-match similarity/confidence scores as a side-channel and added an STL/
template-internal exclusion gate to VT, (T3) deployed BSim with a top-K cap + parallel
aggregation and disabled `decomp_correlate`, and (T4) cleaned the eval oracle
(platform-alias crediting for Wii↔Xbox class twins, per-category stratification) and
re-seeded from re-resolved Bank-8 addresses. **Run 3** was the first run with everything
live: BSim ON, gated strings, scored VT, 1,213 seeds — matching dropped to ~8 min and
holdout recall tripled to 63.8%. **Run 4** tested whether ACCEPT-only re-seeding could
rescue VT (it could not — see §2) and added a `--matches-only` fast path (~8 min vs
~115 min). The key discriminative signal that emerged is BSim **similarity × confidence
(`simconf`)**, not similarity alone (flat ~0.15 even at 0.99) and not VT.

---

## 2. What it's worth (measured, honest)

**Precision of the ACCEPT tier.** The operating point is BSim `simconf ≥ 15` plus the
exact/switch/implied/symbol hashers. On the blind holdout that reads **0.933**
(n=45). On a 30-pair human-judged stratified band3 sample (round 2) it reads
**0.900 overall / 0.905 BSim** (n=21 BSim / n=9 non-BSim) — consistent with the holdout
within CI, the first human-grade confirmation. Practical reading: **roughly 1 in 10
band3 ACCEPTs is wrong**; consumers must treat these as probabilistic, not ground truth.

**The dominant failure mode is same-TU sibling aliasing** (~10%): two near-identical
template/sibling bodies in the same translation unit that differ only in a type-tag
immediate or an STL node-size literal, or a hash-shape match that strings later refute.
BSim reports similarity 1.0 on bodies separated by a single immediate and picks blind.
All 3 wrong pairs in the judged sample were this. A cheap immediate/literal-diff vet
check on small same-TU bodies would catch it (not yet built).

**Yield — the headline.** 978 ACCEPT identities were ingested into `rb3-xenon`
(`ghidriff_identities.json`). Joined against `rb3-xenon`'s **production** pairing set
(`scripts/target_symbol_map.json`, the file objdiff actually pairs against), measured
2026-06-23:

| Slice | Count |
|---|---|
| Ingested ACCEPT identities | 978 |
| Already in production map | 216 |
| **Net-new (not in production map)** | **762** |
| — of which **band3** (RB3 game code) | **232**, across **93 TUs** |
| — system | 311 |
| — network | 205 |
| — main / uncategorized | 14 |

(The brief's earlier snapshot read 764 net-new / 232 band3; the production map has since
absorbed 2 more of our identities — the band3 figure is unchanged.)

**The 232 band3 identities are the irreplaceable core.** band3 is RB3-specific *game*
code — gameplay, scoring, song handling, tracker — that **DC3 (Dance Central 3, no Rock
Band gameplay) fundamentally cannot provide**. All 232 sit in TUs the active port has
**not yet reached**. Top TUs: GemPlayer.o (19), TrackerManager.o (10), Stats.o (8),
Player.o (7), VocalPart.o (7), Game.o (7), MusicLibrary.o (7), VocalTrack.o (7),
Track.o (6), SongRecord.o (5). Certainty split: **20 high** (ExactInstructions /
SwitchSig / Implied — the most reliable feeders), **27** BSim simconf ≥ 30, **92** in
20–30, **93** in 15–20.

**The ~530 net-new system/network identities are now MEASURED at human grade (round 3).**
A 30-pair stratified sample (15 system + 15 network, across all four confidence strata)
was human-judged 2026-06-23: **overall precision 0.967 (29/30)** — *higher* than band3's
0.900. By category: **system 0.933 (14/15), network 1.000 (15/15)**. By stratum:
**high 3/3, BSim ≥ 30 8/8, BSim 20–30 10/10, BSim 15–20 8/9 (0.889)** — every error is
in the weakest BSim 15–20 band. The single miss (pair-15) is the same same-TU sibling-
aliasing failure mode round-2 saw: BSim collapsed two 20-byte `mImp->virtual()` thunks
(`TrackWidget::Init` slot 0x44 vs `TrackWidget::Empty` slot 0xc) that differ only in the
vtable-slot immediate. The HIGH + BSim ≥ 30 slice was **11/11 = 1.000**. Bottom line:
system/network ACCEPTs are now validated as **safe to hand off** (above the 0.85 bar);
the BSim 15–20 tail carries the residual sibling-aliasing risk and should be confirmed
per-fn when consumed. Full judging: `xenon-hardening/round3/` (evidence packs +
per-pair judgments).

**The DC3 same-ISA path is EXHAUSTED — do not pursue it.** DC3 and Xenon are both
PowerPC/MSVC, so BinDiff matches them directly with high confidence. But `rb3-xenon`
has *already harvested DC3 into its production map*: of the **6,609** strict
high-confidence DC3↔Xenon BinDiff matches (`tools/bindiff_match.json`,
similarity ≥ 0.99 ∧ confidence ≥ 0.98), **6,606 are already in `target_symbol_map.json`
— only 3 net-new** (verified 2026-06-23, reproduces the prior measurement exactly).
A prior scout mis-called DC3 a "5× opportunity" by reading a seed-builder's name-filter
rather than the production map; **that was wrong** — at the confidence level `rb3-xenon`
trusts for injection, DC3 is mined out. (The looser conf ≥ 0.90 DC3 set has ~1,250
addrs not in the map, but those are exactly the lower-confidence matches the production
map deliberately excludes — that is an operating-point choice, not untapped value.) The
Wii→Xenon lever's value is precisely that it reaches the **RB3-only band3 code DC3 has
no counterpart for**.

**VT is a dead lever.** VTCombinedReference was the correlator the plan expected to be
the workhorse. It collapsed to 0.109 raw / 0.236 alias-credited judged precision, and
the round-2 two-pass experiment (re-seeding from only the clean ACCEPT tier) **refuted**
seed contamination as the cause: VT moved to 0.222 (marginally *worse*). VT's weakness
is intrinsic to the MWCC→MSVC reference graph. **VTCombinedReference is permanently
demoted to CAUTION-tier feeder; do not invest in it as an ACCEPT source.**

---

## 3. How to leverage this

**(a) The band3 worklist — the primary deliverable for the active port.** The 232
net-new band3 identities are a **targeting + porting worklist and identity oracle**, NOT
a `target_symbol_map.json` injection. The TUs they live in are not yet compiled in the
Xenon port, so there is no MSVC mangled symbol to pair against yet — the value is "port
*these* TUs next, and when you do, here is the Wii name (and Bank-8 address, for
`bin/analyze-function` on the real body) for each function in them." It dovetails with
the active "class-A TU-pure port-then-pin" methodology: it tells the porter which
not-yet-reached band3 TUs have the most recoverable identities (GemPlayer.o, etc.) so
they can prioritize. Regenerate it per §4.

**(b) `fn_resolver` T4b as a live query oracle.** All 978 identities are already wired
into `rb3-xenon/tools/fn_resolver.py` as tier **`ghidriff_wii_b8`** (T4b, between
`fuzzy_pairs` and `bindiff_dc3`; confidence 0.94 for ExactInstr/Implied, 0.93 for
BSim ≥ 15, 0.90 for SwitchSig). Query an Xenon address through `fn_resolver` and it will
surface the ghidriff candidate at the right confidence. This is the supported way for
the port to *consult* the identities without trusting them blindly.

**(c) Re-running the lever buys only marginal recall.** Each ghidriff run nets ~1,000
new vetted ACCEPTs, but recall is already at 63.8% of the holdout and the BSim residual
pool shrinks each run. The cheap wins (`--matches-only`, the score gates, the BSim
top-K cap) are landed. Re-running is worth it mainly to (i) refresh from a newer Wii or
Xenon program, or (ii) after building the sibling-aliasing vet check, to re-vet the
existing CAUTION pool. Do not expect a step-change.

**(e) A system/network worklist is now warranted (round-3 measurement clears the bar).**
With the 30-pair judging at 0.967 overall (1.000 on HIGH + BSim ≥ 30), the ~530 net-new
system(311)/network(205) identities are safe to surface as a second additive worklist —
same form as the band3 one (gitignored JSON feed + tracked TU-ranked markdown), NOT a
`target_symbol_map.json` injection. Unlike band3 (DC3 cannot provide it), much of
system/network is shared engine + Quazal netcode where DC3 also helps, so this worklist
is **second-priority** behind band3. Recommended confidence cut: surface all four strata
but flag BSim 15–20 as "confirm-on-consume" (that band is where the lone miss sat, 8/9).
The HIGH + BSim ≥ 30 slice (system 22 high + 57 ≥30, network 32 ≥30) is the safe core to
hand off first. The same generator (`gen_band3_port_worklist.py`) parameterizes to
`category in {system,network}` trivially.

**(f) The upstream ghidriff/Ghidra PRs are separate, standalone value.** The forks carry
a string-hasher 1:1 gate, a per-match score side-channel, a VT STL-exclusion gate, a
BSim top-K cap + parallel aggregation, a `--matches-only` early exit, and an O(n×m)
dedup hash-join fix — all generally useful to anyone diffing large stripped binaries.
These are worth upstreaming regardless of the RB3 identity work.

**What NOT to do:**
- **Do not pursue DC3 for net-new identities** — exhausted (§2; 3 net-new at strict conf).
- **Do not treat VT as an ACCEPT source** — permanently CAUTION (§2).
- **Do not auto-inject these names into `target_symbol_map.json`.** At ~0.90 precision a
  wrong name there *mis-pairs* objdiff and is actively harmful (it inflates or corrupts
  match%). The map is for *proven* MSVC symbols of *compiled* TUs. Surface the worklist
  additively; let the porter confirm each name when the TU is actually ported.
- **System/network ACCEPTs are now judged (round 3, 0.967 overall / 1.000 on HIGH+BSim≥30).**
  A system/network worklist on the same model as the band3 one is warranted (§3e). The one
  residual risk is the BSim 15–20 tail (sibling aliasing) — confirm those per-fn on consume.
- **Exclude the 158-entry holdout** before feeding `ghidriff_identities.json` into any
  future seed builder (85 of the 978 overlap the holdout; harmless for objdiff/resolver
  use, leaks recall if seeded).

---

## 4. Reproduce

All numbers in §2 are auditable from `rb3-xenon`'s own checked-in files. Address
normalize for every join: `lower()`, strip `0x`, `lstrip("0")`, `zfill(8)`.

**Net-new set + per-TU band3 worklist** (`ghidriff_identities.json` minus
`target_symbol_map.json`):

```bash
cd /home/free/code/milohax/rb3-xenon
python3 - <<'PY'
import json
from collections import Counter
def norm(a):
    a=str(a).lower()
    if a.startswith("0x"): a=a[2:]
    return a.lstrip("0").zfill(8)
ident = json.load(open("ghidriff_identities.json"))            # our 978 ACCEPTs
maps  = {norm(k) for k in json.load(open("scripts/target_symbol_map.json"))}  # production pairing set
netnew = [e for e in ident if norm(e["rb3_addr"]) not in maps]
print("ingested:", len(ident), "| in map:", len(ident)-len(netnew), "| NET-NEW:", len(netnew))
print("by category:", dict(Counter(e.get("category") for e in netnew)))
band3 = [e for e in netnew if e.get("category")=="band3"]
print("band3 net-new:", len(band3), "across", len({e["tu"] for e in band3}), "TUs")
print("top TUs:", Counter(e["tu"] for e in band3).most_common(10))
# the worklist itself, one row per function:
for e in sorted(band3, key=lambda e:(e["tu"], e["rb3_addr"])):
    print(f'{e["tu"]:>24}  rb3={e["rb3_addr"]}  wii_bank8={e["wii_addr_bank8"]}  '
          f'simconf={e.get("bsim_simconf")}  {e["wii_symbol_demangled"]}')
PY
```

Expected: `ingested: 978 | in map: 216 | NET-NEW: 762`, band3 `232 across 93 TUs`.
(The in-map / net-new split drifts by a few as the production map grows; band3 = 232 is
stable because no band3 TU has been ported yet.)

**DC3-exhaustion check** (strict high-confidence DC3 BinDiff vs production map):

```bash
cd /home/free/code/milohax/rb3-xenon
python3 - <<'PY'
import json
def norm(a):
    a=str(a).lower()
    if a.startswith("0x"): a=a[2:]
    return a.lstrip("0").zfill(8)
bm   = json.load(open("tools/bindiff_match.json"))
maps = {norm(k) for k in json.load(open("scripts/target_symbol_map.json"))}
strict = {norm(m["rb3_addr"]) for m in bm if m.get("similarity",0)>=0.99 and m.get("confidence",0)>=0.98}
inmap  = sum(1 for a in strict if a in maps)
print(f"strict DC3 high-conf: {len(strict)} | in map: {inmap} | NET-NEW: {len(strict)-inmap}")
PY
```

Expected: `strict DC3 high-conf: 6609 | in map: 6606 | NET-NEW: 3`.

**Per-function Wii ground truth** for any worklist row (from the `rb3` repo, the
Bank-8-accurate body):

```bash
cd /home/free/code/milohax/rb3
bin/analyze-function <wii_symbol>          # e.g. Hit__9GemPlayerF...
# CW map cross-check: orig/SZBE69_B8/files/band_r_wii.map (Bank 8), asm under build/SZBE69_B8/asm/
```

**Regenerating the ingest** (only if refreshing from a new run; reads the immutable
run-3 archive, never the mutable live dir): `rb3/tools/ghidra/ingest_ghidriff_accepts.py`
(see round-2 synthesis §c for the gate logic).

---

## For the next agent

- The **232 band3 net-new** identities (§4) are the actionable artifact: a per-TU
  porting worklist for the active class-A TU-pure effort, prioritized by GemPlayer.o /
  TrackerManager.o / Stats.o / Player.o / VocalPart.o / Game.o / MusicLibrary.o /
  VocalTrack.o. These are RB3-only game code DC3 cannot supply.
- **Two un-closed validation gaps:** (1) no human-judged sample for the ~530
  system/network ACCEPTs — they ride on the holdout 0.933 alone; (2) no sibling-aliasing
  vet check yet (the ~10% failure mode; a cheap immediate/literal-diff on small same-TU
  bodies would catch it). Both are the highest-value follow-ups.
- **Settled negatives — do not re-open:** DC3 same-ISA harvest (3 net-new at strict
  conf), VT as an ACCEPT source (permanently CAUTION), auto-injecting unproven names
  into `target_symbol_map.json` (harmful at ~0.90 precision).
- Re-running ghidriff buys marginal recall only; the durable wins are the upstream
  ghidriff/Ghidra PRs (§3d) and the fn_resolver T4b oracle (already live).
