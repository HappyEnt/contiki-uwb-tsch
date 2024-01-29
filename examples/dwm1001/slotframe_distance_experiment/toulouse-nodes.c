#include "nodes.h"
#include "toulouse-nodes.h"

// we only use short addresses
 const linkaddr_t dwm1001_26_ll = { { 89, 26 } };
 const linkaddr_t dwm1001_1_ll = { { 82, 1 } };
 const linkaddr_t dwm1001_45_ll = { { 247, 45 } };
 const linkaddr_t dwm1001_21_ll = { { 205, 21 } };
 const linkaddr_t dwm1001_89_ll = { { 242, 89 } };
 const linkaddr_t dwm1001_77_ll = { { 138, 77 } };
 const linkaddr_t dwm1001_85_ll = { { 174, 85 } };
 const linkaddr_t dwm1001_65_ll = { { 253, 65 } };
 const linkaddr_t dwm1001_81_ll = { { 203, 81 } };
 const linkaddr_t dwm1001_5_ll = { { 230, 5 } };
 const linkaddr_t dwm1001_13_ll = { { 238, 13 } };
 const linkaddr_t dwm1001_73_ll = { { 134, 73 } };
 const linkaddr_t dwm1001_94_ll = { { 186, 94 } };
 const linkaddr_t dwm1001_14_ll = { { 28, 14 } };
 const linkaddr_t dwm1001_30_ll = { { 111, 30 } };
 const linkaddr_t dwm1001_70_ll = { { 127, 70 } };
 const linkaddr_t dwm1001_6_ll = { { 234, 6 } };
 const linkaddr_t dwm1001_82_ll = { { 18, 82 } };
 const linkaddr_t dwm1001_2_ll = { { 225, 2 } };
 const linkaddr_t dwm1001_19_ll = { { 106, 19 } };
 const linkaddr_t dwm1001_9_ll = { { 155, 9 } };
 const linkaddr_t dwm1001_29_ll = { { 64, 29 } };
 const linkaddr_t dwm1001_41_ll = { { 36, 41 } };
 const linkaddr_t dwm1001_93_ll = { { 136, 93 } };
 const linkaddr_t dwm1001_17_ll = { { 242, 17 } };
 const linkaddr_t dwm1001_10_ll = { { 196, 10 } };
 const linkaddr_t dwm1001_43_ll = { { 119, 43 } };
 const linkaddr_t dwm1001_18_ll = { { 78, 18 } };
 const linkaddr_t dwm1001_25_ll = { { 44, 25 } };
 const linkaddr_t dwm1001_38_ll = { { 48, 38 } };
 const linkaddr_t dwm1001_69_ll = { { 188, 69 } };
 const linkaddr_t dwm1001_39_ll = { { 247, 39 } };
 const linkaddr_t dwm1001_37_ll = { { 245, 37 } };
 const linkaddr_t dwm1001_63_ll = { { 234, 63 } };
 const linkaddr_t dwm1001_62_ll = { { 234, 62 } };
 const linkaddr_t dwm1001_34_ll = { { 114, 34 } };
 const linkaddr_t dwm1001_95_ll = { { 130, 95 } };
 const linkaddr_t dwm1001_33_ll = { { 66, 33 } };
 const linkaddr_t dwm1001_35_ll = { { 101, 35 } };
 const linkaddr_t dwm1001_61_ll = { { 245, 61 } };
 const linkaddr_t dwm1001_74_ll = { { 207, 74 } };
 const linkaddr_t dwm1001_86_ll = { { 247, 86 } };
 const linkaddr_t dwm1001_42_ll = { { 212, 42 } };

static struct node_role_entry mobile_entry = { NULL, MOBILE, 0 };

struct node_role_entry node_role[] = {
    { &dwm1001_70_ll , ANCHOR, 0, 1 }, 
    { &dwm1001_77_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_45_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_5_ll , ANCHOR,  4, 1 }, 
    { &dwm1001_14_ll , ROOT,   2, 1 }, 
    { &dwm1001_30_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_13_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_69_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_39_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_81_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_10_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_61_ll , ANCHOR, 0, 1 }, 
    { &dwm1001_1_ll , ANCHOR,  0, 0 }, 
    { &dwm1001_38_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_19_ll , ANCHOR, 0, 1 }, 
    { &dwm1001_41_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_37_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_86_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_33_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_17_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_6_ll , ANCHOR,  0, 0 }, 
    { &dwm1001_9_ll , ANCHOR,  0, 0 }, 
    { &dwm1001_43_ll , ANCHOR, 3, 1 }, 
    { &dwm1001_94_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_65_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_93_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_85_ll , ANCHOR, 0, 1 }, 
    { &dwm1001_73_ll , ANCHOR, 0, 0 },
    { &dwm1001_82_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_42_ll , ANCHOR, 0, 1 }, 
    { &dwm1001_63_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_89_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_29_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_2_ll , ANCHOR,  0, 0 }, 
    { &dwm1001_21_ll , ANCHOR, 0, 1 }, 
    { &dwm1001_95_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_62_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_26_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_74_ll , ANCHOR, 0, 1 }, 
    { &dwm1001_35_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_34_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_25_ll , ANCHOR, 0, 0 }, 
    { &dwm1001_18_ll , ANCHOR, 0, 0 }
};

struct node_role_entry* get_node_role_entry(const linkaddr_t *addr) {
    // print address
    printf("address: ");
    for (int i = 0; i < sizeof(addr->u8) / sizeof(uint8_t); i++) {
        printf("%02x", addr->u8[i]);
    }
    printf("\n");
    
    int i;
    for (i = 0; i < sizeof(node_role) / sizeof(struct node_role_entry); i++) {
        if (linkaddr_cmp(addr, node_role[i].addr)) {
            return &node_role[i];
        }
    }
    return &mobile_entry;
}
