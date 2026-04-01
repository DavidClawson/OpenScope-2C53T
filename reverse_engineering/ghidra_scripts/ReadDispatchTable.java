//Read the button dispatch table at 0x8046544
//@category Analysis
import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import java.io.*;
public class ReadDispatchTable extends GhidraScript {
    @Override
    public void run() throws Exception {
        File outFile = new File(getScriptArgs()[0]);
        PrintWriter pw = new PrintWriter(new FileWriter(outFile));
        pw.println("# Button Dispatch Table at 0x08046544");
        Memory mem = currentProgram.getMemory();
        Address base = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(0x08046544);
        for (int i = 0; i < 32; i++) {
            try {
                int val = mem.getInt(base.add(i * 4));
                Address target = currentProgram.getAddressFactory()
                    .getDefaultAddressSpace().getAddress(val & 0xFFFFFFFE);
                Function func = currentProgram.getFunctionManager().getFunctionAt(target);
                String funcName = (func != null) ? func.getName() : "unknown";
                pw.printf("[%2d] 0x%08X -> %s%n", i, val, funcName);
            } catch (Exception e) {
                pw.printf("[%2d] ERROR%n", i);
            }
        }
        pw.close();
        println("Done");
    }
}
