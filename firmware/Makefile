# Makefile for FNIRSI 2C53T Custom Firmware
# Target: GD32F307 (ARM Cortex-M4F @ 120MHz)

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
FW_DIR     = gd32f30x_lib/Firmware
GD32_CMSIS = $(FW_DIR)/CMSIS
GD32_STD   = $(FW_DIR)/GD32F30x_standard_peripheral
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
C_SOURCES += src/ui/scope_state.c
C_SOURCES += src/tasks/input_handler.c
C_SOURCES += src/tasks/measurement.c
C_SOURCES += src/drivers/flash_fs.c
C_SOURCES += src/drivers/esp_comm.c
C_SOURCES += src/drivers/watchdog.c
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

# GD32 CMSIS
C_SOURCES += $(GD32_CMSIS)/GD/GD32F30x/Source/system_gd32f30x.c

# GD32 Standard Peripheral Library (only what we need)
C_SOURCES += $(GD32_STD)/Source/gd32f30x_rcu.c
C_SOURCES += $(GD32_STD)/Source/gd32f30x_gpio.c
C_SOURCES += $(GD32_STD)/Source/gd32f30x_misc.c
C_SOURCES += $(GD32_STD)/Source/gd32f30x_pmu.c
C_SOURCES += $(GD32_STD)/Source/gd32f30x_fmc.c
C_SOURCES += $(GD32_STD)/Source/gd32f30x_fwdgt.c

# FreeRTOS kernel
C_SOURCES += $(FREERTOS)/tasks.c
C_SOURCES += $(FREERTOS)/queue.c
C_SOURCES += $(FREERTOS)/list.c
C_SOURCES += $(FREERTOS)/timers.c
C_SOURCES += $(FREERTOS)/portable/GCC/ARM_CM4F/port.c
C_SOURCES += $(FREERTOS)/portable/MemMang/heap_4.c

# Startup assembly (GD32F30x connectivity line - GCC variant)
ASM_SOURCES = $(GD32_CMSIS)/GD/GD32F30x/Source/GCC/startup_gd32f30x_cl.S

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
C_INCLUDES += -I$(GD32_CMSIS)
C_INCLUDES += -I$(GD32_CMSIS)/GD/GD32F30x/Include
C_INCLUDES += -I$(GD32_STD)/Include
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
C_DEFS  = -DGD32F30X_CL
C_DEFS += -DUSE_STDPERIPH_DRIVER
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
LDSCRIPT = ld/gd32f307.ld

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

# List of ASM objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.S=.o)))
vpath %.S $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.S Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile $(LDSCRIPT)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(CP) -O binary $< $@
	@echo "Built: $@ ($$(wc -c < $@ | tr -d ' ') bytes)"

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

# Build for emulator (skips HAL clock init that hangs without real hardware)
# Exclude system_gd32f30x.c since main.c provides its own SystemInit stub
EMU_EXCLUDE = system_gd32f30x.c
emu: CFLAGS += -DEMULATOR_BUILD
emu: C_SOURCES := $(filter-out %$(EMU_EXCLUDE),$(C_SOURCES))
emu: clean all

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

.PHONY: all clean disasm size renode soak-test soak-quick
