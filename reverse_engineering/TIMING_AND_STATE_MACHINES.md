# Timing, Timer Configuration, and State Machines — FNIRSI 2C53T V1.2.0

Extracted from `stock_v120` decompiled warehouse (ripcord `decompiled` table),
cross-referenced with `remaining_unknowns.md`, `FPGA_BOOT_SEQUENCE.md`,
`usart2_isr_state_machine.md`, and `STATE_STRUCTURE.md`.

---

## 1. Clock Tree Summary

| Clock   | Frequency | Derivation |
|---------|-----------|------------|
| HSE     | 8 MHz     | External crystal |
| SYSCLK  | 240 MHz   | PLL ×30 (configured in pre-main startup) |
| HCLK    | 240 MHz   | AHB prescaler /1 |
| PCLK2   | 120 MHz   | APB2 prescaler /2 |
| PCLK1   | 120 MHz   | APB1 prescaler /2 |
| ADC clk | 20 MHz    | PCLK2/6 (RCU_CFG0 bits [15:14]=0b10, bit 28 clear) |

The clock query function `FUN_0802e430` reads RCU_CFG0 (0x40021004) and
computes SYSCLK, HCLK, PCLK1, PCLK2, and ADC clock into a 5-word struct:
`[SYSCLK, HCLK, PCLK2, PCLK1, ADC_CLK]`. The variable `local_54` used
throughout the init is `PCLK1 = 120,000,000`.

---

## 2. Timer Configurations

### 2.1 SysTick (0xE000E010–0xE000E018)

**At FreeRTOS startup (FUN_0803e9d4):**
```
SysTick_CTRL  (0xE000E010) = 7        → enabled, interrupt enabled, processor clock
SysTick_LOAD  (0xE000E014) = 239999   → period = 240000/240MHz = 1.000 ms
SysTick_VAL   (0xE000E018) = 0        → clear counter
```
This gives the FreeRTOS tick at exactly **1 ms**.

**SysTick delay primitive (used throughout init):**

The delay helper uses `_DAT_20002b20` (= PCLK1 ticks per millisecond = 120,000)
as a base unit. The pattern is:
```c
// SysTick_LOAD = N * _DAT_20002b20    (N ms at PCLK1 rate)
// Poll SysTick_CTRL bit 16 (COUNTFLAG) until set
// Write SysTick_VAL = 0 to clear
```

The SysTick reload register is 24-bit (max 16,777,215). At 120 MHz per ms,
max single-shot = 139 ms. The firmware works around this with a loop that
breaks delays > 50 ms into 50 ms chunks:

```c
uint16_t remaining = delay_ms;
do {
    if (remaining < 51) {
        SysTick_LOAD = remaining * reload_per_ms;
        remaining = 0;
    } else {
        remaining -= 50;
        SysTick_LOAD = reload_per_ms * 50;   // 50 ms chunk
    }
    // wait for COUNTFLAG
    SysTick_VAL = 0;
} while (remaining != 0);
```

### 2.2 TMR1 / TIM5 (0x40015000 / 0x40015400)

**These are NOT TMR1/TIM5 in the AT32 datasheet.** Register base 0x40015000
is the **General-purpose timer (TMR5)** on AT32F403A, and 0x40015400 is
**TMR4**. The original analysis in `remaining_unknowns.md` used STM32
register names. Corrected mapping below.

#### Timer at 0x40015000 (TMR5 on AT32F403A)

From init lines 178–186:
```c
TMR5_PSC  (0x40015028) = PCLK1 / 1000000 - 1;   // = 120000000/1000000 - 1 = 119
TMR5_ARR  (0x4001502c) = 99;                       // auto-reload = 99
TMR5_DIER (0x40015014) |= 1;                       // update interrupt enable
TMR5_SMCR (0x40015004) = (TMR5_SMCR & ~0x100) | 0x100;  // slave mode
TMR5_CCMR1(0x40015018) |= 0x70;                    // OC1 PWM mode
TMR5_CCER (0x40015020) |= 3;                        // CC1 output enable + polarity
TMR5_CCR1 (0x40015034) = 100;                       // compare value (duty = 100%)
TMR5_BDTR (0x40015044) |= 0x8000;                   // MOE (main output enable)
TMR5_CR1  (0x40015000) = (TMR5_CR1 & ~0x70) | 1;   // counter enable, edge-aligned
```

**Timing:** PSC=119, ARR=99 → period = (119+1)×(99+1) / 120 MHz = **100 µs (10 kHz)**

**Purpose:** This generates a 10 kHz PWM output. `TMR5_CCR1 = 100` initially
(100% duty), but later updated to `_DAT_20001058 >> 8 & 0xff` — this is
the **LCD backlight brightness PWM**. The compare register is written
multiple times during init (lines 184, 234, 236, 1265, 1291).

#### Timer at 0x40015400 (TMR4 on AT32F403A)

From init lines 187–195:
```c
TMR4_PSC  (0x40015428) = PCLK1 / 50000 - 1;    // = 120000000/50000 - 1 = 2399
TMR4_ARR  (0x4001542c) = 0x18;                   // auto-reload = 24
TMR4_DIER (0x40015414) |= 1;                      // update interrupt enable
TMR4_SMCR (0x40015404) = (TMR4_SMCR & ~0x100) | 0x100;
TMR4_CCMR1(0x40015418) |= 0x70;                   // OC1 PWM mode
TMR4_CCER (0x40015420) |= 3;                       // CC1 output enable
TMR4_CCR1 (0x40015434) = 0;                        // compare value = 0 (0% duty)
TMR4_BDTR (0x40015444) |= 0x8000;                  // MOE
TMR4_CR1  (0x40015400) = (TMR4_CR1 & ~0x70) | 1;  // enable
```

**Timing:** PSC=2399, ARR=24 → period = (2399+1)×(24+1) / 120 MHz = **500 µs (2 kHz)**

**Purpose:** Signal generator DAC trigger timer. Compare value starts at 0
(no output), updated at runtime. Connected to DAC via the DAC trigger
configuration at line 783 (`DAC_CR |= 0x10150000`).

### 2.3 TMR7 (0x40001C00)

From init lines 196–205:
```c
TMR7_ARR  (0x40001c2c) = 0xFFE;       // auto-reload = 4094
TMR7_PSC  (0x40001c28) = 0;            // prescaler = 0
TMR7_DIER (0x40001c14) |= 1;           // update interrupt enable
TMR7_CR1  (0x40001c00) = (TMR7_CR1 & ~0x70) | 1;  // enable, edge-aligned
```

Then at lines 200–205, it loops through bVar23 from 0 to 24 (0x18),
calling `FUN_080043b4` which appears to generate waveform lookup points.

**Timing:** PSC=0, ARR=4094 → period = 1×4095 / 120 MHz = **34.125 µs (29.3 kHz)**

**Purpose:** Signal generator waveform DDS timer. Drives the 29.3 kHz
waveform sample rate. The loop at init builds a startup waveform table.

### 2.4 TMR3 (0x40000400) — **THE USART EXCHANGE DRIVER**

From init lines 1139–1156:
```c
// Enable TMR3 clock
RCU_APB1EN (0x4002101c) |= 2;         // bit 1 = TMR3EN

// Initial configuration
TMR3_PSC  (0x40000428) = PCLK1 / 10000 - 1;  // = 120000000/10000 - 1 = 11999
TMR3_ARR  (0x4000042c) = 0x13;                 // auto-reload = 19
TMR3_DIER (0x40000414) |= 1;                   // update interrupt enable
TMR3_EGR  (0x4000040c) |= 1;                   // re-init counter (UG bit)

// NVIC configuration
AIRCR = (AIRCR & 0xF8FF) | 0x5FA0300;         // priority group 3
NVIC_IPR29 = calculated_priority;               // TMR3 interrupt priority
NVIC_ISER0 = 0x20000000;                        // enable IRQ 29 (TMR3)

// Clear the CEN bit initially (timer NOT started yet)
TMR3_CR1  (0x40000400) &= ~0x71;               // disable, clear mode bits
```

**Initial timing:** PSC=11999, ARR=19 → period = (11999+1)×(19+1) / 120 MHz
= **2.000 ms (500 Hz)**

**TMR3 is enabled later (line 1234)** after mode setup:
```c
TMR3_CR1 (0x40000400) |= 1;   // enable counter
```

**Runtime reconfiguration (lines 1229–1234):** When `timebase_index >= 0x14`:
```c
TMR3_ARR  = lookup_table[timebase_index - 0x14];  // from DAT_080465a4
TMR3_PSC  = PCLK1 / 10000 - 1;                     // still 11999
TMR3_DIER |= 1;                                     // interrupt enable
TMR3_CNT  (0x40000424) = 0;                         // reset counter
TMR3_CR1  |= 1;                                     // enable
```

The lookup table at `DAT_080465a4` holds 9 entries (for `timebase_index`
values 0x14 through 0x1C). These values are the auto-reload for each slow
timebase setting. The prescaler stays at 11999, so each TMR3 tick = 1/10000 s.

**What TMR3 ISR does (FUN_0802b7b4 is the combined USART2+TMR3 handler):**

The USART2 ISR at 0x0802b7b4 handles both:
1. **RX path:** When USART2 RXNE flag set (bit 5 of USART2_SR at 0x40004400),
   reads byte from USART2_DR (0x40004404) into 12-byte buffer at 0x20004E11.
   Discriminates data frames (0x5A 0xA5, 12 bytes) from echo frames
   (0xAA 0x55, 10 bytes).
2. **TX path:** When USART2 TXE flag set (bit 7 of USART2_SR), writes next
   byte from 10-byte TX buffer at 0x20000005. After all 10 bytes sent,
   disables TXE interrupt.

**TMR3's role:** TMR3 fires at 500 Hz (2 ms period). Each TMR3 ISR
occurrence initiates the next USART2 TX frame by enabling the TXE interrupt
(`USART2_CR1 |= 0x80`). This drives the steady 500 Hz command pump to the
FPGA.

At 9600 baud with 10 bytes per frame (10 bits each = 100 bits), one frame
takes ~10.4 ms. So at 2 ms TMR3 period, TMR3 fires ~5 times per frame.
The ISR is re-entrant: it sends one byte per TXE event, not one frame per
TMR3 tick. TMR3 only *starts* a new frame.

**For slow timebases (≥0x14):** The auto-reload increases, slowing down
the USART command rate to match the slower acquisition rate. This avoids
flooding the FPGA with redundant commands.

### 2.5 TMR2 (0x40000000) — **FREQUENCY COUNTER INPUT CAPTURE**

From init lines 1304–1312:
```c
// Enable TMR2 clock
RCU_APB1EN (0x4002101c) |= 1;          // bit 0 = TMR2EN

TMR2_ARR  (0x4000002c) = 0xFFFFFFFF;   // free-running 32-bit counter
TMR2_PSC  (0x40000028) = 0;             // no prescaler
TMR2_DIER (0x40000014) |= 1;           // update interrupt enable
TMR2_CR1  (0x40000000) = (TMR2_CR1 & ~0x70) | 0x400;  // bit 10 set? (unusual)
// FUN_0803d734 configures capture channel: input capture on CH1
TMR2_CCMR1(0x40000008) = (TMR2_CCMR1 & ~0x70) | 0x67; // IC1 config: filter, prescaler
TMR2_CR1  |= 1;                         // enable
```

**Timing:** PSC=0, ARR=0xFFFFFFFF → free-running at 120 MHz, wraps every ~35.8 seconds.

**Purpose:** Input capture for frequency counter mode. The captured timer
value in TMR2_CCR1 gives the period of the input signal. This is only
active when `timebase_index >= 0x14` (frequency counter modes).

### 2.6 TMR4_CH3 (0x40000C00) — **PERIOD/DUTY MEASUREMENT**

From init lines 1295–1303:
```c
RCU_APB1EN |= 8;                        // bit 3 = TMR4EN (actually TMR4 at 0x40000C00? — verify)
TMR4_ARR  (0x40000c2c) = 0xFFFFFFFF;    // free-running
TMR4_PSC  (0x40000c28) = 0;             // no prescaler
TMR4_DIER (0x40000c14) |= 1;
TMR4_CR1  (0x40000c00) = (TMR4_CR1 & ~0x70) | 0x400;
// FUN_0803d734 configures: input capture, channel config
TMR4_CCMR1(0x40000c08) = (TMR4_CCMR1 & ~0x70) | 0x57;
TMR4_CR1  |= 1;
```

**Purpose:** Second input capture timer for period/duty cycle measurement.

### 2.7 TMR6 (0x40001800) — **WATCHDOG KICK TIMER**

From init lines 1368–1391:
```c
RCU_APB1EN |= 0x40;                     // bit 6 = TMR6EN
FUN_0802e430(&local_58);                 // get clocks → local_54 = PCLK1

TMR6_ARR  (0x4000182c) = 9999;
TMR6_PSC  (0x40001828) = PCLK1 / 10000 - 1;   // = 11999
TMR6_DIER (0x40001814) |= 1;                    // update interrupt enable
TMR6_EGR  (0x4000180c) |= 1;                    // force update
// NVIC priority config for IRQ 43 (TMR8_BRK_TMR12 shared)
NVIC_ISER1 = 0x800;                              // enable IRQ 43
TMR6_CR1  (0x40001800) = (TMR6_CR1 & ~0x370) | 1;  // enable
```

**Timing:** PSC=11999, ARR=9999 → period = 12000×10000 / 120 MHz = **1.000 second**

**Purpose:** Fires once per second. The ISR at vector 43 (shared TMR8_BRK)
kicks the independent watchdog (FWDGT). This is why the watchdog timeout
is set to ~2–5 seconds — the 1 Hz TMR6 ISR must run to prevent reset.

### 2.8 TMR5 at 0x40001400 (Signal Generator Frequency)

From init lines 761–784:
```c
TMR5_ARR  (0x4000142c) = 1 or 0x13;     // depends on freq setting
TMR5_PSC  (0x40001428) = computed;       // from FUN_0802e430 and DAT_20000f54
TMR5_DIER (0x40001414) |= 1;
TMR5_CR1  (0x40001404) = (... & ~0x70) | 0x20;  // down-counting mode
```

**Purpose:** Drives the DAC update rate for the signal generator. The
prescaler is computed dynamically from `DAT_20000f54 * 200` to set the
desired output frequency.

### 2.9 USART2 Baud Rate (0x40004400)

From init lines 740–748:
```c
FUN_0802e430(&local_58);    // get clocks
baud_div = (PCLK1 * 10) / 9600;     // = 120000000*10/9600 = 125000
mantissa = baud_div / 10;            // = 12500
if (baud_div % 10 > 4) mantissa++;   // round up
USART2_BRR (0x40004408) = mantissa & 0xFFFF | (USART2_BRR & 0xFFFF0000);
USART2_CR3  (0x40004410) &= ~0x3000;   // no CTS/RTS
USART2_CR1  (0x4000440c) = (... & ~0x3000) | 0x2C;   // TE, RE, RXNEIE enable
```

Computed baud = PCLK1 / (16 × mantissa_part) ≈ **9600 baud** (exact).

---

## 3. NVIC Interrupt Configuration

| IRQ | Vector | Peripheral | ISER bit | Priority byte | Where enabled |
|-----|--------|-----------|----------|---------------|---------------|
| 9   | EXTI9_5 | External interrupts | ISER0 bit 9 | DAT_e000e409 | Line 720 |
| 12  | DMA1_CH2 | LCD DMA complete | ISER0 bit 12 | — | FUN_0803bee0 |
| 20  | USART2 | USART2 RX/TX | ISER0 bit 20 | DAT_e000e426 | Line 794 |
| 29  | TMR3 | USART exchange pump | ISER0 bit 29 | DAT_e000e41d | Line 1155 |
| 38  | TMR6 | Watchdog kick | ISER1 bit 6 | — | Line 739 |
| 43  | TMR8_BRK/TMR12 | FatFs (repurposed) | ISER1 bit 11 | DAT_e000e42b | Line 1390 |

Priority group = 3 (AIRCR bits [10:8] = 0b011 → 4 bits preempt, 0 bits sub).

---

## 4. Watchdog Configuration

From init lines 1364–1367:
```c
FWDGT_CTL (0x40003000) = 0xCCCC;     // start watchdog
FWDGT_PSC (0x40003004) = (/8 & 0xFFF8) | 4;  // prescaler /64
FWDGT_RLD (0x40003008) = 0x4E1;      // reload = 1249
```

**Timeout:** (1249+1) × 64 / 40,000 Hz (LSI) = **2.0 seconds**

TMR6 fires every 1.0 s and kicks the watchdog. If the system hangs for
>2 seconds, watchdog triggers reset.

**Our firmware must feed the watchdog!** Missing watchdog feed = hard reset
after 2 seconds.

---

## 5. Delay Sequences in Boot/Init Path

Every delay below uses the SysTick busy-wait primitive described in §2.1.
`reload_per_ms` = `_DAT_20002b20` = 120,000 (PCLK1/1000).

### 5.1 LCD Init Delays (lines 95–150)

| # | Delay (ms) | Before | After |
|---|-----------|--------|-------|
| 1 | SysTick poll (no explicit reload) | EXMC config written | Wait for SysTick to expire |
| 2 | 120 ms (uVar15=0x78, chunked ×50) | LCD reset pulse (0x40011414=0x40, 0x40011410=0x40) | LCD command register init |
| 3 | 100 ms (uVar16=100, chunked ×50) | LCD fill black + init commands | SPI3 GPIO enable |

### 5.2 Boot Logo / Splash Delays (lines 219–300)

| # | Delay (ms) | Before | After |
|---|-----------|--------|-------|
| 4 | 5 ms per iteration | Button scan loop (checks GPIOC bit 8) | Re-check button |
| 5 | 500 ms | File system mount failed → display error | Continue init |
| 6 | 3 ms per color step | Boot logo color gradient (256 steps) | Next color |

Total splash delay: up to **768 ms** (256 × 3 ms) for boot animation.

### 5.3 SPI3 FPGA Handshake Delays (lines 559, 1007–1021)

| # | Delay (ms) | Before | After |
|---|-----------|--------|-------|
| 7 | 30 ms | Pre-SPI3 setup | SPI3 chip select assert |
| 8 | 100 ms (chunked ×50) | After SPI3 handshake complete | Post-handshake config |
| 9 | 100 ms (chunked ×50) | After SPI3 bulk data exchange | Second SPI3 command sequence |

### 5.4 Post-Config Delays (lines 1326–1345)

| # | Delay (ms) | Before | After |
|---|-----------|--------|-------|
| 10 | 500 ms (chunked ×50) | TMR3 started, all timers running | Wait for FPGA ready signal |
| 11 | Button-release poll | Check GPIOC bit 8 released | Proceed |
| 12 | 10 ms | After GPIOC stable | Final USART2 baud config |

### 5.5 Summary: Total Init Delay Budget

| Phase | Approx delay |
|-------|-------------|
| LCD init | ~220 ms |
| Boot logo | ~770 ms |
| SPI3 handshake | ~230 ms |
| Post-config | ~510 ms |
| **Total** | **~1.73 seconds** |

---

## 6. State Machines

### 6.1 Mode Switch State Machine

The global mode is stored at `DAT_20001060` (= `state+0xF68`, the
`system_mode` field). `FUN_0800f908` is the mode switch dispatcher
called on mode change events.

**Mode values and their FPGA command sequences:**

| Mode | Name | FPGA commands sent (via queue 0x20002D6C) |
|------|------|-------------------------------------------|
| 0 | Scope CH1 | 0x00, 0x01, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11 |
| 1 | Scope CH2 | 0x00, 0x09, 0x07/0x0A*, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E |
| 2 | Signal gen | 0x02, 0x03, 0x04, 0x05, 0x06, 0x08, 0x09, 0x07/0x0A* |
| 3 | Dual scope | 0x00, 0x08, 0x09, 0x07/0x0A*, 0x16, 0x17, 0x18, 0x19 |
| 4 | Freq counter | 0x00, 0x1F, 0x09, 0x20, 0x21 |
| 5 | Period meas | 0x00, 0x25, 0x09, 0x26, 0x27, 0x28 |
| 6 | Duty cycle | 0x29 |
| 7 | Unknown | 0x15 |
| 8 | Continuity | 0x00, 0x2C |
| 9 | XY/Math | 0x00, 0x12, 0x13, 0x14, 0x09, 0x07/0x0A* |

*0x07 or 0x0A selected by GPIOC bit 7 state (probe detect pin).

**Trigger:** Mode switch is triggered from `FUN_08006c78` (the battery/charger
monitor function), which also handles AC adapter detect (GPIOC bit 7) and
low-battery shutdown. When the adapter state changes, it queues cmd 0x09
(AC power notify) or 0x0A (battery notify), and if mode needs resetting,
calls `FUN_0800f908`.

### 6.2 Scope Mode Entry Sequence

When entering scope mode (mode 0), the full init sequence is:

1. `FUN_0800f908` sends FPGA commands: 0x00, 0x01, 0x0B–0x11
2. Init code (lines 1157–1174) sets:
   - `DAT_20001061 = 1` (scope active flag)
   - `USART2_CR1 |= 0x2000` (enable USART2 — may be re-enable)
   - Suspends tasks at 0x20002DA0 and 0x20002DA4
   - Sets GPIO pins for scope front-end
   - Clears scope state: trigger position, measurement accumulators
   - Sets `DAT_20001030 = 0xFF` (trigger armed flag)
   - Clears `_DAT_20001034 = 0` (exchange lock → enable data frames)
3. TMR3 is already running at 500 Hz from earlier init
4. TMR3 ISR drives USART2 TX → FPGA processes commands → FPGA sends
   scope data back

### 6.3 Scope Timebase Change

The timebase index lives at `DAT_20000125` (= `state+0x2D`).

**For fast timebases (index 0x00–0x13):**
- TMR3 remains at default: PSC=11999, ARR=19 → 500 Hz → 2 ms per USART exchange
- Lines 1211–1218: TMR3 is **disabled** (`TMR3_CR1 &= ~1`)
- `_DAT_20000eac` is set to 0x12D (301 decimal) or 0x12D0000 depending on
  `DAT_2000010f`, which appears to be a sample count / buffer control

**For slow timebases (index ≥ 0x14):**
- Lines 1220–1234: TMR3 is **reconfigured**:
  ```c
  TMR3_ARR = lookup_table[(timebase_index - 0x14) * 4];  // from DAT_080465a4
  TMR3_PSC = PCLK1 / 10000 - 1;                           // still 11999
  TMR3_DIER |= 1;                                          // interrupt enable
  TMR3_CNT = 0;                                            // reset counter
  TMR3_CR1 |= 1;                                           // enable
  ```
- The lookup table contains 9 values for auto-reload, indexed by
  `(timebase_index - 0x14)`. Since PSC=11999 → tick = 0.1 ms, the
  period = ARR × 0.1 ms. If ARR values are e.g. [19, 49, 99, 199, ...],
  the periods would be [2ms, 5ms, 10ms, 20ms, ...].

### 6.4 USART2 ISR Frame State Machine (recap)

Documented fully in `usart2_isr_state_machine.md`. Summary:

```
RX byte 0:  if not 0x5A and not 0xAA → discard
RX byte 1:  valid pairs: (0x5A,0xA5) or (0xAA,0x55), else discard
RX bytes 2+:
  Echo frame (0xAA,0x55): complete at index 10
    validate: byte[3]==sent_cmd, byte[7]==0xAA
  Data frame (0x5A,0xA5): complete at index 12
    if tx_done && exchange_lock==0 → give semaphore → wake parser

TX path: sends 10 bytes from buffer at 0x20000005
  [2]=cmd_hi, [3]=cmd_lo, [9]=checksum, rest=0x00
  On final byte: disable TXE interrupt
```

### 6.5 Acquisition State Machine

The SPI3 acquisition task (`FUN_0803e455` at priority 3) waits on queue
`0x20002D78`. Trigger bytes sent during init (after SPI3 handshake):
1, 2, 6, 7, 8.

Each trigger byte selects an acquisition sub-mode in the SPI3 task:
- 1: Single channel scope acquisition
- 2: Dual channel scope acquisition
- 6–8: Specialized acquisition modes (frequency, period, etc.)

The acquisition task performs polled SPI3 transfers (not DMA) to read
waveform data from the FPGA. Data is stored at:
- CH1: `state+0x5B0` (0x200006A8)
- CH2: `state+0x9B0` (0x20000AA8)

### 6.6 FreeRTOS Task Priorities and Timing

| Task | Entry | Priority | Stack | Role |
|------|-------|----------|-------|------|
| Timer1 | 0x080400B9 | 10 | 40B | FreeRTOS timer daemon |
| Timer2 | 0x080406C9 | 1000 | 4KB | High-priority timer |
| display | 0x0803DA51 | 1 | 1.5KB | LCD rendering |
| key | 0x08040009 | 4 | 512B | Button scanning |
| osc | 0x0804009D | 2 | 1KB | Oscilloscope processing |
| fpga | 0x0803E455 | 3 | 512B | FPGA comm + SPI3 acq |
| dvom_TX | 0x0803E3F5 | 2 | 256B | USART2 TX frame builder |
| dvom_RX | 0x0803DAC1 | 3 | 512B | Meter data parser |

---

## 7. Timer Configuration Table (Summary)

| Timer | Base | PSC | ARR | Period | Frequency | Purpose |
|-------|------|-----|-----|--------|-----------|---------|
| SysTick | 0xE000E010 | — | 239999 | 1.000 ms | 1 kHz | FreeRTOS tick |
| TMR5 (PWM) | 0x40015000 | 119 | 99 | 100 µs | 10 kHz | LCD backlight PWM |
| TMR4 (DAC) | 0x40015400 | 2399 | 24 | 500 µs | 2 kHz | Signal gen DAC trigger |
| TMR7 (DDS) | 0x40001C00 | 0 | 4094 | 34.1 µs | 29.3 kHz | Signal gen waveform DDS |
| **TMR3** | **0x40000400** | **11999** | **19** | **2.000 ms** | **500 Hz** | **USART exchange pump** |
| TMR2 (IC) | 0x40000000 | 0 | 0xFFFFFFFF | free-run | 120 MHz | Freq counter input capture |
| TMR4 (IC) | 0x40000C00 | 0 | 0xFFFFFFFF | free-run | 120 MHz | Period/duty input capture |
| TMR6 (WDG) | 0x40001800 | 11999 | 9999 | 1.000 s | 1 Hz | Watchdog feed |
| FWDGT | 0x40003000 | /64 | 1249 | 2.000 s | — | Independent watchdog |

---

## 8. Critical Timing for OSC Firmware Replication

### What we MUST match exactly:

1. **TMR3 at 500 Hz (2 ms period)** — drives USART2 frame pump. If this
   is wrong, the FPGA won't receive commands at the expected rate and
   data frames will be misaligned.

2. **USART2 at 9600 baud** — already implemented correctly in our firmware.

3. **SysTick delays during FPGA boot** — the FPGA needs specific timing
   between SPI3 commands. Our firmware should insert equivalent delays:
   - 30 ms before first SPI3 handshake
   - 100 ms after handshake completion
   - 100 ms between SPI3 bulk phases
   - 500 ms after TMR3 start (FPGA stabilization)

4. **Watchdog feed every <2 seconds** — TMR6 at 1 Hz handles this in stock.
   Our firmware must kick FWDGT_CTL=0xAAAA at ≤1 Hz or the device resets.

5. **Signal generator timers (TMR4 DAC at 2 kHz, TMR7 DDS at 29.3 kHz)**
   — only needed if implementing signal generator mode.

### What we can vary:

- LCD backlight PWM frequency (10 kHz is fine, exact value doesn't matter)
- FreeRTOS tick rate (1 ms is standard, could be different)
- Task priorities (as long as fpga > display and dvom_RX gets CPU time)

---

## 9. Scope Mode Entry — Complete Delay Sequence

```
1. Mode switch event detected (button press or startup)
2. FUN_0800f908 queues 9 FPGA commands for mode 0:
   [0x00, 0x01, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11]
   Each command = one 10-byte USART2 TX frame
   At 500 Hz TMR3 rate: 9 commands × 2 ms = 18 ms to send all
   
3. dvom_tx_task dequeues each command, fills TX buffer, starts TX
4. USART2 ISR sends 10 bytes at 9600 baud = 10.4 ms per frame
   → 9 frames = ~94 ms total TX time
   
5. FPGA echoes each command (10-byte echo frame, ~10.4 ms each)
   → Echo reception overlaps with next TX frame
   
6. After all commands acknowledged, FPGA begins sending data frames
   (12 bytes at 9600 baud = 12.5 ms each)
   
7. Data frame received → dvom_rx_semaphore released → meter parser runs
8. SPI3 acquisition task triggered → reads waveform data from FPGA
   
Total time from mode switch to first waveform data: ~200-300 ms
```
