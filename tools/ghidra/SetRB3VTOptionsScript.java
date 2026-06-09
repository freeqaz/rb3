/* ###
 * RB3 decomp — AutoVT correlator options prescript (Bank 5 DWARF -> Bank 8 target).
 *
 * A copy of Ghidra's stock
 *   Ghidra/Features/VersionTracking/ghidra_scripts/SetAutoVersionTrackingOptionsScript.java
 * It stashes a GhidraValuesMap of AutoVT *correlator* options into the script state
 * env var "autoVTOptionsMap", which the stock AutoVersionTrackingScript.java reads in
 * headless mode (see AutoVersionTrackingScript.run()).
 *
 * *** IMPORTANT LIMITATION — READ BEFORE USING THIS PAIR ***
 *   The stock AutoVersionTrackingScript's setToolOptionsFromOptionsMap copies ONLY the
 *   AutoVT correlator knobs (RUN_*, *_MIN_LEN, REF_*, MIN_VOTES, MAX_CONFLICTS,
 *   CREATE/APPLY_IMPLIED) out of this map. It does NOT copy any apply-markup enum
 *   (FUNCTION_NAME / PARAMETER_NAMES / DATA_MATCH_DATA_TYPE / ...). The apply-markup
 *   choices therefore fall back to their VTOptionDefines defaults, and the FUNCTION_NAME
 *   default is ADD_AS_PRIMARY — which would add Bank 5's DWARF-demangled names onto our
 *   Bank 8 functions, clobbering the CodeWarrior mangled names we must keep.
 *
 *   Because GhidraValuesMap cannot hold enum values and the stock script ignores markup
 *   options anyway, FUNCTION_NAME=EXCLUDE *cannot* be expressed through this prescript.
 *   For the real RB3 run use tools/ghidra/RB3AutoVersionTrackingScript.java (a single
 *   -postScript), which sets FUNCTION_NAME=EXCLUDE + the other markup choices directly on
 *   the VTOptions the task consumes. This prescript is kept only for correlator tuning /
 *   the stock 2-postScript flow and is NOT used by run_version_tracking.sh by default.
 *
 * @category Examples.Version Tracking
 */
import ghidra.app.script.GhidraScript;
import ghidra.feature.vt.gui.util.VTOptionDefines;
import ghidra.features.base.values.GhidraValuesMap;

public class SetRB3VTOptionsScript extends GhidraScript {

	@Override
	public void run() throws Exception {
		GhidraValuesMap optionsMap = getOptions();
		state.addEnvironmentVar("autoVTOptionsMap", optionsMap);
	}

	private GhidraValuesMap getOptions() {
		GhidraValuesMap optionsValues = new GhidraValuesMap();

		// Run every correlator: exact symbol (all shared mangled names) + exact data +
		// exact function bytes/instructions/mnemonics + duplicate-function + references.
		// ALL AutoVT correlators enabled. The earlier OOM in the Duplicate-Function
		// phase was a heap limit (8G), not a reason to drop the correlator — run with a
		// large heap instead (run_version_tracking.sh: GHIDRA_HEADLESS_MAXMEM=48G).
		// Reference + Implied are speculative; MIN_VOTES=2 / MAX_CONFLICTS=0 /
		// REF_MIN_SCORE=0.95 / REF_MIN_CONF=10 keep their false-positive rate low.
		optionsValues.defineBoolean(VTOptionDefines.CREATE_IMPLIED_MATCHES_OPTION_TEXT, true);
		optionsValues.defineBoolean(VTOptionDefines.RUN_EXACT_SYMBOL_OPTION_TEXT, true);
		optionsValues.defineBoolean(VTOptionDefines.RUN_EXACT_DATA_OPTION_TEXT, true);
		optionsValues.defineBoolean(VTOptionDefines.RUN_EXACT_FUNCTION_BYTES_OPTION_TEXT, true);
		optionsValues.defineBoolean(VTOptionDefines.RUN_EXACT_FUNCTION_INST_OPTION_TEXT, true);
		optionsValues.defineBoolean(VTOptionDefines.RUN_DUPE_FUNCTION_OPTION_TEXT, true);
		optionsValues.defineBoolean(VTOptionDefines.RUN_REF_CORRELATORS_OPTION_TEXT, true);

		optionsValues.defineInt(VTOptionDefines.DATA_CORRELATOR_MIN_LEN_OPTION_TEXT, 5);
		optionsValues.defineInt(VTOptionDefines.SYMBOL_CORRELATOR_MIN_LEN_OPTION_TEXT, 3);
		optionsValues.defineInt(VTOptionDefines.FUNCTION_CORRELATOR_MIN_LEN_OPTION_TEXT, 10);
		optionsValues.defineInt(VTOptionDefines.DUPE_FUNCTION_CORRELATOR_MIN_LEN_OPTION_TEXT, 10);

		optionsValues.defineBoolean(VTOptionDefines.APPLY_IMPLIED_MATCHES_OPTION_TEXT, true);
		optionsValues.defineInt(VTOptionDefines.MIN_VOTES_OPTION_TEXT, 2);
		optionsValues.defineInt(VTOptionDefines.MAX_CONFLICTS_OPTION_TEXT, 0);

		optionsValues.defineDouble(VTOptionDefines.REF_CORRELATOR_MIN_SCORE_OPTION_TEXT, 0.95);
		optionsValues.defineDouble(VTOptionDefines.REF_CORRELATOR_MIN_CONF_OPTION_TEXT, 10.0);

		return optionsValues;
	}
}
