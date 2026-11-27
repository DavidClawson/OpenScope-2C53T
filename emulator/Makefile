# OpenScope 2C53T: emulator tools (see README.md)

PKG_CONFIG_PATH ?= /opt/homebrew/lib/pkgconfig
SDL3_FLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags --libs sdl3)

lcd_viewer: lcd_viewer.c
	cc -O2 -o $@ $< $(SDL3_FLAGS) -Wall -Wextra

# Build the emulator image and the viewer, start Renode, open the viewer.
run:
	./run_emulator.sh

clean:
	rm -f lcd_viewer

.PHONY: clean run
