# Roadmap

## Status sections superseded (2026-08-20)

This file used to open with three status sections ("What Works on Hardware",
"Implemented and Tested (Awaiting Real Data)", "In Progress"). They were prose
copies of a table that now exists properly, and they had drifted — "Implemented
and Tested" described S0 code with no call sites as if it were done, which is
this project's characteristic failure written into its own roadmap.

Status now lives in exactly two places:

- **Where each feature stands:** [`README.md` § Feature maturity](../README.md#feature-maturity)
- **What promotes it, and what's missing entirely:** [`docs/specs/`](specs/)

The sections below survive because they are *design*, not status — the W25Q
region layer, the user-calibration mode, memory headroom, and the stretch
tracks. `docs/dev_plan_2026-08-13.md` §C1 links here for the region layer.

## Future Plans

Roughly ordered by priority. Not committed to timelines.

### Near-term (after data acquisition works)
- **Real oscilloscope display** — live waveforms from FPGA ADC
- **Multimeter with real readings** — FPGA USART meter data → display
- **Signal generator output** — DAC waveform generation on real hardware
- **ADC calibration pipeline** — per-channel calibration from SPI flash data

### Medium-term
- **USB streaming to PC** — CDC serial port, raw samples to sigrok/PulseView
- **CSV/data export** — waveform data to SPI flash, export via USB mass storage
- **Segmented memory** — trigger-only capture, skip dead time
- **Waveform reference library** — known-good overlays on SPI flash

### Memory and resource headroom

Enabling work for everything below — modules especially. Baseline measured
2026-08-13 on `make guest`, gcc 13.2.1.

**The shape of the problem: flash is abundant, RAM is not.**

| Resource | Used | Free |
|---|---:|---:|
| Internal flash (996 KB app slot) | 496,136 B (48.6%) | ~512 KB |
| — of which actual code (`.text`) | 107,020 B | |
| — of which `.rodata` | 388,720 B | |
| SRAM (224 KB) | see below | **24,272 B** after the waterfall move |
| W25Q128 SPI flash (SPI2, streamed) | UI assets / system files | 16 MB, not memory-mapped |

**⚠ The `.spim` region in `at32f403a_guest.ld` is dead and always will be — do
not plan against it.** `0x08400000` is `FLASH_SPIM_START_ADDR`, the AT32's XIP
window for memory-mapping an external SPI NOR chip into code space. It is an
MCU feature, not spare on-chip memory (the part has 1 MB internal flash), and
it cannot reach our Winbond: the W25Q128 sits on **SPI2 (PB12 CS / PB13 SCK /
PB14 MISO / PB15 MOSI)**, an ordinary SPI peripheral with no XIP path. The
SPIM peripheral has exactly two fixed pin maps, and every pin in both is
already committed on this board:

| SPIM signal | `GMUX_1000` | `GMUX_1001` | Conflict on 2C53T |
|---|---|---|---|
| SCK | PB1 | PB1 | battery sense (ADC1 ch9) |
| CS | PA8 | PA8 | button matrix |
| IO0/IO1 | PA11/PA12 | PB10/PB11 | **USB D−/D+** / analog mux + FPGA active |
| IO2/SIO3 | PB7/PB6 | PB7/PB6 | PRM button / FPGA SPI3 CS |

Enabling SPIM would cost USB — the entire closed-case update channel. The
`.spim` output section is inherited Artery boilerplate; it is 0 bytes and
should stay that way.

**What this does not rule out:** the W25Q is still 16 MB of usable storage,
just *streamed over SPI2* rather than executed in place. Anything that can be
read into RAM on demand — bitstreams, tables, module assets — can live there.
Anything needing to be executed directly from external flash cannot.

Only ~107 KB of the flash is logic. Two data blobs dominate `.rodata`: the FPGA
bitstream (115,638 B) and the CMSIS-DSP FFT twiddle/bit-reversal tables
(~101,700 B). RAM is where features actually compete — three buffers held 90%
of it before the move below.

- **✅ Waterfall history → shared pool** *(done, `935d020`)* — 20,480 B of `.bss`
  became a sub-tenant of the FFT pool region. Free SRAM 3,792 → 24,272 B. Also
  surfaced that `SHMEM_NEED_FFT` (90112) understates the radix-2 layout, which
  really needs 98304; now pinned by a `_Static_assert` in `fft.c`.
- **FPGA bitstream → external SPI flash** — frees ~115 KB of internal flash, the
  single largest item. It is already streamed to the FPGA byte-by-byte, so there
  is no reason it must be compiled in. Adds a W25Q dependency to the boot path,
  so this should wait until cold-boot config entry is fully settled; a failed
  read must fall back cleanly rather than leaving the FPGA unconfigured.
- **Modules as streamed assets on the W25Q** — the practical version of
  "loadable modules" on this hardware. JSON procedure files and lookup tables
  read over SPI2 into the pool on demand, which needs no XIP and no new pins.
  Executable module *code* is out of reach (see the SPIM note above); design
  modules as data + a firmware-side interpreter, not as loadable binaries.
  **Prerequisite: the region layer below.**

### W25Q region layer (prerequisite for anything that writes)

Small, and it must exist *before* the first automated writer does. Not FatFs —
a region table plus a bounds-checked allocator, on the order of a couple of
hundred lines.

**Flash wear is not the reason.** W25Q128JV is rated 100,000 P/E cycles per
4 KB sector with 20-year retention; reads cause no wear at all (read-disturb is
a NAND concern, not NOR). Only erases count, and the granularity is 4 KB, so
changing one byte costs a full sector erase:

| Erases/day, one sector | Sector lifetime |
|---|---|
| 1 | ~274 years |
| 10 | ~27 years |
| 100 | ~2.7 years |
| every 10 s | **~12 days** |

So the only rule that matters is about *frequency, not volume*: **write on
explicit user action or at shutdown — never on a timer, never per-keypress.**
Module installs a few times a month land around 1,600 years on a single sector
with no wear levelling at all; module *loads* are reads and cost nothing. Two
cheap habits make it moot anyway: skip no-op writes (compare before erasing),
and append fixed-size records into a sector until it fills rather than
rewriting it (16 × 256 B records = 16× fewer erases).

**The real hazard is a stray erase, not endurance.** Stock's UI assets and its
whole `3:` volume live on that chip, and we cannot regenerate any of it.

> ⚠ **Correction (2026-08-14).** Earlier drafts of this section named
> `3:/System file/cal_ch1.bin` / `cal_ch2.bin` as the factory calibration files.
> **Those filenames were invented.** They appear nowhere in the 16 MB dump
> `archive/w25q128_dump_2026_05_30.bin` and nowhere in any stock APP binary. The
> only cal-shaped path stock actually references is
> `3:/System file/9999.bin`, and in every dump we have that file is an empty
> placeholder (FAT entry `9999    BIN`, attr `0x20`, cluster 0, size 0).
> `analysis_v120/meter_w25q_calibration_boundary_2026_06_06.md` flagged the
> invented names on 2026-06-06 and the warning was not acted on.
>
> What *is* established: stock restores a calibration-like table into RAM
> `0x20000358..0x2000044A` from a saved config, checks a sentinel at `ms[0x34E]`,
> and when that sentinel is erased (`0xFFFF`) or zero it falls back to
> **hardcoded defaults compiled into the firmware image** at
> `0x080261BE..0x08026506`. Separately,
> `analysis_v120/w25q128_flash_map_2026-06-13.md` swept the whole chip and found
> only UI JPEGs, screenshots, FAT metadata and the empty `9999.BIN` — no cal —
> and narrowed the saved-config source to **MCU internal flash `0x08006000`**,
> which our own app overwrites on both bench units. Whether per-device factory
> calibration exists at all, and where, is **under investigation** — see
> `reverse_engineering/analysis_v120/factory_cal_truth_2026-08-14.md`.
> The inverse error is just as easy to make: our W25Q evidence is two
> byte-identical archived dumps plus one live read of bench unit #2, all of
> which had already been reflashed by us. That is not proof about a pristine
> unit, and the June boundary analysis deliberately left cross-unit comparison
> open.

The read-only enforcement below is unchanged and still correct — it protects
stock's volumes by address, whatever they turn out to contain. And
today there is nothing between a caller and that data: `flash_fs_read()` and
`flash_fs_write_atomic()` are still stubs with FatFs TODOs, so only the raw
primitives (`raw_read_bytes`, `raw_write_block`, `raw_sector_erase`) are live,
addressed by absolute sector. What the layer needs:

- a region map with explicit **read-only** regions (everything stock owns —
  both FAT volumes, assets included) that the write path refuses by address,
  not by convention;
- bounds checks on every write/erase against the target region;
- a designated scratch/user region for everything we generate;
- no-op-write elision, and append-style records for anything frequently updated.

*(Current wear: zero. Nothing writes to the W25Q at runtime — `config_save()`
targets a static RAM buffer and has no callers; the only live write paths are
manual `usb_debug.c` shell commands that read back and verify.)*

### User calibration mode (unlocked by the region layer)

Worth filing now because it may be the pragmatic answer to a thread that has
resisted reverse engineering. **Write to a separate user-cal region — never
into anything stock owns.** Whatever per-device provenance the instrument
carries, we have not located it and have not decoded it (see the correction
above); overwriting a region we do not understand trades a possibly-known-good
reference for a guess.

The standard instrument pattern applies: everything stock owns stays read-only,
user cal is an overlay stored in our own format, and a blank or corrupt user
region falls back to whatever the stock-side default is. That gives
revert-to-factory for free and makes a botched calibration recoverable.

Why it may matter more than a convenience feature: the meter's low-Ω and DCV
accuracy currently depend on per-device factory coefficients we have never
recovered (the `0.0304` bench factor was one unit's, and PR #13 correctly fails
those bands closed rather than shipping it). A guided "measure this known
reference, we will store the coefficient" routine sidesteps decoding the stock
format entirely — the user supplies the ground truth a reference DMM already
has. Wear is irrelevant here: a calibration is a handful of writes in the life
of a device.

Gated on the region layer, and on scope work per the FPGA-first stance — not a
now item, but the design constraint (separate region, never the factory sector)
should be settled before anyone starts.
- **FFT twiddle tables → W25Q** — ~101 KB of internal flash, read-only. Would
  have to be *loaded into the pool* at `fft_init()` rather than XIP'd, so it
  trades 101 KB of flash for pool space and a boot-time read. Only worth it if
  internal flash ever gets tight, which at 48.6% used it is not.
- **Streaming screenshot → shrink the pool** — screenshot is the sole reason the
  pool is 150 KB (`SHMEM_NEED_SCREENSHOT` = 153,600). If it captured in row
  bands straight to SPI flash or USB, the pool could drop to the FFT need and
  free ~44 KB outright. PR #13 shrinks it to 96 KB by simply disabling
  screenshot, which is the same trade taken bluntly.

**Budgeting rule for new features:** anything needing large *transient* memory
should claim the pool (`SHMEM_OWNER_MODULE` already exists for this). Anything
needing large *persistent* memory is competing for ~24 KB and needs a design
conversation first.

### Automotive suite
- **Relative compression test** — cranking current analysis, per-cylinder bar chart
- **Alternator ripple test** — FFT of battery voltage, diode fault detection
- **Parasitic draw (fuse voltage drop)** — millivolt drop across blade fuses with built-in lookup table
- **Parasitic draw (current clamp)** — roll mode with event detection for module wake-ups
- **Injector pulse width** — per-cylinder duty cycle comparison
- **Ignition coil analysis** — dwell time, spark duration
- **Battery cranking analysis** — voltage sag curve, CCA estimate

### Specialized applications
- **Audio analysis** — THD+N, speaker impedance curve
- **Ham radio** — harmonic analysis, SWR measurement
- **HVAC/Solar** — motor start capacitor test, inverter THD
- **Industrial** — motor current signature analysis (MCSA), 4-20mA loop testing

### Hardware mods (Phase 3)
- **ESP32 WiFi co-processor** — $4 solder-in mod for phone display, remote control, data logging, OTA updates. Design doc at `docs/esp32_coprocessor.md`.

### Stretch track: custom Gowin bitstreams ("gateware for the FPGA")

Unblocked by the 2026-08-13 breakthrough. Now that our firmware can upload arbitrary
bitstreams to the Gowin GW1N-UV2 from a cold boot, we can write **our own FPGA designs**
— not just replay stock's — turning this from an open-firmware clone of the stock scope
into a scope that does things the stock one can't.

**Why it's realistic now (the prerequisites are already in hand):**
- **Upload path solved** — the bit-bang SSPI loader configures the FPGA every boot.
- **Open synthesis toolchain for this exact part** — `gw1n2-apicula` (Yosys → nextpnr →
  Apicula) targets the GW1N-2 family = our GW1N-UV2. Same toolchain we used to sim the
  netlist.
- **The stock design is reverse-engineered** — the netlist trace (`gw1n2-apicula`
  `tools/m_*.py`, `scope_unpacked.v`) mapped the pins, the ADC data bus into the BRAMs,
  the capture counter, the SPI control register, and the readback mux. Not a black box.

**What it could unlock:**
- **Hardware decimation / configurable sample rate** — the elegant fix for the timebase
  problem (custom fabric samples at any rate, instead of coaxing stock's opaque commands).
- **Our own clean MCU↔FPGA protocol** — a real register map + streaming interface we
  control, replacing stock's cryptic `0x04/0x05`.
- **Logic-analyzer mode, in-fabric edge/pulse triggers, hardware protocol decoders** —
  things stock's fabric simply doesn't do.

**The hard parts (be honest):** replicating the **ADC interface + PLL clocking** (the
250 MS/s DDR capture) is the real challenge — Apicula only partially decodes the rPLL —
and Apicula's completeness for the specific primitives (BSRAM, rPLL, DDR I/O) is a
wildcard.

**Suggested incremental path:** (1) synthesize a *trivial* design (a register the MCU
reads back over SPI) to prove "our bitstream runs and talks to us"; (2) **fork the stock
netlist** we already unpacked — keep its working ADC/PLL front end, tweak buffer
depth/trigger — rather than reinvent sampling; (3) then get ambitious.

**⚠ Hard safety rule:** experiment freely with SRAM config (a bad bitstream just means no
scope until reboot — zero brick risk), but **never write the FPGA's NV flash** — it holds
the meter design and is the only copy.

## Reverse Engineering Status

~98% of the stock firmware is understood:
- 309 functions identified and named (138 high, 182 medium, 42 low confidence)
- All ~40 FPGA commands mapped (0x00-0x2C)
- ADC data format cracked (interleaved CH1/CH2, 8-bit unsigned, offset -28.0)
- Complete hardware pinout documented
- 53-step boot sequence traced
- 8 FreeRTOS tasks and 7 queues mapped
- Remaining: PLL startup assembly, 42 low-confidence function names
