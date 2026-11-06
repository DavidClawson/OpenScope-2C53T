# openscope — host tool for the OpenScope 2C53T remote protocol

Implements the host side of [`docs/design/remote_protocol.md`](../../docs/design/remote_protocol.md) (issue #10).
Core needs only `pyserial`; screenshots need no extra packages either.

```bash
cd tools/openscope_host
python3 -m openscope ports              # which port it would use (USB 2e3c:5740)
python3 -m openscope info               # firmware, protocol, mode, battery, USB health
python3 -m openscope press MENU OK      # inject button presses
python3 -m openscope meter --count 0 --log m.csv   # log the multimeter (~4 Hz) until Ctrl-C
python3 -m openscope scope --frames 10 --channels 1,2 --out cap.csv   # captures (raw counts); .npz needs numpy
python3 -m openscope shell usbstat      # run one ASCII debug-shell command
python3 -m openscope screenshot s.png   # device framebuffer, CRC-checked
```

Exit codes: 0 ok, 1 no device (one-line message, no traceback), 2 device refused or error.

Waveforms are raw unsigned ADC counts. A frame carries the bench-measured sample rate and
volts/div of its timebase code and range (bench unit #1, 0 where none was measured) and flags
saying how far to trust them; there is no per-unit calibration and no measured zero point, so
counts give amplitudes (Vpp), not absolute volts. `NO_CAPTURE_DATA` means the scope has no real
capture — it never sends its demo trace. `scope --frames N` counts only new captures (new
`frame_id`) and gives up, keeping what it got, when acquisition is stopped or held.

## Layers
| module | job |
|---|---|
| `proto.py` | framing, incremental `Decoder` (resyncs on garbage, never accepts a truncated frame), STATUS / METER / WAVEFORM codecs, waveform summary |
| `link.py` | port discovery by VID:PID, reopen after the port vanishes |
| `device.py` | one method per request; stateless; one retry after a replug (e.g. the firmware's CDC self-heal, #39) |
| `cli.py` | the `openscope` command |

## Tests (no hardware)
```bash
python3 tests/test_proto.py        # framing + rejection paths + a mutation
python3 tests/test_end_to_end.py   # host tool vs the REAL firmware esp_comm.c (compiled into a shim, via ctypes)
```
The end-to-end suite checks the §6 acceptance criteria against the firmware's own parser and encoder: `info` shows the live mode and battery and follows a mode change, "no device" is a clean exit 1, button presses reach the injector, a full input queue is a NAK and not a false success, the ASCII shell keeps working next to the protocol, and an abandoned frame does not poison the next request.

## MCP server (LLM agents)
`openscope/mcp_server.py` exposes `scope_info`, `scope_meter`, `scope_waveform`, `scope_press`, `scope_screenshot` and `scope_shell` over MCP (stdio). It needs the `mcp` SDK (Python >= 3.10):

```bash
claude mcp add openscope -- uv run --no-project --python 3.12 --with mcp --with pyserial \
    --directory "$PWD" python -m openscope.mcp_server
```

It is safe by default: the debug shell can erase flash (`fwapply`, `flash wtest`) or desynchronise the FPGA, so only read-only commands are reachable, and POWER is refused. `--allow-raw-shell` lifts both for supervised bench work.
