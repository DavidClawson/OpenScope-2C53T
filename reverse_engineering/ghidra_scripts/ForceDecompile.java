//Force-create functions at specified addresses and decompile them
//@category Analysis

import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;

public class ForceDecompile extends GhidraScript {
    @Override
    public void run() throws Exception {
        File outFile = new File(getScriptArgs()[0]);
        String addrList = getScriptArgs()[1];

        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);

        PrintWriter pw = new PrintWriter(new FileWriter(outFile));
        pw.println("// Force-decompiled functions");
        pw.println();

        String[] addrs = addrList.split(",");
        for (String addrStr : addrs) {
            addrStr = addrStr.trim();
            long addrVal = Long.parseLong(addrStr, 16);
            Address addr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(addrVal);

            // Check if function already exists
            Function func = currentProgram.getFunctionManager().getFunctionAt(addr);

            if (func == null) {
                // Try to create a function
                pw.println("// Attempting to create function at 0x" + addrStr);
                CreateFunctionCmd cmd = new CreateFunctionCmd(addr);
                boolean created = cmd.applyTo(currentProgram, monitor);
                if (created) {
                    func = currentProgram.getFunctionManager().getFunctionAt(addr);
                    pw.println("// Successfully created function");
                } else {
                    pw.println("// WARNING: Could not create function at 0x" + addrStr);

                    // Try containing function
                    func = currentProgram.getFunctionManager().getFunctionContaining(addr);
                    if (func != null) {
                        pw.println("// Found containing function: " + func.getName() +
                            " @ " + func.getEntryPoint());
                    }
                }
            }

            if (func != null) {
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
                    }
                }
            }
        }

        pw.close();
        decomp.dispose();
        println("Done writing to " + outFile.getAbsolutePath());
    }
}
