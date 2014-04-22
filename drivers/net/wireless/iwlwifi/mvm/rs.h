/******************************************************************************
 *
 * Copyright(c) 2003 - 2014 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *****************************************************************************/

#ifndef __rs_h__
#define __rs_h__

#include <net/mac80211.h>

#include "iwl-config.h"

#include "fw-api.h"
#include "iwl-trans.h"

struct iwl_rs_rate_info {
	u8 plcp;	  /* uCode API:  IWL_RATE_6M_PLCP, etc. */
	u8 plcp_ht_siso;  /* uCode API:  IWL_RATE_SISO_6M_PLCP, etc. */
	u8 plcp_ht_mimo2; /* uCode API:  IWL_RATE_MIMO2_6M_PLCP, etc. */
	u8 plcp_vht_siso;
	u8 plcp_vht_mimo2;
	u8 prev_rs;      /* previous rate used in rs algo */
	u8 next_rs;      /* next rate used in rs algo */
};

#define IWL_RATE_60M_PLCP 3

enum {
	IWL_RATE_INVM_INDEX = IWL_RATE_COUNT,
	IWL_RATE_INVALID = IWL_RATE_COUNT,
};

#define LINK_QUAL_MAX_RETRY_NUM 16

enum {
	IWL_RATE_6M_INDEX_TABLE = 0,
	IWL_RATE_9M_INDEX_TABLE,
	IWL_RATE_12M_INDEX_TABLE,
	IWL_RATE_18M_INDEX_TABLE,
	IWL_RATE_24M_INDEX_TABLE,
	IWL_RATE_36M_INDEX_TABLE,
	IWL_RATE_48M_INDEX_TABLE,
	IWL_RATE_54M_INDEX_TABLE,
	IWL_RATE_1M_INDEX_TABLE,
	IWL_RATE_2M_INDEX_TABLE,
	IWL_RATE_5M_INDEX_TABLE,
	IWL_RATE_11M_INDEX_TABLE,
	IWL_RATE_INVM_INDEX_TABLE = IWL_RATE_INVM_INDEX - 1,
};

/* #define vs. enum to keep from defaulting to 'large integer' */
#define	IWL_RATE_6M_MASK   (1 << IWL_RATE_6M_INDEX)
#define	IWL_RATE_9M_MASK   (1 << IWL_RATE_9M_INDEX)
#define	IWL_RATE_12M_MASK  (1 << IWL_RATE_12M_INDEX)
#define	IWL_RATE_18M_MASK  (1 << IWL_RATE_18M_INDEX)
#define	IWL_RATE_24M_MASK  (1 << IWL_RATE_24M_INDEX)
#define	IWL_RATE_36M_MASK  (1 << IWL_RATE_36M_INDEX)
#define	IWL_RATE_48M_MASK  (1 << IWL_RATE_48M_INDEX)
#define	IWL_RATE_54M_MASK  (1 << IWL_RATE_54M_INDEX)
#define IWL_RATE_60M_MASK  (1 << IWL_RATE_60M_INDEX)
#define	IWL_RATE_1M_MASK   (1 << IWL_RATE_1M_INDEX)
#define	IWL_RATE_2M_MASK   (1 << IWL_RATE_2M_INDEX)
#define	IWL_RATE_5M_MASK   (1 << IWL_RATE_5M_INDEX)
#define	IWL_RATE_11M_MASK  (1 << IWL_RATE_11M_INDEX)


/* uCode API values for HT/VHT bit rates */
enum {
	IWL_RATE_HT_SISO_MCS_0_PLCP = 0,
	IWL_RATE_HT_SISO_MCS_1_PLCP = 1,
	IWL_RATE_HT_SISO_MCS_2_PLCP = 2,
	IWL_RATE_HT_SISO_MCS_3_PLCP = 3,
	IWL_RATE_HT_SISO_MCS_4_PLCP = 4,
	IWL_RATE_HT_SISO_MCS_5_PLCP = 5,
	IWL_RATE_HT_SISO_MCS_6_PLCP = 6,
	IWL_RATE_HT_SISO_MCS_7_PLCP = 7,
	IWL_RATE_HT_MIMO2_MCS_0_PLCP = 0x8,
	IWL_RATE_HT_MIMO2_MCS_1_PLCP = 0x9,
	IWL_RATE_HT_MIMO2_MCS_2_PLCP = 0xA,
	IWL_RATE_HT_MIMO2_MCS_3_PLCP = 0xB,
	IWL_RATE_HT_MIMO2_MCS_4_PLCP = 0xC,
	IWL_RATE_HT_MIMO2_MCS_5_PLCP = 0xD,
	IWL_RATE_HT_MIMO2_MCS_6_PLCP = 0xE,
	IWL_RATE_HT_MIMO2_MCS_7_PLCP = 0xF,
	IWL_RATE_VHT_SISO_MCS_0_PLCP = 0,
	IWL_RATE_VHT_SISO_MCS_1_PLCP = 1,
	IWL_RATE_VHT_SISO_MCS_2_PLCP = 2,
	IWL_RATE_VHT_SISO_MCS_3_PLCP = 3,
	IWL_RATE_VHT_SISO_MCS_4_PLCP = 4,
	IWL_RATE_VHT_SISO_MCS_5_PLCP = 5,
	IWL_RATE_VHT_SISO_MCS_6_PLCP = 6,
	IWL_RATE_VHT_SISO_MCS_7_PLCP = 7,
	IWL_RATE_VHT_SISO_MCS_8_PLCP = 8,
	IWL_RATE_VHT_SISO_MCS_9_PLCP = 9,
	IWL_RATE_VHT_MIMO2_MCS_0_PLCP = 0x10,
	IWL_RATE_VHT_MIMO2_MCS_1_PLCP = 0x11,
	IWL_RATE_VHT_MIMO2_MCS_2_PLCP = 0x12,
	IWL_RATE_VHT_MIMO2_MCS_3_PLCP = 0x13,
	IWL_RATE_VHT_MIMO2_MCS_4_PLCP = 0x14,
	IWL_RATE_VHT_MIMO2_MCS_5_PLCP = 0x15,
	IWL_RATE_VHT_MIMO2_MCS_6_PLCP = 0x16,
	IWL_RATE_VHT_MIMO2_MCS_7_PLCP = 0x17,
	IWL_RATE_VHT_MIMO2_MCS_8_PLCP = 0x18,
	IWL_RATE_VHT_MIMO2_MCS_9_PLCP = 0x19,
	IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_HT_MIMO2_MCS_INV_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_VHT_SISO_MCS_INV_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_VHT_MIMO2_MCS_INV_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_HT_SISO_MCS_8_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_HT_SISO_MCS_9_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_HT_MIMO2_MCS_8_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_HT_MIMO2_MCS_9_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
};

#define IWL_RATES_MASK ((1 << IWL_RATE_COUNT) - 1)

#define IWL_INVALID_VALUE    -1

#define IWL_MIN_RSSI_VAL                 -100
#define IWL_MAX_RSSI_VAL                    0

/* These values specify how many Tx frame attempts before
 * searching for a new modulation mode */
#define IWL_LEGACY_FAILURE_LIMIT	160
#define IWL_LEGACY_SUCCESS_LIMIT	480
#define IWL_LEGACY_TABLE_COUNT		160

#define IWL_NONE_LEGACY_FAILURE_LIMIT	400
#define IWL_NONE_LEGACY_SUCCESS_LIMIT	4500
#define IWL_NONE_LEGACY_TABLE_COUNT	1500

/* Success ratio (ACKed / attempted tx frames) values (perfect is 128 * 100) */
#define IWL_RS_GOOD_RATIO		12800	/* 100% */
#define IWL_RATE_SCALE_SWITCH		10880	/*  85% */
#define IWL_RATE_HIGH_TH		10880	/*  85% */
#define IWL_RATE_INCREASE_TH		6400	/*  50% */
#define RS_SR_FORCE_DECREASE		1920	/*  15% */
#define RS_SR_NO_DECREASE		10880	/*  85% */

#define TPC_SR_FORCE_INCREASE		9600	/* 75% */
#define TPC_SR_NO_INCREASE		10880	/* 85% */
#define TPC_TX_POWER_STEP		3
#define TPC_MAX_REDUCTION		15
#define TPC_NO_REDUCTION		0
#define TPC_INVALID			0xff

#define LINK_QUAL_AGG_TIME_LIMIT_DEF	(4000) /* 4 milliseconds */
#define LINK_QUAL_AGG_TIME_LIMIT_MAX	(8000)
#define LINK_QUAL_AGG_TIME_LIMIT_MIN	(100)

#define LINK_QUAL_AGG_DISABLE_START_DEF	(3)
#define LINK_QUAL_AGG_DISABLE_START_MAX	(255)
#define LINK_QUAL_AGG_DISABLE_START_MIN	(0)

#define LINK_QUAL_AGG_FRAME_LIMIT_DEF	(63)
#define LINK_QUAL_AGG_FRAME_LIMIT_MAX	(63)
#define LINK_QUAL_AGG_FRAME_LIMIT_MIN	(0)

#define LQ_SIZE		2	/* 2 mode tables:  "Active" and "Search" */

/* load per tid defines for A-MPDU activation */
#define IWL_AGG_TPT_THREHOLD	0
#define IWL_AGG_LOAD_THRESHOLD	10
#define IWL_AGG_ALL_TID		0xff
#define TID_QUEUE_CELL_SPACING	50	/*mS */
#define TID_QUEUE_MAX_SIZE	20
#define TID_ROUND_VALUE		5	/* mS */

#define TID_MAX_TIME_DIFF ((TID_QUEUE_MAX_SIZE - 1) * TID_QUEUE_CELL_SPACING)
#define TIME_WRAP_AROUND(x, y) (((y) > (x)) ? (y) - (x) : (0-(x)) + (y))

enum iwl_table_type {
	LQ_NONE,
	LQ_LEGACY_G,	/* legacy types */
	LQ_LEGACY_A,
	LQ_HT_SISO,	/* HT types */
	LQ_HT_MIMO2,
	LQ_VHT_SISO,    /* VHT types */
	LQ_VHT_MIMO2,
	LQ_MAX,
};

struct rs_rate {
	int index;
	enum iwl_table_type type;
	u8 ant;
	u32 bw;
	bool sgi;
};


#define is_type_legacy(type) (((type) == LQ_LEGACY_G) || \
			      ((type) == LQ_LEGACY_A))
#define is_type_ht_siso(type) ((type) == LQ_HT_SISO)
#define is_type_ht_mimo2(type) ((type) == LQ_HT_MIMO2)
#define is_type_vht_siso(type) ((type) == LQ_VHT_SISO)
#define is_type_vht_mimo2(type) ((type) == LQ_VHT_MIMO2)
#define is_type_siso(type) (is_type_ht_siso(type) || is_type_vht_siso(type))
#define is_type_mimo2(type) (is_type_ht_mimo2(type) || is_type_vht_mimo2(type))
#define is_type_mimo(type) (is_type_mimo2(type))
#define is_type_ht(type) (is_type_ht_siso(type) || is_type_ht_mimo2(type))
#define is_type_vht(type) (is_type_vht_siso(type) || is_type_vht_mimo2(type))
#define is_type_a_band(type) ((type) == LQ_LEGACY_A)
#define is_type_g_band(type) ((type) == LQ_LEGACY_G)

#define is_legacy(rate)       is_type_legacy((rate)->type)
#define is_ht_siso(rate)      is_type_ht_siso((rate)->type)
#define is_ht_mimo2(rate)     is_type_ht_mimo2((rate)->type)
#define is_vht_siso(rate)     is_type_vht_siso((rate)->type)
#define is_vht_mimo2(rate)    is_type_vht_mimo2((rate)->type)
#define is_siso(rate)         is_type_siso((rate)->type)
#define is_mimo2(rate)        is_type_mimo2((rate)->type)
#define is_mimo(rate)         is_type_mimo((rate)->type)
#define is_ht(rate)           is_type_ht((rate)->type)
#define is_vht(rate)          is_type_vht((rate)->type)
#define is_a_band(rate)       is_type_a_band((rate)->type)
#define is_g_band(rate)       is_type_g_band((rate)->type)

#define is_ht20(rate)         ((rate)->bw == RATE_MCS_CHAN_WIDTH_20)
#define is_ht40(rate)         ((rate)->bw == RATE_MCS_CHAN_WIDTH_40)
#define is_ht80(rate)         ((rate)->bw == RATE_MCS_CHAN_WIDTH_80)

#define IWL_MAX_MCS_DISPLAY_SIZE	12

struct iwl_rate_mcs_info {
	char	mbps[IWL_MAX_MCS_DISPLAY_SIZE];
	char	mcs[IWL_MAX_MCS_DISPLAY_SIZE];
};

/**
 * struct iwl_rate_scale_data -- tx success history for one rate
 */
struct iwl_rate_scale_data {
	u64 data;		/* bitmap of successful frames */
	s32 success_counter;	/* number of frames successful */
	s32 success_ratio;	/* per-cent * 128  */
	s32 counter;		/* number of frames attempted */
	s32 average_tpt;	/* success ratio * expected throughput */
};

/* Possible Tx columns
 * Tx Column = a combo of legacy/siso/mimo x antenna x SGI
 */
enum rs_column {
	RS_COLUMN_LEGACY_ANT_A = 0,
	RS_COLUMN_LEGACY_ANT_B,
	RS_COLUMN_SISO_ANT_A,
	RS_COLUMN_SISO_ANT_B,
	RS_COLUMN_SISO_ANT_A_SGI,
	RS_COLUMN_SISO_ANT_B_SGI,
	RS_COLUMN_MIMO2,
	RS_COLUMN_MIMO2_SGI,

	RS_COLUMN_LAST = RS_COLUMN_MIMO2_SGI,
	RS_COLUMN_COUNT = RS_COLUMN_LAST + 1,
	RS_COLUMN_INVALID,
};

/* Packet stats per rate */
struct rs_rate_stats {
	u64 success;
	u64 total;
};

/**
 * struct iwl_scale_tbl_info -- tx params and success history for all rates
 *
 * There are two of these in struct iwl_lq_sta,
 * one for "active", and one for "search".
 */
struct iwl_scale_tbl_info {
	struct rs_rate rate;
	enum rs_column column;
	const u16 *expected_tpt;	/* throughput metrics; expected_tpt_G, etc. */
	struct iwl_rate_scale_data win[IWL_RATE_COUNT]; /* rate histories */
	/* per txpower-reduction history */
	struct iwl_rate_scale_data tpc_win[TPC_MAX_REDUCTION + 1];
};

enum {
	RS_STATE_SEARCH_CYCLE_STARTED,
	RS_STATE_SEARCH_CYCLE_ENDED,
	RS_STATE_STAY_IN_COLUMN,
};

/**
 * struct iwl_lq_sta -- driver's rate scaling private structure
 *
 * Pointer to this gets passed back and forth between driver and mac80211.
 */
struct iwl_lq_sta {
	u8 active_tbl;		/* index of active table, range 0-1 */
	u8 rs_state;            /* RS_STATE_* */
	u8 search_better_tbl;	/* 1: currently trying alternate mode */
	s32 last_tpt;

	/* The following determine when to search for a new mode */
	u32 table_count_limit;
	u32 max_failure_limit;	/* # failed frames before new search */
	u32 max_success_limit;	/* # successful frames before new search */
	u32 table_count;
	u32 total_failed;	/* total failed frames, any/all rates */
	u32 total_success;	/* total successful frames, any/all rates */
	u64 flush_timer;	/* time staying in mode before new search */

	u32 visited_columns;    /* Bitmask marking which Tx columns were
				 * explored during a search cycle
				 */
	u64 last_tx;
	bool is_vht;
	enum ieee80211_band band;

	struct rs_rate_stats tx_stats[RS_COLUMN_COUNT][IWL_RATE_COUNT];

	/* The following are bitmaps of rates; IWL_RATE_6M_MASK, etc. */
	unsigned long active_legacy_rate;
	unsigned long active_siso_rate;
	unsigned long active_mimo2_rate;

	/* Highest rate per Tx mode */
	u8 max_legacy_rate_idx;
	u8 max_siso_rate_idx;
	u8 max_mimo2_rate_idx;

	s8 max_rate_idx;     /* Max rate set by user */
	u8 missed_rate_counter;

	struct iwl_lq_cmd lq;
	struct iwl_scale_tbl_info lq_info[LQ_SIZE]; /* "active", "search" */
	u8 tx_agg_tid_en;
#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *rs_sta_dbgfs_scale_table_file;
	struct dentry *rs_sta_dbgfs_stats_table_file;
	struct dentry *rs_sta_dbgfs_drv_tx_stats_file;
	struct dentry *rs_sta_dbgfs_tx_agg_tid_en_file;
	struct dentry *rs_sta_dbgfs_reduced_txp_file;
	u32 dbg_fixed_rate;
	u8 dbg_fixed_txp_reduction;
#endif
	struct iwl_mvm *drv;

	/* used to be in sta_info */
	int last_txrate_idx;
	/* last tx rate_n_flags */
	u32 last_rate_n_flags;
	/* packets destined for this STA are aggregated */
	u8 is_agg;

	/* tx power reduce for this sta */
	int tpc_reduce;
};

/* Initialize station's rate scaling information after adding station */
void iwl_mvm_rs_rate_init(struct iwl_mvm *mvm, struct ieee80211_sta *sta,
			  enum ieee80211_band band, bool init);

/**
 * iwl_rate_control_register - Register the rate control algorithm callbacks
 *
 * Since the rate control algorithm is hardware specific, there is no need
 * or reason to place it as a stand alone module.  The driver can call
 * iwl_rate_control_register in order to register the rate control callbacks
 * with the mac80211 subsystem.  This should be performed prior to calling
 * ieee80211_register_hw
 *
 */
int iwl_mvm_rate_control_register(void);

/**
 * iwl_rate_control_unregister - Unregister the rate control callbacks
 *
 * This should be called after calling ieee80211_unregister_hw, but before
 * the driver is unloaded.
 */
void iwl_mvm_rate_control_unregister(void);

struct iwl_mvm_sta;

int iwl_mvm_tx_protection(struct iwl_mvm *mvm, struct iwl_mvm_sta *mvmsta,
			  bool enable);

#endif /* __rs__ */
