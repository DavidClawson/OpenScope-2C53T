/*
 * usb_evidence.c — CDC wedge evidence record (issue #39). See usb_evidence.h.
 *
 * Pure C: no HAL, no FreeRTOS, no section attributes (the .noinit instance is
 * declared by its user, drivers/usb_debug.c), so it builds and is tested on
 * the host by tests/test_usb_evidence.c.
 */

#include "usb_evidence.h"

#include <string.h>

/* The record sits in .noinit on an SRAM that is ~100 B from the
 * link limit, so it must stay small; and `check` must cover every other byte
 * (no padding where power-up noise could sit outside the checksum). */
_Static_assert(sizeof(usb_evidence_t) == 44u, "usb_evidence_t layout changed");
_Static_assert(sizeof(usb_evidence_t) <= 64u, "usb_evidence_t must stay tiny (.noinit)");
_Static_assert(offsetof(usb_evidence_t, check) + sizeof(uint32_t) == sizeof(usb_evidence_t),
               "check must be the last field, with no padding after it");

#define FNV_OFFSET 0x811C9DC5u
#define FNV_PRIME  0x01000193u

uint32_t usb_ev_checksum(const usb_evidence_t *ev)
{
    const uint8_t *p = (const uint8_t *)ev;
    uint32_t h = FNV_OFFSET;
    for (size_t i = 0; i < offsetof(usb_evidence_t, check); i++) {
        h ^= p[i];
        h *= FNV_PRIME;
    }
    return h;
}

void usb_ev_seal(usb_evidence_t *ev)
{
    ev->check = usb_ev_checksum(ev);
}

bool usb_ev_valid(const usb_evidence_t *ev)
{
    if (ev->magic != USB_EV_MAGIC) {
        return false;
    }
    if (usb_ev_checksum(ev) != ev->check) {
        return false;
    }
    return true;
}

usb_ev_prev_t usb_ev_begin_session(usb_evidence_t *ev, usb_evidence_t *prev)
{
    usb_ev_prev_t kind = USB_EV_PREV_NONE;
    uint16_t seq = 1u;

    if (usb_ev_valid(ev)) {
        *prev = *ev;
        kind = (prev->flags & USB_EV_FLAG_PENDING) ? USB_EV_PREV_PENDING
                                                   : USB_EV_PREV_CLEAN;
        seq = (uint16_t)(prev->seq + 1u);
        if (seq == 0u) {
            seq = 1u;           /* 0 never names a session */
        }
    } else {
        memset(prev, 0, sizeof(*prev));
    }

    memset(ev, 0, sizeof(*ev));
    ev->magic = USB_EV_MAGIC;
    ev->seq = seq;
    usb_ev_seal(ev);
    return kind;
}

void usb_ev_stall(usb_evidence_t *ev, uint32_t tick, uint32_t ept,
                  bool tx_completed, bool dtr, bool host_slow)
{
    uint8_t f = USB_EV_FLAG_PENDING;
    if (dtr)          f |= USB_EV_FLAG_STALL_DTR;
    if (tx_completed) f |= USB_EV_FLAG_STALL_TXC;
    ev->flags = f;
    ev->stall_tick = tick;
    ev->stall_ept = ept;
    ev->tx_stalls++;
    if (host_slow) {
        ev->tx_host_slow++;
    }
    usb_ev_seal(ev);
}

void usb_ev_send_completed(usb_evidence_t *ev)
{
    if (ev->flags & USB_EV_FLAG_PENDING) {
        ev->flags &= (uint8_t)~USB_EV_FLAG_PENDING;
        usb_ev_seal(ev);
    }
}

void usb_ev_heal(usb_evidence_t *ev)
{
    if (ev->heals < 0xFFFFu) {
        ev->heals++;
    }
    usb_ev_seal(ev);
}

static uint16_t sat16(uint32_t v)
{
    return v > 0xFFFFu ? (uint16_t)0xFFFFu : (uint16_t)v;
}

void usb_ev_alive(usb_evidence_t *ev, uint32_t tick,
                  uint32_t rx_ok, uint32_t rx_bad_checksum,
                  uint32_t rx_bad_length, uint32_t rx_gap_timeouts)
{
    uint16_t chk = sat16(rx_bad_checksum);
    uint16_t len = sat16(rx_bad_length);
    uint16_t gap = sat16(rx_gap_timeouts);

    if (ev->alive_tick == tick && ev->rx_frames_ok == rx_ok &&
        ev->rx_bad_checksum == chk && ev->rx_bad_length == len &&
        ev->rx_gap_timeouts == gap) {
        return;
    }
    ev->alive_tick = tick;
    ev->rx_frames_ok = rx_ok;
    ev->rx_bad_checksum = chk;
    ev->rx_bad_length = len;
    ev->rx_gap_timeouts = gap;
    usb_ev_seal(ev);
}
