//Reproducible end-to-end benchmark + correctness/determinism proof for the Ghidra
//Version-Tracking *Reference* correlator's O(dest x src) cosine-similarity scoring loop
//(VTAbstractReferenceProgramCorrelator.findDestinations). This is the Tier-2 parallel
//target. The script builds two throwaway PowerPC programs whose CALLER functions each
//call a deterministic subset of a shared set of ANCHOR functions, seeds the anchor pairs
//as ACCEPTED FUNCTION matches (so extractReferenceFeatures produces non-trivial src/dest
//LSH vectors), then runs the REAL Function Reference correlator via the VT API. It dumps
//the resulting matches as a sorted, canonical (srcOff,destOff,similarity,confidence,
//srcLen,dstLen) tuple list (so the serial baseline jar and the parallel jar can be diffed
//byte-for-byte) plus the wall-clock of the scoring+commit phase.
//
//Runs against whatever VersionTracking.jar is installed in the Ghidra it launches under,
//so re-running after swapping the jar measures that jar's behavior for the SAME inputs.
//
//Output lines:
//  VT-REF-BENCH-RESULT n=...            (number of caller functions per program)
//  VT-REF-BENCH-RESULT anchors=...      (number of accepted-match anchors)
//  VT-REF-BENCH-RESULT matches=...      (number of matches the correlator produced)
//  VT-REF-BENCH-RESULT correlateMs=...  (wall-clock of correlator.correlate())
//  VT-REF-BENCH-RESULT digest=<sha256>  (digest of the canonical sorted tuple list)
//  VT-REF-BENCH-TUPLE <srcOff> <destOff> <sim> <conf> <srcLen> <dstLen>  (one per match)
//
//@category VersionTracking.Benchmark
//@menupath

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.feature.vt.api.correlator.program.FunctionReferenceProgramCorrelatorFactory;
import ghidra.feature.vt.api.db.VTSessionDB;
import ghidra.feature.vt.api.main.VTAssociation;
import ghidra.feature.vt.api.main.VTAssociationType;
import ghidra.feature.vt.api.main.VTMatch;
import ghidra.feature.vt.api.main.VTMatchInfo;
import ghidra.feature.vt.api.main.VTMatchSet;
import ghidra.feature.vt.api.main.VTProgramCorrelator;
import ghidra.feature.vt.api.main.VTProgramCorrelatorFactory;
import ghidra.feature.vt.api.main.VTScore;
import ghidra.feature.vt.api.main.VTSession;
import ghidra.program.database.ProgramDB;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.lang.Language;
import ghidra.program.model.lang.LanguageID;
import ghidra.program.model.listing.Program;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.util.DefaultLanguageService;

public class VTRefCorrelatorBenchmarkScript extends GhidraScript {

	// Standard big-endian 32-bit PowerPC. "bl"/"blr"/call-reference semantics (all the
	// reference correlator needs) are identical to the RB3 Gekko/Broadway variant, and this
	// id is present in a stock Ghidra install (the Gekko variant is a fork-only language).
	private static final String LANGUAGE_ID = "PowerPC:BE:32:default";

	// Anchor functions live in a low block; caller functions in a higher block. A PPC
	// "bl" has a +/-32MB signed reach, so both blocks fit comfortably in one 0x100000-ish
	// span. Each anchor is a single "blr" (0x4E800020). Each caller is a run of "bl
	// <anchor>" call instructions followed by "blr".
	private static final long ANCHOR_BASE = 0x100000L;
	private static final int ANCHOR_STRIDE = 0x10;   // 16 bytes per anchor (blr + pad)
	private static final long CALLER_BASE = 0x400000L;
	private static final int CALLER_STRIDE = 0x100;  // 256 bytes per caller (room for calls)

	private static final int BLR = 0x4E800020;       // PPC "blr"

	@Override
	public void run() throws Exception {
		// --- tunables (env or arg overrides) ---
		int n = intParam("VT_REF_BENCH_N", 0, 2000);          // caller fns per program
		int anchors = intParam("VT_REF_BENCH_ANCHORS", 1, 40); // accepted-match anchors
		int callsPerCaller = intParam("VT_REF_BENCH_CALLS", 2, 8); // anchor calls per caller

		println("VT-REF-BENCH: building 2 PowerPC programs: callers=" + n +
			" anchors=" + anchors + " callsPerCaller=" + callsPerCaller);

		Language language =
			DefaultLanguageService.getLanguageService().getLanguage(new LanguageID(LANGUAGE_ID));

		Program source = buildProgram("vt_ref_source", language, n, anchors, callsPerCaller);
		Program dest = buildProgram("vt_ref_dest", language, n, anchors, callsPerCaller);

		ghidra.framework.model.DomainFolder root = getProjectRootFolder();
		root.createFile("vt_ref_source", source, monitor);
		root.createFile("vt_ref_dest", dest, monitor);

		VTSessionDB session = null;
		try {
			session = VTSessionDB.createVTSession("VT Ref Bench Session", source, dest, this);

			// --- seed the ACCEPTED anchor matches (the "previously accepted function
			// matches" extractReferenceFeatures keys its features off of) ---
			seedAcceptedAnchorMatches(session, source, dest, anchors);

			// --- run the REAL Function Reference correlator and time it ---
			VTProgramCorrelatorFactory factory = new FunctionReferenceProgramCorrelatorFactory();
			AddressSetView srcSet = source.getMemory().getLoadedAndInitializedAddressSet();
			AddressSetView dstSet = dest.getMemory().getLoadedAndInitializedAddressSet();
			VTProgramCorrelator correlator = factory.createCorrelator(source, srcSet, dest, dstSet,
				factory.createDefaultOptions());

			int tx = session.startTransaction("VT ref correlate");
			boolean ok = false;
			VTMatchSet resultSet;
			long correlateMs;
			try {
				long t0 = System.nanoTime();
				resultSet = correlator.correlate(session, monitor);
				correlateMs = (System.nanoTime() - t0) / 1_000_000L;
				ok = true;
			}
			finally {
				session.endTransaction(tx, ok);
			}

			// --- canonicalize the produced matches into a sorted tuple list ---
			List<VTMatch> matches = new ArrayList<>(resultSet.getMatches());
			List<String> tuples = new ArrayList<>(matches.size());
			for (VTMatch m : matches) {
				VTAssociation a = m.getAssociation();
				long srcOff = a.getSourceAddress().getOffset();
				long dstOff = a.getDestinationAddress().getOffset();
				VTScore sim = m.getSimilarityScore();
				VTScore conf = m.getConfidenceScore();
				// Canonical fixed-precision rendering so the textual diff is exact and
				// robust to trivial double->string formatting wobble.
				tuples.add(String.format("%08x %08x %.12f %.12f %d %d",
					srcOff, dstOff,
					sim == null ? Double.NaN : sim.getScore(),
					conf == null ? Double.NaN : conf.getScore(),
					m.getSourceLength(), m.getDestinationLength()));
			}
			Collections.sort(tuples);

			String digest = sha256(String.join("\n", tuples));

			println("VT-REF-BENCH-RESULT n=" + n);
			println("VT-REF-BENCH-RESULT anchors=" + anchors);
			println("VT-REF-BENCH-RESULT matches=" + tuples.size());
			println("VT-REF-BENCH-RESULT correlateMs=" + correlateMs);
			println("VT-REF-BENCH-RESULT digest=" + digest);
			for (String t : tuples) {
				println("VT-REF-BENCH-TUPLE " + t);
			}
		}
		finally {
			if (session != null) {
				session.release(this);
			}
			dest.release(this);
			source.release(this);
		}
	}

	/**
	 * Create a match set, add one FUNCTION match per anchor index (source anchor i &lt;-&gt;
	 * destination anchor i), and ACCEPT each so that the reference correlator treats them
	 * as the previously-accepted features.
	 */
	private void seedAcceptedAnchorMatches(VTSessionDB session, Program source, Program dest,
			int anchors) throws Exception {
		int tx = session.startTransaction("seed accepted anchors");
		boolean ok = false;
		try {
			VTMatchSet ms = session.createMatchSet(
				new ManualCorrelatorInfoStub("Seed Accepted Anchors", source, dest));
			for (int i = 0; i < anchors; i++) {
				Address sAddr = anchorAddr(source, i);
				Address dAddr = anchorAddr(dest, i);
				VTMatchInfo info = new VTMatchInfo(ms);
				info.setAssociationType(VTAssociationType.FUNCTION);
				info.setSourceAddress(sAddr);
				info.setDestinationAddress(dAddr);
				info.setSourceLength(4);
				info.setDestinationLength(4);
				info.setSimilarityScore(new VTScore(1.0));
				info.setConfidenceScore(new VTScore(1.0));
				info.setTag(null);
				VTMatch match = ms.addMatch(info);
				match.getAssociation().setAccepted();
			}
			ok = true;
		}
		finally {
			session.endTransaction(tx, ok);
		}
	}

	private Address anchorAddr(Program p, int i) {
		return p.getAddressFactory().getDefaultAddressSpace()
			.getAddress(ANCHOR_BASE + (long) i * ANCHOR_STRIDE);
	}

	private Address callerAddr(Program p, int j) {
		return p.getAddressFactory().getDefaultAddressSpace()
			.getAddress(CALLER_BASE + (long) j * CALLER_STRIDE);
	}

	/**
	 * Deterministically pick which anchors caller j calls. Source and destination use the
	 * IDENTICAL rule, so corresponding callers get identical call fingerprints -> the
	 * cosine-similarity loop has plenty of positive, overlapping neighbors to score (the
	 * whole point: exercise the O(dest x src) compare loop with real LSH vectors).
	 */
	private int[] anchorsForCaller(int j, int anchors, int callsPerCaller) {
		int k = Math.min(callsPerCaller, anchors);
		int[] picks = new int[k];
		// A spread-out deterministic selection that makes callers share subsets.
		int a = (j * 7 + 3) % anchors;
		int step = 1 + (j % Math.max(1, anchors - 1));
		boolean[] used = new boolean[anchors];
		int filled = 0;
		int guard = 0;
		while (filled < k && guard < anchors * 4) {
			if (!used[a]) {
				used[a] = true;
				picks[filled++] = a;
			}
			a = (a + step) % anchors;
			guard++;
		}
		// pad any remainder linearly (only triggers for pathological anchors values)
		for (int x = 0; filled < k; x++) {
			if (!used[x % anchors]) {
				used[x % anchors] = true;
				picks[filled++] = x % anchors;
			}
		}
		return picks;
	}

	private Program buildProgram(String name, Language language, int n, int anchors,
			int callsPerCaller) throws Exception {
		ProgramDB program = new ProgramDB(name, language, language.getDefaultCompilerSpec(), this);
		int tx = program.startTransaction("build " + name);
		boolean ok = false;
		try {
			Memory mem = program.getMemory();
			Address base = program.getAddressFactory().getDefaultAddressSpace()
				.getAddress(ANCHOR_BASE);
			long callerSpan = (long) n * CALLER_STRIDE + 0x100;
			long total = (CALLER_BASE - ANCHOR_BASE) + callerSpan;
			mem.createInitializedBlock("code", base, total, (byte) 0, monitor, false);

			// --- anchors: a single blr each ---
			for (int i = 0; i < anchors; i++) {
				Address entry = anchorAddr(program, i);
				writeInt(mem, entry, BLR);
			}

			// --- callers: run of "bl <anchor>" then blr ---
			for (int j = 0; j < n; j++) {
				Address entry = callerAddr(program, j);
				int[] picks = anchorsForCaller(j, anchors, callsPerCaller);
				Address pc = entry;
				for (int idx = 0; idx < picks.length; idx++) {
					Address target = anchorAddr(program, picks[idx]);
					writeInt(mem, pc, encodeBL(pc, target));
					pc = pc.add(4);
				}
				writeInt(mem, pc, BLR);
			}

			// --- disassemble + declare all functions ---
			for (int i = 0; i < anchors; i++) {
				Address entry = anchorAddr(program, i);
				disassemble(program, entry);
				createFunction(program, entry, "anchor_" + i);
			}
			for (int j = 0; j < n; j++) {
				Address entry = callerAddr(program, j);
				disassemble(program, entry);
				createFunction(program, entry, "caller_" + j);
			}
			ok = true;
		}
		finally {
			program.endTransaction(tx, ok);
		}
		return program;
	}

	/** PPC "bl" (branch-and-link) to an absolute target relative to pc. */
	private int encodeBL(Address pc, Address target) {
		long disp = target.getOffset() - pc.getOffset();
		// 26-bit signed displacement, low 2 bits zero. 0x48000001 = bl form (LK=1).
		return 0x48000000 | ((int) disp & 0x03FFFFFC) | 0x1;
	}

	private void writeInt(Memory mem, Address addr, int value) throws Exception {
		byte[] b = {
			(byte) (value >>> 24), (byte) (value >>> 16),
			(byte) (value >>> 8), (byte) value
		};
		mem.setBytes(addr, b);
	}

	private void disassemble(Program program, Address entry) {
		ghidra.app.cmd.disassemble.DisassembleCommand cmd =
			new ghidra.app.cmd.disassemble.DisassembleCommand(entry, null, true);
		cmd.applyTo(program, monitor);
	}

	private void createFunction(Program program, Address entry, String fname) throws Exception {
		ghidra.app.cmd.function.CreateFunctionCmd cmd =
			new ghidra.app.cmd.function.CreateFunctionCmd(fname, entry, null,
				SourceType.USER_DEFINED);
		cmd.applyTo(program, monitor);
	}

	private int intParam(String env, int min, int def) {
		int v = def;
		String e = System.getenv(env);
		if (e != null && !e.isBlank()) {
			v = Integer.parseInt(e.trim());
		}
		if (v < min) {
			v = min;
		}
		return v;
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
	 * Minimal {@link VTProgramCorrelator} stub used only to attach a name to the manually
	 * seeded match set (createMatchSet requires a correlator-info source). It is never run.
	 */
	private static class ManualCorrelatorInfoStub
			implements ghidra.feature.vt.api.main.VTProgramCorrelator {
		private final String name;
		private final Program src;
		private final Program dst;

		ManualCorrelatorInfoStub(String name, Program src, Program dst) {
			this.name = name;
			this.src = src;
			this.dst = dst;
		}

		@Override
		public String getName() {
			return name;
		}

		@Override
		public ghidra.framework.options.ToolOptions getOptions() {
			return new ghidra.framework.options.ToolOptions("manual");
		}

		@Override
		public Program getSourceProgram() {
			return src;
		}

		@Override
		public Program getDestinationProgram() {
			return dst;
		}

		@Override
		public AddressSetView getSourceAddressSet() {
			return src.getMemory().getLoadedAndInitializedAddressSet();
		}

		@Override
		public AddressSetView getDestinationAddressSet() {
			return dst.getMemory().getLoadedAndInitializedAddressSet();
		}

		@Override
		public VTMatchSet correlate(VTSession session, ghidra.util.task.TaskMonitor monitor) {
			throw new UnsupportedOperationException("stub");
		}
	}
}
