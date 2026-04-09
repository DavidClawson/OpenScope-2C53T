// Trace callers/callees for specific function addresses, creating the function if needed.
//@category Analysis

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.Set;

public class TraceFunctionXrefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        File outFile = new File(getScriptArgs()[0]);
        String addrList = getScriptArgs()[1];

        FunctionManager fm = currentProgram.getFunctionManager();
        PrintWriter pw = new PrintWriter(new FileWriter(outFile));
        pw.println("# Function Xref Trace");
        pw.println();

        for (String addrStrRaw : addrList.split(",")) {
            String addrStr = addrStrRaw.trim();
            long addrVal = Long.parseLong(addrStr, 16);
            Address addr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace()
                .getAddress(addrVal);

            Function func = fm.getFunctionAt(addr);
            if (func == null) {
                CreateFunctionCmd cmd = new CreateFunctionCmd(addr);
                cmd.applyTo(currentProgram, monitor);
                func = fm.getFunctionAt(addr);
            }
            if (func == null) {
                func = fm.getFunctionContaining(addr);
            }

            pw.printf("## Requested: 0x%s%n", addrStr);
            if (func == null) {
                pw.println("STATUS: no function available");
                pw.println();
                continue;
            }

            Set<Function> callers = func.getCallingFunctions(monitor);
            Set<Function> callees = func.getCalledFunctions(monitor);

            pw.printf("Function: %s @ %s%n", func.getName(), func.getEntryPoint());
            pw.printf("Size: %d bytes%n", func.getBody().getNumAddresses());
            pw.printf("Caller count: %d%n", callers.size());
            if (!callers.isEmpty()) {
                pw.println("Called by:");
                for (Function caller : callers) {
                    pw.printf("- %s @ %s%n", caller.getName(), caller.getEntryPoint());
                }
            }
            pw.printf("Callee count: %d%n", callees.size());
            if (!callees.isEmpty()) {
                pw.println("Calls:");
                for (Function callee : callees) {
                    pw.printf("- %s @ %s%n", callee.getName(), callee.getEntryPoint());
                }
            }
            pw.println();
        }

        pw.close();
        println("Done writing to " + outFile.getAbsolutePath());
    }
}
