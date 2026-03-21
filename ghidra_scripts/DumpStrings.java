//Dump all defined strings with their addresses
//@category Analysis
//@keybinding
//@menupath
//@toolbar

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

public class DumpStrings extends GhidraScript {
    @Override
    public void run() throws Exception {
        Listing listing = currentProgram.getListing();
        DataIterator dataIterator = listing.getDefinedData(true);
        while (dataIterator.hasNext() && !monitor.isCancelled()) {
            Data data = dataIterator.next();
            DataType dt = data.getDataType();
            if (dt instanceof StringDataType || dt instanceof TerminatedStringDataType ||
                dt instanceof UnicodeDataType || dt.getName().contains("string") ||
                dt.getName().contains("String")) {
                Object val = data.getValue();
                if (val != null) {
                    String value = val.toString();
                    if (value.length() > 2) {
                        println(data.getAddress().toString() + ": " + value);
                    }
                }
            }
        }
    }
}
