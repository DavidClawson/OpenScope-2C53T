/*
 * OpenScope 2C53T - Input Handler
 *
 * All button-to-action mapping logic lives here.
 * The input task (in main.c) handles GPIO polling and debounce,
 * then calls input_handle_button() for the actual logic.
 */

#include "input_handler.h"
#include "ui.h"
#include "scope_state.h"
#include "signal_gen.h"
#include "theme.h"
#include <stdio.h>

#ifdef FEATURE_FFT
#include "fft.h"
#include "fft_test_signals.h"
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Settings OK handler
 * ═══════════════════════════════════════════════════════════════════ */

void input_handle_settings_ok(void)
{
    scope_state_t *ss = scope_state_get();

    if (settings_depth == 0) {
        switch (settings_selected) {
        case 0: /* Oscilloscope Settings — enter sub-menu */
            settings_depth = 1;
            settings_sub_selected = 0;
            break;
        case 3: /* Display Mode — cycle theme */
            theme_cycle();
            break;
        case 5: /* About — display info screen */
            settings_depth = 2;
            break;
        default:
            break;
        }
    } else if (settings_depth == 1) {
        /* Oscilloscope settings sub-menu */
        switch (settings_sub_selected) {
        case 0: scope_cycle_coupling(&ss->ch1); break;
        case 1: scope_cycle_probe(&ss->ch1); break;
        case 2: scope_toggle_bw_limit(&ss->ch1); break;
        case 3: scope_cycle_coupling(&ss->ch2); break;
        case 4: scope_cycle_probe(&ss->ch2); break;
        case 5: scope_toggle_bw_limit(&ss->ch2); break;
        case 6: scope_cycle_trigger_mode(ss); break;
        case 7: scope_cycle_trigger_edge(ss); break;
        default: break;
        }
    } else if (settings_depth == 2) {
        /* About screen — OK goes back */
        settings_depth = 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Helper: send a display command
 * ═══════════════════════════════════════════════════════════════════ */

static void send_cmd(QueueHandle_t q, uint8_t cmd)
{
    xQueueSend(q, &cmd, 0);
}

/* Helper: show popup and send redraw */
static void popup_and_redraw(QueueHandle_t q, const char *text)
{
    scope_show_popup(text);
    uint8_t cmd = DCMD_REDRAW_ALL;
    send_cmd(q, cmd);
}

/* ═══════════════════════════════════════════════════════════════════
 * Main button dispatcher
 * ═══════════════════════════════════════════════════════════════════ */

uint8_t input_handle_button(button_id_t button, QueueHandle_t dq)
{
    scope_state_t *ss = scope_state_get();
    uint8_t cmd = DCMD_REDRAW_ALL;
    char pb[24];

    switch (button) {

    /* ── Mode / Navigation ──────────────────────────────────── */

    case BTN_MENU:
        if (current_mode == MODE_SETTINGS && settings_depth > 0) {
            settings_depth = 0;
            settings_sub_selected = 0;
        } else {
            current_mode = (device_mode_t)((current_mode + 1) % MODE_COUNT);
            if (current_mode == MODE_SETTINGS) {
                settings_selected = 0;
                settings_depth = 0;
            }
        }
        send_cmd(dq, cmd);
        break;

    /* ── Channel buttons ────────────────────────────────────── */

    case BTN_CH1:
        if (current_mode == MODE_OSCILLOSCOPE) {
            active_channel = 0;
            scope_cycle_coupling(&ss->ch1);
            snprintf(pb, sizeof(pb), "CH1 %s",
                     coupling_labels[ss->ch1.coupling]);
            popup_and_redraw(dq, pb);
        } else {
            send_cmd(dq, cmd);
        }
        break;

    case BTN_CH2:
        if (current_mode == MODE_OSCILLOSCOPE) {
            active_channel = 1;
            scope_cycle_coupling(&ss->ch2);
            snprintf(pb, sizeof(pb), "CH2 %s",
                     coupling_labels[ss->ch2.coupling]);
            popup_and_redraw(dq, pb);
        } else {
            send_cmd(dq, cmd);
        }
        break;

    /* ── Trigger ────────────────────────────────────────────── */

    case BTN_TRIGGER:
        if (current_mode == MODE_OSCILLOSCOPE) {
            scope_cycle_trigger_mode(ss);
            snprintf(pb, sizeof(pb), "Trig: %s",
                     trigger_mode_labels[ss->trigger.mode]);
            popup_and_redraw(dq, pb);
        } else {
            send_cmd(dq, cmd);
        }
        break;

    case BTN_MOVE:
        if (current_mode == MODE_OSCILLOSCOPE) {
            scope_cycle_trigger_edge(ss);
            snprintf(pb, sizeof(pb), "Edge: %s",
                     trigger_edge_labels[ss->trigger.edge]);
            popup_and_redraw(dq, pb);
        } else {
            send_cmd(dq, cmd);
        }
        break;

    /* ── Save ───────────────────────────────────────────────── */

    case BTN_SAVE:
        /* TODO: screenshot capture when flash is available */
        send_cmd(dq, cmd);
        break;

    /* ── Auto ───────────────────────────────────────────────── */

    case BTN_AUTO:
        if (current_mode == MODE_OSCILLOSCOPE) {
#ifdef FEATURE_FFT
            if (scope_view != SCOPE_VIEW_TIME) {
                test_signal_generate(TEST_SIG_SQUARE, fft_sample_buf,
                                     FFT_SIZE, fft_get_config()->sample_rate_hz,
                                     1000.0f, 0.0f, 0.8f);
                fft_auto_configure(fft_sample_buf, FFT_SIZE);
            }
#endif
            send_cmd(dq, cmd);
        }
        break;

    /* ── PRM / SELECT ───────────────────────────────────────── */

#ifdef FEATURE_FFT
    case BTN_PRM:
        if (current_mode == MODE_OSCILLOSCOPE) {
            scope_view = (scope_view_t)((scope_view + 1) % SCOPE_VIEW_COUNT);
            send_cmd(dq, cmd);
        }
        break;

    case BTN_SELECT:
        if (current_mode == MODE_OSCILLOSCOPE &&
            scope_view != SCOPE_VIEW_TIME) {
            fft_cycle_window();
            send_cmd(dq, cmd);
        } else
#endif
        if (current_mode == MODE_SIGNAL_GEN) {
            siggen_cycle_waveform();
            send_cmd(dq, cmd);
        } else if (current_mode == MODE_OSCILLOSCOPE) {
            channel_state_t *ch = (active_channel == 0) ? &ss->ch1 : &ss->ch2;
            scope_cycle_probe(ch);
            send_cmd(dq, cmd);
        }
        break;

    /* ── UP / DOWN ──────────────────────────────────────────── */

    case BTN_UP:
        if (current_mode == MODE_SETTINGS) {
            if (settings_depth == 0) {
                if (settings_selected > 0) settings_selected--;
            } else {
                if (settings_sub_selected > 0) settings_sub_selected--;
            }
            cmd = DCMD_DRAW_SETTINGS;
            send_cmd(dq, cmd);
        }
#ifdef FEATURE_FFT
        else if (current_mode == MODE_OSCILLOSCOPE &&
                 scope_view != SCOPE_VIEW_TIME) {
            fft_adjust_ref_level(5.0f);
            send_cmd(dq, cmd);
        }
#endif
        else if (current_mode == MODE_OSCILLOSCOPE) {
            channel_state_t *ch = (active_channel == 0) ? &ss->ch1 : &ss->ch2;
            scope_adjust_vdiv(ch, 1);
            snprintf(pb, sizeof(pb), "CH%d %s/div",
                     active_channel + 1, vdiv_table[ch->vdiv_idx].label);
            popup_and_redraw(dq, pb);
        }
        else if (current_mode == MODE_SIGNAL_GEN) {
            const siggen_config_t *sc = siggen_get_config();
            float f = sc->frequency_hz;
            if (f < 10.0f) f = 10.0f;
            else if (f < 100.0f) f = 100.0f;
            else if (f < 1000.0f) f = 1000.0f;
            else if (f < 10000.0f) f = 10000.0f;
            else f = 25000.0f;
            siggen_set_frequency(f);
            send_cmd(dq, cmd);
        }
        break;

    case BTN_DOWN:
        if (current_mode == MODE_SETTINGS) {
            if (settings_depth == 0) {
                if (settings_selected < SETTINGS_ITEM_COUNT - 1)
                    settings_selected++;
            } else {
                if (settings_sub_selected < SETTINGS_OSC_ITEM_COUNT - 1)
                    settings_sub_selected++;
            }
            cmd = DCMD_DRAW_SETTINGS;
            send_cmd(dq, cmd);
        }
#ifdef FEATURE_FFT
        else if (current_mode == MODE_OSCILLOSCOPE &&
                 scope_view != SCOPE_VIEW_TIME) {
            fft_adjust_ref_level(-5.0f);
            send_cmd(dq, cmd);
        }
#endif
        else if (current_mode == MODE_OSCILLOSCOPE) {
            channel_state_t *ch = (active_channel == 0) ? &ss->ch1 : &ss->ch2;
            scope_adjust_vdiv(ch, -1);
            snprintf(pb, sizeof(pb), "CH%d %s/div",
                     active_channel + 1, vdiv_table[ch->vdiv_idx].label);
            popup_and_redraw(dq, pb);
        }
        else if (current_mode == MODE_SIGNAL_GEN) {
            const siggen_config_t *sc = siggen_get_config();
            float f = sc->frequency_hz;
            if (f > 10000.0f) f = 10000.0f;
            else if (f > 1000.0f) f = 1000.0f;
            else if (f > 100.0f) f = 100.0f;
            else if (f > 10.0f) f = 10.0f;
            else f = 1.0f;
            siggen_set_frequency(f);
            send_cmd(dq, cmd);
        }
        break;

    /* ── LEFT / RIGHT ───────────────────────────────────────── */

    case BTN_LEFT:
#ifdef FEATURE_FFT
        if (current_mode == MODE_OSCILLOSCOPE &&
            scope_view != SCOPE_VIEW_TIME) {
            fft_zoom_in();
            send_cmd(dq, cmd);
        } else
#endif
        if (current_mode == MODE_OSCILLOSCOPE) {
            scope_adjust_timebase(ss, -1);
            snprintf(pb, sizeof(pb), "H=%s/div",
                     timebase_table[ss->timebase_idx].label);
            popup_and_redraw(dq, pb);
        }
        break;

    case BTN_RIGHT:
#ifdef FEATURE_FFT
        if (current_mode == MODE_OSCILLOSCOPE &&
            scope_view != SCOPE_VIEW_TIME) {
            fft_zoom_out();
            send_cmd(dq, cmd);
        } else
#endif
        if (current_mode == MODE_OSCILLOSCOPE) {
            scope_adjust_timebase(ss, 1);
            snprintf(pb, sizeof(pb), "H=%s/div",
                     timebase_table[ss->timebase_idx].label);
            popup_and_redraw(dq, pb);
        }
        break;

    /* ── OK ──────────────────────────────────────────────────── */

    case BTN_OK:
        if (current_mode == MODE_SETTINGS) {
            input_handle_settings_ok();
            send_cmd(dq, cmd);
        } else if (current_mode == MODE_SIGNAL_GEN) {
            const siggen_config_t *sc = siggen_get_config();
            siggen_enable(!sc->output_enabled);
            send_cmd(dq, cmd);
        } else if (current_mode == MODE_OSCILLOSCOPE) {
            scope_toggle_running(ss);
            scope_show_popup(ss->running ? "RUN" : "STOP");
            send_cmd(dq, cmd);
        }
        break;

    default:
        break;
    }

    return cmd;
}
