//Decompile functions in a specific address range
//@category Analysis

import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;

public class DecompileRange extends GhidraScript {
    @Override
    public void run() throws Exception {
        File outFile = new File(getScriptArgs()[0]);
        String rangeSpec = getScriptArgs()[1];

        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);

        PrintWriter pw = new PrintWriter(new FileWriter(outFile));
        int count = 0;

        String[] parts = rangeSpec.split("-");
        long start = Long.parseLong(parts[0], 16);
        long end = Long.parseLong(parts[1], 16);

        pw.println("// Decompiled functions in range 0x" + parts[0] + " - 0x" + parts[1]);
        pw.println();

        FunctionIterator funcs = currentProgram.getListing().getFunctions(true);
        while (funcs.hasNext() && !monitor.isCancelled()) {
            Function func = funcs.next();
            long addr = func.getEntryPoint().getOffset();
            if (addr >= start && addr <= end) {
                DecompileResults results = decomp.decompileFunction(func, 60, monitor);
                if (results.decompileCompleted()) {
                    DecompiledFunction df = results.getDecompiledFunction();
                    if (df != null) {
                        pw.println("// ==========================================");
                        pw.println("// Function: " + func.getName() + " @ " + func.getEntryPoint());
                        pw.printf("// Size: %d bytes%n", func.getBody().getNumAddresses());
                        pw.println("// ==========================================");
                        pw.println(df.getC());
                        pw.println();
                        count++;
                    }
                }
            }
        }

        pw.close();
        decomp.dispose();
        println("Decompiled " + count + " functions to " + outFile.getAbsolutePath());
    }
}
