#ifndef __NODES_H__
#define __NODES_H__

#include "linkaddr.h"

enum node_role { MOBILE = 0, ANCHOR = 1, ROOT = 2 } ;

struct node_role_entry {
    const linkaddr_t *addr;
    enum node_role role;
    uint8_t fixed_timeslot;
    uint8_t send_beacons;
    const linkaddr_t *timesource_neighbor;
};

extern struct node_role_entry node_role[];

struct node_role_entry* get_node_role_entry(const linkaddr_t *addr);

#endif
