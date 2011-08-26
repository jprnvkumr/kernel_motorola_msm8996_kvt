/******************************************************************************
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>

#include "iwl-dev.h"
#include "iwl-core.h"
#include "iwl-4965-calib.h"
#include "iwl-sta.h"
#include "iwl-io.h"
#include "iwl-helpers.h"
#include "iwl-4965-hw.h"
#include "iwl-4965.h"

void il4965_rx_missed_beacon_notif(struct il_priv *il,
				struct il_rx_buf *rxb)

{
	struct il_rx_pkt *pkt = rxb_addr(rxb);
	struct il_missed_beacon_notif *missed_beacon;

	missed_beacon = &pkt->u.missed_beacon;
	if (le32_to_cpu(missed_beacon->consecutive_missed_beacons) >
	    il->missed_beacon_threshold) {
		D_CALIB(
		    "missed bcn cnsq %d totl %d rcd %d expctd %d\n",
		    le32_to_cpu(missed_beacon->consecutive_missed_beacons),
		    le32_to_cpu(missed_beacon->total_missed_becons),
		    le32_to_cpu(missed_beacon->num_recvd_beacons),
		    le32_to_cpu(missed_beacon->num_expected_beacons));
		if (!test_bit(STATUS_SCANNING, &il->status))
			il4965_init_sensitivity(il);
	}
}

/* Calculate noise level, based on measurements during network silence just
 *   before arriving beacon.  This measurement can be done only if we know
 *   exactly when to expect beacons, therefore only when we're associated. */
static void il4965_rx_calc_noise(struct il_priv *il)
{
	struct stats_rx_non_phy *rx_info;
	int num_active_rx = 0;
	int total_silence = 0;
	int bcn_silence_a, bcn_silence_b, bcn_silence_c;
	int last_rx_noise;

	rx_info = &(il->_4965.stats.rx.general);
	bcn_silence_a =
		le32_to_cpu(rx_info->beacon_silence_rssi_a) & IN_BAND_FILTER;
	bcn_silence_b =
		le32_to_cpu(rx_info->beacon_silence_rssi_b) & IN_BAND_FILTER;
	bcn_silence_c =
		le32_to_cpu(rx_info->beacon_silence_rssi_c) & IN_BAND_FILTER;

	if (bcn_silence_a) {
		total_silence += bcn_silence_a;
		num_active_rx++;
	}
	if (bcn_silence_b) {
		total_silence += bcn_silence_b;
		num_active_rx++;
	}
	if (bcn_silence_c) {
		total_silence += bcn_silence_c;
		num_active_rx++;
	}

	/* Average among active antennas */
	if (num_active_rx)
		last_rx_noise = (total_silence / num_active_rx) - 107;
	else
		last_rx_noise = IL_NOISE_MEAS_NOT_AVAILABLE;

	D_CALIB("inband silence a %u, b %u, c %u, dBm %d\n",
			bcn_silence_a, bcn_silence_b, bcn_silence_c,
			last_rx_noise);
}

#ifdef CONFIG_IWLEGACY_DEBUGFS
/*
 *  based on the assumption of all stats counter are in DWORD
 *  FIXME: This function is for debugging, do not deal with
 *  the case of counters roll-over.
 */
static void il4965_accumulative_stats(struct il_priv *il,
					__le32 *stats)
{
	int i, size;
	__le32 *prev_stats;
	u32 *accum_stats;
	u32 *delta, *max_delta;
	struct stats_general_common *general, *accum_general;
	struct stats_tx *tx, *accum_tx;

	prev_stats = (__le32 *)&il->_4965.stats;
	accum_stats = (u32 *)&il->_4965.accum_stats;
	size = sizeof(struct il_notif_stats);
	general = &il->_4965.stats.general.common;
	accum_general = &il->_4965.accum_stats.general.common;
	tx = &il->_4965.stats.tx;
	accum_tx = &il->_4965.accum_stats.tx;
	delta = (u32 *)&il->_4965.delta_stats;
	max_delta = (u32 *)&il->_4965.max_delta;

	for (i = sizeof(__le32); i < size;
	     i += sizeof(__le32), stats++, prev_stats++, delta++,
	     max_delta++, accum_stats++) {
		if (le32_to_cpu(*stats) > le32_to_cpu(*prev_stats)) {
			*delta = (le32_to_cpu(*stats) -
				le32_to_cpu(*prev_stats));
			*accum_stats += *delta;
			if (*delta > *max_delta)
				*max_delta = *delta;
		}
	}

	/* reset accumulative stats for "no-counter" type stats */
	accum_general->temperature = general->temperature;
	accum_general->ttl_timestamp = general->ttl_timestamp;
}
#endif

#define REG_RECALIB_PERIOD (60)

void il4965_rx_stats(struct il_priv *il,
			      struct il_rx_buf *rxb)
{
	int change;
	struct il_rx_pkt *pkt = rxb_addr(rxb);

	D_RX(
		     "Statistics notification received (%d vs %d).\n",
		     (int)sizeof(struct il_notif_stats),
		     le32_to_cpu(pkt->len_n_flags) &
		     FH_RSCSR_FRAME_SIZE_MSK);

	change = ((il->_4965.stats.general.common.temperature !=
		   pkt->u.stats.general.common.temperature) ||
		   ((il->_4965.stats.flag &
		   STATISTICS_REPLY_FLG_HT40_MODE_MSK) !=
		   (pkt->u.stats.flag &
		   STATISTICS_REPLY_FLG_HT40_MODE_MSK)));
#ifdef CONFIG_IWLEGACY_DEBUGFS
	il4965_accumulative_stats(il, (__le32 *)&pkt->u.stats);
#endif

	/* TODO: reading some of stats is unneeded */
	memcpy(&il->_4965.stats, &pkt->u.stats,
		sizeof(il->_4965.stats));

	set_bit(STATUS_STATISTICS, &il->status);

	/* Reschedule the stats timer to occur in
	 * REG_RECALIB_PERIOD seconds to ensure we get a
	 * thermal update even if the uCode doesn't give
	 * us one */
	mod_timer(&il->stats_periodic, jiffies +
		  msecs_to_jiffies(REG_RECALIB_PERIOD * 1000));

	if (unlikely(!test_bit(STATUS_SCANNING, &il->status)) &&
	    (pkt->hdr.cmd == STATISTICS_NOTIFICATION)) {
		il4965_rx_calc_noise(il);
		queue_work(il->workqueue, &il->run_time_calib_work);
	}
	if (il->cfg->ops->lib->temp_ops.temperature && change)
		il->cfg->ops->lib->temp_ops.temperature(il);
}

void il4965_reply_stats(struct il_priv *il,
			      struct il_rx_buf *rxb)
{
	struct il_rx_pkt *pkt = rxb_addr(rxb);

	if (le32_to_cpu(pkt->u.stats.flag) & UCODE_STATISTICS_CLEAR_MSK) {
#ifdef CONFIG_IWLEGACY_DEBUGFS
		memset(&il->_4965.accum_stats, 0,
			sizeof(struct il_notif_stats));
		memset(&il->_4965.delta_stats, 0,
			sizeof(struct il_notif_stats));
		memset(&il->_4965.max_delta, 0,
			sizeof(struct il_notif_stats));
#endif
		D_RX("Statistics have been cleared\n");
	}
	il4965_rx_stats(il, rxb);
}
