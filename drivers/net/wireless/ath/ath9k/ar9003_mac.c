/*
 * Copyright (c) 2010 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "hw.h"

static void ar9003_hw_rx_enable(struct ath_hw *hw)
{
	REG_WRITE(hw, AR_CR, 0);
}

static void ar9003_hw_set_desc_link(void *ds, u32 ds_link)
{
	((struct ar9003_txc *) ds)->link = ds_link;
}

static void ar9003_hw_get_desc_link(void *ds, u32 **ds_link)
{
	*ds_link = &((struct ar9003_txc *) ds)->link;
}

static bool ar9003_hw_get_isr(struct ath_hw *ah, enum ath9k_int *masked)
{
	u32 isr = 0;
	u32 mask2 = 0;
	struct ath9k_hw_capabilities *pCap = &ah->caps;
	u32 sync_cause = 0;
	struct ath_common *common = ath9k_hw_common(ah);

	if (REG_READ(ah, AR_INTR_ASYNC_CAUSE) & AR_INTR_MAC_IRQ) {
		if ((REG_READ(ah, AR_RTC_STATUS) & AR_RTC_STATUS_M)
				== AR_RTC_STATUS_ON)
			isr = REG_READ(ah, AR_ISR);
	}

	sync_cause = REG_READ(ah, AR_INTR_SYNC_CAUSE) & AR_INTR_SYNC_DEFAULT;

	*masked = 0;

	if (!isr && !sync_cause)
		return false;

	if (isr) {
		if (isr & AR_ISR_BCNMISC) {
			u32 isr2;
			isr2 = REG_READ(ah, AR_ISR_S2);

			mask2 |= ((isr2 & AR_ISR_S2_TIM) >>
				  MAP_ISR_S2_TIM);
			mask2 |= ((isr2 & AR_ISR_S2_DTIM) >>
				  MAP_ISR_S2_DTIM);
			mask2 |= ((isr2 & AR_ISR_S2_DTIMSYNC) >>
				  MAP_ISR_S2_DTIMSYNC);
			mask2 |= ((isr2 & AR_ISR_S2_CABEND) >>
				  MAP_ISR_S2_CABEND);
			mask2 |= ((isr2 & AR_ISR_S2_GTT) <<
				  MAP_ISR_S2_GTT);
			mask2 |= ((isr2 & AR_ISR_S2_CST) <<
				  MAP_ISR_S2_CST);
			mask2 |= ((isr2 & AR_ISR_S2_TSFOOR) >>
				  MAP_ISR_S2_TSFOOR);

			if (!(pCap->hw_caps & ATH9K_HW_CAP_RAC_SUPPORTED)) {
				REG_WRITE(ah, AR_ISR_S2, isr2);
				isr &= ~AR_ISR_BCNMISC;
			}
		}

		if ((pCap->hw_caps & ATH9K_HW_CAP_RAC_SUPPORTED))
			isr = REG_READ(ah, AR_ISR_RAC);

		if (isr == 0xffffffff) {
			*masked = 0;
			return false;
		}

		*masked = isr & ATH9K_INT_COMMON;

		if (ah->config.rx_intr_mitigation)
			if (isr & (AR_ISR_RXMINTR | AR_ISR_RXINTM))
				*masked |= ATH9K_INT_RXLP;

		if (ah->config.tx_intr_mitigation)
			if (isr & (AR_ISR_TXMINTR | AR_ISR_TXINTM))
				*masked |= ATH9K_INT_TX;

		if (isr & (AR_ISR_LP_RXOK | AR_ISR_RXERR))
			*masked |= ATH9K_INT_RXLP;

		if (isr & AR_ISR_HP_RXOK)
			*masked |= ATH9K_INT_RXHP;

		if (isr & (AR_ISR_TXOK | AR_ISR_TXERR | AR_ISR_TXEOL)) {
			*masked |= ATH9K_INT_TX;

			if (!(pCap->hw_caps & ATH9K_HW_CAP_RAC_SUPPORTED)) {
				u32 s0, s1;
				s0 = REG_READ(ah, AR_ISR_S0);
				REG_WRITE(ah, AR_ISR_S0, s0);
				s1 = REG_READ(ah, AR_ISR_S1);
				REG_WRITE(ah, AR_ISR_S1, s1);

				isr &= ~(AR_ISR_TXOK | AR_ISR_TXERR |
					 AR_ISR_TXEOL);
			}
		}

		if (isr & AR_ISR_GENTMR) {
			u32 s5;

			if (pCap->hw_caps & ATH9K_HW_CAP_RAC_SUPPORTED)
				s5 = REG_READ(ah, AR_ISR_S5_S);
			else
				s5 = REG_READ(ah, AR_ISR_S5);

			ah->intr_gen_timer_trigger =
				MS(s5, AR_ISR_S5_GENTIMER_TRIG);

			ah->intr_gen_timer_thresh =
				MS(s5, AR_ISR_S5_GENTIMER_THRESH);

			if (ah->intr_gen_timer_trigger)
				*masked |= ATH9K_INT_GENTIMER;

			if (!(pCap->hw_caps & ATH9K_HW_CAP_RAC_SUPPORTED)) {
				REG_WRITE(ah, AR_ISR_S5, s5);
				isr &= ~AR_ISR_GENTMR;
			}

		}

		*masked |= mask2;

		if (!(pCap->hw_caps & ATH9K_HW_CAP_RAC_SUPPORTED)) {
			REG_WRITE(ah, AR_ISR, isr);

			(void) REG_READ(ah, AR_ISR);
		}
	}

	if (sync_cause) {
		if (sync_cause & AR_INTR_SYNC_RADM_CPL_TIMEOUT) {
			REG_WRITE(ah, AR_RC, AR_RC_HOSTIF);
			REG_WRITE(ah, AR_RC, 0);
			*masked |= ATH9K_INT_FATAL;
		}

		if (sync_cause & AR_INTR_SYNC_LOCAL_TIMEOUT)
			ath_print(common, ATH_DBG_INTERRUPT,
				  "AR_INTR_SYNC_LOCAL_TIMEOUT\n");

			REG_WRITE(ah, AR_INTR_SYNC_CAUSE_CLR, sync_cause);
		(void) REG_READ(ah, AR_INTR_SYNC_CAUSE_CLR);

	}
	return true;
}

static void ar9003_hw_fill_txdesc(struct ath_hw *ah, void *ds, u32 seglen,
				  bool is_firstseg, bool is_lastseg,
				  const void *ds0, dma_addr_t buf_addr,
				  unsigned int qcu)
{
}

static int ar9003_hw_proc_txdesc(struct ath_hw *ah, void *ds,
				 struct ath_tx_status *ts)
{
	return 0;
}
static void ar9003_hw_set11n_txdesc(struct ath_hw *ah, void *ds,
			    u32 pktLen, enum ath9k_pkt_type type, u32 txPower,
			    u32 keyIx, enum ath9k_key_type keyType, u32 flags)
{

}

static void ar9003_hw_set11n_ratescenario(struct ath_hw *ah, void *ds,
					  void *lastds,
					  u32 durUpdateEn, u32 rtsctsRate,
					  u32 rtsctsDuration,
					  struct ath9k_11n_rate_series series[],
					  u32 nseries, u32 flags)
{

}

static void ar9003_hw_set11n_aggr_first(struct ath_hw *ah, void *ds,
					u32 aggrLen)
{

}

static void ar9003_hw_set11n_aggr_middle(struct ath_hw *ah, void *ds,
					 u32 numDelims)
{

}

static void ar9003_hw_set11n_aggr_last(struct ath_hw *ah, void *ds)
{

}

static void ar9003_hw_clr11n_aggr(struct ath_hw *ah, void *ds)
{

}

static void ar9003_hw_set11n_burstduration(struct ath_hw *ah, void *ds,
					   u32 burstDuration)
{

}

static void ar9003_hw_set11n_virtualmorefrag(struct ath_hw *ah, void *ds,
					    u32 vmf)
{

}

void ar9003_hw_attach_mac_ops(struct ath_hw *hw)
{
	struct ath_hw_ops *ops = ath9k_hw_ops(hw);

	ops->rx_enable = ar9003_hw_rx_enable;
	ops->set_desc_link = ar9003_hw_set_desc_link;
	ops->get_desc_link = ar9003_hw_get_desc_link;
	ops->get_isr = ar9003_hw_get_isr;
	ops->fill_txdesc = ar9003_hw_fill_txdesc;
	ops->proc_txdesc = ar9003_hw_proc_txdesc;
	ops->set11n_txdesc = ar9003_hw_set11n_txdesc;
	ops->set11n_ratescenario = ar9003_hw_set11n_ratescenario;
	ops->set11n_aggr_first = ar9003_hw_set11n_aggr_first;
	ops->set11n_aggr_middle = ar9003_hw_set11n_aggr_middle;
	ops->set11n_aggr_last = ar9003_hw_set11n_aggr_last;
	ops->clr11n_aggr = ar9003_hw_clr11n_aggr;
	ops->set11n_burstduration = ar9003_hw_set11n_burstduration;
	ops->set11n_virtualmorefrag = ar9003_hw_set11n_virtualmorefrag;
}

void ath9k_hw_set_rx_bufsize(struct ath_hw *ah, u16 buf_size)
{
	REG_WRITE(ah, AR_DATABUF_SIZE, buf_size & AR_DATABUF_SIZE_MASK);
}
EXPORT_SYMBOL(ath9k_hw_set_rx_bufsize);

void ath9k_hw_addrxbuf_edma(struct ath_hw *ah, u32 rxdp,
			    enum ath9k_rx_qtype qtype)
{
	if (qtype == ATH9K_RX_QUEUE_HP)
		REG_WRITE(ah, AR_HP_RXDP, rxdp);
	else
		REG_WRITE(ah, AR_LP_RXDP, rxdp);
}
EXPORT_SYMBOL(ath9k_hw_addrxbuf_edma);

int ath9k_hw_process_rxdesc_edma(struct ath_hw *ah, struct ath_rx_status *rxs,
				 void *buf_addr)
{
	struct ar9003_rxs *rxsp = (struct ar9003_rxs *) buf_addr;
	unsigned int phyerr;

	/* TODO: byte swap on big endian for ar9300_10 */

	if ((rxsp->status11 & AR_RxDone) == 0)
		return -EINPROGRESS;

	if (MS(rxsp->ds_info, AR_DescId) != 0x168c)
		return -EINVAL;

	if ((rxsp->ds_info & (AR_TxRxDesc | AR_CtrlStat)) != 0)
		return -EINPROGRESS;

	if (!rxs)
		return 0;

	rxs->rs_status = 0;
	rxs->rs_flags =  0;

	rxs->rs_datalen = rxsp->status2 & AR_DataLen;
	rxs->rs_tstamp =  rxsp->status3;

	/* XXX: Keycache */
	rxs->rs_rssi = MS(rxsp->status5, AR_RxRSSICombined);
	rxs->rs_rssi_ctl0 = MS(rxsp->status1, AR_RxRSSIAnt00);
	rxs->rs_rssi_ctl1 = MS(rxsp->status1, AR_RxRSSIAnt01);
	rxs->rs_rssi_ctl2 = MS(rxsp->status1, AR_RxRSSIAnt02);
	rxs->rs_rssi_ext0 = MS(rxsp->status5, AR_RxRSSIAnt10);
	rxs->rs_rssi_ext1 = MS(rxsp->status5, AR_RxRSSIAnt11);
	rxs->rs_rssi_ext2 = MS(rxsp->status5, AR_RxRSSIAnt12);

	if (rxsp->status11 & AR_RxKeyIdxValid)
		rxs->rs_keyix = MS(rxsp->status11, AR_KeyIdx);
	else
		rxs->rs_keyix = ATH9K_RXKEYIX_INVALID;

	rxs->rs_rate = MS(rxsp->status1, AR_RxRate);
	rxs->rs_more = (rxsp->status2 & AR_RxMore) ? 1 : 0;

	rxs->rs_isaggr = (rxsp->status11 & AR_RxAggr) ? 1 : 0;
	rxs->rs_moreaggr = (rxsp->status11 & AR_RxMoreAggr) ? 1 : 0;
	rxs->rs_antenna = (MS(rxsp->status4, AR_RxAntenna) & 0x7);
	rxs->rs_flags  = (rxsp->status4 & AR_GI) ? ATH9K_RX_GI : 0;
	rxs->rs_flags  |= (rxsp->status4 & AR_2040) ? ATH9K_RX_2040 : 0;

	rxs->evm0 = rxsp->status6;
	rxs->evm1 = rxsp->status7;
	rxs->evm2 = rxsp->status8;
	rxs->evm3 = rxsp->status9;
	rxs->evm4 = (rxsp->status10 & 0xffff);

	if (rxsp->status11 & AR_PreDelimCRCErr)
		rxs->rs_flags |= ATH9K_RX_DELIM_CRC_PRE;

	if (rxsp->status11 & AR_PostDelimCRCErr)
		rxs->rs_flags |= ATH9K_RX_DELIM_CRC_POST;

	if (rxsp->status11 & AR_DecryptBusyErr)
		rxs->rs_flags |= ATH9K_RX_DECRYPT_BUSY;

	if ((rxsp->status11 & AR_RxFrameOK) == 0) {
		if (rxsp->status11 & AR_CRCErr) {
			rxs->rs_status |= ATH9K_RXERR_CRC;
		} else if (rxsp->status11 & AR_PHYErr) {
			rxs->rs_status |= ATH9K_RXERR_PHY;
			phyerr = MS(rxsp->status11, AR_PHYErrCode);
			rxs->rs_phyerr = phyerr;
		} else if (rxsp->status11 & AR_DecryptCRCErr) {
			rxs->rs_status |= ATH9K_RXERR_DECRYPT;
		} else if (rxsp->status11 & AR_MichaelErr) {
			rxs->rs_status |= ATH9K_RXERR_MIC;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath9k_hw_process_rxdesc_edma);
