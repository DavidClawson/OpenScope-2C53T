# Makefile for FNIRSI 2C53T Custom Firmware
# Target: Artery AT32F403A (ARM Cortex-M4F @ 240MHz)

######################################################################
# Toolchain
######################################################################
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
CP      = $(PREFIX)objcopy
SZ      = $(PREFIX)size
OBJDUMP = $(PREFIX)objdump
NANO_SPECS := $(shell f="$(shell $(CC) -print-file-name=nano.specs 2>/dev/null)"; test "$$f" != "nano.specs" -a -f "$$f" && printf '%s' '-specs=nano.specs')
HOST_CC ?= gcc
NANO_SPECS := $(shell f="$(shell $(CC) -print-file-name=nano.specs 2>/dev/null)"; test "$$f" != "nano.specs" -a -f "$$f" && printf '%s' '-specs=nano.specs')

######################################################################
# Project paths
######################################################################
BUILD_DIR  = build
AT32_LIB   = at32f403a_lib/libraries
AT32_DRV   = $(AT32_LIB)/drivers
AT32_CMSIS = $(AT32_LIB)/cmsis/cm4
FREERTOS   = FreeRTOS

######################################################################
# Source files
######################################################################

# Feature flags (FFT enabled by default)
FEATURES ?= FFT

# Application
C_SOURCES  = src/main.c
C_SOURCES += src/drivers/lcd.c
C_SOURCES += src/ui/status_bar.c
C_SOURCES += src/ui/scope_ui.c
C_SOURCES += src/ui/meter_ui.c
C_SOURCES += src/ui/siggen_ui.c
C_SOURCES += src/ui/settings_ui.c
C_SOURCES += src/ui/component_ui.c
C_SOURCES += src/ui/resistor_calc_ui.c
C_SOURCES += src/ui/bode_ui.c
C_SOURCES += src/ui/fuse_ui.c
C_SOURCES += src/ui/scope_state.c
C_SOURCES += src/tasks/input_handler.c
C_SOURCES += src/tasks/measurement.c
C_SOURCES += src/drivers/flash_fs.c
C_SOURCES += src/drivers/esp_comm.c
C_SOURCES += src/drivers/watchdog.c
C_SOURCES += src/drivers/button_scan.c
C_SOURCES += src/drivers/dfu_boot.c
C_SOURCES += src/drivers/battery.c
C_SOURCES += src/drivers/fpga.c
C_SOURCES += src/drivers/fpga_scanner.c
C_SOURCES += src/drivers/dac_output.c
C_SOURCES += src/drivers/scope_trigger.c
C_SOURCES += src/drivers/meter_data.c
C_SOURCES += src/drivers/usb_debug.c
C_SOURCES += src/drivers/rtt.c
C_SOURCES += src/drivers/fault.c
C_SOURCES += src/dsp/signal_gen.c
C_SOURCES += src/dsp/shared_mem.c
C_SOURCES += src/ui/persistence.c
C_SOURCES += src/ui/theme.c
C_SOURCES += src/ui/xy_mode.c
C_SOURCES += src/ui/roll_mode.c
C_SOURCES += src/ui/trend_plot.c
C_SOURCES += src/ui/meter_voltage_wave.c
C_SOURCES += src/dsp/math_channel.c
C_SOURCES += src/dsp/bode.c
C_SOURCES += src/tasks/component_test.c
C_SOURCES += src/tasks/mask_test.c
C_SOURCES += src/util/config.c
C_SOURCES += src/util/screenshot.c
C_SOURCES += src/decode/decode_uart.c
C_SOURCES += src/decode/decode_i2c.c
C_SOURCES += src/decode/decode_spi.c
C_SOURCES += src/decode/decode_can.c
C_SOURCES += src/decode/decode_kline.c
C_SOURCES += src/modules/compression_test.c
C_SOURCES += src/modules/alternator_test.c
C_SOURCES += src/fonts/font.c
C_SOURCES += src/fonts/font_small.c
C_SOURCES += src/fonts/font_medium.c
C_SOURCES += src/fonts/font_large.c
C_SOURCES += src/fonts/font_xlarge.c

# FFT / DSP module
ifneq ($(findstring FFT,$(FEATURES)),)
  C_SOURCES += src/dsp/fft.c
  C_SOURCES += src/dsp/fft_test_signals.c
  FFT_DEFS = -DFEATURE_FFT
  # CMSIS-DSP (optional optimization — auto-detected)
  CMSIS_DSP = CMSIS-DSP
  ifneq ($(wildcard $(CMSIS_DSP)/Include/arm_math.h),)
    C_SOURCES += $(CMSIS_DSP)/Source/TransformFunctions/arm_rfft_fast_f32.c
    C_SOURCES += $(CMSIS_DSP)/Source/TransformFunctions/arm_rfft_fast_init_f32.c
    C_SOURCES += $(CMSIS_DSP)/Source/TransformFunctions/arm_cfft_f32.c
    C_SOURCES += $(CMSIS_DSP)/Source/TransformFunctions/arm_cfft_init_f32.c
    C_SOURCES += $(CMSIS_DSP)/Source/CommonTables/arm_common_tables.c
    C_SOURCES += $(CMSIS_DSP)/Source/CommonTables/arm_const_structs.c
    C_SOURCES += $(CMSIS_DSP)/Source/TransformFunctions/arm_cfft_radix8_f32.c
    C_SOURCES += $(CMSIS_DSP)/Source/ComplexMathFunctions/arm_cmplx_mag_f32.c
    C_SOURCES += $(CMSIS_DSP)/Source/BasicMathFunctions/arm_scale_f32.c
    FFT_INCLUDES = -I$(CMSIS_DSP)/Include
    FFT_DEFS += -DUSE_CMSIS_DSP -DARM_MATH_CM4
  endif
endif

# AT32 CMSIS + clock config
C_SOURCES += $(AT32_CMSIS)/device_support/system_at32f403a_407.c
C_SOURCES += src/at32f403a_407_clock.c

# AT32 Peripheral Library (only what we need)
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_crm.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_gpio.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_flash.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_misc.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_xmc.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_wdt.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_adc.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_dac.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_tmr.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_usb.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_acc.c

# USB Device Middleware (CDC virtual COM port)
AT32_USBD = at32f403a_lib/middlewares/usbd_drivers
AT32_CDC  = at32f403a_lib/middlewares/usbd_class/cdc
C_SOURCES += $(AT32_USBD)/src/usbd_core.c
C_SOURCES += $(AT32_USBD)/src/usbd_int.c
C_SOURCES += $(AT32_USBD)/src/usbd_sdr.c
C_SOURCES += $(AT32_CDC)/cdc_class.c
C_SOURCES += $(AT32_CDC)/cdc_desc.c

# FreeRTOS kernel
C_SOURCES += $(FREERTOS)/tasks.c
C_SOURCES += $(FREERTOS)/queue.c
C_SOURCES += $(FREERTOS)/list.c
C_SOURCES += $(FREERTOS)/timers.c
C_SOURCES += $(FREERTOS)/portable/GCC/ARM_CM4F/port.c
C_SOURCES += $(FREERTOS)/portable/MemMang/heap_4.c

# Startup assembly (AT32F403A - GCC variant)
ASM_SOURCES = $(AT32_CMSIS)/device_support/startup/gcc/startup_at32f403a_407.s

# CMSIS-DSP assembly (bit-reversal)
ifneq ($(wildcard CMSIS-DSP/Include/arm_math.h),)
  ASM_SOURCES += CMSIS-DSP/Source/TransformFunctions/arm_bitreversal2.S
endif

######################################################################
# Include paths
######################################################################
C_INCLUDES  = -Iinclude
C_INCLUDES += -Isrc/drivers
C_INCLUDES += -Isrc/dsp
C_INCLUDES += -Isrc/ui
C_INCLUDES += -Isrc/tasks
C_INCLUDES += -Isrc/util
C_INCLUDES += -Isrc/decode
C_INCLUDES += -Isrc/modules
C_INCLUDES += -Isrc/fonts
C_INCLUDES += -Isrc
C_INCLUDES += -I$(AT32_DRV)/inc
C_INCLUDES += -I$(AT32_CMSIS)/device_support
C_INCLUDES += -I$(AT32_CMSIS)/core_support
C_INCLUDES += -I$(FREERTOS)/include
C_INCLUDES += -I$(FREERTOS)/portable/GCC/ARM_CM4F
C_INCLUDES += -I$(AT32_USBD)/inc
C_INCLUDES += -I$(AT32_CDC)

######################################################################
# Compiler flags
######################################################################

# MCU flags: Cortex-M4 with hardware FPU
CPU = -mcpu=cortex-m4
FPU = -mfpu=fpv4-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# C defines
C_DEFS  = -DAT32F403AVGT7
C_DEFS += -DSCOPE_DEBUG_OVERLAY
C_DEFS += $(FFT_DEFS)
C_INCLUDES += $(FFT_INCLUDES)

# Compiler flags
CFLAGS  = $(MCU) $(C_DEFS) $(C_INCLUDES)
CFLAGS += -Wall -Wextra -Wno-unused-parameter
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -std=gnu11
CFLAGS += -Os -g

# Assembler flags
ASFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES)

######################################################################
# Linker flags
######################################################################
LDSCRIPT = ld/at32f403a_app.ld

LDFLAGS  = $(MCU)
LDFLAGS += $(NANO_SPECS)
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/firmware.map,--cref
LDFLAGS += -lc -lm -lnosys

######################################################################
# Build targets
######################################################################
TARGET = firmware

all: $(BUILD_DIR)/$(TARGET).bin

# List of C objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

# List of ASM objects (AT32 uses .s, CMSIS-DSP uses .S)
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
OBJECTS := $(OBJECTS:.S=.o)
vpath %.s $(sort $(dir $(ASM_SOURCES)))
vpath %.S $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.S Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile $(LDSCRIPT)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/option_bytes48.bin
	$(CP) -O binary --gap-fill 0xFF $< $@
	@echo "Built: $@ ($$(wc -c < $@ | tr -d ' ') bytes)"

# One-shot option-byte blob for first-time DFU setup.
# Writes the AT32F403A User System Data region at 0x1FFFF800 so EOPB0 = 0xFE,
# enabling 224KB SRAM mode (default is 96KB, which is too small for our heap).
#
# The file MUST be exactly 48 bytes — the AT32 ROM DFU descriptor reports the
# USD region as "01*048 e" (one 48-byte sector) and rejects any shorter write
# with "address out of range". The "48" in the filename is literally the byte
# count, not a version suffix.
#
# Layout (byte + 1's complement pairs, per AT32F403A RM + usd_type struct):
#   0x00 FAP      = A5 / 5A   (unlocked — required to write other bytes)
#   0x02 SSB      = FF / 00   (watchdog/standby/stop defaults)
#   0x04 DATA0    = FF / 00
#   0x06 DATA1    = FF / 00
#   0x08 EPP0     = FF / 00   (no write-protected sectors)
#   0x0A EPP1     = FF / 00
#   0x0C EPP2     = FF / 00
#   0x0E EPP3     = FF / 00
#   0x10 EOPB0    = FE / 01   ← 224KB SRAM mode (the only non-default value)
#   0x12 reserved = FF / 00
#   0x14 DATA2..DATA7 = FF / 00  (×6 pairs)
#   0x20 ext_flash_key[8] = 16 × FF  (external flash encryption keys, unused)
#
# Flashed via: dfu-util -a 1 -d 2e3c:df11 -s 0x1FFFF800 -D build/option_bytes48.bin
$(BUILD_DIR)/option_bytes48.bin: Makefile | $(BUILD_DIR)
	@printf '\245\132\377\000\377\000\377\000\377\000\377\000\377\000\377\000\376\001\377\000\377\000\377\000\377\000\377\000\377\000\377\000\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377' > $@
	@echo "Built: $@ ($$(wc -c < $@ | tr -d ' ') bytes) — EOPB0=0xFE, 224KB SRAM"

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(CP) -O ihex $< $@

$(BUILD_DIR):
	mkdir -p $@

######################################################################
# Utility targets
######################################################################
clean:
	rm -rf $(BUILD_DIR)

disasm: $(BUILD_DIR)/$(TARGET).elf
	$(OBJDUMP) -d -S $< > $(BUILD_DIR)/$(TARGET).dis

size: $(BUILD_DIR)/$(TARGET).elf
	$(SZ) --format=berkeley $<

# Build for emulator (skips HAL clock init that hangs without real hardware).
# main.c provides stub overrides for SystemInit/system_clock_config when
# EMULATOR_BUILD is defined. --allow-multiple-definition lets main.o's
# versions win over the HAL's (main.o is linked first).
emu: CFLAGS += -DEMULATOR_BUILD
emu: LDFLAGS += -Wl,--allow-multiple-definition
emu: LDSCRIPT = ld/at32f403a.ld
emu: clean all

# Build a GUEST image that runs at 0x08007000 under the FNIRSI STOCK bootloader
# (unit #2), flashed via the stock IAP drive (mcopy). main.c adjusts VTOR to
# 0x7000 and skips the watchdog when GUEST_BUILD is defined. Used to run the
# SPI3/FPGA debug shell on a known-good FPGA.
guest: CFLAGS += -DGUEST_BUILD
guest: LDSCRIPT = ld/at32f403a_guest.ld
guest: clean all

# Warm-handoff experiment build (guest image), reworked 2026-08-12 after
# Stlkv's live-trace result (issue #18). Comes up READ-ONLY against an FPGA
# already scope-configured by stock: skips the SSPI config sequence, arms the
# trigger DAC (DAC1 mid-scale — the reset zeroes it and captures read flat
# zeros until it is restored), and runs a PC0-gated acquisition task speaking
# the real 0x04/0x05 per-channel protocol, feeding the scope UI. Success is
# on the LCD: the synthetic demo trace disappears when the first real frame
# lands (no USB needed — CDC is dead on the bench unit). Pairs
# FPGA_USART_SILENT_SCOPE so USART2 stays electrically dark (UEN clear).
# See fpga.c FPGA_WARM_HANDOFF_TEST and docs/fpga_warm_handoff_test.md.
guest-warmtest: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=1
guest-warmtest: LDSCRIPT = ld/at32f403a_guest.ld
guest-warmtest: clean all

# Warm handoff + CH2 trigger bring-up (bench plan item 5, 2026-08-13). Layers the
# TMR13 CH1 PWM-DAC reference (PA6) onto the warm-handoff scope build so a live
# CH2 trace can be validated — CH2 was predicted dead because our firmware never
# programmed TMR13 (ripcord contract 38: CH2's comparator ref is TMR13_C1DT, same
# cal formula as DAC1). Stock TMR13 config decoded from master_init: PSC=0,
# ARR=4094, CH1 PWM, C1EN|C1P, CEN. ⚠ Drives PA6 as AF PWM — confirm on the bench
# that PA6 is the CH2 reference (it was "Unknown" in HARDWARE_PINOUT). Watch for a
# CH2 trace appearing the way CH1 did after the DAC1 fix. See scope_trigger.c.
guest-warmtest-ch2: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CH2_TRIGGER=1
guest-warmtest-ch2: LDSCRIPT = ld/at32f403a_guest.ld
guest-warmtest-ch2: clean all

# USART-silent scope-boot experiment (guest image). Holds USART2 UEN CLEAR through
# the whole boot, skips Step 3b + Step 8 USART traffic, creates no dvom/meter/acq
# auto-tasks — so the SPI3 wire stays quiet and `fpga reinit`/`spi3 xfer` run
# unperturbed. Tests the stock-bringup-diff hypothesis that live USART2 keeps the
# FPGA's NV design owning the SSPI pins. See fpga.c FPGA_USART_SILENT_SCOPE and
# docs/fpga_stock_bringup_diff_plan.md.
guest-usart-silent: CFLAGS += -DGUEST_BUILD -DFPGA_USART_SILENT_SCOPE=1
guest-usart-silent: LDSCRIPT = ld/at32f403a_guest.ld
guest-usart-silent: clean all

# External-master bring-up build (guest image). fpga_init() brings up the
# peripherals then calls fpga_bus_release() instead of its own SSPI upload:
# tri-states PB3/PB5/PB6 (Hi-Z), stages PC6/PB11 HIGH, keeps PC9 power-hold,
# creates no auto-tasks. Pairs FPGA_USART_SILENT_SCOPE to keep the wire fully
# quiet, so the FPGA's config port is pristine for an external ESP32 (SSPI pads)
# or FT232H (JTAG TAP pads, issue #18). UNTESTED. See fpga.c FPGA_BUS_RELEASED_BOOT
# and tools/esp32_sspi_bringup/README.md.
guest-bringup: CFLAGS += -DGUEST_BUILD -DFPGA_BUS_RELEASED_BOOT=1 -DFPGA_USART_SILENT_SCOPE=1
guest-bringup: LDSCRIPT = ld/at32f403a_guest.ld
guest-bringup: clean all

# Flash app via USB HID bootloader (bootloader must be running)
flash: $(BUILD_DIR)/$(TARGET).bin
	python3 ../scripts/flash_preflight.py hid-app --image $< --address 0x08004000
	uv run ../scripts/hid_flash.py $< --preserve-blank-blocks --preserve-blank-blocks-range 0x08006000:0x08007000

# Flash app via DFU (device must be in ROM DFU mode — BOOT0 + reset)
flash-dfu: $(BUILD_DIR)/$(TARGET).bin
	python3 ../scripts/flash_preflight.py rom-dfu-app --image $<
	python3 ../scripts/at32_dfuse_write.py 0x08004000 $< --preserve-blank-pages-range 0x08006000:0x08007000 --leave 0x08004000

# Build and flash the bootloader (one-time, via DFU)
bootloader:
	$(MAKE) -C bootloader

flash-bootloader:
	$(MAKE) -C bootloader
	python3 ../scripts/flash_preflight.py rom-dfu-bootloader --bootloader bootloader/build/bootloader.bin
	dfu-util -a 0 -d 2e3c:df11 -s 0x08000000 -D bootloader/build/bootloader.bin

# Flash both bootloader + app in one go (via ROM DFU, for initial setup)
flash-all: $(BUILD_DIR)/$(TARGET).bin bootloader
	python3 ../scripts/flash_preflight.py rom-dfu-all --bootloader bootloader/build/bootloader.bin --image $(BUILD_DIR)/$(TARGET).bin --option-bytes $(BUILD_DIR)/option_bytes48.bin
	dfu-util -a 0 -d 2e3c:df11 -s 0x08000000 -D bootloader/build/bootloader.bin
	python3 ../scripts/at32_dfuse_write.py 0x08004000 $(BUILD_DIR)/$(TARGET).bin --preserve-blank-pages-range 0x08006000:0x08007000 --leave 0x08004000

# Run in Renode emulator (display only, no input)
renode: emu
	cd ../emulator/renode && /Applications/Renode.app/Contents/MacOS/renode --disable-gui run_openscope.resc

# Run in Renode with interactive input (use with lcd_viewer in another terminal)
renode-interactive: emu
	@echo "Start the LCD viewer in another terminal: cd emulator && ./lcd_viewer"
	cd ../emulator/renode && /Applications/Renode.app/Contents/MacOS/renode --disable-gui run_interactive.resc

# Run in Renode for quick smoke test (5 seconds, then quit)
renode-test: emu
	cd ../emulator/renode && /Applications/Renode.app/Contents/MacOS/renode --disable-gui --console -e \
		"include @run_openscope.resc; sleep 5; pause; cpu PC; quit"

# Soak test — random button fuzzing with fault monitoring (default 2 minutes)
SOAK_DURATION ?= 120
SOAK_SEED ?= 42
soak-test: emu
	python3 ../scripts/soak_test.py --no-build --duration $(SOAK_DURATION) --seed $(SOAK_SEED)

# Quick soak — 30-second smoke test with fuzzing
soak-quick: emu
	python3 ../scripts/soak_test.py --no-build --duration 30 --seed $(SOAK_SEED) --verbose

$(BUILD_DIR)/test_meter_data: tests/test_meter_data.c src/drivers/meter_data.c src/drivers/meter_data.h | $(BUILD_DIR)
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Isrc/drivers $< src/drivers/meter_data.c -lm -o $@

test-meter-data: $(BUILD_DIR)/test_meter_data
	$<

$(BUILD_DIR)/test_meter_voltage_wave: tests/test_meter_voltage_wave.c src/ui/meter_voltage_wave.c src/ui/meter_voltage_wave.h | $(BUILD_DIR)
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Isrc/ui $< src/ui/meter_voltage_wave.c -lm -o $@

test-meter-voltage-wave: $(BUILD_DIR)/test_meter_voltage_wave
	$<

test-meter: test-meter-data test-meter-voltage-wave

.PHONY: all clean disasm size renode soak-test soak-quick flash flash-dfu bootloader flash-bootloader flash-all guest guest-warmtest guest-usart-silent emu test-meter-data test-meter-voltage-wave test-meter

# Experiment E (2026-07-27): park in fpga_init at the exact config-enable instant
# (prelude 05/12 sent, CS LOW, 0x15 not yet clocked) so SWD can dump peripheral
# state for a diff vs the stock spin image (/tmp/APP_2C53T_expE_spin_stock.bin).
# The device intentionally never reaches the scheduler — reflash to recover.
guest-spin: CFLAGS += -DGUEST_BUILD -DFPGA_SPIN_AT_CONFIG_ENABLE=1
guest-spin: LDSCRIPT = ld/at32f403a_guest.ld
guest-spin: clean all

# Experiment F (2026-07-28): stock-fidelity config build. Closes ALL five
# enumerable MCU-state differences the Exp E SWD diff found at the CONFIG_ENABLE
# instant, in one image: SPI3 BR back to stock's /2, PB4/MISO pulled up instead
# of floating, PC2 + PB12 driven push-pull HIGH, USART2 UEN held clear (via
# FPGA_USART_SILENT_SCOPE), and the SPI2 clock gate enabled. probe_edit is turned
# on so the 0x41 STATUS read still happens at /256 in its own CS frame — read it
# on the LCD overlay as ED:. Wall breaks => bisect the five. Wall holds => MCU-side
# state is excluded and the FT232H JTAG oracle is the only remaining test.
# See reverse_engineering/analysis_v120/expE_swd_state_diff_2026-07-28.md
guest-fidelity: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1
guest-fidelity: LDSCRIPT = ld/at32f403a_guest.ld
guest-fidelity: clean all

# Experiment R (2026-08-11): the pins a LEVEL diff could not see.
#
# Exp F's "all five enumerables closed" was scoped to its method. It compared
# stock-vs-ours output LEVELS (ODT), which cannot distinguish a pin stock drives
# LOW from a pin we leave FLOATING — both report 0. Re-diffing the same Exp E
# dumps on the CONFIG registers (CRL/CRH) finds ~20 differences, of which these
# are open: PC1, PA6, PC11, PD2, PD3, PD6, PD13.
#
#   guest-pc1        fidelity + PC1 driven push-pull LOW. The clean single-pin
#                    case: stock and ours have IDENTICAL ODT bit1, and differ
#                    only in the CRL nibble (0x1 driven-low vs 0x4 floating).
#                    PC1 is `Unknown` in HARDWARE_PINOUT.md and no build of ours
#                    has ever driven it. Stock holds PC2 HIGH + PC1 LOW at
#                    CONFIG_ENABLE (the ==3 arm of the selector at 0x0802C608);
#                    Exp F matched PC2, so this closes the pair.
#
#   guest-fidelity2  fidelity + PC1 + the rest of the uncovered set. Excludes
#                    the Exp C-refuted frontend bank, the button matrix (we need
#                    SAVE/POWER), SPI2 AF pins, and PD12 (our EXMC owns it).
#
# Read ED: on the LCD overlay as usual. If either breaks the wall, bisect.
guest-pc1: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_FIDELITY_DRIVE_PC1=1
guest-pc1: LDSCRIPT = ld/at32f403a_guest.ld
guest-pc1: clean all

guest-fidelity2: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_FIDELITY_DRIVE_PC1=1 -DFPGA_FIDELITY_DRIVE_UNCOVERED=1
guest-fidelity2: LDSCRIPT = ld/at32f403a_guest.ld
guest-fidelity2: clean all

# Experiment S (2026-08-11): Gowin SSPI RELOAD (0x3C) before the prelude.
#
# Gowin documents two ways to make a running part accept a new configuration:
# pulse RECONFIG_N, or power-cycle it. As of Exp R BOTH are closed to us --
# every GPIO form of a RECONFIG_N pulse is refuted (Exps G/O/Q, plus maksidze's
# measurement that QN48 pin 48 never pulses), and a genuine FPGA power cycle
# leaves STATUS at 00039020.
#
# RELOAD is the third form: the same request as a COMMAND, on a bus we have
# proven the part decodes (Exp J: four opcodes, four distinct replies).
#
# The send code and the reload_3c field have existed since June, but the only
# way to reach them was the debug shell -- which needs USB CDC (never enumerated
# on this unit) or RTT (impossible while RDP is set). There was never a make
# target, so this has NEVER been run on a validated readout. It sits in the
# re-run backlog with the trailing-clock sweep and Exp H on stock.
#
# Built on the fidelity base so the ONLY difference from Exp F/R is the 0x3C
# frame. Read ED: on the LCD overlay; anchor still applies.
guest-reload: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_RELOAD_3C_BUILD=1
guest-reload: LDSCRIPT = ld/at32f403a_guest.ld
guest-reload: clean all

# Build A (2026-08-13): isolate the load-bearing variable behind Stlkv's #18
# cold-start success. Stlkv drove OUR pins (PB6 CS) with OUR 115,638-byte payload
# via a GPIO bit-bang loop and got DONE_FINAL set. That narrows the difference
# from our two months of refusals to three candidates: (1) slow/gapped clocking
# of the WHOLE sequence, (2) the richer prelude reads (0x11/0x13/0x41 before
# CONFIG_ENABLE), (3) GPIO-mode vs hardware-SPI-AF pins.
#
# Build A tests (1)+(2) on our EXISTING hardware-SPI3 path WITHOUT bit-banging:
# cmd_br AND upload_br both /256, plus prelude_reads before INIT_ADDR. If the
# wall breaks here, the bit-bang (Build B) is unnecessary and this folds into
# fpga_init as the default cold-boot path. If it holds, GPIO-vs-AF is the last
# variable and Build B is next.
#
# NOTE the /256 upload takes ~2.0s (115638 x 8 x 256/120MHz); the splash->trace
# gap is expected, not a hang. Read the LCD overlay: SUCCESS = CFG line shows D1
# (DONE_FINAL) and S1:037F (both phases at /256 confirmed on-device). Built on
# the USART-silent base so the only deltas from the Exp F/R fidelity baseline are
# the three Build A knobs. See docs/bench_plan_2026-08-13.md and issue #18.
guest-configA: CFLAGS += -DGUEST_BUILD -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_A=1
guest-configA: LDSCRIPT = ld/at32f403a_guest.ld
guest-configA: clean all

# Build B (2026-08-13): true bit-bang transplant of the maksidze/Stlkv V0.4
# loader (ported byte-for-byte from Stlkv/OpenScope-2C23T-2C53T-port). Runs the
# whole SSPI handshake on GPIO-mode PB3/4/5/6 instead of the SPI3 peripheral,
# then restores SPI3 AF. Tested only if Build A's wall holds — it isolates the
# LAST candidate, GPIO/bit-bang clocking vs hardware-SPI AF. If B breaks the wall
# where A did not, GPIO-vs-AF is the answer. Same LCD verdict: CFG line D1 =
# DONE_FINAL. See fpga_bitbang_config_sequence(), docs/build_a_config_entry_test.md.
guest-configB: CFLAGS += -DGUEST_BUILD -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_B=1
guest-configB: LDSCRIPT = ld/at32f403a_guest.ld
guest-configB: clean all

# Build B + engine-arm (bench plan item 4, 2026-08-13). After the bit-bang config
# breaks the wall (DONE_FINAL set) the capture engine is UNARMED. This layers
# stock's post-config sequence (600ms -> five writes 01 08/02 03/06 00/07 00/08 AD
# -> 0x03 status read) onto Build B. Read SS byte 1 (01 = armed candidate) and
# watch the demo trace: it DISAPPEARS on first real sample = armed. See fpga.c
# fpga_bitbang_config_sequence FPGA_CONFIG_B_ARM block.
guest-configB-arm: CFLAGS += -DGUEST_BUILD -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_B=1 -DFPGA_CONFIG_B_ARM=1
guest-configB-arm: LDSCRIPT = ld/at32f403a_guest.ld
guest-configB-arm: clean all

# Cold-config + live-readout rig (bench plan item 4, 2026-08-13). The reliable
# arm-test bench: the warmtest 0x04/0x05 read/render pipeline (which latches the
# demo trace OFF on real data) with the bit-bang COLD config injected at boot
# (Build B breaks the wall) plus the engine-arm writes. So config (DONE_FINAL) +
# arm attempt + a TRUSTWORTHY trace signal in one image. SUCCESS = demo square
# wave disappears -> real continuous capture = cold boot to live trace. If it
# stays with OK: climbing, the readout works but the engine is still unarmed.
guest-coldtrace: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_B=1 -DFPGA_CONFIG_B_ARM=1
guest-coldtrace: LDSCRIPT = ld/at32f403a_guest.ld
guest-coldtrace: clean all

# Experiment G (2026-07-28): RECONFIG_N pulse candidates. Exp F closed every
# enumerable static MCU-state difference and the wall held, which sends us back
# to apicula's answer (docs/CONFIG_ENTRY_REPLY_FROM_APICULA.md): a running GW1N
# needs RECONFIG_N (low pulse >=25ns) or a power cycle before CONFIG_ENABLE will
# engage. Exp E's static snapshot cannot see a pulse — a pin pulsed LOW earlier
# reads HIGH at the config-enable instant, exactly like one merely held HIGH.
# PC2 and PB12 are the only pins stock drives that we float (outside the Exp
# C-refuted frontend bank), so they are the candidates. Fidelity build + pulse.
#   ^ CORRECTED 2026-08-11: that "only" was wrong, and it was wrong because the
#     enumeration behind it compared output LEVELS. PC1, PA6, PC11, PD2, PD3,
#     PD6 and PD13 are also driven by stock and floated by us — see guest-pc1 /
#     guest-fidelity2 above. It does not affect Exp G's own result (PC2 and PB12
#     were pulsed and refuted), only the claim that the candidate list was
#     complete.
guest-reconfig-pc2: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_FIDELITY_RECONFIG_PORT=3 -DFPGA_FIDELITY_RECONFIG_PIN=2
guest-reconfig-pc2: LDSCRIPT = ld/at32f403a_guest.ld
guest-reconfig-pc2: clean all

guest-reconfig-pb12: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_FIDELITY_RECONFIG_PORT=2 -DFPGA_FIDELITY_RECONFIG_PIN=12
guest-reconfig-pb12: LDSCRIPT = ld/at32f403a_guest.ld
guest-reconfig-pb12: clean all

# Experiment T (2026-08-11): RECONFIG_N candidate PC9 — the pin every previous
# search excluded by construction.
#
# Two independent leads point here:
#
# 1. rosenrot00/OpenScope-2C23T (independent 2C23T firmware whose scope WORKS)
#    pulses PC9 LOW 10ms -> HIGH 1ms immediately before its bitstream on the HW3
#    path, and PC8 on HW4. Same silicon family, same opcodes, same 160-byte
#    frame structure as our blob. Its HW3 loader also reads PC8 back as DONE.
#
# 2. Stock 2C53T does the same thing. Every PC9 access in V1.2.0:
#      0x0802AAB2  master_init +0x62   gpio_init(GPIOC, {pins=0x200})
#      0x0802AAB6  master_init +0x66   GPIOC->BRR  = 0x200   => PC9 LOW
#      0x0802B412  master_init +0x9C2  GPIOC->BSRR = 0x200   => PC9 HIGH
#      0x08029D3A  housekeeping/IWDG   GPIOC->BRR  = 0x200   => LOW (shutdown)
#    CONFIG_ENABLE is at 0x0802DA42 = master_init +0x2FF2, i.e. AFTER both.
#    Bases resolved by hand (r7 = movw #0x1014 + movt #0x4001 = GPIOC_BRR;
#    r2 = 0x40011000, [r2,#16] = BSRR) — find_gpio_pulses.py reports the
#    0x0802AAB6 mask as unresolved and so never surfaced this pin.
#
# WHY IT WAS NEVER TESTED: fpga.c's sweep table excludes PC9 by name ("power
# hold — driving it low kills the device"), so Exps O/Q swept twelve pins that
# could not contain the answer. And Exp E/F/R's static diffs all read PC9 as
# output+HIGH in BOTH firmwares (GPIOC CRH nibble 1 vs 3, ODR bit9 set in both)
# because stock raises it 9.8KB of code before the park point — the transition
# blind spot Exp G warned about, hit exactly.
#
# SAFETY: run this with USB ATTACHED. Exp R established that releasing the PC9
# hold does not power the device down while VBUS is present (that is why a
# POWER-button shutdown on USB hangs on "Goodbye" instead of going dark). Stock
# gets away with the same low period because the user's finger is still on POWER.
#
# CAVEAT: the static evidence has two readings and does not separate them —
# (A) a RECONFIG_N assert released before config, or (B) ordinary power-hold
# sequencing (don't latch the rail until the firmware has decided to boot).
# This build is what separates them. Read ED: on the LCD overlay; the anchor
# (0x11 -> 0x0120681B) still applies, and the real success signal is Exp L/M's
# open/closed test — a configured part STOPS answering 0x11.
guest-reconfig-pc9: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_FIDELITY_RECONFIG_PORT=3 -DFPGA_FIDELITY_RECONFIG_PIN=9
guest-reconfig-pc9: LDSCRIPT = ld/at32f403a_guest.ld
guest-reconfig-pc9: clean all

# Experiment J (2026-07-28): ANCHORED opcode-discrimination probe.
# Exp I established that every FPGA status number this project has recorded was
# a phase-slice of the free-running 0xC8100001 MISO pattern, not a register read
# — the root cause being that no measurement here has ever had a known correct
# answer. This build fixes that: it reads 0x11 READ_IDCODE at /256 and looks for
# 0x0120681B (known independently from the Gowin .fs preamble at file offset
# 0x4AD19), with 0x00 / 0x41 / 0x13 alongside as controls, 8 bytes each so a
# repeating 4-byte pattern is visible as data rather than inferred.
#   IDCODE found + ID != NP  => the FPGA decodes SSPI opcodes; the 2026-06-13
#                               "not in config-receive mode" conclusion (which is
#                               what pointed this project at JTAG) collapses.
#   all opcodes identical    => that conclusion finally rests on a valid
#                               measurement, and the FT232H is the route.
# Built on the fidelity image so the wire conditions match Exp F/I.
# See reverse_engineering/analysis_v120/expE_swd_state_diff_2026-07-28.md § Exp I/J
guest-idcode: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_IDCODE_PROBE=1
guest-idcode: LDSCRIPT = ld/at32f403a_guest.ld
guest-idcode: clean all

# Step-resolved anchored config-status trace (2026-07-28).
# Exp M proved we never enter config mode: the port answers IDCODE at every
# checkpoint and STATUS never moves. This build resolves that by STEP — the
# anchored STATUS is captured at all six CS-frame boundaries (pristine, after
# 05, after 12, after 15, after the full upload, after 3A), each one validated
# against the known IDCODE first and DISCARDED if the anchor fails.
#   all six identical => the part ignores every config command while still
#                        answering reads; the failure is config ENTRY, full stop.
#   one step moves    => that is where our sequence first has an effect, and
#                        where to aim next.
#   an 'x' partway    => the config port CLOSED at that step (Exp L behaviour),
#                        which would mean something DID take.
# Built on the fidelity image so wire conditions match Exp F/I/M.
guest-trace: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CFG_TRACE_BUILD=1
guest-trace: LDSCRIPT = ld/at32f403a_guest.ld
guest-trace: clean all

# RECONFIG_N candidate pin sweep (2026-07-28), button-gated.
# Exp N: no config command moves the STATUS register while every read command
# answers correctly — the documented behaviour of a running auto-booted GW1N that
# refuses configuration until RECONFIG_N is pulsed. This searches for that pin.
# Press SAVE in scope mode: each candidate is pulsed LOW->HIGH, CONFIG_ENABLE is
# sent, and the anchored STATUS is read; anything off the baseline is a hit.
#
# Candidates are CONSERVATIVE — only pins stock itself drives as outputs before
# the FPGA handshake (stock_pre_fpga_gpio_state.md), so the MCU provably owns
# them and there is no contention risk. PC9 (power hold), PC8/PC13 (button
# inputs), SPI3, SWD, USART2, the DAC pins, PB8 and the EXMC/LCD bus are all
# excluded. Every pin's config and output latch are restored as the sweep goes.
#
# Button-gated and run from the UI task rather than fpga_init: a bad pulse during
# init fails the boot, and three failed boots latch the bootloader into SAFE MODE
# (this unit has done that twice). From the UI it costs a power-cycle, and the
# sweep can be re-run without a reflash.
guest-sweep: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_PIN_SWEEP_BUILD=1
guest-sweep: LDSCRIPT = ld/at32f403a_guest.ld
guest-sweep: clean all

# RTT-only debug shell (2026-08-11). Cuts the USB CDC half out of the console.
# Measured on the bench unit: usbd_connect_state_get() reports CONFIGURED even
# though CDC never enumerates (error -71), so usb_send_bytes() enters the CDC TX
# path and the shell task stalls there instead of returning to poll RTT — the
# banner reached the RTT ring (rtt_write runs first) and nothing followed.
# RTT exists to bypass that transport; this stops the tee re-introducing it.
guest-rtt: CFLAGS += -DGUEST_BUILD -DDEBUG_SHELL_RTT_ONLY=1
guest-rtt: LDSCRIPT = ld/at32f403a_guest.ld
guest-rtt: clean all

# Fault-handler self-test (2026-08-11). Fires `udf #0` from a task ~10 s after
# boot. Answers (a) does the fault handler record anything at all, and (b) does
# the device freeze on its own with NO debugger attached — the confound that
# makes the "~55 s HardFault" reading suspect, since the freeze so far has only
# ever been observed after an OpenOCD attach.
guest-faulttest: CFLAGS += -DGUEST_BUILD -DFAULT_SELFTEST=1
guest-faulttest: LDSCRIPT = ld/at32f403a_guest.ld
guest-faulttest: clean all
