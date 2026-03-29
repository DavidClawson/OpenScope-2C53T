# OpenScope 2C53T — Emulator Tools

PKG_CONFIG_PATH ?= /opt/homebrew/lib/pkgconfig
SDL3_FLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags --libs sdl3)

lcd_viewer: lcd_viewer.c
	cc -O2 -o $@ $< $(SDL3_FLAGS) -Wall -Wextra

clean:
	rm -f lcd_viewer

.PHONY: clean
