// Dump the whole instruction listing, one line per instruction, with the
// function that owns it.  The decompiler quietly drops 16-bit string
// instructions and mis-splits odd-address code, so the listing is what the
// port gets written against when a routine matters.
//
//   analyzeHeadless <proj> sbp -process SBP98.EXE -noanalysis \
//       -scriptPath tools/ghidra -postScript DumpAsm.java out/sbp98.asm
//
// @category Sokoban
import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.CodeUnitIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class DumpAsm extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String path = args.length > 0 ? args[0] : "sbp98.asm";

        PrintWriter w = new PrintWriter(new FileWriter(path));
        w.println("; " + currentProgram.getName() + " - Ghidra listing");

        CodeUnitIterator it = currentProgram.getListing().getCodeUnits(true);
        Function owner = null;
        int n = 0;
        while (it.hasNext() && !monitor.isCancelled()) {
            CodeUnit cu = it.next();
            Address a = cu.getMinAddress();
            Function f = getFunctionContaining(a);
            if (f != owner) {
                owner = f;
                w.println();
                w.println("; ---- " + (f == null ? "(loose)" : f.getName())
                          + " ----");
            }
            String cmt = cu.getComment(CodeUnit.EOL_COMMENT);
            if (cu instanceof Instruction) {
                w.printf("%-14s %-40s%s%n", a, cu.toString(),
                         cmt == null ? "" : "; " + cmt);
                n++;
            } else {
                w.printf("%-14s %-40s%s%n", a, "db " + cu.toString(),
                         cmt == null ? "" : "; " + cmt);
            }
        }
        w.close();
        println("wrote " + path + ": " + n + " instructions");
    }
}
