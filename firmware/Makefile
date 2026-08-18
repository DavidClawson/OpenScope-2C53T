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
C_SOURCES += src/ui/scope_measure.c
C_SOURCES += src/tasks/input_handler.c
C_SOURCES += src/tasks/measurement.c
C_SOURCES += src/drivers/flash_fs.c
C_SOURCES += src/drivers/flash_regions.c
C_SOURCES += src/drivers/flash_regions_w25q.c
C_SOURCES += src/drivers/esp_comm.c
C_SOURCES += src/drivers/watchdog.c
C_SOURCES += src/drivers/button_scan.c
C_SOURCES += src/drivers/continuity_buzzer.c
C_SOURCES += src/drivers/dfu_boot.c
C_SOURCES += src/drivers/battery.c
C_SOURCES += src/drivers/fpga.c
C_SOURCES += src/drivers/fpga_meter_plan.c
C_SOURCES += src/drivers/fpga_scanner.c
C_SOURCES += src/drivers/dac_output.c
C_SOURCES += src/drivers/scope_trigger.c
C_SOURCES += src/drivers/meter_data.c
C_SOURCES += src/drivers/meter_auto.c
C_SOURCES += src/drivers/meter_autoselect.c
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
C_SOURCES += src/util/cal_dump.c
C_SOURCES += src/util/cal_reference.c
C_SOURCES += src/util/settings_store.c
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
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_exint.c
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
# SCOPE_DEBUG_OVERLAY paints a 4-line debug strip over the bottom of the scope
# area (rows 180-219). Useful for bench diagnostics, but it sits on top of the
# trace — disabled for a clean display. Re-add `-DSCOPE_DEBUG_OVERLAY` to bring
# the strip back.
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
# [LIVE] Standard bench image. The default for every bench session:
#        `make guest` -> factory IAP -> read the LCD SCOPE_DEBUG_OVERLAY.
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
# [LIVE] Warm-handoff dev loop (~2 min/iteration) for scope-side work with
#        no cold-config risk. Needs stock flashed first + unbroken power.
# guest-warmtest-usart — warm handoff with USART2 ALIVE (2026-08-17).
#
# The 2026-08-12 warm-handoff run proved our READ path against an FPGA that
# STOCK configured, but it only ever checked CH1. It never checked CH2, and it
# never checked the meter — and on 2026-08-17 the meter was found DEAD on
# builds that configure the FPGA themselves ([[config-kills-the-meter]]:
# USART2 STS=0x000000C0, TDC set, RDBF/ORERR never set, no byte ever returns).
#
# This build exists to ask both questions at once against a KNOWN-GOOD
# configuration. FPGA_WARM_HANDOFF_TEST returns early from fpga_init before any
# config sequence, so stock's SRAM configuration survives untouched; USART2 is
# still initialised earlier in fpga_init, so with the silent flag off the bus is
# live and `usart raw` + `fpga usart` can measure whether the meter answers.
#
# meter+CH2 both work  -> our bit-bang configuration is deficient, ONE root cause
# neither works        -> our runtime code, not the configuration
# split                -> two independent faults
guest-warmtest-usart: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=0
guest-warmtest-usart: LDSCRIPT = ld/at32f403a_guest.ld
guest-warmtest-usart: clean all

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
# [LIVE] Queued bench item (dev plan F3): CH2 trigger reference via TMR13.
#        Decoded, NOT yet confirmed on hardware.
guest-warmtest-ch2: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CH2_TRIGGER=1
guest-warmtest-ch2: LDSCRIPT = ld/at32f403a_guest.ld
guest-warmtest-ch2: clean all

# USART-silent scope-boot experiment (guest image). Holds USART2 UEN CLEAR through
# the whole boot, skips Step 3b + Step 8 USART traffic, creates no dvom/meter/acq
# auto-tasks — so the SPI3 wire stays quiet and `fpga reinit`/`spi3 xfer` run
# unperturbed. Tests the stock-bringup-diff hypothesis that live USART2 keeps the
# FPGA's NV design owning the SSPI pins. See fpga.c FPGA_USART_SILENT_SCOPE and
# docs/fpga_stock_bringup_diff_plan.md.
# [HISTORICAL] 2026-06-13 user-mode-lockout test. REFUTED — do not re-chase.
#              (The FPGA_USART_SILENT_SCOPE flag itself is still used by most
#              live targets; it is this standalone image that is retired.)
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
# [PARKED — UNTESTED] External-master bring-up (ESP32/FT232H). Never run on
#              hardware. The JTAG/external rationale weakened after Exp J
#              showed SSPI reaches the config engine fine.
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

$(BUILD_DIR)/test_meter_data: tests/test_meter_data.c src/drivers/meter_data.c src/drivers/meter_data.h src/drivers/fpga_meter_plan.c src/drivers/fpga_meter_plan.h | $(BUILD_DIR)
	$(HOST_CC) -std=gnu11 -Wall -Wextra -DMETER_DATA_HOST_TESTS -Isrc/drivers $< src/drivers/meter_data.c src/drivers/fpga_meter_plan.c -lm -pthread -o $@

test-meter-data: $(BUILD_DIR)/test_meter_data
	$<

$(BUILD_DIR)/test_meter_auto: tests/test_meter_auto.c src/drivers/meter_auto.c src/drivers/meter_auto.h src/drivers/meter_data.h src/drivers/fpga_meter_plan.c src/drivers/fpga_meter_plan.h | $(BUILD_DIR)
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Isrc/drivers $< src/drivers/meter_auto.c src/drivers/fpga_meter_plan.c -lm -o $@

test-meter-auto: $(BUILD_DIR)/test_meter_auto
	$<

$(BUILD_DIR)/test_meter_voltage_wave: tests/test_meter_voltage_wave.c src/ui/meter_voltage_wave.c src/ui/meter_voltage_wave.h | $(BUILD_DIR)
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Isrc/ui $< src/ui/meter_voltage_wave.c -lm -o $@

test-meter-voltage-wave: $(BUILD_DIR)/test_meter_voltage_wave
	$<

$(BUILD_DIR)/test_fpga_meter_plan: tests/test_fpga_meter_plan.c src/drivers/fpga_meter_plan.c src/drivers/fpga_meter_plan.h | $(BUILD_DIR)
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Isrc/drivers $< src/drivers/fpga_meter_plan.c -o $@

test-fpga-meter-plan: $(BUILD_DIR)/test_fpga_meter_plan
	$<

$(BUILD_DIR)/test_scope_measure: tests/test_scope_measure.c src/ui/scope_measure.c src/ui/scope_measure.h | $(BUILD_DIR)
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Isrc/ui $< src/ui/scope_measure.c -lm -o $@

# Measurements the scope badges are allowed to print. Includes two negative
# controls (broken implementations that MUST be caught) — see the file header.
test-scope-measure: $(BUILD_DIR)/test_scope_measure
	$<

# Settings persistence: config.c + settings_store.c against a NOR flash model
# (erase sets 0xFF, program only clears bits, page programs cannot cross a page).
# Proves the round trip, the fallbacks (first boot / torn record / bad checksum /
# foreign version), and — the one that matters — that the settings writer cannot
# reach a read-only region. See tests/test_config_persist.c.
$(BUILD_DIR)/test_config_persist: tests/test_config_persist.c src/util/config.c src/util/config.h \
                                  src/util/settings_store.c src/util/settings_store.h \
                                  src/drivers/flash_regions.c src/drivers/flash_regions.h \
                                  src/ui/theme.c src/ui/scope_state.c | $(BUILD_DIR)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -O1 -DSETTINGS_PERSIST_WRITES=1 \
		-Isrc/util -Isrc/drivers -Isrc/ui -Isrc/dsp \
		$< src/util/config.c src/util/settings_store.c src/drivers/flash_regions.c \
		src/ui/theme.c src/ui/scope_state.c -o $@

test-config-persist: $(BUILD_DIR)/test_config_persist
	$<

test-stock-meter-literals:
	python3 ../scripts/test_stock_meter_literals.py

test-meter: test-meter-data test-meter-auto test-meter-voltage-wave test-fpga-meter-plan test-stock-meter-literals

# Redraw gate (B2): counts full-area repaints for the scope/siggen screens
# against a transcription of the pre-B2 logic. See tests/test_redraw_gate.c.
$(BUILD_DIR)/test_redraw_gate: tests/test_redraw_gate.c src/ui/redraw_gate.h | $(BUILD_DIR)
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Isrc/ui $< -o $@

test-redraw-gate: $(BUILD_DIR)/test_redraw_gate
	$<

.PHONY: all clean disasm size renode soak-test soak-quick flash flash-dfu bootloader flash-bootloader flash-all guest guest-warmtest guest-usart-silent emu test-meter-data test-meter-auto test-meter-voltage-wave test-fpga-meter-plan test-stock-meter-literals test-meter test-scope-measure test-redraw-gate test-config-persist guest-persist guest-caldump guest-coldtrace-slow guest-coldtrace-la guest-debugclk fpga-alt-header

# ═══════════════════════════════════════════════════════════════════════════
# BENCH / EXPERIMENT IMAGE INDEX                          (index added 2026-08-13)
#
# Everything below this line is a bench image. They accumulated one per
# experiment over four months of chasing FPGA config entry, and from the target
# names alone there is no way to tell which two you would actually flash today
# from the eighteen that answered a question and are kept only so the answer
# stays reproducible.
#
# NOTHING HERE IS DELETED, deliberately. Each target is the executable form of
# an experiment written up in CLAUDE.md and analysis_v120/; deleting one would
# leave a claim in the docs with no way to re-run it. They are TAGGED instead:
# every target below carries a [LIVE] / [HISTORICAL] / [PARKED] / [BLOCKED] line
# immediately above it, with its experiment and result.
#
#   [LIVE]        part of the current bench loop, or an arm of a question that
#                 is still open. Flash these.
#   [HISTORICAL]  one-shot experiment, already answered. Kept for
#                 reproducibility; re-run only to re-check a specific result.
#   [PARKED]      built but never validated on hardware.
#   [BLOCKED]     cannot run on bench unit #1 for a known reason.
#
# LIVE, in the order you would reach for them:
#   guest              standard bench image (IAP + LCD overlay loop)
#   guest-coldtrace    cold boot -> configured FPGA -> live trace (the rig)
#   guest-coldtrace-slow  same, AUTO read cadence 500 ms (acq starvation test)
#   guest-coldtrace-la    same, bit-bang config SCK slowed for 24 MS/s logic analyzers
#   guest-debugclk     coldtrace but loads the debugclk_hw_top DIAGNOSTIC bitstream
#   guest-configB      config only; also the queued CDC single-variable test
#   guest-configB-arm  config + engine arm, no live readout
#   guest-warmtest     warm-handoff dev loop for scope-side work
#   guest-warmtest-ch2 CH2 trigger bring-up — decoded, NOT bench-confirmed
#   guest-configA      \ the two arms of the still-open bit-bang-vs-AF /
#   guest-config05     / no-0x05 bisect
#
# HISTORICAL (all NEGATIVE unless noted), newest first:
#   guest-reconfig-pc9  Exp T   PC9 RECONFIG_N pulse
#   guest-reload        Exp S   Gowin RELOAD 0x3C
#   guest-pc1           Exp R   PC1 LOW + PC2 HIGH
#   guest-fidelity2     Exp R   full config-register set
#   guest-sweep         Exps O/Q  RECONFIG_N pin sweeps
#   guest-trace         Exp N   anchored STATUS at every CS-frame boundary
#   guest-idcode        Exp J   POSITIVE — the first anchored measurement;
#                               its method is now mandatory project-wide
#   guest-reconfig-pc2  Exp G   PC2 pulse
#   guest-reconfig-pb12 Exp G   PB12 pulse
#   guest-fidelity      Exp F   all five enumerables (also a base for others)
#   guest-spin          Exp E   SWD park at CONFIG_ENABLE
#   guest-usart-silent  2026-06-13 user-mode-lockout test — refuted
#   guest-faulttest     2026-08-11 fault-handler self-test
#
# PARKED: guest-bringup (external master, never run).
# BLOCKED: guest-rtt (RDP makes RTT unreachable on unit #1).
#
# Full writeups: CLAUDE.md, analysis_v120/expE_swd_state_diff_2026-07-28.md,
# docs/build_a_config_entry_test.md, docs/fpga_warm_handoff_test.md.
# ═══════════════════════════════════════════════════════════════════════════

# Experiment E (2026-07-27): park in fpga_init at the exact config-enable instant
# (prelude 05/12 sent, CS LOW, 0x15 not yet clocked) so SWD can dump peripheral
# state for a diff vs the stock spin image (/tmp/APP_2C53T_expE_spin_stock.bin).
# The device intentionally never reaches the scheduler — reflash to recover.
# [HISTORICAL] Exp E (2026-07-27/28) — SWD park at CONFIG_ENABLE. Answered:
#              clock tree identical; see the raw dumps in analysis_v120.
#              Note SWD on this unit is halt-and-poke only (RDP kills the CPU
#              on attach), so re-running has a real cost.
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
# [HISTORICAL] Exp F (2026-07-28) — all five 'enumerables' closed at once.
#              NEGATIVE (ED:00039020). Still useful as the fidelity BASE that
#              several targets below build on.
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
# [HISTORICAL] Exp R (2026-08-11) — PC1 LOW + PC2 HIGH. NEGATIVE.
guest-pc1: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_FIDELITY_DRIVE_PC1=1
guest-pc1: LDSCRIPT = ld/at32f403a_guest.ld
guest-pc1: clean all

# [HISTORICAL] Exp R (2026-08-11) — the full config-register set. NEGATIVE.
#              Closed the config-register class; static MCU state excluded.
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
# [HISTORICAL] Exp S (2026-08-11) — Gowin RELOAD 0x3C. NEGATIVE.
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
# [LIVE] Failed control (CFG:00039020 D0) — but still live, because the
#        bit-bang-vs-AF / no-0x05 bisect it belongs to is still OPEN.
guest-configA: CFLAGS += -DGUEST_BUILD -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_A=1
guest-configA: LDSCRIPT = ld/at32f403a_guest.ld
guest-configA: clean all

# 0x05-omit test (2026-08-13, issue #18): the HARDWARE-SPI config path with the
# 0x05 ERASE_SRAM frame skipped, so the prelude is 0x12 INIT_ADDR -> 0x15
# CONFIG_ENABLE. This is the single-variable experiment maksidze proposed and we
# committed to running in parallel.
#
# WHY: our working Build B bit-bang loader ALSO omits 0x05, while the failing
# hardware-SPI loader sends it -- so "bit-bang vs AF-mode" and "no-0x05 vs 0x05"
# have never been separated. maksidze's stock /64 capture already proved a GW1N
# can be configured over slow hardware SPI, which excludes clock rate and leaves
# the prelude as the live suspect.
#
# READ: the LCD overlay CFG: line. 0003F460 / D1 = DONE_FINAL set -> the prelude
# was the answer all along, and we gain a config path ~100x faster than
# bit-banging 115,638 bytes. 00039020 / D0 = unchanged wall -> 0x05 is not the
# variable and the bit-bang/AF difference is what matters.
# [LIVE] Other arm of the same open bisect: hardware-SPI with 0x05 omitted.
guest-config05: CFLAGS += -DGUEST_BUILD -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_OMIT_05=1
guest-config05: LDSCRIPT = ld/at32f403a_guest.ld
guest-config05: clean all

# Build B (2026-08-13): true bit-bang transplant of the maksidze/Stlkv V0.4
# loader (ported byte-for-byte from Stlkv/OpenScope-2C23T-2C53T-port). Runs the
# whole SSPI handshake on GPIO-mode PB3/4/5/6 instead of the SPI3 peripheral,
# then restores SPI3 AF. Tested only if Build A's wall holds — it isolates the
# LAST candidate, GPIO/bit-bang clocking vs hardware-SPI AF. If B breaks the wall
# where A did not, GPIO-vs-AF is the answer. Same LCD verdict: CFG line D1 =
# DONE_FINAL. See fpga_bitbang_config_sequence(), docs/build_a_config_entry_test.md.
# [LIVE] Config-only half of the winning rig; also the image queued for the
#        CDC single-variable test (dev plan F1).
guest-configB: CFLAGS += -DGUEST_BUILD -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_B=1
guest-configB: LDSCRIPT = ld/at32f403a_guest.ld
guest-configB: clean all

# Build B + engine-arm (bench plan item 4, 2026-08-13). After the bit-bang config
# breaks the wall (DONE_FINAL set) the capture engine is UNARMED. This layers
# stock's post-config sequence (600ms -> five writes 01 08/02 03/06 00/07 00/08 AD
# -> 0x03 status read) onto Build B. Read SS byte 1 (01 = armed candidate) and
# watch the demo trace: it DISAPPEARS on first real sample = armed. See fpga.c
# fpga_bitbang_config_sequence FPGA_CONFIG_B_ARM block.
# [LIVE] Config + engine-arm, no live readout. Kept as the isolation point
#        between 'config took' and 'engine armed'.
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
# [LIVE] THE shippable rig. Cold boot -> configured FPGA -> live capture,
#        bench-confirmed 2026-08-13. Start here when in doubt.
# [LIVE] guest-persist — guest-coldtrace PLUS runtime settings persistence.
# SETTINGS_PERSIST_WRITES defaults to 0 everywhere else (see src/util/config.h):
# this is the ONLY build that writes the W25Q at runtime. Nothing in this
# project has ever done that on hardware, so treat the first run as a
# supervised bench step:
#   1. Dump 0xF10000-0xF1FFFF first and confirm it is 0xFF-blank on the unit.
#   2. Cold-boot check: config entry must still reach CFG:0003F460 (DONE_FINAL).
#      settings_store_init() runs after fpga_init() so it should not perturb it.
#   3. Change theme -> press another button >2 s later (or MENU) -> power off ->
#      power on. The theme should return.
#   4. Dump again and confirm ONLY 0xF10000+ changed.
# Factory calibration lives at 0x000000-0x1FFFFF, a READONLY region ~14 MB away.
# [LIVE] guest-caldump — READ-ONLY diagnostic. Reports MCU flash
# 0x08006000..0x08006FFF, the saved-config sector that is stock's candidate home
# for per-device calibration. Nobody has ever read it on any unit: RDP blocks
# external debuggers, but the CPU reads its own flash normally, so the firmware
# can do what the debugger cannot.
# Writes NOTHING. Links at 0x08007000 so it cannot overlap the region it reads.
# Runs immediately after LCD bring-up and never returns — no FPGA init, no
# scheduler. Report the CRC32 it prints. See src/util/cal_dump.h.
guest-caldump: CFLAGS += -DGUEST_BUILD -DCAL_DUMP_MODE=1
guest-caldump: LDSCRIPT = ld/at32f403a_guest.ld
guest-caldump: clean all

guest-persist: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_B=1 -DFPGA_CONFIG_B_ARM=1 -DSETTINGS_PERSIST_WRITES=1
guest-persist: LDSCRIPT = ld/at32f403a_guest.ld
guest-persist: clean all

# guest-coldtrace-usart — coldtrace with USART2 ALIVE (2026-08-17).
#
# Identical to guest-coldtrace except FPGA_USART_SILENT_SCOPE is 0, so the
# dvom_TX/dvom_RX/meter tasks are created and fpga_init runs the meter boot
# sequence over USART2.
#
# WHY: stock's command table puts SCOPE CHANNEL CONFIGURATION on USART2 --
# cmd 0x01 = "configure channel", type 0 = CH1 / type 1 = CH2, and 0x0B-0x11 =
# channel/trigger/timebase (FPGA_TASK_ANALYSIS.md). CH2 has been chased on SPI3
# for a month; that bus is now exhausted (full stock command set decoded and
# matched, regs 01/02/06/07/08/09 swept, 02/06/07 re-swept with re-arm on).
#
# AND WHY THIS BUILD RATHER THAN A SHELL POKE: on 2026-08-17 USART2 was enabled
# at runtime and verified by register readback (CTRL1 0x00000000 -> 0x0000202C,
# BAUDR 0x30D4 = stock's), yet NOTHING answered -- not even 0x0508/0x0509, the
# meter frames fpga_init itself sends in non-silent builds. So the USART results
# are VOID, not negative: there is no validated instrument. Two causes were
# found and only one is fixed. `usart tx` QUEUES, and coldtrace never creates
# the task that drains the queue, so those frames were never transmitted at all
# (this also voids the earlier 2026-08-16 "no echo ever returned" negative).
# `usart raw` does transmit directly, and still drew no reply.
#
# This build exists to supply the missing positive control: if the METER works
# here, USART2 is proven end-to-end and a silent scope command finally means
# something. If the meter does NOT work here, the fault is in our USART bring-up
# and no scope conclusion can be drawn from that bus at all.
#
# Config entry is unchanged (FPGA_CONFIG_B bit-bang), and stock's own order is
# preserved: USART2 stays dark through config and comes up afterwards.
guest-coldtrace-usart: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=0 -DFPGA_CONFIG_B=1 -DFPGA_CONFIG_B_ARM=1
guest-coldtrace-usart: LDSCRIPT = ld/at32f403a_guest.ld
guest-coldtrace-usart: clean all

guest-coldtrace: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_B=1 -DFPGA_CONFIG_B_ARM=1
guest-coldtrace: LDSCRIPT = ld/at32f403a_guest.ld
guest-coldtrace: clean all

# [CH2 OFFSET 2026-08-17] guest-coldtrace-ch2 — coldtrace plus a boot-time arm
# of the CH2 vertical-offset reference (TMR13 CH1 PWM-DAC on PA6).
#
# EXP-06 measured CH1's full gain ladder (21.23 / 42.04 / 91.41 / 108.83 /
# 228.30 / 491.67 mV per count, cross-checked against Stlkv's independent
# numbers on another unit) but could only get ONE usable tap out of CH2: every
# other mux code parked at a fixed DC level (mean 0, 232, 255, 26) and railed
# regardless of drive amplitude. That is the signature of a missing vertical
# offset, not a gain-table error. CH1's offset is DAC1 on PA4, which
# fpga_init() arms mid-scale and `fpga scope center` tunes per range; CH2's is
# a TMR13 PWM-DAC on PA6 that our firmware has never programmed.
#
# This target only sets FPGA_CH2_TRIGGER, which arms TMR13 at boot alongside
# the DAC1 write. The runtime knobs (`trig2 raw`, `fpga scope center ch2`)
# self-init and work on ANY build, so the experiment does NOT require this
# target — flash it once the pin is confirmed and you want the arm at boot.
#
# ⚠ PA6 is DECODED, NOT CONFIRMED (HARDWARE_PINOUT.md: "candidate,
# unconfirmed"). It is derived from stock's master_init TMR13 setup at
# 0x0802B0FE and the C1DT write at 0x08008C3A (ripcord contract 38). Run the
# CH1 DAC1 sweep as a positive control in the same session before believing a
# negative here: if DAC1 moves CH1's mean and this moves nothing on CH2, the
# pin is wrong — which is a clean result, not an ambiguous one.
guest-coldtrace-ch2: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_B=1 -DFPGA_CONFIG_B_ARM=1 -DFPGA_CH2_TRIGGER=1
guest-coldtrace-ch2: LDSCRIPT = ld/at32f403a_guest.ld
guest-coldtrace-ch2: clean all

# [LA 2026-08-15] guest-coldtrace-la — coldtrace with the bit-bang config clock
# slowed (FPGA_BB_HALF_DELAY=60 nops per half period, ~1-2 MHz SCK) so a
# 24 MS/s logic analyzer can decode the whole config sequence (issue #18:
# maksidze's captures of the undelayed loop were aliased). Otherwise identical
# to guest-coldtrace.
guest-coldtrace-la: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_B=1 -DFPGA_CONFIG_B_ARM=1 -DFPGA_BB_HALF_DELAY=60
guest-coldtrace-la: LDSCRIPT = ld/at32f403a_guest.ld
guest-coldtrace-la: clean all

# [BENCH 2026-08-15] guest-coldtrace-slow — coldtrace with the AUTO free-run
# read cadence throttled from 30 ms to 500 ms (> one ~430 ms buffer fill).
# Single-variable test of the acq-path "flat buffer" bug
# (analysis_v120/scope_display_next_session_2026-08-14.md): spi3 opread always
# follows an acq-task park (>= one full fill) and sees the swing, while the
# 30 Hz acq loop may be starving the capture. If `fpga acq` first bytes /
# overlay span now match `spi3 opread 04 1026`, the mechanism is cadence.
guest-coldtrace-slow: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_B=1 -DFPGA_CONFIG_B_ARM=1 -DFPGA_AUTO_CADENCE_MS=500u -DFPGA_AUTO_TRIG_WAIT_MS=5u
guest-coldtrace-slow: LDSCRIPT = ld/at32f403a_guest.ld
guest-coldtrace-slow: clean all

# [BENCH 2026-08-14] guest-coldtrace-faithful — coldtrace with the config+arm
# sequence made BYTE-EXACT to stock's June-capture wire protocol (see the
# FPGA_CONFIG_B_FAITHFUL comment block in fpga.c for the five deltas closed).
# Target observation: does the engine land in stock's state — post-arm 0x03
# status 00 01 42 2E 2E, CH1 header byte2 flag semantics (00/01), and buffer
# auto-refresh at ~26-45/s with a quiet input (the "23x mystery"). NOTE: this
# build cannot read DONE_FINAL (no config-port reads by design); judge success
# by the 0x03 SS bytes, demo-trace disappearance, and live OK/PC0 rates.
guest-coldtrace-faithful: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_B=1 -DFPGA_CONFIG_B_ARM=1 -DFPGA_CONFIG_B_FAITHFUL=1
guest-coldtrace-faithful: LDSCRIPT = ld/at32f403a_guest.ld
guest-coldtrace-faithful: clean all

# [BENCH 2026-08-16] guest-debugclk — guest-coldtrace with a DIFFERENT FPGA
# payload: the debugclk_hw_top diagnostic image instead of the stock bitstream
# (-DFPGA_ALT_BITSTREAM=1 swaps fpga_cal_table.h's array for
# src/drivers/fpga_bitstream_alt.h; everything else is bit-for-bit the
# coldtrace configuration, so the bit-bang config path being exercised is the
# one already proven to break the config-entry wall).
#
# What it is for: the image pushes SYNTHETIC lanes — CH1 a free-running 8-bit
# ramp, CH2 a walking one — through the REAL capture/readout path. The bytes
# are therefore known before we read them, which is exactly what the 2026-08-15
# desk-sweep result lacks: op 04 and op 05 both returned CH1, and no evidence
# since separates "the readout mux does not split the channels" from "our read
# framing is wrong". A ramp and a one-hot pattern cannot be confused.
#
# It also hands the sample clock to the MCU. There is no PLL in this image:
# PC6 (QN48 pin 30, IOB7B, dbg_clk) advances the whole capture core one sample
# per RISING edge, and PB11 (QN48 pin 46, IOR1B, run_enable) re-arms on a
# low->high edge. Both are driven from the shell — `fpga dbgclk` / `fpga dbgarm`
# — so rate and arming are under direct control rather than inferred.
# ⚠ Those two pins mean something else entirely on a stock payload (PC6 = "FPGA
# SPI enable", PB11 = a CH2 relay per the 2026-08-15 sweep). Scope capture, the
# meter path and every calibration assumption are OFF for this image.
#
# Bench: `fpga dbgarm` -> `fpga dbgclk 16` (>=8 clocks first: an 8-cycle
# power-on-reset shifter runs off dbg_clk) -> `fpga dbgclk 1100` ->
# `spi3 opread 04 1026 dump`. Expect "R1V" (52 31 56) then a rising ramp;
# op 05 should give one-hot bytes. Read with opread, NOT the acq task:
# this image frames 3 header bytes + 1023 samples where stock frames 2 + 1024.
#
# FPGA_CONFIG_B_ARM stays ON deliberately. It is what stages PB11 HIGH and
# PC6 HIGH at boot, which is exactly the run/clock posture this image wants,
# and its five post-config writes (01 08 / 02 03 / 06 00 / 07 00 / 08 AD) are
# no-ops here: those values ARE this image's register reset defaults, so the
# sequence writes each register the value it already holds.
#
# Regenerating the payload header after a new .fs build (the ONE command):
#   python3 ../scripts/fs2header.py firmware/fpga/debugclk_hw_top.fs \
#       firmware/src/drivers/fpga_bitstream_alt.h --name debugclk_hw_top \
#       --notes-file firmware/fpga/debugclk_hw_top.notes.txt
# (run from the repo root, or use `make fpga-alt-header` from firmware/).
guest-debugclk: CFLAGS += -DGUEST_BUILD -DFPGA_WARM_HANDOFF_TEST=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_CONFIG_B=1 -DFPGA_CONFIG_B_ARM=1 -DFPGA_ALT_BITSTREAM=1
guest-debugclk: LDSCRIPT = ld/at32f403a_guest.ld
guest-debugclk: clean all

# Regenerate the alternate payload header from its .fs. Not a build
# dependency on purpose: the generated header is checked in, so a normal
# build never needs Python, and a payload swap stays a deliberate act.
FPGA_ALT_FS    ?= fpga/debugclk_hw_top.fs
FPGA_ALT_NOTES ?= fpga/debugclk_hw_top.notes.txt
FPGA_ALT_NAME  ?= debugclk_hw_top
fpga-alt-header:
	python3 ../scripts/fs2header.py $(FPGA_ALT_FS) src/drivers/fpga_bitstream_alt.h \
		--name $(FPGA_ALT_NAME) --notes-file $(FPGA_ALT_NOTES)

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
# [HISTORICAL] Exp G (2026-07-28) — RECONFIG_N candidate PC2. NEGATIVE.
guest-reconfig-pc2: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_FIDELITY_RECONFIG_PORT=3 -DFPGA_FIDELITY_RECONFIG_PIN=2
guest-reconfig-pc2: LDSCRIPT = ld/at32f403a_guest.ld
guest-reconfig-pc2: clean all

# [HISTORICAL] Exp G (2026-07-28) — RECONFIG_N candidate PB12. NEGATIVE.
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
# [HISTORICAL] Exp T (2026-08-11) — RECONFIG_N candidate PC9. NEGATIVE at
#              this pulse POSITION; stock's position variant was never run.
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
# [HISTORICAL] Exp J (2026-07-28) — POSITIVE, and the most important target
#              in this list: the first anchored measurement. Its method (read
#              IDCODE 0x11 at /256, require 0x0120681B) is now MANDATORY for
#              every FPGA measurement in this project.
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
# [HISTORICAL] Exp N (2026-07-28) — anchored STATUS at all six CS-frame
#              boundaries. NEGATIVE: no config command moved the register.
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
# [HISTORICAL] Exps O/Q (2026-07-28/29) — RECONFIG_N pin sweeps, button-gated.
#              NEGATIVE, zero anchor failures.
guest-sweep: CFLAGS += -DGUEST_BUILD -DFPGA_STOCK_FIDELITY=1 -DFPGA_USART_SILENT_SCOPE=1 -DFPGA_PIN_SWEEP_BUILD=1
guest-sweep: LDSCRIPT = ld/at32f403a_guest.ld
guest-sweep: clean all

# RTT-only debug shell (2026-08-11). Cuts the USB CDC half out of the console.
# Measured on the bench unit: usbd_connect_state_get() reports CONFIGURED even
# though CDC never enumerates (error -71), so usb_send_bytes() enters the CDC TX
# path and the shell task stalls there instead of returning to poll RTT — the
# banner reached the RTT ring (rtt_write runs first) and nothing followed.
# RTT exists to bypass that transport; this stops the tee re-introducing it.
# [BLOCKED] RTT console. Works as code, but is UNREACHABLE on bench unit #1:
#           RDP is set, so attaching a debugger kills the CPU. Needs a
#           different unit or an RDP clear (mass erase) to be useful.
guest-rtt: CFLAGS += -DGUEST_BUILD -DDEBUG_SHELL_RTT_ONLY=1
guest-rtt: LDSCRIPT = ld/at32f403a_guest.ld
guest-rtt: clean all

# Fault-handler self-test (2026-08-11). Fires `udf #0` from a task ~10 s after
# boot. Answers (a) does the fault handler record anything at all, and (b) does
# the device freeze on its own with NO debugger attached — the confound that
# makes the "~55 s HardFault" reading suspect, since the freeze so far has only
# ever been observed after an OpenOCD attach.
# [HISTORICAL] 2026-08-11 one-shot fault-handler self-test.
guest-faulttest: CFLAGS += -DGUEST_BUILD -DFAULT_SELFTEST=1
guest-faulttest: LDSCRIPT = ld/at32f403a_guest.ld
guest-faulttest: clean all
