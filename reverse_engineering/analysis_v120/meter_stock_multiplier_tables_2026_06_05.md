# Stock V1.2.0 DMM Multiplier And Range Tables

Source boundary: this note uses only `archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin` (`sha256 a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760`), `full_decompile.c`, and stock disassembly/decompile around `FUN_08036AC0` and `FUN_080028E0`. It intentionally does not use the current custom decoder as evidence.

## Raw Value Extension

`FUN_08036AC0` extracts four decoded LCD/BCD digits from the USART2 DMM frame:

```text
0x08036BCA..0x08036BF4:
raw = d0*1000 + d1*100 + d2*10 + d3
```

Immediately after raw construction, stock loads the single-precision `10000.0f` literal from `0x08036C88` and conditionally keeps `raw + 10000.0f` when `frame[2].3` is set:

```text
0x08036BFC: vldr s2, [pc, #0x88]       ; 0x08036C88 = 10000.0f
0x08036C08: vadd.f32 s2, s0, s2
0x08036C0C: lsls.w r0, r8, #0x1c       ; r8 = frame[2], bit 3 into sign
0x08036C14: it pl
0x08036C16: vmovpl.f32 s2, s0          ; if frame[2].3 clear, discard +10000
0x08036C24: vstr s2, [meter_state+0xF30]
```

So the stock raw value is:

```text
extended_raw = d0*1000 + d1*100 + d2*10 + d3
if frame[2] & 0x08: extended_raw += 10000
```

This explains low DCV frames such as digits `2949` with `frame[2].3=1`: stock stores `12949`, not `2949`.

## Decimal Exponent Class

The class priority starts at `0x08036C1A` after the extended raw has been stored:

```text
0x08036C1A: ldrsb.w r0, [frame+8]
0x08036C28: bmi.w 0x08036ECA          ; frame[8].7 set -> class 4
0x08036C2C: lsls.w r0, frame[3], #0x1b
0x08036C34: bmi.w 0x0803711C          ; frame[3].4 set -> class 3
0x08036C38: lsls r0, frame[4], #0x1b
0x08036C42: bmi.w 0x0803712C          ; frame[4].4 set -> class 2
0x08036C4A: tst.w frame[5], #0x10
0x08036C52: vldr d1, [pc-relative]    ; frame[5].4 set -> class 1, else class 0
0x08037132: strb.w r0, [meter_state+0xF34]
0x0803713E: pow(10.0, class)
0x08037156: extended_raw / pow_result
0x0803715A: convert back to float
```

The PC-relative exponent table at `0x080373D0` is:

| Address | Bytes | Type | Value | Meaning |
| --- | --- | --- | ---: | --- |
| `0x080373D0` | `00 00 00 00 00 00 08 40` | double | `3.0` | `frame[3].4` |
| `0x080373D8` | `00 00 00 00 00 00 00 40` | double | `2.0` | `frame[4].4` |
| `0x080373E0` | `00 00 00 00 00 00 F0 3F` | double | `1.0` | `frame[5].4` |
| `0x080373E8` | `00 00 00 00 00 00 00 00` | double | `0.0` | default |
| `0x080373F0` | `00 00 00 42` | float | `32.0` | adjacent stock formatter/state literal |

The `frame[8].7` path loads `4.0` from `0x08036C90`, next to the `0x08036C60` literal pool.

The stock-visible conversion is therefore:

```text
class = 4 if frame[8] & 0x80
else class = 3 if frame[3] & 0x10
else class = 2 if frame[4] & 0x10
else class = 1 if frame[5] & 0x10
else class = 0

display_value = extended_raw / 10^class
```

Known voltage examples:

| Digits | Extension | Class bit | Value |
| ---: | --- | --- | ---: |
| `2949` | `frame[2].3=1` | `frame[8].7=1` | `1.2949 V` |
| `4979` | `frame[2].3=1` | `frame[8].7=1` | `1.4979 V` |
| `4994` | clear | `frame[3].4=1` | `4.994 V` |
| `3196` | clear | `frame[4].4=1` | `31.96 V` |

There is no stock-only evidence here for a one-point low-voltage coefficient. Treat any real per-device calibration as unresolved until recovered from stock xrefs, W25Q/system-file data, or the SPI3 bulk initialization table.

## Literal Pools

`FUN_08036AC0` literal pool at `0x08036C60`:

| Address | Bytes | Type | Value |
| --- | --- | --- | ---: |
| `0x08036C60` | `00 00 00 00 00 40 8F 40` | double | `1000.0` |
| `0x08036C68` | `00 00 00 00 00 00 E0 3F` | double | `0.5` |
| `0x08036C70` | `00 00 00 00 00 00 00 00` | double | `0.0` |
| `0x08036C78` | `00 00 00 00 00 00 59 40` | double | `100.0` |
| `0x08036C80` | `00 00 00 00 00 00 24 40` | double | `10.0` |
| `0x08036C88` | `00 40 1C 46` | float | `10000.0` |
| `0x08036C8C` | `00 BF 00 BF` | Thumb padding | `nop; nop` |
| `0x08036C90` | `00 00 00 00 00 00 10 40` | double | `4.0` |

`FUN_080028E0` formatter literals at `0x08002BF0`:

| Address | Bytes | Type | Value |
| --- | --- | --- | ---: |
| `0x08002BF0` | `00 00 00 00 00 40 8F 40` | double | `1000.0` |
| `0x08002BF8` | `00 00 00 00 00 00 00 00` | double | `0.0` |
| `0x08002C00` | `00 00 00 00 00 88 C3 40` | double | `10000.0` |
| `0x08002C08` | `00 00 7A 44` | float | `1000.0` |
| `0x08002C0C` | `00 00 C8 42` | float | `100.0` |

## Display Formatter

`FUN_080028E0` (`full_decompile.c` lines around the `0x080028E0` function) derives `DAT_2000102f`, `DAT_20001030`, and `DAT_20001026` after comparing the absolute display value against `10000.0` and then applying the formatter families:

- `DAT_2000102f` is the display decimal shift; the function repeatedly compares against `10000.0` and uses the `1000.0` literal during its decimal-shift loop.
- Formatter thresholds include `1000.0`, `100.0`, and `10.0`.
- `DAT_20001030` offsets:
  - DCV substate 1: `+0`
  - DCV substate 2/3: `+2`
  - ACV: `+0`
  - DCA small: `+0`
  - DCA large / ACA: `+2`
  - resistance: `+5`
  - mode 5: `+9`
  - modes 6/7: `+10`
- `DAT_20001026` unit indices:
  - DCV/ACV variant copy: `0/1/2`
  - DCA: `4` small, `3` large
  - ACA: `5`
  - resistance: `6`
  - mode 5: `7`
  - mode 6: `8/9`
  - mode 7: `10/11`

The display formatter dispatch guard in `scripts/test_stock_meter_literals.py`
now binary-pins the stock switch body at `0x08002AA0`, the mode-5 extended
case at `0x08002B20`, and the modes-6/7 unit-offset cases at `0x08002B34`.
This guards `DAT_20001026` unit index `7`, unit indices 8/9/10/11, and
`DAT_20001030` format offsets +9/+10 as display-formatter evidence.
It is not a runtime analog range writer and does not prove separate
capacitance-vs-temperature selector words.

The unit lookup boundary guard also pins the stock draw-call slice at
`0x08009AE4`, where the renderer computes `0x0804C40C + mode * 0x30 +
DAT_20001026 * 4`, and the first 48 bytes at `0x0804C40C`. In the downloaded
V1.2.0 APP image that zero-filled lookup region is not a valid in-image Thumb
pointer table. Therefore `DAT_20001026` is stock formatter state, but
`0x0804C40C` is not a recovered stock unit string table and must not be used as
proof for local unit suffix text.
