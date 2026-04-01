/*
 * OpenScope 2C53T - Input Handler
 *
 * All button-to-action mapping logic lives here.
 * The input task (in main.c) handles GPIO polling and debounce,
 * then calls input_handle_button() for the actual logic.
 *
 * Integrated features from 4 agent worktrees:
 *   - Cursor measurement mode (TRIGGER toggles, arrows move cursors)
 *   - Meter 10 sub-modes (LEFT/RIGHT cycle, SELECT resets min/max/avg)
 *   - Signal gen: amplitude stepping, duty cycle, frequency presets
 *   - Math channel, persistence, component tester (settings sub-menus)
 */

#include "input_handler.h"
#include "ui.h"
#include "scope_state.h"
#include "signal_gen.h"
#include "theme.h"
#include "math_channel.h"
#include "component_test.h"
#include "persistence.h"
#include "shared_mem.h"
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
        case 0: /* Oscilloscope Settings -- enter sub-menu */
            settings_depth = 1;
            settings_sub_selected = 0;
            break;
        case 3: /* Display Mode -- cycle theme */
            theme_cycle();
            break;
        case 4: /* Math Channel -- enter math sub-menu */
            settings_depth = 3;
            settings_sub_selected = 0;
            break;
        case 5: /* Component Tester -- enter component test screen */
            settings_depth = 4;
            comp_test_init();
            break;
        case 6: /* Bode Plot -- enter bode screen */
            settings_depth = 6;
            break;
        case 8: /* About -- display info screen */
            settings_depth = 2;
            break;
        case 9: /* Firmware Update -- reboot into DFU bootloader */
            {
                extern void dfu_request_reboot(void);
                dfu_request_reboot();
            }
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
        /* About screen -- OK goes back */
        settings_depth = 0;
    } else if (settings_depth == 3) {
        /* Math channel sub-menu */
        if (settings_sub_selected == 0) {
            /* Math Channel: cycle Off -> A+B -> A-B -> A*B -> ... -> Off */
            if (!math_enabled) {
                math_enabled = true;
                math_op = 0;
            } else if (math_op < MATH_COUNT - 1) {
                math_op++;
            } else {
                math_enabled = false;
                math_op = 0;
            }
        } else if (settings_sub_selected == 1) {
            /* Persistence: toggle on/off with pool lifecycle */
            persist_enabled = !persist_enabled;
            if (persist_enabled) {
                /* Persistence needs the pool — evicts FFT if active */
                if (fft_is_initialized())
                    fft_deinit();
                persist_init();
                persist_set_mode(PERSIST_MEDIUM);
            } else {
                persist_deinit();
            }
        } else if (settings_sub_selected == 2) {
            /* Back */
            settings_depth = 0;
        }
    } else if (settings_depth == 4) {
        /* Component tester: OK = enter resistor calculator */
        settings_depth = 5;
        resistor_calc_init();
    } else if (settings_depth == 5) {
        /* Resistor calculator: OK = back to component tester */
        settings_depth = 4;
    } else if (settings_depth == 6) {
        /* Bode plot: OK = back to settings */
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

    /* -- Mode / Navigation ---------------------------------------- */

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

    /* -- Channel buttons ------------------------------------------ */

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

    /* -- Trigger -------------------------------------------------- */

    case BTN_TRIGGER:
        if (current_mode == MODE_MULTIMETER) {
            meter_toggle_hold();
            send_cmd(dq, cmd);
        } else if (current_mode == MODE_OSCILLOSCOPE) {
#ifdef FEATURE_FFT
            if (scope_view == SCOPE_VIEW_TIME)
#endif
            {
                /* Toggle cursor mode in time view */
                scope_cursor_cycle_mode();
                send_cmd(dq, cmd);
                break;
            }
            /* Fall through for non-time views: cycle trigger mode */
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

    /* -- Save (screenshot) ---------------------------------------- */

    case BTN_SAVE:
        /* TODO: On real hardware, capture shadow framebuffer to SPI flash.
         * For now, show confirmation popup — the emulator's lcd_viewer
         * independently saves screenshots on 'S' key via its own BMP writer. */
        {
            static uint16_t screenshot_num = 0;
            screenshot_num++;
            char sb[24];
            snprintf(sb, sizeof(sb), "SAVED #%d", screenshot_num);
            scope_show_popup(sb);
        }
        send_cmd(dq, cmd);
        break;

    /* -- Auto ----------------------------------------------------- */

    case BTN_AUTO:
        if (current_mode == MODE_MULTIMETER) {
            meter_toggle_relative();
            send_cmd(dq, cmd);
        } else if (current_mode == MODE_OSCILLOSCOPE) {
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

    /* -- PRM / SELECT --------------------------------------------- */

#ifdef FEATURE_FFT
    case BTN_PRM:
        if (current_mode == MODE_OSCILLOSCOPE) {
            scope_view_t prev_view = scope_view;
            scope_view = (scope_view_t)((scope_view + 1) % SCOPE_VIEW_COUNT);

            /* Pool lifecycle: claim FFT pool when entering FFT views,
             * release when returning to time-domain */
            if (prev_view == SCOPE_VIEW_TIME && scope_view != SCOPE_VIEW_TIME) {
                /* Entering FFT view — claim pool */
                fft_config_t fft_cfg;
                fft_cfg.window         = FFT_WINDOW_HANNING;
                fft_cfg.sample_rate_hz = 44100.0f;
                fft_cfg.ref_level_db   = 0.0f;
                fft_cfg.db_range       = 80.0f;
                fft_cfg.peak_count     = 4;
                fft_cfg.avg_count      = 0;
                fft_cfg.max_hold       = false;
                fft_cfg.zoom_start_bin = 1;
                fft_cfg.zoom_end_bin   = FFT_BINS - 1;
                fft_init(&fft_cfg);
            } else if (prev_view != SCOPE_VIEW_TIME && scope_view == SCOPE_VIEW_TIME) {
                /* Returning to time-domain — release pool */
                fft_deinit();
            }
            send_cmd(dq, cmd);
        } else if (current_mode == MODE_SIGNAL_GEN) {
            /* Cycle frequency presets: 1 -> 10 -> 100 -> 1k -> 10k -> 25k -> 1 Hz */
            const siggen_config_t *sc = siggen_get_config();
            float f = sc->frequency_hz;
            if (f < 10.0f) f = 10.0f;
            else if (f < 100.0f) f = 100.0f;
            else if (f < 1000.0f) f = 1000.0f;
            else if (f < 10000.0f) f = 10000.0f;
            else if (f < 25000.0f) f = 25000.0f;
            else f = 1.0f;
            siggen_set_frequency(f);
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
        } else if (current_mode == MODE_MULTIMETER) {
            meter_reset_minmaxavg();
            send_cmd(dq, cmd);
        } else if (current_mode == MODE_SETTINGS &&
                   settings_depth == 5) {
            /* Resistor calc: simulate measurement */
            resistor_calc_simulate_measure();
            cmd = DCMD_DRAW_SETTINGS;
            send_cmd(dq, cmd);
        } else if (current_mode == MODE_SETTINGS &&
                   settings_depth == 4) {
            /* Component tester: cycle component type */
            comp_test_cycle_type();
            cmd = DCMD_DRAW_SETTINGS;
            send_cmd(dq, cmd);
        } else if (current_mode == MODE_OSCILLOSCOPE) {
            channel_state_t *ch = (active_channel == 0) ? &ss->ch1 : &ss->ch2;
            scope_cycle_probe(ch);
            send_cmd(dq, cmd);
        }
        break;

    /* -- UP / DOWN ------------------------------------------------ */

    case BTN_UP:
        if (current_mode == MODE_SETTINGS) {
            if (settings_depth == 5) {
                resistor_calc_change_color(1);
            } else if (settings_depth == 0) {
                if (settings_selected > 0) settings_selected--;
            } else if (settings_depth == 3) {
                if (settings_sub_selected > 0) settings_sub_selected--;
            } else if (settings_depth == 1) {
                if (settings_sub_selected > 0) settings_sub_selected--;
            }
            cmd = DCMD_DRAW_SETTINGS;
            send_cmd(dq, cmd);
        }
        /* Cursor movement in scope time view */
        else if (current_mode == MODE_OSCILLOSCOPE &&
                 ss->cursor.mode != CURSOR_OFF) {
            scope_cursor_move(-4);
            cmd = DCMD_DRAW_SCOPE;
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
            siggen_amplitude_up();
            send_cmd(dq, cmd);
        }
        break;

    case BTN_DOWN:
        if (current_mode == MODE_SETTINGS) {
            if (settings_depth == 5) {
                resistor_calc_change_color(-1);
            } else if (settings_depth == 0) {
                if (settings_selected < SETTINGS_ITEM_COUNT - 1)
                    settings_selected++;
            } else if (settings_depth == 3) {
                /* Math/persist sub-menu: 3 items (0,1,2) */
                if (settings_sub_selected < 2) settings_sub_selected++;
            } else if (settings_depth == 1) {
                if (settings_sub_selected < SETTINGS_OSC_ITEM_COUNT - 1)
                    settings_sub_selected++;
            }
            cmd = DCMD_DRAW_SETTINGS;
            send_cmd(dq, cmd);
        }
        /* Cursor movement */
        else if (current_mode == MODE_OSCILLOSCOPE &&
                 ss->cursor.mode != CURSOR_OFF) {
            scope_cursor_move(4);
            cmd = DCMD_DRAW_SCOPE;
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
            siggen_amplitude_down();
            send_cmd(dq, cmd);
        }
        break;

    /* -- LEFT / RIGHT --------------------------------------------- */

    case BTN_LEFT:
        if (current_mode == MODE_SETTINGS && settings_depth == 5) {
            resistor_calc_move_band(-1);
            cmd = DCMD_DRAW_SETTINGS;
            send_cmd(dq, cmd);
        }
        /* Cursor: switch active cursor */
        else if (current_mode == MODE_OSCILLOSCOPE &&
            ss->cursor.mode != CURSOR_OFF) {
            scope_cursor_next_sel();
            cmd = DCMD_DRAW_SCOPE;
            send_cmd(dq, cmd);
        }
        /* Meter: previous sub-mode */
        else if (current_mode == MODE_MULTIMETER) {
            if (meter_submode == 0)
                meter_submode = METER_SUBMODE_COUNT - 1;
            else
                meter_submode--;
            meter_reset_minmaxavg();
            send_cmd(dq, cmd);
        }
#ifdef FEATURE_FFT
        else if (current_mode == MODE_OSCILLOSCOPE &&
            scope_view != SCOPE_VIEW_TIME) {
            fft_zoom_in();
            send_cmd(dq, cmd);
        }
#endif
        else if (current_mode == MODE_SIGNAL_GEN) {
            siggen_duty_cycle_down();
            send_cmd(dq, cmd);
        }
        else if (current_mode == MODE_OSCILLOSCOPE) {
            scope_adjust_timebase(ss, -1);
            snprintf(pb, sizeof(pb), "H=%s/div",
                     timebase_table[ss->timebase_idx].label);
            popup_and_redraw(dq, pb);
        }
        break;

    case BTN_RIGHT:
        if (current_mode == MODE_SETTINGS && settings_depth == 5) {
            resistor_calc_move_band(1);
            cmd = DCMD_DRAW_SETTINGS;
            send_cmd(dq, cmd);
        }
        /* Cursor: switch active cursor */
        else if (current_mode == MODE_OSCILLOSCOPE &&
            ss->cursor.mode != CURSOR_OFF) {
            scope_cursor_next_sel();
            cmd = DCMD_DRAW_SCOPE;
            send_cmd(dq, cmd);
        }
        /* Meter: next sub-mode */
        else if (current_mode == MODE_MULTIMETER) {
            meter_submode = (meter_submode + 1) % METER_SUBMODE_COUNT;
            meter_reset_minmaxavg();
            send_cmd(dq, cmd);
        }
#ifdef FEATURE_FFT
        else if (current_mode == MODE_OSCILLOSCOPE &&
            scope_view != SCOPE_VIEW_TIME) {
            fft_zoom_out();
            send_cmd(dq, cmd);
        }
#endif
        else if (current_mode == MODE_SIGNAL_GEN) {
            siggen_duty_cycle_up();
            send_cmd(dq, cmd);
        }
        else if (current_mode == MODE_OSCILLOSCOPE) {
            scope_adjust_timebase(ss, 1);
            snprintf(pb, sizeof(pb), "H=%s/div",
                     timebase_table[ss->timebase_idx].label);
            popup_and_redraw(dq, pb);
        }
        break;

    /* -- OK ------------------------------------------------------- */

    case BTN_OK:
        if (current_mode == MODE_SETTINGS) {
            input_handle_settings_ok();
            send_cmd(dq, cmd);
        } else if (current_mode == MODE_SIGNAL_GEN) {
            const siggen_config_t *sc = siggen_get_config();
            siggen_enable(!sc->output_enabled);
            send_cmd(dq, cmd);
        } else if (current_mode == MODE_MULTIMETER) {
            meter_layout = (meter_layout + 1) % METER_LAYOUT_COUNT;
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
