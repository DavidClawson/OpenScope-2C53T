# Live DMM Visual Validation, 2026-06-05

This note records the first live gate that used the USB webcam as visual
evidence instead of a fixed bench expectation.

## Method

- Webcam evidence was captured with `scripts/validate_dmm_goal.py`.
- The source/load display was read from the saved frame by visual inspection
  with the image-view tool; no OCR, template matching, or image digit parser was
  used.
- The firmware LCD shadow was captured through the read-only `screen dumpbin`
  path.
- CDC DMM evidence came from `meter dump` after re-entering DCV.

Command:

```text
python3 scripts/validate_dmm_goal.py --skip-software \
  --observed-source-voltage 0.200 --voltage-tolerance 0.05 \
  --outdir tmp/dmm_goal_validation_live_0200_report
```

## Observation

The webcam frame shows the bench source/load display at:

```text
0.200 V
0.000 A
0.000 W
```

The OpenScope display in the same webcam frame shows about `0.4365 V`.

The CDC DMM steady-state dump from the same validation run reports:

```text
display=0.4368 unit=V valid=1 reject=0
frame=5A A5 44 8E EF E7 0F 24 80 00 01 8B
bcd_value=4368 decimal_pos=1
```

The DMM frontend state immediately before the run was stock-like DCV:

```text
selector=0514 apply=0000
PC11_meter_mux=1 PC7_probe=1
PC12_route=1 PE4=1 PE5=0 PE6=1
PA15=1 PA10=1 PB10=0 PB9=1 PA6=1
```

The `PB9=1 PA6=1` part of that dump is an earlier flashed local firmware
state, not stock PB9/PA6 evidence. Current stock V1.2.0 evidence only guards
PB9/PA6 output configuration at `0x080241D4..0x080241F0` and no direct
mode-specific BOP/BCR level write has been recovered. The current-head
stock-boundary policy keeps PB9/PA6 low through the tested mux-state model until
a stock trace proves otherwise. Do not treat PB9/PA6 high as a low-DCV
correction.

## Interpretation

This is a live mismatch, not a display-parser ambiguity. The source/load value
is visible in the webcam frame and the firmware LCD/CDC DMM values agree with
each other around `0.436x V`.

The local decoder is still following the current stock-disassembly evidence for
this frame shape: raw digits `4368`, stock class bit `frame[8].7`, no
`frame[2].3` raw extension, so the displayed value is `4368 / 10000`.

Do not "fix" this by adding a one-point coefficient for `0.200 V`. The remaining
possibilities are hardware/bench wiring, an unrecovered frontend/calibration
step, or missing FPGA-side initialization/calibration evidence. Any correction
must come from stock xrefs, W25Q/SPI bulk data, or repeatable live hardware
evidence across more than one input value and DMM mode.

## Current Gate Status

The software gate passes through `scripts/validate_dmm_goal.py --skip-live`.

The live visual gate fails:

```text
DCV mismatch: CDC=0.4368 V, visual_observed=0.2 V, tolerance=0.05 V
```

Artifacts:

```text
tmp/dmm_goal_validation_live_0200_report/webcam_source_load.jpg
tmp/dmm_goal_validation_live_0200_report/openscope_screen.bmp
tmp/dmm_goal_validation_live_0200_report/report.json
```

## ACV False-Confidence Regression

After flashing the `2026-06-05 23:37:30` OpenScope app build, the same visual
source/load state was rechecked. The webcam frame again showed:

```text
0.200 V
0.000 A
0.000 W
```

The live gate still failed DCV because the DMM frame decoded to about `0.4366 V`
instead of the visually observed `0.200 V`:

```text
display=0.4366 unit=V valid=1 reject=0
frame=5A A5 44 8E EF E7 07 24 80 00 01 89
```

However, ACV on the same DC input now fails closed with missing-AC-evidence
instead of rendering a confident voltage:

```text
display=--- unit= valid=0 reject=3
frame=5A A5 44 8E EF C7 07 24 80 00 01 88
```

This is why `frame[7]` bit 2 is not treated as AC-present evidence in the local
decoder. Stock evidence in `meter_acv_stock_case_2026_06_05.md` proves
`frame[7]` bit 0 as an ACV decimal-format selector; it does not prove bit 2 as
an AC-valid flag. The local AC confidence rule remains fail-closed until a stock
xref or repeatable live capture proves a better AC evidence bit.

Artifacts:

```text
tmp/dmm_goal_validation_live_after_acfix/webcam_source_load.jpg
tmp/dmm_goal_validation_live_after_acfix/openscope_screen.bmp
tmp/dmm_goal_validation_live_after_acfix/report.json
```
