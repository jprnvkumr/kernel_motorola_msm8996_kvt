/*
 * llc_station.c - station component of LLC
 *
 * Copyright (c) 1997 by Procom Technology, Inc.
 * 		 2001-2003 by Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * This program can be redistributed or modified under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * This program is distributed without any warranty or implied warranty
 * of merchantability or fitness for a particular purpose.
 *
 * See the GNU General Public License for more details.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <net/llc.h>
#include <net/llc_sap.h>
#include <net/llc_conn.h>
#include <net/llc_c_ac.h>
#include <net/llc_s_ac.h>
#include <net/llc_c_ev.h>
#include <net/llc_c_st.h>
#include <net/llc_s_ev.h>
#include <net/llc_s_st.h>
#include <net/llc_pdu.h>

typedef int (*llc_station_ev_t)(struct sk_buff *skb);

typedef int (*llc_station_action_t)(struct sk_buff *skb);

/* Station component state table structure */
struct llc_station_state_trans {
	llc_station_ev_t ev;
	llc_station_action_t *ev_actions;
};

static int llc_stat_ev_rx_null_dsap_xid_c(struct sk_buff *skb)
{
	struct llc_pdu_un *pdu = llc_pdu_un_hdr(skb);

	return LLC_PDU_IS_CMD(pdu) &&			/* command PDU */
	       LLC_PDU_TYPE_IS_U(pdu) &&		/* U type PDU */
	       LLC_U_PDU_CMD(pdu) == LLC_1_PDU_CMD_XID &&
	       !pdu->dsap ? 0 : 1;			/* NULL DSAP value */
}

static int llc_stat_ev_rx_null_dsap_test_c(struct sk_buff *skb)
{
	struct llc_pdu_un *pdu = llc_pdu_un_hdr(skb);

	return LLC_PDU_IS_CMD(pdu) &&			/* command PDU */
	       LLC_PDU_TYPE_IS_U(pdu) &&		/* U type PDU */
	       LLC_U_PDU_CMD(pdu) == LLC_1_PDU_CMD_TEST &&
	       !pdu->dsap ? 0 : 1;			/* NULL DSAP */
}

static int llc_station_ac_send_xid_r(struct sk_buff *skb)
{
	u8 mac_da[ETH_ALEN], dsap;
	int rc = 1;
	struct sk_buff *nskb = llc_alloc_frame(NULL, skb->dev, LLC_PDU_TYPE_U,
					       sizeof(struct llc_xid_info));

	if (!nskb)
		goto out;
	rc = 0;
	llc_pdu_decode_sa(skb, mac_da);
	llc_pdu_decode_ssap(skb, &dsap);
	llc_pdu_header_init(nskb, LLC_PDU_TYPE_U, 0, dsap, LLC_PDU_RSP);
	llc_pdu_init_as_xid_rsp(nskb, LLC_XID_NULL_CLASS_2, 127);
	rc = llc_mac_hdr_init(nskb, skb->dev->dev_addr, mac_da);
	if (unlikely(rc))
		goto free;
	dev_queue_xmit(nskb);
out:
	return rc;
free:
	kfree_skb(nskb);
	goto out;
}

static int llc_station_ac_send_test_r(struct sk_buff *skb)
{
	u8 mac_da[ETH_ALEN], dsap;
	int rc = 1;
	u32 data_size;
	struct sk_buff *nskb;

	/* The test request command is type U (llc_len = 3) */
	data_size = ntohs(eth_hdr(skb)->h_proto) - 3;
	nskb = llc_alloc_frame(NULL, skb->dev, LLC_PDU_TYPE_U, data_size);

	if (!nskb)
		goto out;
	rc = 0;
	llc_pdu_decode_sa(skb, mac_da);
	llc_pdu_decode_ssap(skb, &dsap);
	llc_pdu_header_init(nskb, LLC_PDU_TYPE_U, 0, dsap, LLC_PDU_RSP);
	llc_pdu_init_as_test_rsp(nskb, skb);
	rc = llc_mac_hdr_init(nskb, skb->dev->dev_addr, mac_da);
	if (unlikely(rc))
		goto free;
	dev_queue_xmit(nskb);
out:
	return rc;
free:
	kfree_skb(nskb);
	goto out;
}

/* state transition for LLC_STATION_EV_RX_NULL_DSAP_XID_C event */
static llc_station_action_t llc_stat_up_state_actions_2[] = {
	[0] = llc_station_ac_send_xid_r,
	[1] = NULL,
};

static struct llc_station_state_trans llc_stat_up_state_trans_2 = {
	.ev	    = llc_stat_ev_rx_null_dsap_xid_c,
	.ev_actions = llc_stat_up_state_actions_2,
};

/* state transition for LLC_STATION_EV_RX_NULL_DSAP_TEST_C event */
static llc_station_action_t llc_stat_up_state_actions_3[] = {
	[0] = llc_station_ac_send_test_r,
	[1] = NULL,
};

static struct llc_station_state_trans llc_stat_up_state_trans_3 = {
	.ev	    = llc_stat_ev_rx_null_dsap_test_c,
	.ev_actions = llc_stat_up_state_actions_3,
};

/* array of pointers; one to each transition */
static struct llc_station_state_trans *llc_stat_up_state_trans [] = {
	&llc_stat_up_state_trans_2,
	&llc_stat_up_state_trans_3,
	NULL,
};

/**
 *	llc_exec_station_trans_actions - executes actions for transition
 *	@trans: Address of the transition
 *	@skb: Address of the event that caused the transition
 *
 *	Executes actions of a transition of the station state machine. Returns
 *	0 if all actions complete successfully, nonzero otherwise.
 */
static u16 llc_exec_station_trans_actions(struct llc_station_state_trans *trans,
					  struct sk_buff *skb)
{
	u16 rc = 0;
	llc_station_action_t *next_action = trans->ev_actions;

	for (; next_action && *next_action; next_action++)
		if ((*next_action)(skb))
			rc = 1;
	return rc;
}

/**
 *	llc_find_station_trans - finds transition for this event
 *	@skb: Address of the event
 *
 *	Search thru events of the current state of the station until list
 *	exhausted or it's obvious that the event is not valid for the current
 *	state. Returns the address of the transition if cound, %NULL otherwise.
 */
static struct llc_station_state_trans *
				llc_find_station_trans(struct sk_buff *skb)
{
	int i = 0;
	struct llc_station_state_trans *rc = NULL;
	struct llc_station_state_trans **next_trans;

	for (next_trans = llc_stat_up_state_trans; next_trans[i]; i++)
		if (!next_trans[i]->ev(skb)) {
			rc = next_trans[i];
			break;
		}
	return rc;
}

/**
 *	llc_station_rcv - send received pdu to the station state machine
 *	@skb: received frame.
 *
 *	Sends data unit to station state machine.
 */
static void llc_station_rcv(struct sk_buff *skb)
{
	struct llc_station_state_trans *trans;

	trans = llc_find_station_trans(skb);
	if (trans)
		llc_exec_station_trans_actions(trans, skb);
	kfree_skb(skb);
}

void __init llc_station_init(void)
{
	llc_set_station_handler(llc_station_rcv);
}

void llc_station_exit(void)
{
	llc_set_station_handler(NULL);
}
