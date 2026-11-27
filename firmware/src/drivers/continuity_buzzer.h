/*
 * OpenScope 2C53T - continuity buzzer drive.
 *
 * The buzzer is on PB9, bit-banged here (stock drives it from TMR11_CH1, #94).
 * The beep follows the meter SoC's BUZ tone on PC3, counted by EXINT3 as stock
 * does, so it starts within a few ms of contact; the decoded reading can mute
 * it above the user's threshold (continuity_veto.h) and is the fallback when
 * no tone has been seen. See continuity_buzzer.c and EXP-93.
 */
#ifndef CONTINUITY_BUZZER_H
#define CONTINUITY_BUZZER_H

#include <stdbool.h>
#include <stdint.h>

void continuity_buzzer_init(void);
void continuity_buzzer_create_task(void);
void continuity_buzzer_force_ms(uint32_t duration_ms);
void continuity_buzzer_snapshot(bool *task_started, bool *active,
                                uint32_t *toggle_count,
                                uint32_t *create_fail_count);

#endif /* CONTINUITY_BUZZER_H */
