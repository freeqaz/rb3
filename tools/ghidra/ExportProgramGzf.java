// ExportProgramGzf.java — headless postScript: export the current program as a
// packed Ghidra Zip File (.gzf), analysis and all.
//
// Used by tools/ghidra/export_xenon_gzf.sh to pull the ALREADY-ANALYZED
// rb3-xenon default.xex program out of its project so ghidriff can import it
// into a fresh project with zero re-import / re-analysis risk (see
// tools/ghidra/run_ghidriff_xenon.sh for the decision record).
//
// Usage (via analyzeHeadless):
//   analyzeHeadless <proj_loc> <proj_name> -process <program> -noanalysis -readOnly \
//       -scriptPath rb3/tools/ghidra -postScript ExportProgramGzf.java /abs/out.gzf
//
//@category RB3

import java.io.File;

import ghidra.app.script.GhidraScript;
import ghidra.framework.model.DomainFile;

public class ExportProgramGzf extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            printerr("usage: ExportProgramGzf.java <output.gzf>");
            throw new IllegalArgumentException("expected exactly one arg: output .gzf path");
        }
        File out = new File(args[0]);
        File parent = out.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new RuntimeException("cannot create output dir: " + parent);
        }
        // Use DomainFile.packFile() rather than GzfExporter: the exporter returns
        // false silently in headless read-only mode, while packFile() is the
        // canonical pack primitive (it IS the .gzf format) and works read-only.
        DomainFile df = currentProgram.getDomainFile();
        if (df == null) {
            throw new RuntimeException("currentProgram has no DomainFile: " + currentProgram.getName());
        }
        df.packFile(out, monitor);
        if (!out.exists() || out.length() == 0) {
            throw new RuntimeException("packFile produced no/empty output: " + out.getAbsolutePath());
        }
        println("Exported " + currentProgram.getName() + " (" +
                currentProgram.getLanguageID() + ") -> " + out.getAbsolutePath() +
                " (" + out.length() + " bytes)");
    }
}
