/*
 * net/tipc/subscr.h: Include file for TIPC network topology service
 *
 * Copyright (c) 2003-2006, Ericsson AB
 * Copyright (c) 2005-2007, Wind River Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TIPC_SUBSCR_H
#define _TIPC_SUBSCR_H

struct subscription;

typedef void (*tipc_subscr_event) (struct subscription *sub,
				   u32 found_lower, u32 found_upper,
				   u32 event, u32 port_ref, u32 node);

/**
 * struct subscription - TIPC network topology subscription object
 * @seq: name sequence associated with subscription
 * @timeout: duration of subscription (in ms)
 * @filter: event filtering to be done for subscription
 * @event_cb: routine invoked when a subscription event is detected
 * @timer: timer governing subscription duration (optional)
 * @nameseq_list: adjacent subscriptions in name sequence's subscription list
 * @subscription_list: adjacent subscriptions in subscriber's subscription list
 * @server_ref: object reference of server port associated with subscription
 * @evt: template for events generated by subscription
 */

struct subscription {
	struct tipc_name_seq seq;
	u32 timeout;
	u32 filter;
	tipc_subscr_event event_cb;
	struct timer_list timer;
	struct list_head nameseq_list;
	struct list_head subscription_list;
	u32 server_ref;
	struct tipc_event evt;
};

int tipc_subscr_overlap(struct subscription *sub,
			u32 found_lower,
			u32 found_upper);

void tipc_subscr_report_overlap(struct subscription *sub,
				u32 found_lower,
				u32 found_upper,
				u32 event,
				u32 port_ref,
				u32 node,
				int must_report);

int tipc_subscr_start(void);

void tipc_subscr_stop(void);


#endif
