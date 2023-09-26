#include "nodes.h"


#if LINKADDR_CONF_SIZE == 2
const linkaddr_t node_0_ll = { { 0xDE, 0xCA, 0x22, 0xB2, 0x6D, 0xFD, 0x0B, 0xFE } };
const linkaddr_t node_1_ll = { { 0xDE, 0xCA, 0x21, 0x64, 0xB0, 0x6F, 0x4B, 0xFB } };
const linkaddr_t node_2_ll = { { 0xDE, 0xCA, 0xFB, 0x15, 0x64, 0xC0, 0x87, 0xE9 } };
const linkaddr_t node_3_ll = { { 0xDE, 0xCA, 0x94, 0xB7, 0x2A, 0xCA, 0x34, 0xE6 } };
#else
const linkaddr_t node_0_ll = { {  0x0B, 0xFE } };
const linkaddr_t node_1_ll = { {  0x4B, 0xFB } };
const linkaddr_t node_2_ll = { {  0x87, 0xE9 } };
const linkaddr_t node_3_ll = { {  0x34, 0xE6 } };
#endif

struct node_role_entry node_role[] = {
    {&node_0_ll, ROOT},
    {&node_1_ll, ANCHOR},
    {&node_2_ll, ANCHOR},
    {&node_3_ll, ANCHOR}
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

