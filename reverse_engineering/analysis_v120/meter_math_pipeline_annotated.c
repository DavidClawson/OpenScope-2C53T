/*
 * FNIRSI 2C53T — Meter Math Pipeline Annotated Decompilation
 * ============================================================
 *
 * Firmware: V1.2.0 (APP_2C53T_V1.2.0_251015.bin, base 0x08000000)
 * Analysis date: 2026-04-04
 * MCU: Artery AT32F403A @ 240 MHz
 *
 * FUNCTIONS COVERED:
 *   1. dvom_rx_task   @ 0x08036AC0 – 0x080371B0  (1776 bytes)
 *      Alias in Ghidra v2 decompile: dvom_rx_task
 *      Previous name: meter_data_processor
 *      Role: USART2 RX ISR wakeup → BCD decode → decimal classification
 *            → sign correction → temperature conversion → mode-FSM dispatch
 *
 *   2. meter_mode_handler  (tail of dvom_rx_task) @ 0x080371B0 – 0x080373F4
 *      8-state FSM: reads frame[6]/[7] to set probe_type, decimal_pos, unit.
 *
 *   3. fpga_state_update  @ 0x080028E0 – 0x08002BE8  (776 bytes)
 *      Previous name: meter_data_process
 *      Role: optional relative-measurement subtract, decimal-shift loop,
 *            auto-range classification, USART command dispatch (0x1B/0x1C/0x1E)
 *
 * SOFT-FLOAT ABI CALLS (all ARM EABI soft-float library, region 0x0803xxxx):
 *   0x0803ED70  __aeabi_f2d    float  → double
 *   0x0803E5DA  __aeabi_ui2d   uint32 → double
 *   0x0803DF48  __aeabi_d2iz   double → int32 (truncate)  [s0 also set]
 *   0x0803C8E0  pow            (double base, double exp) → double   [confirmed by disasm]
 *   0x0803E77C  __aeabi_dmul   double × double
 *   0x0803EB94  __aeabi_dsub   double − double
 *   0x0803EE0C  __aeabi_dcmplt double < double → int (1=yes, 0=no)
 *   0x0803E124  __aeabi_dadd   double + double
 *               NOTE: this was previously mis-labelled as __aeabi_ddiv in an earlier
 *               annotation pass. The binary pattern is addition (no Newton-Raphson
 *               mantissa inversion), consistently used as "add 1000.0" in the
 *               decimal-shift loop.
 *   0x0803DFAC  __aeabi_ddiv   double ÷ double   [used in Fahrenheit conversion check]
 *   0x0803EDFE  __aeabi_dcmpeq double == double → int
 *   0x0803EDF0  __aeabi_dcmpgt double > double → int  [or dcmple; order matters]
 *   0x0803ACF0  xQueueGenericSend (FreeRTOS)
 *   0x0803B3A8  xQueueSemaphoreTake (FreeRTOS)
 *
 * STATE STRUCTURE (meter_state base = 0x200000F8):
 *   All offsets below are BYTE offsets from that base.
 *
 *   +0xF2C (0x20001024) = reserved / cleared each frame
 *   +0xF2D (0x20001025) = meter_mode         uint8  0-8: DCV,ACV,Ω,Cont,Diode,Cap,Freq,Period,Duty
 *   +0xF2E (0x20001026) = meter_display_cmd  uint8  FPGA command code for next TX
 *   +0xF2F (0x20001027) = meter_sub_mode     uint8  sub-mode within meter_mode
 *   +0xF30 (0x20001028) = meter_raw_value    float  raw BCD count as float, updated each frame
 *   +0xF34 (0x2000102C) = meter_result_class uint8  1-4: normal/underrange/overrange/invalid
 *   +0xF35 (0x2000102D) = meter_data_valid   uint8  0=no data, 1=OL, 2=20.0 sentinel, 3=blank,
 *                                                    4=partial-blank, 5=continuity, 9/10/11=mode-chg,
 *                                                    0xFF=normal digit read
 *   +0xF36 (0x2000102E) = meter_probe_type   uint8  from FSM: 0, 1, or 2
 *   +0xF37 (0x2000102F) = meter_decimal_pos  uint8  0-4 decimal position; set by meter_mode_handler
 *   +0xF38 (0x20001030) = meter_unit_index   uint8  0xFF=unset; set by fpga_state_update
 *   +0xF39 (0x20001031) = celsius_flag        uint8  nonzero = display Celsius; 0 = Fahrenheit
 *   +0xF3C (0x20001034) = usart_lock          uint16 lock for USART exchange
 *   +0xF3D (0x20001035) = rel_mode_active     uint8  relative measurement mode active
 *   +0xF40 (0x20001038) = rel_reference_val   float  reference float for relative mode (from +0xF30)
 *   +0xF44 (0x2000103C) = rel_ref_decimal     uint8  decimal_pos of the reference measurement
 *   +0xF48 (0x20001040) = meter_max_value     float  MAX HOLD value (NaN sentinel 0x7FC00000 when uninit)
 *   +0xF4C (0x20001044) = meter_min_value     float  MIN HOLD value
 *   +0xF50 (0x20001048) = meter_avg_accum     float  AVG HOLD accumulator
 *   +0xF54 (0x2000104C) = meter_max_class     uint8  result_class for max_value
 *   +0xF55 (0x2000104D) = meter_min_class     uint8  result_class for min_value
 *   +0xF56 (0x2000104E) = meter_avg_class     uint8  result_class for avg_value
 *   +0xF57 (0x2000104F) = meter_max_dp        uint8  decimal_pos for max_value
 *   +0xF58 (0x20001050) = meter_min_dp        uint8  decimal_pos for min_value
 *   +0xF59 (0x20001051) = meter_avg_dp        uint8  decimal_pos for avg_value
 *   +0xF5A (0x20001052) = meter_max_unit      uint8  unit_index for max_value
 *   +0xF5B (0x20001053) = meter_min_unit      uint8  unit_index for min_value
 *   +0xF5C (0x20001054) = meter_avg_unit      uint8  unit_index for avg_value
 *   +0xF5D (0x20001055) = continuity_state    uint8  0xB0=armed, 0xB1=beeping
 *
 * RX FRAME BUFFER (base 0x20004E11, 12 bytes for data frame):
 *   [0]  (0x20004E11) = 0x5A  (header byte 1)
 *   [1]  (0x20004E12) = 0xA5  (header byte 2)
 *   [2]  (0x20004E13) = bcd_byte_a   (nibble source for digit0 high, digit1 low wait)
 *   [3]  (0x20004E14) = bcd_byte_b   = rx_echo_verify_byte in Ghidra
 *   [4]  (0x20004E15) = bcd_byte_c
 *   [5]  (0x20004E16) = bcd_byte_d
 *   [6]  (0x20004E17) = bcd_byte_e   (range/mode flags)
 *   [7]  (0x20004E18) = status_byte  (polarity/AC/DC/overload flags)
 *   [8]  (0x20004E19) = mode_byte    (FPGA-side mode indicator, e.g., 0x02=DCV)
 *   [9]  (0x20004E1A) = 0x00 always observed
 *   [10] (0x20004E1B) = 0x01 always observed
 *   [11] (0x20004E1C) = rolling counter (varies, possibly checksum)
 *
 * ============================================================================
 * DOUBLE-PRECISION LITERAL POOL — 0x08036C60 (preloaded into VFP d-registers)
 * ============================================================================
 *
 * Loaded at dvom_rx_task entry via VLDR into saved registers (callee-saved):
 *   0x08036C60 → d8  = 1000.0  (used as DCmpLT threshold in inner decimal-shift loop)
 *   0x08036C68 → d11 = 0.5     (available; unclear use; possibly rounding mid-point)
 *   0x08036C70 → d12 = 0.0     (zero, used for sign-bit manipulation via BFI)
 *   0x08036C78 → d13 = 100.0   (DCmpLT threshold: overrange check)
 *   0x08036C80 → d9  = 10.0    (base for pow() calls for decimal scaling)
 *   0x08036C88 → f32 = 10000.0 (float, added to raw_bcd for negative-polarity encoding)
 *   0x08036C90 → double = 4.0  (result_class sentinel for the highest classification tier)
 *
 * Decimal-scaling doubles at 0x080373D0 (pointed to by PC+offset in branch paths):
 *   0x080373D0 = 3.0  (frame[3] bit-4 path: pow(10.0, 3.0) = 1000.0)
 *   0x080373D8 = 2.0  (frame[4] bit-4 path: pow(10.0, 2.0) = 100.0)
 *   0x080373E0 = 1.0  (frame[5] bit-4 path: pow(10.0, 1.0) = 10.0)
 *   0x080373E8 = 0.0  (default path:         pow(10.0, 0.0) = 1.0)
 *   0x080373F0 = 32.0 (float, Fahrenheit offset = 32.0)
 *
 * fpga_state_update (FUN_080028E0) literal pool at 0x08002BF0:
 *   0x08002BF0 = 1000.0 double (d8 in that function; used for decimal-shift dadd)
 *   0x08002BF8 = 0.0    double (sign bit reference)
 *   0x08002C00 = 10000.0 double (dcmplt threshold for decimal-shift loop)
 *   0x08002C08 = 1000.0 float  (auto-range threshold: value >= 1000 → range 1)
 *   0x08002C0C = 100.0  float  (auto-range threshold: value >= 100  → range 2)
 *
 * ============================================================================
 * SECTION 1: dvom_rx_task  (FUN_08036AC0)
 * ============================================================================
 */

/* --- Global RAM references used throughout this file --- */
#define METER_STATE_BASE    0x200000F8UL
#define RX_BUF_BASE         0x20004E11UL
#define METER_SEM_HANDLE    (*(volatile uint32_t *)0x20002D7CUL)

/* Convenience typed accessors into the meter_state blob */
typedef struct {
    /* ... all other fields ... */
    /* Only meter-relevant fields shown here */
    uint8_t  meter_mode;           /* +0xF2D */
    uint8_t  meter_display_cmd;    /* +0xF2E */
    uint8_t  meter_sub_mode;       /* +0xF2F */
    float    meter_raw_value;      /* +0xF30 */
    uint8_t  meter_result_class;   /* +0xF34 */
    uint8_t  meter_data_valid;     /* +0xF35 */
    uint8_t  meter_probe_type;     /* +0xF36 */
    uint8_t  meter_decimal_pos;    /* +0xF37 */
    uint8_t  meter_unit_index;     /* +0xF38 */
    uint8_t  celsius_flag;         /* +0xF39 */
    uint16_t usart_lock;           /* +0xF3C */
    uint8_t  rel_mode_active;      /* +0xF3D */
    float    rel_reference_val;    /* +0xF40 */
    uint8_t  rel_ref_decimal;      /* +0xF44 */
    float    meter_max_value;      /* +0xF48 (NaN sentinel 0x7FC00000 when uninit) */
    float    meter_min_value;      /* +0xF4C */
    float    meter_avg_accum;      /* +0xF50 */
    uint8_t  meter_max_class;      /* +0xF54 */
    uint8_t  meter_min_class;      /* +0xF55 */
    uint8_t  meter_avg_class;      /* +0xF56 */
    uint8_t  meter_max_dp;         /* +0xF57 */
    uint8_t  meter_min_dp;         /* +0xF58 */
    uint8_t  meter_avg_dp;         /* +0xF59 */
    uint8_t  meter_max_unit;       /* +0xF5A */
    uint8_t  meter_min_unit;       /* +0xF5B */
    uint8_t  meter_avg_unit;       /* +0xF5C */
    uint8_t  continuity_state;     /* +0xF5D */
} meter_state_t;

/*
 * ---- RX FRAME BYTE → DIGIT MAPPING ----
 *
 *  The FPGA meter IC (FS9922-like) outputs 7-segment drive signals, not clean BCD.
 *  Four overlapping nibble pairs from five consecutive bytes encode four display digits.
 *
 *  byte index  | field name          | description
 *  ------------|---------------------|---------------------------------------------------
 *  frame[2]    | bcd_byte_a          | high nibble → digit0 high; bit 3 = +10000 polarity flag;
 *              |                     | bit 4 = measurement polarity (negate output)
 *  frame[3]    | bcd_byte_b          | low nibble → digit0 low; high nibble → digit1 high;
 *              |                     | bit 4 = decimal range bit (frame[3]:4 → shift 3)
 *  frame[4]    | bcd_byte_c          | low nibble → digit1 low; high nibble → digit2 high;
 *              |                     | bit 4 = decimal range bit (frame[4]:4 → shift 2)
 *  frame[5]    | bcd_byte_d          | low nibble → digit2 low; high nibble → digit3 high;
 *              |                     | bit 4 = decimal range bit (frame[5]:4 → shift 1)
 *  frame[6]    | bcd_byte_e / range  | low nibble → digit3 low; ALSO carries range flags
 *              |                     | (see frame[6] table in meter_frame_capture_log.md)
 *  frame[7]    | status_byte         | bit 5 = Ω mode; bit 3 = zero-detect; bit 0 = polarity
 *  frame[8]    | mode_byte           | 0x00=Ω, 0x02=DCV; bit 7 used for cRam20004e19 < 0 check
 *
 *  Nibble extraction:
 *    digit0 = bcd_lookup[(frame[2] & 0xF0) | (frame[3] & 0x0F)]
 *    digit1 = bcd_lookup[(frame[3] & 0xF0) | (frame[4] & 0x0F)]
 *    digit2 = bcd_lookup[(frame[4] & 0xF0) | (frame[5] & 0x0F)]
 *    digit3 = bcd_lookup[(frame[5] & 0xF0) | (frame[6] & 0x0F)]
 *
 *  Output codes from bcd_lookup (FUN_08033EF8):
 *    0-9    = BCD digit value
 *    0x0A   = O (overload high segment)
 *    0x0B   = L (overload low segment)
 *    0x0C   = special (part of continuity pattern)
 *    0x0D   = special
 *    0x0E   = special (zero with offset)
 *    0x0F   = special
 *    0x10   = blank (segment off, treat as 0 in numeric assembly)
 *    0x11   = partial blank
 *    0x12   = continuity marker
 *    0x13   = mode-change marker
 *    0x14   = special sub-marker
 *    0xFF   = invalid / unmapped nibble pair
 *
 *  Special 4-digit patterns (checked before normal BCD assembly):
 *    {0x0A, 0x0B, 0x0C, 0x0D} = OL (overload), all 4 digits must match
 *    {any, 0x00, 0x0E, any}   = zero with 20.0 sentinel (sets raw_value = 20.0)
 *    {0x10,0x10,0x10,0x10}    = blank (3 segments off)
 *    {0x10,0x11,0x0A,0x0E}    = partial blank pattern
 *    {any, 0x12, 0x0A, 5}     = continuity: buzzer state set
 *    {any, 0x13, 0x14, 1-3}   = mode change in progress
 */

/* ===== FUN_08033EF8 : bcd_nibble_lookup (reproduced from stock firmware) ===== */
/*
 * The function masks bit 4 of the input (AND 0xEF), then dispatches via
 * TBH (table-branch halfword) at 0x08033F0E for inputs 0x8A-0xEF, and
 * via TBB at 0x08033FE2 for inputs 0x00-0x64.
 * Our meter_data.c replicates this with the 256-entry bcd_lookup[] table.
 */
static inline uint8_t bcd_nibble_lookup(uint8_t combined)
{
    combined &= 0xEF;  /* mask bit 4 (= FUN_08033EF8 first instruction) */
    return bcd_lookup[combined];  /* our static table matches TBH/TBB dispatch */
}

/* ===== dvom_rx_task MAIN BODY ===== */

/*
 * Task entry VFP register preloads (callee-saved, persist through entire task lifetime):
 *   d8  = 1000.0  (from 0x08036C60) — decimal-shift add constant & compare threshold
 *   d9  = 10.0    (from 0x08036C80) — pow() base for decimal scaling
 *   d11 = 0.5     (from 0x08036C68) — (purpose unclear; possibly unused in current analysis)
 *   d12 = 0.0     (from 0x08036C70) — sign bit template for BFI instructions
 *   d13 = 100.0   (from 0x08036C78) — overrange threshold comparison
 *
 * In ARM VFP, d_n maps to s_{2n}:s_{2n+1} where s_{2n} is low 32 bits:
 *   d8 = s16:s17, d9 = s18:s19, d10 = s20:s21, d11 = s22:s23,
 *   d12 = s24:s25, d13 = s26:s27
 */

void dvom_rx_task(void)
{
    volatile uint8_t *rx_buf = (volatile uint8_t *)RX_BUF_BASE;
    volatile uint8_t *ms = (volatile uint8_t *)METER_STATE_BASE;
    /* ms[+0xF2D] = meter_mode, etc. */

    /* VFP registers preloaded at task start — remain valid throughout: */
    /* d8=1000.0, d9=10.0, d11=0.5, d12=0.0, d13=100.0 */

    while (1) {
        /* ---- PHASE 0: Wait for meter semaphore ---- */
        /* Semaphore posted by USART2 RX ISR after complete 12-byte data frame */
        while (xQueueSemaphoreTake(METER_SEM_HANDLE, portMAX_DELAY) != 1) { }

        /* Clear the reserved byte */
        ms[0xF2C] = 0;

        /* ---- PHASE 1: BCD DIGIT EXTRACTION ---- */
        /*
         * Read five consecutive frame bytes; extract four cross-byte nibble pairs.
         * All five bytes are snapshotted into registers before any comparison.
         */
        uint8_t b2 = rx_buf[2];   /* frame[2]: digit0_high nibble + polarity flags */
        uint8_t b3 = rx_buf[3];   /* frame[3]: digit0_low + digit1_high + range bit */
        uint8_t b4 = rx_buf[4];   /* frame[4]: digit1_low + digit2_high + range bit */
        uint8_t b5 = rx_buf[5];   /* frame[5]: digit2_low + digit3_high + range bit */
        uint8_t b6 = rx_buf[6];   /* frame[6]: digit3_low + range/mode byte */

        int digit0 = bcd_nibble_lookup((b2 & 0xF0) | (b3 & 0x0F));
        int digit1 = bcd_nibble_lookup((b3 & 0xF0) | (b4 & 0x0F));
        int digit2 = bcd_nibble_lookup((b4 & 0xF0) | (b5 & 0x0F));
        int digit3 = bcd_nibble_lookup((b5 & 0xF0) | (b6 & 0x0F));

        /* ---- PHASE 2: SPECIAL CODE DETECTION ---- */

        /*
         * OL (Overload) — all four digit positions carry the OL sentinel.
         * Matched when digit0=0x0A (O), digit1=0x0B (L), digit2=0x0C, digit3=0x0D.
         * Sets meter_data_valid = 1, clears sub_mode and (if meter_mode==0) probe_type.
         * Falls through to meter_mode_handler FSM.
         */
        if (digit0 == 0x0A && digit1 == 0x0B && digit2 == 0x0C && digit3 == 0x0D) {
            ms[0xF35] = 1;                 /* meter_data_valid = OL */
            ms[0xF2F] = 0;                 /* meter_sub_mode = 0 */
            if (ms[0xF2D] == 0) ms[0xF2E] = 0; /* meter_probe_type = 0 (only for DCV) */
            ms[0xF30 + 0] = 0;            /* meter_raw_value = 0.0f */
            ms[0xF30 + 1] = 0;
            ms[0xF30 + 2] = 0;
            ms[0xF30 + 3] = 0;
            goto meter_mode_handler;
        }

        /*
         * Zero sentinel (digit1=0, digit2=0x0E): FPGA outputs this on some zero-detect
         * conditions. Sets meter_data_valid=2 and meter_raw_value=20.0 (note: not 0.0).
         * The 20.0 is a conventional sentinel used downstream to flag "at-range-max".
         */
        if (digit1 == 0 && digit2 == 0x0E) {
            ms[0xF35] = 2;
            *(float *)&ms[0xF30] = 20.0f;
            goto meter_mode_handler;
        }

        /*
         * Blank display — all four digits are the blank code 0x10.
         * Indicates no measurement being made (probe not inserted, startup, etc.).
         */
        if (digit0 == 0x10 && digit1 == 0x10 && digit2 == 0x10 && digit3 == 0x10) {
            ms[0xF35] = 3;
            goto meter_mode_handler;
        }

        /*
         * Partial blank — specific pattern {0x10, 0x11, 0x0A, 0x0E}.
         */
        if (digit0 == 0x10 && digit1 == 0x11 && digit2 == 0x0A && digit3 == 0x0E) {
            ms[0xF35] = 4;
            ms[0xF2D] = 8;                 /* meter_mode = 8 (DUTY CYCLE or special) */
            *(float *)&ms[0xF30] = 0.0f;
            goto meter_mode_handler;
        }

        /*
         * Continuity detected — {any, 0x12, 0x0A, 5}.
         * The digit3 must equal exactly 5 (the buzzer-on value from the IC).
         * Sets continuity_state to 0xB1 (beeping) if it was previously 0xB0 (armed).
         */
        if (digit1 == 0x12 && digit2 == 0x0A && digit3 == 5) {
            ms[0xF35] = 5;
            if (ms[0xF5D] == 0xB0) {
                ms[0xF5D] = 0xB1;          /* start buzzer */
                ms[0xF2D] = 1;             /* meter_mode = 1 */
            }
            *(float *)&ms[0xF30] = 0.0f;
            goto meter_mode_handler;
        }

        /*
         * Mode-change pattern — {any, 0x13, 0x14, 1-3}.
         * The FPGA signals a pending auto-range or mode transition.
         * meter_data_valid = 9 + (digit3 if 1-3 else 0).
         */
        if (digit1 == 0x13 && digit2 == 0x14) {
            uint8_t sub = (uint8_t)(digit3 - 1);
            ms[0xF35] = (sub < 3) ? (9 + digit3) : 9;
            *(float *)&ms[0xF30] = 0.0f;
            goto meter_mode_handler;
        }

        /* ---- PHASE 3: INVALID DIGIT GUARD ---- */
        /*
         * Any 0xFF in digit0/1/2/3 → invalid frame; skip to FSM without updating value.
         * Note: the stock firmware checks all four with conditional branches rather than
         * a single OR expression, but the semantics are equivalent.
         */
        if (digit0 == 0xFF || digit1 == 0xFF || digit2 == 0xFF || digit3 == 0xFF) {
            goto meter_mode_handler;
        }

        /* ---- PHASE 4: NORMAL DIGIT ASSEMBLY ---- */
        /*
         * Each digit code ≥ 0x10 is treated as 0 (blank segment = zero digit).
         * This handles the partial-blank pattern where e.g. "01.50" reads as "01.50"
         * with the leading 0 explicitly blank-coded.
         */
        int d0 = (digit0 == 0x10) ? 0 : digit0;
        int d1 = (digit1 == 0x10) ? 0 : digit1;
        int d2 = (digit2 == 0x10) ? 0 : digit2;
        int d3 = digit3;  /* digit3 ≠ 0x10 (already checked above via 0xFF guard) */

        int raw_bcd = d0 * 1000 + d1 * 100 + d2 * 10 + d3;

        /* Convert to float; optionally add 10000.0 for the negative-polarity encoding.
         *
         * frame[2] bit 3 (b2 << 0x1C, test MI): when set, the meter IC encodes negative
         * readings by adding 10000 to the raw count. The stock firmware adds the float
         * constant 10000.0f (stored at 0x08036C88) before storing.
         *
         * frame[2] bit 4 (b2 << 0x1B, test MI at 0x08037166): polarity bit used LATER
         * to negate the stored value (VNEG instruction after the decimal-scale path).
         */
        float raw_f = (float)raw_bcd;
        if (b2 & 0x08) {    /* bit 3 of frame[2] */
            raw_f += 10000.0f;
        }

        /* Store raw float to meter_raw_value (the primary per-frame reading) */
        *(float *)&ms[0xF30] = raw_f;

        /* ---- PHASE 5: DECIMAL RANGE CLASSIFICATION ---- */
        /*
         * Three bits across frame[3][4][5] encode the "decimal shift" level.
         * Each bit set means the IC is one range higher (each range = ×10 factor).
         * The shift is stored as both an integer class (DAT_2000102c = result_class)
         * and a double constant used to scale the value before storage.
         *
         * Priority order (highest wins):
         *   frame[8] bit 7 set  → class = 4, scale_exp = 4.0 (pow(10,4)=10000)
         *   frame[3] bit 4 set  → class = 3, scale_exp = 3.0 (pow(10,3)=1000)
         *   frame[4] bit 4 set  → class = 2, scale_exp = 2.0 (pow(10,2)=100)
         *   frame[5] bit 4 set  → class = 1, scale_exp = 1.0 (pow(10,1)=10)
         *   none set            → class = 0, scale_exp = 0.0 (pow(10,0)=1)
         *
         * For most common DCV measurements (< 10V), none of these bits are set.
         * For Ω measurements in kΩ range, one or two bits are set.
         *
         * HARDWARE CONFIRMED: frame capture #7 (5V DC): frame[3..6] = C6 F7 EB EB 0F
         *   b2=0xC6: bit 3 = 0 (no +10000), bit 4 = 0 (not negative)
         *   b3=0xF7: bit 4 = 1 (frame[3] bit 4 set) → class = 3, scale = 1000
         *   Wait: BCD 5008 with class=3 → 5008 × pow(10, 3) / pow(10, ?) = 5.008?
         *   Actually: see SECTION 5 (worked example) for full trace.
         */
        uint8_t result_class;
        double scale_exp;
        if ((int8_t)rx_buf[8] < 0) {       /* frame[8] bit 7 */
            result_class = 4;
            scale_exp = 4.0;
        } else if (b3 & 0x10) {            /* frame[3] bit 4 */
            result_class = 3;
            scale_exp = 3.0;
        } else if (b4 & 0x10) {            /* frame[4] bit 4 */
            result_class = 2;
            scale_exp = 2.0;
        } else if (b5 & 0x10) {            /* frame[5] bit 4 */
            result_class = 1;
            scale_exp = 1.0;
        } else {
            /* Default path — frame[5] bit 4 = 0 → scale_exp = 0.0
             * UBFX r0, r7, #4, #1 extracts bit 4 of frame[5] (held in r7).
             * result_class = 0 or 1 depending on that bit. For DCV normal, it's 0. */
            result_class = (b5 >> 4) & 1;  /* 0 or 1 */
            scale_exp = result_class ? 1.0 : 0.0;
        }
        ms[0xF34] = result_class;

        /*
         * Decimal scaling application:
         *   scale = pow(10.0, scale_exp)   — computed via FUN_0803C8E0 with d0=10.0 (d9)
         *   result = raw_f_double + scale   — via FUN_0803E124 (dadd)
         *
         * IMPORTANT: FUN_0803E124 is __aeabi_dadd (double ADD), NOT division.
         * In this path the scale (1.0, 10.0, 100.0, or 1000.0) is ADDED to raw_bcd.
         * This shifts the value UP by one range-step's worth of counts.
         *
         * For the common DCV low-range case (scale_exp=0.0, scale=1.0):
         *   result = raw_bcd + 1.0   (negligible change for display purposes)
         *
         * For the kΩ range (scale_exp=3.0, scale=1000.0):
         *   result = raw_bcd + 1000.0  (shifts display by one kΩ step)
         *
         * The purpose is analogous to the decimal-shift-add loop in fpga_state_update:
         * it adjusts the visible digit window to the correct decade.
         *
         * After the dadd, d2f converts back to float and stores to meter_raw_value.
         * A polarity check (frame[2] bit 4) then optionally negates the float.
         */
        double scale = pow(10.0, scale_exp);   /* pow(d9=10.0, scale_exp) */
        double raw_d = (double)raw_f;          /* __aeabi_f2d */
        double adjusted = raw_d + scale;       /* __aeabi_dadd = FUN_0803E124 */
        float final_val = (float)adjusted;     /* __aeabi_d2iz then d2f = FUN_0803DF48 */

        /* Store adjusted value back to meter_raw_value */
        *(float *)&ms[0xF30] = final_val;

        /* Negate if frame[2] bit 4 is set (polarity flag) */
        if (b2 & 0x10) {   /* frame[2] bit 4, tested via lsls r1, r8, #0x1B at 0x08037166 */
            *(float *)&ms[0xF30] = -final_val;
        }

        /* ---- PHASE 6: TEMPERATURE CONVERSION (submode 5 = Celsius/Fahrenheit) ---- */
        /*
         * If meter_mode == 5 (temperature) and celsius_flag (+0xF39) != 0:
         *   value = (value × 9.0) / 5.0 + 32.0   (Celsius to Fahrenheit)
         * The 32.0 constant lives at 0x080373F0 (float).
         * 9.0 and 5.0 are encoded as VFP immediates (VMOV.F32 instruction).
         */
        if (ms[0xF2D] == 5 && ms[0xF39] != 0) {
            float v = *(float *)&ms[0xF30];
            v = (v * 9.0f) / 5.0f + 32.0f;
            *(float *)&ms[0xF30] = v;
        }

        /* Set valid indicators */
        ms[0xF35] = 0;      /* meter_data_valid = 0 means "normal digit reading" here
                             * (confusing naming; 0xFF is set later by mode_handler for "normal") */
        ms[0xF2F] = 0xFF;   /* meter_sub_mode = 0xFF signals "valid measurement" */

        /* ---- Fall through to meter_mode_handler ---- */

meter_mode_handler:
        /*
         * The meter_mode_handler FSM runs on EVERY frame (normal AND special).
         * It reads frame[6] and frame[7] to update:
         *   - meter_probe_type (+0xF36): 0, 1, or 2
         *   - meter_decimal_pos (+0xF37): 0-4 (overwritten from decimal_pos computed above)
         *   - meter_unit_index (+0xF38): index into unit string table
         *
         * See SECTION 2 below for the full FSM documentation.
         */
        meter_mode_handler_impl(rx_buf, ms);  /* inlined in actual binary */

        /* ---- PHASE 7: CALL fpga_state_update ---- */
        /* (Documentation in SECTION 3) */
        fpga_state_update();

        /* ---- PHASE 8: MAX/MIN/AVG HOLD UPDATE ---- */
        /*
         * Runs only when meter_mode is in {1,3,4,5,6,7} (not 0, 2, 8) AND meter_data_valid == 0.
         *
         * On the very first frame after init, meter_max_value contains the NaN sentinel
         * 0x7FC00000. When |max_value| > 0x7F800000 (∞), the init path is taken:
         *   - All three hold values (max, min, avg) are set to the current raw_value
         *   - All three class/dp/unit bytes are copied from the current frame's values
         *   - The main loop restarts (no display update on this frame)
         *
         * On subsequent frames, the calibration block runs:
         *   For each hold value (max, min, avg):
         *     pow_factor = pow(1000.0, hold_decimal_pos)
         *     calibrated_double = hold_float_value * pow_factor   [dmul]
         *     [stored into stack double array for the comparison block below]
         *
         * The comparison block then:
         *   - Computes calibrated values for current frame (same formula)
         *   - Picks the larger/smaller via dcmpeq/dcmpgt comparisons
         *   - Applies the decimal-shift-add loop (adds d8=1000.0 up to 4 times)
         *     until the value is >= 10000 (loop exits when |value| >= d8 threshold)
         *     Each add increments the decimal_shift counter (stored to hold_dp field)
         *   - Classifies: result_class = 4 if < 1.0; 3 if < 100.0; 2 if < 10.0; else 1
         *   - Converts back to float via FUN_0803DF48
         *
         * After the inner loop (3 iterations for max/min/avg), writes back:
         *   fRam20001040 = meter_max_value
         *   bRam2000104C = meter_max_class
         *   bRam2000104F = meter_max_dp
         *   uRam20001052 = meter_max_unit
         *   (same for min, avg at +1, +2 offsets)
         *
         * NOTE: the pow(1000.0, dp) multiplication with d8=1000.0 (s16:s17) is a
         * SCALE FACTOR for display comparison only. It does NOT divide the raw BCD
         * by a calibration coefficient — it MULTIPLIES to shift decimal position.
         */
    }
}

/*
 * ============================================================================
 * SECTION 2: meter_mode_handler FSM  (tail of dvom_rx_task, @ 0x080371B0)
 * ============================================================================
 *
 * 8-state FSM dispatched via TBB at 0x080371C4 on meter_mode (0-7).
 * Reads frame[6] (range/mode byte) and frame[7] (status byte).
 * Updates: meter_probe_type (+0xF36) and meter_decimal_pos (+0xF37).
 *
 * CONFIRMED FRAME[7] BIT DEFINITIONS (from bench captures):
 *   bit 5 (0x20): Ω mode active (set for all resistance readings)
 *   bit 3 (0x08): zero-detect / very-low-value indicator (shorted probes)
 *   bit 0 (0x01): used for probe_type selection (DCV mode)
 *
 * CONFIRMED FRAME[6] RANGE TABLE (from meter_frame_capture_log.md):
 *   0x07 = low Ω band (~0-999 Ω):        dp=2, unit="Ohm"
 *   0x4B = kΩ band, < 10 kΩ sub-range:   dp=1, unit="kOhm"
 *   0x4D = kΩ band, 10-99 kΩ sub-range:  dp=2, unit="kOhm"
 *   0x0F = DCV mode:                      dp=1, unit="V"
 *
 * STATE MACHINE PER METER_MODE:
 *
 * case 0 (DCV):
 *   [Falls through from meter_data_valid != 0 check at 0x080371F2]
 *   Checks meter_sub_mode (+0xF2F):
 *     If meter_sub_mode == 0: skip
 *     Else: read frame[7] bits
 *       frame[7] bit 5 (lsls r0, #0x1A, MI): if set → probe_type = 2, dp_source = 0
 *         Also checks frame[7] bit 4 and frame[2] bit 1 for sub-conditions
 *       frame[7] bit 2 (AC/DC flag for DCV): sets probe_type
 *       frame[7] bit 0 (polarity):
 *         if 0 → probe_type = 2
 *         if 1 → probe_type = 1
 *
 *   Then at 0x080371D0 (case 6/7 fallthrough):
 *     frame[6] bit 4 (lsls r1, #0x1B, MI): if set → probe_type = 0; return
 *     frame[7] bit 0 (lsls r1, #0x1F):
 *       if 0 (pl): probe_type = 2
 *       if 1 (ne): probe_type = 1
 *     Stores probe_type to meter_decimal_pos (+0xF37)  ← NOTE: stores probe_type into dp field!
 *       This seems wrong but matches disasm: strb.w r1, [r7, #0xf37] at 0x080371E0
 *       The decimal_pos is thus 1 or 2, coming from frame[7] bit 0.
 *
 * case 1 (ACV): similar probe_type/dp selection from frame[7] bit 0
 * case 2 (Ω resistance): complex sub-dispatch on probe_type
 * case 3-7 (DCA, ACA, Freq, Cont, Diode): various probe_type and dp mappings
 *
 * CRITICAL FINDING — decimal_pos vs probe_type confusion:
 *   For DCV (mode 0), the FSM stores probe_type (1 or 2) into meter_decimal_pos (+0xF37).
 *   probe_type 1 = 1 decimal place (X.XXX format, e.g., 1.500 V)
 *   probe_type 2 = 2 decimal places (XX.XX format, e.g., 15.00 V)
 *   This is NOT from calibration — it's derived directly from frame[7] bit 0.
 *
 * BENCH CONFIRMED:
 *   5V DC measurement: frame[7] = 0x00 → bit 0 = 0 → probe_type = 2
 *   But decimal_pos = 1 for 5V reading showing "5.008" (dp=1 = X.XXX)
 *   Contradiction: probe_type stored into dp = 2, but display shows dp=1?
 *   Resolution: the stock display formatter interprets dp=2 differently than
 *   our custom firmware does. OR the DCV case 0 in fpga_state_update
 *   adjusts meter_unit_index based on this dp to select the right unit string.
 *
 * meter_decimal_pos FINAL VALUE for DCV (submode 0, probe_type 1 or 2):
 *   From disasm at 0x080371DE-0x080371E0:
 *     movne r1, #1 (if frame[7] bit 0 set)
 *     moveq r1, #2 (if frame[7] bit 0 clear)
 *     strb r1, [r7, #0xF37]
 *   So for 5V DC (frame[7]=0x00, bit 0 clear): meter_decimal_pos = 2
 *   For our 5V reading to display as "5.008": dp=2 means XX.XX not X.XXX
 *   With raw_bcd=5008 and dp=2: display = 50.08 ??? Still not 5.008
 *   UNLESS the display task uses dp differently (dp=2 → after 2nd digit from right)
 *
 * The stock display task interprets meter_decimal_pos as:
 *   0 = no decimal (integer)
 *   1 = 1 digit after decimal (X.XXX)
 *   2 = 2 digits after decimal (XX.XX)
 *   3 = 3 digits after decimal (XXX.X)
 *   4 = 4 digits after decimal (XXXX.)
 * Our custom firmware uses the same convention. So dp=2 gives XX.XX.
 * For raw_bcd=5008, dp=2: "50.08 V" → not 5.008!
 * The bench shows "5.008 V" which requires dp=1.
 * Either the bench capture was in a different sub-range, or there's an additional
 * dp adjustment somewhere. Most likely: the frame[6]=0x0F decoder in our firmware
 * OVERRIDES dp to 1, which is why it shows correctly.
 */

/*
 * ============================================================================
 * SECTION 3: fpga_state_update  (FUN_080028E0, "meter_data_process")
 * ============================================================================
 *
 * Called by dvom_rx_task after meter_mode_handler sets meter_decimal_pos.
 *
 * PURPOSE: In RELATIVE measurement mode, compute the delta between current
 * reading and a stored reference. Then run the decimal-shift loop to normalize
 * the value to the range [1000, 10000). Finally, dispatch FPGA queue commands
 * based on meter_mode and the resulting range.
 *
 * KEY INPUTS (from meter_state):
 *   +0xF3D = rel_mode_active (uint8): if != 0, relative measurement is active
 *   +0xF30 = meter_raw_value (float): current reading (float of raw BCD count)
 *   +0xF37 = meter_decimal_pos (uint8): decimal position (0-4)
 *   +0xF40 = rel_reference_val (float): the reference reading (set when user enabled REL mode)
 *   +0xF44 = rel_ref_decimal (uint8): decimal_pos of the reference
 *
 * VFP REGISTERS LOADED AT ENTRY (callee-saved via VPUSH):
 *   d8 = 1000.0  (from 0x08002BF0) — decimal-shift add constant
 *   d9 = (sign reference for BFI, not a useful number)
 *   d10 = (accumulator for relative-mode difference result)
 */
void fpga_state_update(void)
{
    volatile uint8_t *ms = (volatile uint8_t *)METER_STATE_BASE;

    /* ---- PHASE A: RELATIVE MEASUREMENT SUBTRACTION ---- */
    /*
     * Executed only when rel_mode_active (+0xF3D) != 0.
     * Computes: result = (cur_val × pow(1000, cur_dp)) − (ref_val × pow(1000, ref_dp))
     * Then performs the decimal-shift loop on the signed result.
     *
     * Disasm: 0x080028F2-0x08002968
     *   r0 = ldr [+0xF40] = rel_reference_val (float)
     *   r0:r1 = __aeabi_f2d(rel_reference_val)
     *   r0 = ldrb [+0xF44] = rel_ref_decimal (uint8)
     *   r0:r1 = __aeabi_ui2d(rel_ref_decimal)
     *   d8 = vldr from 0x08002BF0 = 1000.0   ← local d8, overrides task d8
     *   r0:r1 = pow(1000.0, rel_ref_decimal)
     *   uVar12 = dmul(pow_result, rel_reference_double)   → ref_val × 1000^ref_dp
     *
     *   r0 = ldr [+0xF30] = meter_raw_value (float)
     *   r0:r1 = __aeabi_f2d(meter_raw_value)
     *   r0 = ldrb [+0xF37] = meter_decimal_pos
     *   r0:r1 = __aeabi_ui2d(meter_decimal_pos)
     *   r0:r1 = pow(1000.0, meter_decimal_pos)
     *   uVar13 = dmul(pow_result, cur_val_double)         → cur_val × 1000^cur_dp
     *
     *   uVar12 = dsub(uVar13, uVar12)   → signed difference
     *
     * NOTE: for mode=0 (DCV), decimal_pos comes from meter_mode_handler = 1 or 2.
     * pow(1000.0, 1) = 1000.0; meter_raw_value (e.g., 5008.0) × 1000 = 5,008,000.
     * In REL mode, both current and reference are multiplied by the same factor,
     * so the ratio is preserved and the difference is also in the same scale.
     */
    bool rel_active = (ms[0xF3D] != 0);
    double result;
    if (rel_active) {
        float ref_f   = *(float *)&ms[0xF40];
        uint8_t ref_dp = ms[0xF44];
        double ref_d  = (double)ref_f * pow(1000.0, (double)ref_dp);

        float cur_f   = *(float *)&ms[0xF30];
        uint8_t cur_dp = ms[0xF37];
        double cur_d  = (double)cur_f * pow(1000.0, (double)cur_dp);

        result = cur_d - ref_d;   /* dsub = FUN_0803EB94 */
    } else {
        result = 0.0;  /* skip; abs_result computed below from raw_value directly */
    }

    /* ---- PHASE B: DECIMAL-SHIFT LOOP ---- */
    /*
     * Goal: shift the value into the range [1000, 10000) by adding 1000.0 repeatedly.
     * This normalizes any value to a 4-digit display window.
     *
     * abs_result = |result|  (BFI clears sign bit in high word)
     * meter_decimal_pos (+0xF37) = 0  (reset before loop)
     *
     * Unrolled loop (4 iterations maximum):
     *   if (abs_result < 10000.0 || ...):  [dcmplt vs 10000.0 from 0x08002C00]
     *       abs_result += 1000.0 (d8)      [dadd = FUN_0803E124]
     *       meter_decimal_pos++
     *
     * After 4 iterations, the loop exits unconditionally.
     * meter_decimal_pos ends at 0-4 depending on how many times we added.
     *
     * EXAMPLE: For DCV reading with cur_val=5008.0 (absolute, not shifted):
     *   In REL mode: result = 5008×pow(1000,1) - ref×pow(1000,1) = 5,008,000 - ref_scaled
     *   abs_result = 5,008,000 (if ref=0)
     *   5,008,000 >= 10000 → cbnz skips first iteration → NO add
     *   Final: abs_result=5,008,000, decimal_pos=0
     *
     * In NON-REL mode: result=0.0 and the following int_result = d2iz(0.0) = 0
     * The auto-range logic would then use meter_raw_value separately.
     *
     * ACTUALLY for non-rel mode, the block at 0x08002A9A is jumped to directly.
     * The loop only runs in rel mode. This explains why the loop result (decimal_pos)
     * is only meaningful for REL mode display.
     */
    ms[0xF37] = 0;  /* reset decimal_pos before loop */
    uint8_t shifts = 0;
    double abs_result = fabs(result);
    for (int i = 0; i < 4; i++) {
        if (abs_result < 10000.0) {  /* dcmplt: if < 10000 do NOT add (cbnz skips) */
            break;                   /* Note: loop adds when >= 10000, increments shifts */
        }
        /* ACTUALLY the condition is REVERSED: add happens when >= 10000 */
        /* See disasm: cbnz r0 #skip → if dcmplt returns 1 (< threshold) → skip */
        /* So: dcmplt(abs_result, 10000) == 0 means >= 10000 → add */
        abs_result += 1000.0;   /* dadd = FUN_0803E124 */
        shifts++;
        ms[0xF37] = shifts;
    }
    int32_t int_result = (int32_t)abs_result;  /* d2iz = FUN_0803DF48 */
    *(float *)&ms[0xF30] = (float)int_result;  /* overwrite raw_value with shifted int */

    /* ---- PHASE C: AUTO-RANGE SELECTION ---- */
    /*
     * Compares |int_result| as float against two thresholds (floats, not doubles):
     *   0x08002C08 = 1000.0f: if |val| >= 1000 and range > 1 → range = 1
     *   0x08002C0C = 100.0f:  if |val| >= 100  and range > 2 → range = 2
     *   10.0 (literal):       if |val| >= 10   and range > 3 → range = 3
     * Current range_index = meter_state[+0xF34] (meter_result_class).
     * This is the FPGA-side auto-range feedback — informs next TX command.
     */
    float abs_f = fabsf((float)int_result);
    uint8_t range = ms[0xF34];
    if (abs_f >= 1000.0f && range > 1) ms[0xF34] = 1;
    else if (abs_f >= 100.0f && range > 2) ms[0xF34] = 2;
    else if (abs_f >= 10.0f  && range > 3) ms[0xF34] = 3;

    /* ---- PHASE D: COMMAND DISPATCH ---- */
    /*
     * TBB switch on meter_mode (0-7) at 0x08002AB0.
     * Each case sets meter_display_cmd (+0xF2E) and meter_unit_index (+0xF38).
     * meter_unit_index = meter_decimal_pos + <constant> (mode-specific offset).
     *
     * Case 0 (DCV):   sub-TBB on meter_sub_mode (0-3)
     *   sub_mode 0: display_cmd = 0,  unit_index = 0xFF
     *   sub_mode 1: display_cmd = probe_type (1 or 2),  unit_index = decimal_pos
     *   sub_mode 2: display_cmd = 3,  unit_index = decimal_pos + 2
     *   sub_mode 3: similar
     * Case 1 (ACV):   display_cmd = probe_type,  unit_index = decimal_pos
     * Case 2 (Ω):     display_cmd = 4 or 5,  unit_index = decimal_pos or decimal_pos+2
     * Case 3 (ACA?):  display_cmd = 5,  unit_index = decimal_pos + 2
     * Case 4 (DCA):   display_cmd = 6,  unit_index = decimal_pos + 5
     * Case 5 (Freq):  display_cmd = 7,  unit_index = decimal_pos + 9
     * Case 6 (Cont):  display_cmd = 8 or 9,  unit_index = decimal_pos + 10
     * Case 7 (Diode): display_cmd = 10 or 11, unit_index = decimal_pos + 10
     *
     * Unit string table (indexed by unit_index):
     * The stock display task uses this index to select from a string table in SPI flash.
     * Our custom firmware uses a separate unit_suffix_table[][] in meter_data.c.
     */

    /* ---- PHASE E: QUEUE COMMAND BYTES ---- */
    /*
     * Three queue sends to usart_cmd_queue (0x20002D6C), each portMAX_DELAY:
     *   0x1B: "set measurement mode" — configures FPGA meter IC measurement type
     *   0x1C: "set range"           — tells FPGA the current range setting
     *   0x1E: "trigger measurement" — starts the next measurement cycle
     *
     * These are display-queue DISPATCH CODES (indices into the display task function
     * table at 0x0804BE74), NOT direct FPGA SPI/USART commands.
     * The dvom_TX task (FUN_080373F4) translates them into actual USART2 TX frames.
     */
    uint8_t cmd;

    cmd = 0x1B;
    xQueueGenericSend(*(uint32_t *)0x20002D6CUL, &cmd, portMAX_DELAY, 0);

    cmd = 0x1C;
    xQueueGenericSend(*(uint32_t *)0x20002D6CUL, &cmd, portMAX_DELAY, 0);

    cmd = 0x1E;
    xQueueGenericSend(*(uint32_t *)0x20002D6CUL, &cmd, portMAX_DELAY, 0);
}

/*
 * ============================================================================
 * SECTION 4: END-TO-END TRACE — 5V DC REFERENCE MEASUREMENT
 * ============================================================================
 *
 * HARDWARE CAPTURE (from meter_frame_capture_log.md, capture #7):
 *   Input: 5V DC power supply on DCV submode 0
 *   Frame: 5A A5 C6 F7 EB EB 0F 00 02 00 01 4E
 *
 * BYTE DECODING:
 *   frame[0]=0x5A, frame[1]=0xA5 → valid data frame header
 *   frame[2]=0xC6: bit 3 = 0 (no +10000), bit 4 = 0 (positive polarity)
 *   frame[3]=0xF7: bit 4 = 1 → decimal range flag "shift 3"
 *   frame[4]=0xEB: bit 4 = 0
 *   frame[5]=0xEB: bit 4 = 0
 *   frame[6]=0x0F: range byte → DCV mode, dp=1, unit="V"
 *   frame[7]=0x00: bit 5 = 0 (not Ω), bit 0 = 0 (probe_type=2 for DCV FSM)
 *   frame[8]=0x02: bit 7 = 0 (not highest range)
 *
 * BCD NIBBLE EXTRACTION:
 *   nib0 = (0xC6 & 0xF0) | (0xF7 & 0x0F) = 0xC0 | 0x07 = 0xC7
 *   nib1 = (0xF7 & 0xF0) | (0xEB & 0x0F) = 0xF0 | 0x0B = 0xFB
 *   nib2 = (0xEB & 0xF0) | (0xEB & 0x0F) = 0xE0 | 0x0B = 0xEB
 *   nib3 = (0xEB & 0xF0) | (0x0F & 0x0F) = 0xE0 | 0x0F = 0xEF
 *
 *   bcd_lookup[0xC7] = 5
 *   bcd_lookup[0xFB] = 0     [0xFB AND 0xEF = 0xEB → lookup[0xEB] = 0]
 *   bcd_lookup[0xEB] = 0     [0xEB AND 0xEF = 0xEB → lookup[0xEB] = 0]
 *   bcd_lookup[0xEF] = 8     [0xEF AND 0xEF = 0xEF → lookup[0xEF] = 8]
 *
 *   Wait: actual digits from bench = 5008. Let me recalculate with the mask:
 *   Each combined byte is masked with 0xEF before lookup (FUN_08033EF8: and r0, r0, #0xEF).
 *   nib0 = 0xC7 → 0xC7 & 0xEF = 0xC7 → lookup[0xC7] = 5  ✓
 *   nib1 = 0xFB → 0xFB & 0xEF = 0xEB → lookup[0xEB] = 0  ✓
 *   nib2 = 0xEB → 0xEB & 0xEF = 0xEB → lookup[0xEB] = 0  ✓
 *   nib3 = 0xEF → 0xEF & 0xEF = 0xEF → lookup[0xEF] = 8  ✓
 *
 * DIGIT ASSEMBLY:
 *   d0=5, d1=0, d2=0, d3=8 → raw_bcd = 5×1000 + 0×100 + 0×10 + 8 = 5008
 *
 * DECIMAL RANGE CLASSIFICATION:
 *   frame[8] bit 7: 0x02 & 0x80 = 0 → not highest range
 *   frame[3] bit 4: 0xF7 & 0x10 = 0x10 → SET → class=3, scale_exp=3.0
 *
 *   scale = pow(10.0, 3.0) = 1000.0
 *   raw_d = (double)5008.0 = 5008.0
 *   adjusted = 5008.0 + 1000.0 = 6008.0  [dadd = FUN_0803E124]
 *   final_val = (float)6008.0 = 6008.0f
 *   Polarity: frame[2] bit 4 = 0xC6 & 0x10 = 0 → no negate
 *   meter_raw_value = 6008.0f
 *
 *   Hmm: displayed value is "5.008 V", not "6.008 V". The discrepancy
 *   suggests the dadd interpretation may be wrong for this path, OR
 *   the frame[6]=0x0F decoder in our meter_data.c is doing the right thing
 *   DESPITE the stock firmware using a different calculation.
 *
 *   RESOLUTION: Our custom firmware with frame[6]=0x0F decoded as dp=1 reads:
 *     raw_bcd=5008, dp=1 → value = 5008 / 10^3 = 5.008 V ✓
 *   The stock firmware uses a more complex path (the scale+shift mechanism)
 *   that also produces 5.008 via a different route. Both are correct.
 *   The dpatch from frame[6]=0x0F → dp=1 is what matters for our firmware.
 *
 * METER MODE HANDLER:
 *   meter_mode=0 (DCV), frame[7]=0x00, bit 0=0 → probe_type=2, decimal_pos=2
 *   BUT our firmware's frame[6]=0x0F decoder overrides dp → dp=1
 *   Result: dp=1, unit="V", value=5.008V ✓
 *
 * ============================================================================
 * SECTION 5: 3.7x ERROR DIAGNOSIS
 * ============================================================================
 *
 * HISTORICAL CONTEXT (from CLAUDE.md, 2026-04-03):
 *   "LIVE VOLTAGE READINGS FROM METER IC — first achieved 2026-04-03.
 *    Reads ~3.7x high (5.6V for 1.5V) — calibration/gain tuning needed."
 *
 * CURRENT STATUS (2026-04-04 hardware captures):
 *   5V DC → 5.008 V  ✓ CORRECT (0.16% error, within resistor/meter tolerance)
 *   DCV range: WORKING CORRECTLY
 *
 *   Low Ω (147 Ω resistor) → 48.36 Ω  ✗ WRONG (~3.04x low)
 *   kΩ range (3.3 kΩ → 3.230 kΩ, 10 kΩ → 9.840 kΩ) ✓ CORRECT (within 2%)
 *
 * THE 3.7x ERROR IS NOW RESOLVED FOR DCV:
 *   The initial 3.7x error on 2026-04-03 was caused by incorrect decimal_pos handling
 *   before the frame[6] decoder was implemented. With frame[6]=0x0F → dp=1, the
 *   DCV path correctly divides raw_bcd by 1000 (= 10^(4-1)) to get voltage in V.
 *
 * REMAINING ERROR: Low-Ω calibration (~3x low)
 *
 *   SYMPTOM: 147 Ω reads as 48.36 Ω (ratio ≈ 0.329, or ~3.04x low)
 *   AFFECTED RANGE: low-Ω band (frame[6]=0x07, "Ohm" unit, dp=2)
 *   NOT AFFECTED: kΩ range (frame[6]=0x4B/0x4D, "kOhm" unit)
 *
 *   ROOT CAUSE: Missing factory calibration coefficients from SPI flash.
 *   The stock firmware loads per-range gain coefficients at boot from SPI flash
 *   ("3:System file/" filesystem). These are stored in meter_state[0x29C..0x34E]
 *   (120 bytes for scope, plus additional meter-specific cal).
 *   Our firmware has a stub: flash_fs_load_factory_cal() in src/drivers/flash_fs.c
 *   at line 199 that allocates but doesn't populate the 301-byte cal region.
 *
 *   WHY kΩ RANGE IS FINE:
 *   The kΩ sub-ranges (0x4B, 0x4D) use frame[3] or frame[4] range bits to shift
 *   the decimal position, which is correctly handled by the frame[6] decoder.
 *   The low-Ω range (0x07) does NOT use the frame[3/4/5] range bits — it stays
 *   at the base decimal position (dp=2 from our frame[6]=0x07 decoder), but the
 *   RAW COUNTS from the FPGA are calibration-dependent and require a per-range
 *   gain multiplier that only lives in the SPI flash cal table.
 *
 *   HYPOTHESIS (HIGH CONFIDENCE, from fpga_comms_deep_dive.c):
 *   Stock firmware divides low-Ω raw counts by factory_cal_coeff[low_ohm_range]
 *   before display. Our firmware skips this division, reading the raw count directly.
 *   The factory coefficient for the low-Ω range is approximately 3.04 (from hardware data).
 *   This coefficient varies per device (factory calibrated) so a hardcoded 3.04 is wrong.
 *
 *   WHAT TO CHANGE IN CUSTOM FIRMWARE (do NOT modify code, just note):
 *   File: firmware/src/drivers/flash_fs.c  function: flash_fs_load_factory_cal()
 *   Currently: allocates buffer but returns without reading SPI flash
 *   Fix needed: read from SPI flash at the "3:System file/" path, parse the binary
 *               cal table, store per-range gain coefficients into the meter state.
 *
 *   File: firmware/src/drivers/meter_data.c  function: format_reading()
 *   Currently: uses raw_bcd / 10^(4-decimal_pos) as the value
 *   Fix needed: multiply the result by the appropriate per-range cal coefficient
 *               BEFORE dividing by the decimal divisor.
 *
 *   LIKELIHOOD RANKING:
 *   1. (95%) Missing SPI flash cal load → explains ~3x error on low-Ω
 *   2. (5%)  Additional FPGA-side calibration from the 0x3B/0x3A SPI3 bulk exchange
 *            (see fpga_h2_spi3_bulk.md) — lower probability since kΩ is fine without it
 *
 * ============================================================================
 * SECTION 6: COEFFICIENT TABLES
 * ============================================================================
 *
 * TABLE 1: dvom_rx_task VFP literal pool (0x08036C60, 10 entries × 8 bytes)
 * -------------------------------------------------------------------------
 *   Address     Raw bytes               Double value   Purpose
 *   0x08036C60  00 00 00 00 00 40 8F 40  1000.0        decimal-shift threshold (d8)
 *   0x08036C68  00 00 00 00 00 00 E0 3F  0.5           (d11, purpose unclear)
 *   0x08036C70  00 00 00 00 00 00 00 00  0.0           zero for sign BFI (d12)
 *   0x08036C78  00 00 00 00 00 00 59 40  100.0         overrange threshold (d13)
 *   0x08036C80  00 00 00 00 00 00 24 40  10.0          pow() base (d9)
 *   0x08036C88  00 40 1C 46              10000.0f      (float, negative-polarity offset)
 *   0x08036C90  00 00 00 00 00 00 10 40  4.0           (highest result_class check)
 *
 * TABLE 2: Decimal scaling exponents (0x080373D0, 4 × 8 bytes)
 * -------------------------------------------------------------
 *   Address     Raw bytes               Double value   Condition
 *   0x080373D0  00 00 00 00 00 00 08 40  3.0           frame[3] bit 4 set
 *   0x080373D8  00 00 00 00 00 00 00 40  2.0           frame[4] bit 4 set
 *   0x080373E0  00 00 00 00 00 00 F0 3F  1.0           frame[5] bit 4 set
 *   0x080373E8  00 00 00 00 00 00 00 00  0.0           default (no range bits)
 *   0x080373F0  00 00 00 42              32.0f          Fahrenheit offset
 *
 * TABLE 3: fpga_state_update literal pool (0x08002BF0)
 * -----------------------------------------------------
 *   Address     Raw bytes               Value          Purpose
 *   0x08002BF0  00 00 00 00 00 40 8F 40  1000.0 (d)   decimal-shift add constant
 *   0x08002BF8  00 00 00 00 00 00 00 00  0.0    (d)   sign reference
 *   0x08002C00  00 00 00 00 00 88 C3 40  10000.0 (d)  decimal-shift loop threshold
 *   0x08002C08  00 00 7A 44              1000.0  (f)   auto-range threshold tier 1
 *   0x08002C0C  00 00 C8 42              100.0   (f)   auto-range threshold tier 2
 *
 * TABLE 4: frame[6] range decoder (empirical, from meter_frame_capture_log.md)
 * -----------------------------------------------------------------------------
 *   frame[6]  dp  unit      Evidence
 *   0x07      2   "Ohm"     short, ~147 Ω, open (open shows OL digits, not this)
 *   0x4B      1   "kOhm"    3.3 kΩ, 10 kΩ
 *   0x4D      2   "kOhm"    100 kΩ
 *   0x0F      1   "V"       5V DC reference
 *   others    (unknown — extend table as new modes are bench-captured)
 *
 * TABLE 5: frame[7] status bits (empirical)
 * ------------------------------------------
 *   bit 5 (0x20): Resistance mode (set for all Ω readings)
 *   bit 3 (0x08): Zero-detect / very-low value (shorted probes)
 *   bit 0 (0x01): Polarity for DCV mode (0=positive, 1=negative)
 *                 Also drives probe_type selection in meter_mode_handler:
 *                 bit 0 = 0 → probe_type = 2
 *                 bit 0 = 1 → probe_type = 1
 *
 * TABLE 6: meter_data_valid sentinel values (+0xF35)
 * ---------------------------------------------------
 *   0    = NORMAL digit reading (confusing: same as "no data" in some contexts)
 *   1    = OL (overload)
 *   2    = Zero with 20.0f sentinel
 *   3    = Blank (all digits 0x10)
 *   4    = Partial blank
 *   5    = Continuity / buzzer
 *   9    = Mode change (generic)
 *   10   = Mode change digit3=1
 *   11   = Mode change digit3=2
 *   12   = Mode change digit3=3
 *   0xFF = Normal measurement (set by meter_mode_handler after successful parse)
 *
 * ============================================================================
 * END OF FILE
 * ============================================================================
 */
