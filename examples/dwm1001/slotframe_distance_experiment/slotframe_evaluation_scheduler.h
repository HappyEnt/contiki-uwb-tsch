#ifndef __SLOTFRAME_EVALUATION_SCHEDULER_H__
#define __SLOTFRAME_EVALUATION_SCHEDULER_H__

void slotframe_evaluation_length_scheduler_init(uint8_t max_distance, uint8_t role);
void set_slotframe_new_distance_callback(void (*callback)(uint8_t slot_distance));
void generate_slotframe_with_distance(uint8_t slotframe_distance);

#endif
