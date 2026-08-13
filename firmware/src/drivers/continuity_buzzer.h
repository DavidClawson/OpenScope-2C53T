/*
 * OpenScope 2C53T - experimental continuity buzzer drive.
 *
 * The stock firmware audibly beeps in continuity mode, but this clean-room
 * tree does not yet have a confirmed buzzer pin/timer map. PB9 is stock
 * AF-push-pull and is one of the unresolved auxiliary pins, so this driver
 * tries it as a bounded bench probe: output toggles only in continuity mode
 * while the parser reports an actual short.
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
