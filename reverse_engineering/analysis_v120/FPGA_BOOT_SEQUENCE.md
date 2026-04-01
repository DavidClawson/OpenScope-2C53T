# FPGA Boot & Communication Sequence — FNIRSI 2C53T V1.2.0

## Complete Timeline: Power-On to First SPI3 Data

Based on comprehensive decompilation of three firmware sections (~20KB total):
- `init_function_decompile.txt` — 15.4KB init function (0x08023A50-0x080276F2)
- `fpga_task_decompile.txt` — 11KB FPGA task (0x08036934-0x08039870)
- `usart_protocol_decompile.txt` — USART2 IRQ handler + protocol spec

### Phase 1: Clock & GPIO Init (0x08023A50)

```
1. Enable clocks: GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, AFIO
2. Configure PC9 as output, set LOW initially (power hold — set HIGH later by main)
3. Configure 47 GPIO pins total:
   - Analog front-end MUX pins
   - LCD parallel bus (EXMC)
   - Button matrix inputs
   - SPI flash (SPI2) pins
   - USB pins
   - Debug UART pins
4. AFIO remap: AFIO_PCF0 = (AFIO_PCF0 & ~0xF000) | 0x2000
   → JTAG-DP disabled, SW-DP enabled → frees PB3/PB4/PB5 for SPI3
```

### Phase 2: Peripheral Init (0x08025800)

```
5. Enable NVIC IRQ9 (EXTI9_5 or DMA)
6. Enable USART clock: RCU_APB1EN |= 0x20000 (bit 17)
7. Configure TIM5 (0x40000C00): timer_init() with APB1-derived prescaler
8. Configure DMA0: clear channel 0 flags
9. Configure USART2 (0x40004400): 9600 baud, 8N1, TX+RX enabled
10. Initialize meter_state (0x200000F8): load defaults from data block
    - Validates data block header (checks for 0x55 or 0xAA signature)
    - Copies calibration values, range settings, mode defaults
```

### Phase 3: FreeRTOS Setup (0x08025BFC)

```
11. Create 7 queues:
    0x20002D6C: xQueueCreate(20, 1)  — USART command queue
    0x20002D70: xQueueCreate(15, 1)  — secondary command queue
    0x20002D74: xQueueCreate(10, 2)  — USART TX queue (2-byte items!)
    0x20002D78: xQueueCreate(15, 1)  — SPI3 data trigger queue
    0x20002D7C: xQueueCreate(1, 0)   — binary semaphore (USART RX complete)
    0x20002D80: xQueueCreate(1, 0)   — binary semaphore
    0x20002D84: xQueueCreate(1, 0)   — binary semaphore

12. Create 8 tasks:
    "Timer1"  @ 0x080400B9  prio=10   stack=10w
    "Timer2"  @ 0x080406C9  prio=1000 stack=1000w
    "display" @ 0x0803DA51  prio=1    stack=384w (1536B)
    "key"     @ 0x08040009  prio=4    stack=128w (512B)
    "osc"     @ 0x0804009D  prio=2    stack=256w (1024B)
    "fpga"    @ 0x0803E455  prio=3    stack=128w (512B)
    "dvom_TX" @ 0x0803E3F5  prio=2    stack=64w  (256B)
    "dvom_RX" @ 0x0803DAC1  prio=3    stack=128w (512B)

13. Signal semaphore at 0x20002D80
14. Suspend tasks at 0x20002D7C and 0x20002D84
```

### Phase 4: USART Init Commands (0x08025D96)

```
15. Send initialization commands to FPGA via USART2 (inline, not via queues)
    Format: 10-byte frames transmitted byte-by-byte with TX polling
    Commands sent: 0x01, 0x02, 0x06, 0x07, 0x08

    TX buffer layout (0x20000005, 10 bytes):
    [0] = byte 0 (unknown — may be 0x00 from BSS)
    [1] = byte 1 (unknown)
    [2] = high byte of command (0x00 for single-byte cmds)
    [3] = low byte of command (the actual command)
    [4-8] = parameters/padding (likely 0x00)
    [9] = checksum = (high + low) & 0xFF
```

### Phase 5: SPI3 Configuration (0x08026540)

```
16. Enable SPI3 clock: RCU_APB1EN |= 0x8000 (bit 15)
17. Enable RCU_APB2EN bits for AFIO (bit 0 = AFEN)

18. Configure SPI3 GPIO:
    PB3 = SPI3_SCK  (AF push-pull, 50MHz)
    PB4 = SPI3_MISO (input floating)
    PB5 = SPI3_MOSI (AF push-pull, 50MHz)
    PB6 = SPI3_CS   (GPIO output push-pull)

19. *** PC6 = FPGA control (output push-pull, set HIGH) ***
    GPIOC_BOP = (1 << 6)

20. Configure GPIOC pin for SPI3 (via gpio_init at 0x08026634)

21. SPI3 peripheral init via spi_init(0x40003C00, config):
    Config struct: {0x100, 0x01010100}
    → Full duplex, Master, /2 clock, MSB first, 8-bit
    → CPOL=1, CPHA=1 (MODE 3)
    → Software NSS (SSM=1, SSI=1)

22. Post-init:
    SPI3_CTL1 |= 0x02  (bit 1 = TXDMAEN or RXNEIE)
    SPI3_CTL1 |= 0x01  (bit 0 = RXDMAEN or TXEIE)
    SPI3_CTL0 |= 0x40  (SPE = SPI enable)

23. Enable more APB2 clocks: RCU_APB2EN |= 0x10
```

### Phase 6: SysTick Delays (0x08026638)

```
24. SysTick delay #1:
    LOAD = *(0x20002B1C) * 10  (value from RAM, set during timer init)
    Wait for COUNTFLAG

25. SysTick delay #2:
    LOAD = *(0x20002B20)
    Wait for COUNTFLAG

26. Timed loop: 2 iterations
    Counter starts at 100 (0x64)
    First: if count >= 51 → LOAD = reload_2 * 50, count -= 50
    Second: if count < 51 → LOAD = reload_2 * count, count = 0
    Each iteration: start SysTick, wait for COUNTFLAG, stop
```

### Phase 7: SPI3 FPGA Handshake (0x0802676E)

```
27. CS DEASSERT: GPIOB_BOP = 0x40 (PB6 HIGH)
28. SPI3 send 0x00 (dummy, CS high) — flush bus
29. Read SPI3 response (discard)

30. CS ASSERT: GPIOB_BC = 0x40 (PB6 LOW)
31. SPI3 send 0x05 ← FPGA COMMAND
32. Read SPI3 response (FPGA status/ID?)

33. SPI3 send 0x00 (parameter, still CS low)
34. Read SPI3 response

35. CS DEASSERT: GPIOB_BOP = 0x40 (PB6 HIGH)

36. SPI3 send 0x00 (dummy, CS high) — flush
37. Read response (discard)

38. SPI3 send 0x00 (dummy)
39. Read response (discard)

40. Another SysTick delay loop (100 → 50 → 0, same as step 26)
```

### Phase 8: Post-Handshake Config (0x080268EA+)

```
41. CS ASSERT again
42. More SPI3 exchanges (command 0x12 and others)
43. Configure ADC sampling parameters
44. Set up trigger levels and timebase
45. Initialize display buffer pointers

46. DMA configuration for SPI3 bulk transfers
47. Timer6/Timer7 configuration
48. Meter measurement function init
49. Watchdog timer init (~5 sec timeout)
50. Enable TMR3 (IRQ 29) — THIS DRIVES THE USART EXCHANGE
51. Enable TMR6/TMR7

52. *** PB11 set HIGH *** (FPGA signal, in mode_switch_reset_handler)
    GPIOB_BOP = 0x800

53. Start FreeRTOS scheduler (tail-call to 0x0803A6D8)
```

### Phase 9: Runtime (FreeRTOS tasks running)

```
54. TMR3 ISR fires periodically → calls usart2_irq_handler()
55. USART handler pumps TX bytes / receives RX bytes
56. On valid 12-byte RX frame (0x5A 0xA5 header):
    → sends to queue 0x20002D7C
    → triggers PendSV for context switch

57. FPGA task processes queue items:
    → usart_cmd_dispatcher processes commands
    → spi3_acquisition_task waits on queue 0x20002D78
    → When triggered, does SPI3 transfers (9 command types)

58. SPI3 data stored at meter_state+0x5B0 (CH1) and +0x9B0 (CH2)
```

---

## GPIO Pin Map (confirmed from decompilation)

| Pin | Function | Set During | Notes |
|-----|----------|-----------|-------|
| PC9 | Power hold | Boot (step 1) | Must be HIGH |
| PB8 | LCD backlight | Boot | HIGH to enable |
| **PC6** | FPGA control | Step 19 | **HIGH to enable FPGA SPI** |
| **PB11** | FPGA signal | Step 52 | **HIGH during active mode** |
| PB3 | SPI3_SCK | Step 18 | AF push-pull |
| PB4 | SPI3_MISO | Step 18 | Input floating |
| PB5 | SPI3_MOSI | Step 18 | AF push-pull |
| PB6 | SPI3_CS | Step 18 | GPIO, active LOW |
| PA2 | USART2_TX | Step 9 | To FPGA |
| PA3 | USART2_RX | Step 9 | From FPGA |

## USART Protocol Summary

**MCU → FPGA (TX):** 10 bytes from buffer at 0x20000005
```
[0]: unknown (likely 0x00)
[1]: unknown (likely 0x00)
[2]: command high byte
[3]: command low byte
[4-8]: parameters (0x00 for simple commands)
[9]: checksum = (byte[2] + byte[3]) & 0xFF
```

**FPGA → MCU (RX):** Two frame types
- **Data frame:** 0x5A 0xA5 + 10 data bytes = 12 bytes total
- **Echo frame:** 0xAA 0x55 + 8 bytes = 10 bytes total
  - Validation: rx[3] must match tx[3], rx[7] must be 0xAA

## What Was Missing From Our Test Firmware

| Item | Status | Impact |
|------|--------|--------|
| SPI3 Mode 3 (CPOL=1, CPHA=1) | Fixed in V2 | Required |
| PC6 HIGH | Fixed in V4 | Required — FPGA SPI enable |
| **PB11 HIGH** | **MISSING** | **FPGA active mode signal** |
| 10-byte USART frames | Fixed in V5 | Correct frame size |
| Boot commands 0x01-0x08 | Fixed in V5 | FPGA init |
| SysTick delays | Missing | Timing-sensitive init |
| SPI3 handshake (0x05 cmd) | Attempted but CS timing wrong | FPGA ID/config |
| TMR3 for USART polling | Missing | Drives USART exchange |
| Watchdog feeding | Missing | 5-sec timeout! |

## Files

- `init_function_decompile.txt` — 6,391 lines, full init pseudocode + disassembly
- `fpga_task_decompile.txt` — 5,701 lines, all 10 sub-functions
- `usart_protocol_decompile.txt` — 787 lines, complete protocol spec
