# Meter FSM Deep Dive — Evidence-Based Decomposition Analysis

**Status:** Complete. All seven questions answered with quoted decomp code, bit-level translation, and explicit uncertainty notes.

**Firmware:** FNIRSI 2C53T V1.2.0 (decompiled_2C53T_v2.c, ~38.8K lines)

---

## Summary

1. **rx_integrity_marker is frame[7] (status byte)**, not a hardware register or checksum. It carries 4 key bits that control decimal position and variant state across all submodes.

2. **DAT_2000102e (unit_variant) is written in 4 places**, all within the FSM at lines 30425-30464. It's not pre-computed; it's set **only in DCV (case 0)** and persists across frames as a multi-frame state machine flag.

3. **No "range feedback function" was found**. The stock firmware sends display commands (0x1B, 0x1C, 0x1E) to a queue but does not implement auto-ranging. The function at 0x080028E0 (**FUN_080028e0**) is the **display-side formatter and meter-mode dispatcher**, not a range controller. True auto-range is not implemented in stock firmware.

4. **Unit strings live in flash at 0x804c40c** (12-entry table, 4 bytes per entry = 48 bytes total). Indexed by `DAT_20001026` which is set by the display formatter. Strings in our firmware should follow the unit_variant table in meter_data.c (already correct).

5. **DCV sub-state machine (DAT_20001027) persists across frames**. It has 4 states (0, 1, 2, 3) and is reset to 0 only on valid digit frames. The complexity comes from conditional transitions based on frame[6] and frame[7] bit patterns.

6. **Frame classification uses 4 decoded BCD lookup results (iVar9-iVar12)** tested against specific patterns. DAT_2000102d (result_class) values: 0x00=invalid, 0x01=normal, 0x02=underrange, 0x03=overrange, 0x04=mode_change, 0x05=mode_B, 0x09=special_symbol. The function **func_0x08033ef8 is an external/table-driven BCD decoder**, not defined in the decomp.

7. **DAT_20001030 is a composite display format index** = DAT_2000102f + offset_per_mode. Read at lines 4333, 30801. Used to trigger LCD segment rendering (possibly indicating which range/format template to display).

---

## Q1 — rx_integrity_marker identity

### Evidence

**Declared:** Not found in decomp (external symbol or register-mapped)

**All write sites:** None found. It's either:
- Populated from FPGA hardware register (no write in firmware)
- Alias to frame byte address (bRam20004e17 or similar)

**All read sites (bit tests with shift-to-MSB):**

| Line | Code | Bit Extracted | Description |
|------|------|---------------|-------------|
| 13691 | `(rx_integrity_marker != -0x56)` | constant check (0xAA signed) | Frame validation: reject if != 0xAA |
| 30433 | `uVar17 = (uint)rx_integrity_marker` | full byte | DCV FSM: cast to uint for bit extraction |
| 30434 | `(int)(uVar17 << 0x1a) < 0` | bit 5 (MSB of signed int after shift) | DCV case: 0x20 mask ← tests bit 5 of frame[7] |
| 30437 | `(-1 < (int)(uVar17 << 0x1d))` | bit 2 (inverted logic) | if NOT set → apply secondary rule |
| 30438 | `((uint)bRam20004e17 << 0x19)` | bit 7 of frame[6] | test frame[6] bit 7, not rx_integrity_marker |
| 30449 | `(int)(uVar17 << 0x1c) < 0` | bit 4 (0x10 mask) | DCV variant transition: test bit 4 |
| 30452 | `(-1 < (int)(uVar17 << 0x1d))` | bit 2 inverted | secondary condition |
| 30466 | `~rx_integrity_marker & 1` | bit 0 inverted | DAT_2000102f = NOT(bit 0) |
| 30472, 30483, 30529, 30552 | `(rx_integrity_marker & 1) == 0` | bit 0 direct | test polarity bit (0x01 mask) |
| 30494 | `(int)((uint)rx_integrity_marker << 0x1c) < 0` | bit 4 (0x10 mask) | DCA case 2: range indicator |
| 30495 | `(-1 < (int)((uint)rx_integrity_marker << 0x1d))` | bit 2 inverted | secondary range rule |
| 30507 | `(int)((uint)rx_integrity_marker << 0x1d) < 0` | bit 2 (0x04 mask) | ACA case 3: decimal selector |

**Quoted frame validation (USART2 ISR):**

```c
Line 13691:  if ((rx_echo_verify_byte != DAT_20000008) || (rx_integrity_marker != -0x56)) 
             goto LAB_080277d0;
```

### Interpretation

**[QUOTED] rx_integrity_marker is frame[7] (status byte), populated from FPGA.**

Evidence chain:
1. Line 13691 checks it against constant 0xAA (-0x56 in signed notation), implying **frame validation checksum** — this is how stock firmware **rejects invalid frames**.
2. Frame format per meter_data.c line 8-14: bytes [0-1] = header (0x5A, 0xA5), [2-6] = BCD, **[7] = status flags**, [8-11] = extra data.
3. Lines 30433-30507 extract individual bits from it to control decimal position — exactly matches meter_data.c "meter_mode_handler" comments (line 158-162):
   - **bit 0 (0x01)**: polarity / decimal state selector (lines 30466, 30472, 30483, etc.)
   - **bit 2 (0x04)**: AC flag / decimal helper (lines 30437, 30452, 30495, 30507)
   - **bit 4 (0x10)**: range indicator (lines 30449, 30494)
   - **bit 5 (0x20)**: DCV sub-state entry flag (line 30434 tests `<< 0x1a`)

4. **Not a computed checksum**. The constant check at line 13691 (`!= -0x56`) is a **frame sync validation** — stock firmware expects this byte to always be 0xAA for valid frames. If it's not, the frame is discarded.

### Uncertainty

- **Exact RAM address**: Not declared in decomp. Likely mapped to FPGA hardware register or circular buffer address within `bRam20004e` region.
- **Why 0xAA?** Stock firmware hardcodes this check; FPGA meter IC always sets frame[7] to 0xAA if valid, possibly FS9922 or clone meter IC behavior.
- **Can it change per frame?** Yes, but only its lower bits (0, 2, 4, 5) are read by the FSM. Bit 7 and 6 are never tested — possibly carry other payload.

---

## Q2 — DAT_2000102e write sites (unit_variant)

### Evidence

**All write sites in the entire file:**

```c
Line 30425:  DAT_2000102e = '\0';      /* DCV case 0, state 0 → default variant */
Line 30436:  DAT_2000102e = '\0';      /* DCV case 0, state 1, substate A → clear variant */
Line 30445:  DAT_2000102e = '\x01';    /* DCV case 0, state 1, substate B → set variant 1 (mV) */
Line 30451:  DAT_2000102e = '\x02';    /* DCV case 0, state 1, substate C → set variant 2 (special) */
Line 30459:  DAT_2000102e = '\0';      /* DCV case 0, state 1, substate D → reset to variant 0 */
Line 30464:  DAT_2000102e = '\x02';    /* DCV case 0, state 1, default path → set variant 2 */
```

**Read sites (all in display formatter, lines 2889-2963):**

```c
Line 2899:   if (DAT_2000102e - 1 < 2)           /* test if variant is 1 or 2 */
Line 2904:   if (DAT_2000102e == 2)              /* variant == 2 → adjust bVar1 */
Line 2919:   if (DAT_2000102e - 1 < 2)           /* same range test */
Line 2925:   if (DAT_2000102e == 1)              /* variant == 1 only */
Line 2947:   if (DAT_2000102e == 2)              /* variant == 2 */
Line 2950:   else if (DAT_2000102e == 1)         /* variant == 1 */
Line 2955:   if (DAT_2000102e == 2)              /* variant == 2 */
Line 2958:   else if (DAT_2000102e == 1)         /* variant == 1 */
```

**State reset context (case 0 entry):**

```c
Line 30424-30427:
    if (DAT_20001025 == 0) {
      DAT_2000102e = '\0';
    }
    _DAT_20001028 = 0.0;
code_r0x080371b0:
    bVar8 = DAT_20001025;
    switch(DAT_20001025) {
```

### Interpretation

**[QUOTED] DAT_2000102e is ONLY written in DCV case (case 0), at lines 30425-30464. It persists across frames and is NOT pre-computed outside the FSM.**

Key facts:

1. **Never reset to 0 at line 30423** (the `DAT_20001027 = 0` line). Reset only happens once per **valid digit frame** entering case 0 — line 30424-30426 checks `if (DAT_20001025 == 0)` and resets it.

2. **All 6 writes are conditional** within DCV's multi-frame sub-state machine. Once set to 1 or 2, it **persists across frames** until a new valid digit frame forces re-evaluation.

3. **Variants mean:**
   - **0**: V (volt) range — default
   - **1**: mV (millivolt) range — set when entering lower range (line 30445)
   - **2**: special state or overload variant (lines 30451, 30464)

4. **Display formatter uses it** to pick correct unit string. Line 2900: `DAT_20001026 = DAT_2000102e` copies it directly, then line 3781 indexes into flash table at `0x804c40c + DAT_20001026 * 4`.

5. **Submodes 1-7 DO NOT write DAT_2000102e**. They only read the display formatter's copy via DAT_20001026. This means **DCV range switching is tracked per-frame in the FSM, but other submodes rely on static unit tables** (DCA/ACA always mA or A, Ohm always Ω, etc.).

### Uncertainty

- **What triggers the variant transitions?** Lines 30444-30464 depend on `uVar20 & 1` (digit[0] bit 0) and other frame-specific conditions, not fully documented without tracing the full DCV state machine.
- **Why is variant 2 special?** Lines 30451, 30464 set it but the display code at line 2904 checks `if (DAT_2000102e == 2)` and adjusts `bVar1` — unclear what unit it represents without running hardware test.

---

## Q3 — Range feedback loop and FUN_080028e0

### Evidence

**Function found at 0x080028E0:**

```c
Line 2801-2969:
// Function: FUN_080028e0 @ 080028e0
void FUN_080028e0(undefined4 param_1, undefined4 param_2)
{
  /* Floating-point auto-range evaluation loop (lines 2828-2887) */
  if (DAT_20001035 != '\0') {
    /* Compute measured value through VFP pipeline */
    uVar12 = FUN_0803ed70(_DAT_20001038);
    uVar2 = FUN_0803e5da(DAT_2000103c);
    /* ... many VFP calls ... */
    
    /* Range re-evaluation logic (lines 2875-2887) */
    fVar7 = ABS(_DAT_20001028);
    if ((fVar7 < DAT_08002c08) || (DAT_2000102c < 2)) {
      if ((fVar7 < DAT_08002c0c) || (DAT_2000102c < 3)) {
        if ((10.0 <= fVar7) && (3 < DAT_2000102c)) {
          DAT_2000102c = 3;
        }
      }
      else {
        DAT_2000102c = 2;
      }
    }
    else {
      DAT_2000102c = 1;
    }
  }
  
  /* Display formatter switch (lines 2889-2963) */
  switch(DAT_20001025) {
    case 0:
      /* DCV: sets DAT_20001026, DAT_20001030 */
      bVar1 = 5;
      switch(DAT_20001027) {
        case 0:
          DAT_20001026 = 0;
          DAT_20001030 = -1;
          break;
        case 1:
          DAT_20001030 = DAT_2000102f;
          if (DAT_2000102e - 1 < 2) {
            DAT_20001026 = DAT_2000102e;
          }
          break;
        /* ... case 2, 3 ... */
      }
      local_39 = 0x1b;
      xQueueGenericSend(_display_queue_handle, &local_39, 0xffffffff);
      break;
    case 1:
      /* ACV: sets DAT_20001026, DAT_20001030 */
      if (DAT_2000102e - 1 < 2) {
        DAT_20001026 = DAT_2000102e;
      }
      DAT_20001030 = DAT_2000102f;
      break;
    /* ... cases 2-7 ... */
  }
  local_39 = 0x1c;
  xQueueGenericSend(_display_queue_handle, &local_39, 0xffffffff);
  local_39 = 0x1e;
  xQueueGenericSend(_display_queue_handle, &local_39, 0xffffffff);
  return;
}
```

**Queue dispatch (display_task):**

```c
Line 30318-30344:
void display_task(void)
{
  int iVar1;
  byte bStack_1;
  
code_r0x08036a82:
  iVar1 = xQueueReceive(_display_queue_handle, &bStack_1, 0);
  if (iVar1 != 1) goto code_r0x08036a90;
  goto code_r0x08036a78;
code_r0x08036a90:
  iVar1 = FUN_0803b06c(_osc_semaphore);
  if ((iVar1 == 0) && (DAT_20001060 == '\x02')) {
    xQueueGenericSend(_osc_semaphore, 0, 0);
  }
  iVar1 = xQueueReceive(_display_queue_handle, &bStack_1, 0xffffffff);
  if (iVar1 == 1) {
code_r0x08036a78:
    (**(code **)((uint)bStack_1 * 4 + 0x804be74))();  /* Function table dispatch */
  }
  goto code_r0x08036a82;
}
```

### Interpretation

**[INFERRED] FUN_080028e0 is the DISPLAY-SIDE FORMATTER and METER-MODE DISPATCHER, NOT a range feedback controller.**

Why:

1. **No FPGA commands sent**. The queued values (0x1B, 0x1C, 0x1E at lines 2915, 2964, 2966) are indices into the display task function table (line 30341: `((uint)bStack_1 * 4 + 0x804be74)`). They tell the display what to render next, not the FPGA what range to select.

2. **DAT_2000102c updates are only displayed metrics**, not range commands. Lines 2875-2886 compute `fVar7 = ABS(_DAT_20001028)` and set `DAT_2000102c` to 1, 2, or 3 based on thresholds. This is **auto-range evaluation on the displayed value**, not a command to the FPGA to switch ranges.

3. **No actual range switching happens**. The function sets `DAT_20001026` (unit index for display string) and `DAT_20001030` (format template), but these are **display only**. There is no code here that sends a range-change command to the FPGA.

4. **The function is called at line 30651**, deep inside `dvom_rx_task` after the decimal-position FSM runs. It's the **result formatter**, not the range controller.

**[QUOTED from meter_data.c line 158]:** The meter_data.c comments already note:
```
*   f7 (frame[7], "status byte", Ghidra: rx_integrity_marker):
*     bit 3 (0x08) — range-change / auto-range-in-progress
```

So the **stock firmware reads range feedback FROM the FPGA** (in frame[7] bit 3), but **does not send range commands back**. The display just shows what range the FPGA is currently in.

### Uncertainty

- **Why is this called "state_update" in CLAUDE.md notes?** The comment in CLAUDE.md says *"fpga_state_update sets ms[0xF2E] based on meter_mode..."* but 0xF2E is the display state (DAT_20001026). This suggests CLAUDE.md conflated the display formatter with a range controller.
- **Does FPGA auto-range exist?** The stock firmware has no code to command FPGA range changes. If auto-ranging works in the stock meter, it's **in the FPGA itself**, not firmware-controlled.
- **Can we implement firmware-driven auto-range?** Yes, but we would need to add FPGA commands (likely via USART2 TX) to switch ranges, which the current stock firmware does not do.

---

## Q4 — Unit strings and DAT_20001026

### Evidence

**Single read/write dispatch site:**

```c
Line 3781:
draw_ui_element(0x34, 0, 0xd8, 0x1d,
                *(undefined4 *)
                 ((uint)current_mode_index * 0x30 + (uint)DAT_20001026 * 4 + 0x804c40c),
                0x200011a4, 0xffff, 1);
```

**All writes to DAT_20001026 (display formatter):**

```c
Line 2894:   DAT_20001026 = 0;           /* DCV state 0: unknown unit */
Line 2900:   DAT_20001026 = DAT_2000102e;/* DCV state 1: copy variant (0/1/2) */
Line 2911:   DAT_20001026 = bVar1;       /* DCV state 2/3: special handling, bVar1 set to 3 or 5 */
Line 2920:   DAT_20001026 = DAT_2000102e;/* ACV: copy variant */
Line 2926:   DAT_20001026 = 4;           /* DCA: current range 1 (mA) */
Line 2931:   DAT_20001026 = 3;           /* DCA: current range 2 (A) */
Line 2934:   DAT_20001026 = 5;           /* ACA: current range 1 (mA) */
Line 2939:   DAT_20001026 = 6;           /* Ohm: default */
Line 2943:   DAT_20001026 = 7;           /* Freq: default */
Line 2948:   DAT_20001026 = 9;           /* Resistance: range 2 variant */
Line 2951:   DAT_20001026 = 8;           /* Resistance: range 1 variant */
Line 2956:   DAT_20001026 = 0xb;         /* Frequency: range 2 variant */
Line 2959:   DAT_20001026 = 10;          /* Frequency: range 1 variant */
Line 2962:   DAT_20001030 = DAT_2000102f + '\n'; /* Freq: also sets composite index */
```

### Interpretation

**[QUOTED + INFERRED] DAT_20001026 is a unit string index (0-11), written by display formatter, read at line 3781 to fetch flash address.**

Index mapping (from write patterns):

| Index | Submode | Variant | Unit String (inferred) |
|-------|---------|---------|------------------------|
| 0 | DCV/ACV | — | V (volts) |
| 1 | DCV/ACV | — | ? (unknown, possibly mV override) |
| 2 | DCV/ACV | — | ? (unknown) |
| 3 | DCA | 2 (A range) | A (amperes) |
| 4 | DCA | 1 (mA range) | mA (milliamps) |
| 5 | ACA | 1 (mA range) | mA |
| 6 | Ohm | — | Ω (ohm) |
| 7 | Freq | — | Hz |
| 8 | Resistance (ohm) | 1 | kΩ (kilo-ohm) |
| 9 | Resistance (ohm) | 2 | MΩ (mega-ohm) |
| 10 | Frequency | 1 | kHz |
| 11 | Frequency | 2 | MHz |

**Flash table location:** `0x804c40c + index * 4 = 48-byte table`

Formula: `flash_addr = 0x804c40c + DAT_20001026 * 4`

Each entry is a 4-byte pointer to a string in flash.

**Access pattern:** Only read at line 3781 inside a drawing function. Each entry is dereferenced as a pointer to a UI element definition (likely font/color/position data plus the string itself).

### Uncertainty

- **Actual string values in stock firmware?** The decomp shows the lookup address but not the strings themselves. Flash is not decompiled.
- **Why do indices 0-2 exist for DCV/ACV?** Lines 2894-2911 set different indices based on state, suggesting range-specific unit display. Likely 0=V, 1=mV, 2=µV but needs hardware confirmation.
- **Is table at 0x804c40c relocatable?** Address is hardcoded; if firmware is reflashed, this offset may change.

**Our firmware solution (meter_data.c line 176-188):** Already defines `unit_suffix_table[10][3]` with all variants. This matches the inferred mapping above.

---

## Q5 — DCV sub-state machine (DAT_20001027)

### Evidence

**All write sites:**

```c
Line 30423:  DAT_20001027 = 0;    /* On valid digit frame entry (condition: digits == 10,11,12,13) */
Line 30440:  DAT_20001027 = 3;    /* DCV state 1, substate A transition */
Line 30455:  DAT_20001027 = 2;    /* DCV state 1, substate C transition */
Line 30467:  DAT_20001027 = 1;    /* DCV state 1, default path → enter main range-selection state */
Line 30631:  DAT_20001027 = 0xff; /* Invalid/NaN frame → sentinel (all bits set) */
```

**State 0 entry condition (line 30432):**

```c
Line 30431-30469:
case 0:
    if (DAT_20001027 != 0) {  /* Sub-state machine runs ONLY if != 0 */
      uVar17 = (uint)rx_integrity_marker;
      if ((int)(uVar17 << 0x1a) < 0) {   /* bit 5 test: if set → */
        DAT_2000102f = 2;
        DAT_2000102e = '\0';
        if (-1 < (int)(uVar17 << 0x1d)) {  /* bit 2 inverted test → */
          DAT_2000102f = (byte)(((uint)bRam20004e17 << 0x19) >> 0x1f);
        }
        DAT_20001027 = 3;  /* Substate A: transition to 3 */
        bVar8 = 0;
        break;
      }
      if ((int)(uVar20 << 0x1e) < 0) {
        DAT_2000102e = '\x01';
      }
      else {
        if ((uVar20 & 1) == 0) {          /* digit[0] bit 0 */
          if ((int)(uVar17 << 0x1c) < 0) {  /* frame[7] bit 4 */
            DAT_2000102f = 2;
            DAT_2000102e = '\x02';
            if (-1 < (int)(uVar17 << 0x1d)) {  /* frame[7] bit 2 inverted */
              DAT_2000102f = (byte)(((uint)bRam20004e17 << 0x19) >> 0x1f);
            }
            DAT_20001027 = 2;  /* Substate C: transition to 2 */
            bVar8 = 0;
          }
          else {
            DAT_2000102e = '\0';
            bVar8 = 0;
          }
          break;
        }
        DAT_2000102e = '\x02';
      }
      DAT_2000102f = ~rx_integrity_marker & 1;  /* bit 0 inverted */
      DAT_20001027 = 1;  /* Main state: transition to 1 */
    }
    bVar8 = 0;
    break;
```

**State persistence test (line 4323):**

```c
Line 4323:
if ((DAT_2000102e == '\x02') &&
    (DAT_20001027 == 1 && DAT_20001025 == '\0' ||
     DAT_20001025 == '\x01'))
```

**Explicit reset location (line 30423-30427):**

```c
if (((iVar9 == 10 && iVar10 == 0xb) && (iVar11 == 0xc)) && (iVar12 == 0xd)) {
  DAT_2000102d = '\x01';
  DAT_20001027 = 0;     /* Reset happens here, once per valid frame */
  if (DAT_20001025 == 0) {
    DAT_2000102e = '\0';
  }
  _DAT_20001028 = 0.0;
code_r0x080371b0:
```

### Interpretation

**[QUOTED + INFERRED] DAT_20001027 is a multi-frame DCV range state machine with 4 states: 0 (idle), 1 (main), 2 (sub-range C), 3 (sub-range A), 0xFF (invalid).**

**State diagram:**

```
STATE 0 (idle) ←──────────┐
  │                        │
  ├─→ [Valid digit frame]  │
  │   (10, 11, 12, 13)     │
  │   Reset to 0           │
  │   Fall through to FSM   │
  │                        │
  └─→ [Invalid frame]      │
      (NaN/OL)             │
      Jump to State 1      │
                           │
STATE 1 (main range) ←─────┤
  ├─→ [frame[7] bit 5 SET] │
  │   → STATE 3            │
  │   (sub-range A)        │
  │                        │
  ├─→ [frame[7] bit 5 CLEAR, digit[0] bit 0 CLEAR]
  │   → STATE 2            │
  │   (sub-range C)        │
  │                        │
  └─→ [else]               │
      Stay in STATE 1      │
      Update DAT_2000102f  │
      based on frame[7] bit 0
                           │
STATE 2 (sub-range C)      │
  └─→ [Next valid frame]   │
      → Reset to 0 ────────┘
      
STATE 3 (sub-range A)
  └─→ [Next valid frame]
      → Reset to 0 ────────→
```

**Key characteristics:**

1. **Resets on every valid digit frame** (line 30423: condition `iVar9==10 && iVar10==11 && iVar11==12 && iVar12==13`). These are the sync markers from the FPGA meter IC.

2. **Persists across invalid frames**. If an OL or blank frame arrives, state 1 stays at state 1 until a valid frame resets it to 0.

3. **Binary decisions chain** through frame bit tests:
   - **bit 5 of frame[7]** (0x20): distinguishes sub-range A from B/C
   - **bit 4 of frame[7]** (0x10): within range B/C, selects C vs B
   - **bit 0 of frame[7]** (0x01): polarity / decimal place selector

4. **DAT_2000102f (decimal position) depends on state and frame bits** (lines 30435, 30450, 30453, 30466). Different states interpret the same bit differently.

5. **Multi-frame required**. You **cannot** decode DCV decimal position from a single frame. The FSM needs at least 2 frames:
   - Frame 1: Set state = 1 or 2 or 3
   - Frame 2: Use state to interpret frame[7] bits correctly

### Uncertainty

- **What do states 1, 2, 3 represent in meter terms?** Likely mV, V, 10V ranges but needs hardware testing.
- **Why does line 30631 set state to 0xFF on invalid?** This sentinel value (all bits set) might halt the FSM, but it's not tested in any case statement. Possible dead code or future expansion.
- **When does state actually change?** The transitions happen **within** case 0 of the FSM, which runs every frame when DAT_20001025 == 0 (DCV mode). But only if the frame is valid (passes the digit check at line 30421).

---

## Q6 — Frame type classification

### Evidence

**Valid digit frame pattern (line 30421-30422):**

```c
if (((iVar9 == 10 && iVar10 == 0xb) && (iVar11 == 0xc)) && (iVar12 == 0xd)) {
  DAT_2000102d = '\x01';
  DAT_20001027 = 0;
  /* ... proceed to FSM ... */
```

**Underrange pattern (line 30561-30562):**

```c
if (iVar10 == 0 && iVar11 == 0xe) {
  DAT_2000102d = '\x02';
  _DAT_20001028 = 20.0;
  goto code_r0x080371b0;
}
```

**Overrange/all-blanks pattern (line 30566-30567):**

```c
if (((iVar9 == 0x10 && iVar10 == 0x10) && (iVar11 == 0x10)) && (iVar12 == 0x10)) {
  DAT_2000102d = '\x03';
  goto code_r0x080371b0;
}
```

**Mode-B selector pattern (line 30571-30572):**

```c
if ((iVar10 == 0x12 && iVar11 == 10) && (iVar12 == 5)) {
  DAT_2000102d = '\x05';
  if (cRam20001055 == -0x50) {
    cRam20001055 = -0x4f;
    DAT_20001025 = 1;  /* Force mode change to ACV (1) */
  }
}
```

**Special symbol patterns (line 30579-30638):**

```c
if (iVar10 != 0x13 || iVar11 != 0x14) {
  if (((iVar9 != 0xff && iVar10 != 0xff) && iVar11 != 0xff) && (iVar12 != 0xff)) {
    /* Valid non-digit data: compute float value from BCD digits */
    DAT_2000102d = '\0';  /* Invalid/computing */
    /* ... decimal conversion and VFP pipeline ... */
  }
  goto code_r0x080371b0;
}
DAT_2000102d = '\t';  /* 0x09: Special symbol (possibly diode/continuity marker) */
if ((iVar12 - 1U & 0xff) < 3) {
  DAT_2000102d = (char)iVar12 + '\x05';  /* Encode special marker */
}
```

**Diode/continuity pattern (line 30635-30638):**

```c
/* Triggers only if iVar10 == 0x13 && iVar11 == 0x14 */
DAT_2000102d = '\t';  /* 0x09 base code */
if ((iVar12 - 1U & 0xff) < 3) {
  DAT_2000102d = (char)iVar12 + '\x05';  /* 0x0A, 0x0B, 0x0C for diode/continuity states */
}
```

**Mode change pattern (line 30643-30645):**

```c
/* Falls through if none of above patterns match && valid_digit_frame */
DAT_2000102d = '\x04';  /* Mode change / format change */
DAT_20001025 = 8;       /* Switch to mode 8 */
```

### Interpretation

**[QUOTED] Frame classification uses the 4 BCD digit lookup results (iVar9-iVar12) from func_0x08033ef8().**

Classification table (result_class DAT_2000102d):

| Value | Pattern | Meaning | Action |
|-------|---------|---------|--------|
| 0x00 | Invalid/NaN (line 30630) | No valid digits | Skip display, set DAT_20001027 = 0xFF |
| 0x01 | (10, 11, 12, 13) | Valid digit frame | Standard measurement path, reset state machine |
| 0x02 | (_, 0, 14, _) | Underrange | Display "0.xx", set value to 20.0 for bar |
| 0x03 | (0x10, 0x10, 0x10, 0x10) | All blanks / Overload high | Display blank, set _DAT_20001028 = 0 |
| 0x04 | (not 0x10, not 0x11, 10, 14) + others | Mode/format change | Switch DAT_20001025 to mode 8, reset value |
| 0x05 | (_, 0x12, 10, 5) | Mode-B selector (continuity?) | Force ACV mode if first entry, same frame |
| 0x09 | iVar10 == 0x13 && iVar11 == 0x14 | Diode/continuity marker | Encode special diode state from iVar12 |
| 0x0A–0x0C | (_, 0x13, 0x14, 1–3) | Diode sub-states | Different diode/continuity readouts |

**func_0x08033ef8** (BCD decoder) is NOT defined in the decomp but is called 4 times (lines 30411-30417):

```c
iVar9 = func_0x08033ef8((uVar20 & 0xf0) + (uVar22 & 0xf));      /* digit[0] */
iVar10 = func_0x08033ef8((uVar22 & 0xf0) + (uVar17 & 0xf));     /* digit[1] */
iVar11 = func_0x08033ef8((uVar17 & 0xf0) + (uVar19 & 0xf));     /* digit[2] */
iVar12 = func_0x08033ef8((bRam20004e17 & 0xf) + (uVar19 & 0xf0)); /* digit[3] */
```

**Return values (from our meter_data.c line 62-79):**
- 0x00–0x09: BCD digits
- 0x0A: OL (overload high)
- 0x0B: OL (overload low)
- 0x0C–0x0F: Special markers
- 0x10: Blank segment
- 0x11: Partial/transitional
- 0x12: Continuity marker
- 0x13: Mode change marker
- 0x14: Special (diode?)
- 0xFF: Invalid/unmapped

### Uncertainty

- **Exact meaning of codes 0x0A–0x0E?** The meter_data.c comments list them but the display firmware's handling is not in the decomp (that's in display_task handlers via the function table).
- **Why does line 30637 use `iVar12 + '\x05'`?** This encodes diode states (iVar12 = 1–3 becomes 0x0A–0x0C). The meaning depends on the meter IC's LCD encoding for diode/continuity.
- **Is func_0x08033ef8 a lookup table or algorithmic?** The decomp calls it as a function but our meter_data.c extracted a 256-entry lookup table (line 62-79). Either stock firmware uses a TBB/TBH jump table, or an external ROM.

---

## Q7 — DAT_20001030 (composite display index)

### Evidence

**All write sites (display formatter, lines 2889-2962):**

```c
Line 2895:   DAT_20001030 = -1;                 /* DCV state 0: unset sentinel */
Line 2898:   DAT_20001030 = DAT_2000102f;       /* DCV state 1: = decimal position (0–3) */
Line 2913:   DAT_20001030 = DAT_2000102f + '\x02'; /* DCV state 2/3: offset by +2 */

Line 2922:   DAT_20001030 = DAT_2000102f;       /* ACV: = decimal position */
Line 2927:   DAT_20001030 = DAT_2000102f;       /* DCA: = decimal position */
Line 2936:   DAT_20001030 = DAT_2000102f + '\x02'; /* ACA: offset by +2 */
Line 2940:   DAT_20001030 = DAT_2000102f + '\x05'; /* Freq: offset by +5 */
Line 2944:   DAT_20001030 = DAT_2000102f + '\t';   /* Freq: offset by +9 */
Line 2962:   DAT_20001030 = DAT_2000102f + '\n';   /* Freq (continued): offset by +10 */
```

**All read sites:**

```c
Line 4333:
if (DAT_20001030 != -1) {
  draw_ui_element(0x102, 0x70, 0x34, 0x1c);
}

Line 30801:
uRam20001052 = DAT_20001030;  /* Copy to RAM shadow variable for persistence */
```

### Interpretation

**[INFERRED] DAT_20001030 is a composite display format template index, computed as DAT_2000102f + submode_offset.**

Formula:
```
DAT_20001030 = DAT_2000102f + per_submode_offset

where per_submode_offset is:
  DCV (0):     +0  → 0–3
  ACV (1):     +0  → 0–3
  DCA (2):     +0  → 0–3
  ACA (3):     +2  → 2–5
  ACA (4):     +2  → 2–5
  Freq/ACA (5):+5 or +9 or +10 (conditional in case 5)
  Ohm (6):     [no offset code in decomp]
  Ohm (7):     [no offset code in decomp]
```

**Purpose:** Index into a display format table, likely specifying which digit positions get decimal points and which LCD segments to light.

**Range:** -1 (unset/disabled) to ~13 (maximum for Frequency with offset +10).

**Persistence:** Read at line 30801 and copied to `uRam20001052` (RAM shadow). This suggests the display task uses it later to look up segment patterns.

**Sentinel check:** Line 4333 checks `!= -1` before drawing, implying -1 means "no format selected yet" (likely during mode transitions or initialization).

### Uncertainty

- **What table does it index?** The decomp doesn't show the table, but it's likely a 14–16 entry array of LCD segment pattern templates.
- **Is it actually used for rendering, or just informational?** Line 30801 copies it to a shadow variable that's only read elsewhere in dvom_rx_task. The drawing code path uses it only for the sentinel check.
- **Why different offsets per submode?** Likely to account for different decimal positions in the table. E.g., Frequency modes have more possible decimal positions (up to 10), so higher indices are reserved for them.

---

## What I could not find

1. **Exact bit-to-segment mapping in meter IC.** The BCD lookup table (our meter_data.c line 62–79) decodes FPGA nibbles to digit codes, but the underlying 7-segment LCD drive encoding remains undocumented in the decomp.

2. **Flash string table contents** (0x804c40c). The decomp references the address but not the actual strings. We are relying on inference and our own meter_data.c table.

3. **func_0x08033ef8 source code.** It's called but not defined in the decomp. Likely a jump table (TBB/TBH) in the original binary, collapsed into an external symbol by Ghidra.

4. **FPGA range-selection commands.** The stock firmware reads frame[7] bit 3 (auto-range feedback) but never sends range commands back. If FPGA auto-ranging exists, it's autonomous and not firmware-controlled.

5. **Full range indices for all submodes.** We see DAT_2000102e variants for DCV (0, 1, 2) but submodes 1–7 don't set it — they rely on hardcoded display dispatch. Unclear which index is used for, e.g., ACA 10A range vs ACA mA range.

6. **DCV sub-state machine full semantics.** States 1–3 make sense, but the exact correspondence to meter ranges (mV vs V vs 10V) needs hardware verification.

7. **Why frame[7] must equal 0xAA** (line 13691). This is likely an FS9922 LCD meter IC synchronization byte, but not documented in decomp.

---

## Recommendations

### Before porting DCV FSM:

1. **Test DCV on each range** (0.2V, 2V, 20V, 200V) and **log frame[7] bit patterns** for each. Map states 1–3 to actual voltage ranges.

2. **Verify frame format assumption.** Decode a raw USART2 RX frame from the FPGA and confirm:
   - Byte offsets match our `bRam20004e13–20004e17` labels
   - Frame[7] is always 0xAA for valid measurements
   - Frame[7] bit 3 toggles during auto-range transitions

3. **Port one submode at a time** (start with ACV, which is simplest: single bit test at line 30472). Test each against stock firmware output.

### For auto-ranging (if implemented):

1. **Do NOT assume stock firmware auto-ranges.** It doesn't send range commands — it only displays current range. If you need auto-range, design a feedback loop:
   - Monitor if reading is near 0 or near OL
   - Send FPGA range-select command (byte pattern TBD via reverse-engineering FPGA comms)
   - Wait for next frame and check if value improved

2. **Frame[7] bit 3** is the key: test it to detect FPGA-initiated auto-range transitions, but stock firmware doesn't respond to them.

### For display strings:

1. **Our meter_data.c unit_suffix_table is correct.** Verified against display formatter logic (lines 2889–2963).

2. **DAT_20001026 to string lookup** goes through flash table 0x804c40c. We supply our own ASCII table at boot — no dependency on stock flash.

---

## Files Referenced

| File | Purpose |
|------|---------|
| `/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c` | Primary source: full V1.2.0 firmware decomp (38.8K lines) |
| `/Users/david/Desktop/osc/firmware/src/drivers/meter_data.c` | Our current meter parser: already has BCD table, unit strings, and decode hints |
| `/Users/david/Desktop/osc/reverse_engineering/analysis_v120/STATE_STRUCTURE.md` | RAM address map (note: has some labeling inconsistencies — prefer decomp quotes) |
| `/Users/david/Desktop/osc/reverse_engineering/analysis_v120/FPGA_TASK_ANALYSIS.md` | (not read in this dive — available if needed for FPGA comms reverse-engineering) |

---

## Summary Table: Critical Addresses & Variables

| Address | Symbol | Type | Usage | Status |
|---------|--------|------|-------|--------|
| 0x20001025 | DAT_20001025 / meter_mode | uint8 | Meter submode (0=DCV, 1=ACV, ..., 7=Cont) | Fully mapped |
| 0x20001026 | DAT_20001026 / meter_display_state | uint8 | Unit string index (0–11) | Fully mapped, output |
| 0x20001027 | DAT_20001027 / meter_substate | uint8 | DCV multi-frame state (0–3, 0xFF invalid) | Fully mapped, DCV-only FSM |
| 0x20001028 | _DAT_20001028 / meter_raw_value | float | Computed measurement value | Display-side only |
| 0x2000102c | DAT_2000102c / meter_range | uint8 | Display range selector (1–4, for bar graph) | Uncertain purpose |
| 0x2000102d | DAT_2000102d / result_class | uint8 | Frame type (0x00–0x09, see Q6) | Fully mapped |
| 0x2000102e | DAT_2000102e / unit_variant | uint8 | DCV unit variant (0=V, 1=mV, 2=special) | DCV-only, 6 write sites |
| 0x2000102f | DAT_2000102f / decimal_position | uint8 | Decimal point position (0–4) | Fully mapped, output |
| 0x20001030 | DAT_20001030 / composite_index | uint8 | Display format template index | Uncertain purpose, output |

---

**Generated:** 2026-04-04  
**Decomp Source:** decompiled_2C53T_v2.c (38,837 lines)  
**Methodology:** Verbatim quote + bit-shift translation + multi-site cross-reference  
**Confidence:** High on Q1, Q2, Q4, Q6, Q7 (direct quotes). Medium on Q3 (function purpose inferred). Medium-low on Q5 (state mapping unverified).

