/*
 * OpenScope 2C53T - when the decoded reading may mute the PC3 continuity tone.
 *
 * The beep follows the meter SoC's tone on PC3 (continuity_buzzer.c). The
 * reading can only mute it: a resistance at or above the user's "Beep below
 * N Ohm" setting, for a part between that setting and the SoC's own (higher)
 * threshold. But the reading lags the probes: contact to the first frame that
 * shows it took 475-1012 ms (EXP-93 Part B), at one frame per ~163 ms (p95
 * 204 ms). A frame that arrives soon after the tone starts can still show
 * what the probes touched before, e.g. the 1 kOhm part they slid off onto a
 * short (review of #102). Frames carry no measurement time, only an arrival.
 *
 * So:
 *   - frames that arrive within CONT_VETO_IGNORE_MS of the tone's onset are
 *     ignored: they may predate it;
 *   - after that, each new frame with a resistance decides: at or above the
 *     threshold mutes, below it unmutes. The veto follows the latest frame
 *     and does not latch, so a 47 Ohm part slid onto a short without the
 *     tone stopping beeps again once the frames show the short;
 *   - a frame without a resistance (OL, blank) changes nothing. Release is
 *     reported fast, unlike contact: on my unit an OL frame arrived ~30 ms
 *     after the tone stopped, inside its 105 ms hold, and when OL unmuted it
 *     a muted 47 Ohm part chirped for 76 ms on release (EXP-93 addendum).
 *
 * Cost: a part between the two thresholds beeps for up to CONT_VETO_IGNORE_MS
 * plus one frame interval before it goes quiet. And a part that reads right
 * at the threshold beeps or not frame by frame, as the screen's SHORT does.
 */
#ifndef CONTINUITY_VETO_H
#define CONTINUITY_VETO_H

#include <stdbool.h>
#include <stdint.h>

/* The longest lag measured (1012 ms) plus about one frame interval. */
#define CONT_VETO_IGNORE_MS 1200U

/* What a frame says against the user's threshold. */
typedef enum {
    CONT_FRAME_NONE = 0,    /* no resistance: OL, blank, invalid */
    CONT_FRAME_BELOW,       /* a resistance below the threshold */
    CONT_FRAME_ABOVE,       /* a resistance at or above it */
} cont_frame_t;

typedef struct {
    bool     muted;
    uint32_t last_upd;      /* update_count of the last frame looked at */
} cont_veto_t;

/* At the tone's onset: not muted; the frame on hand now is already seen. */
static inline void cont_veto_start(cont_veto_t *v, uint32_t upd)
{
    v->muted = false;
    v->last_upd = upd;
}

/* Each sample while the tone sounds. ms: since the onset. upd: the latest
 * frame's update_count. f: what that frame says. Returns whether the beep
 * is muted. */
static inline bool cont_veto_step(cont_veto_t *v, uint32_t ms, uint32_t upd,
                                  cont_frame_t f)
{
    if (upd == v->last_upd)
        return v->muted;            /* no new frame */
    v->last_upd = upd;              /* seen, even if too early to count */
    if (ms >= CONT_VETO_IGNORE_MS && f != CONT_FRAME_NONE)
        v->muted = (f == CONT_FRAME_ABOVE);
    return v->muted;
}

#endif /* CONTINUITY_VETO_H */
