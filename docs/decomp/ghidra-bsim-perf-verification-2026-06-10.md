# Ghidra BSim VT perf — verification & synthesis (2026-06-10)

**Follow-up to:** `docs/decomp/ghidra-bsim-perf-investigation-2026-06-10.md` (rb3 `1691fcbc`)
**Prototype under test:** worktree `/tmp/claude/ghidra-bsim-perf`, branch
`bsim-perf-candidatecap` in `/home/free/code/milohax/ghidra`
(`BinningSystem.java` `MAX_LOOKUP_CANDIDATES=500` → return EMPTY set when a bin
union exceeds the cap, + `BSimProgramCorrelatorMatching.java`).

Five independent verification tasks (T1–T5) were fanned out; this doc is the
synthesis. **All deliverables are compile-/syntax-checked only** — nothing was
run against the live 65k-func Bank8×Xenon pair, and the prototype branch is
unmodified.

---

## 1. Bottleneck verdict: CONFIRMED (with two nuances)

Independent source audit **agrees** with the investigation doc:

- `BinningSystem.lookup()` (`VersionTrackingBSim/.../BinningSystem.java:90-100`)
  does an **unbounded** `result.addAll` union over L LSH bins.
- The serial drain at `BSimProgramCorrelatorMatching.java:220-237` calls
  `FunctionNode.addAssociate` (plain `HashMap`, `FunctionNode.java:54/144`) once
  per pair → degenerate bins make it O(n_src × n_dest) serial puts + rehash +
  `FunctionPair` heap blowup; then `chooseSeeds` (`:280-354`) re-iterates all
  surviving pairs per round through `HashSetValuedHashMap` multimaps.

Nuances the original doc under-stated:

1. The **parallel** `MatchingCallback` compare loop is *also* O(n×m) work in the
   degenerate case — it's just parallel and per-candidate cheap, so the serial
   aggregation / chooseSeeds / heap phases dominate wall-clock.
2. The prototype's cap is **lossier than necessary**: bounding *output* in
   `findSimilarNodes` to top-K by similarity (at the `results.add` site, `:121`)
   preserves the true nearest neighbor while still bounding serial-phase pair
   count. That alternative was built and evaluated as T2 (§3).

---

## 2. Candidate-cap CORRECTNESS verdict: **LOSSY — do not ship as-is**

**T1 (recall stress-test):** `scripts/ghidra/BSimBinAuditScript.java`
(compile-checked vs fork dist jars; ground-truth audit to run against real
projects later) plus three bounded synthetic experiments using the **real**
Ghidra LSH machinery (`lshweights_32.xml`, `WeightedLSHCosineVectorFactory`,
`Partition.hash`, `KandL`).

Key findings — the doc's "those pairs are low-confidence anyway" safety claim
**does not hold**:

1. **The calibration behind the cap is wrong at its root.** `LSHMemoryModel`'s
   ctor is `(label, k, probabilityThreshold, tauBound)`; LARGE = (k=16,
   prob=0.97, tau=0.75). Ghidra's own `KandL.memoryModelToL` gives **L=229**
   for LARGE (MEDIUM k=13 L=104, SMALL k=10 L=47) — not the doc's
   "k=16, L=5" (a tau/probThresh swap). Design-mean union L·n/2^k at
   n_src=40k: LARGE ~140, MEDIUM ~508, SMALL ~1836. So **cap=500 is only ~3.6×
   the design mean on LARGE and BELOW the mean on MEDIUM/SMALL** — not "100×
   expected density".
2. **Empirical rank-1 true-match drops.** SMALL @ n=10k: 20% of probes
   exceeded 500 and *every* dropped true match was rank 1 (mean sim ~0.94).
   MEDIUM @ 40k: the *majority* of all queries return empty. LARGE @ 40k:
   distinctive functions are safe (mean union 136, zero drops) — **but** a
   600-member near-duplicate family (template/stub regime, abundant in RB3's
   stlport-heavy C++) pushed unions to ~729: **all 600 true matches dropped,
   589/600 at rank 1, 460 with sim ≥ 0.95**. The cliff sits at family size
   ~450–600. Bonus hazard: `addExternalFunctions` gives ALL externals one
   identical vector (`0xfade5eed`) → a permanent mega-bin that always trips the
   cap and poisons ~0.35% of colliding queries.
3. **The loss is unrecoverable downstream:** empty lookup → no `FunctionPair`s
   → no associates → the function is invisible to `doMatching`'s call-graph
   implication rounds — the exact mechanism designed to disambiguate near-dup
   families. "No confident match anyway" is doubly wrong.

The exact % recall loss on the real Bank8×Xenon LARGE run depends on the real
family-size distribution; `BSimBinAuditScript.java` measures precisely that
(bin-occupancy histogram, over-cap counts at {100..5000}, per same-named pair:
in-union / dropped-by-cap / rank, plus recall under a per-bin-skip alternative).

---

## 3. The better path: top-K truncation (+ raised safety cap), with parallel aggregation as a complement

**T2 (non-lossy bounded output):** `scripts/ghidra/bsim-topk-cap.patch` —
compile-checked unified diff vs upstream. Design:

- `findSimilarNodes` keeps a bounded min-heap of the top
  `MAX_PAIRS_PER_QUERY=32` pairs per dest query, ranked by a tie-free total
  order (similarity, confidence, source address) → **exact nearest-neighbor
  preserving, deterministic**.
- The empty-set safety cap is RAISED 500 → `SAFETY_MAX_LOOKUP_CANDIDATES=10000`
  (unions of 500–10000, fully dropped by the prototype, now keep exact top-K).
- `Msg.info` post-aggregation summary (pairs discovered / truncated /
  safety-capped) — fixes the silent-stall diagnosability gap.

Cost estimate for the degenerate 40k×65k case: residual compare work lands in
the already-parallel phase (~1.5–4 min wall on 16 threads with the safety cap;
aggregation ≤ ~4M puts ≈ 1–2 s). Healthy same-ISA runs are **bit-identical to
upstream** (neither bound reached; K=32 never hit at normal 3–10 candidates).

**T3 (exact-result-preserving parallel aggregation):**
`scripts/ghidra/bsim-parallel-aggregation.patch` — compile-checked.
**Verdict: complements, cannot replace, the cap.** Exact preservation still
allocates ~150 B/pair → ~4e8 pairs ≈ **~60 GB heap** (worst case ~390 GB) on
the degenerate input, and pair *creation* happens in the already-parallel
lookup phase before aggregation; `chooseSeeds` stays serial. For large
*non-degenerate* workloads it gives ~5–10× from map pre-sizing plus 4–16×
parallel headroom with an identical pair set. Determinism findings:
`chooseSeeds` is order-sensitive, so the drain stays serial/order-preserving
and only per-node map-building is parallelized (partition-by-node, one writer
per plain HashMap); the upstream baseline is itself run-to-run nondeterministic
(ConcurrentQ collects in completion order, `ConcurrentQ.java:645`); a naive
`ConcurrentHashMap` would make `calculateBestNeighbor` tie-breaks
thread-arrival-dependent — partitioning strictly dominates. Hazard noted:
`FunctionNode.equals` is Address-only and can collide across programs →
per-role grouping maps required.

**Recommended ship order:** T2 top-K patch (replaces the prototype's
empty-set cap) → optionally layer T3 for large valid inputs. The prototype's
500-empty-set cap should NOT ship for cross-binary use.

---

## 4. Benchmark harness (A/B, determinism-checked)

`scripts/ghidra/BSimCorrelatorBenchmarkScript.java` — vt_ref_ab.sh-pattern
harness, compile-checked AND smoke-run twice end-to-end on a tiny bounded
input (two copies of `/usr/bin/ls`, maxFuncs=300): 281 matches, determinism
digest (SHA-256 over sorted `srcAddr,destAddr,score,confidence` tuples)
**identical across reps**.

- Phase attribution without patching Ghidra: a `WrappingTaskMonitor` subclass
  timestamps every `setMessage` transition (`BSIM-BENCH-PHASE` lines). The
  serial addAssociate stall runs under the message *"Zealously over-pairing
  matches..."* — that phase's `phaseMs` is the number to compare stock vs
  capped/top-K jars.
- Usage (swap `GHIDRA_HOME` per jar under test; `JAVA_HOME=/usr/lib/jvm/java-26-openjdk`,
  `GHIDRA_HEADLESS_MAXMEM=4G`):

  ```bash
  # one-time import
  analyzeHeadless /tmp/bsim-bench-proj bsimbench -import /tmp/a_src.elf /tmp/b_dst.elf
  # per-run
  analyzeHeadless /tmp/bsim-bench-proj bsimbench -process a_src.elf -noanalysis -readOnly \
    -scriptPath /home/free/code/milohax/rb3/scripts/ghidra \
    -postScript BSimCorrelatorBenchmarkScript.java /a_src.elf /b_dst.elf 300
  ```

  Optional args 4/5 override seed-confidence / implication thresholds; env
  `BSIM_BENCH_MEMORY_MODEL`, `BSIM_BENCH_MAX_SEEDS` for tuning experiments.
- Gotcha encoded in the harness: thunk-duplicated names cause BLOCKED
  associations on accept — seed only names unique on both sides and check
  `VTAssociationStatus.AVAILABLE`.

---

## 5. Tuning sidestep (no rebuild): real but limited, and the naive shape is hazardous

**T5:** `scripts/ghidra/bsim_unmatched_restrict.py` (syntax-checked helper:
`build_unmatched_addr_sets` / `build_frontier_addr_sets` / `estimate_speedup` /
`apply_perf_tuning`).

- **Option-name ground truth** (`BSimProgramCorrelatorFactory.java`):
  `SEED_CONF_THRESHOLD` ("Confidence Threshold for a Seed", default 10.0),
  `IMPLICATION_THRESHOLD` ("Confidence Threshold for a Match", default 0.0),
  `MEMORY_MODEL` (default LARGE), `USE_ACCEPTED_MATCHES_AS_SEEDS` (default true).
- **`SIMILARITY_THRESHOLD=0.5` is NOT tunable without a rebuild** — it's a
  `public static final double` (`BSimProgramCorrelator.java:62`), a
  compile-time constant inlined into bytecode, unreachable even by reflection.
  The original doc §5.1's "2–5× fewer pairs from raising similarity" is not
  achievable zero-rebuild. Raising `SEED_CONF_THRESHOLD` does NOT cut the
  blowup either (applied after `finalPairs` sorting, `Matching.java:366`).
- **Address sets DO pass through** (`VTAbstractProgramCorrelatorFactory.createCorrelator`
  forwards p1/p2 verbatim; participation = entry-point-in-set), and ghidriff's
  `bsim.py` already accepts `p1_addr_set`/`p2_addr_set`.
- **Recall cost:** the naive §5.4 "unmatched-only" restriction is **HAZARDOUS**
  on *either* side: (1) `findAcceptedSeeds` needs a FunctionNode on BOTH
  endpoints of every accepted association plus a re-discovered binning edge —
  excluding matched functions zeroes the accepted-seed mechanism ghidriff
  exists to feed, degrading BSim to self-seeded chooseSeeds (which even lowers
  the threshold to the best available pair — bad-seed propagation on stripped
  cross-ISA); (2) `FunctionNodeContainer.generateCallGraph` silently drops call
  edges whose target has no node → severs NeighborGenerator fan-out through
  excluded anchors.
- **Safe shape:** unmatched ∪ 1-hop matched *frontier* (matched functions
  directly adjacent to an unmatched one), lifted pairwise so both endpoints of
  a frontier pair stay present. Near-lossless by construction (all useful 1-/2-hop
  implication edges survive). Expected win ~**2.9–6×** on the pair phases
  (vs ~10.8× for the hazardous naive shape) — and proportionally less on this
  specific run, since the stripped Xenon side likely had far fewer pre-accepted
  exact matches than the doc's 60–70% assumption. The module's
  `estimate_speedup(stats)` computes the real factor from actual set sizes.

Bottom line for tuning: the frontier address-set restriction is the only
zero-rebuild lever on the stall and is worth ~3–6×, but the Tier-1 candidate
bound (in its top-K form) remains the correct fix for the degenerate-bin
O(n×m) blowup itself.

---

## 6. Decision summary

| Question | Verdict |
|---|---|
| Is the doc's bottleneck diagnosis right? | **Yes** (serial aggregation + chooseSeeds dominate; parallel compare phase is also O(n×m) but cheap/parallel) |
| Is the 500-cap empty-set prototype safe to ship? | **No** — drops rank-1 true matches in near-dup families (all 600/600 in the LARGE synthetic family test), majority-of-queries loss on MEDIUM/SMALL; calibration was based on a tau/L swap |
| Better Tier-1 | **Top-K (K=32) + raised safety cap (10000)** — `scripts/ghidra/bsim-topk-cap.patch`; exact for healthy runs, bounded for degenerate |
| Is exact-parallel a substitute? | **No** — ~60 GB heap on the degenerate input; it's a complement for large valid inputs (`scripts/ghidra/bsim-parallel-aggregation.patch`) |
| How to measure before/after | `scripts/ghidra/BSimCorrelatorBenchmarkScript.java` (phase timings + determinism digest), `scripts/ghidra/BSimBinAuditScript.java` (real recall loss) |
| Zero-rebuild mitigation | Frontier address-set restriction, ~3–6× (`scripts/ghidra/bsim_unmatched_restrict.py`); naive unmatched-only is recall-hazardous |

### Deliverables

| Task | File |
|---|---|
| T1 recall audit | `scripts/ghidra/BSimBinAuditScript.java` |
| T2 top-K patch | `scripts/ghidra/bsim-topk-cap.patch` |
| T3 parallel aggregation patch | `scripts/ghidra/bsim-parallel-aggregation.patch` |
| T4 A/B benchmark harness | `scripts/ghidra/BSimCorrelatorBenchmarkScript.java` |
| T5 address-set helper | `scripts/ghidra/bsim_unmatched_restrict.py` |

All compile-/syntax-checked against the fork dist jars
(`/home/free/code/milohax/ghidra/build/ghidra-dist/ghidra_12.2_DEV/`,
JDK 26, `--release 21`); none run against the live 65k pair.
