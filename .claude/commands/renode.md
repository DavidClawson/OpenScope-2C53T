---
description: Build emulator firmware and launch Renode with LCD bridge
---

Build the firmware for emulator mode and run it in Renode. Optionally start the LCD bridge and React frontend for visual display.

Steps:
1. `cd firmware && make emu` — build with EMULATOR_BUILD defined
2. Find the Default_Handler address from the .map file: `arm-none-eabi-nm build/firmware.elf | grep Default_Handler`
3. Update the hook address in `emulator/renode/run_openscope.resc` if it changed
4. Launch Renode: `cd emulator/renode && /Applications/Renode.app/Contents/MacOS/renode run_openscope.resc`
5. If the user wants LCD visualization, also start the bridge and frontend:
   - LCD bridge: `cd emulator && python3 renode_lcd_bridge.py` (WebSocket on port 8765)
   - React frontend: `cd frontend && npm run dev` (opens browser)

For a quick smoke test without GUI: `cd firmware && make renode-test`

If the user says "with display" or "with LCD", start all three components.
If the user just says "/renode", do the smoke test and report the result.
