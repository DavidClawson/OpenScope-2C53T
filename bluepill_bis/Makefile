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

# CPUTAPID 0 disables OpenOCD's IDCODE check: most Blue Pills carry F103
# clones (bench unit reads 0x2ba01477, CS32-class, vs genuine 0x1ba01477).
# Fine for BIS-1/BIS-2 sequence work; already noted as a BIS-3 waveform caveat.
flash: build/rig.elf
	openocd -f interface/stlink.cfg -c "set CPUTAPID 0" \
	        -f target/stm32f1x.cfg \
	        -c "program build/rig.elf verify reset exit"

clean:
	rm -rf build

.PHONY: all flash clean
