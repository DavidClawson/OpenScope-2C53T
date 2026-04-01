//Find all references to GPIO and peripheral registers
//@category Analysis

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;

public class FindGPIOAccess extends GhidraScript {
    @Override
    public void run() throws Exception {
        File outFile = new File(getScriptArgs()[0]);
        PrintWriter pw = new PrintWriter(new FileWriter(outFile));

        pw.println("# GPIO and Peripheral Register Access Map");
        pw.println("# Searches for references to key hardware addresses");
        pw.println("#");

        // Key peripheral base addresses
        String[][] peripherals = {
            {"40010800", "GPIOA"},
            {"40010C00", "GPIOB"},
            {"40011000", "GPIOC"},
            {"40011400", "GPIOD"},
            {"40011800", "GPIOE"},
            {"40010000", "AFIO/IOMUX"},
            {"40013C00", "EXTI"},
            {"A0000000", "EXMC/XMC"},
            {"40004400", "USART2"},
            {"40004800", "USART3"},
            {"40013800", "USART1"},
            {"40003800", "SPI2"},
            {"40005400", "I2C1"},
            {"40005800", "I2C2"},
            {"40012400", "ADC1"},
            {"40012800", "ADC2"},
            {"40000000", "TIM2"},
            {"40000400", "TIM3"},
            {"40000800", "TIM4"},
            {"40012C00", "TIM1"},
        };

        // For each peripheral, search for references in code
        for (String[] periph : peripherals) {
            pw.println("\n## " + periph[1] + " (0x" + periph[0] + ")");

            // Search for DAT_ references at various offsets
            long base = Long.parseLong(periph[0], 16);
            for (int offset = 0; offset <= 0x20; offset += 4) {
                long addr = base + offset;
                String addrStr = String.format("%08x", addr);

                // Find all references to this address
                Address targetAddr = currentProgram.getAddressFactory()
                    .getDefaultAddressSpace().getAddress(addr);

                ReferenceIterator refs = currentProgram.getReferenceManager()
                    .getReferencesTo(targetAddr);

                boolean hasRefs = false;
                while (refs.hasNext()) {
                    Reference ref = refs.next();
                    Address fromAddr = ref.getFromAddress();
                    Function func = currentProgram.getFunctionManager()
                        .getFunctionContaining(fromAddr);
                    String funcName = (func != null) ? func.getName() : "unknown";

                    if (!hasRefs) {
                        pw.printf("  Offset +0x%02X (0x%s):%n", offset, addrStr);
                        hasRefs = true;
                    }
                    pw.printf("    %s in %s (%s)%n",
                        ref.getReferenceType(), funcName, fromAddr);
                }
            }
        }
        pw.close();
        println("GPIO access map written to " + outFile.getAbsolutePath());
    }
}
