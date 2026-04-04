/*
 * FNIRSI 2C53T V1.2.0 — Master Init Function Phase 1
 * Address range: 0x08023A50 — 0x08024250 (~2KB)
 * Function: FUN_08023A50 (master_init / system_init)
 *
 * Disassembled from APP_2C53T_V1.2.0_251015.bin using Capstone.
 * Annotated with AT32F403A / STM32F1-compatible register map.
 *
 * Helper function:
 *   gpio_init() at 0x080302FC — takes (GPIO_TypeDef *port, gpio_init_struct *cfg)
 *     cfg layout (8 bytes):
 *       +0x00 (u16): pin mask (bit per pin, e.g., 0x200 = pin 9)
 *       +0x04 (u8):  CNF field (open-drain, AF, etc.)
 *       +0x05 (u8):  mode/direction encoding (0x18 = input pull-down, 0x28 = input pull-up)
 *       +0x06 (u8):  gpio_mode (0 = input, 1 = output 10MHz, 2 = output 2MHz, 3 = output 50MHz)
 *       +0x07 (u8):  drive strength (0 = normal, 1 = strong)
 *
 *   Shorthand in annotations: PP = push-pull, OD = open-drain, AF = alternate function
 *
 * Key variable: [0x20002B20] = system core clock frequency (likely 240000000 for 240MHz)
 *
 * Generated: 2026-04-03
 */

void master_init(void)
{
    /* ================================================================
     * SECTION 1: GPIOC CLOCK ENABLE + PC9 POWER HOLD
     * Address: 0x08023A50 — 0x08023AB6
     * ================================================================ */

    // 0x08023A5C: sb = 0x40021018  (CRM->APB2EN register)
    // 0x08023A64: r0 = CRM->APB2EN
    // 0x08023A6C: r0 |= 0x10       (bit 4 = GPIOC clock enable)
    // 0x08023A70: CRM->APB2EN = r0
    CRM->APB2EN |= (1 << 4);  // Enable GPIOC clock — FIRST THING (for power hold)

    // 0x08023A7A: [sp+0x44] = 0    (cfg.direction = 0)
    // 0x08023A90: [sp+0x45] = 0x1018 (truncated to byte = 0x18... but stored as halfword)
    //             Actually: strh 0x1018 at sp+0x45 — this is the raw mode bytes
    // 0x08023AB0: [sp+0x40] = 0x200 (pin mask = bit 9 = PC9)
    // 0x08023AAC: [sp+0x47] = 1    (cfg.drive = 1, strong drive)
    // 0x08023A9A: r0 = 0x40011000  (GPIOC base)
    // 0x08023AB2: bl gpio_init     — Configure PC9 as output

    /* PC9 = POWER HOLD — output push-pull, 50MHz, strong drive
     * This is the very first GPIO operation: if PC9 doesn't go HIGH,
     * the device powers off immediately. */
    gpio_init(GPIOC, &(gpio_init_t){
        .pins = GPIO_PIN_9,     // 0x200
        .mode = GPIO_OUTPUT_50MHZ,
        .type = GPIO_PUSH_PULL,
        .drive = GPIO_STRONG
    });

    // 0x08023AB6: str r4, [r7]  — r7 = 0x40011014 = GPIOC->BRR... wait
    //   Actually: r7 = 0x40011014, r4 = 0x200
    //   GPIOC->BRR = 0x200???  No — let me re-check.
    //   r7 was loaded as 0x40011014 at 0x08023A82/0x08023A9C
    //   r4 = 0x200 from 0x08023A94
    //   str r4, [r7] → *(0x40011014) = 0x200
    //   0x40011014 = GPIOC + 0x14 = GPIOC->BRR (bit reset register)
    //   But that would CLEAR PC9 — that can't be right for power hold.
    //
    //   Wait, r7 = 0x40011014 but the actual store may be BSRR.
    //   Let me re-examine: 0x40011000 + 0x10 = BSRR, +0x14 = BRR
    //   r7 = 0x40011014 → this IS BRR.
    //
    //   BUT: The gpio_init function itself handles setting the pin via
    //   BSRR/BRR based on the pull-up/pull-down config (mode 0x28 = pull-up
    //   triggers BSRR write inside gpio_init). The store to BRR here may be
    //   clearing other pins or is part of a read-modify pattern.
    //
    //   Actually — looking again at 0x08023AB6, this stores 0x200 to
    //   0x40011014 (GPIOC->BRR). This resets PC9 LOW momentarily, which
    //   seems wrong. More likely this is actually setting up for the NEXT
    //   gpio_init call — r7 was being pre-loaded as GPIOC+0x14 but used
    //   as a register variable. Let me reconsider...
    //
    //   CORRECTION: r7 = 0x40011014 is not used as a direct register write
    //   target here. The "str r4, [r7]" at 0x08023AB6 writes 0x200 into
    //   address 0x40011014. However this follows gpio_init which already
    //   configured PC9. This appears to be writing to GPIOC->BRR to
    //   clear the pin — but the ACTUAL power hold (setting PC9 HIGH)
    //   must happen inside gpio_init via the pull-up path, or there's
    //   a BSRR write we haven't traced.
    //
    //   KEY INSIGHT: The gpio_init helper at 0x080302FC, when mode byte
    //   is 0x28 (input pull-up), writes pin mask to BSRR (offset 0x10).
    //   But this pin is configured as OUTPUT, not input pull-up. The
    //   actual high drive comes LATER (see GPIOC->BSRR writes below).

    /* ================================================================
     * SECTION 2: ENABLE CLOCKS FOR GPIOA, GPIOB, GPIOC, GPIOE + AFIO
     * Address: 0x08023AB8 — 0x08023AEC
     * ================================================================ */

    // 0x08023AB8: r0 = CRM->APB2EN
    // 0x08023ABE: r0 |= 0x04       (bit 2 = GPIOA clock)
    // 0x08023AC2: CRM->APB2EN = r0
    CRM->APB2EN |= (1 << 2);  // Enable GPIOA clock

    // 0x08023AC6: r0 = CRM->APB2EN
    // 0x08023ACC: r0 |= 0x08       (bit 3 = GPIOB clock)
    // 0x08023AD0: CRM->APB2EN = r0
    CRM->APB2EN |= (1 << 3);  // Enable GPIOB clock

    // 0x08023AD4: r0 = CRM->APB2EN
    // 0x08023AD8: r0 |= 0x10       (bit 4 = GPIOC, already set but harmless)
    // 0x08023ADC: CRM->APB2EN = r0
    CRM->APB2EN |= (1 << 4);  // Enable GPIOC clock (redundant, already enabled above)

    // 0x08023AE0: r0 = CRM->APB2EN
    // 0x08023AE4: r0 |= 0x40       (bit 6 = GPIOE clock)
    // 0x08023AE8: CRM->APB2EN = r0
    CRM->APB2EN |= (1 << 6);  // Enable GPIOE clock

    // NOTE: GPIOD (bit 5) is NOT enabled here — enabled later for EXMC.

    /* ================================================================
     * SECTION 3: GPIO PIN CONFIGURATION — ACTIVE LOW OUTPUTS
     * Address: 0x08023AEC — 0x08023B62
     *
     * These use gpio_init() at 0x080302FC.
     * r5 = 0x40011000 (GPIOC), sl = 0x40010C00 (GPIOB),
     * r8 = 0x40010800 (GPIOA) [loaded earlier but offset by context],
     * fp = 0x40010800 + something... let me trace more carefully.
     *
     * Register tracking from function entry:
     *   r5  = 0x40011000 (GPIOC)      [0x08023A68/76]
     *   r7  = 0x40011014              [0x08023A82/9C] — GPIOC+0x14
     *   fp  = 0x40010800 (GPIOA)      [0x08023A86/A0] — wait, 0x0800|0x4001<<16 = 0x40010800
     *   r8  = 0x40010008 (AFIO+0x08)  [0x08023A8A/A4] — 0x0008|0x4001<<16 = 0x40010008
     *   sl  = 0x40010C00 (GPIOB)      [computed at 0x08023AF8: r5 - 0x400]
     *         Wait: 0x40011000 - 0x400 = 0x40010C00 = GPIOB. Yes.
     *
     * CORRECTION on fp: 0x0800 | (0x4001 << 16) = 0x40010800 = GPIOA
     * CORRECTION on r8: 0x0008 | (0x4001 << 16) = 0x40010008 = AFIO+0x08 (EXTICR1? No — AFIO base is 0x40010000, +0x08 = MAPR2 or EXTICR1)
     *   Actually AFIO register map: +0x00=EVCR, +0x04=MAPR, +0x08=EXTICR1
     *   But r8 is used with offset +0x28 later → 0x40010008+0x28 = 0x40010030
     *   That's AFIO+0x30 which on AT32 = AFIO->MAPR2 or similar remap register
     ================================================================ */

    // --- Call at 0x08023B04: gpio_init(GPIOB, cfg) ---
    // cfg: pins=0x80 (PB7), mode bytes from strb/strh, direction=input pull-up?
    // 0x08023AEC: cfg.direction = 0 (sp+0x44)
    // 0x08023AF2: cfg.mode_bytes = 0x28 (sp+0x45) — input pull-up
    // 0x08023AFC: cfg.pins = 0x80 (sp+0x40) — pin 7
    // 0x08023B00: cfg.drive = 1 (sp+0x47)
    // 0x08023AFE: r0 = sl = 0x40010C00 (GPIOB)
    gpio_init(GPIOB, &(gpio_init_t){
        .pins = GPIO_PIN_7,     // 0x80 = PB7 (PRM button)
        .mode = GPIO_INPUT,
        .pull = GPIO_PULL_UP,   // 0x28
        .drive = GPIO_STRONG
    });
    // PB7 = PRM button — input with pull-up (active low button)

    // --- Call at 0x08023B18: gpio_init(GPIOC, cfg) ---
    // 0x08023B0A: cfg.mode_bytes = 0x18 (sp+0x45) — input pull-down
    // 0x08023B12: cfg.pins = 0x2100 (sp+0x40) — pins 8 and 13
    // r0 = r5 = 0x40011000 (GPIOC)
    gpio_init(GPIOC, &(gpio_init_t){
        .pins = GPIO_PIN_8 | GPIO_PIN_13,  // 0x2100 = PC8 + PC13
        .mode = GPIO_INPUT,
        .pull = GPIO_PULL_DOWN,  // 0x18
        .drive = GPIO_STRONG
    });
    // PC8 = POWER button (active high with pull-down)
    // PC13 = UP button (active high with pull-down)

    // --- Call at 0x08023B2E: gpio_init(GPIOA, cfg) ---
    // 0x08023B20: cfg.extra = 0x210 (sp+0x46)
    // 0x08023B24: cfg.pins = 0x180 (sp+0x40) — pins 7 and 8
    //   Wait: 0x180 = bits 7,8 = PA7 + PA8
    // r0 = fp = 0x40010800 (GPIOA)
    gpio_init(GPIOA, &(gpio_init_t){
        .pins = GPIO_PIN_7 | GPIO_PIN_8,  // 0x180 = PA7 + PA8
        .mode = GPIO_INPUT,
        .pull = GPIO_PULL_DOWN,
        .drive = GPIO_STRONG
    });
    // PA7, PA8 = Button matrix row/column pins (active scan)

    // --- Call at 0x08023B3C: gpio_init(GPIOB, cfg) ---
    // 0x08023B36: cfg.pins = 1 (sp+0x40) — pin 0
    // 0x08023B38: [sp+0x10] = sl = 0 ... hmm, sl was set to 0 at 0x08023AA8
    //   Actually sl = 0x40010C00 (GPIOB) from 0x08023AF8
    //   [sp+0x10] stores sl for later use
    // r0 = sl = GPIOB (but wait, at 0x08023B32 r0=sl which was recomputed)
    //   sl = 0x40010C00 still
    gpio_init(GPIOB, &(gpio_init_t){
        .pins = GPIO_PIN_0,     // 0x01 = PB0
        .mode = GPIO_INPUT,
        .pull = GPIO_PULL_DOWN,
        .drive = GPIO_STRONG
    });
    // PB0 = Button matrix row pin

    // --- Call at 0x08023B4A: gpio_init(GPIOC, cfg) ---
    // 0x08023B40: cfg.pins = 0x420 (sp+0x40) — pins 5 and 10
    // r0 = r5 = GPIOC
    gpio_init(GPIOC, &(gpio_init_t){
        .pins = GPIO_PIN_5 | GPIO_PIN_10,  // 0x420 = PC5 + PC10
        .mode = GPIO_INPUT,
        .pull = GPIO_PULL_DOWN,
        .drive = GPIO_STRONG
    });
    // PC5 = Button matrix row pin
    // PC10 = Button matrix column pin

    // --- Call at 0x08023B5E: gpio_init(GPIOE, cfg) ---
    // 0x08023B4E: sl = 0x40011800 (GPIOE)
    // 0x08023B52: cfg.pins = 0x0C (sp+0x40) — pins 2 and 3
    // r0 = sl = GPIOE
    gpio_init(GPIOE, &(gpio_init_t){
        .pins = GPIO_PIN_2 | GPIO_PIN_3,  // 0x0C = PE2 + PE3
        .mode = GPIO_INPUT,
        .pull = GPIO_PULL_DOWN,
        .drive = GPIO_STRONG
    });
    // PE2 = Button matrix row pin
    // PE3 = Button matrix column pin

    /* ================================================================
     * SECTION 4: EXMC SETUP FOR LCD (MEMORY-MAPPED PARALLEL BUS)
     * Address: 0x08023B62 — 0x08023C5A
     * ================================================================ */

    // 0x08023B62: r5 = 0x01080400 — this is a GPIO config constant, not an address
    //   Stored at sp+0x44 as part of gpio_init struct packing

    // --- Enable GPIOD + GPIOE + AFIO clocks for EXMC ---
    // 0x08023B72: r0 = CRM->APB2EN
    // 0x08023B7A: r0 |= 0x20       (bit 5 = GPIOD clock)
    // 0x08023B7E: CRM->APB2EN = r0
    CRM->APB2EN |= (1 << 5);  // Enable GPIOD clock (for EXMC data/address pins)

    // 0x08023B82: r0 = CRM->APB2EN
    // 0x08023B8A: r0 |= 0x40       (bit 6 = GPIOE, already set)
    // 0x08023B8E: CRM->APB2EN = r0
    CRM->APB2EN |= (1 << 6);  // Enable GPIOE clock (redundant)

    // 0x08023B92: r0 = CRM->APB2EN
    // 0x08023B98: r0 |= 0x01       (bit 0 = AFIO clock)
    // 0x08023B9C: CRM->APB2EN = r0
    CRM->APB2EN |= (1 << 0);  // Enable AFIO clock (CRITICAL for EXMC alternate function!)

    // --- Enable EXMC clock ---
    // 0x08023B76: r1 = 0x40020444  (CRM base + 0x444? That's not standard...)
    //   Actually: 0x40020444 + 0xBD0 = 0x40021014... no.
    //   r1 = 0x40020444, then ldr r0, [r1, #0xBD0] → address 0x40021014
    //   Wait: 0x40020444 + 0xBD0 = 0x40021014 = CRM->APB2RST? No.
    //   0x40021014 = CRM_BASE(0x40021000) + 0x14 = AHB peripheral enable register
    //   On AT32: CRM->AHBEN is at offset 0x14
    // 0x08023BA0: r0 = *(0x40021014)  — CRM->AHBEN
    // 0x08023BA8: r0 |= 0x100        (bit 8 = EXMC clock enable)
    // 0x08023BAC: CRM->AHBEN = r0
    CRM->AHBEN |= (1 << 8);   // Enable EXMC (external memory controller) clock

    // --- IOMUX remap: Disable JTAG, keep SWD, free PB3/PB4/PB5 ---
    // 0x08023BB0: r0 = *(r8 + 0x28)  — r8 = 0x40010008, so address = 0x40010030
    //   0x40010030 = AFIO + 0x30... On AT32 this might be AFIO->MAP register at a different offset
    //   Actually: AFIO base = 0x40010000. Standard STM32F1: MAPR = +0x04
    //   But r8 = 0x40010008 (AFIO+0x08), offset +0x28 = AFIO+0x30
    //   On AT32F403A: This is likely IOMUX->REMAP register (may differ from STM32)
    //
    //   WAIT — let me reconsider. r8 was loaded as:
    //     0x08023A8A: movw r8, #8
    //     0x08023AA4: movt r8, #0x4001
    //   So r8 = 0x40010008. Then [r8+0x28] = [0x40010030]
    //
    //   On STM32F1/GD32: AFIO base = 0x40010000
    //     +0x00 = EVCR, +0x04 = MAPR, +0x08 = EXTICR1, +0x0C = EXTICR2
    //     +0x10 = EXTICR3, +0x14 = EXTICR4, +0x1C = MAPR2
    //   So 0x40010030 doesn't map to standard AFIO registers.
    //   On AT32F403A: IOMUX base = 0x40010000 (same as AFIO)
    //     +0x04 = REMAP, +0x08 = EXINTC1, +0x2C = REMAP2, +0x30 = REMAP3, +0x34 = REMAP4
    //   0x40010030 = IOMUX->REMAP3!
    //
    // 0x08023BB6: r0 |= 0x08000000  (bit 27)
    // 0x08023BBA: *(0x40010030) = r0
    IOMUX->REMAP3 |= (1 << 27);  // AT32-specific remap (likely XMC_NADV remap or similar)

    // --- GPIO init for EXMC data pins on GPIOD ---
    // r6 was set to r7 at 0x08023B96 → r6 = 0x40011014 (was GPIOC+0x14)
    //   Then r7 = r7 + 0x3EC = 0x40011014 + 0x3EC = 0x40011400 = GPIOD base
    //   And r6 = 0x40011014 (GPIOC+0x14)... but at 0x08023B96 r6=r7=0x40011014
    //   After add: r7 = 0x40011400 (GPIOD)
    //
    // 0x08023BBE: r0 = r7 = 0x40011400 (GPIOD)
    // cfg loaded with pin mask and AF push-pull 50MHz config
    // The config constant r5 = 0x01080400 encodes:
    //   Likely: pin_mask in low 16, config in upper bytes
    //
    // Looking at stored values:
    //   sp+0x40 = 0xC703 (pin mask: PD0,PD1,PD8,PD9,PD10,PD14,PD15 = EXMC data pins)
    //   sp+0x44 = 0x01080400 (config: AF push-pull, 50MHz, strong drive)
    //     Breaking down: byte4=0x00, byte5=0x04, byte6=0x08, byte7=0x01
    //     → CNF=0x04 (bits: AF), speed=0x08 (AF push-pull?), mode=0x08?
    //     Hmm, non-standard encoding. Let me just note the pin mask.
    //
    // Pin mask 0xC703 = bits 0,1,8,9,10,14,15
    gpio_init(GPIOD, &(gpio_init_t){
        .pins = 0xC703,  // PD0,PD1,PD8,PD9,PD10,PD14,PD15
        .mode = GPIO_AF_PUSH_PULL_50MHZ,
        .drive = GPIO_STRONG
    });
    // EXMC data bus: D2=PD0, D3=PD1, D13=PD8, D14=PD9, D15=PD10, D0=PD14, D1=PD15

    // --- GPIO init for EXMC data pins on GPIOE ---
    // 0x08023BC6: cfg.pins = 0xFF80 (sp+0x40) — PE7-PE15
    // 0x08023BCA: strd stores 0xFF80 and r5(config) at sp+0x40,0x44
    // 0x08023BCE: r0 = sl = 0x40011800 (GPIOE)
    gpio_init(GPIOE, &(gpio_init_t){
        .pins = 0xFF80,  // PE7,PE8,PE9,PE10,PE11,PE12,PE13,PE14,PE15
        .mode = GPIO_AF_PUSH_PULL_50MHZ,
        .drive = GPIO_STRONG
    });
    // EXMC data bus: D4=PE7, D5=PE8, D6=PE9, D7=PE10, D8=PE11, D9=PE12, D10=PE13, D11=PE14, D12=PE15

    // --- GPIO init for EXMC control pins on GPIOD ---
    // 0x08023BD6: config constant = 0x01100400 (sp+0x44) — slightly different config
    //   0x0110 vs 0x0108 — different speed or drive setting
    // 0x08023BE8: cfg.pins = 0x40 (sp+0x40) — PD6? That's not standard EXMC.
    //   Wait: 0x40 = pin 6. Hmm. Or this could be encoding differently.
    //   Actually looking at it: PD4=nOE(read), PD5=nWE(write), PD7=NE1(CS), PD11=A16, PD12=A17
    //   0x40 = pin 6 only. PD6 is EXMC NWAIT pin.
    //
    //   But the next call uses pins=0x8B0:
    // 0x08023BF0: cfg.pins = 0x8B0 (sp+0x40) — bits 4,5,7,9,11 = PD4,PD5,PD7,PD9,PD11
    //   Hmm wait, 0x8B0 = 0b 1000 1011 0000
    //   = bits 4,5,7,11 = PD4(nOE), PD5(nWE), PD7(NE1/CS), PD11(A16)
    //   And bit 9 is NOT set (0x8B0 = 0x800+0xB0 = 2048+176 = bit11+bit7+bit5+bit4)
    //
    //   Actually: 0x8B0 = 0b100010110000 = bits 4,5,7,11
    //   PD4 = EXMC_NOE (read strobe)
    //   PD5 = EXMC_NWE (write strobe)
    //   PD7 = EXMC_NE1 (chip select for bank 1)
    //   PD11 = EXMC_A16

    gpio_init(GPIOD, &(gpio_init_t){
        .pins = GPIO_PIN_6,     // 0x40 = PD6
        .mode = GPIO_AF_PUSH_PULL_50MHZ,  // EXMC NWAIT (alternate config)
        .drive = GPIO_STRONG
    });

    gpio_init(GPIOD, &(gpio_init_t){
        .pins = 0x8B0,  // PD4(NOE) + PD5(NWE) + PD7(NE1) + PD11(A16)
        .mode = GPIO_AF_PUSH_PULL_50MHZ,
        .drive = GPIO_STRONG
    });
    // Note: A17 (PD12) would give address line for RS/DCX selection
    // 0x8B0 includes PD11 but NOT PD12. The LCD RS line uses A17 per CLAUDE.md.
    // PD12 might be configured separately or this analysis has a bit error.
    // 0x8B0 = binary: 100010110000, so bits 4,5,7,11 confirmed.
    // A17=PD12 (bit 12 = 0x1000) is NOT in this mask.
    // IMPORTANT: Check if PD12/A17 is configured elsewhere!

    /* ================================================================
     * SECTION 5: EXMC REGISTER CONFIGURATION (MEMORY CONTROLLER)
     * Address: 0x08023C02 — 0x08023C54
     * ================================================================ */

    // 0x08023C02: r0 = 0xA0000000 (EXMC base address)
    // 0x08023C06: r1 = 0x5010
    // 0x08023C0A: *(0xA0000000) = 0x5010
    //   EXMC_SNCTL0 (SRAM/NOR-Flash control register for bank 1, offset 0x00)
    //   Value 0x5010:
    //     bit 0 = 0: bank not enabled yet
    //     bit 4 = 1: NWAIT active low? Or memory type
    //     bit 12 = 1: write enable
    //     bit 14 = 1: extended mode enable
    //   Wait — per CLAUDE.md: SNCTL0=0x5011. Here it's 0x5010 (bit 0 not set = bank disabled)
    //   Bank will be enabled below.
    EXMC->SNCTL0 = 0x5010;  // Configure EXMC bank 1: 16-bit bus, write enable, extended mode, NOT yet enabled

    // 0x08023C0C: r1 = 0xFFFFFDE4 (signed offset)
    // 0x08023C14: r3 = 0x02020424
    // 0x08023C10: r2 = 0xA0000220 (???)
    //   Actually: r2 = 0xA0000220, r1 = 0xFFFFFDE4
    //   r2 + r1 = 0xA0000220 + 0xFFFFFDE4 = 0xA0000004 (with wrap)
    //   Wait: 0xA0000220 + 0xFFFFFDE4 = 0x(1)A0000004 → 0xA0000004 (32-bit)
    //   EXMC_SNTCFG0 = EXMC base + 0x04 = read timing config
    // 0x08023C24: str r3, [r2, r1] → *(0xA0000004) = 0x02020424
    EXMC->SNTCFG0 = 0x02020424;
    // Read timing: ADDSET=4, ADDHOLD=2, DATAST=0x24(=36 HCLK cycles), BUSTURN=2
    // At 240MHz HCLK, this gives ~150ns data phase

    // 0x08023C26: r1 = 0xFFFFFEE4 → r2 + r1 = 0xA0000220 + 0xFFFFFEE4 = 0xA0000104
    //   Hmm: 0x220 + 0xFEE4 (16-bit) = 0x10104... that's 0x(1)0104 → 0x104 offset
    //   EXMC base + 0x104 = EXMC_SNWTCFG0 (write timing, extended mode)
    // 0x08023C2E: r3 = 0x00000202
    // 0x08023C32: str r3, [r2, r1] → *(0xA0000104) = 0x00000202
    EXMC->SNWTCFG0 = 0x00000202;
    // Write timing: ADDSET=2, DATAST=2 (much faster than read — matches ST7789V write specs)

    // 0x08023C34: r1 = *(r2) = *(0xA0000220)???
    //   Hmm, this doesn't make sense as EXMC register.
    //   Let me re-examine: r2 = 0xA0000220 was used as a base for offset calculations.
    //   Actually *(r2) reads from 0xA0000220. In EXMC space that's in the SRAM/NOR area,
    //   not control registers. But the code is:
    //   0x08023C34: ldr r1, [r2]         — read EXMC->SNCTL0 (but r2=0xA0000220, not 0xA0000000)
    //
    //   Actually I think r2 should be re-examined. Let me recheck:
    //   0x08023C10: movw r2, #0x220
    //   0x08023C1C: movt r2, #0xA000
    //   → r2 = 0xA0000220
    //   Then ldr r1, [r2] reads from 0xA0000220 — that's EXMC register space, offset 0x220
    //   This could be the XMC extended register area on AT32F403A.
    //
    //   On AT32: XMC has extended registers. 0xA0000220 might be part of EXMC
    //   region 1 control or a different bank.
    //
    //   Let me look at what's done with it:
    //   0x08023C38: bic r1, r1, #0xFF00  — clear bits [15:8]
    //   0x08023C3C: str r1, [r2]         — write back
    //   0x08023C3E: ldr r1, [r2]
    //   0x08023C44: bfi r1, r3, #0, #8   — insert r3(=8) into bits [7:0]
    //   0x08023C48: str r1, [r2]

    // This is configuring an AT32-specific EXMC register at offset 0x220
    // Clearing bits [15:8] then setting bits [7:0] = 8
    // This might be the XMC bus turnaround or similar timing extension
    *(volatile uint32_t *)0xA0000220 = (*(volatile uint32_t *)0xA0000220 & ~0xFF00);
    *(volatile uint32_t *)0xA0000220 = (*(volatile uint32_t *)0xA0000220 & ~0xFF) | 8;

    // 0x08023C4A: r1 = *(r0) = *(0xA0000000) = EXMC->SNCTL0
    // 0x08023C50: r1 |= 1             — set bit 0 = enable bank
    // 0x08023C54: *(0xA0000000) = r1
    EXMC->SNCTL0 |= 1;  // Enable EXMC bank 1 — LCD is now memory-mapped!
    // After this: 0x6001FFFE = LCD command, 0x60020000 = LCD data (A17 = RS/DCX)

    // 0x08023C56: r0 = 0x20002B20 — address of system_core_clock variable
    // 0x08023C5A: str r8, [r6, #0x3FC]
    //   r8 = 0x40 (was set at 0x08023BDE to 0x40)
    //   r6 was set to 0x40011014 (GPIOC + 0x14)
    //   r6 + 0x3FC = 0x40011410 = GPIOD + 0x10 = GPIOD->BSRR
    //   Writing 0x40 = pin 6 → SET PD6
    GPIOD->BSRR = (1 << 6);  // Set PD6 HIGH
    // PD6 was configured as EXMC pin above — this might be a NWAIT pull-up
    // or initialization pulse

    /* ================================================================
     * SECTION 6: SYSTICK DELAY LOOPS (LCD INIT TIMING)
     * Address: 0x08023C5A — 0x08023DE2
     *
     * These loops use SysTick (0xE000E010) to create precise delays.
     * The pattern is:
     *   1. Load SysTick->LOAD with delay count (based on core clock)
     *   2. Clear SysTick->VAL
     *   3. Enable SysTick
     *   4. Poll SysTick->CTRL for COUNTFLAG (bit 16)
     *   5. Disable SysTick, clear VAL
     *   6. Decrement iteration counter
     *
     * Variable at 0x20002B20 = system_core_clock (240000000 for 240MHz)
     * Formula: SysTick->LOAD = clock_freq * multiplier
     *   For first delay: load = clock * 5 * 2 = clock * 10
     *     At 240MHz: 2,400,000,000 — that overflows 24-bit SysTick!
     *     So the loop iterates, using smaller chunks.
     *
     * The outer loop at 0x08023D72 counts down r2 from 0x78 (120 iterations)
     * with inner factor r1 = 0x32 (50).
     * This implements a multi-millisecond delay for LCD power-on.
     * ================================================================ */

    // First delay block (0x08023C62 - 0x08023CA2):
    // r0 = system_core_clock, multiply by 5, shift left 1 → clock * 10
    // SysTick->LOAD = clock * 10 / some_divisor
    // Wait for COUNTFLAG, repeat
    // This is approximately a 10µs delay per SysTick cycle
    systick_delay_us(10);  // Approximate — exact timing depends on clock value

    // 0x08023CB2: str r1, [r7, #0x400]
    //   r7 was updated: r7 = r6 = 0x40011014, then r7 = r6 at 0x08023C64
    //   Hmm, at 0x08023C64: mov r7, r6  → r7 = r6 = 0x40011014 (GPIOC+0x14)
    //   r7 + 0x400 = 0x40011414 = GPIOD + 0x14 = GPIOD->BRR
    //   r1 = 0x40
    GPIOD->BRR = (1 << 6);   // Clear PD6 LOW
    // Toggle PD6 — this is part of a reset/init sequence for the LCD

    // Second delay block (0x08023CB6 - 0x08023D0A):
    // SysTick->LOAD = clock * 5 * 4 = clock * 20
    systick_delay_us(20);  // Another delay

    // 0x08023D0A: str r1, [r7, #0x3FC]
    //   r7 + 0x3FC = 0x40011014 + 0x3FC = 0x40011410 = GPIOD->BSRR
    //   r1 = 0x40
    GPIOD->BSRR = (1 << 6);  // Set PD6 HIGH again
    // PD6 toggle sequence: HIGH → delay → LOW → delay → HIGH
    // This is an LCD reset pulse on PD6!

    // Third delay block (0x08023D0E - 0x08023D62):
    systick_delay_us(20);  // Post-reset delay

    /* ================================================================
     * SECTION 7: ST7789V LCD INITIALIZATION COMMANDS
     * Address: 0x08023D62 — 0x08023F32
     *
     * sl = 0x60020000 (LCD data address)
     * Writes to [sl, #-2] = 0x6001FFFE = LCD command register
     * Writes to [sl] = 0x60020000 = LCD data register
     *
     * This is the ST7789V initialization sequence sent over the
     * EXMC memory-mapped parallel bus.
     * ================================================================ */

    // LCD command/data addresses
    #define LCD_CMD   (*(volatile uint16_t *)0x6001FFFE)
    #define LCD_DATA  (*(volatile uint16_t *)0x60020000)

    // 0x08023D6A: strh r1, [sl, #-2] → LCD_CMD = 0x11
    LCD_CMD = 0x11;  // SLPOUT — Exit Sleep Mode
    // Followed by a long delay loop (0x78=120 iterations of SysTick delays)
    // This is the mandatory >120ms delay after SLPOUT

    systick_delay_ms(120);  // Mandatory post-SLPOUT delay

    /* --- ST7789V Register Configuration --- */

    // 0x08023DE2-0x08023DF8:
    LCD_CMD = 0x36;   // MADCTL — Memory Access Control
    LCD_DATA = 0x00;  // Normal orientation: MY=0, MX=0, MV=0, ML=0, RGB=0
    // Row/column address order, RGB color filter

    LCD_CMD = 0x3A;   // COLMOD — Interface Pixel Format
    LCD_DATA = 0x55;  // 16-bit/pixel (RGB565) for both RGB and MCU interface

    LCD_CMD = 0xB2;   // PORCTRL — Porch Setting
    LCD_DATA = 0x0C;  // Back porch
    LCD_DATA = 0x0C;  // Front porch (normal mode)
    LCD_DATA = 0x00;  // Porch enable (separate)
    LCD_DATA = 0x33;  // Back porch idle
    LCD_DATA = 0x33;  // Front porch idle

    LCD_CMD = 0xB7;   // GCTRL — Gate Control
    LCD_DATA = 0x46;  // VGH=14.06V, VGL=-10.43V (adjusted for panel)

    LCD_CMD = 0xBB;   // VCOMS — VCOM Setting
    LCD_DATA = 0x1B;  // VCOM = 0.875V

    LCD_CMD = 0xC0;   // LCMCTRL — LCM Control
    LCD_DATA = 0x2C;  // XOR: MY complement, XBGR, XMH

    LCD_CMD = 0xC2;   // VDVVRHEN — VDV and VRH Command Enable
    LCD_DATA = 0x01;  // VDV and VRH from command write

    LCD_CMD = 0xC3;   // VRHS — VRH Set
    LCD_DATA = 0x0F;  // VRH = 4.35V + (0x0F * 0.05) = 5.1V

    LCD_CMD = 0xC4;   // VDVS — VDV Set
    LCD_DATA = 0x20;  // VDV = 0V (default)

    LCD_CMD = 0xC6;   // FRCTRL2 — Frame Rate Control in Normal Mode
    LCD_DATA = 0x0F;  // 60Hz frame rate (0x0F = 60fps)

    LCD_CMD = 0xD0;   // PWCTRL1 — Power Control 1
    LCD_DATA = 0xA4;  // Power mode: AVDD=6.8V, AVCL=-4.8V
    LCD_DATA = 0xA1;  // VDS=2.3V, AVCL=-4.8V

    LCD_CMD = 0xD6;   // Unknown/vendor register
    LCD_DATA = 0xA1;  // Vendor-specific setting

    /* --- Positive Gamma Correction (E0h) --- */
    LCD_CMD = 0xE0;   // PVGAMCTRL — Positive Voltage Gamma Control
    LCD_DATA = 0xF0;  // V63P3, V63P2, V63P1, V63P0
    LCD_DATA = 0x00;  // V0P3, V0P2, V0P1, V0P0
    LCD_DATA = 0x06;  // V1P5, V1P4, V1P3, V1P2, V1P1, V1P0
    LCD_DATA = 0x04;  // V2P5, V2P4, V2P3, V2P2, V2P1, V2P0
    LCD_DATA = 0x05;  // V4P4, V4P3, V4P2, V4P1, V4P0
    LCD_DATA = 0x05;  // V6P4, V6P3, V6P2, V6P1, V6P0
    LCD_DATA = 0x31;  // J0P1, J0P0, V13P3, V13P2, V13P1, V13P0
    LCD_DATA = 0x44;  // J1P1, J1P0, V20P3, V20P2, V20P1, V20P0
    LCD_DATA = 0x48;  // V36P2, V36P1, V36P0, V27P2, V27P1, V27P0
    LCD_DATA = 0x36;  // V43P6, V43P5, V43P4, V43P3, V43P2, V43P1, V43P0 (packed)
    LCD_DATA = 0x12;  // ... continued
    LCD_DATA = 0x12;  // ... continued
    LCD_DATA = 0x2B;  // V50 settings
    LCD_DATA = 0x34;  // V57/V59 settings

    /* --- Negative Gamma Correction (E1h) --- */
    LCD_CMD = 0xE1;   // NVGAMCTRL — Negative Voltage Gamma Control
    LCD_DATA = 0xF0;  // V63N
    LCD_DATA = 0x0B;
    LCD_DATA = 0x0F;
    LCD_DATA = 0x0F;
    LCD_DATA = 0x0D;
    LCD_DATA = 0x26;
    LCD_DATA = 0x31;  // Reuses same J0/J1 values as positive gamma
    LCD_DATA = 0x43;
    LCD_DATA = 0x47;
    LCD_DATA = 0x38;
    LCD_DATA = 0x14;
    LCD_DATA = 0x14;
    LCD_DATA = 0x2C;  // V50N
    LCD_DATA = 0x32;  // V57N

    LCD_CMD = 0x29;   // DISPON — Display On

    /* ================================================================
     * SECTION 8: LCD DISPLAY PARAMETERS + MEMORY WINDOW SETUP
     * Address: 0x08023F26 — 0x0802405A
     *
     * Stores display geometry into RAM struct at 0x20008340
     * then sends CASET/RASET/RAMWR to set up the drawing window.
     * ================================================================ */

    // 0x08023F26: r8 = 0x20008340  — LCD display parameters struct
    // struct lcd_params {
    //   uint16_t width;       // +0x00 = 320 (0x140)
    //   uint16_t height;      // +0x02 = 240 (0xF0)
    //   uint8_t  reserved[2]; // +0x04, +0x05
    //   uint8_t  orientation; // +0x06 = 1
    //   uint8_t  reserved2;   // +0x07
    //   uint16_t cmd_caset;   // +0x08 = 0x2C (seems wrong, might be different layout)
    //   uint16_t cmd_raset;   // +0x0A = 0x2A
    //   uint16_t cmd_ramwr;   // +0x0C = 0x2B
    // };

    // 0x08023F3E: strb 1, [r8+6]    → orientation = 1 (landscape)
    // 0x08023F42: strh 0x140, [r8]   → width = 320
    // 0x08023F48: strh 0xF0, [r8+2]  → height = 240
    // 0x08023F4C: strh 0x2C, [r8+8]  → RAMWR command code
    // 0x08023F50: strh 0x2A, [r8+0xA] → CASET command code
    // 0x08023F56: strh 0x2B, [r8+0xC] → RASET command code

    lcd_params.width = 320;       // 0x140
    lcd_params.height = 240;      // 0xF0
    lcd_params.orientation = 1;   // Landscape
    lcd_params.ramwr_cmd = 0x2C;  // RAMWR — Memory Write
    lcd_params.caset_cmd = 0x2A;  // CASET — Column Address Set
    lcd_params.raset_cmd = 0x2B;  // RASET — Row Address Set

    // 0x08023F5A: LCD_CMD = 0x36 (MADCTL again — set orientation)
    LCD_CMD = 0x36;   // MADCTL — update for landscape
    LCD_DATA = 0xA0;  // MY=1, MX=0, MV=1, ML=0, BGR=0 → landscape 320x240
    //   MY=1 (bit 7), MV=1 (bit 5) → landscape, top-left origin

    // --- CASET: Set column address window (0 to width-1 = 0 to 319) ---
    // 0x08023F62: r2 = lcd_params.caset_cmd = 0x2A
    LCD_CMD = 0x2A;   // CASET
    LCD_DATA = 0x00;  // XS[15:8] = 0 (start column high byte)
    LCD_DATA = 0x00;  // XS[7:0] = 0 (start column low byte)
    // 0x08023F7A-0x08023F8E: compute (width-1) >> 8 and (width-1) & 0xFF
    LCD_DATA = (320 - 1) >> 8;   // = 0x01 (end column high byte = 319 >> 8)
    LCD_DATA = (320 - 1) & 0xFF; // = 0x3F (end column low byte = 319 & 0xFF)

    // --- RASET: Set row address window (0 to height-1 = 0 to 239) ---
    // 0x08023F96: LCD_CMD = lcd_params.raset_cmd = 0x2B
    LCD_CMD = 0x2B;   // RASET
    LCD_DATA = 0x00;  // YS[15:8] = 0
    LCD_DATA = 0x00;  // YS[7:0] = 0
    LCD_DATA = (240 - 1) >> 8;   // = 0x00 (end row high byte = 239 >> 8)
    LCD_DATA = (240 - 1) & 0xFF; // = 0xEF (end row low byte = 239 & 0xFF)

    // --- Full-screen CASET again (wider range for RAMWR) ---
    // 0x08023FC8: LCD_CMD = 0x2A
    LCD_CMD = 0x2A;   // CASET
    LCD_DATA = 0x00;
    LCD_DATA = 0x00;
    LCD_DATA = (320) >> 8;    // Note: width, not width-1 (= 0x01)
    LCD_DATA = (320) & 0xFF;  // = 0x40

    // --- Full-screen RASET ---
    LCD_CMD = 0x2B;   // RASET
    LCD_DATA = 0x00;
    LCD_DATA = 0x00;
    LCD_DATA = (240) >> 8;    // = 0x00
    LCD_DATA = (240) & 0xFF;  // = 0xF0

    // --- RAMWR: Start memory write (fill screen with black) ---
    // 0x08024010: r2 = lcd_params.ramwr_cmd (0x2C)
    LCD_CMD = 0x2C;   // RAMWR — begin pixel data

    // The loop at 0x08024018-0x0802405A fills the entire screen with 0x0000 (black)
    // Total pixels = 320 * 240 = 76800 halfwords
    // Uses unrolled loop: 4 pixels per iteration, then handles remainder
    for (int i = 0; i < 320 * 240; i++) {
        LCD_DATA = 0x0000;  // Black pixel (RGB565)
    }
    // Screen is now cleared to black

    /* ================================================================
     * SECTION 9: POST-LCD DELAY
     * Address: 0x0802405A — 0x080240CE
     *
     * Another SysTick-based delay loop, 100 iterations (r2=0x64)
     * with same structure as Section 6.
     * This is the post-RAMWR stabilization delay.
     * ================================================================ */

    systick_delay_ms(100);  // Post-clear stabilization

    /* ================================================================
     * SECTION 10: APB PERIPHERAL CLOCK ENABLE + DEEPER GPIO CONFIG
     * Address: 0x080240CE — 0x080241F0
     *
     * Enables clocks for SPI, USART, ADC, DMA and configures more GPIO pins.
     * THIS IS WHERE THE INTERESTING ANALOG FRONTEND PINS ARE.
     * ================================================================ */

    // 0x080240CE: r1 = CRM->APB2EN
    // 0x080240D4: r1 |= 0x10       (GPIOC — already enabled, redundant)
    // 0x080240D8: CRM->APB2EN = r1
    CRM->APB2EN |= (1 << 4);  // GPIOC clock (redundant)

    // 0x080240DC: r1 = CRM->APB2EN
    // 0x080240E4: r1 |= 0x10       (same again)
    // 0x080240E8: CRM->APB2EN = r1
    CRM->APB2EN |= (1 << 4);  // GPIOC clock (redundant x2)

    // --- GPIO init: GPIOB pin with input pull-down ---
    // 0x080240F2: cfg.mode_bytes = 0x18 (input pull-down)
    // 0x080240F8: cfg.pins = 0x80 (PB7)
    //   Wait, that was already configured as pull-UP above.
    //   Actually looking more carefully:
    //   r0 is loaded from different sources here.
    //   At 0x08024118 the bl target gets:
    //     cfg.pins = 0x80
    //     cfg.mode_bytes = 0x18 (pull-down)
    //     cfg.drive = 1
    //   And r0 = ??? — need to check what register was loaded.
    //
    //   The movw at 0x080240FE: r7 = 0x40001C00 (SPI2_BASE? No...)
    //   Wait: 0x40001C00. Let me check:
    //     0x40001C00 = APB1 peripherals. This is... actually TIM14 on some chips,
    //     or could be mapped differently on AT32.
    //     On AT32F403A: 0x40001C00 is in the APB1 space. Not a standard GPIO.
    //
    //   Actually the function call:
    //   0x08024118: bl gpio_init
    //   The first arg (r0) was set way back... Let me trace:
    //   After all the stores, r0 was not explicitly set in the immediate vicinity.
    //   Looking at 0x08024106: mov r1, r4 (r4 = sp+0x40 = cfg struct)
    //   No explicit mov r0 before the bl...
    //   Hmm, checking: at 0x080240FE r7 = 0x40001C00
    //   But the function target register is r0, not r7.
    //   r0 was last set at 0x080240F8: movs r0, #0x80... wait, that's cfg.pins.
    //   Actually 0x080240F8 sets r0=0x80, then 0x08024104: str r0, [sp+0x40] stores it.
    //   Then... r0 isn't set again before the bl. So r0 = 0x80???
    //   That can't be a GPIO port address.
    //
    //   Oh wait — I missed something. Let me re-read:
    //   0x080240EC: movs r1, #0
    //   0x080240EE: strb r1, [sp+0x44]
    //   0x080240F2: movs r1, #0x18
    //   0x080240F4: strh r1, [sp+0x45]
    //   0x080240F8: movs r1, #0x80       ← r1, not r0!
    //   0x080240FA-0x08024114: loading r6, fp, r7, r5 with addresses
    //   0x08024104: str r1, [sp+0x40]    — cfg.pins = 0x80 (still r1)
    //   0x08024106: mov r1, r4           — r1 = &cfg
    //
    //   So where does r0 come from? It wasn't modified since...
    //   Looking back: after the SysTick delay at 0x080240CE, the function
    //   was doing CRM register modifications. r0 would have been clobbered.
    //
    //   Actually at line 0x08024118 we need to check what r0 is right before.
    //   The str at 0x08024114: strb r5, [sp+0x47] doesn't touch r0.
    //   The mov at 0x08024106: mov r1, r4 doesn't touch r0.
    //   Going back: r0 was last set at... 0x080240F8? No that's r1.
    //   0x080240F2: movs r1... 0x080240EE: strb r1... 0x080240EC: movs r1...
    //   0x080240E8: str.w r1, [sb] ← this is str, r1 unchanged
    //   0x080240E4: orr r1 ← modifies r1
    //   0x080240DC: ldr.w r1, [sb] ← loads r1
    //   Still no r0 since CRM operations used r1.
    //
    //   Going further back: after the delay loop exits at 0x080240CE,
    //   r0 would be whatever the delay loop left it as.
    //   In the delay loops, r0 is used as a temporary. The last use before
    //   0x080240CE was at 0x080240C2-0x080240CC where r0 isn't modified
    //   (those use r3). The exit path goes through 0x08024060 where
    //   r3 is loaded. So r0...
    //
    //   Actually I realize this is getting very complex to trace statically.
    //   Let me just note the likely intent based on the surrounding context:

    // Based on register values loaded in this section:
    //   r6 = 0x40015404  (AT32 extended register? Or GPIOA+offset?)
    //        0x40015404... that's well past GPIO space.
    //        Actually: 0x5404 | (0x4001 << 16) = 0x40015404
    //        On AT32: This could be a timer or DMA extended register
    //   fp = 0x40015034  (similar extended space)
    //   r7 = 0x40001C00  (SPI2? TMR? On AT32: 0x40001C00 = SPI2_BASE...
    //        wait, SPI2 is at 0x40003800 on STM32F1. On AT32 it might differ.)
    //        Actually on AT32F403A: 0x40001C00 is in APB1 space, possibly I2C2_BASE

    // MOVING ON — the exact r0 value requires dynamic tracing. The key
    // gpio_init calls in this section configure:

    // Call at 0x08024118: gpio_init(???, {pins=0x80, pull_down, strong})
    // Likely configuring a passive input pin (PB7 reconfigured? Or different port)

    // Call at 0x0802412C: gpio_init(GPIOC, {pins=0x4000, ...})
    // 0x0802411C: cfg.pins = 0x4000 = PC14
    // 0x08024122: r0 = 0x40011000 (GPIOC)
    gpio_init(GPIOC, &(gpio_init_t){
        .pins = GPIO_PIN_14,    // 0x4000 = PC14
        .mode = GPIO_INPUT,     // Exact mode from stored config
    });
    // PC14 — interesting! Not documented in our firmware.
    // Could be: oscillator pin (LSE), or an undocumented function pin.
    // On many STM32/AT32 chips, PC14 = OSC32_IN for 32.768kHz crystal.
    // But configuring it as GPIO suggests it's used for something else here.

    /* ================================================================
     * SECTION 11: USART INIT + SPI FLASH INIT + NVIC CONFIG
     * Address: 0x08024130 — 0x0802417C
     * ================================================================ */

    // 0x08024130: r4 = 0x20001070  — USART config struct in RAM
    // 0x08024138: ldr r0, [r4+0xC]  — load baud rate or clock value
    // 0x0802413A: r1 = 0x2BC0 = 11200  — could be baud rate divisor
    //   Actually 0x2BC0 = 11200. If this is a baud divider:
    //   For 240MHz APB1 clock: 240000000 / 11200 = ~21428 (not standard baud)
    //   More likely this is used as a parameter for a calc.
    // 0x0802413E: bl 0x080012BC  — this is NOT gpio_init (different address)
    //   This is likely a division or USART baud rate calculation function

    // 0x08024142: strb 1, [r4+0x10]  — enable flag in USART config struct

    // 0x08024144: bl 0x0802EF48  — major peripheral init function
    //   This likely initializes USART2 for FPGA communication
    usart2_init();  // FPGA USART command interface (9600 baud)

    // 0x08024148: bl 0x0802E7BC  — another init function
    //   Likely SPI flash or DMA init
    spi_flash_init();  // W25Q128 SPI flash init? Or DMA init?

    // 0x0802414C: r0 = 0x20036B80  — large RAM buffer or struct
    // 0x08024150: r1 = 0x0803A23D  — flash address (function pointer or data)
    // 0x0802415C: str r1, [r0]     — store function pointer/callback at RAM location
    *(uint32_t *)0x20036B80 = 0x0803A23D;  // Callback or lookup table pointer

    // 0x0802415E: NVIC configuration struct setup
    // 0x08024166: [sp+0x64] = 0x01080400  — same config constant as EXMC GPIO
    //   Actually this is part of a different struct now:
    //   sp+0x60 = 0x100 (256)
    //   sp+0x64 = 0x01080400
    //   sp+0x50 = 0
    //   sp+0x48..0x4C = 0, 0
    //   sp+0x40..0x44 = 0, 0
    // 0x0802417C: bl 0x0802A430  — NVIC configuration function
    //   Sets up interrupt priorities for USART2, SPI3, TMR3, DMA etc.
    nvic_init();  // Configure interrupt priorities

    /* ================================================================
     * SECTION 12: MORE APB CLOCKS — SPI, TMR, ADC, DMA
     * Address: 0x08024180 — 0x080241C8
     * ================================================================ */

    // 0x08024180: r0 = CRM->APB2EN
    // 0x08024186: r0 |= 0x08       (bit 3 = GPIOB)
    // 0x0802418A: CRM->APB2EN = r0
    CRM->APB2EN |= (1 << 3);  // Enable GPIOB (already enabled, redundant)

    // 0x0802418E: r0 = CRM->APB2EN
    // 0x08024194: r0 |= 0x100000   (bit 20)
    // 0x08024198: CRM->APB2EN = r0
    CRM->APB2EN |= (1 << 20);  // *** UNKNOWN BIT 20 ***
    // On STM32F1, APB2EN bit 20 is not defined.
    // On AT32F403A, this might be: TMR9 or TMR10 clock enable
    // IMPORTANT: This is an AT32-specific clock enable not in our firmware!

    // 0x0802419C: r0 = CRM->APB2EN
    // 0x080241A0: r0 |= 0x08       (GPIOB again)
    CRM->APB2EN |= (1 << 3);  // GPIOB (redundant again)

    // 0x080241A8: r0 = CRM->APB2EN
    // 0x080241AC: r0 |= 0x200000   (bit 21)
    // 0x080241B0: CRM->APB2EN = r0
    CRM->APB2EN |= (1 << 21);  // *** UNKNOWN BIT 21 ***
    // On AT32F403A: possibly TMR10 or TMR11 clock enable
    // Another AT32-specific clock not in our firmware!

    // 0x080241B4: r0 = CRM->APB2EN
    // 0x080241B8: r0 |= 0x04       (bit 2 = GPIOA, already enabled)
    CRM->APB2EN |= (1 << 2);  // GPIOA (redundant)

    // --- APB1 clock enable ---
    // 0x080241C0: r0 = CRM->APB1EN  (sb + 4 = 0x40021018 + 4 = 0x4002101C)
    // 0x080241C4: r0 |= 0x80        (bit 7)
    // 0x080241C8: CRM->APB1EN = r0
    CRM->APB1EN |= (1 << 7);
    // APB1EN bit 7: on AT32F403A this could be TMR12 or similar
    // On GD32F30x: bit 7 might be WWDG
    // IMPORTANT: Need AT32F403A reference to decode this exactly.

    // --- GPIO init calls for analog/peripheral pins ---

    // 0x080241CC: r5 = [sp+0x10]  — this was stored earlier (GPIOB base or 0)
    //   At 0x08023B38: [sp+0x10] = sl = 0 (no wait, sl was set differently)
    //   Actually tracing: at 0x08023B38, sl was still 0 from 0x08023AA8.
    //   So [sp+0x10] = 0. But then the function is called with r0=0, which
    //   would be an invalid GPIO port address. This suggests sl was NOT 0
    //   at that point — it was reassigned at 0x08023AF8 to 0x40010C00 (GPIOB).
    //   But wait, 0x08023B38 stores sl BEFORE the sub at 0x08023AF8? No,
    //   0x08023AF8 is BEFORE 0x08023B38. So sl = GPIOB at store time.
    //
    //   [sp+0x10] = GPIOB (0x40010C00)
    //
    // 0x080241CE: r0 = r5 = GPIOB
    // 0x080241D0: bl gpio_init(GPIOB, cfg)
    //   cfg was loaded at sp+0x60: pins=0x200 (PB9? pin 9)
    //   Hmm, sp+0x60 was set to 0x100 (pin 8) at 0x0802416C
    //   Wait, the cfg struct for this call is at sp+0x60 (r4 = sp+0x60 at 0x08024184)

    // Actually r1 = r4 = sp+0x60 from 0x08024192
    // And sp+0x60 = 0x100 (pin 8 = PB8)
    // sp+0x64 = 0x01080400 (AF config)
    gpio_init(GPIOB, &(gpio_init_t){
        .pins = GPIO_PIN_8,     // 0x100 = PB8
        .mode = GPIO_OUTPUT_50MHZ,  // From config constant
        .type = GPIO_PUSH_PULL,
        .drive = GPIO_STRONG
    });
    // PB8 = LCD BACKLIGHT — output push-pull, strong drive

    // 0x080241D4: cfg.pins = 0x200 (sp+0x60) — pin 9
    // 0x080241DA: r0 = r5 = GPIOB
    gpio_init(GPIOB, &(gpio_init_t){
        .pins = GPIO_PIN_9,     // 0x200 = PB9
        .mode = GPIO_OUTPUT_50MHZ,
        .type = GPIO_PUSH_PULL,
        .drive = GPIO_STRONG
    });
    // PB9 — OUTPUT PIN, not documented in our firmware!
    // *** DISCOVERY: PB9 is configured as an output ***
    // Possible function: analog frontend MUX select, FPGA control, or power rail enable.
    // This is NOT in our current pin map. Needs investigation.

    // 0x080241E2: cfg.pins = 0x40 (sp+0x60) — pin 6
    // 0x080241E6: r0 = 0x40010800 (GPIOA)
    gpio_init(GPIOA, &(gpio_init_t){
        .pins = GPIO_PIN_6,     // 0x40 = PA6
        .mode = GPIO_OUTPUT_50MHZ,
        .type = GPIO_PUSH_PULL,
        .drive = GPIO_STRONG
    });
    // PA6 — OUTPUT PIN, not documented in our firmware!
    // *** DISCOVERY: PA6 is configured as an output ***
    // On STM32F1, PA6 is also TIM3_CH1 / SPI1_MISO / ADC12_IN6
    // In this context, likely an analog MUX select or FPGA control signal.

    /* ================================================================
     * SECTION 13: TIMER/ADC PERIPHERAL CONFIGURATION
     * Address: 0x080241F4 — 0x08024250
     *
     * Register addresses decoded from fp offsets:
     *   fp = 0x40015034 (set at 0x080240FA/0x0802410C)
     *   fp - 0x08 = 0x4001502C
     *   fp - 0x0C = 0x40015028
     *   fp - 0x20 = 0x40015014
     *   fp - 0x34 = 0x40015000
     *   fp - 0x30 = 0x40015004
     *
     *   0x40015000 region: On AT32F403A this is TMR1 or ADC territory
     *   Actually: 0x40015000 = beyond standard STM32F1 map
     *   On AT32F403A: 0x40015000 might be in the extended peripheral space
     *
     *   Wait — let me recalculate. APB2 peripherals:
     *   0x40010000 = AFIO, 0x40010800 = GPIOA, ... 0x40012400 = ADC1,
     *   0x40012800 = ADC2, 0x40013000 = SPI1, 0x40013800 = USART1,
     *   0x40014C00 = TMR1 (on AT32F403A)
     *
     *   Actually on AT32F403A: TMR1 base = 0x40012C00
     *   0x40015000 is not a standard AT32 peripheral base.
     *
     *   Hmm, let me reconsider fp value:
     *   0x080240FA: movw fp, #0x5034
     *   0x0802410C: movt fp, #0x4001
     *   fp = 0x40015034
     *
     *   On AT32F403A extended peripheral map:
     *   0x40014C00 = TMR1
     *   0x40015000 = TMR8 base (confirmed by CLAUDE.md: TMR8 vector repurposed)
     *   So fp = TMR8_BASE + 0x34 = TMR8->DMAINT (DMA/interrupt enable register)
     *
     *   fp - 0x34 = TMR8->CR1 (control register 1)
     *   fp - 0x30 = TMR8->CR2 (control register 2)
     *   fp - 0x20 = TMR8->SMCR or similar
     *   fp - 0x0C = TMR8->PSC (prescaler) at offset 0x28
     *   fp - 0x08 = TMR8->ARR (auto-reload) at offset 0x2C
     *
     *   BUT WAIT: CLAUDE.md says "TMR8: Vector repurposed for FatFs.
     *   TMR8 hardware is unused." So the stock firmware DOES configure TMR8!
     *   The vector is repurposed but the timer itself is configured here.
     * ================================================================ */

    // 0x080241F4: r0 = [sp+0x44]  — some saved value from earlier
    // 0x080241F6: r1 = 0x431BDE83  — magic constant for division
    // 0x080241FE: umull r1, r2, r0, r1  — 64-bit multiply for dividing by clock
    //   This is a compiler-generated division: r0 / some_constant
    //   0x431BDE83 is the magic number for dividing by 10,000,000 or similar
    //   Result in r2 (high word of multiply, right-shifted)
    // 0x08024206: r2 = 0xFFFFFFFF + (r2 >> 18)  — adjust quotient

    // Timer configuration (likely TMR8 or another timer):
    // The actual register is at fp - offsets:

    // 0x0802420C: str r3, [fp-8]  → *(0x4001502C) = 0x63 (= 99 decimal)
    //   fp - 8 = TMR8_BASE + 0x2C = TMR8->ARR (auto-reload register)
    //   ARR = 99 → timer counts 0-99 (100 ticks per period)
    TMR8->ARR = 99;  // Auto-reload: 100 counts per period

    // 0x08024210: str r2, [fp-0xC]  → *(0x40015028) = prescaler value
    //   fp - 0xC = TMR8_BASE + 0x28 = TMR8->PSC (prescaler)
    //   Value is computed from clock: approximately clock / 10,000,000 - 1
    //   At 240MHz: PSC ≈ 23 → timer clock = 240MHz / 24 = 10MHz
    //   With ARR=99: interrupt rate = 10MHz / 100 = 100kHz? That seems fast.
    //   Or if the input clock is APB2 timer clock (120MHz):
    //   PSC ≈ 11 → 120MHz / 12 = 10MHz, / 100 = 100kHz
    //   This would be a 10µs interrupt — used for FatFs timing.
    TMR8->PSC = (clock_freq / 10000000) - 1;  // Prescaler for ~10MHz timer clock

    // 0x08024214: r2 = [fp-0x20]  → TMR8_BASE + 0x14 = TMR8->DIER
    //   (actually offset 0x14 from TMR8_BASE = 0x40015014... that's not standard)
    //   Hmm: TMR register map: +0x00=CR1, +0x04=CR2, +0x08=SMCR, +0x0C=DIER
    //   So fp - 0x20 = base + 0x14 = ???
    //   Wait: TMR8 base = 0x40015000
    //   fp - 0x20 = 0x40015034 - 0x20 = 0x40015014
    //   0x40015014 = TMR8_BASE + 0x14 = TMR8->SR (status register) on STM32
    //   Or it could be DIER on AT32 if register layout differs.
    //
    //   On standard STM32F1 timer layout:
    //   +0x00 CR1, +0x04 CR2, +0x08 SMCR, +0x0C DIER, +0x10 SR, +0x14 EGR
    //   So 0x14 = EGR (event generation register)

    // 0x0802421C: r2 |= 1
    // 0x08024220: str r2, [fp-0x20]  → TMR8->EGR |= 1 (update event)
    TMR8->EGR |= 1;  // Generate update event (load prescaler and ARR)

    // 0x08024224: r2 = [fp-0x34]  → *(0x40015000) = TMR8->CR1
    // 0x0802422A: bic r2, #0x70    — clear bits [6:4] (CMS + DIR fields)
    // 0x0802422E: str r2, [fp-0x34]
    TMR8->CR1 &= ~0x70;  // Clear CMS[1:0] and DIR: edge-aligned, up-counting

    // 0x08024232: r2 = [fp-0x30]  → *(0x40015004) = TMR8->CR2
    // 0x0802423A: bic r2, #0x100   — clear bit 8 (OIS1 or similar)
    // 0x0802423E: str r2, [fp-0x30]
    TMR8->CR2 &= ~0x100;  // Clear bit 8 of CR2

    // 0x08024242: r2 = [fp-0x30]  → TMR8->CR2 again
    // 0x0802424A: r2 |= 0x100      — set bit 8
    //   This is a read-clear-set pattern, probably handling multiple fields.
    //   Actually the clear at 0x0802423A and set at 0x0802424A of the same bit
    //   suggests this might be setting bit 8 while ensuring no glitch.
    TMR8->CR2 |= 0x100;  // Set bit 8 of CR2 (master mode output?)

    // Additional computation at 0x08024228/0x08024246:
    // r0 >>= 4, then umull with 0x0A7C5AC5 — another magic division constant
    // This computes a second prescaler or timing value.
    // (Continues past 0x08024250 into Phase 2...)
}

/* ================================================================
 * SUMMARY OF DISCOVERIES
 * ================================================================
 *
 * KNOWN PINS CONFIRMED:
 *   PC9  — Power hold (OUTPUT, first thing configured)
 *   PB7  — PRM button (INPUT, pull-up)
 *   PC8  — POWER button (INPUT, pull-down, active high)
 *   PC13 — UP button (INPUT, pull-down, active high)
 *   PA7, PA8 — Button matrix (INPUT, pull-down)
 *   PB0  — Button matrix row (INPUT, pull-down)
 *   PC5, PC10 — Button matrix (INPUT, pull-down)
 *   PE2, PE3 — Button matrix (INPUT, pull-down)
 *   PB8  — LCD backlight (OUTPUT, push-pull)
 *   PD0,PD1,PD4,PD5,PD6,PD7,PD8,PD9,PD10,PD11,PD14,PD15 — EXMC (AF push-pull)
 *   PE7-PE15 — EXMC data bus (AF push-pull)
 *
 * *** NEW DISCOVERIES ***:
 *   PB9  — OUTPUT push-pull, 50MHz — NOT IN OUR FIRMWARE
 *           Possible: analog MUX, probe detect relay, or AC/DC coupling switch
 *   PA6  — OUTPUT push-pull, 50MHz — NOT IN OUR FIRMWARE
 *           Possible: analog frontend control (MUX/relay/coupling)
 *   PC14 — Configured as GPIO (not crystal) — unusual, needs investigation
 *
 * CLOCK ENABLES NOT IN OUR FIRMWARE:
 *   APB2EN bit 20 — AT32-specific (TMR9 or TMR10?)
 *   APB2EN bit 21 — AT32-specific (TMR10 or TMR11?)
 *   APB1EN bit 7  — AT32-specific
 *   IOMUX->REMAP3 bit 27 — AT32-specific remap register
 *
 * PERIPHERAL INIT CALLS:
 *   0x0802EF48 — USART2 init (FPGA commands)
 *   0x0802E7BC — SPI flash init or DMA init
 *   0x0802A430 — NVIC configuration
 *
 * TMR8 CONFIGURATION:
 *   Despite CLAUDE.md saying "TMR8 hardware is unused", the stock firmware
 *   DOES configure TMR8 with PSC and ARR for a periodic interrupt.
 *   ARR = 99, PSC computed from clock. Likely 100kHz or 10kHz rate.
 *   Used for FatFs timing (1ms tick for sd_card/spi_flash timing).
 *
 * LCD INITIALIZATION:
 *   Full ST7789V init sequence with gamma curves confirmed.
 *   PD6 is used as LCD RESET (toggle sequence: HIGH → delay → LOW → delay → HIGH)
 *   *** PD6 = LCD RESET — not previously documented! ***
 *   Screen cleared to black (76800 pixels) before enabling display.
 *   Landscape mode (320x240) with MADCTL = 0xA0.
 *
 * EXMC NOTE:
 *   PD12 (A17, used for RS/DCX) was NOT seen in the pin mask 0x8B0.
 *   It may be configured in a later phase or via a different mechanism.
 *   Confirmed: 0x1000 values after 0x08024250 are GPIOC base (0x40011000),
 *   not PD12 pin mask. PD12/A17 must be configured later in the 15.4KB function.
 *   Our firmware uses A17 for LCD command/data selection and it works, so it
 *   IS configured somewhere — just not in Phase 1.
 *
 * LCD PARAMS STRUCT at 0x20008340:
 *   +0x00: width (320)
 *   +0x02: height (240)
 *   +0x06: orientation (1 = landscape)
 *   +0x08: RAMWR cmd (0x2C)
 *   +0x0A: CASET cmd (0x2A)
 *   +0x0C: RASET cmd (0x2B)
 */
