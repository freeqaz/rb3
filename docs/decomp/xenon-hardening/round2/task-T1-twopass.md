# Task T1 — Two-pass VT rescue: THE EXPERIMENT (results)

Agent: opus (T1). Date: 2026-06-11.

**Hypothesis under test:** seeding the whole ghidriff cascade from ONLY the
vetted ACCEPT tier (2,130 pairs ≈ 0.93 precision) — instead of the run-3
1,213-pair seeds that let BSim propagate a ~0.3-precision contaminated graph —
RESCUES VTCombinedReference precision from its run-3 collapse (0.109 raw /
0.236 alias-credited).

**VERDICT: REFUTED.** ACCEPT-only seeding did NOT rescue VT. VT judged
precision came out 0.093 raw / 0.222 alias — slightly *worse* than run 3
(0.109 / 0.236), not better. No score-gated operating point reaches ≥0.85 at
meaningful yield. **VT should be demoted to a CAUTION-tier feeder permanently.**

Implementation + offline verification: see `task-T1-impl.md` (ghidriff commit
`e52d935`, rb3 commit `e1693918`). Seed builder
`tools/ghidra/build_accept_seeds.py` → 2,130 ACCEPT-only pairs
(`seeds_accept_run3.json`), anti-leak (73 holdout drops) + strict 1:1.

## THE RUN

```
RB3_XENON_MATCHES_ONLY=1 \
RB3_XENON_SEEDS=$PWD/build/SZBE69_B8/ghidra/xenon-seeds/seeds_accept_run3.json \
  ./tools/ghidra/run_ghidriff_xenon.sh
```
- Started 2026-06-11 10:27:30 UTC. `SeedMatch: pre-accepted 2130 seed pairs,
  skipped 0 unresolvable` (== the 2,130 seeds; clean resolution).
- BSim ON (default). Exit code 0. Watchdog confirmed: matching done at 7m, no kill.
- **Matching wall-clock ≈ 7.4 min** (seed-load 10:28:13 → matches-json
  10:35:37); total incl. JVM start ≈ 8 min. The `--matches-only` early-exit
  fired (log: `--matches-only: skipping post-match diff/report stage` at
  10:35:37,409) — the ~107-min report stage was skipped. **Total run ~8 min vs
  run-3's ~115 min.**

Per-correlator (this run): seeds 2,130 → BSim **3,964** (run 3: 5,939 — fewer,
the larger seed set pre-accepts more, shrinking BSim's residual pool) →
VTCombinedReference seeded with 6,353 accepted, **9,912 candidates → accepted
1,193** (run 3: 1,093 — VT accepted *more* this round) → Implied 9 new.

### Post-run sanity — ALL PASS
- **matches.json FRESH**: `2026-06-11 10:35:37`, 4.23 MB (run-3 copy was 4.88 MB).
- **full `.ghidriff.json` NOT rewritten**: stale `2026-06-10 20:48:55`, 209 MB
  (old file on disk untouched — checked mtime, not existence).
- **`.md` NOT rewritten**: stale `2026-06-10 20:48:53`.
- **VTCombinedReference: 1,193 entries, ALL carry `scores.VTCombinedReference`**
  (`similarity`/`confidence`/`product`; sample product 805.7).
- **addrs bare hex** (no `0x`): `p1_addr='8000e2e0'`, `p2_addr='82260ec8'`;
  first 200 all bare-hex.

## EVAL (with `--seeds seeds_accept_run3.json`, mandatory)

Eval defaults to the OLD seeds.json (corrupts seed-exclusion/holdout math), so
ALL eval invocations passed `--seeds …/seeds_accept_run3.json`. Report:
`build/SZBE69_B8/ghidra/ghidriff-xenon/eval_report.json` (alias-credited).

```
pairs in output: 7555  (seeds: 2130, seed-conflicts: 1, bad-addr: 0)
scored (non-seed) pairs: 5424  wii-addr resolved: 5375
(a) holdout recovery: recovered 85 correct / 18 wrong / 38 unmatched
     recovery rate 0.603   precision on recovered 0.825
(d) precision proxy by match type (holdout + high-conf bindiff):
     OVERALL  judged 406  prec 0.323 (alias) / 0.291 (raw)
     BSIM            315       0.279        / 0.260
     VTCombinedReference  54   0.222        / 0.093
     ExactInstructionsFunctionHasher 30  0.967
     Implied Match    4        0.500
     StringsRefsHasher 3       0.000
EXPERIMENT-1 bar (>0.32): 0.323 -> PASS (alias)
```

## Results vs run 3

| Metric | Run 3 (1,213 seeds.json) | Round-2 (2,130 ACCEPT seeds) | Δ |
|---|---|---|---|
| Total matches | 8,527 | **7,555** | −972 |
| BSim matches | 5,939 | 3,964 | −1,975 |
| VT accepted | 1,093 | **1,193** | +100 |
| **VT judged precision (raw)** | 0.109 | **0.093** | −0.016 |
| **VT judged precision (alias)** | 0.236 | **0.222** | −0.014 |
| BSim judged precision (alias) | 0.319 | 0.279 | −0.040 |
| Holdout recall | 0.638 | 0.603 | −0.035 |
| Holdout precision | 0.833 | 0.825 | −0.008 |
| Vetted ACCEPT tier | 2,207 | **2,246** | +39 |

The total-match drop is entirely the smaller BSim yield (the bigger seed set
pre-accepts pairs BSim would otherwise have re-discovered). The ACCEPT tier
nonetheless *grew* (+39): seeds all land in ACCEPT, and the seed set grew by
~920. Holdout recall/precision are within noise of run 3 (the meter is stable).

## VT score sweep (judged precision per `scores.product` floor, alias-credited)

The requested `9.5:14:0.5` range is entirely below VT's actual product
distribution (min 20.3 / median 85.4 / max 805.7), so it culls nothing (flat
0.222). Swept across the real range instead:

| product floor | VT judged | correct | wrong | precision | VT culled |
|---|---|---|---|---|---|
| none / ≤20 | 54 | 12 | 42 | 0.222 | 0 |
| 60 | 37 | 12 | 25 | 0.324 | 282 |
| 100 | 9 | 5 | 4 | 0.556 | 719 |
| 140 | 8 | 4 | 4 | 0.500 | 895 |
| 180 | 5 | 2 | 3 | 0.400 | 1015 |
| 220 | 3 | 2 | 1 | 0.667 | 1087 |
| 260 | 1 | 1 | 0 | 1.000 | 1120 |
| ≥300 | 0 | 0 | 0 | n/a | 1146+ |

**No operating point reaches ≥0.85 judged precision at meaningful yield.** The
curve rises with the floor, but the *judged sample collapses to noise* — at
floor 260 the "1.000" is a single judged match out of 1,193. There is no
score threshold that turns VT into a ≥0.85 ACCEPT-tier feeder; the precision
gain is indistinguishable from sample exhaustion. (Per-stratum: band3 VT 0.154,
system VT 0.321, network/sdk VT 0.000 — uniformly weak.)

## Per-type / per-stratum stability

ExactInstructionsFunctionHasher held at 0.967 (30 judged, 29 correct) — the
reliable non-BSim ACCEPT feeder, stratum-independent (band3 0.917, system 1.000).
BSim dropped a touch (0.319→0.279 alias) but stays the workhorse. Network BSim
is strongest (0.596); sdk is 0.000 across every type (never ingest sdk — confirmed
again). band3 remains BSim's weakest stratum (0.179).

## Cross-task interaction (IMPORTANT for reading the holdout numbers)

The live `xenon-seeds/holdout.json` was **grown from 146→158 by the concurrent
T3 task** (12 human-judged band3 ACCEPT identities carrying `wii_addr_bank8`,
`source='judged-round2-correct'`) DURING my run. Those 12 grown entries come
from the same band3 ACCEPT pool my seeds draw from, so **12 of them are in my
2,130-seed set** (a leak T3 introduced, not the seed builder — my anti-leak
filter ran against the original 146 before T3 grew it). The eval handles this
correctly: it reports `eligible: 146 (excluded as seeds: 12)` — it drops the 12
seed-overlapping holdout entries from recall/precision scoring, so the holdout
numbers above (0.603 / 0.825) are computed on the leak-free original-146
eligible set. **No corrective action needed from T1**; flagged for T4's
invariant audit (reserved-for-seeds vs holdout XOR discipline must account for
the band3 ACCEPT pool overlap between my seeds and T3's grown holdout).

## Verdict & recommendation

**Hypothesis REFUTED.** Seeding the cascade from only the vetted ACCEPT tier
(~0.93 precision) does NOT rescue VTCombinedReference. VT precision is
essentially unchanged (marginally worse): 0.093 raw / 0.222 alias vs run-3's
0.109 / 0.236. The run-3 collapse was therefore NOT primarily caused by
BSim-contaminated seed propagation — VT's reference-graph signal is simply too
weak across the MWCC→MSVC compiler gap, even when every seed is high-precision.
The product score does not separate correct from wrong at any usable yield.

**Recommendation: DEMOTE VT to a CAUTION-tier feeder permanently.** Do NOT
promote any VT≥threshold band to ACCEPT. Resolves round-1 Open follow-up #1 in
the negative. Stop investing in `--vt-ref-correlators` as an ACCEPT source; it
remains useful only as cluster-coherence CAUTION signal.

**Round-3 option NOT indicated.** The PLAN's mid-band fallback (if VT landed
0.4–0.6, re-seed from seeds+exacts only ≈1,280 pairs ≈0.99) does NOT apply —
VT landed at 0.22, far below 0.4. Re-seeding from an even purer set would only
shrink the residual pool further; there is no reason to expect it lifts VT when
2,130 ≈0.93 seeds did not. The lever is exhausted.

**Net positive byproducts of this run, independent of the VT verdict:**
1. `--matches-only` works — ~107 min/run saved, making future seed-threshold
   iteration practical (~8 min/run).
2. The ACCEPT-only seed set yields a *larger* vetted ACCEPT tier (2,246 vs
   2,207, +39) at comparable holdout precision — a cleaner identity export
   than run 3, usable by T2's ingest (though T2 reads the immutable
   run3-archive by design).

## For the verifier
- Run dir: `build/SZBE69_B8/ghidra/ghidriff-xenon/`. The live matches.json
  (`json/…matches.json`, mtime 10:35:37) is THIS run; run-3 immutable copies in
  `run3-archive/` (md5 of its vetted_identities.json verified
  `dbc440b6…` before the run). The live `vetted_identities.json` was
  re-vetted on this run's matches (ACCEPT 2,246); run-3's is in `run3-archive/`.
- Eval report: `eval_report.json` (alias-credited, accept-seeds input) vs
  `run3-archive/eval_report.json` (run 3, seeds.json input).
- Reproduce eval:
  `ghidriff-venv/bin/python tools/ghidra/eval_xenon_matches.py --run-dir
  build/SZBE69_B8/ghidra/ghidriff-xenon --seeds
  build/SZBE69_B8/ghidra/xenon-seeds/seeds_accept_run3.json
  --credit-platform-alias --stratify` (add `--sweep-vt-score 20:820:40` for the
  VT curve; the assigned 9.5:14 range is below VT's product distribution and
  culls nothing).
- Seeds `seeds_accept_run3.json` (2,130) gitignored; regen via
  `build_accept_seeds.py`.
- Cross-task leak note above is for T4, not a T1 defect.
