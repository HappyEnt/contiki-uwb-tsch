#ifndef DEMO_SCHEDULE_H_
#define DEMO_SCHEDULE_H_

#include "schedule.h"
#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/tsch-schedule.h"

extern struct schedule_link eb_shared_schedule[4];
/* extern struct schedule_link *eb_prop_schedule; */
void init_custom_schedules(const linkaddr_t *addr);

#endif /* DEMO_SCHEDULE_H_ */
