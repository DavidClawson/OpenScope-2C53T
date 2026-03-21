//Trace string references to find which functions use them
//@category Analysis

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;

public class TraceStrings extends GhidraScript {
    @Override
    public void run() throws Exception {
        ReferenceManager refMgr = currentProgram.getReferenceManager();
        Listing listing = currentProgram.getListing();
        FunctionManager funcMgr = currentProgram.getFunctionManager();

        DataIterator dataIterator = listing.getDefinedData(true);
        while (dataIterator.hasNext() && !monitor.isCancelled()) {
            Data data = dataIterator.next();
            DataType dt = data.getDataType();
            if (dt instanceof StringDataType || dt instanceof TerminatedStringDataType ||
                dt.getName().contains("string") || dt.getName().contains("String")) {
                Object val = data.getValue();
                if (val == null) continue;
                String value = val.toString();
                if (value.length() < 3) continue;

                Reference[] refs = refMgr.getReferencesTo(data.getAddress());
                for (Reference ref : refs) {
                    Address fromAddr = ref.getFromAddress();
                    Function func = funcMgr.getFunctionContaining(fromAddr);
                    String funcName = func != null ? func.getName() + " @ " + func.getEntryPoint() : "unknown";
                    println(String.format("STRING 0x%s: \"%s\" -> USED BY %s (ref at 0x%s)",
                        data.getAddress(), value, funcName, fromAddr));
                }
                if (refs.length == 0) {
                    println(String.format("STRING 0x%s: \"%s\" -> NO REFERENCES FOUND",
                        data.getAddress(), value));
                }
            }
        }
    }
}
