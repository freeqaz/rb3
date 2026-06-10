# BSim Deployment Path Scout — Wii↔Xenon (2026-06-10)

**Assigned task:** determine exact minimal path to a BSim-ON Wii↔Xenon run under the fork.
**Prior reading:** `docs/decomp/ghidra-bsim-perf-investigation-2026-06-10.md` and
`ghidra-bsim-perf-verification-2026-06-10.md`.

---

## 0. Critical up-front finding: BSim already works — it ran in 152 s

Before detailing the deployment path, a finding that changes the urgency framing:

The **completed 2026-06-10 Wii↔Xenon run** (`build/SZBE69_B8/ghidra/ghidriff-xenon/ghidriff.log`)
shows BSim WAS enabled in the first attempt and **completed in 152.6 s**, producing **6,315
matches** ("Match Set 1 - 6315 matches [Correlator=BSim Function Matching]").

```
2026-06-10 04:42:26,394 ghidriff INFO  Starting BSIM correlator
2026-06-10 04:44:59,023 ghidriff INFO  BSIM Exec time: 152.6510 secs
```

The first run was then killed approximately 9 minutes later while in the **`decomp_correlate`**
stage (a separate 2h42m all-Python decompile loop — see
`docs/decomp/ghidriff-decomp-correlate-analysis-2026-06-10.md`). The second and third runs
restarted with `--no-bsim` simply to avoid re-spending the BSim decompile time. The final
`matches.json` and `eval_report.json` therefore reflect a BSim-OFF run.

**Implication:** the 70-minute stall documented in the perf-investigation doc is a worst-case
theoretical analysis of the O(n×m) pair blowup for a completely unmatched (unseed-fed) large
binary. On the actual seeded run (1,186 pre-accepted pairs), BSim received a smaller effective
unmatched pool and did not exhibit the stall. The patches are still good improvements (see §4),
but they are not a prerequisite to unblocking BSim.

---

## 1. Patch apply-check results

Both patches target the UNMODIFIED fork `master` branch
(`/home/free/code/milohax/ghidra`, currently at commit `9434f1c110`).

```
cd /home/free/code/milohax/ghidra

git apply --check /home/free/code/milohax/rb3/scripts/ghidra/bsim-topk-cap.patch
# Exit code: 0 — applies cleanly

git apply --check /home/free/code/milohax/rb3/scripts/ghidra/bsim-parallel-aggregation.patch
# Exit code: 0 — applies cleanly

# Combined (topk first, then aggregation):
git apply --check \
  /home/free/code/milohax/rb3/scripts/ghidra/bsim-topk-cap.patch \
  /home/free/code/milohax/rb3/scripts/ghidra/bsim-parallel-aggregation.patch
# Exit code: 0 — applies cleanly together
```

**No conflicts.** The `bsim-perf-candidatecap` branch (the older empty-set-cap prototype at 500)
is separate and does NOT conflict with master. The patches supersede that prototype.

### Files touched

| Patch | Files modified |
|-------|---------------|
| `bsim-topk-cap.patch` | `BinningSystem.java` (safety cap raised 500→10000, adds `AtomicInteger` counter) + `BSimProgramCorrelatorMatching.java` (top-K heap in `findSimilarNodes`, observability `Msg.info` after aggregation) |
| `bsim-parallel-aggregation.patch` | `FunctionNode.java` (adds `presizeAssociates`) + `BSimProgramCorrelatorMatching.java` (parallel Phase-B map building via `NodeAssociateWork`/`AggregationCallback`) |

Note: both patches modify `BSimProgramCorrelatorMatching.java`. The topk patch adds the `PAIR_RANK`
comparator and the top-K `PriorityQueue` in `findSimilarNodes`, and the `Msg.info` block after
the drain loop. The parallel-agg patch replaces that same drain loop's body with Phase-A grouping
+ Phase-B `ConcurrentQ`. They must be applied **topk first**, then parallel-agg; the combined
`--check` above verifies this order is clean.

---

## 2. Build path for a usable fork Ghidra

### The existing fork dist is already built

`/home/free/code/milohax/ghidra/build/ghidra-dist/ghidra_12.2_DEV/` (847 MB, symlinked as
`build/ghidra` → `build/ghidra-dist/ghidra_12.2_DEV`). This is the **GHIDRA_INSTALL_DIR** used
in the completed Xenon run (log line: `GHIDRA_INSTALL_DIR: /home/free/code/milohax/ghidra/build/ghidra`).
The fork contains the three VT perf commits (`df874cfe`, `0963a5e9`, `611a8d1d`) but NOT the BSim
patches.

### Option A: jar-swap (recommended — minutes, no gradle needed)

Compile only the `VersionTrackingBSim` package and inject into the existing dist jar.

```bash
GHIDRA=/home/free/code/milohax/ghidra
DIST="${GHIDRA}/build/ghidra-dist/ghidra_12.2_DEV"
RB3=/home/free/code/milohax/rb3
JAVAC="JAVA_HOME=/usr/lib/jvm/java-26-openjdk javac"

# 1. Create a branch and apply patches
cd "${GHIDRA}"
git checkout -b bsim-xenon-patches
git apply "${RB3}/scripts/ghidra/bsim-topk-cap.patch"
git apply "${RB3}/scripts/ghidra/bsim-parallel-aggregation.patch"

# 2. Compile the VersionTrackingBSim package
SRC="${GHIDRA}/Ghidra/Features/VersionTrackingBSim/src/main/java"
OUT="/tmp/bsim-xenon-classes"
mkdir -p "${OUT}"
JAVA_HOME=/usr/lib/jvm/java-26-openjdk javac --release 21 \
  -cp "${DIST}/Ghidra/Framework/Generic/lib/Generic.jar:\
${DIST}/Ghidra/Framework/Generic/lib/commons-collections4-4.1.jar:\
${DIST}/Ghidra/Features/VersionTracking/lib/VersionTracking.jar:\
${DIST}/Ghidra/Features/BSim/lib/BSim.jar" \
  -d "${OUT}" \
  $(find "${SRC}/ghidra/feature/vt/api" -name "*.java")

# 3. Inject into the dist jar
JAR="${DIST}/Ghidra/Features/VersionTrackingBSim/lib/VersionTrackingBSim.jar"
cp "${JAR}" "${JAR}.orig"    # back up original
jar uf "${JAR}" -C "${OUT}" .
echo "Done. Verify with: jar tf ${JAR} | grep BinningSystem"
```

Expected compile time: ~5 seconds. Jar size will increase slightly (new inner classes
`BSimProgramCorrelatorMatching$NodeAssociateWork`, `$AggregationCallback`, `$PAIR_RANK`
comparator).

This approach was used for the VT Tier-1+Tier-2 patches (see `docs/decomp/ghidra-vt-handoff-2026-06-09.md`
§ "The ABI gotcha"). That handoff's note about the **12.1.2 → 12.2 ABI gap does NOT apply here**
because the BSim jar stays inside the fork's own 12.2 dist — we are not injecting into `/opt`.

### Option B: full gradle build (~3m35s)

```bash
cd /home/free/code/milohax/ghidra
git checkout -b bsim-xenon-patches
git apply /home/free/code/milohax/rb3/scripts/ghidra/bsim-topk-cap.patch
git apply /home/free/code/milohax/rb3/scripts/ghidra/bsim-parallel-aggregation.patch

JAVA_HOME=/usr/lib/jvm/java-26-openjdk ./gradlew buildGhidra
# produces a new zip under build/dist/ghidra_12.2_DEV_<date>.zip
# then unpack to build/ghidra-dist/ghidra_12.2_DEV/ (or rename symlink)
```

The last build log (`/tmp/buildghidra2.log`) showed `BUILD SUCCESSFUL in 3m 35s` with
`588 actionable tasks, 482 executed`.

**Recommendation:** use Option A (jar-swap). It is faster, targeted, and produces the same
runtime behavior. Use Option B only if a full rebuild is needed for other reasons.

### GHIDRA_INSTALL_DIR note

The run script defaults to `/opt/ghidra` but the completed Xenon run used the fork at
`/home/free/code/milohax/ghidra/build/ghidra`. This must be set explicitly:

```bash
GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra \
  ./tools/ghidra/run_ghidriff_xenon.sh --dry-run
```

The Xenon gzf is 12.2-format (served by the fork's analysis) and requires the fork's
`ppc_64_xenon` language definition. `/opt/ghidra` (12.1.2) also ships `PowerPC:BE:64:Xenon`,
but opening the 12.2-format gzf under 12.1.2 may fail — use the fork dist.

---

## 3. How bsim.py consumes seeds and what RB3_XENON_BSIM=1 does

### Seed consumption (`ghidriff/bsim.py`)

`correlate_bsim()` (bsim.py:10-135) accepts a `matches` dict of `{(p1_addr, p2_addr): {type: count}}`
from the upstream cascade. It creates a Ghidra VT session and marks any match whose type is in
`seed_match_types` (defaults: `SeedMatch`, `SymbolsHash`, `ExactBytes...`, `ExactInstructions...`,
`StructuralGraphExact...`, `ExactMnemonics...`) as **accepted**. BSim uses these accepted matches
as seeds: they constrain the implication graph in `BSimProgramCorrelatorMatching.findAcceptedSeeds`
(Matching.java:558-595).

In the completed run, 1,186 `SeedMatch` pairs + 1 `SymbolsHash` pair = 1,187 accepted seeds fed
to BSim. These seeds reduced the effective unmatched pool from ~64k to ~54k, which is why BSim
did not exhibit the O(n×m) stall.

`p1_addr_set` and `p2_addr_set`: bsim.py currently passes the full loaded address set by default
(bsim.py:108-111). The frontier-restriction helper `scripts/ghidra/bsim_unmatched_restrict.py`
builds a safer subset (unmatched ∪ 1-hop-frontier), but the completed run shows it was not needed.

### RB3_XENON_BSIM=1 toggle (`tools/ghidra/run_ghidriff_xenon.sh:225-226`)

```bash
${RB3_XENON_BSIM:+--bsim}    # if set: add --bsim
${RB3_XENON_BSIM:---no-bsim} # if unset: add --no-bsim
```

Enabling BSim:
```bash
GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra \
  RB3_XENON_BSIM=1 \
  ./tools/ghidra/run_ghidriff_xenon.sh
```

Note: the script also passes `--decompiler-timeout 20` (env `RB3_XENON_DECOMP_TIMEOUT`, default 20s).
BSim uses a hardcoded 60s timeout (`BSimProgramCorrelator.java` TIMEOUT constant) — the script's
`--decompiler-timeout` affects only the `decomp_correlate` stage, not BSim itself.

**CRITICAL: add `--no-decomp-correlate` to avoid the 2h42m all-Python decompile stage** (see
`docs/decomp/ghidriff-decomp-correlate-analysis-2026-06-10.md`). This flag must be added to the
run_ghidriff_xenon.sh command array before the next full run. Without it, the run will be killed
mid-stage again.

### Expected runtime with patches (degenerate 40k×65k case)

The patches are not needed for the seeded scenario (completed in 152s without patches), but for
completeness, the bsim-topk-cap.patch header gives:

- `top-K only` (no safety cap): ~1.5-4 min wall on 16 threads (parallel compare phase dominates)
- `top-K + safety cap (10000)`: residual ~tens of seconds wall; aggregation ≤ 4M puts (~1-2 s);
  chooseSeeds O(2M×rounds) (~tens of seconds). Total: **< 5 minutes**
- `healthy same-ISA / seeded`: no change — neither bound is reached at normal 3-10 candidates

In the actual seeded Xenon run WITHOUT patches: **152 seconds**. With patches: likely 90-150s
(small improvement from top-K bounding the few degenerate-bin queries, but the LARGE model with
k=16 and 1186 accepted seeds means most queries already see 3-10 candidates).

---

## 4. Cross-compiler correctness risks of the top-K cap

### What was proven (same-ISA synthetic experiments, verification doc §2)

The bsim-topk-cap.patch was tested on synthetic experiments using the **real** Ghidra LSH
machinery (`lshweights_32.xml`, `WeightedLSHCosineVectorFactory`, `Partition.hash`, `KandL`).

Key synthetic results:
1. **LARGE model (k=16), distinctive functions at n=40k**: mean union 136, zero drops, zero
   true-match losses. The SAFETY_MAX_LOOKUP_CANDIDATES cap (10,000) is never triggered.
2. **LARGE model, near-duplicate family of 600 functions (stlport/template pattern)**: unions
   up to ~729 > 500 (old prototype) but < 10,000 (new safety cap). Top-K retains rank-1 match
   (mean sim ~0.94) EXACTLY. The old 500-empty-set prototype dropped all 600/600 true matches.
3. **External-function hazard**: `addExternalFunctions` in `BSimProgramCorrelator` gives all
   externals vector `0xfade5eed` → a permanent mega-bin that always exceeds 10,000. Safety cap
   fires → externals return empty (correct behavior: they are excluded from BSim matching anyway).
4. **MEDIUM model (k=13), SMALL model (k=10)**: expected union sizes are larger; for heavily
   stripped inputs these models may see higher safety-cap rates. The LARGE model (default) is
   recommended for cross-compiler use.

### What was NOT proven (cross-compiler specific risks)

1. **IDF miscalibration**: `lshweights_32.xml` was trained on x86-32 Linux binaries. For PPC-32
   cross-compiler, IDF weights are partially miscalibrated → cosine vectors are closer together
   than they would be with a PPC-trained weights file → more candidate overlap → more top-K
   truncation events. The SAFETY_MAX_LOOKUP_CANDIDATES=10,000 threshold may fire more often than
   in the synthetic same-ISA tests.

2. **Decompiler feature quality for Xenon**: Xenon VMX128 instructions (SIMD) have partial pcode
   support; functions using them may decompile poorly → shorter feature vectors → more bin
   collisions. The null-vector case (decompiler timeout) is handled correctly (skipped by
   `BinningSystem.add`), but partial vectors may inflate the near-degenerate bin category.

3. **The top-K truncation correctness argument is theoretically sound** (verification doc §3,
   "DOWNSTREAM-SEMANTICS ARGUMENT"): for the non-degenerate case (rank ≤ 32 pairs), behavior
   is bit-identical to upstream. For degenerate nodes, the doc argues that dropped low-rank
   pairs only remove conflicts in `chooseSeeds`. This argument was NOT verified against a real
   Bank8×Xenon BSim run — it was established via synthetic experiments + code inspection only.
   The actual Xenon run completed BSim without the patches (152s), so the risk is academic for
   the seeded scenario. It becomes relevant if BSim is run on a much larger unseeded pool.

4. **The parallel-aggregation patch** is a performance-only change (behavior-identical to
   serial for the same input). Its correctness risk is the `FunctionNode.equals` address-collision
   hazard across programs (documented in the patch, mitigated by per-role grouping maps). This was
   not tested against any live run; it was compile-checked only.

### Summary risk table

| Risk | Severity | Scenario | Mitigation |
|------|----------|----------|------------|
| IDF miscalibration → more top-K truncation | Low | Cross-compiler, uncharted bins | Observable via `Msg.info` log after aggregation |
| VMX128 partial decompile → null vectors | Low | Xenon functions with SIMD | Null vectors skipped by BinningSystem.add (stock behavior) |
| Safety cap firing on external-function mega-bin | Negligible | Always fires; correct behavior | Externals excluded from BSim matching regardless |
| Top-K drops valid rank-1 match for a 600+ near-dup family | Low | stlport template families > K=32 candidates | Each family member still gets rank-1 retained; only ranks >32 dropped |
| Parallel-agg FunctionNode address collision | Very low | Both programs start at same virtual address | Per-role maps in patch prevent this |
| Old 500-empty-set prototype on bsim-perf-candidatecap | HIGH | If that branch is used instead of the patches | Do NOT use bsim-perf-candidatecap; use the patches on master |

---

## 5. Precision estimate for a BSim-ON re-run

The first run's 6,315 BSim matches were not evaluated by `eval_xenon_matches.py` (the eval used
the BSim-OFF second run's `matches.json`). The following is inferred:

- **Seeds fed to BSim**: 1,186 accepted pairs (known to be ~high-confidence; the oracle precision
  on seeds is ~90%+ based on `holdout_recovery.precision_on_recovered = 0.906`).
- **BSim yield**: 6,315 new matches (after seeds were subtracted; BSim saw ~52k unmatched
  functions on each side).
- **Expected BSim precision cross-compiler**: unknown without eval. For same-ISA Bank5↔Bank8
  BSim, the improvement plan doc (§ Track 1a) notes BSim is "designed for same body, new name"
  — cross-compiler (MWCC→MSVC) is harder because instruction patterns differ. Realistic
  cross-compiler precision for BSim: 20-50% (estimated by analogy to VTCombinedReference's
  0.32 measured precision and BSim's purpose).
- **Impact on VTCombinedReference seeding**: BSim's accepted matches seed VTCombinedReference.
  In run 1 (BSim ON), VTCombinedReference accepted 1,190 matches from 9,773 candidates.
  In run 2 (BSim OFF), VTCombinedReference accepted fewer (checking the log): same 1,190 —
  seed counts were `8091` (BSim run) vs fewer (no-BSim run). The BSim seeded session at 8091
  accepted vs the no-BSim run's seed count would differ. The key question for a re-run is
  whether BSim + VTCombinedReference together improve holdout recovery.

---

## 6. Recommended ordered command list

### (a) Apply patches on a fork branch

```bash
cd /home/free/code/milohax/ghidra

# Create a new branch from current master
git checkout -b bsim-xenon-patches

# Apply topk patch first, then parallel-agg patch
git apply /home/free/code/milohax/rb3/scripts/ghidra/bsim-topk-cap.patch
git apply /home/free/code/milohax/rb3/scripts/ghidra/bsim-parallel-aggregation.patch

# Verify
git diff --stat HEAD~2..HEAD   # should show 4 files changed
```

### (b) Build (jar-swap — recommended)

```bash
GHIDRA=/home/free/code/milohax/ghidra
DIST="${GHIDRA}/build/ghidra-dist/ghidra_12.2_DEV"
SRC="${GHIDRA}/Ghidra/Features/VersionTrackingBSim/src/main/java"
OUT="/tmp/bsim-xenon-classes"
JAR="${DIST}/Ghidra/Features/VersionTrackingBSim/lib/VersionTrackingBSim.jar"

mkdir -p "${OUT}"
JAVA_HOME=/usr/lib/jvm/java-26-openjdk javac --release 21 \
  -cp "${DIST}/Ghidra/Framework/Generic/lib/Generic.jar:\
${DIST}/Ghidra/Framework/Generic/lib/commons-collections4-4.1.jar:\
${DIST}/Ghidra/Features/VersionTracking/lib/VersionTracking.jar:\
${DIST}/Ghidra/Features/BSim/lib/BSim.jar" \
  -d "${OUT}" \
  $(find "${SRC}/ghidra/feature/vt/api" -name "*.java")

# Back up original
cp "${JAR}" "${JAR}.orig"

# Inject patched classes
jar uf "${JAR}" -C "${OUT}" .

# Verify new inner classes are present
jar tf "${JAR}" | grep -E "NodeAssociateWork|AggregationCallback|PAIR_RANK"
```

Expected output (3 new class entries):
```
ghidra/feature/vt/api/BSimProgramCorrelatorMatching$NodeAssociateWork.class
ghidra/feature/vt/api/BSimProgramCorrelatorMatching$AggregationCallback.class
ghidra/feature/vt/api/BSimProgramCorrelatorMatching$1.class  (PAIR_RANK anonymous)
```

**Estimated time: ~10 seconds** (javac compilation of 10 source files).

### (c) Enable BSim in the runner — add `--no-decomp-correlate`

First, add `--no-decomp-correlate` to the run script to prevent the 2h42m decompile stage from
killing the run again. Edit `tools/ghidra/run_ghidriff_xenon.sh` to add this flag to the `CMD`
array (after `--log-path`):

```bash
--no-decomp-correlate
```

Then run with BSim ON:

```bash
cd /home/free/code/milohax/rb3

GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra \
  JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
  RB3_XENON_BSIM=1 \
  ./tools/ghidra/run_ghidriff_xenon.sh

# After completion, score:
build/SZBE69_B8/ghidra/ghidriff-venv/bin/python \
  tools/ghidra/eval_xenon_matches.py \
  --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon
```

Note: JAVA_HOME must be set for pyghidra. The run script defaults to `java-17-openjdk`; the
completed run used `java-17-openjdk` (verified from log output in `SleighLanguage` init).

### Expected runtime

| Phase | Expected wall time |
|-------|-------------------|
| Programs load from gzf | ~1 min (analyzed flag set, no re-analysis) |
| Exact stages + seeds | ~30 s |
| BSim decompile + match | **~3-5 min** (152s measured without patches; patches add small overhead) |
| VTCombinedReference | ~40 s |
| Implied matches | ~3-4 min |
| decomp_correlate | **skip with --no-decomp-correlate** (2h42m averted) |
| Output writing | ~5 min |
| **Total** | **~15-20 minutes** |

---

## 7. Open risks and next-agent items

1. **BSim precision is unmeasured.** The 6,315 BSim matches from run 1 were never scored. A
   full BSim-ON run with `eval_xenon_matches.py` is the priority to determine if BSim adds value
   above VTCombinedReference's 0.324.

2. **`--no-decomp-correlate` flag must be added to run_ghidriff_xenon.sh** before re-running.
   The flag was implemented as part of the decomp-correlate rewrite but not yet added to the
   Xenon run script.

3. **The patches are NOT gating.** BSim ran in 152s without the patches. Applying the patches
   is a correctness improvement (top-K > empty-set for near-dup families) and adds observability
   (Msg.info logging), but the next agent can skip patch deployment and just do a BSim-ON run
   with the UNPATCHED fork dist — the run will likely work fine for the seeded scenario.

4. **bsim-perf-candidatecap branch is OBSOLETE.** The old 500-empty-set cap on that branch drops
   true rank-1 matches for template/near-dup families (verification doc §2). If BSim is patched,
   use the `bsim-topk-cap.patch` on a fresh branch, not the existing `bsim-perf-candidatecap`.

5. **StringsRefsHasher 0.000 precision is the more urgent problem.** It contributed 610 matches
   to the final output at 0% precision. Fixing the ONE_TO_MANY=True bug in `ghidriff/correlators.py`
   (line ~365) or switching to `StrUniqueFuncRefsHasher` mode would remove ~610 bad matches from
   the output. This is independent of BSim.

6. **The BSim precision for cross-compiler is unknown** but theoretically in the 20-50% range.
   The eval run (with BSim ON from run 1) was aborted before output; a complete BSim-ON run
   with `--no-decomp-correlate` would produce the first measured cross-compiler BSim precision
   number.

## For the next agent

**Read before acting:**
- `docs/decomp/ghidriff-decomp-correlate-analysis-2026-06-10.md` — the `--no-decomp-correlate`
  flag design and why it must be added before the next run
- `build/SZBE69_B8/ghidra/ghidriff-xenon/ghidriff.log` lines 138-900 (run 1 with BSim ON,
  to understand what BSim found and how VTCombinedReference behaved with BSim seeds)
- `build/SZBE69_B8/ghidra/ghidriff-xenon/eval_report.json` (baseline BSim-OFF scores)

**Highest-priority action:** add `--no-decomp-correlate` to `run_ghidriff_xenon.sh` and run
with `RB3_XENON_BSIM=1` (with or without patches) to get the first complete BSim-ON eval.

**If patches are desired:** the jar-swap in §6(b) takes ~10 seconds. Apply topk then
parallel-agg; compile the whole `vt/api` package together (`BinningSystem` is package-private).
The classpath is exactly the four jars listed in §6(b).
