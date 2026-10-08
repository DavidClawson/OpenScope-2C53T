/*
 * Automotive fuse resistance lookup table
 *
 * Used for parasitic drain testing: measure millivolt drop across an
 * in-place fuse, divide by resistance to estimate circuit current.
 *   current_A = voltage_drop_mV / resistance_mOhm
 *
 * Resistance values averaged from multiple sources. Expect ~10% accuracy.
 * Source data: docs/fuse_model.json
 */

#ifndef FUSE_TABLE_H
#define FUSE_TABLE_H

#include <stdint.h>

typedef struct {
    uint8_t  rating_amps;     /* Fuse rating (1-100A) */
    uint32_t resistance_uohm; /* Resistance in micro-ohms (0.001 mOhm = 1 uOhm) */
} fuse_entry_t;

typedef enum {
    FUSE_TYPE_ATO_ATC = 0,  /* Standard blade */
    FUSE_TYPE_MINI,         /* ATM / Mini blade */
    FUSE_TYPE_MICRO,        /* Micro2 (Micro3 is a three-blade part: not supported) */
    FUSE_TYPE_MAXI,         /* APX / Maxi blade */
    FUSE_TYPE_JCASE,        /* JCase cartridge */
    FUSE_TYPE_COUNT
} fuse_type_t;

static const char *const fuse_type_names[FUSE_TYPE_COUNT] = {
    "ATO/ATC",
    "Mini",
    "Micro2",
    "Maxi",
    "J-Case"
};

/* ATO/ATC — Standard blade fuse */
static const fuse_entry_t fuse_ato_atc[] = {
    {  1, 139500 },  /* 139.5 mOhm */
    {  2,  54600 },  /* 54.6 mOhm */
    {  3,  31400 },  /* 31.4 mOhm */
    {  4,  22700 },  /* 22.7 mOhm */
    {  5,  17700 },  /* 17.7 mOhm */
    {  7,  11000 },  /* 11 mOhm (7.5A) */
    { 10,   7900 },  /* 7.9 mOhm */
    { 15,   4900 },  /* 4.9 mOhm */
    { 20,   3500 },  /* 3.5 mOhm */
    { 25,   26U * 100U },  /* 2.6 mOhm */
    { 30,   2100 },  /* 2.1 mOhm */
    { 35,   1700 },  /* 1.7 mOhm */
    { 40,   1500 },  /* 1.5 mOhm */
};

/* Mini — ATM / Mini blade fuse */
static const fuse_entry_t fuse_mini[] = {
    {  1, 121000 },  /* 121 mOhm */
    {  2,  52700 },  /* 52.7 mOhm */
    {  3,  31700 },  /* 31.7 mOhm */
    {  4,  23600 },  /* 23.6 mOhm */
    {  5,  17200 },  /* 17.2 mOhm */
    {  7,  11000 },  /* 11 mOhm (7.5A) */
    { 10,   7600 },  /* 7.6 mOhm */
    { 15,   4800 },  /* 4.8 mOhm */
    { 20,   3300 },  /* 3.3 mOhm */
    { 25,   2500 },  /* 2.5 mOhm */
    { 30,   2000 },  /* 2 mOhm */
};

/* Micro2 blade fuse */
static const fuse_entry_t fuse_micro[] = {
    {  3,  31700 },  /* 31.7 mOhm */
    {  5,  17400 },  /* 17.4 mOhm */
    {  7,  10800 },  /* 10.8 mOhm (7.5A) */
    { 10,   7700 },  /* 7.7 mOhm */
    { 15,   4900 },  /* 4.9 mOhm */
    { 20,   3500 },  /* 3.5 mOhm */
    { 25,   26U * 100U },  /* 2.6 mOhm */
    { 30,   2100 },  /* 2.1 mOhm */
};

/* Maxi — APX / Maxi blade fuse */
static const fuse_entry_t fuse_maxi[] = {
    { 20,   3100 },  /* 3.1 mOhm */
    { 25,   2400 },  /* 2.4 mOhm */
    { 30,   1900 },  /* 1.9 mOhm */
    { 35,   1700 },  /* 1.7 mOhm */
    { 40,   1400 },  /* 1.4 mOhm */
    { 50,   1100 },  /* 1.1 mOhm */
    { 60,    900 },  /* 0.9 mOhm */
    { 70,    600 },  /* 0.6 mOhm */
    { 80,    500 },  /* 0.5 mOhm */
};

/* JCase — JCase cartridge / low-profile cartridge */
static const fuse_entry_t fuse_jcase[] = {
    { 20,   6000 },  /* 6 mOhm */
    { 30,   5200 },  /* 5.2 mOhm */
    { 40,   3800 },  /* 3.8 mOhm */
    { 50,   2400 },  /* 2.4 mOhm */
    { 60,   1700 },  /* 1.7 mOhm */
    { 80,   1200 },  /* 1.2 mOhm */
    {100,    500 },  /* 0.5 mOhm */
};

/* Index table for lookup by fuse_type_t */
typedef struct {
    const fuse_entry_t *entries;
    uint8_t count;
} fuse_table_t;

static const fuse_table_t fuse_tables[FUSE_TYPE_COUNT] = {
    { fuse_ato_atc, sizeof(fuse_ato_atc) / sizeof(fuse_ato_atc[0]) },
    { fuse_mini,    sizeof(fuse_mini)    / sizeof(fuse_mini[0])    },
    { fuse_micro,   sizeof(fuse_micro)   / sizeof(fuse_micro[0])   },
    { fuse_maxi,    sizeof(fuse_maxi)    / sizeof(fuse_maxi[0])    },
    { fuse_jcase,   sizeof(fuse_jcase)   / sizeof(fuse_jcase[0])   },
};

/*
 * Look up fuse resistance in micro-ohms.
 * Returns 0 if the rating is not found for the given type.
 */
static inline uint32_t fuse_lookup_resistance_uohm(fuse_type_t type, uint8_t rating_amps)
{
    if (type >= FUSE_TYPE_COUNT)
        return 0;
    const fuse_table_t *tbl = &fuse_tables[type];
    for (uint8_t i = 0; i < tbl->count; i++) {
        if (tbl->entries[i].rating_amps == rating_amps)
            return tbl->entries[i].resistance_uohm;
    }
    return 0;
}

/*
 * Estimate current in milliamps from voltage drop in microvolts.
 *   current_mA = voltage_drop_uV * 1000 / resistance_uOhm
 * (uV / uOhm is amps; it returned that, unscaled, until 2026-10-06.)
 *
 * Returns 0 if the fuse type/rating is unknown.
 */
static inline uint32_t fuse_estimate_current_mA(fuse_type_t type, uint8_t rating_amps,
                                                  uint32_t voltage_drop_uV)
{
    uint32_t r = fuse_lookup_resistance_uohm(type, rating_amps);
    if (r == 0)
        return 0;
    return (uint32_t)(((uint64_t)voltage_drop_uV * 1000u) / r);
}

#endif /* FUSE_TABLE_H */
