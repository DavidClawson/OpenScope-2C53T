/*
 * OpenScope 2C53T - Oscilloscope Runtime State
 */

#include "scope_state.h"

/* ═══════════════════════════════════════════════════════════════════
 * Lookup tables
 * ═══════════════════════════════════════════════════════════════════ */

const vdiv_entry_t vdiv_table[VDIV_COUNT] = {
    { "5mV",   5000 },
    { "10mV",  10000 },
    { "20mV",  20000 },
    { "50mV",  50000 },
    { "100mV", 100000 },
    { "200mV", 200000 },
    { "500mV", 500000 },
    { "1V",    1000000 },
    { "2V",    2000000 },
    { "5V",    5000000 },
};

const timebase_entry_t timebase_table[TIMEBASE_COUNT] = {
    { "5ns",   5 },
    { "10ns",  10 },
    { "20ns",  20 },
    { "50ns",  50 },
    { "100ns", 100 },
    { "200ns", 200 },
    { "500ns", 500 },
    { "1us",   1000 },
    { "2us",   2000 },
    { "5us",   5000 },
    { "10us",  10000 },
    { "20us",  20000 },
    { "50us",  50000 },
    { "100us", 100000 },
    { "200us", 200000 },
    { "500us", 500000 },
    { "1ms",   1000000 },
    { "2ms",   2000000 },
    { "5ms",   5000000 },
    { "10ms",  10000000 },
    { "20ms",  20000000 },
};

const char *coupling_labels[COUPLING_COUNT] = { "DC", "AC", "GND" };
const char *probe_labels[PROBE_COUNT] = { "1X", "10X" };
const char *trigger_mode_labels[TRIG_COUNT] = { "Auto", "Normal", "Single" };
const char *trigger_edge_labels[TRIG_EDGE_COUNT] = { "Rising", "Falling" };
const char *trigger_source_labels[TRIG_SRC_COUNT] = { "CH1", "CH2" };

/* ═══════════════════════════════════════════════════════════════════
 * Global singleton
 * ═══════════════════════════════════════════════════════════════════ */

static scope_state_t g_scope;

void scope_state_init(scope_state_t *s)
{
    /* CH1: enabled, 2V/div, DC coupling, 1X probe */
    s->ch1.enabled   = true;
    s->ch1.vdiv_idx  = 8;     /* 2V */
    s->ch1.coupling  = COUPLING_DC;
    s->ch1.probe     = PROBE_1X;
    s->ch1.bw_limit  = false;
    s->ch1.position  = 0;

    /* CH2: enabled, 200mV/div, DC coupling, 1X probe */
    s->ch2.enabled   = true;
    s->ch2.vdiv_idx  = 5;     /* 200mV */
    s->ch2.coupling  = COUPLING_DC;
    s->ch2.probe     = PROBE_1X;
    s->ch2.bw_limit  = false;
    s->ch2.position  = 0;

    /* Trigger: auto, rising edge, CH1, center level */
    s->trigger.mode   = TRIG_AUTO;
    s->trigger.edge   = TRIG_RISING;
    s->trigger.source = TRIG_SRC_CH1;
    s->trigger.level  = 0;

    /* Timebase: 50us/div */
    s->timebase_idx = 12;     /* 50us */

    /* Running */
    s->running = true;
}

scope_state_t *scope_state_get(void)
{
    return &g_scope;
}

/* ═══════════════════════════════════════════════════════════════════
 * Mutators (cycle/adjust with bounds checking)
 * ═══════════════════════════════════════════════════════════════════ */

void scope_cycle_trigger_mode(scope_state_t *s)
{
    s->trigger.mode = (trigger_mode_t)((s->trigger.mode + 1) % TRIG_COUNT);
}

void scope_cycle_trigger_edge(scope_state_t *s)
{
    s->trigger.edge = (trigger_edge_t)((s->trigger.edge + 1) % TRIG_EDGE_COUNT);
}

void scope_cycle_trigger_source(scope_state_t *s)
{
    s->trigger.source = (trigger_source_t)((s->trigger.source + 1) % TRIG_SRC_COUNT);
}

void scope_cycle_coupling(channel_state_t *ch)
{
    ch->coupling = (coupling_t)((ch->coupling + 1) % COUPLING_COUNT);
}

void scope_cycle_probe(channel_state_t *ch)
{
    ch->probe = (probe_t)((ch->probe + 1) % PROBE_COUNT);
}

void scope_toggle_bw_limit(channel_state_t *ch)
{
    ch->bw_limit = !ch->bw_limit;
}

void scope_toggle_channel(channel_state_t *ch)
{
    ch->enabled = !ch->enabled;
}

void scope_adjust_vdiv(channel_state_t *ch, int direction)
{
    int idx = (int)ch->vdiv_idx + direction;
    if (idx < 0) idx = 0;
    if (idx >= VDIV_COUNT) idx = VDIV_COUNT - 1;
    ch->vdiv_idx = (uint8_t)idx;
}

void scope_adjust_timebase(scope_state_t *s, int direction)
{
    int idx = (int)s->timebase_idx + direction;
    if (idx < 0) idx = 0;
    if (idx >= TIMEBASE_COUNT) idx = TIMEBASE_COUNT - 1;
    s->timebase_idx = (uint8_t)idx;
}

void scope_adjust_trigger_level(scope_state_t *s, int direction)
{
    s->trigger.level += direction * 5;
    if (s->trigger.level < -100) s->trigger.level = -100;
    if (s->trigger.level > 100) s->trigger.level = 100;
}

void scope_toggle_running(scope_state_t *s)
{
    s->running = !s->running;
}
