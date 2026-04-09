// Dump a function pointer table from the current program.
// Usage:
//   DumpFunctionPointerTable.java <out-file> <base-addr-hex> <count>
//@category Analysis

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;

public class DumpFunctionPointerTable extends GhidraScript {
    private long parseHex(String s) {
        String cleaned = s.startsWith("0x") || s.startsWith("0X") ? s.substring(2) : s;
        return Long.parseUnsignedLong(cleaned, 16);
    }

    @Override
    public void run() throws Exception {
        if (getScriptArgs().length < 3) {
            throw new IllegalArgumentException(
                "Usage: DumpFunctionPointerTable.java <out-file> <base-addr-hex> <count>"
            );
        }

        File outFile = new File(getScriptArgs()[0]);
        long baseAddr = parseHex(getScriptArgs()[1]);
        int count = Integer.parseInt(getScriptArgs()[2]);

        Memory mem = currentProgram.getMemory();
        Address base = currentProgram.getAddressFactory()
            .getDefaultAddressSpace()
            .getAddress(baseAddr);

        try (PrintWriter pw = new PrintWriter(new FileWriter(outFile))) {
            pw.printf("# Function Pointer Table @ 0x%08X%n", baseAddr);
            pw.println("# idx | slot | raw | thumb_target | function_at | function_containing");

            for (int i = 0; i < count; i++) {
                Address slot = base.add(i * 4L);
                int raw = mem.getInt(slot);
                long rawUnsigned = Integer.toUnsignedLong(raw);
                long targetVal = rawUnsigned & 0xFFFFFFFEL;
                Address target = currentProgram.getAddressFactory()
                    .getDefaultAddressSpace()
                    .getAddress(targetVal);

                Function at = currentProgram.getFunctionManager().getFunctionAt(target);
                Function containing = currentProgram.getFunctionManager().getFunctionContaining(target);

                String atName = (at != null) ? at.getName() : "";
                String containingName = (containing != null) ? containing.getName() : "";

                pw.printf(
                    "%d | %s | 0x%08X | %s | %s | %s%n",
                    i,
                    slot.toString(),
                    rawUnsigned,
                    target.toString(),
                    atName,
                    containingName
                );
            }
        }

        println("Wrote table dump to " + outFile.getAbsolutePath());
    }
}
