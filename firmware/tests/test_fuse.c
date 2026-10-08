/*
 * Fuse tester model (src/ui/fuse_model.c, src/ui/fuse_table.h).
 *
 * The fuse view printed "0 mA" for a 48 mA parasitic draw for as long as it
 * existed: fuse_ui.c computed mV * 1e3 / uOhm, which is amps, and labelled it
 * mA. fuse_table.h's fuse_estimate_current_mA() had the same slip
 * (uV / uOhm = A). The first test pins the arithmetic against a worked
 * example and keeps the old formula as a negative control that must FAIL it.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "fuse_model.h"

static int failures, checks;

#define CHECK(cond, ...) do {                         \
        checks++;                                     \
        if (!(cond)) {                                \
            failures++;                               \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                      \
            printf("\n");                             \
        }                                             \
    } while (0)

/* The pre-2026-10-06 fuse_ui.c formula. */
static uint32_t old_calc_current_ma(float voltage_mv, uint32_t r_uohm)
{
    float current = (voltage_mv * 1000.0f) / (float)r_uohm;
    if (current < 0.0f) current = -current;
    return (uint32_t)(current + 0.5f);
}

static void test_current_worked_examples(void)
{
    /* 0.379 mV across a 10 A ATO (7.9 mOhm) = 47.97 mA. */
    uint32_t r10 = fuse_lookup_resistance_uohm(FUSE_TYPE_ATO_ATC, 10);
    CHECK(r10 == 7900, "ATO 10 A = %u uOhm", (unsigned)r10);
    CHECK(fuse_current_ma(0.379f, r10) == 48, "0.379 mV / 7.9 mOhm -> %u mA",
          (unsigned)fuse_current_ma(0.379f, r10));
    /* Reversed probes give the same magnitude. */
    CHECK(fuse_current_ma(-0.379f, r10) == 48, "negative drop");
    /* 1 A ATO, 139.5 mOhm: 1.395 mV = 10 mA. */
    CHECK(fuse_current_ma(1.395f, 139500) == 10, "1 A fuse");
    /* A 5 A load through a 15 A ATO (4.9 mOhm) drops 24.5 mV. */
    CHECK(fuse_current_ma(24.5f, 4900) == 5000, "5 A load -> %u",
          (unsigned)fuse_current_ma(24.5f, 4900));
    CHECK(fuse_current_ma(1.0f, 0) == 0, "unknown fuse refuses");

    /* Integer helper in fuse_table.h agrees. */
    CHECK(fuse_estimate_current_mA(FUSE_TYPE_ATO_ATC, 10, 379) == 47,
          "uV helper truncates 47.97 -> %u",
          (unsigned)fuse_estimate_current_mA(FUSE_TYPE_ATO_ATC, 10, 379));
    CHECK(fuse_estimate_current_mA(FUSE_TYPE_ATO_ATC, 99, 379) == 0, "unknown rating");

    /* Negative control: the old formula must fail the same worked example. */
    CHECK(old_calc_current_ma(0.379f, r10) != 48,
          "negative control: old formula should not reach 48 mA");
}

static void test_drop_from_reading(void)
{
    float mv = -1.0f;
    CHECK(fuse_drop_mv_from_reading(0.379f, "mV", &mv) && fabsf(mv - 0.379f) < 1e-6f, "mV");
    CHECK(fuse_drop_mv_from_reading(0.0004f, "V", &mv) && fabsf(mv - 0.4f) < 1e-5f, "V");
    CHECK(!fuse_drop_mv_from_reading(4.7f, "kOhm", &mv), "kOhm refused");
    CHECK(!fuse_drop_mv_from_reading(4.7f, "mA", &mv), "mA refused");
    CHECK(!fuse_drop_mv_from_reading(4.7f, NULL, &mv), "NULL refused");

    CHECK(fuse_mv_decimals("0.379", "mV") == 3, "mV keeps 3");
    CHECK(fuse_mv_decimals("12.4", "mV") == 1, "mV keeps 1");
    CHECK(fuse_mv_decimals("0.0004", "V") == 1, "V 4 dp -> 1 dp in mV");
    CHECK(fuse_mv_decimals("13.82", "V") == 0, "V 2 dp -> 0");
    CHECK(fuse_mv_decimals("---", "mV") == 0, "no number");
}

static void test_labels_and_colours(void)
{
    char b[8];
    fuse_rating_label(7, b, sizeof b);
    CHECK(strcmp(b, "7.5") == 0, "7 is the 7.5 A fuse: %s", b);
    fuse_rating_label(15, b, sizeof b);
    CHECK(strcmp(b, "15") == 0, "15");

    uint32_t rgb = 0;
    CHECK(fuse_rating_rgb(FUSE_TYPE_ATO_ATC, 10, &rgb) && rgb == 0xD7263D, "ATO 10 red");
    CHECK(fuse_rating_rgb(FUSE_TYPE_MINI, 15, &rgb) && rgb == 0x2F6FD6, "Mini 15 blue");
    CHECK(fuse_rating_rgb(FUSE_TYPE_MAXI, 50, &rgb) && rgb == 0xD7263D, "Maxi 50 red");
    CHECK(fuse_rating_rgb(FUSE_TYPE_JCASE, 30, &rgb) && rgb == 0xF06FA6, "J-Case 30 pink");
    /* Ratings with no standard colour in their family must refuse. */
    CHECK(!fuse_rating_rgb(FUSE_TYPE_MAXI, 35, &rgb), "Maxi 35 has no standard colour");
    CHECK(!fuse_rating_rgb(FUSE_TYPE_JCASE, 100, &rgb), "J-Case 100 has no standard colour");

    /* Every blade-family rating in the tables has a colour. */
    for (int t = FUSE_TYPE_ATO_ATC; t <= FUSE_TYPE_MICRO; t++) {
        const fuse_table_t *tb = &fuse_tables[t];
        for (uint8_t i = 0; i < tb->count; i++)
            CHECK(fuse_rating_rgb((fuse_type_t)t, tb->entries[i].rating_amps, &rgb),
                  "type %d %u A has a colour", t, tb->entries[i].rating_amps);
    }
}

static void test_scan_and_presets(void)
{
    uint8_t r[4];
    for (int t = 0; t < FUSE_TYPE_COUNT; t++) {
        uint8_t n = fuse_scan_ratings((fuse_type_t)t, r);
        CHECK(n == 4, "type %d scans 4 ratings (got %u)", t, n);
        for (uint8_t i = 0; i < n; i++)
            CHECK(fuse_lookup_resistance_uohm((fuse_type_t)t, r[i]) != 0,
                  "type %d scan rating %u is in its table", t, r[i]);
    }
    CHECK(fabsf(fuse_scan_threshold_next_preset(0.1f) - 0.2f) < 1e-6f, "0.1 -> 0.2");
    CHECK(fabsf(fuse_scan_threshold_next_preset(0.3f) - 0.5f) < 1e-6f, "0.3 -> 0.5");
    CHECK(fabsf(fuse_scan_threshold_next_preset(2.0f) - 0.1f) < 1e-6f, "2.0 wraps");
}

static void test_shapes(void)
{
    for (int t = 0; t < FUSE_TYPE_COUNT; t++) {
        const fuse_shape_t *s = fuse_shape((fuse_type_t)t);
        CHECK(s->body_h + 1 <= s->h, "type %d body fits", t);
        CHECK(s->win_x + s->win_w <= s->w && s->win_y + s->win_h <= s->body_h,
              "type %d window inside the body", t);
        CHECK(2 * s->blade_x + 2 * s->blade_w <= s->w, "type %d blades don't cross", t);
        CHECK(s->blade_y + s->blade_h <= s->h, "type %d blades fit", t);
        CHECK(2 * s->pad_x + 2 * s->pad_w <= s->w && s->pad_y + s->pad_h <= s->top_h,
              "type %d pads fit the top view", t);
        /* Drawn at 4 px/mm: the outline agrees with the caption to ~3 mm. */
        int dw = (int)s->w / 4 - (int)s->width_mm;
        CHECK(dw >= -3 && dw <= 3, "type %d width %d px vs %u mm", t, s->w, s->width_mm);
        CHECK(s->cartridge == (t == FUSE_TYPE_JCASE), "only J-Case is a cartridge");
    }
}

/* Bench series from unit #1, 2026-10-07 (DC V, mV). */
static const float shorted[] = { -0.9f, -0.9f, -0.9f, -0.9f, -0.8f, -0.9f, -0.9f, -0.9f,
                                 -0.9f, -0.9f, -0.9f, -0.9f, -0.9f, -0.9f, -0.8f, -0.9f };
static const float open_leads[] = { -1.0f, -1.9f, -1.2f, -1.0f, -0.6f, -0.8f, -0.9f, -1.0f,
                                    -0.9f, -0.3f, -0.6f, -0.8f, -0.8f, -1.0f, -1.0f, -0.3f,
                                    -0.4f,  5.1f,  0.0f, -0.9f, -0.8f,  0.1f,  0.1f, -0.7f,
                                    -0.8f, -1.0f, -2.8f, -2.2f, -1.1f,  1.0f, -0.7f, -1.7f,
                                     0.3f,  0.3f, -1.0f, -2.4f, -0.7f, -1.0f, -2.7f, -1.0f };

static void test_settling_and_cal(void)
{
    fuse_input_t s;
    memset(&s, 0, sizeof s);
    uint32_t r10 = fuse_lookup_resistance_uohm(FUSE_TYPE_ATO_ATC, 10);

    /* Negative control: uncalibrated, shorted leads are a phantom draw. */
    CHECK(fuse_current_ma(-0.9f, r10) > FUSE_DRAW_LIMIT_MA, "shorted -0.9 mV reads as a draw");

    for (int k = 0; k < FUSE_STEADY_N - 1; k++) fuse_input_push(&s, shorted[k]);
    CHECK(!fuse_input_steady(&s), "not steady before %d readings", FUSE_STEADY_N);
    CHECK(fuse_input_calibrate(&s) == FUSE_CAL_UNSTEADY && !s.cal_set, "cal refused early");
    fuse_input_push(&s, shorted[FUSE_STEADY_N - 1]);
    CHECK(fuse_input_steady(&s), "shorted leads settle");
    CHECK(fuse_input_calibrate(&s) == FUSE_CAL_OK && s.cal_set, "cal accepted");
    CHECK(fabsf(s.cal_mv + 0.8875f) < 0.001f, "cal = mean -0.8875 (got %f)", (double)s.cal_mv);
    for (size_t k = 0; k < sizeof shorted / sizeof *shorted; k++)
        CHECK(fuse_current_ma(shorted[k] - s.cal_mv, r10) < FUSE_DRAW_LIMIT_MA,
              "calibrated shorted reading %zu is no draw", k);
    /* The worked example still holds on top of the offset. */
    CHECK(fuse_current_ma(0.379f + s.cal_mv - s.cal_mv, r10) == 48, "cal is a pure offset");

    /* Open leads: no full window of the bench series is steady. */
    fuse_input_t o;
    memset(&o, 0, sizeof o);
    int steady_windows = 0, windows = 0;
    for (size_t k = 0; k < sizeof open_leads / sizeof *open_leads; k++) {
        fuse_input_push(&o, open_leads[k]);
        if (o.n == FUSE_STEADY_N) { windows++; steady_windows += fuse_input_steady(&o); }
    }
    CHECK(windows > 20 && steady_windows == 0, "open leads never settle (%d/%d)",
          steady_windows, windows);
    o.cal_set = true; o.cal_mv = -0.5f;
    CHECK(fuse_input_calibrate(&o) == FUSE_CAL_UNSTEADY && fabsf(o.cal_mv + 0.5f) < 1e-6f,
          "unsteady cal leaves the old cal alone");

    /* A loaded fuse wobbling ~4% still settles (5 A on a 15 A ATO, 24.5 mV). */
    fuse_input_t l;
    memset(&l, 0, sizeof l);
    for (int k = 0; k < FUSE_STEADY_N; k++) fuse_input_push(&l, 24.5f + ((k & 1) ? 1.0f : -1.0f));
    CHECK(fuse_input_steady(&l), "loaded fuse with 2 mV wobble is steady");
    CHECK(fuse_input_calibrate(&l) == FUSE_CAL_TOO_LARGE && !l.cal_set,
          "cal on a live fuse is refused");

    /* Clearing drops the history but keeps the cal. */
    fuse_input_clear(&s);
    CHECK(!fuse_input_steady(&s) && s.cal_set, "clear keeps the cal");
}

int main(void)
{
    test_current_worked_examples();
    test_drop_from_reading();
    test_labels_and_colours();
    test_scan_and_presets();
    test_shapes();
    test_settling_and_cal();
    printf("test_fuse: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
