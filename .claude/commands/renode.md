---
description: Build emulator firmware and launch Renode with LCD bridge
---

Build the firmware for emulator mode and run it in Renode.

Steps:
1. `cd firmware && make emu` — build with EMULATOR_BUILD defined
2. Find the Default_Handler address: `arm-none-eabi-nm build/firmware.elf | grep Default_Handler`
3. Update the hook address in `emulator/renode/run_openscope.resc` AND `emulator/renode/run_interactive.resc` if it changed
4. Choose the run mode:

**Quick smoke test** (default for `/renode`):
```
cd firmware && make renode-test
```
Runs 5 seconds headless, reports PC address. Success = PC in main loop area (0x080044xx), not a fault handler.

**Interactive with SDL3 viewer** (for `/renode` with display, or "interactive"):
```
# Terminal 1:
cd firmware && make renode-interactive
# Terminal 2:
cd emulator && ./lcd_viewer
```
LCD viewer keys: M=menu, arrows=navigate, Enter=OK, Space=select, P=PRM, A=auto, S=save/screenshot, 1=CH1, 2=CH2, Q=quit.
Screenshots saved to `/tmp/openscope_screenshot_NNN.bmp`.

**Display-only (no button input)**:
```
cd firmware && make renode
# In another terminal:
cd emulator && ./lcd_viewer
```

If the SDL viewer isn't built yet: `cd emulator && make` (requires SDL3: `brew install sdl3`).

If Renode fails with "Address already in use" on port 1234, kill the stale process: `lsof -ti :1234 | xargs kill`

If the user just says "/renode", do the smoke test and report the result.
If the user says "with display", "interactive", or "with LCD", use the interactive mode.
