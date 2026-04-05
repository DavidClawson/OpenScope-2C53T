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
C_SOURCES += src/drivers/dac_output.c
C_SOURCES += src/drivers/meter_data.c
C_SOURCES += src/dsp/signal_gen.c
C_SOURCES += src/dsp/shared_mem.c
C_SOURCES += src/ui/persistence.c
C_SOURCES += src/ui/theme.c
C_SOURCES += src/ui/xy_mode.c
C_SOURCES += src/ui/roll_mode.c
C_SOURCES += src/ui/trend_plot.c
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
    FFT_DEFS += -DUSE_CMSIS_DSP -DARM_MATH_CM4 -D__FPU_PRESENT=1
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
LDFLAGS += -specs=nano.specs
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
	$(CP) -O binary $< $@
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
	@printf '\xA5\x5A\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFE\x01\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF' > $@
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

# Flash app via USB HID bootloader (bootloader must be running)
flash: $(BUILD_DIR)/$(TARGET).bin
	uv run ../scripts/hid_flash.py $<

# Flash app via DFU (device must be in ROM DFU mode — BOOT0 + reset)
flash-dfu: $(BUILD_DIR)/$(TARGET).bin
	dfu-util -a 0 -d 2e3c:df11 -s 0x08004000:leave -D $<

# Build and flash the bootloader (one-time, via DFU)
bootloader:
	$(MAKE) -C bootloader

flash-bootloader:
	$(MAKE) -C bootloader
	dfu-util -a 0 -d 2e3c:df11 -s 0x08000000 -D bootloader/build/bootloader.bin

# Flash both bootloader + app in one go (via ROM DFU, for initial setup)
flash-all: $(BUILD_DIR)/$(TARGET).bin bootloader
	dfu-util -a 0 -d 2e3c:df11 -s 0x08000000 -D bootloader/build/bootloader.bin
	dfu-util -a 0 -d 2e3c:df11 -s 0x08004000:leave -D $(BUILD_DIR)/$(TARGET).bin

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

.PHONY: all clean disasm size renode soak-test soak-quick flash flash-dfu bootloader flash-bootloader flash-all
