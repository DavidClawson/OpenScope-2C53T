# RTOS Analysis

## Identification: FreeRTOS (Confirmed)

The firmware runs **FreeRTOS**, confirmed by two signature strings found in the binary:

- `IDLE` — the FreeRTOS idle task name (created automatically by the kernel)
- `Tmr Svc` — the FreeRTOS timer service daemon task name

These are the default task names assigned by `vTaskStartScheduler()` and are definitive identifiers of FreeRTOS.

### Evidence from Code Analysis

| Finding | FreeRTOS Function |
|---|---|
| PendSV handler (vector 14, 0x08028D51) | `xPortPendSVHandler` — RTOS context switch |
| SVCall handler (vector 11, 0x08028DC1) | `vPortSVCHandler` — first task launch |
| SysTick handler (vector 15, 0x0802A995) | `xPortSysTickHandler` — tick increment + yield check |
| `isCurrentModePrivileged()` call | FreeRTOS kernel privilege check |
| `DataSynchronizationBarrier` / `InstructionSynchronizationBarrier` | Memory barriers in context switch |
| SCB_ICSR write (0xE000ED04 = 0x10000000) | Trigger PendSV for context switch |
| Idle loop stuck at 0x0803A634 | `prvIdleTask` — runs when no other task is ready |

### Why FreeRTOS (not RT-Thread)

Despite being a Chinese product on a GD32 chip (where RT-Thread is dominant), FNIRSI chose FreeRTOS. Possible reasons:
- FreeRTOS has a smaller kernel footprint (~5-10KB vs RT-Thread's full stack)
- GigaDevice provides FreeRTOS examples alongside RT-Thread
- FreeRTOS is simpler when you only need basic task scheduling

## Firmware Architecture

The firmware is multi-threaded with FreeRTOS managing concurrent tasks:

```
FreeRTOS Kernel (Cortex-M4 port)
│
├── Display Task
│   ├── Renders UI to framebuffer (RGB565)
│   ├── Handles menu drawing
│   ├── Waveform display
│   └── Uses draw_text (0x08032f6c), draw_ui_element (0x08008154)
│
├── Acquisition Task
│   ├── Communicates with FPGA via USART2
│   ├── Reads ADC sample data
│   ├── Processes waveform data
│   └── Triggers on signal events
│
├── Input Task
│   ├── Touch panel via I2C
│   ├── Button state via GPIO
│   └── Posts events to other tasks via FreeRTOS queues
│
├── Measurement Task
│   ├── Calculates frequency, voltage, duty cycle, etc.
│   ├── Auto-ranging (floating point math in draw_measurement_display)
│   └── Updates measurement display
│
├── USB Task
│   ├── USB mass storage class (FAT32 filesystem)
│   ├── "USB Sharing" mode
│   └── USB interrupt handler at vector 36
│
├── Timer Service (Tmr Svc)
│   ├── FreeRTOS software timers
│   ├── Likely handles auto-shutdown countdown
│   └── Buzzer timing
│
└── IDLE Task
    ├── Runs when no other task needs CPU
    ├── May include power-saving (WFI instruction)
    └── Stuck point in emulator (0x0803A634)
```

## FreeRTOS Kernel Functions Identified

| Address | Likely FreeRTOS Function | Description |
|---|---|---|
| 0803a610 | `vTaskSwitchContext` or `xTaskResumeAll` | Task scheduling with PendSV trigger |
| 08028D51 | `xPortPendSVHandler` | Context switch — saves/restores task registers |
| 08028DC1 | `vPortSVCHandler` | Starts first task after scheduler launch |
| 0802A995 | `xPortSysTickHandler` | Increments tick count, checks for yield |
| 08033cfc | `pvPortMalloc` | FreeRTOS memory allocator (called as `memory_alloc`) |

## Implications for Emulation

### Why Unicorn Engine Gets Stuck

Unicorn doesn't model the NVIC interrupt controller. The FreeRTOS scheduler depends on:
1. **SysTick firing periodically** — increments the tick count and checks if a higher-priority task is ready
2. **PendSV being triggered** — performs the actual context switch between tasks
3. **Proper interrupt priority** — FreeRTOS uses NVIC priority levels to manage critical sections

Without SysTick interrupts, the scheduler never runs, all tasks stay blocked, and the CPU sits in the idle task forever.

### Why Renode Would Work

Renode properly models:
- NVIC with priority levels and preemption
- SysTick with configurable frequency (120MHz for GD32F307)
- PendSV/SVCall handling
- Exception entry/return with proper stack frame management

This means FreeRTOS tasks would actually run, display code would execute, and we'd see framebuffer output.

## Implications for Custom Firmware

For a firmware rewrite, FreeRTOS is freely available (MIT license) and well-documented:

1. **Use the same RTOS** — FreeRTOS Cortex-M4 port, same task structure
2. **GD32F307 BSP available** — GigaDevice provides FreeRTOS board support packages
3. **Task architecture is known** — create the same task structure (display, acquisition, input, measurement, USB)
4. **Existing FreeRTOS config** — could extract `FreeRTOSConfig.h` values from the firmware (heap size, tick rate, max priorities, stack sizes)

### Extracting FreeRTOS Configuration

The `FreeRTOSConfig.h` values can be inferred from the binary:
- **configTICK_RATE_HZ** — determined by SysTick reload value
- **configMINIMAL_STACK_SIZE** — visible in idle task stack allocation
- **configTOTAL_HEAP_SIZE** — visible in heap memory pool size
- **configMAX_PRIORITIES** — visible in priority level checks

These would be found by examining the RTOS initialization code around the reset vector (0x08007311).
