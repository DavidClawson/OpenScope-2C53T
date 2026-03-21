# FreeRTOS Task Architecture (Complete)

## Critical Discovery: Flash Base Address Offset

The firmware binary is loaded at **0x08007000**, not 0x08000000 as initially assumed.

- **Ghidra addresses** = file offset + 0x08000000 (what we've been using)
- **Real runtime addresses** = file offset + 0x08007000 = Ghidra address + 0x7000
- Confirmed by finding "IDLE" string at real address 0x080BBC32

This means our Ghidra project has a 0x7000 offset from real addresses. PC-relative branches work correctly in Ghidra, but absolute addresses (MOVW/MOVT) reflect real flash addresses.

## All 6 User Tasks

Found via xTaskCreate calls in the initialization function:

| # | Task Name | Entry Point (real) | Stack | Priority | Queue/Semaphore | Description |
|---|---|---|---|---|---|---|
| 1 | **display** | 0x0803DA50 | 1536 B | 1 (lowest) | Queue: 20 items × 1 byte | UI renderer. Receives display commands, dispatches via function pointer table |
| 2 | **key** | 0x08040008 | 512 B | 4 (highest) | Queue: 15 items × 1 byte + Binary semaphore | Button/input handler. Debounce, auto-repeat |
| 3 | **osc** | 0x0804009C | 1024 B | 2 | Binary semaphore | Oscilloscope acquisition. Takes semaphore, calls processing function |
| 4 | **fpga** | 0x0803E454 | 512 B | 3 | Queue: 15 items × 1 byte | FPGA data readout. ADC calibration/scaling with FPU. Accesses SPI/FSMC |
| 5 | **dvom_TX** | 0x0803E3F4 | 256 B | 2 | Queue: 10 items × 2 bytes | Digital voltmeter UART transmit. Sends to UART at 0x4000440C |
| 6 | **dvom_RX** | 0x0803DAC0 | 512 B | 3 | Binary semaphore (from UART RX ISR) | Digital voltmeter UART receive + measurement calculations |

### Internal FreeRTOS Tasks

| Name | Entry Point | Stack | Priority |
|---|---|---|---|
| IDLE | 0x0803BECD | 512 B | 0 |
| Tmr Svc | 0x0803C79D | 512 B | 10 |

## Software Timers

| Name | Period | Auto-Reload | Likely Purpose |
|---|---|---|---|
| Timer1 | 10 ticks | Yes | Key debounce or display refresh (~100Hz) |
| Timer2 | 1000 ticks | Yes | 1-second periodic tasks (measurement update, auto-power-off countdown) |

## Task Interaction Pattern

```
Hardware Interrupts
    │
    ├── UART RX ISR ──gives──→ [dvom_RX semaphore] → dvom_RX task (processes measurement)
    │                                                      │
    │                                                      └──sends──→ [display queue] → display task
    │
    ├── DMA/FPGA ISR ──gives──→ [osc semaphore] → osc task (processes waveform)
    │                                                  │
    │                                                  └──sends──→ [display queue] → display task
    │
    ├── Key scan ISR ──gives──→ [key semaphore] → key task (debounce + dispatch)
    │                                                 │
    │                                                 ├──sends──→ [fpga queue] → fpga task
    │                                                 └──sends──→ [display queue] → display task
    │
    └── Timer tick ──→ Timer1 (10 tick) → periodic scan
                   ──→ Timer2 (1000 tick) → auto-off countdown, measurement refresh
```

## Key Architectural Insight: External DVOM Chip

The **dvom_TX** and **dvom_RX** tasks reveal that the multimeter function uses a **dedicated external digital voltmeter chip** communicating over UART (at 0x4000440C = USART2). This means:

- The USART2 port talks to the **DVOM chip**, not the FPGA (correcting our earlier assumption)
- The FPGA communication likely happens via **SPI or parallel GPIO** (the fpga task accesses 0x40003C08 which is in the SPI region)
- The multimeter measurements are done by dedicated hardware, not the MCU's internal ADC
- The DVOM chip handles voltage, current, resistance, continuity, etc.

## Identified FreeRTOS API Functions

| API Function | Ghidra Address | Real Address |
|---|---|---|
| xTaskCreate | FUN_0803b6a0 | 0x080426A0 |
| vTaskStartScheduler | FUN_0803a6d8 | 0x080416D8 |
| vTaskResume | FUN_0803a610 | 0x08041610 |
| vTaskSuspend | FUN_0803a78c | 0x0804178C |
| vTaskSuspendAll | FUN_0803a904 | 0x08041904 |
| vTaskDelay | FUN_0803a390 | 0x08041390 |
| xQueueGenericCreate | FUN_0803ab74 | 0x08041B74 |
| xQueueGenericSend | FUN_0803acf0 | 0x08041CF0 |
| xQueueReceive | FUN_0803b1d8 | 0x080421D8 |
| xQueueSemaphoreTake | FUN_0803b3a8 | 0x080423A8 |
| xQueueReset | FUN_0803ac38 | 0x08041C38 |
| xTimerCreate | FUN_0803bd88 | 0x08042D88 |
| pvPortMalloc | FUN_08035c64 | 0x0803CC64 |
| vPortFree | FUN_0803a1e0 | 0x080411E0 |
| taskENTER_CRITICAL | FUN_0803a168 | 0x08041168 |
| taskEXIT_CRITICAL | FUN_0803a1b0 | 0x080411B0 |

## Queues and Semaphores (RAM addresses)

| Handle | Length | Item Size | Type | Used By |
|---|---|---|---|---|
| 0x20002D6C | 20 | 1 byte | Queue | display task commands |
| 0x20002D70 | 15 | 1 byte | Queue | key task scan codes |
| 0x20002D74 | 10 | 2 bytes | Queue | dvom_TX data |
| 0x20002D78 | 15 | 1 byte | Queue | fpga task commands |
| 0x20002D7C | 1 | 0 | Binary Semaphore | dvom_RX (UART ISR trigger) |
| 0x20002D80 | 1 | 0 | Binary Semaphore | osc (acquisition trigger) |
| 0x20002D84 | 1 | 0 | Binary Semaphore | key (scan ISR notification) |
