# Meter Mode Raw Command Table, 2026-06-05

This note records the local extraction of the eight-byte stock meter-mode raw
command table referenced by the `0x080BB3FC` notes.

The archived V1.2.0 app binary is loaded at app-slot VMA `0x08004000`, while the
current decompile notes use addresses as if the app started at `0x08000000`.
For literal data, `raw_app_base_offset_2026_04_08.md` says to inspect
`runtime_literal - 0x4000` in the current project/raw image. Applying that to
`0x080BB3FC` gives raw address `0x080B43FC`, file offset `0x000B43FC`.

Extraction from `archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin`:

```text
14 0c 17 0b 0a 12 11 10
```

These bytes are the low byte of raw UART words of the form `0x0500 | table[i]`.
They are not display/update selector bytes from queue `0x20002D6C`.

Local port:

| Stock meter mode | Low byte | Raw word |
|---:|---:|---:|
| 0 | `0x14` | `0x0514` |
| 1 | `0x0C` | `0x050C` |
| 2 | `0x17` | `0x0517` |
| 3 | `0x0B` | `0x050B` |
| 4 | `0x0A` | `0x050A` |
| 5 | `0x12` | `0x0512` |
| 6 | `0x11` | `0x0511` |
| 7 | `0x10` | `0x0510` |
*** Update File: /home/kom/proj/OpenScope-2C53T/firmware/Makefile
@@
 C_SOURCES += src/drivers/fpga.c
+C_SOURCES += src/drivers/fpga_meter_plan.c
 C_SOURCES += src/drivers/fpga_scanner.c
@@
 test-meter-voltage-wave: $(BUILD_DIR)/test_meter_voltage_wave
 	$<
 
-test-meter: test-meter-data test-meter-voltage-wave
+$(BUILD_DIR)/test_fpga_meter_plan: tests/test_fpga_meter_plan.c src/drivers/fpga_meter_plan.c src/drivers/fpga_meter_plan.h | $(BUILD_DIR)
+	$(HOST_CC) -std=gnu11 -Wall -Wextra -Isrc/drivers $< src/drivers/fpga_meter_plan.c -o $@
+
+test-fpga-meter-plan: $(BUILD_DIR)/test_fpga_meter_plan
+	$<
+
+test-meter: test-meter-data test-meter-voltage-wave test-fpga-meter-plan
 
-.PHONY: all clean disasm size renode soak-test soak-quick flash flash-dfu bootloader flash-bootloader flash-all test-meter-data test-meter-voltage-wave test-meter
+.PHONY: all clean disasm size renode soak-test soak-quick flash flash-dfu bootloader flash-bootloader flash-all test-meter-data test-meter-voltage-wave test-fpga-meter-plan test-meter
