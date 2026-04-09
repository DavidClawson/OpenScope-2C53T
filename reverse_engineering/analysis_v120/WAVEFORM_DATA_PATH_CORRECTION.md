# Critical Correction: Waveform Data Comes via FSMC, Not SPI3

**Date:** 2026-04-09
**Source:** Automated decompilation analysis of stock firmware V1.2.0
via [ripcord](https://github.com/DavidClawson/ripcord) pipeline
(305/305 functions named, 100% coverage)
**Confidence:** HIGH — based on decompiled C, DMA register writes,
and xref analysis across the entire stock binary

## Summary

The custom firmware's `fpga_acquisition_task` reads waveform samples
over SPI3 (`spi3_xfer(0xFF)` in a polling loop). **The stock firmware
does not use SPI3 for waveform data.** It reads ADC samples from the
FPGA through the FSMC parallel bus at `0x60020000` — the same
memory-mapped interface the LCD uses — transferring bulk data via
DMA1 Channel 2.

The SPI peripheral at `0x40003800` in the stock firmware is **SPI2**
(used for the external W25Q128 SPI flash), not an FPGA data channel.
Stock firmware SPI2 sends standard flash commands (0x03=READ,
0x05=READ_STATUS, 0x20=SECTOR_ERASE) to the flash chip via GPIOC
pin 12 as chip select.

This explains why `spi3_xfer(0xFF)` returns constant 0xFF — there is
no waveform data on that bus.

## Stock Firmware FPGA Data Path (Decompiled)

### Two FSMC Registers

The FPGA sits behind FSMC Bank 1 (NOR/SRAM), addressed via A17:

| Address      | Direction | Purpose |
|-------------|-----------|---------|
| `0x6001FFFE` | Write only | Command/control register (A17=0, RS low) |
| `0x60020000` | Read/Write | Data register (A17=1, RS high) |

These are the same addresses `lcd.h` already defines as `LCD_CMD_ADDR`
and `LCD_DATA_ADDR`. The LCD and FPGA acquisition share the bus.

### DMA Acquisition Init: `FUN_0803fee0` (ripcord name: `init_dma_acquisition`)

Decompiled C (cleaned up with register names):

```c
void init_dma_acquisition(void)
{
    // Step 1: Write acquisition parameters to FPGA via FSMC
    *((volatile uint16_t *)0x60020000) =
        (DAT_20008356 + DAT_20008352) - 1) & 0xFF;   // range to FPGA data reg
    *((volatile uint16_t *)0x6001FFFE) = DAT_20008348; // command to FPGA cmd reg

    // Step 2: Enable DMA1 clock
    // RCU_AHBEN (0x40021014) |= bit 0 (DMA1 clock enable)
    *(volatile uint32_t *)0x40021014 |= 1;

    // Step 3: Reset DMA1 Channel 2 config
    // DMA1_CH2_CTL (0x40020004+0x1C-8 region, but offset 0x04 from base)
    *(volatile uint32_t *)0x40020004 |= 0xF0;   // clear channel flags

    // Step 4: Configure DMA1 Channel 2
    *(volatile uint32_t *)0x40020020 =           // DMA1_C2DTCNT: transfer count
        (uint16_t)(DAT_20008356 * DAT_20008354); //   = sample_count * stride
    *(volatile uint32_t *)0x40020024 =           // DMA1_C2MADDR: memory dest
        DAT_20008358;                            //   = RAM buffer pointer
    *(volatile uint32_t *)0x40020028 =           // DMA1_C2PADDR: peripheral src
        0x60020000;                              //   = FPGA data register (!!!)

    // Step 5: Set DMA1 Ch2 interrupt priority in NVIC
    // SCB_AIRCR priority group config, then NVIC_IPR for IRQ 12
    *(volatile uint32_t *)0xE000ED0C =
        (*(volatile uint32_t *)0xE000ED0C & 0xF8FF) | 0x05FA0300;
    // ... priority calculation ...
    DAT_e000e40c = (priority_bits << 4);

    // Step 6: Enable DMA1 Ch2 interrupt in NVIC (IRQ 12 = bit 12)
    *(volatile uint32_t *)0xE000E100 = 0x1000;   // NVIC_ISER0 bit 12

    // Step 7: Enable DMA1 Channel 2 with config
    *(volatile uint32_t *)0x4002001C = 0x4543;    // DMA1_C2CTL
    //   bit 0:  CHEN  = 1 (channel enable)
    //   bit 1:  FTFIE = 1 (full transfer complete interrupt enable)
    //   bit 6:  DTD   = 0 (peripheral → memory)
    //   bit 8:  PWIDTH = 01 (16-bit peripheral)
    //   bit 10: MWIDTH = 01 (16-bit memory)
    //   bit 14: CMEN  = 1 (circular mode? or priority)
    //   = 0x4543
}
```

### DMA ISR: `FUN_08006670` (ripcord name: `setup_acquisition_dma`)

This is the DMA1 Channel 2 transfer-complete interrupt handler. It
implements double-buffering:

```c
void DMA1_Channel2_IRQHandler(void)
{
    // Check DMA1 status: transfer complete flag for channel 2
    if (*(volatile uint32_t *)0x40020000 & (1 << 5)) {  // DMA1_STS FTIF2

        if (DAT_200000F2 < 2) {
            // First or second transfer: reconfigure for next buffer half
            DAT_200000F2++;

            // Re-enable DMA1 clock, reset channel flags
            *(volatile uint32_t *)0x40021014 |= 1;
            *(volatile uint32_t *)0x40020004 |= 0xF0;

            // Reconfigure DMA1 Ch2 for next transfer
            *(volatile uint32_t *)0x40020020 =       // transfer count
                (uint)(DAT_20008342) * (uint)(DAT_20008340) * 0x8000 >> 16;
            *(volatile uint32_t *)0x40020024 = 0x2000835C;  // second buffer
            *(volatile uint32_t *)0x40020028 = 0x60020000;  // FPGA data reg
            *(volatile uint32_t *)0x4002001C = 0x4503;      // re-enable

        } else {
            // Third transfer complete: signal acquisition done
            // Post to FreeRTOS queue + trigger PendSV for context switch
            FUN_0803f09c(queue_handle, &local_c);
            *(volatile uint32_t *)0xE000ED04 = 0x10000000;  // SCB_ICSR PendSV
        }
    }
}
```

### FPGA Register Read: `FUN_0801de00` (ripcord name: `read_fpga_status_reg`)

```c
uint16_t read_fpga_status_reg(void)
{
    return *(volatile uint16_t *)0x60020000;  // single 16-bit read from FPGA
}
```

### FPGA Command Write: `FUN_0801de78` (ripcord name: `fpga_write_control_value`)

```c
void fpga_write_control_value(void)
{
    *(volatile uint16_t *)0x6001FFFE = DAT_20008348;  // write command byte
}
```

### Set Acquisition Range: `FUN_0801de18` (ripcord name: `set_acquisition_range`)

```c
void set_acquisition_range(int p1, short start, int p3, short end)
{
    *(volatile uint16_t *)0x6001FFFE = DAT_2000834C;         // command
    *(volatile uint16_t *)0x60020000 = (end + start - 1) & 0xFF;  // range param
}
```

## What The Stock SPI2 Actually Does

The SPI peripheral at `0x40003800` (which our firmware calls SPI3)
is used exclusively for external flash memory access:

| Function | ripcord name | What it does |
|----------|-------------|-------------|
| `FUN_080330c4` | `spi2_exchange_byte` | Full-duplex SPI byte via `0x4000380C` (SPI2_DR) |
| `FUN_08033048` | `spi_flash_read` | CS assert → cmd 0x03 → 3-byte addr → read N bytes → CS deassert |
| `FUN_0803311c` | `spi_flash_wait_ready` | CS assert → cmd 0x05 → poll until WIP bit clear |
| `FUN_0803336c` | `spi_flash_write` | CS assert → cmd 0x02 → 3-byte addr → write N bytes |
| `FUN_0803316c` | `flash_write_with_erase` | Sector erase (0x20) then page program |

CS pin is GPIOC bit 12 (`0x40010C10`/`0x40010C14`), which matches
the SPI flash chip select, not an FPGA data channel.

## Key RAM State Variables

These addresses in the stock firmware control the acquisition DMA:

| RAM Address | Size | Purpose |
|------------|------|---------|
| `0x20008340` | u16 | Display width (init: 0x140 = 320) |
| `0x20008342` | u16 | Display height (init: 0xF0 = 240) |
| `0x20008348` | u16 | FPGA command byte (init: 0x2C = "memory write") |
| `0x2000834A` | u16 | FPGA command byte 2 (init: 0x2A = "column addr set") |
| `0x2000834C` | u16 | FPGA command byte 3 (init: 0x2B = "page addr set") |
| `0x20008352` | u16 | Acquisition range start |
| `0x20008354` | u16 | Acquisition stride |
| `0x20008356` | u16 | Acquisition sample count |
| `0x20008358` | u32 | DMA destination buffer pointer |
| `0x2000835C` | buf | Second DMA buffer (double-buffering target) |
| `0x200000F2` | u8  | DMA transfer counter (0,1,2 = three-phase) |
| `0x20001060` | u8  | Operating mode selector (0-9) for command dispatcher |

## Required Changes to Custom Firmware

### 1. Remove SPI3 Acquisition Code

The entire `fpga_acquisition_task` SPI3 polling loop in `fpga.c`
(lines ~1157-1340) is reading from the wrong peripheral. Remove:
- `spi3_xfer()` calls for waveform data
- SPI3 CS assert/deassert around acquisition reads
- SPI3 timeout/backoff logic for acquisition

SPI3 init code can stay if needed for the external flash, but rename
it to reflect its actual purpose (flash access, not FPGA ADC).

### 2. Add FSMC-Based Acquisition

The waveform data path should use the FSMC registers that `lcd.h`
already defines:

```c
#define FPGA_CMD_REG   LCD_CMD_ADDR    /* 0x6001FFFE — shared with LCD */
#define FPGA_DATA_REG  LCD_DATA_ADDR   /* 0x60020000 — shared with LCD */

static inline uint16_t fpga_read_data(void) {
    return *FPGA_DATA_REG;
}

static inline void fpga_write_cmd(uint16_t cmd) {
    *FPGA_CMD_REG = cmd;
}

static inline void fpga_write_data(uint16_t data) {
    *FPGA_DATA_REG = data;
}
```

### 3. Implement DMA1 Channel 2 Transfer

Replace the SPI3 polling loop with DMA-based acquisition:

```c
/* DMA1 Channel 2 registers (AT32F403A) */
#define DMA1_BASE       0x40020000
#define DMA1_STS        (*(volatile uint32_t *)(DMA1_BASE + 0x00))
#define DMA1_CLR        (*(volatile uint32_t *)(DMA1_BASE + 0x04))
#define DMA1_C2CTL      (*(volatile uint32_t *)(DMA1_BASE + 0x1C))
#define DMA1_C2DTCNT    (*(volatile uint32_t *)(DMA1_BASE + 0x20))
#define DMA1_C2PADDR    (*(volatile uint32_t *)(DMA1_BASE + 0x24))
#define DMA1_C2MADDR    (*(volatile uint32_t *)(DMA1_BASE + 0x28))

/* Note: AT32F403A DMA channel register layout:
 *   Ch1: offset 0x08-0x14
 *   Ch2: offset 0x1C-0x28  <-- this one
 *   Ch3: offset 0x30-0x3C
 * Verify against AT32F403A reference manual Table "DMA register map" */

void fpga_start_acquisition(uint16_t sample_count, uint16_t stride,
                            volatile uint16_t *dest_buf)
{
    /* Write acquisition parameters to FPGA */
    fpga_write_cmd(acquisition_command);
    fpga_write_data((sample_count + range_start - 1) & 0xFF);

    /* Enable DMA1 clock */
    CRM->ahben |= CRM_DMA1_PERIPH_CLOCK;

    /* Clear DMA1 Ch2 flags */
    DMA1_CLR = 0xF0;  /* clear all Ch2 flags */

    /* Configure DMA1 Ch2: FPGA (0x60020000) → RAM buffer */
    DMA1_C2DTCNT = sample_count * stride;
    DMA1_C2MADDR = (uint32_t)dest_buf;
    DMA1_C2PADDR = 0x60020000;  /* FPGA data register via FSMC */

    /* Enable DMA1 Ch2 interrupt (IRQ 12) */
    NVIC_EnableIRQ(DMA1_Channel2_IRQn);
    NVIC_SetPriority(DMA1_Channel2_IRQn, 3);

    /* Start transfer:
     * 0x4543 = CHEN | FTFIE | P-to-M | 16-bit periph | 16-bit mem | high prio
     * Verify bit layout against AT32F403A DMA_CxCTRL register */
    DMA1_C2CTL = 0x4543;
}
```

### 4. Implement DMA ISR with Double-Buffering

```c
void DMA1_Channel2_IRQHandler(void)
{
    if (DMA1_STS & (1 << 5)) {  /* FTIF2: Ch2 full transfer complete */
        DMA1_CLR = (1 << 5);    /* clear flag */

        if (dma_phase < 2) {
            dma_phase++;
            /* Reconfigure for next buffer half */
            DMA1_CLR = 0xF0;
            DMA1_C2DTCNT = next_transfer_count;
            DMA1_C2MADDR = (uint32_t)second_buffer;
            DMA1_C2PADDR = 0x60020000;
            DMA1_C2CTL = 0x4503;
        } else {
            /* All phases complete — signal acquisition task */
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(acq_done_queue, &msg,
                              &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}
```

### 5. Bus Arbitration with LCD

Since FPGA and LCD share the FSMC bus, acquisition and display
writes cannot overlap. The stock firmware handles this implicitly:
the acquisition DMA runs during the vertical blanking / idle period,
and the display task doesn't write pixels while DMA is active.

Options for the custom firmware:
- **Mutex:** Take a `fsmc_mutex` before LCD writes, release after.
  The DMA ISR signals a semaphore; the acquisition task holds the
  mutex during the entire DMA sequence.
- **Task priority:** Give the acquisition task higher priority than
  the display task, so DMA setup preempts LCD rendering.
- **Simplest:** Disable LCD writes during the DMA transfer (a few
  hundred microseconds at 60MHz for 1024 bytes).

## Verification Plan

Before committing the full DMA implementation:

1. **Smoke test:** In the existing firmware, add a single read of
   `*(volatile uint16_t *)0x60020000` after the FPGA init sequence
   and display the value. If it's not 0xFFFF, the FPGA is responding
   on the FSMC bus.

2. **Polled read:** Try reading 512 consecutive values from
   `0x60020000` in a loop (no DMA) after sending the appropriate
   acquisition command via USART2. Check if values vary.

3. **DMA transfer:** Only after polled reads work, switch to DMA.

4. **Unicorn validation (optional):** Load the stock firmware binary
   into Unicorn, execute `init_dma_acquisition` (0x0803FEE0), and
   trace all MMIO writes. Compare against the custom firmware's
   register write sequence to catch any divergence.

## Evidence Chain

This analysis was produced by:
1. Ghidra headless decompilation of `stock_v120` (305 functions)
2. ripcord warehouse: peripheral_xrefs table (1669 access records)
3. ripcord agent swarm: 4 propagation rounds, 200 LLM analysis tasks
4. xref analysis: all 71 accesses to 0x60000000+ range target exactly
   `0x6001FFFE` (write, 4 functions) and `0x60020000` (in decompiled C)
5. DMA register identification: `0x40020028` (DMA1_C2PADDR) is set to
   `0x60020000` in both `init_dma_acquisition` and the DMA ISR

No function in the stock firmware reads from `0x40003C00` (SPI3) for
waveform data. The SPI peripheral at `0x40003800` (SPI2) is used
exclusively for flash memory commands (0x03, 0x02, 0x05, 0x20).
