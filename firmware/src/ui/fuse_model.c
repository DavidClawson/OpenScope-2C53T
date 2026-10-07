/*
 * Fuse tester model -- see fuse_model.h.
 */
#include "fuse_model.h"
#include <stdio.h>
#include <string.h>

uint32_t fuse_current_ma(float drop_mv, uint32_t resistance_uohm)
{
    if (resistance_uohm == 0) return 0;
    if (drop_mv < 0.0f) drop_mv = -drop_mv;
    /* mV / mOhm = A, so mV * 1e6 / uOhm = mA. */
    float ma = drop_mv * 1.0e6f / (float)resistance_uohm;
    if (ma > 2.0e9f) return 2000000000u;
    return (uint32_t)(ma + 0.5f);
}

bool fuse_drop_mv_from_reading(float value, const char *unit, float *mv_out)
{
    if (!unit) return false;
    if (strcmp(unit, "mV") == 0) { *mv_out = value;           return true; }
    if (strcmp(unit, "V")  == 0) { *mv_out = value * 1000.0f; return true; }
    return false;
}

int fuse_mv_decimals(const char *display_str, const char *unit)
{
    int dec = 0;
    const char *d = display_str ? strchr(display_str, '.') : NULL;
    if (d)
        for (d++; *d >= '0' && *d <= '9'; d++) dec++;
    if (unit && strcmp(unit, "V") == 0) dec -= 3;
    if (dec < 0) dec = 0;
    if (dec > 3) dec = 3;
    return dec;
}

void fuse_rating_label(uint8_t rating_amps, char *buf, uint8_t n)
{
    if (rating_amps == 7) snprintf(buf, n, "7.5");
    else                  snprintf(buf, n, "%u", (unsigned)rating_amps);
}

typedef struct { uint8_t amps; uint32_t rgb; } fuse_colour_t;

/* ATO/ATC, Mini and Micro2 share one code. */
static const fuse_colour_t blade_colours[] = {
    {  1, 0x1C1C1C }, {  2, 0x8A8F96 }, {  3, 0x7E4FC4 }, {  4, 0xF49CC0 },
    {  5, 0xC8A57A }, {  7, 0x7A4A28 }, { 10, 0xD7263D }, { 15, 0x2F6FD6 },
    { 20, 0xF2D02A }, { 25, 0xE8E2D2 }, { 30, 0x2E9E4F }, { 35, 0x1F8C8C },
    { 40, 0xF28C28 },
};
static const fuse_colour_t maxi_colours[] = {
    { 20, 0xF2D02A }, { 30, 0x2E9E4F }, { 40, 0xF28C28 }, { 50, 0xD7263D },
    { 60, 0x2F6FD6 }, { 70, 0x7A4A28 }, { 80, 0xE8E2D2 }, {100, 0x7E4FC4 },
};
static const fuse_colour_t jcase_colours[] = {
    { 20, 0x6FB7E8 }, { 25, 0xE8E2D2 }, { 30, 0xF06FA6 }, { 40, 0x2E9E4F },
    { 50, 0xD7263D }, { 60, 0xF2D02A },
};

bool fuse_rating_rgb(fuse_type_t type, uint8_t rating_amps, uint32_t *rgb)
{
    const fuse_colour_t *t;
    size_t n;
    switch (type) {
    case FUSE_TYPE_ATO_ATC:
    case FUSE_TYPE_MINI:
    case FUSE_TYPE_MICRO: t = blade_colours; n = sizeof blade_colours / sizeof *t; break;
    case FUSE_TYPE_MAXI:  t = maxi_colours;  n = sizeof maxi_colours  / sizeof *t; break;
    case FUSE_TYPE_JCASE: t = jcase_colours; n = sizeof jcase_colours / sizeof *t; break;
    default: return false;
    }
    for (size_t i = 0; i < n; i++)
        if (t[i].amps == rating_amps) { *rgb = t[i].rgb; return true; }
    return false;
}

uint32_t fuse_type_rgb(fuse_type_t type)
{
    static const uint32_t rep[FUSE_TYPE_COUNT] = {
        0xD7263D, 0x2F6FD6, 0xC8A57A, 0xF28C28, 0xF06FA6,
    };
    return type < FUSE_TYPE_COUNT ? rep[type] : 0x8A8F96;
}

uint8_t fuse_scan_ratings(fuse_type_t type, uint8_t out[4])
{
    static const uint8_t blade[4] = { 10, 15, 20, 30 };
    static const uint8_t big[4]   = { 20, 30, 40, 60 };
    const uint8_t *src = (type == FUSE_TYPE_MAXI || type == FUSE_TYPE_JCASE) ? big : blade;
    uint8_t k = 0;
    for (uint8_t i = 0; i < 4; i++)
        if (fuse_lookup_resistance_uohm(type, src[i]) != 0) out[k++] = src[i];
    return k;
}

float fuse_scan_threshold_next_preset(float cur_mv)
{
    static const float p[] = { 0.1f, 0.2f, 0.5f, 1.0f, 2.0f };
    for (size_t i = 0; i < sizeof p / sizeof p[0]; i++)
        if (p[i] > cur_mv + 0.01f) return p[i];
    return p[0];
}

/* 4 px per mm. Blade/pad x is the LEFT one; the right one is mirrored. */
static const fuse_shape_t shapes[FUSE_TYPE_COUNT] = {
    /* w   h  body  bx  by  bw  bh   wx  wy  ww  wh  top  px  py  pw  ph  mm      cart */
    {  76, 74, 44,   9, 42, 16, 31,  10, 12, 56, 22,  22, 11,  6, 12, 10, 19, 19, false }, /* ATO   */
    {  44, 65, 38,   5, 36,  9, 28,   6,  9, 32, 20,  16,  6,  4,  8,  8, 11, 16, false }, /* Mini  */
    {  36, 61, 34,   6, 33,  7, 27,   5,  8, 26, 18,  16,  6,  4,  7,  8,  9, 15, false }, /* Micro2 */
    { 117,137, 96,  16, 94, 26, 42,  14, 18, 89, 50,  34, 20,  9, 18, 16, 29, 34, false }, /* Maxi  */
    {  90, 52, 50,  14, 36, 16, 12,  10,  6, 70, 22,  38, 16, 13, 12, 12, 22, 13, true  }, /* J-Case */
};

const fuse_shape_t *fuse_shape(fuse_type_t type)
{
    return &shapes[type < FUSE_TYPE_COUNT ? type : 0];
}
