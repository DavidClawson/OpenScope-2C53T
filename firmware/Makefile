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

# FFT / DSP module
ifneq ($(findstring FFT,$(FEATURES)),)
  C_SOURCES += src/dsp/fft.c
  C_SOURCES += src/dsp/fft_test_signals.c
  C_DEFS += -DFEATURE_FFT
endif

# GD32 CMSIS
C_SOURCES += $(GD32_CMSIS)/GD/GD32F30x/Source/system_gd32f30x.c

# GD32 Standard Peripheral Library (only what we need)
C_SOURCES += $(GD32_STD)/Source/gd32f30x_rcu.c
C_SOURCES += $(GD32_STD)/Source/gd32f30x_gpio.c
C_SOURCES += $(GD32_STD)/Source/gd32f30x_misc.c
C_SOURCES += $(GD32_STD)/Source/gd32f30x_pmu.c
C_SOURCES += $(GD32_STD)/Source/gd32f30x_fmc.c

# FreeRTOS kernel
C_SOURCES += $(FREERTOS)/tasks.c
C_SOURCES += $(FREERTOS)/queue.c
C_SOURCES += $(FREERTOS)/list.c
C_SOURCES += $(FREERTOS)/timers.c
C_SOURCES += $(FREERTOS)/portable/GCC/ARM_CM4F/port.c
C_SOURCES += $(FREERTOS)/portable/MemMang/heap_4.c

# Startup assembly (GD32F30x connectivity line - GCC variant)
ASM_SOURCES = $(GD32_CMSIS)/GD/GD32F30x/Source/GCC/startup_gd32f30x_cl.S

######################################################################
# Include paths
######################################################################
C_INCLUDES  = -Iinclude
C_INCLUDES += -Isrc/drivers
C_INCLUDES += -Isrc/dsp
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

# Flash via Renode emulator (for testing)
renode: $(BUILD_DIR)/$(TARGET).bin
	/Applications/Renode.app/Contents/MacOS/renode --disable-gui -e \
		"mach create; \
		machine LoadPlatformDescription @../../emulator/renode/gd32f307_2c53t.repl; \
		sysbus LoadBinary @$< 0x08000000; \
		cpu VectorTableOffset 0x08000000; \
		start; sleep 3; quit"

.PHONY: all clean disasm size renode
