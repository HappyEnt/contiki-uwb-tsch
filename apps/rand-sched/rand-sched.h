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
 *         Rand Scheduler Header Files 
 *
 * \author El Pato
 */

#ifndef __DRAND_H__
#define __DRAND_H__

#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/tsch-conf.h"
#include "net/mac/tsch/tsch-schedule.h"

enum mtm_node_role {
    MTM_PASSIVE,
    MTM_ACTIVE,
    MTM_ANCHOR // an anchor is a active participant that never loses its slot 
};


void rand_sched_set_mobile(uint8_t enable);
void rand_sched_init(uint8_t max_mtm_slots);
void rand_sched_set_timeslot(uint8_t timeslot);
void rand_sched_set_join_prob(uint16_t denominator);
void rand_sched_set_rank(uint8_t rank);
void rand_sched_start();
void rand_sched_stop();

// If timeslot is set fixed, a node with never backoff from its choice.
// This is used for nodes that should act as anchors
void rand_set_timeslot_fixed(uint8_t enable);

#endif /* __DRAND_H__ */
