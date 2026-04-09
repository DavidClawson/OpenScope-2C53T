// Dump raw bytes and simple 32-bit words from a memory range in the current
// Ghidra program.
//@category Analysis

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;

public class DumpMemoryRange extends GhidraScript {
    @Override
    public void run() throws Exception {
        if (getScriptArgs().length < 3) {
            printerr("Usage: DumpMemoryRange.java <out-file> <start-addr> <end-addr>");
            return;
        }

        File outFile = new File(getScriptArgs()[0]);
        Address start = currentAddress.getAddressSpace().getAddress(getScriptArgs()[1]);
        Address end = currentAddress.getAddressSpace().getAddress(getScriptArgs()[2]);

        Memory mem = currentProgram.getMemory();
        MemoryBlock block = mem.getBlock(start);

        try (PrintWriter pw = new PrintWriter(new FileWriter(outFile))) {
            pw.printf("# Dump %s .. %s%n", start, end);
            if (block != null) {
                pw.printf("# Block: %s | Start=%s | End=%s | Size=0x%X | Overlay=%s%n",
                    block.getName(), block.getStart(), block.getEnd(), block.getSize(), block.isOverlay());
            } else {
                pw.println("# Block: <none>");
            }

            long len = end.subtract(start) + 1;
            byte[] buf = new byte[(int) len];
            mem.getBytes(start, buf);

            pw.println();
            pw.println("# Hex Bytes");
            for (int i = 0; i < buf.length; i += 16) {
                long addr = start.getOffset() + i;
                pw.printf("%08X:", addr);
                for (int j = 0; j < 16 && i + j < buf.length; j++) {
                    pw.printf(" %02X", buf[i + j] & 0xff);
                }
                pw.println();
            }

            pw.println();
            pw.println("# Little-Endian 32-bit Words");
            for (int i = 0; i + 3 < buf.length; i += 4) {
                long addr = start.getOffset() + i;
                long w = ((long) buf[i] & 0xff)
                    | (((long) buf[i + 1] & 0xff) << 8)
                    | (((long) buf[i + 2] & 0xff) << 16)
                    | (((long) buf[i + 3] & 0xff) << 24);
                pw.printf("%08X: 0x%08X%n", addr, w);
            }
        }

        println("Wrote dump to " + outFile.getAbsolutePath());
    }
}
