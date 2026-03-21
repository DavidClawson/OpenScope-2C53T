//Find all references to addresses in range 0x080b0000-0x080c0000
//@category Analysis

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;

public class FindAllStringRefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        FunctionManager funcMgr = currentProgram.getFunctionManager();
        Listing listing = currentProgram.getListing();
        
        // Scan all data in the code region for pointer values to string area
        // ARM Thumb uses literal pools: the instruction loads from a nearby
        // 4-byte value that contains the target address
        
        long codeStart = 0x08000000L;
        long codeEnd = 0x08040000L;
        long stringStart = 0x08040000L;
        long stringEnd = 0x080c0000L;
        
        byte[] mem = new byte[(int)(codeEnd - codeStart)];
        currentProgram.getMemory().getBytes(
            currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(codeStart), 
            mem);
        
        // Scan for 4-byte little-endian values that point to string region
        java.util.Map<Long, java.util.List<Long>> funcToStrings = new java.util.TreeMap<>();
        
        for (int i = 0; i < mem.length - 4; i += 2) {
            long val = (mem[i] & 0xFFL) | ((mem[i+1] & 0xFFL) << 8) | 
                       ((mem[i+2] & 0xFFL) << 16) | ((mem[i+3] & 0xFFL) << 24);
            
            if (val >= stringStart && val < stringEnd) {
                long instrAddr = codeStart + i;
                Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(instrAddr);
                Function func = funcMgr.getFunctionContaining(addr);
                
                // Read string at that address
                long strOffset = val - codeStart;
                byte[] fullMem = new byte[(int)(stringEnd - codeStart)];
                try {
                    currentProgram.getMemory().getBytes(
                        currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(codeStart),
                        fullMem);
                } catch (Exception e) { continue; }
                
                StringBuilder sb = new StringBuilder();
                for (int j = (int)strOffset; j < fullMem.length && j < strOffset + 80; j++) {
                    byte b = fullMem[j];
                    if (b == 0) break;
                    if (b >= 0x20 && b < 0x7f) sb.append((char)b);
                }
                
                String strVal = sb.toString();
                if (strVal.length() >= 3) {
                    String funcName = func != null ? func.getName() + " @ " + func.getEntryPoint() : "literal_pool";
                    println(String.format("%s | 0x%08X | \"%s\" | pool_at 0x%08X",
                        funcName, val, strVal, instrAddr));
                }
            }
        }
    }
}
