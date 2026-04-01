//Comprehensive firmware analysis: find ALL functions, decompile everything,
//map all hardware register accesses, extract data tables
//@category Analysis

import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import java.io.*;
import java.util.*;

public class ComprehensiveAnalysis extends GhidraScript {
    @Override
    public void run() throws Exception {
        String outDir = getScriptArgs()[0];

        // Step 1: Force-create functions at all uncovered code regions
        println("Step 1: Finding code gaps and creating functions...");
        findAndCreateFunctions();

        // Step 2: Full decompilation of ALL functions
        println("Step 2: Full decompilation...");
        decompileAll(outDir + "/full_decompile.c");

        // Step 3: Hardware register access map
        println("Step 3: Hardware register map...");
        mapHardwareAccess(outDir + "/hardware_map.txt");

        // Step 4: Function cross-reference map
        println("Step 4: Cross-reference map...");
        exportXrefs(outDir + "/xref_map.txt");

        // Step 5: RAM variable usage map
        println("Step 5: RAM usage map...");
        exportRAMUsage(outDir + "/ram_map.txt");

        println("=== COMPREHENSIVE ANALYSIS COMPLETE ===");
    }

    private void findAndCreateFunctions() throws Exception {
        // Scan code section for function prologues that Ghidra missed
        Memory mem = currentProgram.getMemory();
        Address codeStart = toAddr(0x08000000);
        Address codeEnd = toAddr(0x08046000);

        int created = 0;
        Address addr = codeStart;

        while (addr.compareTo(codeEnd) < 0 && !monitor.isCancelled()) {
            // Check if this address is already in a function
            Function existing = currentProgram.getFunctionManager().getFunctionContaining(addr);

            if (existing == null) {
                // Check if there's valid code here (look for ARM thumb instructions)
                try {
                    short instr = mem.getShort(addr);
                    // Common ARM Cortex-M function prologues:
                    // PUSH {r4-r7, lr} = 0xB5xx
                    // PUSH {r3-r7, lr} = 0xB5xx
                    // PUSH {lr} = 0xB500
                    if ((instr & 0xFF00) == 0xB500 || (instr & 0xFF00) == 0xB400) {
                        // Try to create a function here
                        CreateFunctionCmd cmd = new CreateFunctionCmd(addr);
                        if (cmd.applyTo(currentProgram, monitor)) {
                            created++;
                        }
                    }
                } catch (Exception e) {
                    // Skip unreadable memory
                }
            } else {
                // Skip past this function
                addr = existing.getBody().getMaxAddress();
            }

            addr = addr.add(2); // Thumb instructions are 2-byte aligned
        }
        println("Created " + created + " new functions from code gaps");
    }

    private void decompileAll(String outPath) throws Exception {
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        decomp.setSimplificationStyle("decompile");

        PrintWriter pw = new PrintWriter(new FileWriter(outPath));
        pw.println("// ============================================");
        pw.println("// COMPREHENSIVE DECOMPILATION");
        pw.println("// FNIRSI 2C53T V1.2.0 Firmware");
        pw.println("// Generated: " + new java.util.Date());
        pw.println("// ============================================");
        pw.println();

        FunctionIterator funcs = currentProgram.getListing().getFunctions(true);
        int count = 0;
        int failed = 0;

        while (funcs.hasNext() && !monitor.isCancelled()) {
            Function func = funcs.next();
            DecompileResults results = decomp.decompileFunction(func, 120, monitor);

            pw.println("// ==========================================");
            pw.println("// Function: " + func.getName() + " @ " + func.getEntryPoint());
            pw.printf("// Size: %d bytes | Callers: %d | Callees: %d%n",
                func.getBody().getNumAddresses(),
                func.getCallingFunctions(monitor).size(),
                func.getCalledFunctions(monitor).size());
            pw.println("// ==========================================");

            if (results.decompileCompleted()) {
                DecompiledFunction df = results.getDecompiledFunction();
                if (df != null) {
                    pw.println(df.getC());
                    count++;
                } else {
                    pw.println("// [DECOMPILE RETURNED NULL]");
                    failed++;
                }
            } else {
                pw.println("// [DECOMPILE FAILED: " + results.getErrorMessage() + "]");
                failed++;
            }
            pw.println();
        }

        pw.close();
        decomp.dispose();
        println("Decompiled " + count + " functions (" + failed + " failed) to " + outPath);
    }

    private void mapHardwareAccess(String outPath) throws Exception {
        PrintWriter pw = new PrintWriter(new FileWriter(outPath));
        pw.println("# Complete Hardware Register Access Map");
        pw.println("# Every reference to GPIO, timer, USART, SPI, I2C, ADC, EXMC, DMA registers");
        pw.println();

        // Comprehensive peripheral map
        long[][] peripherals = {
            // GPIO ports
            {0x40010800, 0x40010820, 0}, // GPIOA
            {0x40010C00, 0x40010C20, 1}, // GPIOB
            {0x40011000, 0x40011020, 2}, // GPIOC
            {0x40011400, 0x40011420, 3}, // GPIOD
            {0x40011800, 0x40011820, 4}, // GPIOE
            // AFIO/IOMUX
            {0x40010000, 0x40010040, 5},
            // EXTI
            {0x40013C00, 0x40013C20, 6},
            // Timers
            {0x40000000, 0x40000054, 7},  // TIM2
            {0x40000400, 0x40000454, 8},  // TIM3
            {0x40000800, 0x40000854, 9},  // TIM4
            {0x40012C00, 0x40012C54, 10}, // TIM1
            // USART
            {0x40013800, 0x40013820, 11}, // USART1
            {0x40004400, 0x40004420, 12}, // USART2
            {0x40004800, 0x40004820, 13}, // USART3
            // SPI
            {0x40003800, 0x40003820, 14}, // SPI2
            {0x40013000, 0x40013020, 15}, // SPI1
            // I2C
            {0x40005400, 0x40005420, 16}, // I2C1
            {0x40005800, 0x40005820, 17}, // I2C2
            // ADC
            {0x40012400, 0x40012460, 18}, // ADC1
            {0x40012800, 0x40012860, 19}, // ADC2
            // DMA
            {0x40020000, 0x40020090, 20}, // DMA1
            {0x40020400, 0x40020490, 21}, // DMA2
            // EXMC/XMC
            {0xA0000000, 0xA0000110, 22},
            // RCC/CRM
            {0x40021000, 0x40021030, 23},
            // DAC
            {0x40007400, 0x40007440, 24},
        };

        String[] periphNames = {
            "GPIOA", "GPIOB", "GPIOC", "GPIOD", "GPIOE",
            "AFIO/IOMUX", "EXTI",
            "TIM2", "TIM3", "TIM4", "TIM1",
            "USART1", "USART2", "USART3",
            "SPI2", "SPI1", "I2C1", "I2C2",
            "ADC1", "ADC2", "DMA1", "DMA2",
            "EXMC/XMC", "RCC/CRM", "DAC"
        };

        for (int p = 0; p < peripherals.length; p++) {
            long start = peripherals[p][0];
            long end = peripherals[p][1];
            boolean hasAny = false;

            for (long regAddr = start; regAddr < end; regAddr += 4) {
                Address targetAddr = toAddr(regAddr);
                ReferenceIterator refs = currentProgram.getReferenceManager()
                    .getReferencesTo(targetAddr);

                boolean first = true;
                while (refs.hasNext()) {
                    Reference ref = refs.next();
                    Address fromAddr = ref.getFromAddress();
                    Function func = currentProgram.getFunctionManager()
                        .getFunctionContaining(fromAddr);
                    String funcName = (func != null) ? func.getName() + "@" + func.getEntryPoint() : "unknown@" + fromAddr;

                    if (!hasAny) {
                        pw.println("## " + periphNames[p] + " (0x" + String.format("%08X", start) + ")");
                        hasAny = true;
                    }
                    if (first) {
                        pw.printf("  +0x%02X (0x%08X):%n", regAddr - start, regAddr);
                        first = false;
                    }
                    pw.printf("    %s %s [%s]%n", ref.getReferenceType(), funcName, fromAddr);
                }
            }
            if (hasAny) pw.println();
        }

        pw.close();
        println("Hardware map written to " + outPath);
    }

    private void exportXrefs(String outPath) throws Exception {
        PrintWriter pw = new PrintWriter(new FileWriter(outPath));
        pw.println("# Function Cross-Reference Map");
        pw.println("# Format: Function @ Address (Size bytes) [CallerCount -> CalleeCount]");
        pw.println("#   CALLED_BY: list of callers");
        pw.println("#   CALLS: list of callees");
        pw.println();

        FunctionIterator funcs = currentProgram.getListing().getFunctions(true);
        while (funcs.hasNext() && !monitor.isCancelled()) {
            Function func = funcs.next();
            Set<Function> callers = func.getCallingFunctions(monitor);
            Set<Function> callees = func.getCalledFunctions(monitor);

            pw.printf("## %s @ %s (%d bytes) [%d callers -> %d callees]%n",
                func.getName(), func.getEntryPoint(),
                func.getBody().getNumAddresses(),
                callers.size(), callees.size());

            if (!callers.isEmpty()) {
                pw.print("  CALLED_BY: ");
                for (Function caller : callers) {
                    pw.print(caller.getName() + "@" + caller.getEntryPoint() + " ");
                }
                pw.println();
            }
            if (!callees.isEmpty()) {
                pw.print("  CALLS: ");
                for (Function callee : callees) {
                    pw.print(callee.getName() + "@" + callee.getEntryPoint() + " ");
                }
                pw.println();
            }
            pw.println();
        }
        pw.close();
        println("Xref map written to " + outPath);
    }

    private void exportRAMUsage(String outPath) throws Exception {
        PrintWriter pw = new PrintWriter(new FileWriter(outPath));
        pw.println("# RAM Variable Usage Map");
        pw.println("# All labeled addresses in SRAM (0x20000000-0x2003FFFF)");
        pw.println("# with list of functions that reference them");
        pw.println();

        SymbolTable symTable = currentProgram.getSymbolTable();
        SymbolIterator symbols = symTable.getAllSymbols(true);

        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol sym = symbols.next();
            long offset = sym.getAddress().getOffset();
            if (offset < 0x20000000 || offset >= 0x20040000) continue;

            ReferenceIterator refs = currentProgram.getReferenceManager()
                .getReferencesTo(sym.getAddress());

            Set<String> refFuncs = new LinkedHashSet<>();
            int refCount = 0;
            while (refs.hasNext()) {
                Reference ref = refs.next();
                Function func = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (func != null) {
                    refFuncs.add(func.getName() + "@" + func.getEntryPoint());
                } else {
                    refFuncs.add("unknown@" + ref.getFromAddress());
                }
                refCount++;
            }

            if (refCount > 0) {
                pw.printf("0x%08X %s (%d refs): %s%n",
                    offset, sym.getName(), refCount,
                    String.join(", ", refFuncs));
            }
        }
        pw.close();
        println("RAM map written to " + outPath);
    }
}
