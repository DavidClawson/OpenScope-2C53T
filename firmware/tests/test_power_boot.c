/*
 * Host test for power_boot_classify() (drivers/power.c): which boots land on
 * the charge screen and which boot the instrument.
 *
 * The scenarios are the real ways this unit comes up. The flag sets are what
 * the AT32 accumulates in CRM_CTRLSTS: POR also sets NRST, a software reset
 * also sets NRST, and nothing before our app clears them (the factory IAP
 * never writes RSTFC). After an IAP flash the factory IAP does not reset, it
 * jumps to the app with PC9 still latched from upgrade mode, so that boot
 * carries the flags of whatever reset entered upgrade mode plus `latched`.
 *
 * It proves the decision table, not the wiring: that PC8 really reads LOW
 * when held, that PC9 LOW releases the battery path, and what the factory
 * IAP does to PC9 and the flags are bench and disassembly claims (power.h).
 *
 * Negative controls: four plausible wrong classifiers must each fail at
 * least one scenario, or the scenarios are not discriminating.
 *
 * Build/run:  make -C firmware test-power-boot
 */

#include <stdbool.h>
#include <stdio.h>

#ifndef POWER_HOST_TEST
#define POWER_HOST_TEST 1
#endif
#include "power.h"

#define POR   POWER_RSTF_POR
#define NRST  POWER_RSTF_NRST
#define SW    POWER_RSTF_SW
#define WDT   POWER_RSTF_WDT

typedef struct {
    const char  *what;
    uint32_t     flags;
    bool         held;
    bool         req;
    bool         latched;
    power_boot_t want;
} scenario_t;

#define RUN    POWER_BOOT_RUN
#define CHARGE POWER_BOOT_CHARGE

static const scenario_t scenarios[] = {
    /*  what                                     flags              held   req    latched want */
    { "POWER pressed on battery",               POR | NRST,        true,  false, false, RUN    },
    { "USB plugged into an off unit",           POR | NRST,        false, false, false, CHARGE },
    { "USB plugged in while holding POWER",     POR | NRST,        true,  false, false, RUN    },
    { "our power-off, POWER still held",        NRST | SW,         true,  true,  false, CHARGE },
    { "our power-off, POWER released",          NRST | SW,         false, true,  false, CHARGE },
    { "pinhole reset on USB",                   NRST,              false, false, false, RUN    },
    { "IAP flash, entered from running app",    NRST | SW,         false, false, true,  RUN    },
    { "IAP flash, cold MENU+POWER entry",       POR | NRST,        false, false, true,  RUN    },
    { "IAP flash, MENU + pinhole entry",        NRST,              false, false, true,  RUN    },
    { "CDC fwapply system reset",               NRST | SW,         false, false, false, RUN    },
    { "EOPB0 programming reset, cold unit",     POR | NRST | SW,   false, false, false, RUN    },
    { "watchdog reset on USB",                  NRST | WDT,        false, false, false, RUN    },
    { "watchdog reset straight after power-on", POR | NRST | WDT,  false, false, false, RUN    },
    { "no flags at all (cleared, jumped into)", 0,                 false, false, false, RUN    },
    /* HID-bootloader builds: latched is always false there (power.c), since
     * that bootloader latches PC9 on every boot. Its post-flash handoff is a
     * direct jump, so the flags are those of the entry. From the running app
     * there is no POR and it boots; a cold POWER+PRM entry cannot be told
     * from USB into an off unit and lands on the charge screen (known, not
     * unsafe: POWER hold boots it). */
    { "HID flash, Settings > Firmware Update",  NRST | SW,         false, false, false, RUN    },
    { "HID flash, cold POWER+PRM entry",        POR | NRST,        false, false, false, CHARGE },
    /* Power-off on battery, pressed again so soon that SRAM kept the request
     * through the outage. POR says the rail went down: the request is stale. */
    { "battery re-press, SRAM kept request",    POR | NRST,        true,  true,  false, RUN    },
    { "USB plugged in, SRAM kept request",      POR | NRST,        false, true,  false, CHARGE },
};
#define N_SCEN (int)(sizeof(scenarios) / sizeof(scenarios[0]))

typedef power_boot_t (*classifier_t)(uint32_t, bool, bool, bool);

/* The firmware before this change: always latch and boot. */
static power_boot_t old_always_run(uint32_t f, bool held, bool req, bool latched)
{
    (void)f; (void)held; (void)req; (void)latched;
    return POWER_BOOT_RUN;
}

/* Stock-shaped but naive: any POR without POWER is a charge boot, even when
 * a software or watchdog reset followed it. */
static power_boot_t naive_por(uint32_t f, bool held, bool req, bool latched)
{
    (void)latched;
    if (req) return POWER_BOOT_CHARGE;
    if (held) return POWER_BOOT_RUN;
    return (f & POWER_RSTF_POR) ? POWER_BOOT_CHARGE : POWER_BOOT_RUN;
}

/* Flags only, ignoring the loader latch: assumes the factory IAP resets
 * after a flash. It jumps, so a cold-entry flash would land on the charge
 * screen instead of booting the new image. */
static power_boot_t flags_only(uint32_t f, bool held, bool req, bool latched)
{
    (void)latched;
    return power_boot_classify(f, held, req, false);
}

/* Trusts the power-off request whatever the flags say: a request that
 * survived a battery power cycle in SRAM would keep the unit from turning
 * on with one press. */
static power_boot_t trusts_req(uint32_t f, bool held, bool req, bool latched)
{
    if (req) return POWER_BOOT_CHARGE;
    return power_boot_classify(f, held, false, latched);
}

static int run(const char *name, classifier_t c, bool verbose)
{
    int bad = 0;
    for (int i = 0; i < N_SCEN; i++) {
        const scenario_t *s = &scenarios[i];
        power_boot_t got = c(s->flags, s->held, s->req, s->latched);
        if (got != s->want) {
            bad++;
            if (verbose)
                printf("  FAIL %-40s want %s got %s\n", s->what,
                       s->want == POWER_BOOT_RUN ? "RUN" : "CHARGE",
                       got == POWER_BOOT_RUN ? "RUN" : "CHARGE");
        }
    }
    printf("%-24s %d/%d scenarios as expected\n", name, N_SCEN - bad, N_SCEN);
    return bad;
}

int main(void)
{
    int fails = 0;

    if (run("power_boot_classify", power_boot_classify, true) != 0)
        fails++;

    /* Negative controls: each must get something wrong. */
    if (run("control: old always-run", old_always_run, false) == 0) {
        printf("  FAIL control passed: scenarios do not catch the USB auto-boot\n");
        fails++;
    }
    if (run("control: naive POR", naive_por, false) == 0) {
        printf("  FAIL control passed: scenarios do not catch POR followed by a reset\n");
        fails++;
    }
    if (run("control: flags only", flags_only, false) == 0) {
        printf("  FAIL control passed: scenarios do not catch the cold IAP flash jump\n");
        fails++;
    }
    if (run("control: trusts request", trusts_req, false) == 0) {
        printf("  FAIL control passed: scenarios do not catch a stale power-off request\n");
        fails++;
    }

    printf(fails ? "FAILED\n" : "OK\n");
    return fails ? 1 : 0;
}
