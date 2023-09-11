#include "nodes.h"

// we only use short addresses
const linkaddr_t node_0_ll = { {  21, 215  } };
const linkaddr_t node_1_ll = { {  23, 206  } };
const linkaddr_t node_2_ll = { {  108, 22  } };
const linkaddr_t node_3_ll = { {  195, 49  } };
const linkaddr_t node_4_ll = { {  50, 78   } };
const linkaddr_t node_5_ll = { {  219, 229 } };
const linkaddr_t node_6_ll = { {  61, 196 } };
const linkaddr_t node_7_ll = { {  106, 6 } };
const linkaddr_t node_8_ll = { {  8, 125 } };
const linkaddr_t node_9_ll = { {  112, 158 } };
const linkaddr_t node_10_ll = { {  226, 113 } };
const linkaddr_t node_11_ll = { {  159, 95 } };
const linkaddr_t node_12_ll = { {  177, 193 } };
const linkaddr_t node_13_ll = { {  230, 218 } };

struct node_role_entry node_role[] = {
    {&node_0_ll, ROOT},
    {&node_1_ll, ANCHOR},
    {&node_2_ll, ANCHOR},
    {&node_3_ll, ANCHOR},
    {&node_4_ll, ANCHOR},
    {&node_5_ll, ANCHOR},
    {&node_6_ll, ANCHOR},
    {&node_7_ll, ANCHOR},
    {&node_8_ll, ANCHOR},
    {&node_9_ll, ANCHOR},
    {&node_10_ll, ANCHOR},
    {&node_11_ll, ANCHOR},
    {&node_12_ll, ANCHOR},
    {&node_13_ll, ANCHOR}
};

enum node_role get_role_for_node(const linkaddr_t *addr) {
    // print address
    printf("address: ");
    for (int i = 0; i < sizeof(addr->u8) / sizeof(uint8_t); i++) {
        printf("%02x", addr->u8[i]);
    }
    printf("\n");
    
    int i;
    for (i = 0; i < sizeof(node_role) / sizeof(struct node_role_entry); i++) {
        if (linkaddr_cmp(addr, node_role[i].addr)) {
            return node_role[i].role;
        }
    }
    return MOBILE;
}

