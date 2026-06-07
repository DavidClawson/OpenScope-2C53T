# Makefile for the OpenScope high-recovery layout installer application.
#
# This builds a normal app-slot image at 0x08004000. The existing HID
# bootloader flashes it as an app; the installer then writes the high recovery
# bootloader first and the tiny stage0 sector last.

PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
CP      = $(PREFIX)objcopy
SZ      = $(PREFIX)size
NANO_SPECS := $(shell f="$(shell $(CC) -print-file-name=nano.specs 2>/dev/null)"; test "$$f" != "nano.specs" -a -f "$$f" && printf '%s' '-specs=nano.specs')

BUILD_DIR       = build
AT32_LIB        = ../at32f403a_lib/libraries
AT32_DRV        = $(AT32_LIB)/drivers
AT32_CMSIS      = $(AT32_LIB)/cmsis/cm4
STAGE0_BIN      ?= ../stage0/build/stage0.bin
HIGH_BOOT_BIN   ?= ../bootloader/build_high/bootloader_high.bin

C_SOURCES  = src/main.c
C_SOURCES += $(BUILD_DIR)/high_layout_payload.c
C_SOURCES += $(AT32_CMSIS)/device_support/system_at32f403a_407.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_crm.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_flash.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_misc.c

ASM_SOURCES = $(AT32_CMSIS)/device_support/startup/gcc/startup_at32f403a_407.s

C_INCLUDES  = -Iinc
C_INCLUDES += -I../bootloader/inc
C_INCLUDES += -I$(AT32_DRV)/inc
C_INCLUDES += -I$(AT32_CMSIS)/device_support
C_INCLUDES += -I$(AT32_CMSIS)/core_support

CPU = -mcpu=cortex-m4
FPU = -mfpu=fpv4-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

C_DEFS = -DAT32F403AVGT7

CFLAGS  = $(MCU) $(C_DEFS) $(C_INCLUDES)
CFLAGS += -Wall -Wextra -Wno-unused-parameter
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -std=gnu11
CFLAGS += -Os -g

ASFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES)

LDSCRIPT = ../ld/at32f403a_app.ld

LDFLAGS  = $(MCU)
LDFLAGS += $(NANO_SPECS)
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/high_layout_updater.map,--cref
LDFLAGS += -lc -lm -lnosys

TARGET = high_layout_updater

all: $(BUILD_DIR)/$(TARGET).bin size

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/high_layout_payload.c: $(STAGE0_BIN) $(HIGH_BOOT_BIN) Makefile | $(BUILD_DIR)
	@python3 ../../scripts/build_high_layout_payload.py --stage0 "$(STAGE0_BIN)" --high-bootloader "$(HIGH_BOOT_BIN)" --output "$@"

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile $(LDSCRIPT)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(CP) -O binary $< $@
	@echo "Built: $@ ($$(wc -c < $@ | tr -d ' ') bytes)"

$(BUILD_DIR):
	mkdir -p $@

size: $(BUILD_DIR)/$(TARGET).elf
	$(SZ) --format=berkeley $<

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean size
