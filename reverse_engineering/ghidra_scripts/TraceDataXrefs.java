// Trace data references for specific addresses and show the containing function when possible.
//@category Analysis

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressFactory;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;

public class TraceDataXrefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        if (getScriptArgs().length < 2) {
            printerr("Usage: TraceDataXrefs <outFile> <addr1,addr2,...>");
            return;
        }

        File outFile = new File(getScriptArgs()[0]);
        String addrList = getScriptArgs()[1];

        AddressFactory af = currentProgram.getAddressFactory();
        ReferenceManager rm = currentProgram.getReferenceManager();
        Listing listing = currentProgram.getListing();

        try (PrintWriter pw = new PrintWriter(new FileWriter(outFile))) {
            pw.println("# Data Xref Trace");
            pw.println();

            for (String rawAddr : addrList.split(",")) {
                String addrStr = rawAddr.trim();
                long addrVal = Long.parseLong(addrStr, 16);
                Address addr = af.getDefaultAddressSpace().getAddress(addrVal);

                pw.printf("## Target: 0x%s%n", addrStr);
                pw.printf("Address: %s%n", addr);

                ReferenceIterator refs = rm.getReferencesTo(addr);
                int count = 0;
                while (refs.hasNext()) {
                    Reference ref = refs.next();
                    Address from = ref.getFromAddress();
                    Function func = listing.getFunctionContaining(from);
                    CodeUnit cu = listing.getCodeUnitContaining(from);

                    pw.printf("- from %s", from);
                    if (func != null) {
                        pw.printf(" | function %s @ %s", func.getName(), func.getEntryPoint());
                    }
                    pw.printf(" | type %s", ref.getReferenceType());
                    if (cu != null) {
                        String text = cu.toString().replace('\n', ' ').trim();
                        pw.printf(" | code %s", text);
                    }
                    pw.println();
                    count++;
                }

                if (count == 0) {
                    pw.println("No references found.");
                }
                pw.println();
            }
        }

        println("Done writing to " + outFile.getAbsolutePath());
    }
}
