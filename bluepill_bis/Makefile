# bluepill_bis — Blue Pill (STM32F103C8) config-transport bisect rig.
# Build: make      Flash: make flash   (ST-Link/V2 + OpenOCD)
CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size

CFLAGS  = -mcpu=cortex-m3 -mthumb -Os -g -Wall -Wextra \
          -ffreestanding -fno-builtin -ffunction-sections -fdata-sections
LDFLAGS = -nostdlib -T stm32f103c8.ld -Wl,--gc-sections -Wl,-Map=build/rig.map

all: build/rig.bin

build/rig.elf: src/rig.c stm32f103c8.ld | build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
	$(SIZE) $@

build/rig.bin: build/rig.elf
	$(OBJCOPY) -O binary $< $@

build:
	mkdir -p build

flash: build/rig.elf
	openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
	        -c "program build/rig.elf verify reset exit"

clean:
	rm -rf build

.PHONY: all flash clean
