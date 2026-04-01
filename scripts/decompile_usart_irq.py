#!/usr/bin/env python3
"""
Comprehensive disassembly and annotation of the USART2 IRQ handler and related
interrupt infrastructure in the FNIRSI 2C53T V1.2.0 firmware.

Target: AT32F403A (ARM Cortex-M4F, Thumb-2)
Firmware: APP_2C53T_V1.2.0_251015.bin @ base 0x08000000
USART2 peripheral: 0x40004400 (command channel to Gowin FPGA)

Output: reverse_engineering/analysis_v120/usart_protocol_decompile.txt
"""

import struct
import sys
import os

# Add venv to path
sys.path.insert(0, "/Users/david/Desktop/osc/emulator/.venv/lib/python3.14/site-packages")

from capstone import *

FIRMWARE_PATH = os.path.join(os.path.dirname(__file__), "..",
    "2C53T Firmware V1.2.0", "APP_2C53T_V1.2.0_251015.bin")
OUTPUT_PATH = os.path.join(os.path.dirname(__file__), "..",
    "reverse_engineering", "analysis_v120", "usart_protocol_decompile.txt")

BASE = 0x08000000

# Known function addresses
KNOWN_FUNCS = {
    0x0803b09c: "xQueueGenericSendFromISR",
    0x0803acf0: "xQueueGenericSend",
    0x0803a610: "vTaskResume",
    0x0803a78c: "vTaskSuspend",
    0x0803af08: "xQueueReceiveFromISR",
    0x0803b1d8: "xQueueReceive",
    0x080302fc: "gpio_pin_config",
    0x0802a430: "some_init_func",
    0x08039990: "vPortEnterCritical_or_similar",
    0x08039b24: "usart_hal_function",
}

# AT32 USART2 register map
USART_REGS = {
    0x40004400: "USART2_STS   (Status)",
    0x40004404: "USART2_DATA  (Data)",
    0x40004408: "USART2_BRR   (Baud Rate)",
    0x4000440C: "USART2_CTRL1 (Control 1)",
}

# AT32 USART_STS bit definitions
STS_BITS = {
    0: "PERR  (Parity error)",
    1: "FERR  (Framing error)",
    2: "NERR  (Noise error)",
    3: "ROERR (Receive overrun error)",
    4: "IDLEF (IDLE frame detected)",
    5: "RDBF  (Receive data buffer full)",
    6: "TDC   (Transmit data complete)",
    7: "TDBE  (Transmit data buffer empty)",
    8: "BFF   (Break frame flag)",
    9: "CTSCF (CTS change flag)",
}

# AT32 USART_CTRL1 bit definitions
CTRL1_BITS = {
    0: "SBF    (Send break frame)",
    1: "RECMUTE (Receiver mute)",
    2: "REN    (Receiver enable)",
    3: "TEN    (Transmitter enable)",
    4: "IDLEIEN (IDLE interrupt enable)",
    5: "RDBFIEN (Receive data buffer full interrupt enable)",
    6: "TDCIEN (Transmit data complete interrupt enable)",
    7: "TDBEIEN (Transmit data buffer empty interrupt enable)",
    8: "PERRIEN (Parity error interrupt enable)",
    9: "PEN    (Parity enable)",
    10: "PSEL   (Parity selection: 0=even, 1=odd)",
    11: "WKMETHOD (Wakeup method)",
    12: "DBN    (Data bit number: 0=8bit, 1=9bit)",
    13: "UEN    (USART enable)",
}

# RAM addresses
RAM_VARS = {
    0x20000005: "usart2_tx_buffer[0]  (10-byte TX frame to FPGA)",
    0x2000000F: "usart2_tx_index      (current TX byte position)",
    0x20004E10: "usart2_rx_index      (current RX byte position)",
    0x20004E11: "usart2_rx_buffer[0]  (RX frame from FPGA)",
    0x200000F8: "global_state_base    (base for many state variables)",
    0x20001060: "device_mode          (0=scope_ch1, 1=scope_dual, 2=siggen, etc.)",
    0x20002D7C: "fpga_rx_queue_handle (FreeRTOS queue for FPGA responses)",
    0x20002DA0: "fpga_task_handle_1   (FreeRTOS task handle)",
    0x20002DA4: "fpga_task_handle_2   (FreeRTOS task handle)",
    0x20002D6C: "fpga_cmd_queue_handle (FreeRTOS queue for FPGA commands)",
}


def load_firmware():
    with open(FIRMWARE_PATH, "rb") as f:
        return f.read()


def disasm_range(md, data, start, end):
    """Disassemble a range and return list of (address, mnemonic, op_str, bytes)."""
    offset = start - BASE
    code = data[offset:offset + (end - start)]
    result = []
    for insn in md.disasm(code, start):
        result.append((insn.address, insn.mnemonic, insn.op_str, insn.bytes))
    return result


def read32(data, addr):
    """Read a 32-bit word from firmware at given address."""
    offset = addr - BASE
    if 0 <= offset < len(data) - 3:
        return struct.unpack_from("<I", data, offset)[0]
    return None


def format_insn(addr, mnemonic, op_str, annotation=""):
    """Format a single instruction with optional annotation."""
    line = f"  0x{addr:08X}:  {mnemonic:10s} {op_str}"
    if annotation:
        line = f"{line:55s} ; {annotation}"
    return line


def annotate_usart2_handler(insns, data):
    """Produce fully annotated disassembly of the USART2 ISR."""
    lines = []
    lines.append("=" * 90)
    lines.append("USART2 ISR — usart2_irq_handler @ 0x080277B4")
    lines.append("=" * 90)
    lines.append("")
    lines.append("FUNCTION SIGNATURE: void usart2_irq_handler(void)")
    lines.append("CALLED BY: USART2 global interrupt (IRQ 38, exception 54)")
    lines.append("           Vector table at offset 0xD8 = 0x0802E7B5 (see NOTE below)")
    lines.append("           Actually dispatched via mode-switch handler (see analysis)")
    lines.append("")
    lines.append("PURPOSE: Handles both TX and RX for USART2 (FPGA command channel)")
    lines.append("  - TX: Pumps 10 bytes from tx_buffer[0x20000005] one at a time")
    lines.append("  - RX: Receives bytes, validates sync headers, parses frames")
    lines.append("  - On complete valid RX frame: sends to FreeRTOS queue + triggers PendSV")
    lines.append("")
    lines.append("REGISTER USAGE:")
    lines.append("  r4 = 0x4000440C (USART2_CTRL1 — control register 1)")
    lines.append("  ip = 0x20004E10 (rx_index pointer)")
    lines.append("  [r4-0xC] = 0x40004400 (USART2_STS — status register)")
    lines.append("  [r4-0x8] = 0x40004404 (USART2_DATA — data register)")
    lines.append("")
    lines.append("-" * 90)
    lines.append("")

    annotations = {
        # Prologue
        0x080277B4: "--- ENTRY: save r4, lr ---",
        0x080277B6: "allocate 8 bytes on stack",
        0x080277B8: "r4 = 0x4000440C (USART2_CTRL1)",
        0x080277BC: "",

        # RX enable check
        0x080277C0: "--- CHECK 1: Is RDBFIEN (RX interrupt) enabled? ---",
        0x080277C2: "shift bit 5 to sign bit (31-5=26=0x1A)",
        0x080277C4: "if RDBFIEN set (bit 5 of CTRL1):",
        0x080277C6: "  read USART2_STS (0x40004400)",
        0x080277CA: "  shift STS bit 5 to sign bit -> check RDBF",
        0x080277CE: "  if RDBF set -> goto RX_PATH (0x080277E4)",

        # TX enable check
        0x080277D0: "--- CHECK 2: Is TDBEIEN (TX interrupt) enabled? ---",
        0x080277D2: "shift bit 7 to sign bit (31-7=24=0x18)",
        0x080277D4: "if TDBEIEN set (bit 7 of CTRL1):",
        0x080277D6: "  read USART2_STS",
        0x080277DA: "  shift STS bit 7 to sign bit -> check TDBE",
        0x080277DE: "  if TDBE set -> goto TX_PATH (0x08027822)",

        # Exit (neither RX nor TX triggered)
        0x080277E0: "--- EXIT: no interrupt pending ---",
        0x080277E2: "return",

        # RX path
        0x080277E4: "--- RX_PATH: Receive one byte from FPGA ---",
        0x080277E8: "ip = 0x20004E10 (rx_index address)",
        0x080277EC: "r2 = USART2_DATA (read received byte)",
        0x080277F0: "r0 = *rx_index (current position in RX buffer)",
        0x080277F4: "r1 = 0x20004E11 (rx_buffer base)",
        0x080277F8: "",
        0x080277FC: "rx_buffer[rx_index] = received_byte",
        0x080277FE: "r2 = rx_buffer[0] (first byte, for sync check)",
        0x08027800: "r3 = rx_index + 1",
        0x08027802: "*rx_index = r3 (increment index)",
        0x08027806: "if rx_index was 0 -> first byte, check sync (0x08027850)",

        # RX byte 1 check (index was 0, now 1)
        0x08027808: "r3 = (rx_index+1) & 0xFF",
        0x0802780A: "if rx_index+1 == 2 -> we have 2 bytes, check header pair",
        0x0802780C: "if != 2 -> check for complete frames (0x0802785A)",

        # RX 2-byte header validation
        0x0802780E: "r1 = rx_buffer[1] (second received byte)",
        0x08027810: "--- SYNC CHECK: 2-byte header validation ---",
        0x08027812: "if byte[0] != 0x5A -> not data sync, try echo sync",
        0x08027814: "  byte[0]==0x5A: check byte[1]==0xA5 (data sync: 5A A5)",
        0x08027816: "  if byte[1] != 0xA5 -> RESET rx_index (bad sync)",

        0x08027818: "if byte[0] == 0xAA -> check echo sync (AA 55)",
        0x0802781A: "if byte[0] != 0xAA -> not recognized, go to TX check",
        0x0802781C: "  byte[0]==0xAA: check byte[1]==0x55",
        0x0802781E: "  if byte[1] != 0x55 -> RESET rx_index",
        0x08027820: "  AA 55 header valid -> continue receiving (goto TX check)",

        # TX path
        0x08027822: "--- TX_PATH: Send next byte to FPGA ---",
        0x08027826: "r0 = 0x2000000F (tx_index address)",
        0x0802782A: "r1 = *tx_index (current TX position)",
        0x0802782C: "r2 = tx_index + 1",
        0x0802782E: "*tx_index = tx_index + 1",
        0x08027830: "r0 = 0x20000005 (tx_buffer base)",
        0x08027834: "",
        0x08027838: "r3 = (tx_index+1) & 0xFF",
        0x0802783A: "r0 = tx_buffer[old_tx_index] (byte to send)",
        0x0802783C: "if tx_index+1 == 10 -> all 10 bytes sent",
        0x0802783E: "USART2_DATA = byte_to_send (write to 0x40004404)",
        0x08027842: "if not done -> return (more bytes to pump)",

        # TX complete
        0x08027844: "--- TX COMPLETE: All 10 bytes sent ---",
        0x08027846: "CTRL1 &= ~0x80 (clear TDBEIEN — disable TX interrupt)",
        0x0802784A: "write back to CTRL1",
        0x0802784C: "deallocate stack",
        0x0802784E: "return",

        # First byte sync check (rx_index was 0)
        0x08027850: "--- FIRST_BYTE: rx_index==0, check if valid start ---",
        0x08027852: "if byte == 0x5A -> valid data sync start, continue",
        0x08027854: "if byte == 0xAA -> valid echo sync start, continue",
        0x08027858: "else -> unknown byte, RESET rx_index to 0",

        # Frame length checks
        0x0802785A: "--- FRAME LENGTH CHECK ---",
        0x0802785C: "if byte[0]==0xAA AND rx_count==10 -> 10-byte echo frame",
        0x0802785E: "",
        0x08027860: "  goto ECHO_FRAME_COMPLETE (0x080278BC)",
        0x08027862: "if rx_count == 12 -> 12-byte data frame complete",
        0x08027864: "if rx_count != 12 -> keep receiving (goto TX check)",

        # 12-byte data frame complete
        0x08027866: "--- 12-BYTE DATA FRAME COMPLETE ---",
        0x0802786A: "",
        0x0802786E: "r0 = *tx_index (check if TX completed its 10-byte frame)",
        0x08027870: "r1 = 0",
        0x08027872: "if tx_index != 10 -> TX not done, discard this RX frame",
        0x08027874: "rx_index = 0 (reset for next frame)",
        0x08027878: "if tx_index != 10 -> goto TX check (frame mismatch)",

        # Check exchange lock
        0x0802787A: "--- CHECK EXCHANGE LOCK ---",
        0x0802787E: "",
        0x08027882: "r0 = *(0x200010F8 + 0xF3C) = exchange_lock_flag",
        0x08027886: "if exchange_lock_flag != 0 -> exchange already in progress",
        0x08027888: "  -> skip queue send, goto TX check",

        # Queue send
        0x0802788A: "--- SEND TO FREERTOS QUEUE ---",
        0x0802788E: "",
        0x08027892: "r1 = 0 (item to send = NULL / notification)",
        0x08027894: "r0 = *fpga_rx_queue_handle (0x20002D7C)",
        0x08027896: "higher_priority_woken = 0 (on stack)",
        0x08027898: "r1 = &higher_priority_woken",
        0x0802789A: "xQueueGenericSendFromISR(queue, &item, &woken, 0)",

        # Check if context switch needed
        0x0802789E: "r0 = higher_priority_woken",
        0x080278A0: "if woken == 0 -> no context switch needed",
        0x080278A2: "  goto TX check (continue ISR)",

        # Trigger PendSV for context switch
        0x080278A4: "--- TRIGGER CONTEXT SWITCH ---",
        0x080278A8: "r0 = 0xE000ED04 (ICSR - Interrupt Control State Register)",
        0x080278AC: "r1 = 0x10000000 (PENDSVSET bit)",
        0x080278B0: "ICSR = PENDSVSET (trigger PendSV exception)",
        0x080278B2: "DSB (data synchronization barrier)",
        0x080278B6: "ISB (instruction synchronization barrier)",
        0x080278BA: "goto TX check",

        # 10-byte echo frame validation
        0x080278BC: "--- 10-BYTE ECHO FRAME VALIDATION ---",
        0x080278BE: "r0 = rx_buffer[1]; check == 0x55",
        0x080278C0: "if byte[1] != 0x55 -> not valid echo, check 12-byte",
        0x080278C2: "--- ECHO BYTE MATCH ---",
        0x080278C6: "",
        0x080278CA: "r0 = tx_buffer[3] (TX echo byte — command param low)",
        0x080278CC: "r2 = rx_buffer[3] (RX echo byte — should match TX)",
        0x080278CE: "if rx_buffer[3] != tx_buffer[3] -> MISMATCH, discard",
        0x080278D0: "  goto TX check (frame rejected)",
        0x080278D4: "r0 = rx_buffer[7] (integrity marker)",
        0x080278D6: "if rx_buffer[7] != 0xAA -> integrity check FAILED",
        0x080278D8: "  goto TX check (frame rejected)",

        # Reset after failed sync
        0x080278DC: "--- RESET RX INDEX (bad sync or unrecognized) ---",
        0x080278DE: "rx_index = 0",
        0x080278E2: "goto TX check (re-enter main loop)",
    }

    for addr, mnemonic, op_str, raw_bytes in insns:
        if addr in annotations:
            ann = annotations[addr]
            if ann.startswith("---"):
                lines.append("")
                lines.append(f"  {ann}")
            lines.append(format_insn(addr, mnemonic, op_str, ann if not ann.startswith("---") else ""))
        else:
            lines.append(format_insn(addr, mnemonic, op_str))

    return lines


def annotate_shared_irq_handler(insns, data):
    """Annotate the shared/default IRQ handler at 0x080072F4."""
    lines = []
    lines.append("")
    lines.append("=" * 90)
    lines.append("SHARED IRQ HANDLER — mode_switch_reset_handler @ 0x080072F4")
    lines.append("=" * 90)
    lines.append("")
    lines.append("FUNCTION SIGNATURE: void mode_switch_reset_handler(void)")
    lines.append("CALLED BY: 60+ interrupt vectors point here (all 'unused' IRQs)")
    lines.append("           See vector table analysis below for complete list.")
    lines.append("")
    lines.append("PURPOSE: This is NOT a 'default' handler — it does real work!")
    lines.append("  1. Reads state variable at (global_base + 0xF68) = mode_state")
    lines.append("  2. Dispatches via TBB (table branch byte) on mode_state-2")
    lines.append("  3. Then: re-enables USART2, resumes FreeRTOS tasks, sets GPIO,")
    lines.append("     resets state variables to defaults")
    lines.append("")
    lines.append("REGISTER USAGE:")
    lines.append("  r4 = 0x200000F8 (global state base)")
    lines.append("")
    lines.append("TBB DISPATCH TABLE (mode_state values 2-9):")
    lines.append("  mode_state=2: Clear DAT_20002D50 (counter/timestamp)")
    lines.append("  mode_state=3: Fall through to default")
    lines.append("  mode_state=4: Fall through to default")
    lines.append("  mode_state=5: Clear 4 state vars at base+0xE12..0xE1C")
    lines.append("  mode_state=6: Same as mode_state=5")
    lines.append("  mode_state=7: Enable TMR11_CTL0 bit 1 and TMR11_CTL0+0x20 bit 1")
    lines.append("                (0x40005C40 = TMR11, enables timer)")
    lines.append("                Then check if mode_state==1 -> early return")
    lines.append("  mode_state=8: Fall through to default")
    lines.append("  mode_state=9: Clear byte at base+0x355")
    lines.append("")
    lines.append("COMMON TAIL (after TBB dispatch):")
    lines.append("  1. Set mode_state = 1 (mark as 'active')")
    lines.append("  2. USART2_CTRL1 |= 0x2000 -> re-enable USART2 (UEN bit)")
    lines.append("  3. vTaskResume(task_handle_1)  -- at 0x20002DA0")
    lines.append("  4. vTaskResume(task_handle_2)  -- at 0x20002DA4")
    lines.append("  5. GPIOB_BOP = 0x800 -> set PB11 high (FPGA signal?)")
    lines.append("  6. Store 0x7FC00000 to base+0xF48, 0xF4C, 0xF50 (NaN float values)")
    lines.append("  7. Reset multiple state bytes to default values")
    lines.append("  8. Tail-call to function at 0x0800B908 (mode init dispatcher)")
    lines.append("")
    lines.append("-" * 90)

    annotations = {
        0x080072F4: "--- ENTRY ---",
        0x080072F6: "r4 = 0x200000F8 (global state base)",
        0x080072FA: "",
        0x080072FE: "r0 = *(r4 + 0xF68) = mode_state",
        0x08007302: "r1 = mode_state - 2",
        0x08007304: "if r1 > 7 -> skip TBB dispatch",
        0x08007306: "",
        0x08007308: "TBB dispatch on (mode_state - 2)",
        0x08007314: "--- case 5,6: Clear state vars ---",
        0x08007316: "*(r4+0xE1C) = 0 (halfword)",
        0x0800731A: "*(r4+0xE12) = 0 (word)",
        0x0800731E: "*(r4+0xE16) = 0 (word)",
        0x08007322: "*(r4+0xE1A) = 0 (byte)",
        0x08007326: "goto COMMON_TAIL",
        0x08007328: "--- case 2: Clear counter ---",
        0x0800732C: "",
        0x08007330: "",
        0x08007332: "DAT_20002D50 = 0",
        0x08007334: "goto COMMON_TAIL",
        0x08007336: "--- case 7: Enable TMR11 ---",
        0x0800733A: "r0 = 0x40005C40 (TMR11_CTL0)",
        0x0800733E: "r1 = TMR11_CTL0",
        0x08007340: "r1 |= 2 (enable bit)",
        0x08007344: "TMR11_CTL0 = r1",
        0x08007346: "r1 = TMR11_CTL0+0x20 (0x40005C60)",
        0x08007348: "r1 |= 2",
        0x0800734C: "store back",
        0x0800734E: "r0 = mode_state",
        0x08007352: "if mode_state == 1 -> return early (already active)",
        0x08007354: "",
        0x08007356: "return (pop {r4, pc})",
        0x08007358: "goto COMMON_TAIL (for other cases)",
        0x0800735A: "--- case 9: Clear flag ---",
        0x0800735C: "*(r4+0x355) = 0",
        0x08007360: "--- COMMON_TAIL ---",
        0x08007362: "mode_state = 1 (mark active)",
        0x08007366: "r0 = 0x4000440C (USART2_CTRL1)",
        0x0800736A: "",
        0x0800736E: "r1 = USART2_CTRL1",
        0x08007370: "r1 |= 0x2000 (set UEN — enable USART2)",
        0x08007374: "USART2_CTRL1 = r1",
        0x08007376: "r0 = &task_handle_1 (0x20002DA0)",
        0x0800737A: "",
        0x0800737E: "r0 = task_handle_1",
        0x08007380: "vTaskResume(task_handle_1)",
        0x08007384: "r0 = &task_handle_2 (0x20002DA4)",
        0x08007388: "",
        0x0800738C: "r0 = task_handle_2",
        0x0800738E: "vTaskResume(task_handle_2)",
        0x08007392: "r0 = 0x40011010 (GPIOB_BOP — bit set register)",
        0x08007396: "",
        0x0800739A: "r1 = 0x800 (bit 11 = PB11)",
        0x0800739E: "GPIOB_BOP = 0x800 -> PB11 HIGH",
        0x080073A0: "r0 = 0x7FC00000 (float NaN / quiet NaN)",
        0x080073A2: "",
        0x080073A6: "r1 = 0x0101",
        0x080073AA: "*(r4+0xF48) = NaN (measurement_ch1?)",
        0x080073AE: "*(r4+0xF4C) = NaN (measurement_ch2?)",
        0x080073B2: "*(r4+0xF50) = NaN (measurement_ch3?)",
        0x080073B6: "",
        0x080073B8: "*(r4+0xF35) = 0x0101 (halfword, flags)",
        0x080073BC: "r1 = 0xFF",
        0x080073BE: "*(r4+0xF5D) = 0",
        0x080073C2: "*(r4+0xF2F) = 0",
        0x080073C6: "*(r4+0xF38) = 0xFF",
        0x080073CA: "*(r4+0xF3C) = 0 (exchange_lock_flag)",
        0x080073CE: "*(r4+0xF2C) = 0xFF",
        0x080073D2: "*(r4+0xF69) = 0",
        0x080073D6: "*(r4+0xF6B) = 0",
        0x080073DA: "--- TAIL CALL ---",
    }

    for addr, mnemonic, op_str, raw_bytes in insns:
        ann = annotations.get(addr, "")
        if ann.startswith("---"):
            lines.append("")
            lines.append(f"  {ann}")
            lines.append(format_insn(addr, mnemonic, op_str))
        elif ann:
            lines.append(format_insn(addr, mnemonic, op_str, ann))
        else:
            lines.append(format_insn(addr, mnemonic, op_str))

    return lines


def annotate_irq38_vector(insns, data):
    """Annotate what the IRQ38 vector actually points to."""
    lines = []
    lines.append("")
    lines.append("=" * 90)
    lines.append("IRQ38 VECTOR TARGET — 0x0802E7B4 (exception 54 = USART2 global)")
    lines.append("=" * 90)
    lines.append("")
    lines.append("CRITICAL FINDING: The vector table entry for USART2 (exception 54)")
    lines.append("points to 0x0802E7B5 (Thumb), which is address 0x0802E7B4.")
    lines.append("")
    lines.append("This is the TAIL of function FUN_0802E71C — a FatFs f_read/f_write")
    lines.append("implementation. The code at 0x0802E7B4 is:")
    lines.append("  strb r0, [r4, #0x11]   ; store to file object")
    lines.append("  add  sp, #4")
    lines.append("  pop  {r4-r11, pc}      ; return")
    lines.append("")
    lines.append("This CANNOT be a valid ISR entry point. If entered as an interrupt,")
    lines.append("r4 would contain the interrupted code's register value (garbage),")
    lines.append("and the pop would corrupt the stack and crash.")
    lines.append("")
    lines.append("EXPLANATION: The USART2 global interrupt (IRQ 38) is NEVER ENABLED")
    lines.append("in the NVIC. The firmware does not call NVIC_EnableIRQ(38) anywhere.")
    lines.append("Instead, the USART2 handler at 0x080277B4 is called by a DIFFERENT")
    lines.append("mechanism — likely through the mode-switch interrupt infrastructure")
    lines.append("or a timer-driven polling loop.")
    lines.append("")
    lines.append("Evidence:")
    lines.append("  - No NVIC_ISER write enables bit 6 of ISER1 (IRQ 38)")
    lines.append("  - No BL instruction targets 0x080277B4")
    lines.append("  - No literal pool contains 0x080277B5 as a function pointer")
    lines.append("  - The vector entry is likely a linker artifact or vestigial")
    lines.append("")
    lines.append("The REAL interrupt path for USART2 data exchange:")
    lines.append("  1. USART2 TX/RX is driven by enabling CTRL1 bits (RDBFIEN, TDBEIEN)")
    lines.append("  2. The USART2 ISR at 0x080277B4 handles byte-by-byte TX/RX")
    lines.append("  3. It is likely registered via the AT32 HAL interrupt callback")
    lines.append("     mechanism or called from a parent interrupt handler")
    lines.append("  4. The mode-switch handler at 0x080072F4 manages USART2 UEN")
    lines.append("     (enable/disable) and task resume/suspend")
    lines.append("")
    lines.append("DISASSEMBLY of code at vector target:")
    lines.append("-" * 90)

    for addr, mnemonic, op_str, _ in insns:
        lines.append(format_insn(addr, mnemonic, op_str))

    return lines


def analyze_dvom_tx_setup(data, md):
    """Analyze the DVOM TX frame setup and USART2 init."""
    lines = []
    lines.append("")
    lines.append("=" * 90)
    lines.append("DVOM TX TASK — USART2 Frame Transmission Setup")
    lines.append("=" * 90)
    lines.append("")
    lines.append("The DVOM TX task (part of the large function at ~0x08026E00) sets up")
    lines.append("USART2 for command exchange with the FPGA. Key operations:")
    lines.append("")
    lines.append("INITIALIZATION SEQUENCE (at ~0x08026E7A-0x08026F50):")
    lines.append("  1. Configure GPIOC pins (via gpio_pin_config at 0x080302FC)")
    lines.append("  2. Set up TMR2 (0x40000414) for USART timing:")
    lines.append("     - TMR2 prescaler = 0x13 (19), auto-reload = computed")
    lines.append("     - TMR2 enabled with bit 0 in CTL0")
    lines.append("  3. Configure NVIC priority for the timer interrupt:")
    lines.append("     - AIRCR (0xE000ED0C) PRIGROUP = 3 (4 bits preempt, 0 sub)")
    lines.append("     - Priority byte written to NVIC_IPR")
    lines.append("  4. Enable timer interrupt in NVIC_ISER0 (0xE000E100)")
    lines.append("     - Writes 0x20000000 = bit 29 -> enables IRQ 29")
    lines.append("")
    lines.append("NOTE: IRQ 29 (exception 45) has vector 0x0802E71D — this is the")
    lines.append("TIMER interrupt that drives the USART2 exchange cycle, NOT the USART2")
    lines.append("global interrupt.")
    lines.append("")
    lines.append("FRAME TRANSMISSION (at ~0x08026F50-0x08027060):")
    lines.append("  The task reads mode_state (base+0xF64) and dispatches:")
    lines.append("    mode_state=1: ENABLE mode")
    lines.append("      - USART2_CTRL1 |= 0x2000 (enable USART2)")
    lines.append("      - vTaskResume(task_handle_1, task_handle_2)")
    lines.append("      - GPIOB_BOP = 0x800 (PB11 HIGH)")
    lines.append("      - Reset all state variables")
    lines.append("      - Then falls through to mode init dispatcher")
    lines.append("    mode_state=2: DISABLE mode")
    lines.append("      - USART2_CTRL1 &= ~0x2000 (disable USART2)")
    lines.append("      - vTaskSuspend(task_handle_1, task_handle_2)")
    lines.append("      - GPIOB_BOP = 0x800 (PB11 control)")
    lines.append("    mode_state=3: TX FRAME mode")
    lines.append("      - Builds 10-byte frame in tx_buffer")
    lines.append("      - Enables TDBEIEN to start TX pump via ISR")

    # Show the key code that enables TX interrupt
    lines.append("")
    lines.append("USART2 TX Enable (searching for TDBEIEN enable):")
    lines.append("  The TX interrupt (TDBEIEN, bit 7) is enabled by ORR #0x80 to CTRL1")
    lines.append("  This has NOT been found in the current analysis — the firmware may")
    lines.append("  use a HAL wrapper or the enable is in an unmapped gap.")
    lines.append("")
    lines.append("  What IS found:")
    lines.append("  - CTRL1 |= 0x2000 (UEN) at 0x08004DB6, 0x08007366, 0x08026F8E")
    lines.append("  - CTRL1 &= ~0x2000 (UEN clear) at 0x08006874, 0x0800741A, 0x0800752A, 0x0802700A")
    lines.append("  - CTRL1 &= ~0x80 (TDBEIEN clear) in the ISR at 0x08027846")
    lines.append("  - CTRL1 bit 5 (RDBFIEN) and bit 7 (TDBEIEN) checked in ISR at 0x080277C0-0x080277DE")
    lines.append("")
    lines.append("  The RDBFIEN and TDBEIEN enables are likely set in the USART init")
    lines.append("  code within the boot gap at 0x08002BE0, or in the 11KB FPGA task")
    lines.append("  at FUN_08036934.")

    return lines


def generate_protocol_summary():
    """Generate the complete protocol documentation."""
    lines = []
    lines.append("")
    lines.append("=" * 90)
    lines.append("COMPLETE USART2 PROTOCOL DOCUMENTATION")
    lines.append("=" * 90)
    lines.append("")
    lines.append("╔══════════════════════════════════════════════════════════════════════════╗")
    lines.append("║  MCU (AT32F403A)  ←——— USART2 9600 baud ———→  FPGA (Gowin GW1N-UV2)  ║")
    lines.append("║  PA2 (TX) ────────────────────────────────────→                        ║")
    lines.append("║  PA3 (RX) ←────────────────────────────────────                        ║")
    lines.append("╚══════════════════════════════════════════════════════════════════════════╝")
    lines.append("")
    lines.append("─── TX FRAME FORMAT (MCU → FPGA): 10 bytes ───")
    lines.append("")
    lines.append("  Buffer: 0x20000005 (10 bytes)")
    lines.append("  Index:  0x2000000F (byte, 0-9)")
    lines.append("")
    lines.append("  ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐")
    lines.append("  │ [0]  │ [1]  │ [2]  │ [3]  │ [4]  │ [5]  │ [6]  │ [7]  │ [8]  │ [9]  │")
    lines.append("  ├──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┤")
    lines.append("  │ HDR  │ HDR  │ CMD  │ ECHO │ DAT0 │ DAT1 │ DAT2 │ DAT3 │ DAT4 │ CSUM │")
    lines.append("  │      │      │ HIGH │ LOW  │      │      │      │      │      │      │")
    lines.append("  └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘")
    lines.append("")
    lines.append("  TX ISR pumps bytes [0]-[9] sequentially via TDBE interrupt.")
    lines.append("  After byte [9] is loaded into DATA register, TDBEIEN is cleared.")
    lines.append("  Byte [3] is the 'echo' byte — FPGA must return it in its response.")
    lines.append("  Byte [9] = checksum: (CMD_HIGH + (CMD_HIGH >> 8)) & 0xFF (per docs)")
    lines.append("")
    lines.append("─── RX FRAME FORMAT (FPGA → MCU): Variable (2, 10, or 12 bytes) ───")
    lines.append("")
    lines.append("  Buffer: 0x20004E11 (12 bytes max)")
    lines.append("  Index:  0x20004E10 (byte, 0-11)")
    lines.append("")
    lines.append("  The ISR uses a state machine based on rx_index and byte values.")
    lines.append("")
    lines.append("  STATE MACHINE:")
    lines.append("  ┌─────────────────────────────────────────────────────────┐")
    lines.append("  │ rx_index=0: FIRST BYTE                                 │")
    lines.append("  │   if byte == 0x5A → accept, continue to index 1        │")
    lines.append("  │   if byte == 0xAA → accept, continue to index 1        │")
    lines.append("  │   else → RESET (discard, stay at index 0)              │")
    lines.append("  ├─────────────────────────────────────────────────────────┤")
    lines.append("  │ rx_index=1 (2 bytes received): SYNC CHECK              │")
    lines.append("  │   [0x5A, 0xA5] → DATA sync accepted, continue         │")
    lines.append("  │   [0x5A, ???]  → RESET                                 │")
    lines.append("  │   [0xAA, 0x55] → ECHO sync accepted, continue         │")
    lines.append("  │   [0xAA, ???]  → RESET                                 │")
    lines.append("  ├─────────────────────────────────────────────────────────┤")
    lines.append("  │ rx_index=9 (10 bytes): ECHO FRAME CHECK                │")
    lines.append("  │   if byte[0]==0xAA → check 10-byte echo frame:         │")
    lines.append("  │     if byte[1]==0x55                                    │")
    lines.append("  │       AND byte[3]==tx_buffer[3] (echo match)            │")
    lines.append("  │       AND byte[7]==0xAA (integrity marker)              │")
    lines.append("  │     → FRAME ACCEPTED (but no queue send yet)            │")
    lines.append("  │     else → continue receiving to 12 bytes               │")
    lines.append("  ├─────────────────────────────────────────────────────────┤")
    lines.append("  │ rx_index=11 (12 bytes): DATA FRAME COMPLETE             │")
    lines.append("  │   Checks:                                               │")
    lines.append("  │     1. tx_index must be 10 (TX frame fully sent)        │")
    lines.append("  │     2. exchange_lock (base+0xF3C) must be 0             │")
    lines.append("  │   If valid:                                             │")
    lines.append("  │     → xQueueGenericSendFromISR(fpga_rx_queue)           │")
    lines.append("  │     → if higher_priority_woken: trigger PendSV          │")
    lines.append("  │   Reset rx_index = 0                                    │")
    lines.append("  └─────────────────────────────────────────────────────────┘")
    lines.append("")
    lines.append("  RX FRAME TYPE 1 — Data Sync (header only, 2 bytes):")
    lines.append("  ┌──────┬──────┐")
    lines.append("  │ 0x5A │ 0xA5 │")
    lines.append("  └──────┴──────┘")
    lines.append("  FPGA acknowledgment. Continues receiving to 12 bytes total.")
    lines.append("")
    lines.append("  RX FRAME TYPE 2 — Echo/Status Response (10 bytes):")
    lines.append("  ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐")
    lines.append("  │ 0xAA │ 0x55 │ STAT │ ECHO │ DAT0 │ DAT1 │ DAT2 │ 0xAA │ DAT3 │ DAT4 │")
    lines.append("  └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘")
    lines.append("    [3] = ECHO: Must match tx_buffer[3] or frame is discarded")
    lines.append("    [7] = 0xAA: Integrity marker, must be 0xAA or frame is discarded")
    lines.append("")
    lines.append("  RX FRAME TYPE 3 — Acquisition Complete (12 bytes):")
    lines.append("  ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐")
    lines.append("  │ 0x5A │ 0xA5 │  D0  │  D1  │  D2  │  D3  │  D4  │  D5  │  D6  │  D7  │  D8  │  D9  │")
    lines.append("  └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘")
    lines.append("    Triggers queue send + PendSV for context switch to fpga_task")
    lines.append("")
    lines.append("─── EXCHANGE HANDSHAKE ───")
    lines.append("")
    lines.append("  The full command/response exchange:")
    lines.append("")
    lines.append("  MCU                              FPGA")
    lines.append("   │                                │")
    lines.append("   │  TX: [10 bytes] ──────────────→│  MCU sends command frame")
    lines.append("   │  (TDBEIEN pumps bytes 0-9)     │")
    lines.append("   │  (TDBEIEN cleared after [9])   │")
    lines.append("   │                                │")
    lines.append("   │←── RX: [5A A5 ...] (12 bytes)  │  FPGA sends 12-byte data response")
    lines.append("   │  OR                            │")
    lines.append("   │←── RX: [AA 55 ...] (10 bytes)  │  FPGA sends 10-byte echo response")
    lines.append("   │                                │")
    lines.append("   │  Validation:                   │")
    lines.append("   │  - Echo match (byte[3])        │")
    lines.append("   │  - Integrity marker (byte[7])  │")
    lines.append("   │  - TX complete check           │")
    lines.append("   │  - Exchange lock check         │")
    lines.append("   │                                │")
    lines.append("   │  If valid 12-byte frame:       │")
    lines.append("   │  → xQueueSendFromISR           │")
    lines.append("   │  → PendSV (context switch)     │")
    lines.append("   │  → fpga_task wakes up          │")
    lines.append("")
    lines.append("─── OBSERVED HEARTBEAT ───")
    lines.append("")
    lines.append("  Captured on real hardware (9600 baud, PA3):")
    lines.append("  5A A5 E4 2E 63 25 07 00 00 00 00 Fx")
    lines.append("")
    lines.append("  This is a 12-byte data frame (5A A5 header).")
    lines.append("  Bytes 2-10 are constant (sensor data or calibration).")
    lines.append("  Byte 11 cycles F0→F1→F2→F3 (rolling counter).")
    lines.append("  The FPGA sends this continuously even without MCU commands.")
    lines.append("")
    lines.append("─── INTERRUPT ARCHITECTURE ───")
    lines.append("")
    lines.append("  The USART2 interrupt system uses an INDIRECT mechanism:")
    lines.append("")
    lines.append("  1. A TIMER interrupt (IRQ 29, exception 45, vector 0x0802E71D)")
    lines.append("     is enabled via NVIC_ISER0 bit 29")
    lines.append("  2. This timer fires periodically and triggers the USART2 exchange")
    lines.append("  3. The USART2 ISR at 0x080277B4 handles the actual byte transfer")
    lines.append("     via RDBFIEN/TDBEIEN interrupts in CTRL1")
    lines.append("  4. The USART2 global interrupt (IRQ 38) is NOT enabled in NVIC")
    lines.append("  5. The mode-switch handler at 0x080072F4 manages USART2 UEN")
    lines.append("")
    lines.append("  Key distinction: USART2 interrupt FLAGS (RDBFIEN, TDBEIEN) are")
    lines.append("  enabled in the USART2_CTRL1 register, but the NVIC-level USART2")
    lines.append("  interrupt is not enabled. This means the USART2 status bits are")
    lines.append("  polled by another interrupt (likely the timer at IRQ 29) which")
    lines.append("  then calls 0x080277B4 as a subroutine.")
    lines.append("")
    lines.append("  Alternatively, the AT32 HAL may route the USART2 interrupt")
    lines.append("  through a different mechanism (DMA completion, timer capture, etc.)")

    return lines


def analyze_vector_table(data):
    """Analyze the complete interrupt vector table."""
    lines = []
    lines.append("")
    lines.append("=" * 90)
    lines.append("INTERRUPT VECTOR TABLE ANALYSIS")
    lines.append("=" * 90)
    lines.append("")

    # AT32F403A IRQ names (approximate, some differ from STM32F103)
    irq_names = {
        0: "WWDT", 1: "PVM", 2: "TAMPER", 3: "ERTC", 4: "FLASH",
        5: "CRM", 6: "EXINT0", 7: "EXINT1", 8: "EXINT2", 9: "EXINT3",
        10: "EXINT4", 11: "DMA1_CH1", 12: "DMA1_CH2", 13: "DMA1_CH3",
        14: "DMA1_CH4", 15: "DMA1_CH5", 16: "DMA1_CH6", 17: "DMA1_CH7",
        18: "ADC1_2", 19: "CAN1_TX", 20: "CAN1_RX0", 21: "CAN1_RX1",
        22: "CAN1_SCE", 23: "EXINT9_5", 24: "TMR1_BRK_TMR9",
        25: "TMR1_OVF_TMR10", 26: "TMR1_TRG_HALL_TMR11", 27: "TMR1_CH",
        28: "TMR2", 29: "TMR3", 30: "TMR4", 31: "I2C1_EVT",
        32: "I2C1_ERR", 33: "I2C2_EVT", 34: "I2C2_ERR",
        35: "SPI1", 36: "SPI2", 37: "USART1", 38: "USART2", 39: "USART3",
        40: "EXINT15_10", 41: "ERTC_ALARM", 42: "USB_WKUP", 43: "TMR8_BRK_TMR12",
        44: "TMR8_OVF_TMR13", 45: "TMR8_TRG_HALL_TMR14", 46: "TMR8_CH",
        47: "SDIO1", 48: "TMR5", 49: "SPI3", 50: "USART4", 51: "USART5",
        52: "TMR6", 53: "TMR7", 54: "DMA2_CH1", 55: "DMA2_CH2",
        56: "DMA2_CH3", 57: "DMA2_CH4_5",
    }

    lines.append("System exceptions:")
    lines.append(f"  [0]  Initial SP:     0x{read32(data, BASE):08X}")
    for i in range(1, 16):
        names = {1: "Reset", 2: "NMI", 3: "HardFault", 4: "MemManage",
                 5: "BusFault", 6: "UsageFault", 11: "SVCall", 12: "DebugMon",
                 14: "PendSV", 15: "SysTick"}
        name = names.get(i, f"Reserved_{i}")
        vec = read32(data, BASE + i*4)
        if vec:
            lines.append(f"  [{i:2d}] {name:15s} 0x{vec:08X}")
        else:
            lines.append(f"  [{i:2d}] {name:15s} (not set)")

    lines.append("")
    lines.append("IRQ handlers (non-default only):")
    lines.append("  Default handler: 0x08007345 (mode_switch_reset_handler at 0x080072F4)")
    lines.append("")

    custom_count = 0
    default_count = 0
    for irq in range(58):
        exc = irq + 16
        vec = read32(data, BASE + exc*4)
        name = irq_names.get(irq, f"IRQ_{irq}")
        if vec == 0x08007345:
            default_count += 1
        else:
            custom_count += 1
            lines.append(f"  IRQ {irq:2d} ({name:25s}) exc {exc:2d}: 0x{vec:08X}")

    lines.append("")
    lines.append(f"  Custom handlers: {custom_count}")
    lines.append(f"  Default handlers: {default_count}")
    lines.append("")
    lines.append("NON-DEFAULT HANDLERS DETAIL:")
    lines.append("  IRQ  9 (EXINT3)                    -> 0x08009C11  [EXTI3: continuity buzzer, V1.2.0 addition]")
    lines.append("  IRQ 12 (DMA1_CH2)                  -> 0x08009671  [DMA complete for LCD framebuffer]")
    lines.append("  IRQ 20 (CAN1_RX0)                  -> 0x0802E8E5  [Filesystem/FatFs related]")
    lines.append("  IRQ 29 (TMR3)                      -> 0x0802E71D  [Timer for USART2 exchange cycle]")
    lines.append("  IRQ 38 (USART2)                    -> 0x0802E7B5  [VESTIGIAL — points to FatFs tail]")
    lines.append("  IRQ 43 (TMR8_BRK_TMR12)            -> 0x0802E78D  [Filesystem/FatFs related]")

    return lines


def analyze_related_handlers(data, md):
    """Analyze other interrupt handlers that interact with USART/SPI."""
    lines = []
    lines.append("")
    lines.append("=" * 90)
    lines.append("RELATED INTERRUPT HANDLERS")
    lines.append("=" * 90)

    # IRQ 29 (TMR3) handler at 0x0802E71C
    lines.append("")
    lines.append("--- IRQ 29 (TMR3) Handler at 0x0802E71C ---")
    lines.append("Vector: 0x0802E71D (exception 45)")
    lines.append("This timer drives the USART2 exchange cycle.")
    lines.append("")
    insns = disasm_range(md, data, 0x0802E71C, 0x0802E7BC)
    for addr, mnemonic, op_str, _ in insns:
        lines.append(format_insn(addr, mnemonic, op_str))

    lines.append("")
    lines.append("NOTE: The code at 0x0802E71C is actually inside/after the FatFs")
    lines.append("function region. This further confirms these vector table entries")
    lines.append("may be artifacts of the linker placing FatFs code at these addresses")
    lines.append("rather than actual ISR implementations.")

    # IRQ 9 (EXINT3) handler at 0x08009C10
    lines.append("")
    lines.append("--- IRQ 9 (EXINT3) Handler at 0x08009C10 ---")
    lines.append("Vector: 0x08009C11 (exception 25)")
    lines.append("Added in V1.2.0 — handles continuity buzzer detection via EXTI3.")
    lines.append("")
    insns = disasm_range(md, data, 0x08009C10, 0x08009C78)
    for addr, mnemonic, op_str, _ in insns:
        ann = ""
        if addr == 0x08009C10:
            ann = "r0=param_1 (EXTI pending bits), r1=param_2"
        elif "0x2d84" in op_str:
            ann = "queue handle for EXTI notifications"
        elif "0x803b3a8" in op_str:
            ann = "xQueueReceiveFromISR or xTaskNotifyFromISR"
        elif "0x20001080" in op_str or "1080" in op_str:
            ann = "buzzer state flag"
        elif "0x1070" in op_str or "1070" in op_str:
            ann = "function pointer (buzzer callback)"
        elif "0x1078" in op_str:
            ann = "timestamp/counter base"
        elif "0x107c" in op_str:
            ann = "lookup table pointer"
        elif "0x80012bc" in op_str:
            ann = "audio/buzzer output function"
        lines.append(format_insn(addr, mnemonic, op_str, ann))

    # IRQ 12 (DMA1_CH2) handler at 0x08009670
    lines.append("")
    lines.append("--- IRQ 12 (DMA1_CH2) Handler at 0x08009670 ---")
    lines.append("Vector: 0x08009671 (exception 28)")
    lines.append("DMA completion for LCD framebuffer transfer (RAM → EXMC).")
    lines.append("")
    insns = disasm_range(md, data, 0x08009670, 0x0800967C)
    for addr, mnemonic, op_str, _ in insns:
        lines.append(format_insn(addr, mnemonic, op_str))
    lines.append("  (Indirect jump through function pointer — HAL DMA callback)")

    # Function at 0x0802771C — EXTI/watchdog handler near USART2 ISR
    lines.append("")
    lines.append("--- Watchdog/EXTI Handler at 0x0802771C ---")
    lines.append("Located immediately before USART2 ISR (0x080277B4).")
    lines.append("Checks TMR2 status (0x40000410), sends to queue, triggers PendSV.")
    lines.append("")
    insns = disasm_range(md, data, 0x0802771C, 0x0802778A)
    annotations_771c = {
        0x0802771C: "push {r4, lr}",
        0x08027720: "r4 = 0x40000410 (TMR2_STS)",
        0x08027728: "r0 = TMR2_STS",
        0x0802772A: "check bit 0 (overflow flag)",
        0x0802772C: "if no overflow -> return",
        0x0802772E: "r0 = global_base+0x2C = some flag",
        0x0802773A: "if flag set -> clear TMR2 and return",
        0x0802773C: "TMR2_STS = ~2 (clear specific flag)",
        0x08027746: "item = 0",
        0x0802774A: "queue = *(0x20002D78)",
        0x08027752: "item = 3",
        0x08027760: "xQueueReceiveFromISR(queue, &item, &woken)",
        0x08027764: "TMR2_STS = ~2",
        0x0802776A: "if woken == 0 -> return",
        0x08027770: "ICSR = PENDSVSET (trigger PendSV)",
    }
    for addr, mnemonic, op_str, _ in insns:
        ann = annotations_771c.get(addr, "")
        lines.append(format_insn(addr, mnemonic, op_str, ann))

    # Watchdog kick at 0x0802778C
    lines.append("")
    lines.append("--- Watchdog Kick Function at 0x0802778C ---")
    lines.append("Checks TMR6 status and reloads IWDG (watchdog).")
    lines.append("")
    insns = disasm_range(md, data, 0x0802778C, 0x080277B4)
    wdg_ann = {
        0x0802778C: "r0 = 0x40001810 (TMR6_STS?)",
        0x08027794: "r1 = TMR6_STS",
        0x08027796: "check bit 0",
        0x08027798: "if not set -> return",
        0x0802779C: "r1 = 0x40003000 (IWDG_KR - watchdog key register)",
        0x080277A4: "r2 = 0xAAAA (IWDG reload key)",
        0x080277A8: "IWDG_KR = 0xAAAA (kick watchdog)",
        0x080277AA: "TMR6_STS = ~2 (clear flag)",
        0x080277B0: "return",
    }
    for addr, mnemonic, op_str, _ in insns:
        ann = wdg_ann.get(addr, "")
        lines.append(format_insn(addr, mnemonic, op_str, ann))

    return lines


def main():
    print(f"Loading firmware from: {FIRMWARE_PATH}")
    data = load_firmware()
    print(f"Firmware size: {len(data)} bytes ({len(data)/1024:.1f} KB)")

    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True

    output = []

    # Header
    output.append("FNIRSI 2C53T V1.2.0 — USART2 Protocol Decompilation")
    output.append("=" * 90)
    output.append(f"Firmware: APP_2C53T_V1.2.0_251015.bin ({len(data)} bytes)")
    output.append(f"Base: 0x{BASE:08X}")
    output.append(f"MCU: AT32F403A (ARM Cortex-M4F, Thumb-2)")
    output.append(f"USART2: 0x40004400 (PA2 TX, PA3 RX, 9600 baud)")
    output.append("")
    output.append("Generated by: scripts/decompile_usart_irq.py")
    output.append("")

    # 1. USART2 ISR
    print("Disassembling USART2 ISR (0x080277B4-0x080278E4)...")
    usart_insns = disasm_range(md, data, 0x080277B4, 0x080278E4)
    output.extend(annotate_usart2_handler(usart_insns, data))

    # 2. Shared IRQ handler
    print("Disassembling shared IRQ handler (0x080072F4-0x080073E2)...")
    shared_insns = disasm_range(md, data, 0x080072F4, 0x080073E2)
    output.extend(annotate_shared_irq_handler(shared_insns, data))

    # 3. IRQ38 vector target analysis
    print("Analyzing IRQ38 vector target (0x0802E7B0-0x0802E7BC)...")
    irq38_insns = disasm_range(md, data, 0x0802E7B0, 0x0802E7BC)
    output.extend(annotate_irq38_vector(irq38_insns, data))

    # 4. Vector table analysis
    print("Analyzing interrupt vector table...")
    output.extend(analyze_vector_table(data))

    # 5. DVOM TX setup analysis
    print("Analyzing DVOM TX task setup...")
    output.extend(analyze_dvom_tx_setup(data, md))

    # 6. Related handlers
    print("Disassembling related interrupt handlers...")
    output.extend(analyze_related_handlers(data, md))

    # 7. Protocol summary
    print("Generating protocol documentation...")
    output.extend(generate_protocol_summary())

    # 8. Pseudocode
    output.append("")
    output.append("=" * 90)
    output.append("ANNOTATED PSEUDOCODE — usart2_irq_handler @ 0x080277B4")
    output.append("=" * 90)
    output.append("""
// USART2 Interrupt Handler — called when USART2 has data to send or receive
// This handles the MCU ↔ FPGA command channel (9600 baud on PA2/PA3)
//
// REGISTERS:
//   USART2_STS   = *(volatile uint32_t*)0x40004400  // Status register
//   USART2_DATA  = *(volatile uint32_t*)0x40004404  // Data register (read=RX, write=TX)
//   USART2_CTRL1 = *(volatile uint32_t*)0x4000440C  // Control register 1

void usart2_irq_handler(void)  // @ 0x080277B4
{
    volatile uint32_t *CTRL1 = (volatile uint32_t *)0x4000440C;
    volatile uint32_t *STS   = (volatile uint32_t *)0x40004400;
    volatile uint32_t *DATA  = (volatile uint32_t *)0x40004404;

    // ─── CHECK RX: Is receive interrupt enabled AND data available? ───
    if ((*CTRL1 & (1<<5)) && (*STS & (1<<5)))   // RDBFIEN && RDBF
    {
        uint8_t byte = (uint8_t)*DATA;           // Read received byte (clears RDBF)
        uint8_t idx = rx_index;                   // 0x20004E10
        rx_buffer[idx] = byte;                    // 0x20004E11 + idx
        rx_index = idx + 1;

        uint8_t first_byte = rx_buffer[0];
        uint8_t count = idx + 1;

        // ─── FIRST BYTE: Validate sync header start ───
        if (idx == 0) {
            if (first_byte != 0x5A && first_byte != 0xAA) {
                rx_index = 0;  // Unknown start byte → reset
            }
            goto check_tx;
        }

        // ─── SECOND BYTE: Validate complete 2-byte sync header ───
        if (count == 2) {
            uint8_t second_byte = rx_buffer[1];
            if (first_byte == 0x5A) {
                if (second_byte != 0xA5) {
                    rx_index = 0;  // Bad data sync → reset
                }
                // [5A A5] = valid data sync, continue receiving
            }
            else if (first_byte == 0xAA) {
                if (second_byte != 0x55) {
                    rx_index = 0;  // Bad echo sync → reset
                }
                // [AA 55] = valid echo sync, continue receiving
            }
            else {
                goto check_tx;  // Should not reach here
            }
            goto check_tx;
        }

        // ─── 10-BYTE ECHO FRAME: Validate AA 55 response ───
        if (first_byte == 0xAA && count == 10)
        {
            if (rx_buffer[1] != 0x55) goto check_12byte;

            // Echo match: rx_buffer[3] must equal tx_buffer[3]
            if (rx_buffer[3] != tx_buffer[3]) {
                goto check_tx;  // Echo mismatch → discard silently
            }
            // Integrity marker: rx_buffer[7] must be 0xAA
            if (rx_buffer[7] != 0xAA) {
                goto check_tx;  // Integrity check failed → discard
            }
            // 10-byte echo frame is VALID but we don't queue-send for echo frames
            // (echo frames just confirm the command was received correctly)
            goto check_tx;
        }

    check_12byte:
        // ─── 12-BYTE DATA FRAME: Full acquisition response ───
        if (count == 12)
        {
            rx_index = 0;  // Reset for next frame

            // TX must have completed its 10-byte frame
            if (tx_index != 10) goto check_tx;

            // Exchange lock must not be held
            if (exchange_lock_flag != 0) goto check_tx;  // *(base+0xF3C)

            // ─── SEND TO FREERTOS QUEUE ───
            BaseType_t higher_priority_woken = 0;
            QueueHandle_t queue = *(QueueHandle_t*)0x20002D7C;
            xQueueGenericSendFromISR(queue, NULL, &higher_priority_woken, 0);

            if (higher_priority_woken) {
                // Trigger PendSV for immediate context switch to fpga_task
                *(volatile uint32_t*)0xE000ED04 = 0x10000000;  // ICSR.PENDSVSET
                __DSB();
                __ISB();
            }
            goto check_tx;
        }

        // Received byte count is 3-9 or 11: keep accumulating
        if (count != 2 && count != 10 && count != 12) {
            // For unrecognized states, just continue
            goto check_tx;
        }

        // Fall through: reset if sync was bad
        rx_index = 0;
    }

check_tx:
    // ─── CHECK TX: Is transmit interrupt enabled AND buffer empty? ───
    if ((*CTRL1 & (1<<7)) && (*STS & (1<<7)))   // TDBEIEN && TDBE
    {
        uint8_t idx = tx_index;                   // 0x2000000F
        tx_index = idx + 1;

        *DATA = tx_buffer[idx];                   // 0x20000005 + idx → USART2_DATA

        if ((idx + 1) == 10) {
            // All 10 bytes sent — disable TX interrupt
            *CTRL1 &= ~(1<<7);                   // Clear TDBEIEN
        }
    }
}
""")

    # Pseudocode for shared handler
    output.append("")
    output.append("=" * 90)
    output.append("ANNOTATED PSEUDOCODE — mode_switch_reset_handler @ 0x080072F4")
    output.append("=" * 90)
    output.append("""
// Shared IRQ handler — installed as the default vector for 60+ interrupts
// Called when ANY unassigned interrupt fires (likely only fires on mode switch)
//
// This handler re-enables USART2, resumes FreeRTOS tasks, and resets
// state variables. It's the "mode switch reset" — when switching between
// oscilloscope, multimeter, and signal generator modes.

void mode_switch_reset_handler(void)  // @ 0x080072F4
{
    uint8_t *base = (uint8_t *)0x200000F8;    // Global state base
    uint8_t mode_state = base[0xF68];          // Current mode state

    // ─── DISPATCH on mode_state ───
    switch (mode_state - 2) {
    case 0:  // mode_state == 2
        *(uint16_t*)0x20002D50 = 0;            // Clear counter/timestamp
        break;

    case 3:  // mode_state == 5
    case 4:  // mode_state == 6
        // Clear acquisition state variables
        *(uint16_t*)(base + 0xE1C) = 0;
        *(uint32_t*)(base + 0xE12) = 0;
        *(uint32_t*)(base + 0xE16) = 0;
        *(uint8_t*)(base + 0xE1A) = 0;
        break;

    case 5:  // mode_state == 7
        // Enable TMR11 (0x40005C40)
        *(volatile uint32_t*)0x40005C40 |= 2;
        *(volatile uint32_t*)0x40005C60 |= 2;
        if (mode_state == 1) return;           // Already active, early exit
        break;

    case 7:  // mode_state == 9
        base[0x355] = 0;                       // Clear some flag
        break;

    case 1:  // mode_state == 3
    case 2:  // mode_state == 4
    case 6:  // mode_state == 8
        if (mode_state == 1) return;           // Early exit check
        break;

    default:
        if (mode_state == 1) return;           // Already active
        break;
    }

    // ─── COMMON TAIL: Reset everything for new mode ───
    base[0xF68] = 1;                           // Mark as active

    // Re-enable USART2
    *(volatile uint32_t*)0x4000440C |= 0x2000; // USART2_CTRL1 |= UEN

    // Resume FreeRTOS tasks
    vTaskResume(*(TaskHandle_t*)0x20002DA0);    // fpga_task or similar
    vTaskResume(*(TaskHandle_t*)0x20002DA4);    // display_task or similar

    // Set PB11 high (FPGA control signal)
    *(volatile uint32_t*)0x40011010 = 0x800;   // GPIOB_BOP = PB11

    // Reset measurement/state variables
    *(float*)(base + 0xF48) = NaN;             // 0x7FC00000
    *(float*)(base + 0xF4C) = NaN;
    *(float*)(base + 0xF50) = NaN;
    *(uint16_t*)(base + 0xF35) = 0x0101;       // Flags
    base[0xF5D] = 0;
    base[0xF2F] = 0;
    base[0xF38] = 0xFF;
    *(uint16_t*)(base + 0xF3C) = 0;            // exchange_lock = 0
    *(uint16_t*)(base + 0xF2C) = 0xFF;
    *(uint16_t*)(base + 0xF69) = 0;
    base[0xF6B] = 0;

    // Tail-call to mode init dispatcher (sets up commands for new mode)
    mode_init_dispatcher();                     // @ 0x0800B908
}
""")

    # Write output
    output_text = "\n".join(output)
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w") as f:
        f.write(output_text)

    print(f"\nOutput written to: {OUTPUT_PATH}")
    print(f"Total lines: {len(output)}")
    print(f"Total size: {len(output_text)} bytes")
    print("\nKey findings summary:")
    print("  1. USART2 ISR at 0x080277B4 handles TX (10-byte pump) and RX (frame parsing)")
    print("  2. Vector table exception 54 (USART2) points to FatFs tail — NOT a real ISR")
    print("  3. USART2 NVIC interrupt is NOT enabled; handler called via timer/polling")
    print("  4. Shared handler at 0x080072F4 re-enables USART2 on mode switch")
    print("  5. RX frames: 5A A5 (12-byte data) or AA 55 (10-byte echo)")
    print("  6. Echo validation: byte[3] match + byte[7]==0xAA integrity marker")
    print("  7. On valid 12-byte frame: xQueueSendFromISR + PendSV context switch")


if __name__ == "__main__":
    main()
