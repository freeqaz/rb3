//Runnable A/B benchmark + determinism proof for the Ghidra VT *BSim* program correlator
//(BSimProgramCorrelator / BSimProgramCorrelatorMatching -- the discoverPotentialMatches
//O(n_src x n_dest) aggregation stall; see
//rb3/docs/decomp/ghidra-bsim-perf-investigation-2026-06-10.md). Sibling of
//VTRefCorrelatorBenchmarkScript.java and meant to be driven the same way as
//vt_ref_ab.sh / vt_ref_benchmark.sh: run it once per Ghidra install (stock jar vs the
//bsim-perf-candidatecap jar vs future Tier-2 jars) on the SAME small bounded input, then
//grep the BSIM-BENCH-* marker lines and compare correlateMs + the determinism digest.
//
//What it does:
//  1. Opens TWO programs from the CURRENT (scratch!) project by project path
//     (script args). Never point this at the live rb3 / rb3-xenon projects.
//  2. Bounds the input: takes the FIRST maxFuncs functions of each program (entry-point
//     order via FunctionManager.getFunctions(true)) and unions their bodies into the
//     correlator's source/destination address sets. This is the size knob that keeps a
//     smoke run tiny (maxFuncs <= ~500) while preserving the real code path.
//  3. Seeds accepted matches exactly the way the ghidriff driver
//     (ghidriff/bsim.py:correlate_bsim) does: VTMatchInfo(FUNCTION, sim=1.0, conf=1.0)
//     added to session.getManualMatchSet(), then association.setAccepted() on each.
//     Seed pairs are name-identical functions inside the two bounded sets (for an A/B on
//     two copies of the same binary every function name pairs up; we then take an evenly
//     spaced subset, default maxFuncs/10, env BSIM_BENCH_MAX_SEEDS to override) -- this
//     stands in for ghidriff's "exact-stage matches seed BSim" contract.
//  4. Runs the REAL BSim correlator (BSimProgramCorrelatorFactory.createCorrelator with
//     createDefaultOptions(), optional threshold overrides) and times correlate().
//  5. PHASE ATTRIBUTION WITHOUT PATCHING GHIDRA: the BSim phases are not separable from
//     outside the jar, but every phase announces itself via TaskMonitor.setMessage()
//     ("Generating source dictionary", "Binning source functions...", "Zealously
//     over-pairing matches...", "Generating seeds...", "Matching round N...", "Patching
//     holes...", "Adding results to database"). We wrap the script monitor in a
//     delegating WrappingTaskMonitor subclass that timestamps every setMessage /
//     initialize transition and prints BSIM-BENCH-PHASE lines. The serial
//     discoverPotentialMatches aggregation stall is therefore the wall-time between the
//     "Zealously over-pairing matches..." transition and the "Generating seeds..."
//     transition -- zero Ghidra patching needed.
//  6. DETERMINISM DIGEST: SHA-256 over the SORTED lines
//     "srcAddr,destAddr,score,confidence" (hex offsets, %.12f scores) for every VTMatch
//     the correlator produced. Two runs (or two jars) produce identical results iff the
//     digests are equal.
//
//Output markers (grep-able by a wrapper, vt_ref_ab.sh style):
//  BSIM-BENCH-RESULT srcFuncs=N destFuncs=N         (bounded function counts)
//  BSIM-BENCH-RESULT seeds=N                        (accepted seed matches created)
//  BSIM-BENCH-RESULT matches=N                      (matches the correlator produced)
//  BSIM-BENCH-RESULT correlateMs=N                  (wall-clock of correlate())
//  BSIM-BENCH-RESULT digest=<sha256>                (determinism digest, sorted tuples)
//  BSIM-BENCH-PHASE atMs=<t> phaseMs=<d> msg="..."  (per-phase attribution, see above)
//  BSIM-BENCH-TUPLE <srcAddr,destAddr,score,confidence>  (one per match, sorted)
//
//Script args: <srcProgramPath> <destProgramPath> <maxFuncs> [seedConfThreshold]
//             [implicationThreshold]
//  srcProgramPath/destProgramPath: project paths, e.g. /a_src.elf
//  maxFuncs: bound on functions per program (<= ~500 for a smoke run)
//  seedConfThreshold: optional override of "Confidence Threshold for a Seed" (def 10.0)
//  implicationThreshold: optional override of "Confidence Threshold for a Match" (def 0.0)
//Env knobs: BSIM_BENCH_MAX_SEEDS (cap on accepted seeds, default maxFuncs/10 min 8)
//           BSIM_BENCH_MEMORY_MODEL (SMALL|MEDIUM|LARGE, default LARGE = stock default)
//
//Full invocation recipe (SCRATCH project under /tmp -- NEVER rb3 / rb3-xenon):
//
//  GHIDRA_HOME=/home/free/code/milohax/ghidra/build/ghidra-dist/ghidra_12.2_DEV
//  export JAVA_HOME=/usr/lib/jvm/java-26-openjdk
//  export GHIDRA_HEADLESS_MAXMEM=4G
//  PROJ=/tmp/bsim-bench-proj; rm -rf "$PROJ"; mkdir -p "$PROJ"
//  cp /usr/bin/ls /tmp/a_src.elf; cp /usr/bin/ls /tmp/b_dst.elf   # tiny identical pair
//
//  # 1) one-time: import + auto-analyze BOTH programs into the scratch project
//  "$GHIDRA_HOME/support/analyzeHeadless" "$PROJ" bsimbench \
//      -import /tmp/a_src.elf /tmp/b_dst.elf
//
//  # 2) benchmark (repeat per Ghidra install / per jar for the A/B; -readOnly +
//  #    -noanalysis keep the programs untouched, so step 1 need not be re-run)
//  "$GHIDRA_HOME/support/analyzeHeadless" "$PROJ" bsimbench \
//      -process a_src.elf -noanalysis -readOnly \
//      -scriptPath /home/free/code/milohax/rb3/scripts/ghidra \
//      -postScript BSimCorrelatorBenchmarkScript.java /a_src.elf /b_dst.elf 300
//
//A/B: run step 2 once with GHIDRA_HOME = the stock dist and once with the
//candidate-cap dist (they differ only in VersionTrackingBSim.jar), then compare
//"BSIM-BENCH-RESULT correlateMs=" and "digest=" lines. Re-run >=3x per jar to check
//determinism (digest must be identical across reps of the same jar).
//
//@category VersionTracking.Benchmark
//@menupath

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import generic.lsh.LSHMemoryModel;
import ghidra.app.script.GhidraScript;
import ghidra.feature.vt.api.BSimProgramCorrelatorFactory;
import ghidra.feature.vt.api.db.VTSessionDB;
import ghidra.feature.vt.api.main.VTAssociation;
import ghidra.feature.vt.api.main.VTAssociationType;
import ghidra.feature.vt.api.main.VTMatch;
import ghidra.feature.vt.api.main.VTMatchInfo;
import ghidra.feature.vt.api.main.VTMatchSet;
import ghidra.feature.vt.api.main.VTProgramCorrelator;
import ghidra.feature.vt.api.main.VTScore;
import ghidra.feature.vt.api.util.VTOptions;
import ghidra.framework.model.DomainFile;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Program;
import ghidra.util.task.TaskMonitor;
import ghidra.util.task.WrappingTaskMonitor;

public class BSimCorrelatorBenchmarkScript extends GhidraScript {

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 3) {
			printerr("Usage: BSimCorrelatorBenchmarkScript.java <srcProgramPath> " +
				"<destProgramPath> <maxFuncs> [seedConfThreshold] [implicationThreshold]");
			return;
		}
		String srcPath = args[0];
		String destPath = args[1];
		int maxFuncs = Integer.parseInt(args[2]);
		Double seedConfOverride = args.length > 3 ? Double.valueOf(args[3]) : null;
		Double impThreshOverride = args.length > 4 ? Double.valueOf(args[4]) : null;

		Program source = openProgramByPath(srcPath);
		Program dest = null;
		VTSessionDB session = null;
		try {
			dest = openProgramByPath(destPath);

			println("BSIM-BENCH: source=" + srcPath + " (" + source.getLanguageID() + ")");
			println("BSIM-BENCH: dest=" + destPath + " (" + dest.getLanguageID() + ")");

			// --- bound both inputs to the first maxFuncs functions ---
			List<Function> srcFuncs = firstFunctions(source, maxFuncs);
			List<Function> destFuncs = firstFunctions(dest, maxFuncs);
			AddressSetView srcSet = unionBodies(srcFuncs);
			AddressSetView destSet = unionBodies(destFuncs);
			println("BSIM-BENCH-RESULT srcFuncs=" + srcFuncs.size() + " destFuncs=" +
				destFuncs.size());

			session = VTSessionDB.createVTSession("BSim Bench Session", source, dest, this);

			// --- seed accepted matches the way ghidriff/bsim.py does ---
			int seeds = seedAcceptedNameMatches(session, srcFuncs, destFuncs, maxFuncs);
			println("BSIM-BENCH-RESULT seeds=" + seeds);

			// --- options: factory defaults, mirroring bsim.py, plus overrides ---
			BSimProgramCorrelatorFactory factory = new BSimProgramCorrelatorFactory();
			VTOptions options = factory.createDefaultOptions();
			if (seedConfOverride != null) {
				options.setDouble(BSimProgramCorrelatorFactory.SEED_CONF_THRESHOLD,
					seedConfOverride.doubleValue());
			}
			if (impThreshOverride != null) {
				options.setDouble(BSimProgramCorrelatorFactory.IMPLICATION_THRESHOLD,
					impThreshOverride.doubleValue());
			}
			String memModelEnv = System.getenv("BSIM_BENCH_MEMORY_MODEL");
			if (memModelEnv != null && !memModelEnv.isBlank()) {
				options.setEnum(BSimProgramCorrelatorFactory.MEMORY_MODEL,
					LSHMemoryModel.valueOf(memModelEnv.trim().toUpperCase()));
			}
			println("BSIM-BENCH: options seedConf=" +
				options.getDouble(BSimProgramCorrelatorFactory.SEED_CONF_THRESHOLD,
					BSimProgramCorrelatorFactory.SEED_CONF_THRESHOLD_DEFAULT) +
				" impThreshold=" +
				options.getDouble(BSimProgramCorrelatorFactory.IMPLICATION_THRESHOLD,
					BSimProgramCorrelatorFactory.IMPLICATION_THRESHOLD_DEFAULT) +
				" memoryModel=" +
				options.getEnum(BSimProgramCorrelatorFactory.MEMORY_MODEL,
					BSimProgramCorrelatorFactory.MEMORY_MODEL_DEFAULT));

			VTProgramCorrelator correlator =
				factory.createCorrelator(source, srcSet, dest, destSet, options);

			// --- run + time correlate() under the phase-timing monitor ---
			PhaseTimingMonitor phaseMonitor = new PhaseTimingMonitor(monitor);
			int tx = session.startTransaction("BSim bench correlate");
			boolean ok = false;
			VTMatchSet resultSet;
			long correlateMs;
			try {
				long t0 = System.nanoTime();
				resultSet = correlator.correlate(session, phaseMonitor);
				correlateMs = (System.nanoTime() - t0) / 1_000_000L;
				ok = true;
			}
			finally {
				session.endTransaction(tx, ok);
			}
			phaseMonitor.finish();

			// --- determinism digest over the sorted canonical match tuples ---
			List<String> tuples = new ArrayList<>();
			for (VTMatch m : resultSet.getMatches()) {
				VTAssociation a = m.getAssociation();
				VTScore sim = m.getSimilarityScore();
				VTScore conf = m.getConfidenceScore();
				tuples.add(String.format("%08x,%08x,%.12f,%.12f",
					a.getSourceAddress().getOffset(), a.getDestinationAddress().getOffset(),
					sim == null ? Double.NaN : sim.getScore(),
					conf == null ? Double.NaN : conf.getScore()));
			}
			Collections.sort(tuples);
			String digest = sha256(String.join("\n", tuples));

			println("BSIM-BENCH-RESULT matches=" + tuples.size());
			println("BSIM-BENCH-RESULT correlateMs=" + correlateMs);
			println("BSIM-BENCH-RESULT digest=" + digest);
			for (String t : tuples) {
				println("BSIM-BENCH-TUPLE " + t);
			}
		}
		finally {
			if (session != null) {
				session.release(this);
			}
			if (dest != null) {
				dest.release(this);
			}
			source.release(this);
		}
	}

	private Program openProgramByPath(String path) throws Exception {
		DomainFile df = state.getProject().getProjectData().getFile(path);
		if (df == null) {
			throw new IllegalArgumentException("No domain file at project path: " + path);
		}
		Object obj = df.getDomainObject(this, false, false, monitor);
		if (!(obj instanceof Program)) {
			((ghidra.framework.model.DomainObject) obj).release(this);
			throw new IllegalArgumentException("Not a program: " + path);
		}
		return (Program) obj;
	}

	/** First maxFuncs functions in entry-point order — the bounded-input knob. */
	private List<Function> firstFunctions(Program p, int maxFuncs) {
		List<Function> out = new ArrayList<>(Math.min(maxFuncs, 4096));
		FunctionIterator it = p.getFunctionManager().getFunctions(true);
		while (it.hasNext() && out.size() < maxFuncs) {
			out.add(it.next());
		}
		return out;
	}

	private AddressSetView unionBodies(List<Function> funcs) {
		AddressSet set = new AddressSet();
		for (Function f : funcs) {
			set.add(f.getBody());
		}
		return set;
	}

	/**
	 * Mirror ghidriff/bsim.py:correlate_bsim seeding: add FUNCTION matches (sim=1.0,
	 * conf=1.0) to the session's manual match set, then setAccepted() each association.
	 * Seed pairs = name-identical functions across the two bounded lists, thinned to an
	 * evenly spaced subset so the correlator still has work to discover.
	 */
	private int seedAcceptedNameMatches(VTSessionDB session, List<Function> srcFuncs,
			List<Function> destFuncs, int maxFuncs) throws Exception {

		// Only names UNIQUE on both sides may seed: thunks duplicate their target's name,
		// and a duplicate would pair two source funcs to one dest func -- the second
		// association is then BLOCKED and setAccepted() throws.
		Map<String, Function> destByName = uniqueByName(destFuncs);
		Map<String, Function> srcByName = uniqueByName(srcFuncs);
		List<Function[]> pairs = new ArrayList<>();
		for (Function sf : srcFuncs) {
			Function df = destByName.get(sf.getName());
			if (df != null && srcByName.containsKey(sf.getName())) {
				pairs.add(new Function[] { sf, df });
			}
		}

		int maxSeeds = intEnv("BSIM_BENCH_MAX_SEEDS", Math.max(8, maxFuncs / 10));
		List<Function[]> chosen;
		if (pairs.size() <= maxSeeds) {
			chosen = pairs;
		}
		else {
			chosen = new ArrayList<>(maxSeeds);
			double stride = (double) pairs.size() / maxSeeds;
			for (int i = 0; i < maxSeeds; i++) {
				chosen.add(pairs.get((int) (i * stride)));
			}
		}

		int accepted = 0;
		int tx = session.startTransaction("seed accepted matches");
		boolean ok = false;
		try {
			VTMatchSet manual = session.getManualMatchSet();
			for (Function[] pair : chosen) {
				VTMatchInfo info = new VTMatchInfo(manual);
				info.setAssociationType(VTAssociationType.FUNCTION);
				info.setSourceAddress(pair[0].getEntryPoint());
				info.setDestinationAddress(pair[1].getEntryPoint());
				info.setSourceLength((int) pair[0].getBody().getNumAddresses());
				info.setDestinationLength((int) pair[1].getBody().getNumAddresses());
				info.setSimilarityScore(new VTScore(1.0));
				info.setConfidenceScore(new VTScore(1.0));
				info.setTag(null);
				VTMatch match = manual.addMatch(info);
				// Guard: a conflicting association (shouldn't happen post unique-name
				// filter, but stay deterministic + non-fatal if it does) would be BLOCKED.
				if (match.getAssociation()
						.getStatus() == ghidra.feature.vt.api.main.VTAssociationStatus.AVAILABLE) {
					match.getAssociation().setAccepted();
					accepted++;
				}
			}
			ok = true;
		}
		finally {
			session.endTransaction(tx, ok);
		}
		return accepted;
	}

	/** Map of name -&gt; function for names appearing EXACTLY once in the list. */
	private Map<String, Function> uniqueByName(List<Function> funcs) {
		Map<String, Function> byName = new LinkedHashMap<>();
		java.util.Set<String> dupes = new java.util.HashSet<>();
		for (Function f : funcs) {
			if (byName.putIfAbsent(f.getName(), f) != null) {
				dupes.add(f.getName());
			}
		}
		byName.keySet().removeAll(dupes);
		return byName;
	}

	private int intEnv(String name, int def) {
		String e = System.getenv(name);
		if (e == null || e.isBlank()) {
			return def;
		}
		return Integer.parseInt(e.trim());
	}

	private static String sha256(String s) throws Exception {
		MessageDigest md = MessageDigest.getInstance("SHA-256");
		byte[] d = md.digest(s.getBytes(StandardCharsets.UTF_8));
		StringBuilder sb = new StringBuilder(d.length * 2);
		for (byte b : d) {
			sb.append(Character.forDigit((b >> 4) & 0xF, 16));
			sb.append(Character.forDigit(b & 0xF, 16));
		}
		return sb.toString();
	}

	/**
	 * Delegating monitor that timestamps every setMessage transition (and logs
	 * initialize() maxima), giving per-phase wall-time attribution for the BSim
	 * correlator WITHOUT patching Ghidra. Synchronized: ParallelDecompiler /
	 * ConcurrentQ workers may touch the monitor from multiple threads.
	 */
	private class PhaseTimingMonitor extends WrappingTaskMonitor {
		private final long start = System.nanoTime();
		private long phaseStart = start;
		private String phaseMsg = "<start>";

		PhaseTimingMonitor(TaskMonitor delegate) {
			super(delegate);
		}

		@Override
		public void setMessage(String message) {
			transition(message);
			super.setMessage(message);
		}

		@Override
		public void initialize(long max) {
			synchronized (this) {
				println(String.format("BSIM-BENCH-PHASE-INIT atMs=%d max=%d during=\"%s\"",
					(System.nanoTime() - start) / 1_000_000L, max, phaseMsg));
			}
			super.initialize(max);
		}

		private synchronized void transition(String message) {
			if (message == null || message.equals(phaseMsg)) {
				return;
			}
			long now = System.nanoTime();
			println(String.format("BSIM-BENCH-PHASE atMs=%d phaseMs=%d msg=\"%s\"",
				(now - start) / 1_000_000L, (now - phaseStart) / 1_000_000L, phaseMsg));
			phaseMsg = message;
			phaseStart = now;
		}

		/** Close out the final phase (call after correlate() returns). */
		synchronized void finish() {
			long now = System.nanoTime();
			println(String.format("BSIM-BENCH-PHASE atMs=%d phaseMs=%d msg=\"%s\" (final)",
				(now - start) / 1_000_000L, (now - phaseStart) / 1_000_000L, phaseMsg));
		}
	}
}
