#include "mtm_control.h"

void take_timeslot(uint8_t timeslot) {
    // find slotframe
    struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);

    printf("Try to take timeslot %d\n", timeslot);

    if(sf_eb != NULL) {
        printf("Setting link to tx\n");
        struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_eb, timeslot);
        if(l == NULL) {
            printf("Could not find link\n");
        }
        l->link_options = LINK_OPTION_TX;
    }
}
