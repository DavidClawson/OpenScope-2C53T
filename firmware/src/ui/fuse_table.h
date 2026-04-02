/*
 * Automotive fuse resistance lookup table
 *
 * Used for parasitic drain testing: measure millivolt drop across an
 * in-place fuse, divide by resistance to estimate circuit current.
 *   current_mA = voltage_drop_mV / resistance_mOhm
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
    FUSE_TYPE_MICRO,        /* Micro2 / Micro3 */
    FUSE_TYPE_MAXI,         /* APX / Maxi blade */
    FUSE_TYPE_JCASE,        /* JCase cartridge */
    FUSE_TYPE_COUNT
} fuse_type_t;

static const char *const fuse_type_names[FUSE_TYPE_COUNT] = {
    "ATO/ATC",
    "Mini",
    "Micro",
    "Maxi",
    "JCase"
};

/* ATO/ATC — Standard blade fuse */
static const fuse_entry_t fuse_ato_atc[] = {
    {  1, 139500 },  /* 0.1395 mOhm */
    {  2,  54600 },  /* 0.0546 mOhm */
    {  3,  31400 },  /* 0.0314 mOhm */
    {  4,  22700 },  /* 0.0227 mOhm */
    {  5,  17700 },  /* 0.0177 mOhm */
    {  7,  11000 },  /* 0.0110 mOhm (7.5A) */
    { 10,   7900 },  /* 0.0079 mOhm */
    { 15,   4900 },  /* 0.0049 mOhm */
    { 20,   3500 },  /* 0.0035 mOhm */
    { 25,   2600 },  /* 0.0026 mOhm */
    { 30,   2100 },  /* 0.0021 mOhm */
    { 35,   1700 },  /* 0.0017 mOhm */
    { 40,   1500 },  /* 0.0015 mOhm */
};

/* Mini — ATM / Mini blade fuse */
static const fuse_entry_t fuse_mini[] = {
    {  1, 121000 },  /* 0.121 mOhm */
    {  2,  52700 },  /* 0.0527 mOhm */
    {  3,  31700 },  /* 0.0317 mOhm */
    {  4,  23600 },  /* 0.0236 mOhm */
    {  5,  17200 },  /* 0.0172 mOhm */
    {  7,  11000 },  /* 0.0110 mOhm (7.5A) */
    { 10,   7600 },  /* 0.0076 mOhm */
    { 15,   4800 },  /* 0.0048 mOhm */
    { 20,   3300 },  /* 0.0033 mOhm */
    { 25,   2500 },  /* 0.0025 mOhm */
    { 30,   2000 },  /* 0.0020 mOhm */
};

/* Micro — Micro2 / Micro3 blade fuse */
static const fuse_entry_t fuse_micro[] = {
    {  3,  31700 },  /* 0.0317 mOhm */
    {  5,  17400 },  /* 0.0174 mOhm */
    {  7,  10800 },  /* 0.0108 mOhm (7.5A) */
    { 10,   7700 },  /* 0.0077 mOhm */
    { 15,   4900 },  /* 0.0049 mOhm */
    { 20,   3500 },  /* 0.0035 mOhm */
    { 25,   2600 },  /* 0.0026 mOhm */
    { 30,   2100 },  /* 0.0021 mOhm */
};

/* Maxi — APX / Maxi blade fuse */
static const fuse_entry_t fuse_maxi[] = {
    { 20,   3100 },  /* 0.0031 mOhm */
    { 25,   2400 },  /* 0.0024 mOhm */
    { 30,   1900 },  /* 0.0019 mOhm */
    { 35,   1700 },  /* 0.0017 mOhm */
    { 40,   1400 },  /* 0.0014 mOhm */
    { 50,   1100 },  /* 0.0011 mOhm */
    { 60,    900 },  /* 0.0009 mOhm */
    { 70,    600 },  /* 0.0006 mOhm */
    { 80,    500 },  /* 0.0005 mOhm */
};

/* JCase — JCase cartridge / low-profile cartridge */
static const fuse_entry_t fuse_jcase[] = {
    { 20,   6000 },  /* 0.006 mOhm */
    { 30,   5200 },  /* 0.0052 mOhm */
    { 40,   3800 },  /* 0.0038 mOhm */
    { 50,   2400 },  /* 0.0024 mOhm */
    { 60,   1700 },  /* 0.0017 mOhm */
    { 80,   1200 },  /* 0.0012 mOhm */
    {100,    500 },  /* 0.0005 mOhm */
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
 *   current_mA = voltage_drop_uV / resistance_uOhm
 *
 * Returns 0 if the fuse type/rating is unknown.
 */
static inline uint32_t fuse_estimate_current_mA(fuse_type_t type, uint8_t rating_amps,
                                                  uint32_t voltage_drop_uV)
{
    uint32_t r = fuse_lookup_resistance_uohm(type, rating_amps);
    if (r == 0)
        return 0;
    return voltage_drop_uV / r;
}

#endif /* FUSE_TABLE_H */
