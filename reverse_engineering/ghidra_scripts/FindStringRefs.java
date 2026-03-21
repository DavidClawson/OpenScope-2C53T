//Find functions that reference string-region addresses via literal pools
//@category Analysis

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.lang.OperandType;
import ghidra.program.model.symbol.*;

public class FindStringRefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        FunctionManager funcMgr = currentProgram.getFunctionManager();
        Listing listing = currentProgram.getListing();
        AddressFactory af = currentProgram.getAddressFactory();
        
        // String region is approximately 0x080b4000 to 0x080b7680
        long stringStart = 0x080b4000L;
        long stringEnd = 0x080b7680L;
        
        // Iterate all instructions looking for references to the string region
        InstructionIterator instIter = listing.getInstructions(true);
        while (instIter.hasNext() && !monitor.isCancelled()) {
            Instruction inst = instIter.next();
            
            // Check all references from this instruction
            Reference[] refs = inst.getReferencesFrom();
            for (Reference ref : refs) {
                long targetAddr = ref.getToAddress().getOffset();
                if (targetAddr >= stringStart && targetAddr < stringEnd) {
                    Function func = funcMgr.getFunctionContaining(inst.getAddress());
                    String funcName = func != null ? func.getName() + " @ " + func.getEntryPoint() : "no_function";
                    
                    // Try to read the string at the target
                    Data data = listing.getDataAt(ref.getToAddress());
                    String strVal = "";
                    if (data != null && data.getValue() != null) {
                        strVal = data.getValue().toString();
                    } else {
                        // Try to read raw bytes as string
                        try {
                            byte[] bytes = new byte[80];
                            currentProgram.getMemory().getBytes(ref.getToAddress(), bytes);
                            StringBuilder sb = new StringBuilder();
                            for (byte b : bytes) {
                                if (b == 0) break;
                                if (b >= 0x20 && b < 0x7f) sb.append((char)b);
                                else sb.append('.');
                            }
                            strVal = sb.toString();
                        } catch (Exception e) {
                            strVal = "?";
                        }
                    }
                    
                    println(String.format("%s | 0x%s | \"%s\" | ref_at 0x%s",
                        funcName, ref.getToAddress(), strVal, inst.getAddress()));
                }
            }
        }
    }
}
