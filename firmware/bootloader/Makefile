# Makefile for OpenScope 2C53T USB HID Bootloader
# Target: Artery AT32F403A (ARM Cortex-M4F @ 240MHz)
# Must fit in 16KB (0x08000000 - 0x08003FFF), flag sector at 0x08003800

######################################################################
# Toolchain
######################################################################
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
CP      = $(PREFIX)objcopy
SZ      = $(PREFIX)size
NANO_SPECS := $(shell f="$(shell $(CC) -print-file-name=nano.specs 2>/dev/null)"; test "$$f" != "nano.specs" -a -f "$$f" && printf '%s' '-specs=nano.specs')

######################################################################
# Paths
######################################################################
BUILD_DIR  = build
HIGH_BUILD_DIR = build_high
AT32_LIB   = ../at32f403a_lib/libraries
AT32_DRV   = $(AT32_LIB)/drivers
AT32_CMSIS = $(AT32_LIB)/cmsis/cm4
USBD_DRV   = ../at32f403a_lib/middlewares/usbd_drivers
USBD_CLASS = ../at32f403a_lib/middlewares/usbd_class/hid_iap

######################################################################
# Source files
######################################################################
C_SOURCES  = src/main.c
C_SOURCES += src/hid_iap_user.c
C_SOURCES += src/at32f403a_407_clock.c

# USB device core
C_SOURCES += $(USBD_DRV)/src/usbd_core.c
C_SOURCES += $(USBD_DRV)/src/usbd_int.c
C_SOURCES += $(USBD_DRV)/src/usbd_sdr.c

# USB HID IAP class (descriptors + class handler)
C_SOURCES += $(USBD_CLASS)/hid_iap_class.c
C_SOURCES += $(USBD_CLASS)/hid_iap_desc.c

# AT32 CMSIS + system
C_SOURCES += $(AT32_CMSIS)/device_support/system_at32f403a_407.c

# AT32 peripheral drivers (minimal set)
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_crm.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_gpio.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_flash.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_misc.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_usb.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_acc.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_crc.c
C_SOURCES += $(AT32_DRV)/src/at32f403a_407_wdt.c

# Startup assembly
ASM_SOURCES = $(AT32_CMSIS)/device_support/startup/gcc/startup_at32f403a_407.s

######################################################################
# Include paths
######################################################################
C_INCLUDES  = -Iinc
C_INCLUDES += -I$(AT32_DRV)/inc
C_INCLUDES += -I$(AT32_CMSIS)/device_support
C_INCLUDES += -I$(AT32_CMSIS)/core_support
C_INCLUDES += -I$(USBD_DRV)/inc
C_INCLUDES += -I$(USBD_CLASS)

######################################################################
# Compiler flags
######################################################################
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

######################################################################
# Linker flags
######################################################################
LDSCRIPT = bootloader.ld

LDFLAGS  = $(MCU)
LDFLAGS += $(NANO_SPECS)
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/bootloader.map,--cref
LDFLAGS += -lc -lm -lnosys

HIGH_CFLAGS = $(CFLAGS) -DBOOTLOADER_BASE_ADDRESS=0x080F0000 -DHIGH_IAP_ALLOW_LOW_FLASH=1
HIGH_ASFLAGS = $(ASFLAGS) -DBOOTLOADER_BASE_ADDRESS=0x080F0000
HIGH_LDFLAGS  = $(MCU)
HIGH_LDFLAGS += $(NANO_SPECS)
HIGH_LDFLAGS += -Tbootloader_high.ld
HIGH_LDFLAGS += -Wl,--gc-sections
HIGH_LDFLAGS += -Wl,-Map=$(HIGH_BUILD_DIR)/bootloader_high.map,--cref
HIGH_LDFLAGS += -lc -lm -lnosys

######################################################################
# Build targets
######################################################################
TARGET = bootloader
HIGH_TARGET = bootloader_high

all: $(BUILD_DIR)/$(TARGET).bin size
high: $(HIGH_BUILD_DIR)/$(HIGH_TARGET).bin high-size

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

HIGH_OBJECTS = $(addprefix $(HIGH_BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
HIGH_OBJECTS += $(addprefix $(HIGH_BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(HIGH_BUILD_DIR)/%.o: %.c Makefile | $(HIGH_BUILD_DIR)
	$(CC) -c $(HIGH_CFLAGS) $< -o $@

$(HIGH_BUILD_DIR)/%.o: %.s Makefile | $(HIGH_BUILD_DIR)
	$(AS) -c $(HIGH_ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile $(LDSCRIPT)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(HIGH_BUILD_DIR)/$(HIGH_TARGET).elf: $(HIGH_OBJECTS) Makefile bootloader_high.ld
	$(CC) $(HIGH_OBJECTS) $(HIGH_LDFLAGS) -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(CP) -O binary $< $@
	@echo "Built: $@ ($$(wc -c < $@ | tr -d ' ') bytes)"
	@SIZE=$$(wc -c < $@ | tr -d ' '); \
	if [ $$SIZE -gt 14336 ]; then \
		echo "WARNING: Bootloader is $$SIZE bytes, close to 16KB limit (flag sector at 0x3800)"; \
	fi; \
	if [ $$SIZE -gt 16384 ]; then \
		echo "ERROR: Bootloader is $$SIZE bytes, exceeds 16KB hard limit!"; \
		exit 1; \
	else \
		echo "OK: $$SIZE / 16384 bytes ($$(( $$SIZE * 100 / 16384 ))%% used)"; \
	fi

$(HIGH_BUILD_DIR)/$(HIGH_TARGET).bin: $(HIGH_BUILD_DIR)/$(HIGH_TARGET).elf
	$(CP) -O binary $< $@
	@echo "Built: $@ ($$(wc -c < $@ | tr -d ' ') bytes)"
	@SIZE=$$(wc -c < $@ | tr -d ' '); \
	if [ $$SIZE -gt 65536 ]; then \
		echo "ERROR: High bootloader is $$SIZE bytes, exceeds 64KB reserved region!"; \
		exit 1; \
	else \
		echo "OK: $$SIZE / 65536 bytes ($$(( $$SIZE * 100 / 65536 ))%% used)"; \
	fi

$(BUILD_DIR):
	mkdir -p $@

$(HIGH_BUILD_DIR):
	mkdir -p $@

size: $(BUILD_DIR)/$(TARGET).elf
	$(SZ) --format=berkeley $<

high-size: $(HIGH_BUILD_DIR)/$(HIGH_TARGET).elf
	$(SZ) --format=berkeley $<

clean:
	rm -rf $(BUILD_DIR) $(HIGH_BUILD_DIR)

# Flash bootloader via DFU (one-time, requires BOOT0 + reset)
flash-dfu: $(BUILD_DIR)/$(TARGET).bin
	dfu-util -a 0 -d 2e3c:df11 -s 0x08000000 -D $<

.PHONY: all high clean size high-size flash-dfu
