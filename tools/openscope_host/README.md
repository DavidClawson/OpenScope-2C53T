# openscope — host tool for the OpenScope 2C53T remote protocol

Implements the host side of [`docs/design/remote_protocol.md`](../../docs/design/remote_protocol.md) (issue #10).
Core needs only `pyserial`; screenshots need no extra packages either.

```bash
cd tools/openscope_host
python3 -m openscope ports              # which port it would use (USB 2e3c:5740)
python3 -m openscope info               # firmware, protocol, mode, battery, USB health
python3 -m openscope press MENU OK      # inject button presses
python3 -m openscope meter --count 0 --log m.csv   # log the multimeter (~4 Hz) until Ctrl-C
python3 -m openscope shell usbstat      # run one ASCII debug-shell command
python3 -m openscope screenshot s.png   # device framebuffer, CRC-checked
```

Exit codes: 0 ok, 1 no device (one-line message, no traceback), 2 device refused or error.

## Layers
| module | job |
|---|---|
| `proto.py` | framing, incremental `Decoder` (resyncs on garbage, never accepts a truncated frame), STATUS v1 codec |
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
`openscope/mcp_server.py` exposes `scope_info`, `scope_meter`, `scope_press`, `scope_screenshot` and `scope_shell` over MCP (stdio). It needs the `mcp` SDK (Python >= 3.10):

```bash
claude mcp add openscope -- uv run --no-project --python 3.12 --with mcp --with pyserial \
    --directory "$PWD" python -m openscope.mcp_server
```

The debug shell can erase flash (`fwapply`, `flash wtest`) or desynchronise the FPGA, so whoever starts the server picks how much of it `scope_shell` may reach with `--level` (append it to the command above). The lists live in `openscope/device.py`:

| `--level` | `scope_shell` accepts | use |
|---|---|---|
| `readonly` (default) | `READ_ONLY_SHELL`, exact: `version` `status` `uptime` `usbstat` `fwstat` `fwcrumb` `help` | any agent session |
| `bench` | readonly + `BENCH_SHELL`: scope/acquisition setters and reads the experiment scripts use (`fpga scope timebase\|range\|center\|vdiv\|trigmode\|level\|…`, `trig`, `trig2`, `mode`, the EXP acquisition knobs, `fpga scope measure\|freq`, `spi3 read\|frame`, `gpio read\|scan`, meter/cal/flash reads; `spi3 opread` only with the channel-read opcodes `04`/`05`, since others write FPGA registers or hit the config port) | experiments, someone nearby |
| `unsafe` | every shell command except the deny-list | bench work with a human watching |

Refused at **every** level (`NEVER_SHELL`): `fwload` `fwapply` `fwswap` `fwcrumb clear` `cal backup` `cal restore` `flash wtest` `mem write` `mode startup` `reboot` `gpio set` `gpio mode` `bench restore` `spi3 armtest` `fpga dbgclk` `fpga dbgarm` `fpga reinit` (its `<a-e><pin>` option pulses any pin LOW, `c9` = PC9 power hold), plus the reserved names `flash erase` `flash write` `iap` `dfu` `reset`; lines with control or non-ASCII characters (the firmware's line editor would rewrite them after the check) or over 127 characters; and the POWER button. Run those yourself with `python3 -m openscope shell …`. Every refusal reaches the agent as a tool error that says why and which level, if any, would allow it. `--allow-raw-shell` still works as a deprecated alias for `--level unsafe` (it warns on stderr and no longer lifts POWER).
