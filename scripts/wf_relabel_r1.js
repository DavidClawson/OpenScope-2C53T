export const meta = {
  name: 'relabel-and-decode-r1',
  description: 'Phase 1: rename 46 mislabeled FreeRTOS kernel fns to correct upstream symbols. Phase 2: decode the 42 hardware/protocol R1 functions to D3 and give honest resolve verdicts.',
  phases: [
    { title: 'Relabel' },
    { title: 'Decode-R1' },
    { title: 'Tally-R1' },
  ],
}

const DEC = '/Users/david/Projects/RESEARCH/osc/reverse_engineering/decompiled_2C53T_v2.c'
const FULLDEC = '/Users/david/Projects/RESEARCH/osc/reverse_engineering/analysis_v120/full_decompile.c'
const FW = '/Users/david/Projects/RESEARCH/osc/firmware/src'
const FREERTOS = '/Users/david/Projects/RESEARCH/osc/firmware/FreeRTOS'

const RELABEL = ["080349b8|freertos_list_insert_sorted","08034ab0|freertos_timer_command","08034bc0|freertos_timer_pend_call","08034c24|freertos_timer_process_expired","08034c80|freertos_task_set_timeout","08034cc4|freertos_queue_copy_item","08034d90|freertos_timer_reload","08034da8|freertos_task_increment_tick","08034dd0|freertos_port_yield_from_isr","08034e10|freertos_timer_create_static","08034f04|freertos_timer_start_from_isr","08034f44|freertos_timer_daemon_init","08035058|freertos_timer_pend_callback","080350d0|freertos_timer_execute_callback","0803515c|freertos_timer_switch_lists","08035224|freertos_queue_item_init","080352ac|freertos_list_remove_head","080352d4|freertos_queue_check_space","08035304|freertos_port_disable_interrupts","0803532c|freertos_task_create_static","080353b4|freertos_scheduler_start","08035504|freertos_task_create","080355a0|freertos_critical_nesting_inc","080355dc|freertos_scheduler_suspend","08035620|freertos_scheduler_resume","08035730|freertos_task_get_current","080357b8|freertos_ready_list_add","0803a024|freertos_list_remove","0803a06c|freertos_port_enter_critical","0803a09c|freertos_port_exit_critical","0803a0ac|freertos_list_insert_end","0803a118|freertos_list_init","0803a154|freertos_port_save_context","0803a6d8|freertos_timer_start","0803a78c|freertos_systick_handler","0803a904|freertos_port_yield","0803a914|freertos_port_pendsv_handler","0803a9d4|freertos_timer_delete","0803ab74|freertos_timer_reset","0803ac38|freertos_timer_stop","0803acf0|xQueueGenericSend","0803af08|xQueueGenericSendFromISR","0803b06c|freertos_queue_notify_from_isr","0803b09c|xQueueReceiveFromISR","0803b1d8|xQueueReceive","0803be38|freertos_tick_handler"]

const R1 = ["0800039c|FUN_0800039c|hardware","080018a4|gpio_mux_portc_porte|hardware","08001a58|gpio_mux_porta_portb|hardware","08001c60|siggen_configure|hardware","080028e0|meter_data_process|protocol","08002c78|FUN_08002c78|hardware","08009014|meter_mode_init_ac_voltage|protocol","080096e8|meter_mode_init_dc_voltage|protocol","08009a94|meter_mode_init_resistance|protocol","0800b908|FUN_0800b908|protocol","0800ba06|probe_detect_handler_1|protocol","0800bb10|probe_detect_handler_2|protocol","0800bba6|probe_continuity_test|protocol","0800bc00|probe_component_test|protocol","0800bc98|probe_detect_handler_3|protocol","08019e98|scope_main_fsm|protocol","0801d2ec|scope_mode_timebase|protocol","0801e1e4|scope_mode_trigger|protocol","0801eaac|scope_mode_measure|protocol","0801efc0|scope_mode_math|protocol","0801f6f8|scope_measurement_engine|protocol","0802771c|tmr3_isr|hardware","08028314|gpio_port_d_set_pin|hardware","0802a514|gpio_port_b_configure_pin|hardware","0802a534|scope_state_handler|protocol","0802ce94|spi_flash_read_status|hardware","0802cf38|spi_flash_write_disable|hardware","0802cf7c|spi_flash_init_controller|hardware","0802cfbc|spi_flash_chip_detect|hardware","0802d014|spi_flash_read_with_cache|hardware","0802d1c4|spi_flash_status_check|hardware","0802d1e8|spi_flash_write_sectors|hardware","0802eee8|spi2_read_jedec_id|hardware","0802ef48|spi2_init|hardware","080302fc|gpio_pin_config|hardware","080304e0|gpio_read_input|hardware","08036820|spi_peripheral_reset|hardware","08036830|spi_peripheral_enable|hardware","0803683c|spi_check_flag|hardware","08036848|spi_peripheral_init|hardware","08039734|usart_tx_config_writer|protocol","0803ee20|scope_read_adc_sample|protocol"]

const RELABEL_SCHEMA = {
  type: 'object', additionalProperties: false, required: ['results'],
  properties: { results: { type: 'array', items: {
    type: 'object', additionalProperties: false,
    required: ['addr','old_name','correct_name','freertos_symbol','confidence','evidence'],
    properties: {
      addr: { type: 'string' },
      old_name: { type: 'string' },
      correct_name: { type: 'string', description: 'The corrected ledger name (the real upstream symbol, e.g. xTaskIncrementTick or prvAddNewTaskToReadyList).' },
      freertos_symbol: { type: 'string', description: 'Exact upstream FreeRTOS symbol plus source file, e.g. xQueueGenericSend in queue.c. Use UNSURE if not pinnable.' },
      confidence: { type: 'string', enum: ['high','med','low'] },
      evidence: { type: 'string', description: 'What in the decompiled behavior plus which FreeRTOS source confirms the match.' },
    },
  } } },
}

const R1_SCHEMA = {
  type: 'object', additionalProperties: false, required: ['results'],
  properties: { results: { type: 'array', items: {
    type: 'object', additionalProperties: false,
    required: ['addr','name','new_D','classification','our_impl','new_R','new_V','resolved','findings','next_action'],
    properties: {
      addr: { type: 'string' },
      name: { type: 'string' },
      new_D: { type: 'integer', minimum: 0, maximum: 3, description: 'Decode level after full register-level review.' },
      classification: { type: 'string', enum: ['VERIFY_FAITHFUL','DIVERGENT','FPGA_BLOCKED','ABSENT_R0','NA_OUR_OWN','DECODE_ONLY'] },
      our_impl: { type: 'string', description: 'our firmware file:func/line equivalent, or ABSENT.' },
      new_R: { type: 'string', enum: ['R0','R1','R2','NA'] },
      new_V: { type: 'string', enum: ['V0','V1','V2','NA'] },
      resolved: { type: 'boolean' },
      findings: { type: 'string', description: 'Register-level summary of what stock does plus the key divergence or equivalence vs ours. Cite stock addr and our file:line.' },
      next_action: { type: 'string', description: 'Concrete step to resolve (reimplement what, verify how) or why blocked.' },
    },
  } } },
}

const TALLY_SCHEMA = {
  type: 'object', additionalProperties: false,
  required: ['by_classification','newly_resolved','resolved_list','fpga_blocked_list','reimpl_roadmap','flags','notes'],
  properties: {
    by_classification: { type: 'object', additionalProperties: true },
    newly_resolved: { type: 'integer' },
    resolved_list: { type: 'array', items: { type: 'string' } },
    fpga_blocked_list: { type: 'array', items: { type: 'string' } },
    reimpl_roadmap: { type: 'array', items: {
      type: 'object', additionalProperties: false, required: ['name','effort','what'],
      properties: { name: { type: 'string' }, effort: { type: 'string', enum: ['low','med','high'] }, what: { type: 'string' } },
    } },
    flags: { type: 'array', items: { type: 'string' } },
    notes: { type: 'string' },
  },
}

function batch(arr, n) {
  const out = []
  for (let i = 0; i < arr.length; i += n) out.push(arr.slice(i, i + n))
  return out
}

// Phase 1: Relabel kernel functions
phase('Relabel')
const relabelPrompt = (b) => [
  'Correct the Ghidra NAMES of FreeRTOS kernel functions in the stock FNIRSI 2C53T binary.',
  'Each was confirmed to be vendored FreeRTOS source we compile, but its Ghidra name is often WRONG',
  '(e.g. a timer_ name on a queue/task/heap primitive). Pin each to its real upstream symbol.',
  '',
  'For each: read the stock decompile (' + DEC + ' or ' + FULLDEC + ', search for the FUN_ address)',
  'and match its behavior to the FreeRTOS source under ' + FREERTOS + '/Source/ (tasks.c, queue.c,',
  'timers.c, list.c, event_groups.c) and the port layer ' + FREERTOS + '/Source/portable/GCC/ARM_CM4F/port.c.',
  'Identify the exact symbol. If one Ghidra function fuses two upstream functions, name the dominant one and say so.',
  'Use UNSURE only if genuinely unpinnable.',
  '',
  'BATCH (addr|current_name):',
  b.join('\n'),
  '',
  'Return corrected names with the upstream symbol, source file, and evidence.',
].join('\n')

const relabelResults = (await parallel(batch(RELABEL, 8).map((b, bi) => () =>
  agent(relabelPrompt(b), { label: 'relabel:b' + bi, phase: 'Relabel', schema: RELABEL_SCHEMA })
))).filter(Boolean).flatMap(r => r.results || [])

// Phase 2: Decode + resolve the R1 driver/protocol cluster
phase('Decode-R1')
const r1Prompt = (b) => [
  'Rigorous decode-and-resolve of stock FNIRSI 2C53T HARDWARE/PROTOCOL functions currently scored R1 (partial reimpl)',
  'and unresolved. For EACH: decode to full register level (D3 if nothing unexplained) and give an HONEST resolve',
  'verdict against our reverse-engineered reimplementation firmware.',
  '',
  'Context: MCU = Artery AT32F403A (STM32F1-compatible GPIO/EXMC, AT32 HAL). Our firmware under ' + FW,
  '(drivers/fpga.c, drivers/lcd.c, drivers/flash_fs.c, drivers/meter_data.c, drivers/dac_output.c, drivers/button_scan.c,',
  'main.c, etc.). Stock decompile: ' + DEC + ' / ' + FULLDEC + ' (search for the FUN_ address).',
  'The scope acquisition path is NON-FUNCTIONAL on our firmware (FPGA config-entry wall) — so anything that can only be',
  'proven by reading live scope data is FPGA_BLOCKED, not VERIFY_FAITHFUL.',
  '',
  'RUBRIC (resolved = D3 AND (faithful+verified OR justified-NA)):',
  '- VERIFY_FAITHFUL -> R2/V1/resolved: our firmware mirrors stock register behavior; CITE our file:line and matching ops vs stock addr. Only if you actually confirm equivalence by reading both.',
  '- DIVERGENT -> unresolved: we intentionally do it differently (e.g. meter_data_process uses our corrected band logic). Note the divergence.',
  '- FPGA_BLOCKED -> unresolved: equivalence needs a live FPGA/scope. Decode fully (D3) and say what bench step verifies it.',
  '- ABSENT_R0 -> R0, unresolved: not in our firmware.',
  '- NA_OUR_OWN -> R=NA/resolved: our own original implementation, justified-not-needed (cite ours + how stock differs).',
  '- DECODE_ONLY -> unresolved: fully decoded but none of the above cleanly applies.',
  '',
  'Be skeptical — do NOT stamp VERIFY_FAITHFUL without reading our actual code. The gpio_mux/scope_/meter_ functions are',
  'the load-bearing spine; decode them properly. The spi_flash_/spi2_ cluster likely maps to our flash_fs.c (some',
  'VERIFY_FAITHFUL reads, ABSENT writes). gpio_pin_config/gpio_read_input likely map to our GPIO init.',
  '',
  'BATCH (addr|name|class):',
  b.join('\n'),
  '',
  'Read the stock decompile and our firmware, then return the verdict object per function with concrete evidence.',
].join('\n')

const r1Results = (await parallel(batch(R1, 5).map((b, bi) => () =>
  agent(r1Prompt(b), { label: 'r1:b' + bi, phase: 'Decode-R1', schema: R1_SCHEMA })
))).filter(Boolean).flatMap(r => r.results || [])

// Phase 3: Tally R1
phase('Tally-R1')
const tallyPrompt = [
  'Tally these ' + r1Results.length + ' decode-and-resolve verdicts for the R1 hardware/protocol cluster.',
  'Report: counts by classification; newly-resolved (VERIFY_FAITHFUL + NA_OUR_OWN) with list; the FPGA_BLOCKED list',
  '(decoded but bench-gated); and a REIMPL ROADMAP = the DIVERGENT/ABSENT_R0/DECODE_ONLY functions worth faithfully',
  'reimplementing to grow the numerator, ordered by value/effort. Flag any weak/over-eager VERIFY_FAITHFUL.',
  'Be honest about how much is real resolution vs decode-only.',
  '',
  'Verdicts (JSON):',
  JSON.stringify(r1Results, null, 1),
].join('\n')
const tally = await agent(tallyPrompt, { label: 'tally-r1', phase: 'Tally-R1', schema: TALLY_SCHEMA })

return { relabel: relabelResults, r1: r1Results, tally }
