export const meta = {
  name: 'resolve-squeeze-2026-06-13',
  description: 'Static-equivalence resolve pass on the 12 non-blocked unresolved-meaningful D3 fns; promote genuinely-faithful ones to R2/V1 with cited register matches, adversarial-verify every promotion',
  phases: [ { title: 'Resolve' }, { title: 'Verify' } ],
}

const TARGETS = [{"addr": "0801e1e4", "name": "scope_mode_trigger", "klass": "protocol", "R": "R1", "V": "V0", "size": 2232, "note": "DIVERGENT 2026-06-13: Stock 0801e1e4 (2.2KB, 1 caller=scope_main_fsm) is MISNAMED 'scope_mode_trigger' \u2014 it is the FFT/s"}, {"addr": "08036084", "name": "spi_flash_fs_format", "klass": "logic", "R": "R2", "V": "V0", "size": 1360, "note": "REIMPL_FAITHFUL 2026-06-13(wf): Disposition (MISLABELED; role=screenshot save BMP) and rScore R2 are CONFIRMED by readin"}, {"addr": "0802d1e8", "name": "spi_flash_write_sectors", "klass": "hardware", "R": "R0", "V": "NA", "size": 842, "note": "DECODE_ONLY 2026-06-13: MISNAMED \u2014 this is FatFs `f_write` (842 bytes), not a raw SPI-flash sector writer. Register-leve"}, {"addr": "080028e0", "name": "meter_data_process", "klass": "protocol", "R": "R1", "V": "V0", "size": 768, "note": "DIVERGENT 2026-06-13: Stock FUN_080028e0 (fpga_state_update): when DAT_20001035 set, runs softfloat (FUN_0803ed70/e5da/c"}, {"addr": "0800b908", "name": "FUN_0800b908", "klass": "protocol", "R": "R2", "V": "V1", "size": 512, "note": "VERIFY_FAITHFUL 2026-06-13: FUN_0800b908 is the boot-time FPGA MODE-INIT DISPATCHER (fpga_comms_deep_dive.c:31-145): a s"}, {"addr": "08037800", "name": "fpga_spi3_transfer", "klass": "hardware", "R": "R2", "V": "V0", "size": 472, "note": "UNRESOLVED 2026-06-13 (harvest): Per the prompt's explicit flag and verified here: corrupt disasm context blocks faithfu"}, {"addr": "0803bee0", "name": "dma1_configure", "klass": "hardware", "R": "R0", "V": "V0", "size": 358, "note": "DIVERGENT 2026-06-13(wf): Role corrected: not generic DMA config \u2014 it's the LCD viewport framebuffer DMA blit trigger. S"}, {"addr": "08036848", "name": "spi_peripheral_init", "klass": "hardware", "R": "R1", "V": "V0", "size": 232, "note": "DIVERGENT 2026-06-13: Stock FUN_08036848 (full_decompile.c:28411-28463) builds SPI_CR1/CR2 bit-by-bit from an 8-byte con"}, {"addr": "0802f2ac", "name": "spi2_page_write_loop", "klass": "hardware", "R": "R1", "V": "V0", "size": 152, "note": "DIVERGENT 2026-06-13(wf): Role CORRECT (256-byte page-aligned program loop) but bus label WRONG: not SPI2. Splits write "}, {"addr": "0802eee8", "name": "spi2_read_jedec_id", "klass": "hardware", "R": "R1", "V": "NA", "size": 96, "note": "DIVERGENT 2026-06-13: Stock = SPI2 software-CS ID read, 96 bytes. Register exact: `_DAT_40010c14=0x1000` (GPIOB_BRR, PB1"}, {"addr": "08019e18", "name": "scope_set_ch_offset", "klass": "logic", "R": "R1", "V": "V0", "size": 82, "note": "DECODE_ONLY 2026-06-13(wf): REFUTED at the V1/REIMPL_FAITHFUL level; the MISLABELED correction itself is correct and sta"}, {"addr": "0802d1c4", "name": "spi_flash_status_check", "klass": "hardware", "R": "R0", "V": "NA", "size": 36, "note": "DECODE_ONLY 2026-06-13: MISNAMED in the batch \u2014 this is NOT an SPI-flash status check. Full register decode: 36-byte Fat"}]

const RESOLVE_SCHEMA = {
  type: 'object', additionalProperties: false,
  required: ['addr','disposition','rFinal','vFinal','promote','ourSource','evidence','reimplNeeded'],
  properties: {
    addr: { type: 'string' },
    disposition: { type: 'string', enum: ['RESOLVE_FAITHFUL','DIVERGENT','ABSENT','FPGA_BLOCKED','NA_OURS','STILL_PARTIAL'] },
    rFinal: { type: 'string', enum: ['R0','R1','R2','RNA'] },
    vFinal: { type: 'string', enum: ['V0','V1','V2','VNA'] },
    promote: { type: 'boolean' },
    ourSource: { type: 'string' },
    evidence: { type: 'string' },
    reimplNeeded: { type: 'string' }
  }
}

const VERIFY_SCHEMA = {
  type: 'object', additionalProperties: false,
  required: ['addr','confirmed','rFinal','vFinal','reason'],
  properties: {
    addr: { type: 'string' },
    confirmed: { type: 'boolean' },
    rFinal: { type: 'string', enum: ['R0','R1','R2','RNA'] },
    vFinal: { type: 'string', enum: ['V0','V1','V2','VNA'] },
    reason: { type: 'string' }
  }
}

function rp(f) {
  return [
    'FNIRSI 2C53T stock-firmware RESOLVE pass (Artery AT32F403A). This function is already fully decoded (D3) but UNRESOLVED.',
    'TARGET: ' + f.name + ' @0x' + f.addr + ', ' + f.size + 'B, current R=' + f.R + '/V=' + f.V + '.',
    'Prior note: ' + f.note,
    '',
    'GOAL: determine its TRUE final resolution honestly. RESOLVED requires R2 AND V>=1 (faithful reimpl + static equivalence), OR RNA (justified skip).',
    'Steps:',
    ' 1. Read the stock body (grep reverse_engineering/decompiled_2C53T_v2.c / decompiled_2C53T.c / analysis_v120/full_decompile.c for the name or FUN_0x' + f.addr + '). Note every hardware register (absolute addr) + exact protocol/formula.',
    ' 2. Search firmware/src/ for our equivalent (flash_fs.c, drivers/*.c, lcd.c, dac_output.c, fpga.c, meter_data.c).',
    ' 3. Decide:',
    '    - RESOLVE_FAITHFUL (promote=true): we have a register/byte-faithful equivalent NOW -> R2/V1. You MUST cite exact matching register addresses / protocol bytes / formula constants in ourSource. Do NOT promote on vibes.',
    '    - DIVERGENT: we implement it but differ in a way that matters -> keep R1, describe the diff in reimplNeeded.',
    '    - ABSENT: we do not implement it at all -> R0, reimplNeeded = what to build.',
    '    - FPGA_BLOCKED: equivalence needs the FPGA/scope acquisition chain -> V0, cannot statically verify.',
    '    - NA_OURS: library/UI we legitimately replace -> RNA/VNA.',
    '    - STILL_PARTIAL: decode has a gap -> R1/V0.',
    ' Default to NOT promoting when uncertain. evidence <= 350 chars with the concrete register/byte citation. addr=' + f.addr + '.'
  ].join('\n')
}

function vp(f, d) {
  return [
    'Adversarial verification of a RESOLVE promotion. Stock ' + f.name + ' (0x' + f.addr + ') was scored:',
    JSON.stringify({disposition:d.disposition, rFinal:d.rFinal, vFinal:d.vFinal, ourSource:d.ourSource, evidence:d.evidence}),
    '',
    'Try to REFUTE the R2/V1 (or RNA) claim. Open ourSource, read the cited code, read the stock body yourself.',
    ' - V1 requires EVERY hardware register address, protocol byte, and formula constant to actually match stock. One real divergence => downgrade (DIVERGENT/R1/V0).',
    ' - If RNA: confirm we truly have a working equivalent subsystem (not stubbed/absent). If stubbed => ABSENT/R0.',
    'Be skeptical; default to refuting if the evidence does not concretely hold. addr=' + f.addr + '.'
  ].join('\n')
}

phase('Resolve')
const out = await pipeline(
  TARGETS,
  (f) => agent(rp(f), { label: 'resolve:' + f.name, phase: 'Resolve', schema: RESOLVE_SCHEMA }),
  (d, f) => {
    if (!d) return null
    if (d.promote || d.rFinal === 'R2' || d.rFinal === 'RNA') {
      return agent(vp(f, d), { label: 'verify:' + f.name, phase: 'Verify', schema: VERIFY_SCHEMA })
        .then(v => ({ f, d, v }))
    }
    return { f, d, v: null }
  }
)
return { results: out.filter(Boolean) }
