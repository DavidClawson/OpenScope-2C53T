//Export all labeled data addresses in RAM/flash with their values and cross-references
//@category Analysis

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;
import java.io.*;

public class DataMap extends GhidraScript {
    @Override
    public void run() throws Exception {
        File outFile = new File(getScriptArgs()[0]);
        PrintWriter pw = new PrintWriter(new FileWriter(outFile));

        pw.println("# Global Data Map");
        pw.println("# Address | Name | Size | Value | NumRefs | RefFunctions");
        pw.println("#");

        SymbolTable symTable = currentProgram.getSymbolTable();
        SymbolIterator symbols = symTable.getAllSymbols(true);
        Memory mem = currentProgram.getMemory();
        int count = 0;

        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol sym = symbols.next();
            if (sym.getSymbolType() != SymbolType.LABEL) continue;

            Address addr = sym.getAddress();
            long offset = addr.getOffset();

            // Only interested in RAM (0x20000000-0x2003FFFF) and peripheral (0x40000000+)
            // and flash data (0x08040000+)
            if (offset < 0x20000000) continue;
            if (offset >= 0x20040000 && offset < 0x40000000) continue;
            if (offset >= 0x40020000 && offset < 0xA0000000) continue;
            if (offset >= 0xA0001000) continue;

            // Get references to this data
            ReferenceIterator refs = currentProgram.getReferenceManager()
                .getReferencesTo(addr);
            StringBuilder refFuncs = new StringBuilder();
            int refCount = 0;
            while (refs.hasNext()) {
                Reference ref = refs.next();
                Function func = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (func != null) {
                    if (refFuncs.length() > 0) refFuncs.append(",");
                    refFuncs.append(func.getName());
                }
                refCount++;
            }

            // Try to read value
            String value = "";
            try {
                if (offset >= 0x08000000 && offset < 0x08100000) {
                    int val = mem.getInt(addr);
                    value = String.format("0x%08X", val);
                }
            } catch (Exception e) {
                value = "N/A";
            }

            if (refCount > 0) {
                pw.printf("0x%08X | %s | %d | %s | %s%n",
                    offset, sym.getName(), refCount, value, refFuncs.toString());
                count++;
            }
        }
        pw.close();
        println("Exported " + count + " data symbols to " + outFile.getAbsolutePath());
    }
}
