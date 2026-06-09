/* ###
 * RB3 decomp — headless Auto Version Tracking runner (Bank 5 DWARF -> Bank 8 target).
 *
 * This is a self-contained, RB3-specific copy of Ghidra's stock
 *   Ghidra/Features/VersionTracking/ghidra_scripts/AutoVersionTrackingScript.java
 * with ONE structural change that the stock script cannot express headlessly:
 * it sets the *apply-markup* options (FUNCTION_NAME=EXCLUDE, PARAMETER_NAMES,
 * PARAMETER_DATA_TYPES, FUNCTION_RETURN_TYPE, FUNCTION_SIGNATURE, DATA_MATCH_DATA_TYPE,
 * LABELS, comments) on the SAME VTOptions object that the AutoVersionTrackingTask
 * passes to ApplyMarkupItemTask.
 *
 * WHY A COPY INSTEAD OF -postScript SetRB3VTOptionsScript + AutoVersionTrackingScript:
 *   The stock AutoVersionTrackingScript only propagates the AutoVT *correlator*
 *   options from the prescript's `autoVTOptionsMap` (its setToolOptionsFromOptionsMap
 *   copies RUN_*, *_MIN_LEN, REF_*, MIN_VOTES, MAX_CONFLICTS only). It never copies any
 *   apply-markup enum (FUNCTION_NAME etc.) out of that map onto the ToolOptions that the
 *   task consumes. So with the stock script the markup-apply choices fall back to their
 *   VTOptionDefines defaults — and the FUNCTION_NAME default is ADD_AS_PRIMARY, which
 *   WOULD add Bank 5's DWARF-demangled names as primary on our Bank 8 functions,
 *   clobbering the CodeWarrior-map mangled names we want to keep. There is no headless
 *   hook in the stock script to override that, hence this copy.
 *
 *   FunctionNameMarkupType.getApplyAction() reads options.getEnum(FUNCTION_NAME, default)
 *   and returns null (== "do not apply") only when the value is EXCLUDE. We set it here.
 *
 * USAGE (headless, run by the orchestrator with the pyghidra-mcp service STOPPED):
 *   analyzeHeadless <projectLocationDir> <projectName> \
 *     -process <DestinationProgram> -noanalysis \
 *     -scriptPath tools/ghidra \
 *     -postScript RB3AutoVersionTrackingScript.java \
 *        "<sessionFolder>" "<sessionName>" "<sourceProgramPath>"
 *   e.g. args: "/"  "RB3 b5->b8 autoVT"  "/band_r_wii.elf-781439"
 *   The destination program is the -process program (Bank 8 / bank8_target.elf-...).
 *   The source program must already be analyzed (it is — it's in the project).
 *
 * API verified against Ghidra 12.1 source in
 *   /opt/ghidra/Ghidra/Features/VersionTracking/lib/VersionTracking-src.zip:
 *   - AutoVersionTrackingTask reads the AutoVT correlator options AND passes the same
 *     ToolOptions to ApplyMarkupItemTask (lines ~661), which calls
 *     markupType.getApplyAction(options) -> the apply-markup enums below are honored.
 *   - VTMatchApplyChoices enum member names (EXCLUDE/REPLACE/PRIORITY_REPLACE/...) confirmed.
 *
 * @category Version Tracking
 */
import ghidra.app.script.GhidraScript;
import ghidra.feature.vt.api.db.VTSessionContentHandler;
import ghidra.feature.vt.api.db.VTSessionDB;
import ghidra.feature.vt.api.main.VTSession;
import ghidra.feature.vt.api.util.VTOptions;
import ghidra.feature.vt.gui.actions.AutoVersionTrackingTask;
import ghidra.feature.vt.gui.util.VTMatchApplyChoices.CallingConventionChoices;
import ghidra.feature.vt.gui.util.VTMatchApplyChoices.CommentChoices;
import ghidra.feature.vt.gui.util.VTMatchApplyChoices.FunctionNameChoices;
import ghidra.feature.vt.gui.util.VTMatchApplyChoices.FunctionSignatureChoices;
import ghidra.feature.vt.gui.util.VTMatchApplyChoices.LabelChoices;
import ghidra.feature.vt.gui.util.VTMatchApplyChoices.ParameterDataTypeChoices;
import ghidra.feature.vt.gui.util.VTMatchApplyChoices.ReplaceDataChoices;
import ghidra.feature.vt.gui.util.VTMatchApplyChoices.SourcePriorityChoices;
import ghidra.feature.vt.gui.util.VTOptionDefines;
import ghidra.framework.model.DomainFile;
import ghidra.framework.model.DomainFolder;
import ghidra.framework.options.ToolOptions;
import ghidra.program.model.listing.Program;
import ghidra.util.exception.CancelledException;
import ghidra.util.task.TaskLauncher;

public class RB3AutoVersionTrackingScript extends GhidraScript {

	private static final int NUM_ARGS = 3;

	@Override
	public void run() throws Exception {

		if (currentProgram == null) {
			println("RB3AutoVT: please open/-process the DESTINATION program (Bank 8).");
			return;
		}

		Program destinationProgram = currentProgram;

		if (!destinationProgram.canSave()) {
			println("RB3AutoVT: destination program " + destinationProgram.getName() +
				" is read-only; cannot apply VT markup. (Did you pass -readOnly?)");
			return;
		}

		String[] args = getScriptArgs();
		if (args.length < NUM_ARGS) {
			println("RB3AutoVT: expected " + NUM_ARGS +
				" args: <sessionFolderPath> <sessionName> <sourceProgramPath>");
			return;
		}
		String sessionFolderPath = args[0];
		String sessionName = args[1];
		String sourceProgramPath = args[2];

		DomainFolder folder = getProjectFolder(sessionFolderPath);
		if (folder == null) {
			println("RB3AutoVT: session folder not found: " + sessionFolderPath);
			return;
		}
		if (hasExistingSession(sessionName, folder)) {
			println("RB3AutoVT: session '" + sessionName + "' already exists in " +
				sessionFolderPath + " — choose a new name (cannot reuse an existing session).");
			return;
		}

		DomainFile sourceProgramDF = getProjectFile(sourceProgramPath);
		if (sourceProgramDF == null) {
			println("RB3AutoVT: SOURCE program not found: " + sourceProgramPath);
			return;
		}
		if (!Program.class.isAssignableFrom(sourceProgramDF.getDomainObjectClass())) {
			println("RB3AutoVT: " + sourceProgramDF.getName() + " is not a Program.");
			return;
		}

		// Headless: auto-upgrade the source program if needed.
		boolean autoUpgradeIfNeeded = isRunningHeadless();

		Program sourceProgram =
			(Program) sourceProgramDF.getDomainObject(this, autoUpgradeIfNeeded, false, monitor);

		VTSession session = null;
		try {
			// End the script transaction so it doesn't fight the VT session locks.
			end(true);

			session = new VTSessionDB(sessionName, sourceProgram, destinationProgram, this);

			if (folder.getFile(sessionName) == null) {
				folder.createFile(sessionName, session, monitor);
			}

			ToolOptions vtOptions = buildOptions();

			AutoVersionTrackingTask autoVtTask = new AutoVersionTrackingTask(session, vtOptions);
			TaskLauncher.launch(autoVtTask);

			destinationProgram.save("RB3 Auto Version Tracking (Bank5->Bank8)", monitor);
			session.save();

			println(autoVtTask.getStatusMsg());
		}
		catch (CancelledException e) {
			return;
		}
		finally {
			if (sourceProgram != null) {
				sourceProgram.release(this);
			}
			if (session != null) {
				session.release(this);
			}
		}
	}

	/**
	 * Helper that resolves a project folder by path; root "/" returns the project root.
	 */
	private DomainFolder getProjectFolder(String path) {
		DomainFolder root = getProjectRootFolder();
		if (path == null || path.isEmpty() || path.equals("/")) {
			return root;
		}
		DomainFolder folder = root;
		for (String part : path.split("/")) {
			if (part.isEmpty()) {
				continue;
			}
			folder = folder.getFolder(part);
			if (folder == null) {
				return null;
			}
		}
		return folder;
	}

	/**
	 * Helper that resolves a project file (program) by absolute project path.
	 */
	private DomainFile getProjectFile(String path) {
		String p = path;
		if (p.startsWith("/")) {
			p = p.substring(1);
		}
		int slash = p.lastIndexOf('/');
		String dir = (slash < 0) ? "/" : ("/" + p.substring(0, slash));
		String name = (slash < 0) ? p : p.substring(slash + 1);
		DomainFolder folder = getProjectFolder(dir);
		if (folder == null) {
			return null;
		}
		return folder.getFile(name);
	}

	private boolean hasExistingSession(String name, DomainFolder folder) {
		DomainFile file = folder.getFile(name);
		return file != null &&
			file.getContentType().equals(VTSessionContentHandler.CONTENT_TYPE);
	}

	/**
	 * Build the single VTOptions object the AutoVersionTrackingTask consumes for BOTH
	 * correlator configuration AND apply-markup choices.
	 *
	 * Correlators: run exact symbol (all 41,680 shared mangled names) + exact data +
	 * exact function bytes + exact function instructions/mnemonics + duplicate-function +
	 * reference correlators + implied matches. (Conservative reference thresholds.)
	 *
	 * Apply-markup: take Bank 5's signature/param names/types/return type/calling
	 * convention/labels/comments and global data types, but DO NOT touch the function
	 * NAME (EXCLUDE) so the CodeWarrior-map mangled names survive.
	 */
	private ToolOptions buildOptions() {
		ToolOptions o = new VTOptions("RB3 AutoVT");

		// ---- AutoVT correlator selection ----
		o.setBoolean(VTOptionDefines.RUN_EXACT_SYMBOL_OPTION, true);
		o.setBoolean(VTOptionDefines.RUN_EXACT_DATA_OPTION, true);
		o.setBoolean(VTOptionDefines.RUN_EXACT_FUNCTION_BYTES_OPTION, true);
		o.setBoolean(VTOptionDefines.RUN_EXACT_FUNCTION_INST_OPTION, true);
		o.setBoolean(VTOptionDefines.RUN_DUPE_FUNCTION_OPTION, true);
		o.setBoolean(VTOptionDefines.RUN_REF_CORRELATORS_OPTION, true);

		o.setInt(VTOptionDefines.SYMBOL_CORRELATOR_MIN_LEN_OPTION, 3);
		o.setInt(VTOptionDefines.DATA_CORRELATOR_MIN_LEN_OPTION, 5);
		o.setInt(VTOptionDefines.FUNCTION_CORRELATOR_MIN_LEN_OPTION, 10);
		o.setInt(VTOptionDefines.DUPE_FUNCTION_CORRELATOR_MIN_LEN_OPTION, 10);

		o.setDouble(VTOptionDefines.REF_CORRELATOR_MIN_SCORE_OPTION, 0.95);
		o.setDouble(VTOptionDefines.REF_CORRELATOR_MIN_CONF_OPTION, 10.0);

		// ---- Implied matches ----
		o.setBoolean(VTOptionDefines.CREATE_IMPLIED_MATCHES_OPTION, true);
		o.setBoolean(VTOptionDefines.APPLY_IMPLIED_MATCHES_OPTION, true);
		o.setInt(VTOptionDefines.MIN_VOTES_OPTION, 2);
		o.setInt(VTOptionDefines.MAX_CONFLICTS_OPTION, 0);

		// ---- Apply-markup choices (read by the markup types via ApplyMarkupItemTask) ----
		// THE critical override: keep our CodeWarrior mangled names, do not import Bank 5's.
		o.setEnum(VTOptionDefines.FUNCTION_NAME, FunctionNameChoices.EXCLUDE);

		// Take Bank 5's full function signature (return + params) where the param count matches.
		o.setEnum(VTOptionDefines.FUNCTION_SIGNATURE,
			FunctionSignatureChoices.WHEN_SAME_PARAMETER_COUNT);
		o.setEnum(VTOptionDefines.FUNCTION_RETURN_TYPE, ParameterDataTypeChoices.REPLACE);
		o.setEnum(VTOptionDefines.PARAMETER_DATA_TYPES, ParameterDataTypeChoices.REPLACE);
		o.setEnum(VTOptionDefines.PARAMETER_NAMES, SourcePriorityChoices.PRIORITY_REPLACE);
		o.setEnum(VTOptionDefines.CALLING_CONVENTION, CallingConventionChoices.NAME_MATCH);

		// Global data variable types (e.g. gHeadMale) — the port_dwarf_types.py gap VT closes.
		o.setEnum(VTOptionDefines.DATA_MATCH_DATA_TYPE, ReplaceDataChoices.REPLACE_ALL_DATA);

		// Labels + comments: additive (never destroy existing).
		o.setEnum(VTOptionDefines.LABELS, LabelChoices.ADD);
		o.setEnum(VTOptionDefines.PLATE_COMMENT, CommentChoices.APPEND_TO_EXISTING);
		o.setEnum(VTOptionDefines.PRE_COMMENT, CommentChoices.APPEND_TO_EXISTING);
		o.setEnum(VTOptionDefines.END_OF_LINE_COMMENT, CommentChoices.APPEND_TO_EXISTING);
		o.setEnum(VTOptionDefines.REPEATABLE_COMMENT, CommentChoices.APPEND_TO_EXISTING);
		o.setEnum(VTOptionDefines.POST_COMMENT, CommentChoices.APPEND_TO_EXISTING);
		o.setEnum(VTOptionDefines.PARAMETER_COMMENTS, CommentChoices.APPEND_TO_EXISTING);

		return o;
	}
}
