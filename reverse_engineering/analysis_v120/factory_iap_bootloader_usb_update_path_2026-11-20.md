# Factory IAP bootloader: how the USB update path actually behaves (entry, power latch, write handling)

**Date:** 2026-11-20
**Source:** static disassembly (capstone, Thumb-2) of
`archive/factory_iap_bootloader_2C53T.bin` (28,672 B, base `0x08000000`,
sha256 `0c9ec7d642d233ea09c87274867ad3460e1dbec37c4332f7edb8f836175630c7`),
plus host-side observations on one retail unit running stock V1.2.0, case
never opened, connected to a Windows host.
**Evidence level:** everything in sections 2, 3 and 5 is read from the code
and has **not** been exercised on hardware. Section 4 is observed, read-only:
nothing was written to the device. No flash, option-byte or protection
register was touched at any point.
**Why this matters:** the docs describe this path as "MENU + tap Power, a
FAT12 drive named `IAP` appears, drag the `.bin`". That is what the user
sees, but not what the bootloader does, and the gap explains three things
people run into: the tap that is too short, the drive that only appears
after Power is released, and hosts that corrupt the image by writing
anything other than the firmware file.

---

## 1. Summary

1. **MENU is sampled once**, right after reset, with a two-phase matrix scan
   on PE2/PE3. Not pressed: the bootloader jumps to `0x08007000`
   **unconditionally**. There is no vector, checksum or flag check.
2. **The bootloader drives the power-hold pin (PC9) low at start** and only
   drives it high after it has initialised the LCD and drawn
   `Firmware upgrade`. Until then the Power key itself keeps the unit alive.
3. **USB is not started while Power is held.** The bootloader waits for PC8
   to read high (key released) before it initialises USB. PC8 is therefore
   the power-key sense line, active low. It is listed as "Unknown" in
   `HARDWARE_PINOUT.md`.
4. **In upgrade mode, pressing Power again clears PC9**, so the unit switches
   off when the key is released.
5. **Writes are not interpreted as a filesystem.** The handler looks at the
   disk offset only to tell "root directory" from "data area". Every block
   written to the data area is programmed to flash **in arrival order**,
   starting at the target address, whatever file it belongs to.
6. **The only page outside the app slot that an update touches is the flag
   page**, `0x08006800-0x08006FFF`. The calibration half-page
   `0x08006000-0x080067FF` is a separate 2 KB page and no code path in this
   image erases or programs it.
7. **The completion flag `0x41544B38` at `0x08006800` is written but never
   read** by this bootloader.

---

## 2. Entry decision (`main` at `0x08002938`)

Order of the calls that matter:

| Address | What it does |
|---|---|
| `0x08001CD8` | Fills the IAP state struct at `0x20000414` (see section 6). |
| `0x08000934` | PC9 as push-pull output, then **cleared** (`GPIOC+0x14`, mask `0x200`). |
| `0x080004B0` | Scan phase 1: PA7, PB0, PC5, PE2 as inputs with pull; PA8, PC10, PE3 as outputs driven low. |
| `0x08002726` | Reads `GPIOE` (`0x40011800`) mask `0x04`: PE2 low counts as "pressed". |
| `0x080003F8` | Scan phase 2 (only if phase 1 saw the key): PA8, PC10, PE3 as inputs; PA7, PB0, PC5, PE2 as outputs driven low. |
| `0x08002726` | Reads `GPIOE` mask `0x08`: PE3 low confirms. |

Both phases must see the key. If either does not, `main` calls
`0x08002784` with the app address from the state struct (`0x08007000`).
That routine loads SP and PC from the image, disables and resets one
peripheral through CRM, and branches. It does not validate the vector
table, and nothing reads the flag page first.

Two consequences:

- A half-written app slot is jumped into as-is. This matches the
  dark-screen case already described in `docs/dfu_mode_guide.md` (EXP-57),
  where MENU held across a reset was the way back in.
- The key the bootloader looks for connects PE2 and PE3 directly: it is
  seen from both sides. In the matrix documented in `HARDWARE_PINOUT.md`
  ("Button Input"), CH2/Down/Save sit on PE3 against PA7/PC5/PB0 and
  Right/Auto sit on PE2 against PA8/PC10. PE2 x PE3 is the remaining
  crossing of those two groups, which is consistent with it being MENU.
  This is an inference from the scan pattern, not a board trace.

There is no timeout and no second sample. MENU has to be down at the moment
the scan runs, early in `main` and before the LCD is initialised.

---

## 3. Power latch, USB start, exit

After the entry decision, on the upgrade path:

| Address | What it does |
|---|---|
| `0x080006E0` | LCD init. |
| `0x080028CC` | Draws the string `Firmware upgrade`. |
| `0x080029B6` | **PC9 set** (`GPIOC+0x10`, mask `0x200`). First point where the unit holds its own power. |
| `0x080029BC`, `0x080029D2` | Two loops on `GPIOC` mask `0x100`, 10 ms apart: stay here **while PC8 reads low**. |
| `0x080029F8`-`0x08002A0A` | USB device init and connect (core `0x200001C8`, class table `0x200000FC`, descriptor table `0x20000124`). |
| `0x08002A10` | Main loop: status handling (`0x08001D2C`), then if PC8 reads low, **PC9 cleared**. |

Read as a sequence for the user:

1. With the unit off, hold MENU.
2. Press Power **and keep it pressed** until `Firmware upgrade` is on the
   screen. Releasing earlier removes power before the latch is set.
3. Release Power. Only now does the bootloader start USB, so the `IAP`
   drive cannot appear while the key is still down.
4. Pressing Power again switches the unit off.

How long step 2 takes was not measured. It is bounded by the LCD init.

On the app path the bootloader never sets PC9: it leaves it driven low and
relies on the application to raise it, which is what
`HARDWARE_PINOUT.md` already says about PC9 ("first operation in
`main()`").

---

## 4. What a host sees (observed, one V1.2.0 unit, Windows)

| | Upgrade mode (bootloader) | USB Sharing (application) |
|---|---|---|
| Disk name | `AT32 Msc iap` | `AT32 Disk0` |
| Bus-reported name | not captured | `AT32 Mass Storage` |
| USB ID | not captured | `VID_2E3C&PID_5720`, `REV_0200` |
| Disk size | 8,386,560 B | 14,680,064 B |
| Logical sector size | 2048 B | 4096 B |
| Volume | FAT, label `IAP`, 8,347,648 B, 8,339,456 B free | FAT, no label, 14,643,200 B |
| Contents | `Ready.TXT`, 0 bytes | `LOGO`, `Screenshot file`, `Screenshot simple file`, ... |

Notes:

- 8,386,560 / 2048 = 4095 sectors. The archived image carries the strings
  `Msc iap`, `IAP` and `Ready`/`TXT`, and a 4095-sector total shows up in
  its (compressed) initialised data. The archived unit-1 dump and this unit
  agree on everything that could be compared from the host.
- The application's drive reports **4096-byte** logical sectors on this
  unit, and 14,680,064 / 4096 = 3584 blocks. That is what the V1.2.0
  READ CAPACITY handler returns for LUN 0 (`0x080303D2`): block size
  4096, and 3584 blocks when `DAT_20001066` is 0 or 512 blocks otherwise.
  The 512 that `stock_iap_bootloader.md` listed as the block size is that
  second block count. Corrected there in this PR.
- The application's drive exists only while the full-screen "USB Sharing"
  image is shown. With USB Sharing off, the host sees no device at all.
- On this unit, "MENU + tap Power" with the cable attached did not reach
  upgrade mode. The sequence in section 3, started with the cable
  unplugged and plugging it in after releasing Power, did. Which of the two
  differences mattered (the length of the press or the cable) was not
  isolated.
- A managed Windows laptop refused the copy with "Access denied" although
  the volume was not read-only: Defender Device Control was enabled. Worth
  knowing before blaming the device.

---

## 5. How writes are consumed (static)

### 5.1 Dispatch by disk offset (`0x080020B0`)

The MSC write callback receives the byte offset on the virtual disk:

| Offset | Sector (2048 B) | Action |
|---|---|---|
| `0x0000`, `0x3000`, `0x3800` | 0, 6, 7 | Ignored (boot sector, the two FAT copies). |
| `0x4000` | 8 | Root directory: `0x08001C0C`. |
| anything else below `0x8800` | 1-5, 9-16 | Ignored. |
| `>= 0x8800` | 17 and up | Data area: `0x08001ED8`. |

The FAT is never consulted. The bootloader does not know which cluster
belongs to which file.

### 5.2 Root directory writes (`0x08001C0C`)

Starting at the third 32-byte entry, it looks for an entry whose attribute
byte is exactly `0x20` and whose **next** slot has attribute `0x00`, i.e.
the last plain file in the directory. That raw entry is copied to
`0x200003F4` (name at `+0`, extension at `+8`, size at `+0x1C`). If none is
found the copy is zeroed.

Every time such an entry is found, **the running offset is reset to 0**.

### 5.3 Data-area writes (`0x08001ED8`)

1. The first two words of the first data block seen are remembered
   (`0x20000080`, `0x20000084`) until a programming run succeeds.
2. The block is taken as firmware if the remembered extension is `BIN` or
   `bin`. Failing that, it is still taken as firmware if the two remembered
   words look like a vector table: `(w0 & 0xF0000000) == 0x20000000` and
   `(w1 & 0xFF000000) == 0x08000000`.
3. Otherwise, if the remembered name starts with `JUMP`, status becomes 7
   (jump to the app after about a second, nothing programmed). Anything
   else is dropped.
4. If the remembered size is 0 (data arrived before a directory entry with
   a size), the size is assumed to be the whole app slot and a "size
   pending" byte (`0x2000007C`) is set. The next root-directory write that
   brings a size closes the run if enough data has already been programmed.
5. On the first block of a run: the target address is chosen (5.4), the
   flag page is erased (5.5), status becomes 1.
6. If the size exceeds the app slot: status 4 (`Large`).
7. The block is programmed at `target + running offset` by `0x08002478`
   (page-erases as it goes when the address is page-aligned, upper bound
   checked against the end of flash), and the running offset advances by
   the block length. **The disk offset of the block is not used.**
8. When the running offset reaches the size: status 2, flag written.
   On a programming error: status 3.

Once status is 2 or 6, further data is ignored until the next power cycle.

So the bootloader treats the data area as a **stream**. It gets the right
image only if the host writes the firmware file's clusters in file order
and writes nothing else to the data area during the run. This is the
mechanism behind the existing warning about Finder: an AppleDouble `._`
file, a `.DS_Store` or an index file written to the data area while the
remembered extension is `BIN` is programmed into the app slot like any
other block, and a directory rewrite in the middle of the run restarts the
stream at offset 0.

### 5.4 Target address from the file name (`0x08001B74`)

If the 8.3 name starts with `A` followed by six hex digits, those digits
are taken as an offset from `0x08000000`, accepted only when
`0x7000 <= offset <= flash size`. The image is then programmed there and
the post-update jump goes there. Any other name gives the default
`0x08007000`.

`APP_2C53T_...bin` (short name `APP_2C~1.BIN`) and `openscope-...bin` fail
the hex test at the second character, so they take the default. The check
keeps this feature out of the bootloader and the calibration page. It is
still worth avoiding image names of the form `A0nnnnn.bin`.

### 5.5 Flag page

- Start of a run (`0x08001CB8`): erase the page at `app - page size` =
  `0x08006800`.
- Successful end (`0x0800208C`): program `0x41544B38` at `0x08006800`.
- Nothing in this image reads that word. The archived dump has it set
  (`38 4B 54 41` at file offset `0x6800`).

Page size is 2048 B when the flash-size register (`0x1FFFF7E0`) reads 256
or more, 1024 B otherwise. On this part that puts calibration
(`0x08006000-0x080067FF`) and the flag (`0x08006800-0x08006FFF`) in
different pages.

### 5.6 Status (`0x08001D2C`)

The status file is renamed to report progress. The name table is at
`0x08001DF4`.

| Status | File name | What follows |
|---|---|---|
| 0 | `Ready` | Waiting. |
| 1 | (unchanged) | Programming. |
| 2 | `Success` | USB re-enumerates, about 1 s later the bootloader jumps to the target. |
| 3 | `Failed` | USB re-enumerates. No jump. |
| 4 | `Large` | USB re-enumerates. No jump. |
| 5 | `Unkown` (sic) | No code path that sets it was found in the write handler. |
| 6 | (unchanged) | Idle after 2, 3 or 4. |
| 7 | `Jump` | About 1 s later the bootloader jumps to the target. |

---

## 6. IAP state struct (`0x20000414`)

| Offset | Meaning | Value on this part |
|---|---|---|
| `+0x00` | Flash base | `0x08000000` |
| `+0x04` | App address | `0x08007000` |
| `+0x08` | Flash size | flash-size register x 1024 |
| `+0x0C` | Max app size | flash size - `0x7000` |
| `+0x10` | Page size | `0x800` |
| `+0x14` | Page mask | `0x7FF` |
| `+0x24` | Target of the current run | `0x08007000` unless 5.4 applies |
| `+0x28` | Running offset | |
| `+0x2C` | Status | see 5.6 |

---

## 7. Not verified, open questions

- Nothing in sections 2, 3 and 5 was run on a device. In particular the
  `JUMP` file, the name-addressed target and the vector-table fallback are
  code readings only.
- Whether a short tap fails because of the power latch (section 3) or for
  another reason was not measured. A scope on PC9 during a MENU + Power
  start would settle it.
- What the unit does when Power is pressed with the charger already
  attached was not looked at. It may not be a cold start of the MCU, in
  which case the one-shot MENU sample in section 2 would not run.
- PC8 is identified from the bootloader only. The V1.2.0 application also
  tests `GPIOC` input bit 8 (for example at `0x0802B3BA`, image base
  `0x08007000`), but what it does with it was not analysed.
- MENU = PE2 x PE3 is an inference (section 2).
- The comparison in section 4 cannot tell whether the bootloader on a
  V1.2.0 retail unit is byte-identical to the archived unit-1 dump. Flash
  access protection prevents reading it out, and no attempt was made.
