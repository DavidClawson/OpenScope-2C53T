//Apply known function and variable names to the 2C53T firmware
//@category Analysis

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;

public class ApplyNames extends GhidraScript {

    private int funcCount = 0;
    private int varCount = 0;
    private int failCount = 0;

    @Override
    public void run() throws Exception {
        // ===================================================================
        // Task entry points (real addresses from freertos_tasks.md)
        // Ghidra addr = real addr - 0x7000
        // These are Thumb functions, so we OR with 1 for createFunction but
        // use even address for the actual function location.
        // ===================================================================
        createAndName(0x08036A50L, "display_task");
        createAndName(0x08039008L, "key_task");
        createAndName(0x0803909CL, "osc_task");
        createAndName(0x08037454L, "fpga_task");
        createAndName(0x080373F4L, "dvom_tx_task");
        createAndName(0x08036AC0L, "dvom_rx_task");

        // Internal FreeRTOS tasks (real addresses - 0x7000)
        // IDLE: real 0x0803BECD -> Ghidra 0x08034ECD (Thumb, odd address)
        createAndName(0x08034ECDL, "freertos_idle_task");
        // Tmr Svc: real 0x0803C79D -> Ghidra 0x0803579DL
        createAndName(0x0803579DL, "freertos_timer_service_task");

        // ===================================================================
        // FreeRTOS API functions (Ghidra addresses from freertos_tasks.md)
        // These are already listed as FUN_08xxxxxx = Ghidra addresses
        // ===================================================================
        renameFunc(0x0803b6a0L, "xTaskCreate");
        renameFunc(0x0803a6d8L, "vTaskStartScheduler");
        renameFunc(0x0803a610L, "vTaskResume");
        renameFunc(0x0803a78cL, "vTaskSuspend");
        renameFunc(0x0803a904L, "vTaskSuspendAll");
        renameFunc(0x0803a390L, "vTaskDelay");
        renameFunc(0x0803ab74L, "xQueueGenericCreate");
        renameFunc(0x0803acf0L, "xQueueGenericSend");
        renameFunc(0x0803b1d8L, "xQueueReceive");
        renameFunc(0x0803b3a8L, "xQueueSemaphoreTake");
        renameFunc(0x0803ac38L, "xQueueReset");
        renameFunc(0x0803bd88L, "xTimerCreate");
        renameFunc(0x08035c64L, "pvPortMalloc");
        renameFunc(0x0803a1e0L, "vPortFree");
        renameFunc(0x0803a168L, "taskENTER_CRITICAL");
        renameFunc(0x0803a1b0L, "taskEXIT_CRITICAL");

        // ===================================================================
        // Display primitives (from function_map.md - these are Ghidra addrs)
        // Note: addresses like 08032f6c are Ghidra addresses already
        // ===================================================================
        renameFunc(0x08032f6cL, "draw_text");
        renameFunc(0x08008154L, "draw_ui_element");
        renameFunc(0x080003b4L, "sprintf_to_buffer");
        renameFunc(0x08033cfcL, "memory_alloc");

        // ===================================================================
        // GPIO / Hardware (Ghidra addresses from function_map.md)
        // ===================================================================
        renameFunc(0x080302fcL, "gpio_configure_pins");
        renameFunc(0x080304e0L, "gpio_read_pin");

        // ===================================================================
        // I2C Touch Panel (Ghidra addresses from function_map.md)
        // ===================================================================
        renameFunc(0x08036848L, "i2c_init");
        renameFunc(0x08036830L, "i2c_enable");
        renameFunc(0x0803683cL, "i2c_transfer");

        // ===================================================================
        // UI Screens (Ghidra addresses from function_map.md)
        // ===================================================================
        renameFunc(0x08019e98L, "main_event_loop");
        renameFunc(0x08015f50L, "draw_oscilloscope_screen");
        renameFunc(0x0800e79cL, "draw_measurement_values");
        renameFunc(0x0800ec70L, "draw_measurement_display");
        renameFunc(0x0800bd84L, "draw_mode_label");
        renameFunc(0x0800bde0L, "draw_channel_info");
        renameFunc(0x08018324L, "draw_settings_menu");
        renameFunc(0x080096e8L, "draw_status_indicator_1");
        renameFunc(0x08009a94L, "draw_status_indicator_2");

        // ===================================================================
        // Interrupt handlers (Ghidra addresses from function_map.md)
        // Note: These are odd (Thumb bit set), need even address for function
        // ===================================================================
        createAndName(0x0802A995L, "systick_handler");
        createAndName(0x08009C11L, "exti3_continuity_handler");
        createAndName(0x08009671L, "dma1_ch2_handler");
        createAndName(0x0802E8E5L, "usb_interrupt_handler");
        createAndName(0x0802E71DL, "timer3_interrupt_handler");
        createAndName(0x0802E7B5L, "usart2_irq_handler");
        createAndName(0x0802E78DL, "timer8_break_handler");

        // ===================================================================
        // Corrected functions from fpga_protocol.md (Ghidra addresses)
        // ===================================================================
        renameFunc(0x0802e7bcL, "fatfs_init");
        renameFunc(0x080277b4L, "usart2_isr_real");
        renameFunc(0x0802d534L, "fatfs_read_write");
        renameFunc(0x0802dc40L, "fatfs_open");
        renameFunc(0x0802d8b8L, "fatfs_open_read");
        renameFunc(0x0802d80cL, "fatfs_mount");
        renameFunc(0x0802cfbcL, "fatfs_operation");

        // ===================================================================
        // USART2 protocol handler (from function_map.md, Ghidra addr)
        // Already renamed above as fatfs_init per fpga_protocol.md correction

        // ===================================================================
        // Named global variables (RAM addresses - label in address space)
        // ===================================================================
        labelAddress(0x20008350L, "viewport_x");
        labelAddress(0x20008352L, "viewport_y");
        labelAddress(0x20008354L, "viewport_width");
        labelAddress(0x20008356L, "viewport_height");
        labelAddress(0x20008358L, "framebuffer_ptr");
        labelAddress(0x20008360L, "string_format_buffer");
        labelAddress(0x20001078L, "font_base_ptr");
        labelAddress(0x2000107cL, "font_table_ptr");
        labelAddress(0x20001080L, "font_loaded_flag");
        labelAddress(0x20000125L, "device_mode");
        labelAddress(0x20001058L, "current_mode_index");
        labelAddress(0x20001062L, "ui_state_flags");
        labelAddress(0x20001061L, "ui_sub_state");
        labelAddress(0x20001063L, "ui_state_flag_2");
        labelAddress(0x200000fcL, "system_state");
        labelAddress(0x200000fdL, "system_flag");
        labelAddress(0x2000010cL, "config_value");
        labelAddress(0x20000130L, "setting_value");

        // FPGA communication buffers
        labelAddress(0x20000005L, "usart2_tx_buffer");
        labelAddress(0x2000000fL, "usart2_tx_byte_index");
        labelAddress(0x20000018L, "fpga_cmd_status_struct");
        labelAddress(0x20000025L, "fpga_command_code");
        labelAddress(0x20004E10L, "usart2_rx_byte_index");
        labelAddress(0x20004E11L, "usart2_rx_buffer");
        labelAddress(0x20004E14L, "rx_echo_verify_byte");
        labelAddress(0x20004E18L, "rx_integrity_marker");

        // Queue/semaphore handles
        labelAddress(0x20002D6CL, "display_queue_handle");
        labelAddress(0x20002D70L, "key_queue_handle");
        labelAddress(0x20002D74L, "dvom_tx_queue_handle");
        labelAddress(0x20002D78L, "fpga_queue_handle");
        labelAddress(0x20002D7CL, "dvom_rx_semaphore");
        labelAddress(0x20002D80L, "osc_semaphore");
        labelAddress(0x20002D84L, "key_semaphore");

        println("=== ApplyNames Summary ===");
        println("Functions named/created: " + funcCount);
        println("Variables labeled: " + varCount);
        println("Failed operations: " + failCount);
    }

    private void createAndName(long addr, String name) {
        try {
            Address a = toAddr(addr & ~1L); // Clear Thumb bit for function address
            Function existing = getFunctionAt(a);
            if (existing == null) {
                // Try to create the function
                existing = createFunction(a, name);
                if (existing != null) {
                    println("Created function: " + name + " @ " + a);
                    funcCount++;
                } else {
                    // Try disassembling first
                    disassemble(a);
                    existing = createFunction(a, name);
                    if (existing != null) {
                        println("Created function (after disasm): " + name + " @ " + a);
                        funcCount++;
                    } else {
                        println("FAILED to create function: " + name + " @ " + a);
                        failCount++;
                    }
                }
            } else {
                existing.setName(name, SourceType.USER_DEFINED);
                println("Renamed existing function: " + name + " @ " + a);
                funcCount++;
            }
        } catch (Exception e) {
            println("ERROR creating/naming " + name + " @ 0x" + Long.toHexString(addr) + ": " + e.getMessage());
            failCount++;
        }
    }

    private void renameFunc(long addr, String name) {
        try {
            Address a = toAddr(addr);
            Function func = getFunctionAt(a);
            if (func == null) {
                // Function doesn't exist yet at this address, try to create it
                func = createFunction(a, name);
                if (func != null) {
                    println("Created and named: " + name + " @ " + a);
                    funcCount++;
                } else {
                    disassemble(a);
                    func = createFunction(a, name);
                    if (func != null) {
                        println("Created (after disasm) and named: " + name + " @ " + a);
                        funcCount++;
                    } else {
                        println("FAILED to rename (no function at): " + name + " @ " + a);
                        failCount++;
                    }
                }
            } else {
                func.setName(name, SourceType.USER_DEFINED);
                println("Renamed: " + name + " @ " + a);
                funcCount++;
            }
        } catch (Exception e) {
            println("ERROR renaming " + name + " @ 0x" + Long.toHexString(addr) + ": " + e.getMessage());
            failCount++;
        }
    }

    private void labelAddress(long addr, String name) {
        try {
            Address a = toAddr(addr);
            SymbolTable st = currentProgram.getSymbolTable();
            st.createLabel(a, name, SourceType.USER_DEFINED);
            println("Labeled: " + name + " @ " + a);
            varCount++;
        } catch (Exception e) {
            println("ERROR labeling " + name + " @ 0x" + Long.toHexString(addr) + ": " + e.getMessage());
            failCount++;
        }
    }
}
