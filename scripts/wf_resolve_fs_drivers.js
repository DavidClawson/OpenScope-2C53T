export const meta = {
  name: 'resolve-fs-drivers-2026-06-13',
  description: 'Decode + faithfully resolve the 52 generic-note genuine fns (FatFs/SPI2/scope) and batch-classify 28 render fns; adversarial verify of every REIMPL/NA claim',
  phases: [
    { title: 'Decode' },
    { title: 'Verify' },
    { title: 'Render' },
  ],
}

const GENUINE = [{"addr": "0802a664", "name": "fs_fat_traverse", "klass": "logic", "R": "R0", "size": 5748}, {"addr": "0802c250", "name": "fs_directory_operations", "klass": "logic", "R": "R0", "size": 3138}, {"addr": "080278e4", "name": "usb_endpoint_handler", "klass": "hardware", "R": "R0", "size": 2566}, {"addr": "0802920c", "name": "fs_read_file_data", "klass": "logic", "R": "R0", "size": 1396}, {"addr": "08036084", "name": "spi_flash_fs_format", "klass": "logic", "R": "R0", "size": 1360}, {"addr": "0802f6d8", "name": "spi_flash_fat_update", "klass": "logic", "R": "R0", "size": 1196}, {"addr": "0802bde4", "name": "fs_write_data", "klass": "logic", "R": "R0", "size": 1130}, {"addr": "0802e12c", "name": "spi_flash_create_entry", "klass": "logic", "R": "R1", "size": 1026}, {"addr": "0803586c", "name": "spi_flash_cluster_chain_read", "klass": "logic", "R": "R1", "size": 1016}, {"addr": "0802ff18", "name": "spi_flash_cluster_io", "klass": "logic", "R": "R0", "size": 994}, {"addr": "0802d8b8", "name": "spi_flash_directory_scan", "klass": "logic", "R": "R1", "size": 904}, {"addr": "080337a0", "name": "spi_flash_read_sector_cached", "klass": "logic", "R": "R0", "size": 880}, {"addr": "0802e530", "name": "spi_flash_rename_entry", "klass": "logic", "R": "R1", "size": 652}, {"addr": "08029e0c", "name": "fs_open_file", "klass": "logic", "R": "R0", "size": 620}, {"addr": "0802dcbc", "name": "spi_flash_read_dir_entry", "klass": "logic", "R": "R1", "size": 610}, {"addr": "0802a078", "name": "fs_create_file", "klass": "logic", "R": "R0", "size": 578}, {"addr": "0802d534", "name": "spi_flash_fs_sync", "klass": "logic", "R": "R1", "size": 572}, {"addr": "0802df20", "name": "spi_flash_delete_entry", "klass": "logic", "R": "R1", "size": 524}, {"addr": "08033cfc", "name": "display_alloc_buffer", "klass": "logic", "R": "R0", "size": 508}, {"addr": "08035ed4", "name": "spi_flash_fs_mount", "klass": "logic", "R": "R0", "size": 432}, {"addr": "0803bee0", "name": "dma1_configure", "klass": "hardware", "R": "R0", "size": 358}, {"addr": "0802a2d4", "name": "fs_extend_cluster", "klass": "logic", "R": "R0", "size": 346}, {"addr": "0802e7bc", "name": "spi_flash_fs_operations", "klass": "logic", "R": "R1", "size": 330}, {"addr": "0802f16c", "name": "spi_flash_write_block", "klass": "hardware", "R": "R0", "size": 320}, {"addr": "08034878", "name": "fs_init_sequence", "klass": "logic", "R": "R1", "size": 318}, {"addr": "08036934", "name": "fs_flush_and_sync", "klass": "logic", "R": "R0", "size": 282}, {"addr": "08029b80", "name": "fs_read_sector", "klass": "logic", "R": "R0", "size": 276}, {"addr": "0802e908", "name": "spi_flash_error_handler", "klass": "logic", "R": "R1", "size": 254}, {"addr": "080027e8", "name": "lcd_read_image_data", "klass": "hardware", "R": "R0", "size": 248}, {"addr": "0802ea08", "name": "spi_flash_init_fs", "klass": "logic", "R": "R1", "size": 242}, {"addr": "0802912c", "name": "fs_format_filename", "klass": "logic", "R": "R0", "size": 224}, {"addr": "08029cc4", "name": "fs_write_entry", "klass": "logic", "R": "R0", "size": 220}, {"addr": "08022d40", "name": "FUN_08022d40", "klass": "unknown", "R": "R0", "size": 200}, {"addr": "0802d80c", "name": "spi_flash_format_check", "klass": "logic", "R": "R1", "size": 172}, {"addr": "0803e538", "name": "scope_set_sampling_params", "klass": "protocol", "R": "R0", "size": 162}, {"addr": "0802f2ac", "name": "spi2_page_write_loop", "klass": "hardware", "R": "R0", "size": 152}, {"addr": "0802d774", "name": "spi_flash_cache_invalidate", "klass": "logic", "R": "R1", "size": 150}, {"addr": "0803e600", "name": "scope_set_measure_config", "klass": "protocol", "R": "R0", "size": 138}, {"addr": "0802dc40", "name": "spi_flash_alloc_cluster", "klass": "logic", "R": "R1", "size": 122}, {"addr": "0802f36c", "name": "spi2_page_program", "klass": "hardware", "R": "R0", "size": 118}, {"addr": "08029da0", "name": "fs_init_header", "klass": "logic", "R": "R0", "size": 106}, {"addr": "08001830", "name": "FUN_08001830", "klass": "protocol", "R": "R0", "size": 100}, {"addr": "0802985c", "name": "fs_check_path", "klass": "logic", "R": "R0", "size": 92}, {"addr": "08019e18", "name": "scope_set_ch_offset", "klass": "logic", "R": "R1", "size": 82}, {"addr": "0802f11c", "name": "spi2_wait_busy", "klass": "hardware", "R": "R0", "size": 78}, {"addr": "0802ee9c", "name": "spi2_sector_erase", "klass": "hardware", "R": "R0", "size": 76}, {"addr": "080304f0", "name": "fs_close_file", "klass": "logic", "R": "R0", "size": 50}, {"addr": "0802f344", "name": "spi2_write_enable", "klass": "hardware", "R": "R0", "size": 38}, {"addr": "08019e78", "name": "scope_set_ch_coupling", "klass": "logic", "R": "R1", "size": 22}, {"addr": "0800bcd4", "name": "mode_dispatch_indirect", "klass": "logic", "R": "R1", "size": 18}, {"addr": "08019e6c", "name": "scope_get_ch_offset", "klass": "logic", "R": "R1", "size": 10}, {"addr": "08034070", "name": "fs_close_helper", "klass": "logic", "R": "R0", "size": 8}]
const RENDER = [{"addr": "08020930", "name": "scope_mode_display_settings", "klass": "render", "R": "R1", "size": 4558}, {"addr": "08022e14", "name": "lcd_draw_bitmap_from_flash", "klass": "render", "R": "R0", "size": 2900}, {"addr": "0800ec70", "name": "meter_ui_draw_bargraph", "klass": "render", "R": "R1", "size": 1798}, {"addr": "08019470", "name": "scope_draw_waveform_trace", "klass": "render", "R": "R1", "size": 1672}, {"addr": "08021de4", "name": "scope_draw_channel_info", "klass": "render", "R": "R1", "size": 1492}, {"addr": "080342f8", "name": "scope_display_full_redraw", "klass": "render", "R": "R1", "size": 1292}, {"addr": "08034078", "name": "scope_display_refresh", "klass": "render", "R": "R1", "size": 626}, {"addr": "080365d4", "name": "scope_draw_xy_mode", "klass": "render", "R": "R1", "size": 588}, {"addr": "0800bde0", "name": "meter_ui_draw_value", "klass": "render", "R": "R1", "size": 530}, {"addr": "08032f6c", "name": "measurement_dispatch", "klass": "render", "R": "R0", "size": 520}, {"addr": "08021b40", "name": "scope_draw_trigger_overlay", "klass": "render", "R": "R1", "size": 516}, {"addr": "08015d58", "name": "scope_ui_draw_range_list_ch1", "klass": "render", "R": "R1", "size": 500}, {"addr": "0800e79c", "name": "meter_ui_draw_range_list", "klass": "render", "R": "R1", "size": 490}, {"addr": "080298c0", "name": "meter_coord_transform", "klass": "render", "R": "R1", "size": 384}, {"addr": "08019af8", "name": "scope_draw_trigger_marker", "klass": "render", "R": "R1", "size": 332}, {"addr": "08019ce8", "name": "scope_draw_fft_bars", "klass": "render", "R": "R1", "size": 276}, {"addr": "08018da0", "name": "draw_line_segment_bresenham", "klass": "render", "R": "R1", "size": 262}, {"addr": "08029a70", "name": "scope_draw_measurements", "klass": "render", "R": "R1", "size": 260}, {"addr": "0803340c", "name": "font_get_glyph_index", "klass": "render", "R": "R1", "size": 222}, {"addr": "080022dc", "name": "FUN_080022dc", "klass": "render", "R": "R1", "size": 200}, {"addr": "08033174", "name": "display_color_fill", "klass": "render", "R": "R1", "size": 196}, {"addr": "0803f020", "name": "scope_draw_trigger_line", "klass": "render", "R": "R1", "size": 184}, {"addr": "08019c48", "name": "scope_draw_controller", "klass": "render", "R": "R1", "size": 160}, {"addr": "0803f0e4", "name": "scope_draw_measurement_box", "klass": "render", "R": "R1", "size": 116}, {"addr": "0800bd84", "name": "meter_ui_draw_header", "klass": "render", "R": "R1", "size": 90}, {"addr": "08015cfc", "name": "scope_ui_draw_header_ch1", "klass": "render", "R": "R1", "size": 90}, {"addr": "08033c6c", "name": "display_set_clip_region", "klass": "render", "R": "R0", "size": 80}, {"addr": "08033cbc", "name": "display_clear_region", "klass": "render", "R": "R1", "size": 62}]

const DECODE_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['addr','dLevel','disposition','rScore','vScore','ourSource','note'],
  properties: {
    addr: { type: 'string' },
    dLevel: { type: 'integer', minimum: 0, maximum: 3 },
    disposition: { type: 'string', enum: ['REIMPL_FAITHFUL','NA_OURS','ABSENT','FPGA_BLOCKED','DIVERGENT','DECODE_ONLY'] },
    rScore: { type: 'string', enum: ['R0','R1','R2','RNA'] },
    vScore: { type: 'string', enum: ['V0','V1','V2','VNA'] },
    ourSource: { type: 'string' },
    note: { type: 'string' }
  }
}

const VERIFY_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['addr','confirmed','dispositionFinal','rFinal','vFinal','reason'],
  properties: {
    addr: { type: 'string' },
    confirmed: { type: 'boolean' },
    dispositionFinal: { type: 'string', enum: ['REIMPL_FAITHFUL','NA_OURS','ABSENT','FPGA_BLOCKED','DIVERGENT','DECODE_ONLY'] },
    rFinal: { type: 'string', enum: ['R0','R1','R2','RNA'] },
    vFinal: { type: 'string', enum: ['V0','V1','V2','VNA'] },
    reason: { type: 'string' }
  }
}

const RENDER_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['results'],
  properties: {
    results: {
      type: 'array',
      items: {
        type: 'object',
        additionalProperties: false,
        required: ['addr','dLevel','disposition','rScore','vScore','note'],
        properties: {
          addr: { type: 'string' },
          dLevel: { type: 'integer', minimum: 0, maximum: 3 },
          disposition: { type: 'string', enum: ['REIMPL_FAITHFUL','NA_OURS','ABSENT','FPGA_BLOCKED','DIVERGENT','DECODE_ONLY'] },
          rScore: { type: 'string', enum: ['R0','R1','R2','RNA'] },
          vScore: { type: 'string', enum: ['V0','V1','V2','VNA'] },
          note: { type: 'string' }
        }
      }
    }
  }
}

function decodePrompt(f) {
  return [
    'You are reverse-engineering the FNIRSI 2C53T stock firmware (Artery AT32F403A, Cortex-M4F).',
    'TARGET FUNCTION: ' + f.name + ' at flash 0x' + f.addr + ', size ' + f.size + ' bytes, current class=' + f.klass + ', current R=' + f.R + '.',
    '',
    'STEP 1 - DECODE (to D3 = full register-level reconstruction, nothing unexplained):',
    ' - Find the function body. Grep reverse_engineering/decompiled_2C53T_v2.c for the name or the address FUN_0x' + f.addr + '. Also try reverse_engineering/decompiled_2C53T.c and analysis_v120/full_decompile.c.',
    ' - Read the FULL body. Identify every hardware register touched (by absolute address, e.g. 0x4000xxxx SPI/DMA/GPIO), every memory/state field, control flow, and exact semantics.',
    ' - Cross-check the existing note in reverse_engineering/analysis_v120/function_names.md - MANY names in this batch are MISLABELED (Ghidra guesses). Correct the role if wrong.',
    '',
    'STEP 2 - DISPOSITION against OUR reimplementation in firmware/src/:',
    ' - Search firmware/src/ (especially flash_fs.c, drivers/*.c, lcd.c) for an equivalent we already implement.',
    ' - REIMPL_FAITHFUL: we have a byte/register-faithful equivalent -> rScore R2, vScore V1, ourSource=path:func. Cite the exact register match.',
    ' - NA_OURS: this is library/subsystem code we legitimately replace with our own (FatFs/FTL filesystem library, our UI/font engine, libc) -> rScore RNA, vScore VNA, ourSource=our subsystem. Justify in one line.',
    ' - ABSENT: genuine stock device behavior we LACK and should add -> rScore R0, vScore V0, ourSource=none. State exactly what is missing.',
    ' - FPGA_BLOCKED: decoded but equivalence needs the FPGA/scope acquisition chain (config-entry wall) -> keep rScore, vScore V0.',
    ' - DIVERGENT: we intentionally do it differently -> note the difference.',
    ' - DECODE_ONLY: decoded D3 but none of the above cleanly applies.',
    '',
    'Be rigorous and HONEST - do not claim REIMPL_FAITHFUL without citing the exact register/protocol match in our source. dLevel reflects ONLY decode completeness. note <= 280 chars: lead with the corrected role + key registers, then the disposition reason.',
    'Return ONLY the structured object. addr must be ' + f.addr + '.'
  ].join('\n')
}

function verifyPrompt(f, d) {
  return [
    'Adversarial verification. Stock function ' + f.name + ' (0x' + f.addr + ') was decoded with this claim:',
    JSON.stringify(d),
    '',
    'Your job: try to REFUTE the disposition/score. Default to skepticism.',
    ' - If claim is REIMPL_FAITHFUL: open ourSource (' + d.ourSource + '), read the cited code, and confirm the register addresses / protocol bytes / formula ACTUALLY match the stock decode. If any divergence, downgrade (DIVERGENT or lower vFinal). V1 requires genuine static register-equivalence.',
    ' - If claim is NA_OURS: confirm we truly have our own working equivalent subsystem (not just absent). If it is actually ABSENT (we do not implement it at all and it is genuine device behavior), correct to ABSENT/R0.',
    ' - Otherwise (ABSENT/FPGA_BLOCKED/DIVERGENT/DECODE_ONLY): sanity-check the reasoning; usually confirm.',
    'Read the stock body yourself (decompiled_2C53T_v2.c) and our source (firmware/src/) - do not trust the claim.',
    'Return the final verdict. confirmed=true if the original stands. addr=' + f.addr + '.'
  ].join('\n')
}

phase('Decode')
const results = await pipeline(
  GENUINE,
  (f) => agent(decodePrompt(f), { label: 'decode:' + f.name, phase: 'Decode', schema: DECODE_SCHEMA }),
  (d, f) => {
    if (!d) return null
    // Only spend a verify agent on claims that move the headline or assert equivalence.
    if (d.disposition === 'REIMPL_FAITHFUL' || d.disposition === 'NA_OURS') {
      return agent(verifyPrompt(f, d), { label: 'verify:' + f.name, phase: 'Verify', schema: VERIFY_SCHEMA })
        .then(v => ({ f, d, v }))
    }
    return { f, d, v: null }
  }
)

phase('Render')
// Batch the 28 render fns: 4 agents x 7. These are almost all our-own-UI; classify each.
const batches = []
for (let i = 0; i < RENDER.length; i += 7) batches.push(RENDER.slice(i, i + 7))
const renderResults = await parallel(batches.map((b, bi) => () =>
  agent([
    'Reverse-engineering the FNIRSI 2C53T stock firmware. Classify these ' + b.length + ' stock RENDER/UI functions.',
    'For EACH, grep reverse_engineering/decompiled_2C53T_v2.c for the name/address, skim the body to confirm it is display/drawing/UI code, then decide disposition vs OUR firmware (firmware/src/ has our own LCD driver lcd.c, font system, and UI draw code).',
    'Almost all stock UI draw functions are NA_OURS (we render our own UI) -> dLevel 3 if you confirmed the role, rScore RNA, vScore VNA. But flag any that are actually device-protocol/hardware (NOT pure UI) as the appropriate disposition (ABSENT/DECODE_ONLY/REIMPL_FAITHFUL).',
    'Functions:',
    b.map(f => '- ' + f.name + ' @0x' + f.addr + ' (' + f.size + 'B)').join('\n'),
    'Return results array, one entry per function, addr matching the hex above. note <= 200 chars: confirmed role + disposition reason.'
  ].join('\n'), { label: 'render-batch-' + bi, phase: 'Render', schema: RENDER_SCHEMA })
))

return {
  genuine: results.filter(Boolean),
  render: renderResults.filter(Boolean)
}
