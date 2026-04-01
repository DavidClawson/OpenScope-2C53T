//Extract and decode the ARM Cortex-M interrupt vector table
//@category Analysis

import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import java.io.*;

public class InterruptVectorTable extends GhidraScript {
    @Override
    public void run() throws Exception {
        File outFile = new File(getScriptArgs()[0]);
        PrintWriter pw = new PrintWriter(new FileWriter(outFile));

        pw.println("# ARM Cortex-M4 Interrupt Vector Table");
        pw.println("# AT32F403A / GD32F307 / STM32F1 compatible");
        pw.println("#");

        String[] vectorNames = {
            "Initial SP", "Reset", "NMI", "HardFault",
            "MemManage", "BusFault", "UsageFault", "Reserved7",
            "Reserved8", "Reserved9", "Reserved10", "SVCall",
            "DebugMon", "Reserved13", "PendSV", "SysTick",
            // IRQn 0-67 for AT32F403A
            "WWDT", "PVM", "TAMPER", "ERTC",
            "FLASH", "CRM", "EXINT0", "EXINT1",
            "EXINT2", "EXINT3", "EXINT4", "DMA1_Channel1",
            "DMA1_Channel2", "DMA1_Channel3", "DMA1_Channel4", "DMA1_Channel5",
            "DMA1_Channel6", "DMA1_Channel7", "ADC1_2", "USB_HP_CAN1_TX",
            "USB_LP_CAN1_RX0", "CAN1_RX1", "CAN1_SCE", "EXINT9_5",
            "TMR1_BRK_TMR9", "TMR1_OV_TMR10", "TMR1_TRG_HALL_TMR11", "TMR1_CH",
            "TMR2", "TMR3", "TMR4", "I2C1_EVT",
            "I2C1_ERR", "I2C2_EVT", "I2C2_ERR", "SPI1",
            "SPI2", "USART1", "USART2", "USART3",
            "EXINT15_10", "ERTCAlarm", "USBWakeUp", "TMR8_BRK_TMR12",
            "TMR8_OV_TMR13", "TMR8_TRG_HALL_TMR14", "TMR8_CH", "ADC3",
            "XMC", "SDIO1", "TMR5", "SPI3",
            "UART4", "UART5", "TMR6", "TMR7",
            "DMA2_Channel1", "DMA2_Channel2", "DMA2_Channel3", "DMA2_Channel4_5",
            "SDIO2", "I2C3_EVT", "I2C3_ERR", "SPI4",
        };

        Memory mem = currentProgram.getMemory();
        Address base = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(0x08000000);

        for (int i = 0; i < vectorNames.length && i < 84; i++) {
            try {
                int val = mem.getInt(base.add(i * 4));
                String target = String.format("0x%08X", val);

                // Look up function at target address
                String funcName = "";
                if (i > 0 && val != 0) {  // Skip SP value
                    Address targetAddr = currentProgram.getAddressFactory()
                        .getDefaultAddressSpace().getAddress(val & 0xFFFFFFFE); // Clear thumb bit
                    Function func = currentProgram.getFunctionManager()
                        .getFunctionAt(targetAddr);
                    if (func != null) {
                        funcName = " -> " + func.getName();
                    }
                }

                pw.printf("[%3d] %-28s %s%s%n", i, vectorNames[i], target, funcName);
            } catch (Exception e) {
                pw.printf("[%3d] %-28s ERROR: %s%n", i, vectorNames[i], e.getMessage());
            }
        }
        pw.close();
        println("Vector table written to " + outFile.getAbsolutePath());
    }
}
