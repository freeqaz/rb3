//Reproducible benchmark for the Ghidra Version-Tracking Duplicate-Function O(n^2)
//apply-phase stall (RB3 Bank5->Bank8). Builds two throwaway PowerPC programs each
//containing N byte-identical tiny functions, runs the REAL Duplicate-Function
//correlator to create the k^2 associations, then times the exact hot method
//AutoVersionTrackingTask.getAllRelatedAssociations -> HashSet<VTAssociationDB>.add
//-> VTAssociationDB.equals/hashCode -> *synchronized* AddressMapDB.decodeAddress.
//Prints elapsedMillis + a correctness counter (associations in the dupe group, and
//total related-association elements materialized = the work the apply phase does).
//
//Runs against whatever VersionTracking.jar is installed in the Ghidra it launches
//under, so re-running after swapping the patched jar measures the patched behavior.
//
//@category VersionTracking.Benchmark
//@menupath

import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.feature.vt.api.correlator.program.DuplicateFunctionMatchProgramCorrelatorFactory;
import ghidra.feature.vt.api.db.VTSessionDB;
import ghidra.feature.vt.api.main.VTAssociation;
import ghidra.feature.vt.api.main.VTAssociationManager;
import ghidra.feature.vt.api.main.VTProgramCorrelator;
import ghidra.feature.vt.api.main.VTProgramCorrelatorFactory;
import ghidra.feature.vt.api.main.VTSession;
import ghidra.program.database.ProgramDB;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.lang.Language;
import ghidra.program.model.lang.LanguageID;
import ghidra.program.model.lang.LanguageService;
import ghidra.program.model.listing.Program;
import ghidra.program.model.mem.Memory;
import ghidra.program.util.DefaultLanguageService;
import ghidra.util.task.TaskMonitor;

public class VTDupeBenchmarkScript extends GhidraScript {

	// PowerPC big-endian 32, the same family as RB3 Bank5/Bank8.
	private static final String LANGUAGE_ID = "PowerPC:BE:32:Gekko_Broadway";

	// A 12-byte byte-identical body (3 PowerPC instructions, >= the dupe
	// correlator's 10-address minimum-function-size). All three are "or r0,r0,r0"
	// (a NOP form) followed by nothing special -- the bytes are identical across
	// every function and across both programs, so every function lands in one
	// giant identical-code hash bucket -> k*k associations.
	// 0x60000000 = ori r0,r0,0 (PPC NOP). Repeat 3x = 12 bytes.
	private static final byte[] BODY = {
		0x60, 0x00, 0x00, 0x00,
		0x60, 0x00, 0x00, 0x00,
		0x60, 0x00, 0x00, 0x00,
	};
	private static final int FUNC_STRIDE = 0x20; // spacing between function entries
	private static final long BASE = 0x100000L;

	@Override
	public void run() throws Exception {
		// --- tunable: dupe group size N per program (env override) ---
		int n = 300;
		String envN = System.getenv("VT_BENCH_N");
		if (envN != null && !envN.isBlank()) {
			n = Integer.parseInt(envN.trim());
		}
		String[] args = getScriptArgs();
		if (args != null && args.length >= 1) {
			n = Integer.parseInt(args[0].trim());
		}

		println("VT-DUPE-BENCH: building 2 PowerPC programs with N=" + n +
			" byte-identical functions each...");

		Language language =
			DefaultLanguageService.getLanguageService().getLanguage(new LanguageID(LANGUAGE_ID));

		Program source = buildProgram("vt_bench_source", language, n);
		Program dest = buildProgram("vt_bench_dest", language, n);

		// VTSessionDB requires writable (savable) programs. Saving each ProgramDB into
		// the throwaway project gives it a writable GhidraFile -> canSave()==true.
		ghidra.framework.model.DomainFolder root = getProjectRootFolder();
		root.createFile("vt_bench_source", source, monitor);
		root.createFile("vt_bench_dest", dest, monitor);

		VTSessionDB session = null;
		try {
			session = VTSessionDB.createVTSession("VT Dupe Bench Session", source, dest, this);

			// --- run the REAL duplicate-function correlator -> k^2 associations ---
			long corrStart = System.nanoTime();
			runDuplicateCorrelator(session, source, dest);
			long corrMs = (System.nanoTime() - corrStart) / 1_000_000L;

			VTAssociationManager assocMgr = session.getAssociationManager();
			int assocCount = assocMgr.getAssociationCount();
			println("VT-DUPE-BENCH: correlator created associations=" + assocCount +
				" (expected ~ N*N = " + ((long) n * n) + ") in " + corrMs + " ms");

			// --- THE HOT PATH: AutoVersionTrackingTask.getAllRelatedAssociations ---
			// This is the exact method the apply phase calls once per dupe group;
			// it builds HashSet<VTAssociationDB> whose every add() pays
			// VTAssociationDB.equals/hashCode -> synchronized AddressMapDB.decodeAddress.
			// We call it directly over the dupe group (no markup needed) so the
			// measurement isolates the stall and is correctness-comparable
			// before/after the jar swap.
			List<VTAssociation> all = new ArrayList<>(assocMgr.getAssociations());
			if (all.isEmpty()) {
				println("VT-DUPE-BENCH: ERROR no associations were created; cannot measure.");
				return;
			}

			// Seed pair = the first association's (source,dest). getAllRelatedAssociations
			// will fan out to the whole k^2 group from it.
			Address seedSrc = all.get(0).getSourceAddress();
			Address seedDst = all.get(0).getDestinationAddress();

			// reflectively reach the private static-ish hot method via a faithful
			// in-script reimplementation that calls the SAME public manager API the
			// real method calls (getRelatedAssociationsBySourceAndDestinationAddress)
			// and builds the SAME HashSet<VTAssociation>. This exercises the identical
			// equals/hashCode/decodeAddress codepath in the installed jar.
			long hotStart = System.nanoTime();
			long relatedElementsTouched =
				measureGetAllRelatedAssociations(assocMgr, seedSrc, seedDst);
			long hotMs = (System.nanoTime() - hotStart) / 1_000_000L;

			// correctness counters
			println("VT-DUPE-BENCH-RESULT n=" + n);
			println("VT-DUPE-BENCH-RESULT associations=" + assocCount);
			println("VT-DUPE-BENCH-RESULT relatedElementsTouched=" + relatedElementsTouched);
			println("VT-DUPE-BENCH-RESULT correlatorMs=" + corrMs);
			println("VT-DUPE-BENCH-RESULT getAllRelatedAssociationsMs=" + hotMs);
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
	 * Faithful reimplementation of {@code AutoVersionTrackingTask.getAllRelatedAssociations}
	 * (AutoVersionTrackingTask.java:825-847). Builds {@code HashSet<VTAssociation>} over the
	 * dupe group, then for every member unions in that member's related set -- the exact
	 * O(g^2) HashSet build whose add() drives VTAssociationDB.equals/hashCode ->
	 * synchronized AddressMapDB.decodeAddress. Returns the number of related-association
	 * elements materialized (the correctness counter: same set contents before/after the
	 * jar swap), so a patched jar must produce the SAME count, just faster.
	 */
	private long measureGetAllRelatedAssociations(VTAssociationManager mgr, Address source,
			Address destination) throws Exception {

		Collection<VTAssociation> relatedAssociations =
			mgr.getRelatedAssociationsBySourceAndDestinationAddress(source, destination);

		Set<VTAssociation> allRelatedAssociations = new HashSet<>(relatedAssociations);

		long elementsTouched = relatedAssociations.size();
		for (VTAssociation association : relatedAssociations) {
			monitor.checkCancelled();
			Collection<VTAssociation> more =
				mgr.getRelatedAssociationsBySourceAndDestinationAddress(
					association.getSourceAddress(), association.getDestinationAddress());
			elementsTouched += more.size();
			allRelatedAssociations.addAll(more);
		}
		// touch the result so JIT can't elide the HashSet build
		println("VT-DUPE-BENCH: deduped related set size=" + allRelatedAssociations.size());
		return elementsTouched;
	}

	private void runDuplicateCorrelator(VTSession session, Program source, Program dest)
			throws Exception {

		VTProgramCorrelatorFactory factory = new DuplicateFunctionMatchProgramCorrelatorFactory();
		AddressSetView srcSet = source.getMemory().getLoadedAndInitializedAddressSet();
		AddressSetView dstSet = dest.getMemory().getLoadedAndInitializedAddressSet();

		VTProgramCorrelator correlator = factory.createCorrelator(source, srcSet, dest, dstSet,
			factory.createDefaultOptions());

		// createMatchSet / addMatch mutate the session DB -> need an open session transaction
		// (this is exactly what AutoVersionTrackingTask.run opens around the whole run).
		VTSessionDB sessionDB = (VTSessionDB) session;
		int tx = sessionDB.startTransaction("VT dupe correlate");
		boolean ok = false;
		try {
			correlator.correlate(session, monitor);
			ok = true;
		}
		finally {
			sessionDB.endTransaction(tx, ok);
		}
	}

	/**
	 * Build a real ProgramDB with N byte-identical PowerPC functions, disassembled and
	 * declared as functions, so the duplicate correlator hashes them into one bucket.
	 */
	private Program buildProgram(String name, Language language, int n) throws Exception {
		ProgramDB program = new ProgramDB(name, language, language.getDefaultCompilerSpec(), this);
		int tx = program.startTransaction("build " + name);
		boolean ok = false;
		try {
			Memory mem = program.getMemory();
			Address base = program.getAddressFactory().getDefaultAddressSpace().getAddress(BASE);
			long size = (long) n * FUNC_STRIDE + 0x100;
			mem.createInitializedBlock("code", base, size, (byte) 0, monitor, false);

			// write N identical bodies
			for (int i = 0; i < n; i++) {
				Address entry = base.add((long) i * FUNC_STRIDE);
				mem.setBytes(entry, BODY);
				// pad the rest of the stride with NOPs so disassembly doesn't run on
				for (int off = BODY.length; off < FUNC_STRIDE; off += 4) {
					mem.setBytes(entry.add(off), BODY, 0, 4);
				}
			}

			// disassemble + create functions
			for (int i = 0; i < n; i++) {
				Address entry = base.add((long) i * FUNC_STRIDE);
				disassemble(program, entry);
				createFunction(program, entry, "func_" + i);
			}
			ok = true;
		}
		finally {
			program.endTransaction(tx, ok);
		}
		return program;
	}

	private void disassemble(Program program, Address entry) {
		ghidra.app.cmd.disassemble.DisassembleCommand cmd =
			new ghidra.app.cmd.disassemble.DisassembleCommand(entry, null, true);
		cmd.applyTo(program, monitor);
	}

	private void createFunction(Program program, Address entry, String name) throws Exception {
		ghidra.app.cmd.function.CreateFunctionCmd cmd =
			new ghidra.app.cmd.function.CreateFunctionCmd(name, entry, null,
				ghidra.program.model.symbol.SourceType.USER_DEFINED);
		cmd.applyTo(program, monitor);
	}
}
