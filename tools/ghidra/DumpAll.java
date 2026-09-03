// Dump every function's decompilation to one file, so the port can be written
// against grep instead of a GUI.
//
//   analyzeHeadless <proj> sbp -process SBP98.EXE -noanalysis \
//       -scriptPath tools/ghidra -postScript DumpAll.java out/sbp98.c
//
// @category Sokoban
import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class DumpAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String path = args.length > 0 ? args[0] : "sbp98.c";

        DecompInterface dec = new DecompInterface();
        dec.openProgram(currentProgram);

        PrintWriter w = new PrintWriter(new FileWriter(path));
        w.println("/* " + currentProgram.getName() + " - decompiled by Ghidra. */");

        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        int done = 0;
        int failed = 0;
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            w.println();
            w.println("/* ==== " + f.getName() + " at " + f.getEntryPoint()
                      + " ==== */");
            DecompileResults r = dec.decompileFunction(f, 90, monitor);
            if (r != null && r.decompileCompleted()) {
                w.println(r.getDecompiledFunction().getC());
                done++;
            } else {
                w.println("/* decompile failed: "
                          + (r == null ? "null" : r.getErrorMessage()) + " */");
                failed++;
            }
        }
        w.close();
        dec.dispose();
        println("wrote " + path + ": " + done + " functions, " + failed
                + " failed");
    }
}
