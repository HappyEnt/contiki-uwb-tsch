#ifndef __NODES_H__
#define __NODES_H__

#include "linkaddr.h"

enum node_role { MOBILE = 0, ANCHOR = 1, ROOT = 2 } ;

struct node_role_entry {
    const linkaddr_t *addr;
    enum node_role role; 
};

extern struct node_role_entry node_role[];

enum node_role get_role_for_node(const linkaddr_t *addr);

#endif
