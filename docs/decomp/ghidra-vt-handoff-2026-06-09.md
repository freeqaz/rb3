# Ghidra Version Tracking — performance work handoff (2026-06-09)

Session goal: VT (Bank 5 DWARF → Bank 8 target markup port via `analyzeHeadless`) was a
single-threaded ~1h45m run. We found and fixed the two bottlenecks, validated + deployed them,
and assessed ghidriff. **All work coordinated via subagent workflows; this doc is the durable record.**

See also: [ghidra-vt-optimization-2026-06-09.md](ghidra-vt-optimization-2026-06-09.md) (root-cause +
3-tier plan), [ghidra-vt-tier2-concurrency-design-2026-06-09.md](ghidra-vt-tier2-concurrency-design-2026-06-09.md)
(concurrency design), and memory `project_ghidra_vt_optimization.md`.

## TL;DR status

| Item | State |
|---|---|
| **Tier-1** record-key equals/hashCode (kills O(n²) dupe-apply) | ✅ **LIVE in /opt**, validated ~11–19× on the hot path |
| **Tier-2** parallel reference correlator (ConcurrentQ) | ✅ validated 3–4×, byte-identical + deterministic, review SAFE |
| **Tier-2 mem-fix** (chunked score+commit) | ✅ no-OOM at 2G proven w/ negative control |
| **Combined jar deployed to /opt** | ✅ user ran the `sudo cp` (sha `7378183658`, both fixes confirmed in /opt) |
| **Full VT re-run (both fixes)** | ⏳ IN PROGRESS — session `RB3_b5_to_b8_opt`, watcher task `bjyefv33g`, ~30m+ in (orig was 105m) |
| **:8001 pyghidra service** | ⛔ DOWN (stopped to free the project lock for the re-run) — restart after |
| **ghidriff Bank5↔Bank8 diff** | ✅ done; modest decomp value (see below); 6 GB byproduct to clean |
| **Upstream PR** | ⏳ not started — 3 cherry-pickable fork commits ready |

## The two fixes (in the Ghidra fork `/home/free/code/milohax/ghidra`, branch `master`)

**Tier-1 `df874cfe` — `VTAssociationDB.equals/hashCode`.** They decoded the stored address keys
back to `Address` via the *`synchronized` `AddressMapDB.decodeAddress`* (a DB lock per `HashSet.add`),
and `hashCode` was sum-of-offsets (collides across identical thunks). `AutoVersionTrackingTask.getAllRelatedAssociations`
builds a `HashSet<VTAssociationDB>` over the ~k² associations a duplicate-function group emits → O(n²)
locked decodes → hours on one core. Fix: compare the cached DB **record key** (`getKey()`, which
`getOrCreateAssociationDB` keeps strictly 1:1 with `(src,dst)`) — lock-free, decode-free,
behavior-preserving. (`AssociationStub` uses identity equals/hashCode and is never `(src,dst)`-mixed
with DB objects, so no consistency regression.)

**Tier-2 `0963a5e9` (parallelize) + `611a8d1d` (mem-fix) — `VTAbstractReferenceProgramCorrelator.findDestinations`.**
The dominant cost is the `O(dest×src)` cosine-compare loop over read-only LSH vector maps. Parallelized
it with Ghidra's own `ConcurrentQ`/`ConcurrentQBuilder` (stateless workers, per-pair-local `VectorCompare`)
→ **serial, sorted-by-address commit** for `transform`/`getFunctionAt`/`addMatch` (all Program + matchSet
access stays single-thread). **Race-free crux:** `LSHCosineVectorAccum.compare()` lazily mutates both
operands via `doFinalize()`; it's idempotent + public, and `compare()` is read-only afterward — so we
**pre-finalize every vector serially up front**, then the parallel compares need no locks. Also collapsed
a redundant double-`compare` (free 2×). The mem-fix processes destinations in **sorted chunks of 256**
(score one chunk in parallel → commit it serially → drop it) to bound peak heap (the first cut materialized
all results and OOM'd a 2G heap at scale).

## Deployment — the ABI gotcha (IMPORTANT)

- `/opt/ghidra` is **Ghidra 12.1.2 DEV** (what pyghidra + `run_version_tracking.sh` use). The **fork is
  12.2 DEV** (renamed `DBObject`→`DbObject`, added classes). A fork-built `VersionTracking.jar` references
  `DbObject` (absent in /opt) → `NoClassDefFoundError`. **You cannot just drop a fork jar into /opt.**
- Correct deploy = **recompile only the two patched classes against /opt's OWN 12.1.2 classpath** and
  inject them into a copy of /opt's stock jar. The ready artifact:
  **`build/SZBE69_B8/ghidra/VersionTracking-opt1212.jar`** (stock 12.1.2 VT module + Tier-1 + Tier-2;
  sha `7378183658`). It was smoke-tested in a reflink-/opt (dupe `164ms` vs `2487ms` stock; ref correlator
  2.75× deterministic; no NoClassDefFoundError) and the user installed it.
- Backup of stock 12.1.2 jar: `/opt/.../VersionTracking/lib/VersionTracking.jar.orig`.
- **Re-deploy after ANY Ghidra reinstall/update** (binary install gets overwritten). Install command:
  `sudo cp build/SZBE69_B8/ghidra/VersionTracking-opt1212.jar /opt/ghidra/Ghidra/Features/VersionTracking/lib/VersionTracking.jar`

## Benchmarks (rb3 `scripts/ghidra/`, need a JDK not a JRE)

- `vt_dupe_benchmark.sh N` + `VTDupeBenchmarkScript.java` — Tier-1 dupe-apply path. Proves the O(n²)
  (N=100/200/300/700 → 96/1380/3963/68842 ms unpatched) and the fix (getAllRelatedAssociationsMs collapses).
- `vt_ref_ab.sh` / `vt_ref_benchmark.sh` + `VTRefCorrelatorBenchmarkScript.java` — Tier-2 reference
  correlator A/B (serial vs parallel runtime), emits speedup + a tuple/SHA digest for determinism.
- Run with `JAVA_HOME=/usr/lib/jvm/java-26-openjdk` (the host's `java-21` is a JRE — no `javac`).
- Pre-built ABI-matched fork runtimes for A/B: `/home/free/code/milohax/ghidra-rt-serial` (serial baseline)
  and `/home/free/code/milohax/ghidra-rt-parallel` (parallel, has the mem-fixed jar). CoW reflinks.

## ghidriff (Bank 5 ↔ Bank 8 instruction diff) — assessment

Output: `build/SZBE69_B8/ghidra/ghidriff/`. Keep-ables (~12M): `*.ghidriff.md`, `divergence_index.json`,
`renames.json`. **Byproduct to delete (~6 GB): `json/` (5.2G) + `proj/` (691M) + `gzfs/` (197M).**

- **Useful signal:** instruction-level divergence verdicts for the **2,183 same-symbol functions**
  (1,826 TRUST / 182 CAUTION / 175 MISLEADING) — a precision refinement of `scripts/analysis/bank_divergence.py`
  (which only uses CW-map size deltas). Cross-ref found **27 "false-trust" cases** the size-heuristic misses
  (size says TRUST, ghidriff instruction-diff says MISLEADING; e.g. `__ct__9Transform` 8B stub vs 100B body)
  + 81 agreements. Mostly tiny inlined-away stubs → low-value targets, but real blind-spots.
- **NOT useful:** `renames.json` (cross-name identity recovery) is dominated by noise — "Implied Match"
  pairings at ~0% similarity + trivial-stub byte collisions. Do **not** wire it into anything.
- Net: ghidriff *confirms* the bank-divergence strategy and adds a small precision bump. `divergence_index.json`
  is worth keeping as a per-function reference oracle. Distill helper: `tools/ghidra/distill_ghidriff.py`;
  query: `scripts/analysis/bank_divergence_ghidriff.py` (currently only in worktrees, not main `scripts/analysis/`).

## Other landed this session
- patchdiff fuzzy correlators built + installed (`tools/patchdiff/build_and_install.sh`, fixed 4 build bugs,
  rb3 commit `88927d77`). NOTE: installed to user dir `ghidra_12.1_DEV/Extensions` — **/opt is 12.1.2, so
  verify it actually loads there** (possible version-dir mismatch; unused in a VT run yet).
- VT config: all correlators + 48G heap, validated (rb3 `beb4d975`).
- Benchmark harnesses + 3 design docs (rb3 `bbb7d178`).

## OPEN ITEMS / NEXT STEPS
1. **VT re-run result** — pending (watcher `bjyefv33g`, session `RB3_b5_to_b8_opt`). Compare end-to-end vs 105m.
2. **Restart :8001 service** after the re-run (`tools/ghidra/pyghidra-service.sh start`), clear `cache.db`
   first, then verify VT markup is live via `bin/analyze-function`.
3. **Upstream PR** (user's plan: land in fork, cherry-pick onto an `nsa/master`-rebased branch): the 3
   commits `df874cfe`, `0963a5e9`, `611a8d1d` are each single-file + cherry-pickable. Prep branch + PR body.
4. **Cleanup ~6 GB+:** ghidriff `json/`+`proj/`+`gzfs/`; reflink copies `/home/free/ghidra-vtpatched-full`
   (756M, Tier-1 era), `/tmp/vt-opt-reflink*`. Keep `ghidra-rt-{serial,parallel}` if more A/B is planned.
5. **Optional:** fold ghidriff's 27+81 instruction-level MISLEADING catches into `analyze-function`'s
   bank-divergence warning (closes the size-heuristic blind spot).

## GOTCHAS / DON'T-BREAK
- A **concurrent agent runs a BSim query on `rb3-xenon`** (separate project + the fork's `ghidra_12.2_DEV`
  build) — do NOT kill its JVM or touch `rb3-xenon`.
- VT re-run needs the project lock → **stop :8001 first**; a stale `RB3.lock` after a killed JVM is safe to
  `rm` only after confirming `fuser` shows no holder.
- Build Ghidra/VT with `JAVA_HOME=/usr/lib/jvm/java-26-openjdk` (java-21 here is JRE-only).
- VT session names can't contain `>` and must not already exist (use a fresh `RB3_VT_SESSION_NAME`).
