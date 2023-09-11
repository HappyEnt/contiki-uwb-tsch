/*
 * Copyright (c) 2015, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/**
 * \file
 *         DRAND: drand protocol for automatic conflict free TDMA scheduling build on top of RIME stack
 *
 * \author El Pato
 */

#include "apps/mqtt/mqtt.h"
#include "contiki.h"
#include "drand.h"
#include "linkaddr.h"
#include "net/packetbuf.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/rpl/rpl-private.h"
#include "net/rime/rime.h"
#include "tsch-private.h"

#include "net/rime/neighbor-discovery.h"
#include "net/rime/unicast.h"
#include <stdint.h>

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

/* #define DEBUG_DRAND_UART 1 */
/* #if DEBUG_DRAND_UART */
/* #include "dbg-io/dbg.h" */
/* #undef PRINTF */
/* #define PRINTF(...) do { char buf[128]; sprintf(buf, __VA_ARGS__); dbg_send_bytes(buf, strlen(buf)); } while(0) */
/* #else */
#define PRINTF(...) printf(__VA_ARGS__)
/* #endif */


#include "lib/random.h"
#include "lib/list.h"

PROCESS(drand_process, "DRAND");
/* AUTOSTART_PROCESSES(&drand_process); */

#define DRAND_MAX_NEIGHBORS 40


static struct etimer drand_phase_timer;
static struct etimer neighbor_discovery_phase_timer;

static struct ctimer neighbor_purge_timer;
static struct ctimer request_timeout_timer;
static struct ctimer fail_release_timeout_timer;

enum DRAND_STATE {
    DRAND_IDLE,
    DRAND_REQUEST,
    DRAND_GRANT,
    DRAND_RELEASE
};

enum DRAND_MESSAGE_TYPE {
    DRAND_MSG_REQUEST,
    DRAND_MSG_GRANT,
    DRAND_MSG_REJECT,
    DRAND_MSG_FAIL,
    DRAND_MSG_RELEASE,
    DRAND_MSG_TWO_HOP_RELEASE,
};

enum DRAND_GRANT_REJECT_STATE {
    DRAND_ANSWER_PENDING,
    DRAND_RECEIVED_GRANT,
    DRAND_RECEIVED_REJECT,
};

enum DRAND_NODE_TYPE {
    TWO_HOP_NEIGHBOR,
    DIRECT_NEIGHBOR
};

struct drand_neighbor_state {
    struct drand_neighbor_state *next;

    enum DRAND_NODE_TYPE node_type;

    linkaddr_t neighbor_addr;
    uint8_t has_timeslot, chosen_timeslot;

    enum DRAND_GRANT_REJECT_STATE grant_or_reject_state;

    clock_time_t last_seen;
};

static void grant_reject_update_dA(clock_time_t reception_time);
static void handle_request(uint8_t roundNumber, const linkaddr_t *from_addr);
static void handle_release(uint8_t roundNumber, const linkaddr_t *from_addr);
static void handle_reject(uint8_t roundNumber, const linkaddr_t *from_addr);
static void broadcast_fail(uint8_t roundNumber);
static void broadcast_release(uint8_t roundNumber);
static void unicast_fail(uint8_t roundNumber, const linkaddr_t *to_addr);
static void handle_fail(uint8_t roundNumber, const linkaddr_t *from_addr);
static void handle_grant(uint8_t roundNumber, const linkaddr_t *from_addr);
static uint8_t check_all_grant();
static uint8_t check_pending();
static void pick_timeslot();
static void broadcast_two_hop_release(uint8_t roundNumber, uint8_t timeslot, const linkaddr_t *node_addr);
static void unicast_release(uint8_t roundNumber, const linkaddr_t *node_addr);
static uint8_t toss_coin();
static void fail_release_timeout_callback(void*);
static void request_timeout_callback(void*);
static void drand_change_state(enum DRAND_STATE new_state);
static void pretty_print_drand_state(enum DRAND_STATE state);
static uint8_t drand_compare_address(const linkaddr_t *addr1, const linkaddr_t *addr2);

LIST(drand_neighbor_list);
MEMB(drand_neighbor_memb, struct drand_neighbor_state, DRAND_MAX_NEIGHBORS);


// we are a little bit hacky here and use only the two high order bytes of the address
// in our neighbor table and for message transmissions
static uint8_t drand_compare_address(const linkaddr_t *addr1, const linkaddr_t *addr2) {
    return (addr1->u8[0] == addr2->u8[0]) && (addr1->u8[1] == addr2->u8[1]);
}



static struct drand_node_state {
    enum DRAND_STATE drand_state;
    uint8_t currentRound;

    uint8_t max_slotframe_length;

    uint8_t own_timeslot, has_timeslot;

    clock_time_t TA, dA, last_grant_or_reject_time, last_request_time, last_lottery_time;

    // timeslot callback function
    void (*timeslot_callback)(uint8_t timeslot);

    // information that we keep upon receiving a request from a neighbor
    linkaddr_t grant_neighbor_addr; // address which caused us to go into GRANT state
    uint8_t grant_neighbor_round;

} node_state = {
    .currentRound = 0,
    .drand_state = DRAND_IDLE,
    .max_slotframe_length = 0,
    .has_timeslot = 0,
    .own_timeslot = UINT8_MAX,
    // initialize dA, dB with some sensible defaults
    .dA = CLOCK_SECOND,
    .TA = 3 * CLOCK_SECOND
};


void drand_set_timeslot_callback(void (*cb)(uint8_t timeslot)) {
    node_state.timeslot_callback = cb;
}

void drand_print_neighbor_table() {
    printf("drand neighbor table:\n");
    struct drand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        uint16_t last_seen_seconds = neighbor->last_seen / CLOCK_SECOND;
        printf("neighbor %d.%d has_timeslot %d chosen_timeslot %d last_seen %d.%d\n",
            neighbor->neighbor_addr.u8[0], neighbor->neighbor_addr.u8[1],
            neighbor->has_timeslot, neighbor->chosen_timeslot,
            last_seen_seconds / 60, last_seen_seconds % 60);
    }
}


static struct neighbor_discovery_conn neighbor_discovery;
static struct unicast_conn unicast;
static struct broadcast_conn broadcast;

static void pretty_print_msg_type(enum DRAND_MESSAGE_TYPE msg_type) {
    switch(msg_type) {
    case DRAND_MSG_REQUEST:
        printf("DRAND_MSG_REQUEST");
        break;
    case DRAND_MSG_GRANT:
        printf("DRAND_MSG_GRANT");
        break;
    case DRAND_MSG_REJECT:
        printf("DRAND_MSG_REJECT");
        break;
    case DRAND_MSG_FAIL:
        printf("DRAND_MSG_FAIL");
        break;
    case DRAND_MSG_RELEASE:
        printf("DRAND_MSG_RELEASE");
        break;
    case DRAND_MSG_TWO_HOP_RELEASE:
        printf("DRAND_MSG_TWO_HOP_RELEASE");
        break;
    default:
        printf("UNKNOWN");
        break;
    }
}

static void recvd_message(const linkaddr_t *from) {
   enum DRAND_MESSAGE_TYPE msg_type = ((uint8_t *)packetbuf_dataptr())[0];
    uint8_t roundNumber = ((uint8_t *)packetbuf_dataptr())[1];
    printf("received message from %d.%d type ", from->u8[0], from->u8[1]);
    pretty_print_msg_type(msg_type);
    printf("\n");

    switch(msg_type) {
    case DRAND_MSG_REQUEST:
        handle_request(roundNumber, from);
        break;
    case DRAND_MSG_GRANT:
        handle_grant(roundNumber, from);
        break;
    case DRAND_MSG_FAIL:
        handle_fail(roundNumber, from);
        break;
    case DRAND_MSG_REJECT:
        handle_reject(roundNumber, from);
        break;
    case DRAND_MSG_RELEASE:
        handle_release(roundNumber, from);
        default:
            break;
    }
}

static void unicast_recv(struct unicast_conn *c, const linkaddr_t *from) {
    recvd_message(from);
}


static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
    recvd_message(from);
}

static void unicast_sent(struct unicast_conn *ptr, int status, int num_tx) {
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static const struct unicast_callbacks unicast_call = {.recv = unicast_recv, .sent = unicast_sent };

static void discovery_recv(struct neighbor_discovery_conn *c,
    const linkaddr_t *from, uint16_t val) {
    printf("discovered %d.%d val %d\n", from->u8[0], from->u8[1], val);

    // add entry to drand_neighbor_list
    // look whether there is an entry already in our table
    struct drand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(drand_compare_address(&neighbor->neighbor_addr, from)) {
            break;
        }
    }

    if(neighbor == NULL) {
        // no entry found, create new one
        neighbor = memb_alloc(&drand_neighbor_memb);
        if(neighbor == NULL) {
            printf("could not allocate memory for neighbor\n");
            return;
        }
        linkaddr_copy(&neighbor->neighbor_addr, from);
        neighbor->has_timeslot = 0;
        neighbor->chosen_timeslot = 0;
        neighbor->last_seen = clock_time();
        neighbor->node_type = DIRECT_NEIGHBOR;
        list_add(drand_neighbor_list, neighbor);
    } else {
        neighbor->last_seen = clock_time();
    }
}

static void discovery_sent(struct neighbor_discovery_conn *c) {
    /* printf("sent neighbor discovery thingy\n"); */
}

static const struct neighbor_discovery_callbacks neighbor_discovery_calls = { .recv = discovery_recv, .sent = discovery_sent };

static void broadcast_request(uint8_t roundNumber) {
    printf("broadcasting request for round %d\n", roundNumber);
    uint8_t buf[2];
    buf[0] = DRAND_MSG_REQUEST;
    buf[1] = roundNumber;
    packetbuf_copyfrom(buf, 2);
    broadcast_send(&broadcast);
}

static void unicast_request(uint8_t roundNumber, const linkaddr_t *to_addr) {
    printf("unicasting request for round %d to %d.%d\n", roundNumber, to_addr->u8[0], to_addr->u8[1]);
    uint8_t buf[2];
    buf[0] = DRAND_MSG_REQUEST;
    buf[1] = roundNumber;
    packetbuf_copyfrom(buf, 2);
    unicast_send(&unicast, to_addr);
}

static void resend_request(uint8_t roundNumber) {
    // go over list of neighbors, and if we did not yet receive a grant or reject, resend the request message
    struct drand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(neighbor->grant_or_reject_state != DRAND_RECEIVED_GRANT && neighbor->node_type == DIRECT_NEIGHBOR) {
            unicast_request(roundNumber, &neighbor->neighbor_addr);
        }
    }
}

static void send_reject(uint8_t roundNumber, const linkaddr_t *to_addr) {
    printf("unicasting reject for round %d to %d.%d\n", roundNumber, to_addr->u8[0], to_addr->u8[1]);
    uint8_t buf[2];
    buf[0] = DRAND_MSG_REJECT;
    buf[1] = roundNumber;
    packetbuf_copyfrom(buf, 2);
    unicast_send(&unicast, to_addr);
}

static void broadcast_fail(uint8_t roundNumber) {
    printf("broadcasting fail for round %d\n", roundNumber);
    uint8_t buf[2];
    buf[0] = DRAND_MSG_FAIL;
    buf[1] = roundNumber;
    packetbuf_copyfrom(buf, 2);
    broadcast_send(&broadcast);
}

static void unicast_fail(uint8_t roundNumber, const linkaddr_t *to_addr) {
    printf("unicasting fail for round %d to %d.%d\n", roundNumber, to_addr->u8[0], to_addr->u8[1]);
    uint8_t buf[2];
    buf[0] = DRAND_MSG_FAIL;
    buf[1] = roundNumber;
    packetbuf_copyfrom(buf, 2);
    unicast_send(&unicast, to_addr);
}

static void handle_reject(uint8_t roundNumber, const linkaddr_t *from_addr) {
    clock_time_t reception_time = clock_time();
    // search through neighbor list and update state
    struct drand_neighbor_state *neighbor = NULL;

    if(node_state.drand_state != DRAND_REQUEST) {
        printf("received reject but we are not in REQUEST\n");
        return;
    }

    if(roundNumber != node_state.currentRound) {
        printf("received reject for round %d, but we are in round %d\n", roundNumber, node_state.currentRound);
        return;
    }

    ctimer_stop(&request_timeout_timer);        

    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(drand_compare_address(&neighbor->neighbor_addr, from_addr)) {
            neighbor->grant_or_reject_state = DRAND_RECEIVED_REJECT;
            neighbor->last_seen = reception_time;
            break;
        }
    }

    drand_change_state(DRAND_IDLE);
    broadcast_fail(roundNumber);

    grant_reject_update_dA(reception_time);
}

static void handle_fail(uint8_t roundNumber, const linkaddr_t *from_addr) {

    if(node_state.drand_state != DRAND_GRANT) {
        printf("received fail but we are not in GRANT\n");
        return;
    }
    
    if (!drand_compare_address(&node_state.grant_neighbor_addr, from_addr) || !(roundNumber == node_state.grant_neighbor_round)) {
        printf("received fail from neighbor we did not grant to\n");
        return;
    }
    
    ctimer_stop(&fail_release_timeout_timer);
    
    if(!node_state.has_timeslot) {
        drand_change_state(DRAND_IDLE);
    } else {
        drand_change_state(DRAND_RELEASE);
    }
}

static uint8_t toss_coin() {
    return (random_rand()) > (RANDOM_RAND_MAX/2);
}


// returns the number of neighbors that have not yet decided on a timeslot
static uint8_t undecided_number_of_neighbors() {
    uint8_t neighbor_counter = 0;
    for(struct drand_neighbor_state *neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(neighbor->has_timeslot == 0) {
            neighbor_counter++;
        }
    }
    return neighbor_counter;
}


// upon successfull coin toss, a node will try the lottery
static uint8_t try_lottery() {
    clock_time_t curr_lottery_time = clock_time();
    if((node_state.last_lottery_time - curr_lottery_time) < node_state.TA) {
        return 0;
    }

    node_state.last_lottery_time = curr_lottery_time;
    
    // every new try of the lottery will increase the round counter
    node_state.currentRound++;

    printf("increasing round counter to %d\n", node_state.currentRound);
    
    // the probability of winning is equal to 1 / the number of undecided neighbors
    uint8_t neighbors = undecided_number_of_neighbors();
    uint16_t rand = random_rand();
    
    if(!neighbors) {
        return 1;
    }
    
    return (rand < (RANDOM_RAND_MAX / neighbors));
}

#define GRANT_MSG_NEIGHBOR_CNT_POS 2

static void send_grant(uint8_t roundNumber, const linkaddr_t *to_addr) {
    uint8_t buf[100];
    uint8_t curr_len = 0;
    buf[curr_len] = DRAND_MSG_GRANT;
    curr_len++;
    buf[curr_len] = roundNumber;
    curr_len++;
    
    // list length
    curr_len = GRANT_MSG_NEIGHBOR_CNT_POS;
    buf[curr_len] = 0;
    curr_len++;

    // further we will iterate over our neighbor table and include
    // the timeslots that have been chosen by them
    struct drand_neighbor_state *neighbor = NULL;
    uint8_t neighbor_count = 0;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(!drand_compare_address(&neighbor->neighbor_addr, to_addr) && neighbor->has_timeslot && neighbor->node_type == DIRECT_NEIGHBOR) {
            // include two high order bytes of address
            buf[curr_len] = neighbor->neighbor_addr.u8[0];
            curr_len++;
            buf[curr_len] = neighbor->neighbor_addr.u8[1];
            curr_len++;
            buf[curr_len] = neighbor->chosen_timeslot;
            curr_len++;
            neighbor_count++;
        }
    }

    buf[GRANT_MSG_NEIGHBOR_CNT_POS] = neighbor_count;

    packetbuf_copyfrom(buf, curr_len);
    unicast_send(&unicast, to_addr);

    printf("sending grant(%u) to %u.%u\n", roundNumber, to_addr->u8[0], to_addr->u8[1]);
}

static void grant_reject_update_dA(clock_time_t reception_time) {
    node_state.last_grant_or_reject_time = reception_time;
    clock_time_t time_diff = node_state.last_grant_or_reject_time - node_state.last_request_time;

    if(time_diff > node_state.dA) {
        node_state.dA = time_diff;
        node_state.TA = 3*node_state.dA;
    }

    printf("new dA: %lu\n", node_state.dA);
}

static void drand_change_state(enum DRAND_STATE new_state) {
    printf("changing state: ");
    pretty_print_drand_state(node_state.drand_state);
    printf(" -> ");
    pretty_print_drand_state(new_state);
    printf("\n");

    node_state.drand_state = new_state;
}

static void handle_grant(uint8_t roundNumber, const linkaddr_t *from_addr) {
    clock_time_t reception_time = clock_time();
    uint8_t neighbor_timeslot_amount; // byte 3 in neighbor message
    uint8_t curr_len = GRANT_MSG_NEIGHBOR_CNT_POS; // start parsing from neighbor amount

    memcpy(&neighbor_timeslot_amount, packetbuf_dataptr() + curr_len, sizeof(uint8_t));
    curr_len++;

    // in this case previously we previosyly failed through a reject, so we should always answer
    // by uicasting back a fail
    if(node_state.drand_state == DRAND_IDLE || node_state.drand_state == DRAND_GRANT) {
        unicast_fail(roundNumber, from_addr);
    }

    // update neighbor state from which we received grant
    struct drand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(drand_compare_address(&neighbor->neighbor_addr, from_addr)) {
            if(neighbor->grant_or_reject_state == DRAND_RECEIVED_GRANT && node_state.has_timeslot) { // TODO Should we only release if we got grant from everybody else?
                unicast_release(roundNumber, from_addr);
            /* } else if(neighbor->grant_or_reject_state == DRAND_RECEIVED_REJECT) { */
            /*     // if we received a reject from the node we will transmit back a fail to it */
            /*     unicast_fail(roundNumber, from_addr);  */
            /* } */
            }

            neighbor->grant_or_reject_state = DRAND_RECEIVED_GRANT;
            neighbor->last_seen = reception_time;
            break;
        }
    }

    printf("Grant contained %u timeslots\n", neighbor_timeslot_amount);
    for(uint8_t i = 0; i < neighbor_timeslot_amount; i++) {
        uint8_t neighbor_addr_high;
        uint8_t neighbor_addr_low;
        uint8_t chosen_timeslot;
        memcpy(&neighbor_addr_high, packetbuf_dataptr() + curr_len, sizeof(uint8_t));
        curr_len++;
        memcpy(&neighbor_addr_low, packetbuf_dataptr() + curr_len, sizeof(uint8_t));
        curr_len++;
        memcpy(&chosen_timeslot, packetbuf_dataptr() + curr_len, sizeof(uint8_t));
        curr_len++;

        // update neighbor state from which we received grant, if there is no neighbor entry yet
        // we will create a new entry and set neighbor state to two_hop
        struct drand_neighbor_state *neighbor = NULL;
        for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
            if(neighbor->neighbor_addr.u8[0] == neighbor_addr_high && neighbor->neighbor_addr.u8[1] == neighbor_addr_low) {
                neighbor->has_timeslot = 1;
                neighbor->chosen_timeslot = chosen_timeslot;
                break;
            }
        }
        
        if(neighbor == NULL) {
            neighbor = memb_alloc(&drand_neighbor_memb);
            neighbor->neighbor_addr.u8[0] = neighbor_addr_high;
            neighbor->neighbor_addr.u8[1] = neighbor_addr_low;
            neighbor->has_timeslot = 1;
            neighbor->chosen_timeslot = chosen_timeslot;
            neighbor->node_type = TWO_HOP_NEIGHBOR;
            neighbor->last_seen = reception_time;
            list_add(drand_neighbor_list, neighbor);
        }
    }

    grant_reject_update_dA(reception_time);

    if(!check_pending()) {
        ctimer_stop(&request_timeout_timer);        
    }

    if(!node_state.has_timeslot && check_all_grant()) {
        pick_timeslot();
        drand_change_state(DRAND_RELEASE);
        broadcast_release(node_state.currentRound);
    }
}

static void handle_request(uint8_t roundNumber, const linkaddr_t *from_addr) {
    switch(node_state.drand_state) {
    case DRAND_IDLE:
    case DRAND_RELEASE:
        drand_change_state(DRAND_GRANT);
        node_state.grant_neighbor_addr = *from_addr;
        node_state.grant_neighbor_round = roundNumber;
        send_grant(node_state.grant_neighbor_round, &node_state.grant_neighbor_addr);
        ctimer_set(&fail_release_timeout_timer, node_state.dA, fail_release_timeout_callback, NULL);
        break;
    case DRAND_REQUEST:
    case DRAND_GRANT:
        /* if(drand_compare_address(&node_state.grant_neighbor_addr, from_addr) && node_state.grant_neighbor_round == roundNumber) { */
        /*     send_grant(roundNumber, from_addr); */
        /* } else { */
        // we will send a reject to the node we received request from

        // TODO we have a timeout which resends grant messages. But couldn't we also answer to a repeated request with a grant?
        if(!drand_compare_address(&node_state.grant_neighbor_addr, from_addr)) {
            send_reject(roundNumber, from_addr);
        }
        /* } */
        break;
    }
}

static void handle_release(uint8_t roundNumber, const linkaddr_t *from_addr) {
    uint8_t other_timeslot;
    memcpy(&other_timeslot, packetbuf_dataptr() + 2, sizeof(uint8_t));

    // check first if we even expect a release message
    if(node_state.drand_state != DRAND_GRANT) {
        printf("received release, but we are not in GRANT\n");
        return;
    }

    if (!drand_compare_address(&node_state.grant_neighbor_addr, from_addr) || !(roundNumber == node_state.grant_neighbor_round)) {
        printf("received release from neighbor we did not grant to. %u vs %u\n", roundNumber, node_state.grant_neighbor_round);
        return;
    }
    
    ctimer_stop(&fail_release_timeout_timer);

    // update neighbor state from which we received release
    struct drand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(drand_compare_address(&neighbor->neighbor_addr, from_addr)) {
            printf("received release from neighbor with timeslot %u\n", other_timeslot);
            neighbor->has_timeslot = 1;
            neighbor->chosen_timeslot = other_timeslot;
            neighbor->last_seen = clock_time();
            break;
        }
    }

    // go into IDLE or Release depending on whether we already have a slot
    if(node_state.has_timeslot) {
        drand_change_state(DRAND_RELEASE);
    } else {
        drand_change_state(DRAND_IDLE);
    }

    broadcast_two_hop_release(roundNumber, other_timeslot, from_addr);
}

static void handle_two_hop_release(uint8_t roundNumber, const linkaddr_t *from_addr) {
    uint8_t other_timeslot;
    uint8_t other_addr_high, other_addr_low;
    memcpy(&other_timeslot, packetbuf_dataptr() + 2, sizeof(uint8_t));
    memcpy(&other_addr_high, packetbuf_dataptr() + 3, sizeof(uint8_t));
    memcpy(&other_addr_low, packetbuf_dataptr() + 4, sizeof(uint8_t));

    // search for entry in our table
    struct drand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(neighbor->neighbor_addr.u8[0] == other_addr_high && neighbor->neighbor_addr.u8[1] == other_addr_low) {
            neighbor->has_timeslot = 1;
            neighbor->chosen_timeslot = other_timeslot;
            break;
        }
    }

    // otherwise create new time timeslot with two_hop neighbor type
    if(neighbor == NULL) {
        neighbor = memb_alloc(&drand_neighbor_memb);
        neighbor->neighbor_addr.u8[0] = other_addr_high;
        neighbor->neighbor_addr.u8[1] = other_addr_low;
        neighbor->has_timeslot = 1;
        neighbor->chosen_timeslot = other_timeslot;
        neighbor->node_type = TWO_HOP_NEIGHBOR;
        list_add(drand_neighbor_list, neighbor);
    }
}

static void pick_timeslot() {
    // go through our neighbor table and pick the minimum timeslot
    // that has not yet been taken by a node in a two hop neighborhood
    uint8_t min_free_timeslot = 0;
    uint8_t found[node_state.max_slotframe_length];

    /* if(node_state.has_timeslot) { */
    /*     return; */
    /* } */

    // initialize with zeros
    for(uint8_t i = 0; i < node_state.max_slotframe_length; i++) {
        found[i] = 0;
    }

    // iterate once over all neighbors and mark with a 1 timeslots that are occupied
    struct drand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(neighbor->has_timeslot) {
            found[neighbor->chosen_timeslot] = 1;
        }
    }

    // next choose smallest found index number as new timeslot
    uint8_t i = 0;
    for(i = 0; i < node_state.max_slotframe_length; i++) {
        if(!found[i]) {
            min_free_timeslot = i;
            break;
        }
    }

    if(i == node_state.max_slotframe_length) {
        printf("Error: Could not find free timeslot\n");
        return;
    }

    // call callback only once
    if(!node_state.has_timeslot) {
        node_state.timeslot_callback(min_free_timeslot);        
    }    

    node_state.has_timeslot = 1;
    node_state.own_timeslot = min_free_timeslot;
}


static void broadcast_release(uint8_t roundNumber) {
    printf("broadcasting release with round number %u and timeslot %u\n", roundNumber, node_state.own_timeslot);
    // broadcast release message as well as chosen timeslot
    uint8_t buf[3];
    buf[0] = DRAND_MSG_RELEASE;
    buf[1] = roundNumber;
    buf[2] = node_state.own_timeslot;
    packetbuf_copyfrom(buf, 3);
    broadcast_send(&broadcast);
}

static void unicast_release(uint8_t roundNumber, const linkaddr_t *node_addr) {
    // unicast release message as well as chosen timeslot
    printf("unicasting release with round number %u and timeslot %u\n", roundNumber, node_state.own_timeslot);
    uint8_t buf[3];
    buf[0] = DRAND_MSG_RELEASE;
    buf[1] = roundNumber;
    buf[2] = node_state.own_timeslot;
    packetbuf_copyfrom(buf, 3);
    unicast_send(&unicast, node_addr);
}


// used for optimizing send intervals, we don't use that information yet however
static void broadcast_two_hop_release(uint8_t roundNumber, uint8_t timeslot, const linkaddr_t *node_addr) {
    uint8_t buf[5];
    buf[0] = DRAND_MSG_TWO_HOP_RELEASE;
    buf[1] = roundNumber;
    buf[2] = timeslot;
    buf[3] = node_addr->u8[0];
    buf[4] = node_addr->u8[1];

    packetbuf_copyfrom(buf, 5);

    broadcast_send(&broadcast);
}

static uint8_t check_all_grant() {
    // go over neighbor list and check whether we have received a release for all neighbors
    printf("check_all_received\n");
    struct drand_neighbor_state *neighbor = NULL;
    uint8_t all_grant = 1;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(neighbor->node_type == DIRECT_NEIGHBOR) {
            if(neighbor->grant_or_reject_state != DRAND_RECEIVED_GRANT) {
                all_grant = 0;
                break;
            }
        }
    }
    
    return all_grant;
}

static uint8_t check_pending() {
    printf("check_all_received\n");
    struct drand_neighbor_state *neighbor = NULL;
    uint8_t pending = 0;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(neighbor->node_type == DIRECT_NEIGHBOR) {
            if(neighbor->grant_or_reject_state == DRAND_ANSWER_PENDING) {
                pending = 1;
                break;
            }
        }
    }
    
    return pending;    
}

#define MTM_ROUND_START_OFFSET 1

void drand_init(uint8_t max_slotframe_length) {
    node_state.max_slotframe_length = max_slotframe_length;

    // configure initial slotframe layout
    struct tsch_slotframe *sf_eb = tsch_schedule_add_slotframe(0, max_slotframe_length + MTM_ROUND_START_OFFSET);
    tsch_schedule_add_link(
        sf_eb,
        LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
        LINK_TYPE_ADVERTISING,
        &tsch_broadcast_address,
        0,
        0);

    // initialize all other links as RX PROP MTM
    for(int i = MTM_ROUND_START_OFFSET; i < max_slotframe_length+1; i++) {
        tsch_schedule_add_link(
            sf_eb,
            LINK_OPTION_RX,
            LINK_TYPE_PROP_MTM,
            &tsch_broadcast_address,
            i,
            0);
    }

    neighbor_discovery_open(&neighbor_discovery,
                             42,
                             CLOCK_SECOND/2,
                             CLOCK_SECOND/8,
                             CLOCK_SECOND,
        &neighbor_discovery_calls);
    broadcast_open(&broadcast, 43, &broadcast_call);
    unicast_open(&unicast, 44, &unicast_call);

    neighbor_discovery_start(&neighbor_discovery, 0);

    // also start drand process
    process_start(&drand_process, NULL);
}

static void neighbors_set_response_pending() {
    // iterate over all neighbors and set that wait for a grant/reject
    struct drand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(neighbor->node_type == DIRECT_NEIGHBOR) {
            neighbor->grant_or_reject_state = DRAND_ANSWER_PENDING;
        }
    }
}


static void fail_release_timeout_callback(void*) {
    printf("FAIL RELEASE TIMEOUT: resending grant\n");
    send_grant(node_state.grant_neighbor_round, &node_state.grant_neighbor_addr);
    
    ctimer_reset(&fail_release_timeout_timer);
}

static void request_timeout_callback(void*) {
    printf("REQUEST TIMEOUT: Resending request\n");
    
    node_state.last_request_time = clock_time();
    resend_request(node_state.currentRound);
    
    ctimer_reset(&request_timeout_timer);
}

// I don't think we should do this but rather use the last_seen stats
static void purge_neighbors(void*) {
    struct drand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(clock_time() - neighbor->last_seen > CLOCK_SECOND * 10) {
            list_remove(drand_neighbor_list, neighbor);
            memb_free(&drand_neighbor_memb, neighbor);
        }

        drand_print_neighbor_table();
    }
}

static void pretty_print_drand_state(enum DRAND_STATE state) {
    switch(state) {
        case DRAND_IDLE:
            printf("IDLE");
            break;
        case DRAND_REQUEST:
            printf("REQUEST");
            break;
        case DRAND_GRANT:
            printf("GRANT");
            break;
        case DRAND_RELEASE:
            printf("RELEASE");
            break;
        default:
            printf("UNKNOWN");
            break;
    }
}

static void print_drand_status() {
    // nicely print current node status and status of neighbors
    PRINTF("------------------------------------\n");
    PRINTF("Node %u.%u: ", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
    PRINTF("current_state: ");
    pretty_print_drand_state(node_state.drand_state);
    PRINTF("\n");
    PRINTF("current request timeout: %u\n", node_state.dA);

    struct drand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(drand_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        PRINTF("Neighbor %u.%u, Status: ", neighbor->neighbor_addr.u8[0], neighbor->neighbor_addr.u8[1]);
        switch(neighbor->grant_or_reject_state) {
            case DRAND_ANSWER_PENDING:
                PRINTF("Pending");
                break;
            case DRAND_RECEIVED_GRANT:
                PRINTF("Granted");
                break;
            case DRAND_RECEIVED_REJECT:
                PRINTF("Rejected");
                break;
            default:
                PRINTF("Unknown state");
                break;
        }

        // print TWO hop type
        PRINTF(", Hops: ");
        switch(neighbor->node_type) {
            case DIRECT_NEIGHBOR:
                PRINTF("1");
                break;
        case TWO_HOP_NEIGHBOR:
                PRINTF("2");
                break;
         default:
                PRINTF("?");
                break;
        }
        
        if(neighbor->has_timeslot) {
            PRINTF(", Slot: %u", neighbor->chosen_timeslot);
        }

        /* watchdoc periodic */
        watchdog_periodic();
        
        PRINTF("\n");
    }
    
    if(node_state.has_timeslot) {
        PRINTF("Timeslot: %u\n", node_state.own_timeslot);
    }
    PRINTF("------------------------------------\n");
}

PROCESS_THREAD(drand_process, ev, data)
{
  PROCESS_BEGIN();

  static uint8_t drand_running = 0;

// we will run a neighbor discovery for about 20 seconds before beginning the coloring algorithm
  etimer_set(&neighbor_discovery_phase_timer, CLOCK_SECOND * 40);
  /* ctimer_set(&purge_neighbors_timer, CLOCK_SECOND * 10, purge_neighbors, NULL); */

  while(1) {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&drand_phase_timer)
          || etimer_expired(&neighbor_discovery_phase_timer)
          );

      if(etimer_expired(&neighbor_discovery_phase_timer) && !drand_running) {
          printf("Initial neighbor discovery phase is over, starting DRAND phase\n");
          etimer_set(&drand_phase_timer, CLOCK_SECOND * 2);
          drand_change_state(DRAND_IDLE);

          etimer_stop(&neighbor_discovery_phase_timer); // is this enough to prevent multiple triggers?
          neighbor_discovery_close(&neighbor_discovery);
          drand_running = 1;
      } else if(etimer_expired(&drand_phase_timer) && drand_running) {
          print_drand_status();
          // no more neighbor discovery needed
          if(!node_state.has_timeslot && node_state.drand_state == DRAND_IDLE && toss_coin() && try_lottery()) {
              printf("Node %u.%u won the lottery\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
              drand_change_state(DRAND_REQUEST);
              neighbors_set_response_pending();
              
              broadcast_request(node_state.currentRound);
              
              // start timer based on the value of node_state.dA              
              node_state.last_request_time = clock_time();
              ctimer_set(&request_timeout_timer, node_state.dA, request_timeout_callback, NULL);
          }
          
          etimer_reset(&drand_phase_timer);
      }
  }

  PROCESS_END();
}
