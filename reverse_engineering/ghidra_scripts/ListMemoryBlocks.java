// List memory blocks in the current program so we can sanity-check the
// imported ROM map before doing targeted RE passes.
//@category Analysis

import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;

public class ListMemoryBlocks extends GhidraScript {
    @Override
    public void run() throws Exception {
        File outFile = new File(getScriptArgs()[0]);
        PrintWriter pw = new PrintWriter(new FileWriter(outFile));

        Memory mem = currentProgram.getMemory();
        pw.println("# Memory Blocks");
        pw.println("# Name | Start | End | Length | Initialized | R | W | X | Volatile | Overlay");

        for (MemoryBlock block : mem.getBlocks()) {
            pw.printf(
                "%s | %s | %s | 0x%X | %s | %s | %s | %s | %s | %s%n",
                block.getName(),
                block.getStart(),
                block.getEnd(),
                block.getSize(),
                block.isInitialized(),
                block.isRead(),
                block.isWrite(),
                block.isExecute(),
                block.isVolatile(),
                block.isOverlay()
            );
        }

        pw.close();
        println("Wrote memory blocks to " + outFile.getAbsolutePath());
    }
}
