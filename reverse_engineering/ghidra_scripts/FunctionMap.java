//Export complete function map: address, name, size, call count, callers, callees
//@category Analysis

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class FunctionMap extends GhidraScript {
    @Override
    public void run() throws Exception {
        File outFile = new File(getScriptArgs()[0]);
        PrintWriter pw = new PrintWriter(new FileWriter(outFile));

        pw.println("# Complete Function Map");
        pw.println("# Address | Size | Name | NumCallers | NumCallees | CallerAddrs | CalleeAddrs");
        pw.println("#");

        FunctionIterator funcs = currentProgram.getListing().getFunctions(true);
        int count = 0;
        while (funcs.hasNext() && !monitor.isCancelled()) {
            Function func = funcs.next();
            long size = func.getBody().getNumAddresses();

            // Get callers
            Set<Function> callers = func.getCallingFunctions(monitor);
            StringBuilder callerStr = new StringBuilder();
            for (Function caller : callers) {
                if (callerStr.length() > 0) callerStr.append(",");
                callerStr.append(caller.getEntryPoint());
            }

            // Get callees
            Set<Function> callees = func.getCalledFunctions(monitor);
            StringBuilder calleeStr = new StringBuilder();
            for (Function callee : callees) {
                if (calleeStr.length() > 0) calleeStr.append(",");
                calleeStr.append(callee.getEntryPoint());
            }

            pw.printf("%s | %d | %s | %d | %d | %s | %s%n",
                func.getEntryPoint(), size, func.getName(),
                callers.size(), callees.size(),
                callerStr.toString(), calleeStr.toString());
            count++;
        }
        pw.close();
        println("Exported " + count + " functions to " + outFile.getAbsolutePath());
    }
}
