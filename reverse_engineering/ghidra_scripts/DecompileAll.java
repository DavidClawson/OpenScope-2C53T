//Decompile all functions to C pseudocode
//@category Analysis

import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;

public class DecompileAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);

        File outFile = new File(getScriptArgs()[0]);
        PrintWriter pw = new PrintWriter(new FileWriter(outFile));

        FunctionIterator funcs = currentProgram.getListing().getFunctions(true);
        int count = 0;
        while (funcs.hasNext() && !monitor.isCancelled()) {
            Function func = funcs.next();
            DecompileResults results = decomp.decompileFunction(func, 30, monitor);
            if (results.decompileCompleted()) {
                DecompiledFunction df = results.getDecompiledFunction();
                if (df != null) {
                    pw.println("// ==========================================");
                    pw.println("// Function: " + func.getName() + " @ " + func.getEntryPoint());
                    pw.println("// ==========================================");
                    pw.println(df.getC());
                    pw.println();
                    count++;
                }
            }
        }
        pw.close();
        decomp.dispose();
        println("Decompiled " + count + " functions to " + outFile.getAbsolutePath());
    }
}
