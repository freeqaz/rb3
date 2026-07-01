//BSim LSH bin audit: recall stress-test for the Tier-1 candidate-cap prototype
//(branch bsim-perf-candidatecap: BinningSystem.lookup returns Collections.emptySet()
//when the bin union exceeds MAX_LOOKUP_CANDIDATES=500).
//
//WHAT IT MEASURES
//  Replicates BinningSystem's LSH binning EXACTLY via the public generic.lsh API
//  (same Random(23) seed, same int[L][k] identity construction order, same
//  Partition.hash over LSHVector.getEntries()), generates real BSim signature
//  vectors via the public DecompInterface.generateSignatures +
//  WeightedLSHCosineVectorFactory path (mirroring BSimProgramCorrelator), then reports:
//    (a) bin-occupancy histogram across all L tables
//    (b) per-query candidate-union size distribution (the value the cap tests)
//    (c) how many queries exceed each cap in {100,250,500,1000,2000,5000}
//    (d) GROUND TRUTH (when source and dest programs share mangled names, e.g.
//        Bank5 vs Bank8, or SELF mode): for every same-named pair, the true-pair
//        cosine similarity, whether the true counterpart IS in the query's union,
//        whether the 500-cap empty-set return would DROP it, and the true pair's
//        similarity RANK within the union (rank 1 = it would have been the top match).
//    (e) the same recall numbers under an alternative PER-BIN-SKIP policy
//        (skip only individual bins with occupancy > B; keep sparse bins), which is
//        the exact-result-preserving fix candidate.
//
//WHY (analytic context, verified against the dist jars 2026-06-10):
//  The investigation doc justified the cap with "LARGE model (k=16, L=5, tau=0.97),
//  expected candidates = L*n/2^k ~= 3, so >500 means 100x expected density".
//  That reading is WRONG: LSHMemoryModel's ctor is (label, k, probabilityThreshold,
//  tauBound) = LARGE("...", 16, 0.97, 0.75), i.e. tau=0.75 / probThresh=0.97, and
//  KandL.memoryModelToL(LARGE) = 229 (MEDIUM: k=13 L=104; SMALL: k=10 L=47).
//  Expected union at n_src=40k (uniform bins): LARGE ~140, MEDIUM ~508, SMALL ~1836.
//  So cap=500 is only ~3.6x the design-mean union on LARGE and BELOW the mean on
//  MEDIUM/SMALL. A query needs only ~2.2 average co-occupants per table (over 229
//  tables) to exceed 500 — easily reached by template-instantiation / idiom clusters
//  in a C++ game binary even when the query has a near-perfect (sim~1.0) true match
//  that would rank #1. This script quantifies that recall loss empirically.
//
//HOW TO RUN (headless; NEVER against the live rb3/rb3-xenon projects or :8001):
//  1) Create a NEW scratch project under /tmp and import both programs, e.g. the
//     Bank5 DWARF ELF as both source and dest (SELF mode), or Bank5 + Bank8 builds:
//
//     DIST=/home/free/code/milohax/ghidra/build/ghidra-dist/ghidra_12.2_DEV
//     export JAVA_HOME=/usr/lib/jvm/java-26-openjdk
//     ELF='/home/free/code/milohax/milo-executable-library/rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.elf'
//     "$DIST/support/analyzeHeadless" /tmp/bsim-bin-audit proj \
//        -import "$ELF" -processor "PowerPC:BE:32:default" \
//        -scriptPath /home/free/code/milohax/rb3/scripts/ghidra \
//        -postScript BSimBinAuditScript.java SELF 1500 500 LARGE 10
//
//     (Initial auto-analysis of the full ELF is heavy; to bound it, add
//      -analysisTimeoutPerFile 900, or pre-import once and re-run with -process
//      instead of -import on subsequent invocations.)
//
//  2) Two-program (real Bank5-vs-Bank8 ground truth): import both into the scratch
//     project, then -process the DEST program and pass the source's project path:
//
//     "$DIST/support/analyzeHeadless" /tmp/bsim-bin-audit proj \
//        -process band_r_wii.elf -noanalysis \
//        -scriptPath /home/free/code/milohax/rb3/scripts/ghidra \
//        -postScript BSimBinAuditScript.java /bank8_target 1500 500 LARGE 10
//
//  Script args: [0] source program project path, or SELF   (default SELF)
//               [1] max functions per side                  (default 1500)
//               [2] cap to emulate (prototype = 500)        (default 500)
//               [3] memory model SMALL|MEDIUM|LARGE         (default LARGE)
//               [4] per-function decompile timeout, seconds (default 10)
//
//  All result lines are prefixed BSIM-BIN-AUDIT for grepping.
//
//PRIOR SYNTHETIC EVIDENCE (2026-06-10, real Ghidra LSH machinery, no programs;
//harnesses at /tmp/bsim-audit-check/CapRecallSim{,2,3}.java, javac'd against the
//dist jars):
//  - SMALL model, n_src=10k, near-orthogonal vectors: mean union 449-498; 20% of
//    probes exceeded cap=500 and EVERY dropped true match (sim ~0.94) was RANK 1.
//    At n_src=40k the SMALL design-mean union is 1836 >> 500: total recall collapse.
//  - MEDIUM, n_src=40k: design-mean union is 508 > 500 -> the cap empties the
//    MAJORITY of queries. (Measured 127 mean at 10k; scales ~linearly with n.)
//  - LARGE, n_src=40k-equivalent: distinctive functions mean union 136, ZERO drops
//    (cap safe there). BUT a 600-member near-duplicate family at mutual sim only
//    ~0.53 pushed every member's union to ~729 -> ALL 600 true matches (mean sim
//    0.934, 589/600 at RANK 1) dropped. A 300-member family (union 434) survived.
//    And addExternalFunctions gives ALL externals one identical vector
//    (0xfade5eed) -> a mega-bin (union 1332 at 1200 externals) that is always
//    dropped and poisons colliding queries.
//  CONCLUSION the cap=500 EMPTY-SET semantics is NOT lossless: it loses rank-1
//  high-similarity matches for large similar-function families (template/stub
//  families -- common in RB3) and is catastrophic for MEDIUM/SMALL. Prefer a
//  per-bin skip (degenerate-bin threshold) and/or a model/n-scaled cap
//  (e.g. max(2000, 10 * L*n/2^k)) over whole-union empty-return.
//
//@category BSim.Audit
//@menupath

import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.BitSet;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.TreeMap;

import generic.jar.ResourceFile;
import generic.lsh.KandL;
import generic.lsh.LSHMemoryModel;
import generic.lsh.Partition;
import generic.lsh.vector.LSHVector;
import generic.lsh.vector.VectorCompare;
import generic.lsh.vector.WeightedLSHCosineVectorFactory;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.signature.SignatureResult;
import ghidra.app.script.GhidraScript;
import ghidra.features.bsim.query.GenSignatures;
import ghidra.framework.model.DomainFile;
import ghidra.program.model.lang.LanguageID;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Program;
import ghidra.util.xml.SpecXmlUtils;
import ghidra.xml.NonThreadedXmlPullParserImpl;
import ghidra.xml.XmlPullParser;

public class BSimBinAuditScript extends GhidraScript {

	// Confidence floor for calling a same-named pair a "true match BSim should keep".
	// BSim's own secondary filter is SIMILARITY_THRESHOLD = 0.5; tau for the binning
	// design is 0.75; we report recall at both >=0.5 and >=0.75 plus a >=0.95
	// "near-duplicate" tier.
	private static final double[] SIM_TIERS = { 0.5, 0.75, 0.95 };

	// Caps to emulate (the prototype's value is 500).
	private static final int[] CAPS = { 100, 250, 500, 1000, 2000, 5000 };

	// Per-bin-skip alternative policy: skip only single bins whose occupancy exceeds
	// this (degenerate hash pileups), instead of nuking the whole union.
	private static final int PER_BIN_SKIP_B = 200;

	private static class Sig {
		final String name;
		final LSHVector vec;
		Sig(String name, LSHVector vec) {
			this.name = name;
			this.vec = vec;
		}
	}

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		String sourcePath = args.length > 0 ? args[0] : "SELF";
		int maxFuncs = args.length > 1 ? Integer.parseInt(args[1]) : 1500;
		int mainCap = args.length > 2 ? Integer.parseInt(args[2]) : 500;
		LSHMemoryModel model =
			args.length > 3 ? LSHMemoryModel.valueOf(args[3]) : LSHMemoryModel.LARGE;
		int decompTimeout = args.length > 4 ? Integer.parseInt(args[4]) : 10;

		boolean selfMode = sourcePath.equalsIgnoreCase("SELF");
		Program destProgram = currentProgram;
		Program sourceProgram = null;
		Object consumer = new Object();
		try {
			if (selfMode) {
				sourceProgram = destProgram;
			}
			else {
				DomainFile df = state.getProject().getProjectData().getFile(sourcePath);
				if (df == null) {
					printerr("BSIM-BIN-AUDIT: source program not found at project path: " +
						sourcePath);
					return;
				}
				sourceProgram = (Program) df.getDomainObject(consumer, true, false, monitor);
			}

			// --- weights / vector factory, mirroring BSimProgramCorrelator.doCorrelate ---
			LanguageID id1 = sourceProgram.getLanguageID();
			LanguageID id2 = destProgram.getLanguageID();
			ResourceFile weightsFile = GenSignatures.getWeightsFile(id1, id2);
			if (weightsFile == null) {
				printerr("BSIM-BIN-AUDIT: no weights file for " + id1 + " vs " + id2);
				return;
			}
			WeightedLSHCosineVectorFactory vectorFactory = new WeightedLSHCosineVectorFactory();
			try (InputStream input = weightsFile.getInputStream()) {
				XmlPullParser parser = new NonThreadedXmlPullParserImpl(input,
					"Vector weights parser", SpecXmlUtils.getXmlHandler(), false);
				vectorFactory.readWeights(parser);
			}
			println("BSIM-BIN-AUDIT weightsFile=" + weightsFile.getName() + " settings=" +
				vectorFactory.getSettings());

			// --- LSH parameters: the REAL k and L (computed by Ghidra's own KandL) ---
			int k = model.getK();
			int L = KandL.memoryModelToL(model);
			println("BSIM-BIN-AUDIT model=" + model.name() + " k=" + k + " L=" + L +
				" tauBound=" + model.getTauBound() + " probThresh=" +
				model.getProbabilityThreshold());

			// --- signature generation (bounded, serial) ---
			println("BSIM-BIN-AUDIT generating source signatures (max " + maxFuncs + ")...");
			List<Sig> sourceSigs =
				generateSignatures(sourceProgram, vectorFactory, maxFuncs, decompTimeout);
			List<Sig> destSigs;
			if (selfMode) {
				destSigs = sourceSigs;
			}
			else {
				println("BSIM-BIN-AUDIT generating dest signatures (max " + maxFuncs + ")...");
				destSigs = generateSignatures(destProgram, vectorFactory, maxFuncs, decompTimeout);
			}
			println("BSIM-BIN-AUDIT nSource=" + sourceSigs.size() + " nDest=" + destSigs.size());
			if (sourceSigs.isEmpty() || destSigs.isEmpty()) {
				printerr("BSIM-BIN-AUDIT: no signatures generated; aborting");
				return;
			}

			// --- binning replica: EXACT BinningSystem construction order ---
			// BinningSystem ctor: Random(23); for ii in 0..L-1 { identities[ii][jj]=nextInt; }
			int[][] partitionIdentities = new int[L][];
			Random random = new Random(23);
			for (int ii = 0; ii < L; ++ii) {
				partitionIdentities[ii] = new int[k];
				for (int jj = 0; jj < k; ++jj) {
					partitionIdentities[ii][jj] = random.nextInt();
				}
			}

			// binTables[ii]: binId -> list of source indices (BinningSystem uses
			// TreeMap<Integer,TreeSet<FunctionNode>>; sets dedupe by address — our indices
			// are unique so a list is equivalent).
			List<TreeMap<Integer, List<Integer>>> binTables = new ArrayList<>(L);
			for (int ii = 0; ii < L; ++ii) {
				binTables.add(new TreeMap<>());
			}
			int[][] sourceBinIds = new int[sourceSigs.size()][];
			for (int s = 0; s < sourceSigs.size(); s++) {
				monitor.checkCancelled();
				int[] ids = binIds(partitionIdentities, sourceSigs.get(s).vec, L);
				sourceBinIds[s] = ids;
				for (int ii = 0; ii < L; ++ii) {
					binTables.get(ii).computeIfAbsent(ids[ii], x -> new ArrayList<>()).add(s);
				}
			}

			// --- (a) bin-occupancy histogram across all tables ---
			List<Integer> occ = new ArrayList<>();
			int degenerateBins = 0;
			for (TreeMap<Integer, List<Integer>> table : binTables) {
				for (List<Integer> bin : table.values()) {
					occ.add(bin.size());
					if (bin.size() > PER_BIN_SKIP_B) {
						degenerateBins++;
					}
				}
			}
			int[] occArr = occ.stream().mapToInt(Integer::intValue).toArray();
			Arrays.sort(occArr);
			println("BSIM-BIN-AUDIT binOccupancy bins=" + occArr.length + " p50=" +
				pct(occArr, 50) + " p90=" + pct(occArr, 90) + " p99=" + pct(occArr, 99) +
				" max=" + (occArr.length == 0 ? 0 : occArr[occArr.length - 1]) +
				" degenerateBins(>" + PER_BIN_SKIP_B + ")=" + degenerateBins);

			// --- ground truth name map (first occurrence wins, mirroring uniqueness) ---
			Map<String, Integer> sourceByName = new HashMap<>();
			for (int s = 0; s < sourceSigs.size(); s++) {
				sourceByName.putIfAbsent(sourceSigs.get(s).name, s);
			}

			// --- (b)+(c)+(d)+(e): per-query union audit ---
			int[] unionSizes = new int[destSigs.size()];
			int[] overCap = new int[CAPS.length];
			// recall tallies per sim tier: [tier][0]=truePairs, [1]=trueInUnion,
			// [2]=droppedByMainCap, [3]=droppedByPerBinSkip, [4]=rank1AmongDropped
			long[][] tier = new long[SIM_TIERS.length][5];
			List<String> droppedExamples = new ArrayList<>();
			VectorCompare veccompare = new VectorCompare();

			for (int q = 0; q < destSigs.size(); q++) {
				monitor.checkCancelled();
				Sig query = destSigs.get(q);
				int[] qIds = binIds(partitionIdentities, query.vec, L);

				BitSet union = new BitSet(sourceSigs.size());
				BitSet unionSkip = new BitSet(sourceSigs.size());   // per-bin-skip policy
				for (int ii = 0; ii < L; ++ii) {
					List<Integer> bin = binTables.get(ii).get(qIds[ii]);
					if (bin == null) {
						continue;
					}
					for (int s : bin) {
						union.set(s);
					}
					if (bin.size() <= PER_BIN_SKIP_B) {
						for (int s : bin) {
							unionSkip.set(s);
						}
					}
				}
				int uSize = union.cardinality();
				unionSizes[q] = uSize;
				for (int c = 0; c < CAPS.length; c++) {
					if (uSize > CAPS[c]) {
						overCap[c]++;
					}
				}

				// ground truth
				Integer trueSrc = sourceByName.get(query.name);
				if (trueSrc == null) {
					continue;
				}
				double trueSim =
					sourceSigs.get(trueSrc).vec.compare(query.vec, veccompare);
				boolean inUnion = union.get(trueSrc);
				boolean droppedMain = inUnion && uSize > mainCap;   // empty-set semantics
				boolean droppedSkip = !unionSkip.get(trueSrc) && inUnion;

				int rank = -1;
				if (droppedMain) {
					// similarity rank of the true pair within the union it was dropped from
					rank = 1;
					for (int s = union.nextSetBit(0); s >= 0; s = union.nextSetBit(s + 1)) {
						if (s == trueSrc) {
							continue;
						}
						if (sourceSigs.get(s).vec.compare(query.vec, veccompare) > trueSim) {
							rank++;
						}
					}
				}

				for (int t = 0; t < SIM_TIERS.length; t++) {
					if (trueSim < SIM_TIERS[t]) {
						continue;
					}
					tier[t][0]++;
					if (inUnion) {
						tier[t][1]++;
					}
					if (droppedMain) {
						tier[t][2]++;
						if (rank == 1) {
							tier[t][4]++;
						}
					}
					if (droppedSkip) {
						tier[t][3]++;
					}
				}
				if (droppedMain && trueSim >= 0.75 && droppedExamples.size() < 25) {
					droppedExamples.add(String.format("%s sim=%.3f union=%d rank=%d",
						query.name, trueSim, uSize, rank));
				}
			}

			// --- report ---
			int[] us = unionSizes.clone();
			Arrays.sort(us);
			double meanUnion = Arrays.stream(us).average().orElse(0);
			println(String.format(
				"BSIM-BIN-AUDIT unionSize mean=%.1f p50=%d p90=%d p99=%d max=%d  (uniform-design mean for this n: %.1f)",
				meanUnion, pct(us, 50), pct(us, 90), pct(us, 99),
				us.length == 0 ? 0 : us[us.length - 1],
				(double) L * sourceSigs.size() / Math.pow(2, k)));
			for (int c = 0; c < CAPS.length; c++) {
				println(String.format("BSIM-BIN-AUDIT overCap cap=%d queries=%d (%.1f%%)",
					CAPS[c], overCap[c], 100.0 * overCap[c] / destSigs.size()));
			}
			for (int t = 0; t < SIM_TIERS.length; t++) {
				long truePairs = tier[t][0];
				println(String.format(
					"BSIM-BIN-AUDIT recall simTier>=%.2f truePairs=%d inUnion=%d " +
						"droppedByCap%d=%d (%.2f%%) rank1AmongDropped=%d droppedByPerBinSkip(B=%d)=%d (%.2f%%)",
					SIM_TIERS[t], truePairs, tier[t][1], mainCap, tier[t][2],
					truePairs == 0 ? 0 : 100.0 * tier[t][2] / truePairs, tier[t][4],
					PER_BIN_SKIP_B, tier[t][3],
					truePairs == 0 ? 0 : 100.0 * tier[t][3] / truePairs));
			}
			for (String ex : droppedExamples) {
				println("BSIM-BIN-AUDIT droppedExample " + ex);
			}
			println("BSIM-BIN-AUDIT done");
		}
		finally {
			if (sourceProgram != null && sourceProgram != destProgram) {
				sourceProgram.release(consumer);
			}
		}
	}

	/**
	 * Exact replica of BinningSystem.getBinIds(): Partition.hash of the vector's
	 * HashEntry[] against each table's identity row.
	 */
	private static int[] binIds(int[][] partitionIdentities, LSHVector vec, int L) {
		int[] result = new int[L];
		for (int ii = 0; ii < L; ++ii) {
			result[ii] = Partition.hash(partitionIdentities[ii], vec.getEntries());
		}
		return result;
	}

	/**
	 * Generate BSim signature vectors for the first maxFuncs non-thunk functions,
	 * mirroring BSimProgramCorrelator's DecompilerFactory + ParallelDecompilerCallback
	 * (serial here; the audit is bounded so parallelism is unnecessary).
	 */
	private List<Sig> generateSignatures(Program program,
			WeightedLSHCosineVectorFactory vectorFactory, int maxFuncs, int timeoutSecs)
			throws Exception {
		List<Sig> result = new ArrayList<>();
		DecompInterface decompiler = new DecompInterface();
		try {
			DecompileOptions options = new DecompileOptions();
			options.setNoCastPrint(true);
			options.setDefaultTimeout(timeoutSecs);
			decompiler.setOptions(options);
			decompiler.setSignatureSettings(vectorFactory.getSettings());
			if (!decompiler.openProgram(program)) {
				throw new Exception("decompiler open failed: " + decompiler.getLastMessage());
			}
			FunctionIterator iter = program.getFunctionManager().getFunctions(true);
			int failures = 0;
			while (iter.hasNext() && result.size() < maxFuncs) {
				monitor.checkCancelled();
				Function func = iter.next();
				if (func.isThunk() || func.isExternal()) {
					continue;
				}
				SignatureResult sigres =
					decompiler.generateSignatures(func, false, timeoutSecs, monitor);
				if (sigres == null || sigres.features == null || sigres.features.length == 0) {
					failures++;
					continue;
				}
				LSHVector vec = vectorFactory.buildVector(sigres.features);
				if (vec == null || vec.getEntries() == null || vec.getEntries().length == 0) {
					failures++;
					continue;
				}
				result.add(new Sig(func.getName(), vec));
				if (result.size() % 250 == 0) {
					println("BSIM-BIN-AUDIT   ..." + result.size() + " signatures (" +
						program.getName() + ")");
				}
			}
			println("BSIM-BIN-AUDIT signatures program=" + program.getName() + " ok=" +
				result.size() + " failed=" + failures);
		}
		finally {
			decompiler.dispose();
		}
		return result;
	}

	private static int pct(int[] sorted, int p) {
		if (sorted.length == 0) {
			return 0;
		}
		int idx = (int) Math.ceil(p / 100.0 * sorted.length) - 1;
		if (idx < 0) {
			idx = 0;
		}
		if (idx >= sorted.length) {
			idx = sorted.length - 1;
		}
		return sorted[idx];
	}
}
