/*
 * scope_record.h — what every consumer of an acquisition record must know.
 *
 * One number lives here so that the trace, the FFT, the waterfall and the
 * split view cannot disagree about it.
 *
 * SCOPE_RECORD_HEAD_SKIP — the first samples of every 1024-sample readout
 * are INVALID (EXP-22 found the seam at indices ~32..96; EXP-42/47 closed
 * the seam's root cause and left this as a separate, bounded defect: the
 * first 32-64 samples of every record are zeros or a dropout in every
 * trigger mode, not a rotation). The body, samples [HEAD_SKIP, 1024), is
 * static once the read is edge-gated (EXP-42: 0/10 gross seams).
 *
 * The trace's soft trigger starts its search here (it used to be a local
 * `SEAM_GUARD` in scope_ui.c); the spectrum views start their window here.
 * A consumer that reads the record from index 0 is analysing the defect.
 */
#ifndef SCOPE_RECORD_H
#define SCOPE_RECORD_H

#define SCOPE_RECORD_HEAD_SKIP  128u

#endif /* SCOPE_RECORD_H */
