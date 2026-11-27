# What each key does on a running stock unit (live, 2026-11-26 and 27)

Not static analysis: this is what one retail unit running stock V1.2.0 did when its keys
were pressed, read from its RAM over USB with the case closed
([fnirsi-2c53t-link](https://github.com/Metalaria/fnirsi-2c53t-link), a read-only HID
interface added to stock's own image). Offsets are into the state structure at
`0x200000F8`, as in [STATE_STRUCTURE.md](STATE_STRUCTURE.md).

## How it was taken

A host tool asked for one key every 5 s, wrote the request in the log, and logged every
change it saw, reading these blocks about every 65 ms:

| Block | Bytes |
|---|---|
| Signal generator settings | `0x20000F50`, 10 bytes (`+0xE58`) |
| Meter state | `+0xF2D` (menu entry), `+0xF2E` (function), `+0xF36` (variant), `+0xF3C` (HOLD) |
| UI | `+0xF60` to `+0xF6B` |
| Arrow keys' mode | `+0x354` |
| Oscilloscope state | `+0x00` to `+0x37` |

Each row below says where it comes from: **keys** is a change logged after the request
for that key; **owner** is what the owner saw at the unit, with no log behind it; **not
asked** means the key was not in that mode's log. "Nothing" means no change in the
blocks above and the link not lost. It does not say the key did nothing else. All
presses are short unless said. The logs:
[`sessions/2026-11-26_keys/`](https://github.com/Metalaria/fnirsi-2c53t-link/tree/main/sessions/2026-11-26_keys)
(the six `guided_*` files).

The key that this document calls **blue** is the one in the middle of the arrows.

## Multimeter

| Key | What it did | From |
|---|---|---|
| RIGHT, LEFT | `+0xF2D` one up, one down, going round at both ends (7 to 0, 0 to 7) | keys |
| MOVE | The other variant of the entry, both ways: DC and AC voltage in entry 1 (`+0xF2E` 1 and 2, `+0xF36` 1 and 2), continuity and diode in entry 2 (`+0xF2E` 3 and 4, `+0xF36` 2 and 1) | keys |
| AUTO | Back to entry 0, Automatic, from entry 1 and from entry 2 | keys |
| blue | `+0xF3C` (HOLD) 0 to 1 | keys |
| SELECT, TRIGGER, PRM, SAVE, CH1, CH2, UP, DOWN, MENU | Nothing | keys |
| MENU held a few seconds | The screen where the mode is chosen (below) | owner |

Entering an entry with RIGHT or LEFT, the log has: entry 1 with function 1 (DC Voltage)
and variant 1; entry 2 with function 3 (Continuity) and variant 2; entry 7 with function
10 (Large DC current) and variant 1.

## Signal generator

`+0xF69` is a level: 0 the list of waveforms, 1 the list of settings, 2 a value being
edited.

| `+0xF69` | Key | What it did | From |
|---|---|---|---|
| 0 | UP, DOWN | The waveform `+0xE59` one down, one up | keys |
| 0 | blue | `+0xF69` 0 to 1 | keys |
| 0 | MENU, RIGHT, LEFT | Nothing | keys |
| 1 | UP, DOWN | `+0xF6B` 2 to 0, 0 to 2. 1 was not seen: with the waveform that was selected the screen draws the duty row in grey, and it was not tried with another | keys |
| 1 | blue | `+0xF69` 1 to 2 | keys |
| 1 | MENU | `+0xF69` 1 to 0 | keys |
| 1 | RIGHT, LEFT | Nothing | keys |
| 2 | LEFT, RIGHT | The digit being edited, in the high nibble of `+0xF6B`: LEFT one up (0, 1, 2), RIGHT one down | keys |
| 2 | UP, DOWN | With `+0xF6B` low nibble 0: the frequency `+0xE5C` plus and minus one unit of that digit (1, 10 and 100 Hz logged) | keys |
| 2 | MENU | `+0xF69` 2 to 1 | keys |
| any | SELECT, AUTO, SAVE | Nothing seen on the screen | owner; not asked |
| any | MENU held a few seconds | The screen where the mode is chosen | owner |

In one log of two the request for LEFT at level 2 was followed by no change, and UP and
DOWN after it changed the frequency by 1.

## The screen where the mode is chosen

The link is lost while this screen is up (stock stops answering there), so nothing of it
is in a log. The owner, at the unit in Spanish: LEFT and RIGHT move between four modes,
in this order going right: multimeter, oscilloscope, signal generator, settings; the
blue key enters the one shown.

## Oscilloscope

`+0x354`: the low nibble is what the arrow keys do, and bit 7 the channel UP and DOWN
act on.

| Key | What it did | From |
|---|---|---|
| SELECT | `+0x354` low nibble 0, 1, 2, 0 | keys |
| MOVE | With both channels on: bit 7 of `+0x354` (`0x00` / `0x80`, `0x01` / `0x81`, `0x02` / `0x82`). With CH1 alone: nothing | keys |
| RIGHT, LEFT, nibble 0 | The time base `+0x2D` one index down, one up | keys |
| UP, DOWN, nibble 0 | The range of the channel bit 7 names (`+0x02` or `+0x03`) one index up, one down | keys |
| RIGHT, LEFT, nibble 1 or 2 | The horizontal position `+0x1A` one up, one down, and `+0x20` with it | keys |
| UP, DOWN, nibble 1 | The vertical position of the channel bit 7 names (`+0x04` or `+0x05`) one up, one down | keys |
| UP, DOWN, nibble 2 | The trigger level `+0x1C` one up, one down, whichever channel bit 7 names (trigger on CH1) | keys |
| blue | `+0x2E` 1 to 0 (stopped), and 0 to 1 | keys |
| AUTO | A sequence of steps in `+0x2F`, as logged: `01`, `02`, `03`, `11`, `12`, `13`, `FF`, 0 with CH1 alone (5.69 s); `01`, `02`, `03`, `11`, `21`, `32`, `F3`, `FF`, 0 with both channels on (7.20 s) | keys |
| CH1, CH2, TRIGGER, PRM | The link is lost 1.4 to 2.3 s after the request; a MENU press after each brought it back 2.5 to 3.1 s after its request | keys |
| SAVE | Not asked: it stores a capture | not asked |

The trigger level also moved without UP and DOWN in nibble 2: with the range and with
the position of CH1, the trigger's channel, and not with CH2's.

## Not covered

- A long press of any key but MENU, and MENU's only as the owner saw it.
- The keys inside the panels that CH1, CH2, TRIGGER and PRM open, and the arrows with
  the cursors on (`+0x354` nibble 3).
- The trigger on CH2.
- One unit, one firmware version, the instrument's language set to Spanish.
