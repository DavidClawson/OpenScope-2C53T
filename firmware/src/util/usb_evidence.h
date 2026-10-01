/*
 * usb_evidence.h — CDC wedge evidence that survives a reset (issue #39).
 *
 * WHY
 * ---------------------------------------------------------------------------
 * #39: the CDC shell goes silent (UI alive) and only replug + reset brings it
 * back. PR #41 counts stalls and snapshots the IN endpoint at the last one,
 * but in plain RAM: the reset that recovers the port erased the evidence
 * before anyone could type `usbstat`. This record lives in .noinit, which
 * startup never zeroes, so the next session can say what the previous one was
 * doing when it was reset. Assumed, not yet shown on hardware: the factory IAP
 * that runs on every reset leaves the top of SRAM alone (its initial SP, from
 * the vector table of archive/factory_iap_bootloader_2C53T.bin, is 0x20001A08).
 *
 * This file is the HARDWARE-FREE half: layout, checksum, validation and the
 * per-event updates, host-tested in tests/test_usb_evidence.c. The .noinit
 * instance, the endpoint read and the printing are in drivers/usb_debug.c.
 *
 * TRUST RULE
 * ---------------------------------------------------------------------------
 * After a cold power-up SRAM holds noise. A record is believed only when the
 * magic AND the checksum both match; anything else reads as "cold power-up,
 * no previous session". A reset that lands inside an update (a few hundred
 * cycles) breaks the checksum: that session's evidence is lost, never
 * misreported.
 *
 * SINGLE WRITER
 * ---------------------------------------------------------------------------
 * Every update runs on the usb_dbg task (CDC writes, the remote protocol's
 * block writer and the loop tick all do). The USB ISR does not touch this
 * record: DTR is read from s_usb at stall time.
 */

#ifndef USB_EVIDENCE_H
#define USB_EVIDENCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define USB_EV_MAGIC 0x55534256u            /* "VBSU" little-endian: USB eVidence */

#define USB_EV_FLAG_PENDING   0x01u  /* a stall happened and no send has completed since */
#define USB_EV_FLAG_STALL_DTR 0x02u  /* host DTR was set at the last stall */
#define USB_EV_FLAG_STALL_TXC 0x04u  /* g_tx_completed was nonzero at the last stall */

/* 52 bytes, no padding (asserted in usb_evidence.c). Field by field, why: */
typedef struct {
    uint32_t magic;            /* USB_EV_MAGIC: written by this firmware, not power-up noise */
    uint16_t seq;              /* session number since cold power-up (1 = first boot after
                                * power-on): tells a reset from a power cycle */
    uint8_t  flags;            /* USB_EV_FLAG_*: PENDING is the wedge signature; DTR/TXC are
                                * the stall snapshot's two booleans */
    uint8_t  reserved;         /* 0; keeps the layout explicit */
    uint32_t stall_tick;       /* ms tick of the last stall: when the IN path stopped */
    uint32_t stall_ept;        /* USB->ept[1] at the last stall: TX NAK/DIS = lost completion,
                                * TX VALID = host not reading */
    uint32_t alive_tick;       /* ms tick of the last usb_dbg loop pass: past stall_tick = the
                                * task kept looping after the stall (wedged port, live task) */
    uint32_t tx_stalls;        /* waits for g_tx_completed that timed out */
    uint32_t tx_host_slow;     /* of those, with the packet still VALID (host not reading) */
    uint32_t rx_frames_ok;     /* esp_comm RX stats, copied once per loop: was remote protocol
                                * traffic flowing (or failing) when the port went silent? */
    uint16_t heals;            /* soft reconnects performed by the #39 self-heal */
    uint16_t rx_bad_checksum;  /* RX error counters; they saturate at 0xFFFF here (usbstat's */
    uint16_t rx_bad_length;    /*   live line still prints esp_comm's 32-bit originals) */
    uint16_t rx_gap_timeouts;
    uint32_t heal_stall_tick;  /* the stall that fired the self-heal (its tick) and its
                                * endpoint register: kept apart from stall_tick/stall_ept
                                * because the host_slow stalls that follow a heal (the
                                * host has usually abandoned the port by then) overwrite
                                * the last-stall snapshot. EXP-66 lost the one #39 wedge
                                * ever caught that way. 0 until a heal happens. */
    uint32_t heal_stall_ept;
    uint32_t check;            /* usb_ev_checksum() of every byte above; MUST stay last */
} usb_evidence_t;

/* What the record left by the previous session says. */
typedef enum {
    USB_EV_PREV_NONE = 0,      /* invalid: cold power-up (or a torn write) */
    USB_EV_PREV_CLEAN,         /* valid, every stall was followed by a completed send */
    USB_EV_PREV_PENDING,       /* valid, last stall NOT followed by a completed send */
} usb_ev_prev_t;

/* FNV-1a (32-bit) over bytes [0, offsetof(check)). Each step is a bijection
 * of the running hash, so any single changed byte changes the result. */
uint32_t usb_ev_checksum(const usb_evidence_t *ev);
void     usb_ev_seal(usb_evidence_t *ev);
bool     usb_ev_valid(const usb_evidence_t *ev);

/* Boot: classify what is in *ev, copy it to *prev (zeroed when invalid), then
 * clear *ev for this session (seq = prev.seq + 1, or 1 after a cold power-up)
 * and seal it. */
usb_ev_prev_t usb_ev_begin_session(usb_evidence_t *ev, usb_evidence_t *prev);

/* A wait for g_tx_completed timed out: snapshot it, count it, mark PENDING. */
void usb_ev_stall(usb_evidence_t *ev, uint32_t tick, uint32_t ept,
                  bool tx_completed, bool dtr, bool host_slow);

/* A wait for g_tx_completed succeeded (the IN path works): clear PENDING.
 * Called per 64-byte chunk, so it only reseals when PENDING was set. */
void usb_ev_send_completed(usb_evidence_t *ev);

/* A soft reconnect was performed: count it and copy the stall that fired it
 * (the current stall snapshot) into heal_stall_tick/ept, where later stalls
 * cannot overwrite it. */
void usb_ev_heal(usb_evidence_t *ev);

/* Once per usb_dbg loop: the alive tick and a copy of the esp_comm RX stats.
 * Reseals only when something changed. */
void usb_ev_alive(usb_evidence_t *ev, uint32_t tick,
                  uint32_t rx_ok, uint32_t rx_bad_checksum,
                  uint32_t rx_bad_length, uint32_t rx_gap_timeouts);

#endif /* USB_EVIDENCE_H */
