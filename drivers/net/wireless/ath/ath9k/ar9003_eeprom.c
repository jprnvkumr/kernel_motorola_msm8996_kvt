/*
 * Copyright (c) 2010-2011 Atheros Communications Inc.
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
#include "ar9003_phy.h"
#include "ar9003_eeprom.h"

#define COMP_HDR_LEN 4
#define COMP_CKSUM_LEN 2

#define AR_CH0_TOP (0x00016288)
#define AR_CH0_TOP_XPABIASLVL (0x300)
#define AR_CH0_TOP_XPABIASLVL_S (8)

#define AR_CH0_THERM (0x00016290)
#define AR_CH0_THERM_XPABIASLVL_MSB 0x3
#define AR_CH0_THERM_XPABIASLVL_MSB_S 0
#define AR_CH0_THERM_XPASHORT2GND 0x4
#define AR_CH0_THERM_XPASHORT2GND_S 2

#define AR_SWITCH_TABLE_COM_ALL (0xffff)
#define AR_SWITCH_TABLE_COM_ALL_S (0)

#define AR_SWITCH_TABLE_COM2_ALL (0xffffff)
#define AR_SWITCH_TABLE_COM2_ALL_S (0)

#define AR_SWITCH_TABLE_ALL (0xfff)
#define AR_SWITCH_TABLE_ALL_S (0)

#define LE16(x) __constant_cpu_to_le16(x)
#define LE32(x) __constant_cpu_to_le32(x)

/* Local defines to distinguish between extension and control CTL's */
#define EXT_ADDITIVE (0x8000)
#define CTL_11A_EXT (CTL_11A | EXT_ADDITIVE)
#define CTL_11G_EXT (CTL_11G | EXT_ADDITIVE)
#define CTL_11B_EXT (CTL_11B | EXT_ADDITIVE)
#define REDUCE_SCALED_POWER_BY_TWO_CHAIN     6  /* 10*log10(2)*2 */
#define REDUCE_SCALED_POWER_BY_THREE_CHAIN   9  /* 10*log10(3)*2 */
#define PWRINCR_3_TO_1_CHAIN      9             /* 10*log(3)*2 */
#define PWRINCR_3_TO_2_CHAIN      3             /* floor(10*log(3/2)*2) */
#define PWRINCR_2_TO_1_CHAIN      6             /* 10*log(2)*2 */

#define SUB_NUM_CTL_MODES_AT_5G_40 2    /* excluding HT40, EXT-OFDM */
#define SUB_NUM_CTL_MODES_AT_2G_40 3    /* excluding HT40, EXT-OFDM, EXT-CCK */

#define CTL(_tpower, _flag) ((_tpower) | ((_flag) << 6))

#define EEPROM_DATA_LEN_9485	1088

static int ar9003_hw_power_interpolate(int32_t x,
				       int32_t *px, int32_t *py, u_int16_t np);


static const struct ar9300_eeprom ar9300_default = {
	.eepromVersion = 2,
	.templateVersion = 2,
	.macAddr = {1, 2, 3, 4, 5, 6},
	.custData = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		     0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	.baseEepHeader = {
		.regDmn = { LE16(0), LE16(0x1f) },
		.txrxMask =  0x77, /* 4 bits tx and 4 bits rx */
		.opCapFlags = {
			.opFlags = AR5416_OPFLAGS_11G | AR5416_OPFLAGS_11A,
			.eepMisc = 0,
		},
		.rfSilent = 0,
		.blueToothOptions = 0,
		.deviceCap = 0,
		.deviceType = 5, /* takes lower byte in eeprom location */
		.pwrTableOffset = AR9300_PWR_TABLE_OFFSET,
		.params_for_tuning_caps = {0, 0},
		.featureEnable = 0x0c,
		 /*
		  * bit0 - enable tx temp comp - disabled
		  * bit1 - enable tx volt comp - disabled
		  * bit2 - enable fastClock - enabled
		  * bit3 - enable doubling - enabled
		  * bit4 - enable internal regulator - disabled
		  * bit5 - enable pa predistortion - disabled
		  */
		.miscConfiguration = 0, /* bit0 - turn down drivestrength */
		.eepromWriteEnableGpio = 3,
		.wlanDisableGpio = 0,
		.wlanLedGpio = 8,
		.rxBandSelectGpio = 0xff,
		.txrxgain = 0,
		.swreg = 0,
	 },
	.modalHeader2G = {
	/* ar9300_modal_eep_header  2g */
		/* 4 idle,t1,t2,b(4 bits per setting) */
		.antCtrlCommon = LE32(0x110),
		/* 4 ra1l1, ra2l1, ra1l2, ra2l2, ra12 */
		.antCtrlCommon2 = LE32(0x22222),

		/*
		 * antCtrlChain[AR9300_MAX_CHAINS]; 6 idle, t, r,
		 * rx1, rx12, b (2 bits each)
		 */
		.antCtrlChain = { LE16(0x150), LE16(0x150), LE16(0x150) },

		/*
		 * xatten1DB[AR9300_MAX_CHAINS];  3 xatten1_db
		 * for ar9280 (0xa20c/b20c 5:0)
		 */
		.xatten1DB = {0, 0, 0},

		/*
		 * xatten1Margin[AR9300_MAX_CHAINS]; 3 xatten1_margin
		 * for ar9280 (0xa20c/b20c 16:12
		 */
		.xatten1Margin = {0, 0, 0},
		.tempSlope = 36,
		.voltSlope = 0,

		/*
		 * spurChans[OSPREY_EEPROM_MODAL_SPURS]; spur
		 * channels in usual fbin coding format
		 */
		.spurChans = {0, 0, 0, 0, 0},

		/*
		 * noiseFloorThreshCh[AR9300_MAX_CHAINS]; 3 Check
		 * if the register is per chain
		 */
		.noiseFloorThreshCh = {-1, 0, 0},
		.ob = {1, 1, 1},/* 3 chain */
		.db_stage2 = {1, 1, 1}, /* 3 chain  */
		.db_stage3 = {0, 0, 0},
		.db_stage4 = {0, 0, 0},
		.xpaBiasLvl = 0,
		.txFrameToDataStart = 0x0e,
		.txFrameToPaOn = 0x0e,
		.txClip = 3, /* 4 bits tx_clip, 4 bits dac_scale_cck */
		.antennaGain = 0,
		.switchSettling = 0x2c,
		.adcDesiredSize = -30,
		.txEndToXpaOff = 0,
		.txEndToRxOn = 0x2,
		.txFrameToXpaOn = 0xe,
		.thresh62 = 28,
		.papdRateMaskHt20 = LE32(0x0cf0e0e0),
		.papdRateMaskHt40 = LE32(0x6cf0e0e0),
		.futureModal = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		},
	 },
	.base_ext1 = {
		.ant_div_control = 0,
		.future = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	},
	.calFreqPier2G = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1),
	 },
	/* ar9300_cal_data_per_freq_op_loop 2g */
	.calPierData2G = {
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
	 },
	.calTarget_freqbin_Cck = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2484, 1),
	 },
	.calTarget_freqbin_2G = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	 },
	.calTarget_freqbin_2GHT20 = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	 },
	.calTarget_freqbin_2GHT40 = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	 },
	.calTargetPowerCck = {
		 /* 1L-5L,5S,11L,11S */
		 { {36, 36, 36, 36} },
		 { {36, 36, 36, 36} },
	},
	.calTargetPower2G = {
		 /* 6-24,36,48,54 */
		 { {32, 32, 28, 24} },
		 { {32, 32, 28, 24} },
		 { {32, 32, 28, 24} },
	},
	.calTargetPower2GHT20 = {
		{ {32, 32, 32, 32, 28, 20, 32, 32, 28, 20, 32, 32, 28, 20} },
		{ {32, 32, 32, 32, 28, 20, 32, 32, 28, 20, 32, 32, 28, 20} },
		{ {32, 32, 32, 32, 28, 20, 32, 32, 28, 20, 32, 32, 28, 20} },
	},
	.calTargetPower2GHT40 = {
		{ {32, 32, 32, 32, 28, 20, 32, 32, 28, 20, 32, 32, 28, 20} },
		{ {32, 32, 32, 32, 28, 20, 32, 32, 28, 20, 32, 32, 28, 20} },
		{ {32, 32, 32, 32, 28, 20, 32, 32, 28, 20, 32, 32, 28, 20} },
	},
	.ctlIndex_2G =  {
		0x11, 0x12, 0x15, 0x17, 0x41, 0x42,
		0x45, 0x47, 0x31, 0x32, 0x35, 0x37,
	},
	.ctl_freqbin_2G = {
		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2457, 1),
			FREQ2FBIN(2462, 1)
		},
		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2462, 1),
			0xFF,
		},

		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2462, 1),
			0xFF,
		},
		{
			FREQ2FBIN(2422, 1),
			FREQ2FBIN(2427, 1),
			FREQ2FBIN(2447, 1),
			FREQ2FBIN(2452, 1)
		},

		{
			/* Data[4].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[4].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[4].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			/* Data[4].ctlEdges[3].bChannel */ FREQ2FBIN(2484, 1),
		},

		{
			/* Data[5].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[5].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[5].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			0,
		},

		{
			/* Data[6].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[6].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			FREQ2FBIN(2472, 1),
			0,
		},

		{
			/* Data[7].ctlEdges[0].bChannel */ FREQ2FBIN(2422, 1),
			/* Data[7].ctlEdges[1].bChannel */ FREQ2FBIN(2427, 1),
			/* Data[7].ctlEdges[2].bChannel */ FREQ2FBIN(2447, 1),
			/* Data[7].ctlEdges[3].bChannel */ FREQ2FBIN(2462, 1),
		},

		{
			/* Data[8].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[8].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[8].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
		},

		{
			/* Data[9].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[9].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[9].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			0
		},

		{
			/* Data[10].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[10].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[10].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			0
		},

		{
			/* Data[11].ctlEdges[0].bChannel */ FREQ2FBIN(2422, 1),
			/* Data[11].ctlEdges[1].bChannel */ FREQ2FBIN(2427, 1),
			/* Data[11].ctlEdges[2].bChannel */ FREQ2FBIN(2447, 1),
			/* Data[11].ctlEdges[3].bChannel */ FREQ2FBIN(2462, 1),
		}
	 },
	.ctlPowerData_2G = {
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 1) } },

		 { { CTL(60, 1), CTL(60, 0), CTL(0, 0), CTL(0, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },

		 { { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },

		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 1) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 1) } },
	 },
	.modalHeader5G = {
		/* 4 idle,t1,t2,b (4 bits per setting) */
		.antCtrlCommon = LE32(0x110),
		/* 4 ra1l1, ra2l1, ra1l2,ra2l2,ra12 */
		.antCtrlCommon2 = LE32(0x22222),
		 /* antCtrlChain 6 idle, t,r,rx1,rx12,b (2 bits each) */
		.antCtrlChain = {
			LE16(0x000), LE16(0x000), LE16(0x000),
		},
		 /* xatten1DB 3 xatten1_db for AR9280 (0xa20c/b20c 5:0) */
		.xatten1DB = {0, 0, 0},

		/*
		 * xatten1Margin[AR9300_MAX_CHAINS]; 3 xatten1_margin
		 * for merlin (0xa20c/b20c 16:12
		 */
		.xatten1Margin = {0, 0, 0},
		.tempSlope = 68,
		.voltSlope = 0,
		/* spurChans spur channels in usual fbin coding format */
		.spurChans = {0, 0, 0, 0, 0},
		/* noiseFloorThreshCh Check if the register is per chain */
		.noiseFloorThreshCh = {-1, 0, 0},
		.ob = {3, 3, 3}, /* 3 chain */
		.db_stage2 = {3, 3, 3}, /* 3 chain */
		.db_stage3 = {3, 3, 3}, /* doesn't exist for 2G */
		.db_stage4 = {3, 3, 3},	 /* don't exist for 2G */
		.xpaBiasLvl = 0,
		.txFrameToDataStart = 0x0e,
		.txFrameToPaOn = 0x0e,
		.txClip = 3, /* 4 bits tx_clip, 4 bits dac_scale_cck */
		.antennaGain = 0,
		.switchSettling = 0x2d,
		.adcDesiredSize = -30,
		.txEndToXpaOff = 0,
		.txEndToRxOn = 0x2,
		.txFrameToXpaOn = 0xe,
		.thresh62 = 28,
		.papdRateMaskHt20 = LE32(0x0c80c080),
		.papdRateMaskHt40 = LE32(0x0080c080),
		.futureModal = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		},
	 },
	.base_ext2 = {
		.tempSlopeLow = 0,
		.tempSlopeHigh = 0,
		.xatten1DBLow = {0, 0, 0},
		.xatten1MarginLow = {0, 0, 0},
		.xatten1DBHigh = {0, 0, 0},
		.xatten1MarginHigh = {0, 0, 0}
	},
	.calFreqPier5G = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5220, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5600, 0),
		FREQ2FBIN(5725, 0),
		FREQ2FBIN(5825, 0)
	},
	.calPierData5G = {
			{
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
			},
			{
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
			},
			{
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
			},

	},
	.calTarget_freqbin_5G = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5220, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5600, 0),
		FREQ2FBIN(5725, 0),
		FREQ2FBIN(5825, 0)
	},
	.calTarget_freqbin_5GHT20 = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5240, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5700, 0),
		FREQ2FBIN(5745, 0),
		FREQ2FBIN(5725, 0),
		FREQ2FBIN(5825, 0)
	},
	.calTarget_freqbin_5GHT40 = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5240, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5700, 0),
		FREQ2FBIN(5745, 0),
		FREQ2FBIN(5725, 0),
		FREQ2FBIN(5825, 0)
	 },
	.calTargetPower5G = {
		/* 6-24,36,48,54 */
		{ {20, 20, 20, 10} },
		{ {20, 20, 20, 10} },
		{ {20, 20, 20, 10} },
		{ {20, 20, 20, 10} },
		{ {20, 20, 20, 10} },
		{ {20, 20, 20, 10} },
		{ {20, 20, 20, 10} },
		{ {20, 20, 20, 10} },
	 },
	.calTargetPower5GHT20 = {
		/*
		 * 0_8_16,1-3_9-11_17-19,
		 * 4,5,6,7,12,13,14,15,20,21,22,23
		 */
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
	 },
	.calTargetPower5GHT40 =  {
		/*
		 * 0_8_16,1-3_9-11_17-19,
		 * 4,5,6,7,12,13,14,15,20,21,22,23
		 */
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
		{ {20, 20, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10, 0, 0} },
	 },
	.ctlIndex_5G =  {
		0x10, 0x16, 0x18, 0x40, 0x46,
		0x48, 0x30, 0x36, 0x38
	},
	.ctl_freqbin_5G =  {
		{
			/* Data[0].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[0].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[0].ctlEdges[2].bChannel */ FREQ2FBIN(5280, 0),
			/* Data[0].ctlEdges[3].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[0].ctlEdges[4].bChannel */ FREQ2FBIN(5600, 0),
			/* Data[0].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[0].ctlEdges[6].bChannel */ FREQ2FBIN(5745, 0),
			/* Data[0].ctlEdges[7].bChannel */ FREQ2FBIN(5825, 0)
		},
		{
			/* Data[1].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[1].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[1].ctlEdges[2].bChannel */ FREQ2FBIN(5280, 0),
			/* Data[1].ctlEdges[3].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[1].ctlEdges[4].bChannel */ FREQ2FBIN(5520, 0),
			/* Data[1].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[1].ctlEdges[6].bChannel */ FREQ2FBIN(5745, 0),
			/* Data[1].ctlEdges[7].bChannel */ FREQ2FBIN(5825, 0)
		},

		{
			/* Data[2].ctlEdges[0].bChannel */ FREQ2FBIN(5190, 0),
			/* Data[2].ctlEdges[1].bChannel */ FREQ2FBIN(5230, 0),
			/* Data[2].ctlEdges[2].bChannel */ FREQ2FBIN(5270, 0),
			/* Data[2].ctlEdges[3].bChannel */ FREQ2FBIN(5310, 0),
			/* Data[2].ctlEdges[4].bChannel */ FREQ2FBIN(5510, 0),
			/* Data[2].ctlEdges[5].bChannel */ FREQ2FBIN(5550, 0),
			/* Data[2].ctlEdges[6].bChannel */ FREQ2FBIN(5670, 0),
			/* Data[2].ctlEdges[7].bChannel */ FREQ2FBIN(5755, 0)
		},

		{
			/* Data[3].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[3].ctlEdges[1].bChannel */ FREQ2FBIN(5200, 0),
			/* Data[3].ctlEdges[2].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[3].ctlEdges[3].bChannel */ FREQ2FBIN(5320, 0),
			/* Data[3].ctlEdges[4].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[3].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[3].ctlEdges[6].bChannel */ 0xFF,
			/* Data[3].ctlEdges[7].bChannel */ 0xFF,
		},

		{
			/* Data[4].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[4].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[4].ctlEdges[2].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[4].ctlEdges[3].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[4].ctlEdges[4].bChannel */ 0xFF,
			/* Data[4].ctlEdges[5].bChannel */ 0xFF,
			/* Data[4].ctlEdges[6].bChannel */ 0xFF,
			/* Data[4].ctlEdges[7].bChannel */ 0xFF,
		},

		{
			/* Data[5].ctlEdges[0].bChannel */ FREQ2FBIN(5190, 0),
			/* Data[5].ctlEdges[1].bChannel */ FREQ2FBIN(5270, 0),
			/* Data[5].ctlEdges[2].bChannel */ FREQ2FBIN(5310, 0),
			/* Data[5].ctlEdges[3].bChannel */ FREQ2FBIN(5510, 0),
			/* Data[5].ctlEdges[4].bChannel */ FREQ2FBIN(5590, 0),
			/* Data[5].ctlEdges[5].bChannel */ FREQ2FBIN(5670, 0),
			/* Data[5].ctlEdges[6].bChannel */ 0xFF,
			/* Data[5].ctlEdges[7].bChannel */ 0xFF
		},

		{
			/* Data[6].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[6].ctlEdges[1].bChannel */ FREQ2FBIN(5200, 0),
			/* Data[6].ctlEdges[2].bChannel */ FREQ2FBIN(5220, 0),
			/* Data[6].ctlEdges[3].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[6].ctlEdges[4].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[6].ctlEdges[5].bChannel */ FREQ2FBIN(5600, 0),
			/* Data[6].ctlEdges[6].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[6].ctlEdges[7].bChannel */ FREQ2FBIN(5745, 0)
		},

		{
			/* Data[7].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[7].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[7].ctlEdges[2].bChannel */ FREQ2FBIN(5320, 0),
			/* Data[7].ctlEdges[3].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[7].ctlEdges[4].bChannel */ FREQ2FBIN(5560, 0),
			/* Data[7].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[7].ctlEdges[6].bChannel */ FREQ2FBIN(5745, 0),
			/* Data[7].ctlEdges[7].bChannel */ FREQ2FBIN(5825, 0)
		},

		{
			/* Data[8].ctlEdges[0].bChannel */ FREQ2FBIN(5190, 0),
			/* Data[8].ctlEdges[1].bChannel */ FREQ2FBIN(5230, 0),
			/* Data[8].ctlEdges[2].bChannel */ FREQ2FBIN(5270, 0),
			/* Data[8].ctlEdges[3].bChannel */ FREQ2FBIN(5510, 0),
			/* Data[8].ctlEdges[4].bChannel */ FREQ2FBIN(5550, 0),
			/* Data[8].ctlEdges[5].bChannel */ FREQ2FBIN(5670, 0),
			/* Data[8].ctlEdges[6].bChannel */ FREQ2FBIN(5755, 0),
			/* Data[8].ctlEdges[7].bChannel */ FREQ2FBIN(5795, 0)
		}
	 },
	.ctlPowerData_5G = {
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
			}
		},
		{
			{
				CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 0),
				CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
				CTL(60, 0), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 0), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 0), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 0), CTL(60, 1),
			}
		},
	 }
};

static const struct ar9300_eeprom ar9300_x113 = {
	.eepromVersion = 2,
	.templateVersion = 6,
	.macAddr = {0x00, 0x03, 0x7f, 0x0, 0x0, 0x0},
	.custData = {"x113-023-f0000"},
	.baseEepHeader = {
		.regDmn = { LE16(0), LE16(0x1f) },
		.txrxMask =  0x77, /* 4 bits tx and 4 bits rx */
		.opCapFlags = {
			.opFlags = AR5416_OPFLAGS_11A,
			.eepMisc = 0,
		},
		.rfSilent = 0,
		.blueToothOptions = 0,
		.deviceCap = 0,
		.deviceType = 5, /* takes lower byte in eeprom location */
		.pwrTableOffset = AR9300_PWR_TABLE_OFFSET,
		.params_for_tuning_caps = {0, 0},
		.featureEnable = 0x0d,
		 /*
		  * bit0 - enable tx temp comp - disabled
		  * bit1 - enable tx volt comp - disabled
		  * bit2 - enable fastClock - enabled
		  * bit3 - enable doubling - enabled
		  * bit4 - enable internal regulator - disabled
		  * bit5 - enable pa predistortion - disabled
		  */
		.miscConfiguration = 0, /* bit0 - turn down drivestrength */
		.eepromWriteEnableGpio = 6,
		.wlanDisableGpio = 0,
		.wlanLedGpio = 8,
		.rxBandSelectGpio = 0xff,
		.txrxgain = 0x21,
		.swreg = 0,
	 },
	.modalHeader2G = {
	/* ar9300_modal_eep_header  2g */
		/* 4 idle,t1,t2,b(4 bits per setting) */
		.antCtrlCommon = LE32(0x110),
		/* 4 ra1l1, ra2l1, ra1l2, ra2l2, ra12 */
		.antCtrlCommon2 = LE32(0x44444),

		/*
		 * antCtrlChain[AR9300_MAX_CHAINS]; 6 idle, t, r,
		 * rx1, rx12, b (2 bits each)
		 */
		.antCtrlChain = { LE16(0x150), LE16(0x150), LE16(0x150) },

		/*
		 * xatten1DB[AR9300_MAX_CHAINS];  3 xatten1_db
		 * for ar9280 (0xa20c/b20c 5:0)
		 */
		.xatten1DB = {0, 0, 0},

		/*
		 * xatten1Margin[AR9300_MAX_CHAINS]; 3 xatten1_margin
		 * for ar9280 (0xa20c/b20c 16:12
		 */
		.xatten1Margin = {0, 0, 0},
		.tempSlope = 25,
		.voltSlope = 0,

		/*
		 * spurChans[OSPREY_EEPROM_MODAL_SPURS]; spur
		 * channels in usual fbin coding format
		 */
		.spurChans = {FREQ2FBIN(2464, 1), 0, 0, 0, 0},

		/*
		 * noiseFloorThreshCh[AR9300_MAX_CHAINS]; 3 Check
		 * if the register is per chain
		 */
		.noiseFloorThreshCh = {-1, 0, 0},
		.ob = {1, 1, 1},/* 3 chain */
		.db_stage2 = {1, 1, 1}, /* 3 chain  */
		.db_stage3 = {0, 0, 0},
		.db_stage4 = {0, 0, 0},
		.xpaBiasLvl = 0,
		.txFrameToDataStart = 0x0e,
		.txFrameToPaOn = 0x0e,
		.txClip = 3, /* 4 bits tx_clip, 4 bits dac_scale_cck */
		.antennaGain = 0,
		.switchSettling = 0x2c,
		.adcDesiredSize = -30,
		.txEndToXpaOff = 0,
		.txEndToRxOn = 0x2,
		.txFrameToXpaOn = 0xe,
		.thresh62 = 28,
		.papdRateMaskHt20 = LE32(0x0c80c080),
		.papdRateMaskHt40 = LE32(0x0080c080),
		.futureModal = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		},
	 },
	 .base_ext1 = {
		.ant_div_control = 0,
		.future = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	 },
	.calFreqPier2G = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1),
	 },
	/* ar9300_cal_data_per_freq_op_loop 2g */
	.calPierData2G = {
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
	 },
	.calTarget_freqbin_Cck = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2472, 1),
	 },
	.calTarget_freqbin_2G = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	 },
	.calTarget_freqbin_2GHT20 = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	 },
	.calTarget_freqbin_2GHT40 = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	 },
	.calTargetPowerCck = {
		 /* 1L-5L,5S,11L,11S */
		 { {34, 34, 34, 34} },
		 { {34, 34, 34, 34} },
	},
	.calTargetPower2G = {
		 /* 6-24,36,48,54 */
		 { {34, 34, 32, 32} },
		 { {34, 34, 32, 32} },
		 { {34, 34, 32, 32} },
	},
	.calTargetPower2GHT20 = {
		{ {32, 32, 32, 32, 32, 28, 32, 32, 30, 28, 0, 0, 0, 0} },
		{ {32, 32, 32, 32, 32, 28, 32, 32, 30, 28, 0, 0, 0, 0} },
		{ {32, 32, 32, 32, 32, 28, 32, 32, 30, 28, 0, 0, 0, 0} },
	},
	.calTargetPower2GHT40 = {
		{ {30, 30, 30, 30, 30, 28, 30, 30, 28, 26, 0, 0, 0, 0} },
		{ {30, 30, 30, 30, 30, 28, 30, 30, 28, 26, 0, 0, 0, 0} },
		{ {30, 30, 30, 30, 30, 28, 30, 30, 28, 26, 0, 0, 0, 0} },
	},
	.ctlIndex_2G =  {
		0x11, 0x12, 0x15, 0x17, 0x41, 0x42,
		0x45, 0x47, 0x31, 0x32, 0x35, 0x37,
	},
	.ctl_freqbin_2G = {
		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2457, 1),
			FREQ2FBIN(2462, 1)
		},
		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2462, 1),
			0xFF,
		},

		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2462, 1),
			0xFF,
		},
		{
			FREQ2FBIN(2422, 1),
			FREQ2FBIN(2427, 1),
			FREQ2FBIN(2447, 1),
			FREQ2FBIN(2452, 1)
		},

		{
			/* Data[4].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[4].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[4].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			/* Data[4].ctlEdges[3].bChannel */ FREQ2FBIN(2484, 1),
		},

		{
			/* Data[5].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[5].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[5].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			0,
		},

		{
			/* Data[6].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[6].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			FREQ2FBIN(2472, 1),
			0,
		},

		{
			/* Data[7].ctlEdges[0].bChannel */ FREQ2FBIN(2422, 1),
			/* Data[7].ctlEdges[1].bChannel */ FREQ2FBIN(2427, 1),
			/* Data[7].ctlEdges[2].bChannel */ FREQ2FBIN(2447, 1),
			/* Data[7].ctlEdges[3].bChannel */ FREQ2FBIN(2462, 1),
		},

		{
			/* Data[8].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[8].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[8].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
		},

		{
			/* Data[9].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[9].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[9].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			0
		},

		{
			/* Data[10].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[10].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[10].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			0
		},

		{
			/* Data[11].ctlEdges[0].bChannel */ FREQ2FBIN(2422, 1),
			/* Data[11].ctlEdges[1].bChannel */ FREQ2FBIN(2427, 1),
			/* Data[11].ctlEdges[2].bChannel */ FREQ2FBIN(2447, 1),
			/* Data[11].ctlEdges[3].bChannel */ FREQ2FBIN(2462, 1),
		}
	 },
	.ctlPowerData_2G = {
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 1) } },

		 { { CTL(60, 1), CTL(60, 0), CTL(0, 0), CTL(0, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },

		 { { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },

		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 1) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 1) } },
	 },
	.modalHeader5G = {
		/* 4 idle,t1,t2,b (4 bits per setting) */
		.antCtrlCommon = LE32(0x220),
		/* 4 ra1l1, ra2l1, ra1l2,ra2l2,ra12 */
		.antCtrlCommon2 = LE32(0x11111),
		 /* antCtrlChain 6 idle, t,r,rx1,rx12,b (2 bits each) */
		.antCtrlChain = {
			LE16(0x150), LE16(0x150), LE16(0x150),
		},
		 /* xatten1DB 3 xatten1_db for AR9280 (0xa20c/b20c 5:0) */
		.xatten1DB = {0, 0, 0},

		/*
		 * xatten1Margin[AR9300_MAX_CHAINS]; 3 xatten1_margin
		 * for merlin (0xa20c/b20c 16:12
		 */
		.xatten1Margin = {0, 0, 0},
		.tempSlope = 68,
		.voltSlope = 0,
		/* spurChans spur channels in usual fbin coding format */
		.spurChans = {FREQ2FBIN(5500, 0), 0, 0, 0, 0},
		/* noiseFloorThreshCh Check if the register is per chain */
		.noiseFloorThreshCh = {-1, 0, 0},
		.ob = {3, 3, 3}, /* 3 chain */
		.db_stage2 = {3, 3, 3}, /* 3 chain */
		.db_stage3 = {3, 3, 3}, /* doesn't exist for 2G */
		.db_stage4 = {3, 3, 3},	 /* don't exist for 2G */
		.xpaBiasLvl = 0xf,
		.txFrameToDataStart = 0x0e,
		.txFrameToPaOn = 0x0e,
		.txClip = 3, /* 4 bits tx_clip, 4 bits dac_scale_cck */
		.antennaGain = 0,
		.switchSettling = 0x2d,
		.adcDesiredSize = -30,
		.txEndToXpaOff = 0,
		.txEndToRxOn = 0x2,
		.txFrameToXpaOn = 0xe,
		.thresh62 = 28,
		.papdRateMaskHt20 = LE32(0x0cf0e0e0),
		.papdRateMaskHt40 = LE32(0x6cf0e0e0),
		.futureModal = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		},
	 },
	.base_ext2 = {
		.tempSlopeLow = 72,
		.tempSlopeHigh = 105,
		.xatten1DBLow = {0, 0, 0},
		.xatten1MarginLow = {0, 0, 0},
		.xatten1DBHigh = {0, 0, 0},
		.xatten1MarginHigh = {0, 0, 0}
	 },
	.calFreqPier5G = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5240, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5600, 0),
		FREQ2FBIN(5745, 0),
		FREQ2FBIN(5785, 0)
	},
	.calPierData5G = {
			{
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
			},
			{
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
			},
			{
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
			},

	},
	.calTarget_freqbin_5G = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5220, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5600, 0),
		FREQ2FBIN(5745, 0),
		FREQ2FBIN(5785, 0)
	},
	.calTarget_freqbin_5GHT20 = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5240, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5700, 0),
		FREQ2FBIN(5745, 0),
		FREQ2FBIN(5825, 0)
	},
	.calTarget_freqbin_5GHT40 = {
		FREQ2FBIN(5190, 0),
		FREQ2FBIN(5230, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5410, 0),
		FREQ2FBIN(5510, 0),
		FREQ2FBIN(5670, 0),
		FREQ2FBIN(5755, 0),
		FREQ2FBIN(5825, 0)
	 },
	.calTargetPower5G = {
		/* 6-24,36,48,54 */
		{ {42, 40, 40, 34} },
		{ {42, 40, 40, 34} },
		{ {42, 40, 40, 34} },
		{ {42, 40, 40, 34} },
		{ {42, 40, 40, 34} },
		{ {42, 40, 40, 34} },
		{ {42, 40, 40, 34} },
		{ {42, 40, 40, 34} },
	 },
	.calTargetPower5GHT20 = {
		/*
		 * 0_8_16,1-3_9-11_17-19,
		 * 4,5,6,7,12,13,14,15,20,21,22,23
		 */
		{ {40, 40, 40, 40, 32, 28, 40, 40, 32, 28, 40, 40, 32, 20} },
		{ {40, 40, 40, 40, 32, 28, 40, 40, 32, 28, 40, 40, 32, 20} },
		{ {40, 40, 40, 40, 32, 28, 40, 40, 32, 28, 40, 40, 32, 20} },
		{ {40, 40, 40, 40, 32, 28, 40, 40, 32, 28, 40, 40, 32, 20} },
		{ {40, 40, 40, 40, 32, 28, 40, 40, 32, 28, 40, 40, 32, 20} },
		{ {40, 40, 40, 40, 32, 28, 40, 40, 32, 28, 40, 40, 32, 20} },
		{ {38, 38, 38, 38, 32, 28, 38, 38, 32, 28, 38, 38, 32, 26} },
		{ {36, 36, 36, 36, 32, 28, 36, 36, 32, 28, 36, 36, 32, 26} },
	 },
	.calTargetPower5GHT40 =  {
		/*
		 * 0_8_16,1-3_9-11_17-19,
		 * 4,5,6,7,12,13,14,15,20,21,22,23
		 */
		{ {40, 40, 40, 38, 30, 26, 40, 40, 30, 26, 40, 40, 30, 24} },
		{ {40, 40, 40, 38, 30, 26, 40, 40, 30, 26, 40, 40, 30, 24} },
		{ {40, 40, 40, 38, 30, 26, 40, 40, 30, 26, 40, 40, 30, 24} },
		{ {40, 40, 40, 38, 30, 26, 40, 40, 30, 26, 40, 40, 30, 24} },
		{ {40, 40, 40, 38, 30, 26, 40, 40, 30, 26, 40, 40, 30, 24} },
		{ {40, 40, 40, 38, 30, 26, 40, 40, 30, 26, 40, 40, 30, 24} },
		{ {36, 36, 36, 36, 30, 26, 36, 36, 30, 26, 36, 36, 30, 24} },
		{ {34, 34, 34, 34, 30, 26, 34, 34, 30, 26, 34, 34, 30, 24} },
	 },
	.ctlIndex_5G =  {
		0x10, 0x16, 0x18, 0x40, 0x46,
		0x48, 0x30, 0x36, 0x38
	},
	.ctl_freqbin_5G =  {
		{
			/* Data[0].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[0].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[0].ctlEdges[2].bChannel */ FREQ2FBIN(5280, 0),
			/* Data[0].ctlEdges[3].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[0].ctlEdges[4].bChannel */ FREQ2FBIN(5600, 0),
			/* Data[0].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[0].ctlEdges[6].bChannel */ FREQ2FBIN(5745, 0),
			/* Data[0].ctlEdges[7].bChannel */ FREQ2FBIN(5825, 0)
		},
		{
			/* Data[1].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[1].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[1].ctlEdges[2].bChannel */ FREQ2FBIN(5280, 0),
			/* Data[1].ctlEdges[3].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[1].ctlEdges[4].bChannel */ FREQ2FBIN(5520, 0),
			/* Data[1].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[1].ctlEdges[6].bChannel */ FREQ2FBIN(5745, 0),
			/* Data[1].ctlEdges[7].bChannel */ FREQ2FBIN(5825, 0)
		},

		{
			/* Data[2].ctlEdges[0].bChannel */ FREQ2FBIN(5190, 0),
			/* Data[2].ctlEdges[1].bChannel */ FREQ2FBIN(5230, 0),
			/* Data[2].ctlEdges[2].bChannel */ FREQ2FBIN(5270, 0),
			/* Data[2].ctlEdges[3].bChannel */ FREQ2FBIN(5310, 0),
			/* Data[2].ctlEdges[4].bChannel */ FREQ2FBIN(5510, 0),
			/* Data[2].ctlEdges[5].bChannel */ FREQ2FBIN(5550, 0),
			/* Data[2].ctlEdges[6].bChannel */ FREQ2FBIN(5670, 0),
			/* Data[2].ctlEdges[7].bChannel */ FREQ2FBIN(5755, 0)
		},

		{
			/* Data[3].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[3].ctlEdges[1].bChannel */ FREQ2FBIN(5200, 0),
			/* Data[3].ctlEdges[2].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[3].ctlEdges[3].bChannel */ FREQ2FBIN(5320, 0),
			/* Data[3].ctlEdges[4].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[3].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[3].ctlEdges[6].bChannel */ 0xFF,
			/* Data[3].ctlEdges[7].bChannel */ 0xFF,
		},

		{
			/* Data[4].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[4].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[4].ctlEdges[2].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[4].ctlEdges[3].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[4].ctlEdges[4].bChannel */ 0xFF,
			/* Data[4].ctlEdges[5].bChannel */ 0xFF,
			/* Data[4].ctlEdges[6].bChannel */ 0xFF,
			/* Data[4].ctlEdges[7].bChannel */ 0xFF,
		},

		{
			/* Data[5].ctlEdges[0].bChannel */ FREQ2FBIN(5190, 0),
			/* Data[5].ctlEdges[1].bChannel */ FREQ2FBIN(5270, 0),
			/* Data[5].ctlEdges[2].bChannel */ FREQ2FBIN(5310, 0),
			/* Data[5].ctlEdges[3].bChannel */ FREQ2FBIN(5510, 0),
			/* Data[5].ctlEdges[4].bChannel */ FREQ2FBIN(5590, 0),
			/* Data[5].ctlEdges[5].bChannel */ FREQ2FBIN(5670, 0),
			/* Data[5].ctlEdges[6].bChannel */ 0xFF,
			/* Data[5].ctlEdges[7].bChannel */ 0xFF
		},

		{
			/* Data[6].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[6].ctlEdges[1].bChannel */ FREQ2FBIN(5200, 0),
			/* Data[6].ctlEdges[2].bChannel */ FREQ2FBIN(5220, 0),
			/* Data[6].ctlEdges[3].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[6].ctlEdges[4].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[6].ctlEdges[5].bChannel */ FREQ2FBIN(5600, 0),
			/* Data[6].ctlEdges[6].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[6].ctlEdges[7].bChannel */ FREQ2FBIN(5745, 0)
		},

		{
			/* Data[7].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[7].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[7].ctlEdges[2].bChannel */ FREQ2FBIN(5320, 0),
			/* Data[7].ctlEdges[3].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[7].ctlEdges[4].bChannel */ FREQ2FBIN(5560, 0),
			/* Data[7].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[7].ctlEdges[6].bChannel */ FREQ2FBIN(5745, 0),
			/* Data[7].ctlEdges[7].bChannel */ FREQ2FBIN(5825, 0)
		},

		{
			/* Data[8].ctlEdges[0].bChannel */ FREQ2FBIN(5190, 0),
			/* Data[8].ctlEdges[1].bChannel */ FREQ2FBIN(5230, 0),
			/* Data[8].ctlEdges[2].bChannel */ FREQ2FBIN(5270, 0),
			/* Data[8].ctlEdges[3].bChannel */ FREQ2FBIN(5510, 0),
			/* Data[8].ctlEdges[4].bChannel */ FREQ2FBIN(5550, 0),
			/* Data[8].ctlEdges[5].bChannel */ FREQ2FBIN(5670, 0),
			/* Data[8].ctlEdges[6].bChannel */ FREQ2FBIN(5755, 0),
			/* Data[8].ctlEdges[7].bChannel */ FREQ2FBIN(5795, 0)
		}
	 },
	.ctlPowerData_5G = {
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
			}
		},
		{
			{
				CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 0),
				CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
				CTL(60, 0), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 0), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 0), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 0), CTL(60, 1),
			}
		},
	 }
};


static const struct ar9300_eeprom ar9300_h112 = {
	.eepromVersion = 2,
	.templateVersion = 3,
	.macAddr = {0x00, 0x03, 0x7f, 0x0, 0x0, 0x0},
	.custData = {"h112-241-f0000"},
	.baseEepHeader = {
		.regDmn = { LE16(0), LE16(0x1f) },
		.txrxMask =  0x77, /* 4 bits tx and 4 bits rx */
		.opCapFlags = {
			.opFlags = AR5416_OPFLAGS_11G | AR5416_OPFLAGS_11A,
			.eepMisc = 0,
		},
		.rfSilent = 0,
		.blueToothOptions = 0,
		.deviceCap = 0,
		.deviceType = 5, /* takes lower byte in eeprom location */
		.pwrTableOffset = AR9300_PWR_TABLE_OFFSET,
		.params_for_tuning_caps = {0, 0},
		.featureEnable = 0x0d,
		/*
		 * bit0 - enable tx temp comp - disabled
		 * bit1 - enable tx volt comp - disabled
		 * bit2 - enable fastClock - enabled
		 * bit3 - enable doubling - enabled
		 * bit4 - enable internal regulator - disabled
		 * bit5 - enable pa predistortion - disabled
		 */
		.miscConfiguration = 0, /* bit0 - turn down drivestrength */
		.eepromWriteEnableGpio = 6,
		.wlanDisableGpio = 0,
		.wlanLedGpio = 8,
		.rxBandSelectGpio = 0xff,
		.txrxgain = 0x10,
		.swreg = 0,
	},
	.modalHeader2G = {
		/* ar9300_modal_eep_header  2g */
		/* 4 idle,t1,t2,b(4 bits per setting) */
		.antCtrlCommon = LE32(0x110),
		/* 4 ra1l1, ra2l1, ra1l2, ra2l2, ra12 */
		.antCtrlCommon2 = LE32(0x44444),

		/*
		 * antCtrlChain[AR9300_MAX_CHAINS]; 6 idle, t, r,
		 * rx1, rx12, b (2 bits each)
		 */
		.antCtrlChain = { LE16(0x150), LE16(0x150), LE16(0x150) },

		/*
		 * xatten1DB[AR9300_MAX_CHAINS];  3 xatten1_db
		 * for ar9280 (0xa20c/b20c 5:0)
		 */
		.xatten1DB = {0, 0, 0},

		/*
		 * xatten1Margin[AR9300_MAX_CHAINS]; 3 xatten1_margin
		 * for ar9280 (0xa20c/b20c 16:12
		 */
		.xatten1Margin = {0, 0, 0},
		.tempSlope = 25,
		.voltSlope = 0,

		/*
		 * spurChans[OSPREY_EEPROM_MODAL_SPURS]; spur
		 * channels in usual fbin coding format
		 */
		.spurChans = {FREQ2FBIN(2464, 1), 0, 0, 0, 0},

		/*
		 * noiseFloorThreshCh[AR9300_MAX_CHAINS]; 3 Check
		 * if the register is per chain
		 */
		.noiseFloorThreshCh = {-1, 0, 0},
		.ob = {1, 1, 1},/* 3 chain */
		.db_stage2 = {1, 1, 1}, /* 3 chain  */
		.db_stage3 = {0, 0, 0},
		.db_stage4 = {0, 0, 0},
		.xpaBiasLvl = 0,
		.txFrameToDataStart = 0x0e,
		.txFrameToPaOn = 0x0e,
		.txClip = 3, /* 4 bits tx_clip, 4 bits dac_scale_cck */
		.antennaGain = 0,
		.switchSettling = 0x2c,
		.adcDesiredSize = -30,
		.txEndToXpaOff = 0,
		.txEndToRxOn = 0x2,
		.txFrameToXpaOn = 0xe,
		.thresh62 = 28,
		.papdRateMaskHt20 = LE32(0x80c080),
		.papdRateMaskHt40 = LE32(0x80c080),
		.futureModal = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		},
	},
	.base_ext1 = {
		.ant_div_control = 0,
		.future = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	},
	.calFreqPier2G = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1),
	},
	/* ar9300_cal_data_per_freq_op_loop 2g */
	.calPierData2G = {
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
	},
	.calTarget_freqbin_Cck = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2484, 1),
	},
	.calTarget_freqbin_2G = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	},
	.calTarget_freqbin_2GHT20 = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	},
	.calTarget_freqbin_2GHT40 = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	},
	.calTargetPowerCck = {
		/* 1L-5L,5S,11L,11S */
		{ {34, 34, 34, 34} },
		{ {34, 34, 34, 34} },
	},
	.calTargetPower2G = {
		/* 6-24,36,48,54 */
		{ {34, 34, 32, 32} },
		{ {34, 34, 32, 32} },
		{ {34, 34, 32, 32} },
	},
	.calTargetPower2GHT20 = {
		{ {32, 32, 32, 32, 32, 30, 32, 32, 30, 28, 28, 28, 28, 24} },
		{ {32, 32, 32, 32, 32, 30, 32, 32, 30, 28, 28, 28, 28, 24} },
		{ {32, 32, 32, 32, 32, 30, 32, 32, 30, 28, 28, 28, 28, 24} },
	},
	.calTargetPower2GHT40 = {
		{ {30, 30, 30, 30, 30, 28, 30, 30, 28, 26, 26, 26, 26, 22} },
		{ {30, 30, 30, 30, 30, 28, 30, 30, 28, 26, 26, 26, 26, 22} },
		{ {30, 30, 30, 30, 30, 28, 30, 30, 28, 26, 26, 26, 26, 22} },
	},
	.ctlIndex_2G =  {
		0x11, 0x12, 0x15, 0x17, 0x41, 0x42,
		0x45, 0x47, 0x31, 0x32, 0x35, 0x37,
	},
	.ctl_freqbin_2G = {
		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2457, 1),
			FREQ2FBIN(2462, 1)
		},
		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2462, 1),
			0xFF,
		},

		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2462, 1),
			0xFF,
		},
		{
			FREQ2FBIN(2422, 1),
			FREQ2FBIN(2427, 1),
			FREQ2FBIN(2447, 1),
			FREQ2FBIN(2452, 1)
		},

		{
			/* Data[4].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[4].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[4].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			/* Data[4].ctlEdges[3].bChannel */ FREQ2FBIN(2484, 1),
		},

		{
			/* Data[5].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[5].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[5].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			0,
		},

		{
			/* Data[6].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[6].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			FREQ2FBIN(2472, 1),
			0,
		},

		{
			/* Data[7].ctlEdges[0].bChannel */ FREQ2FBIN(2422, 1),
			/* Data[7].ctlEdges[1].bChannel */ FREQ2FBIN(2427, 1),
			/* Data[7].ctlEdges[2].bChannel */ FREQ2FBIN(2447, 1),
			/* Data[7].ctlEdges[3].bChannel */ FREQ2FBIN(2462, 1),
		},

		{
			/* Data[8].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[8].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[8].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
		},

		{
			/* Data[9].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[9].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[9].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			0
		},

		{
			/* Data[10].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[10].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[10].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			0
		},

		{
			/* Data[11].ctlEdges[0].bChannel */ FREQ2FBIN(2422, 1),
			/* Data[11].ctlEdges[1].bChannel */ FREQ2FBIN(2427, 1),
			/* Data[11].ctlEdges[2].bChannel */ FREQ2FBIN(2447, 1),
			/* Data[11].ctlEdges[3].bChannel */ FREQ2FBIN(2462, 1),
		}
	},
	.ctlPowerData_2G = {
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		{ { CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 1) } },

		{ { CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 0) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },

		{ { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 0) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },

		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 1) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 1) } },
	},
	.modalHeader5G = {
		/* 4 idle,t1,t2,b (4 bits per setting) */
		.antCtrlCommon = LE32(0x220),
		/* 4 ra1l1, ra2l1, ra1l2,ra2l2,ra12 */
		.antCtrlCommon2 = LE32(0x44444),
		/* antCtrlChain 6 idle, t,r,rx1,rx12,b (2 bits each) */
		.antCtrlChain = {
			LE16(0x150), LE16(0x150), LE16(0x150),
		},
		/* xatten1DB 3 xatten1_db for AR9280 (0xa20c/b20c 5:0) */
		.xatten1DB = {0, 0, 0},

		/*
		 * xatten1Margin[AR9300_MAX_CHAINS]; 3 xatten1_margin
		 * for merlin (0xa20c/b20c 16:12
		 */
		.xatten1Margin = {0, 0, 0},
		.tempSlope = 45,
		.voltSlope = 0,
		/* spurChans spur channels in usual fbin coding format */
		.spurChans = {0, 0, 0, 0, 0},
		/* noiseFloorThreshCh Check if the register is per chain */
		.noiseFloorThreshCh = {-1, 0, 0},
		.ob = {3, 3, 3}, /* 3 chain */
		.db_stage2 = {3, 3, 3}, /* 3 chain */
		.db_stage3 = {3, 3, 3}, /* doesn't exist for 2G */
		.db_stage4 = {3, 3, 3},	 /* don't exist for 2G */
		.xpaBiasLvl = 0,
		.txFrameToDataStart = 0x0e,
		.txFrameToPaOn = 0x0e,
		.txClip = 3, /* 4 bits tx_clip, 4 bits dac_scale_cck */
		.antennaGain = 0,
		.switchSettling = 0x2d,
		.adcDesiredSize = -30,
		.txEndToXpaOff = 0,
		.txEndToRxOn = 0x2,
		.txFrameToXpaOn = 0xe,
		.thresh62 = 28,
		.papdRateMaskHt20 = LE32(0x0cf0e0e0),
		.papdRateMaskHt40 = LE32(0x6cf0e0e0),
		.futureModal = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		},
	},
	.base_ext2 = {
		.tempSlopeLow = 40,
		.tempSlopeHigh = 50,
		.xatten1DBLow = {0, 0, 0},
		.xatten1MarginLow = {0, 0, 0},
		.xatten1DBHigh = {0, 0, 0},
		.xatten1MarginHigh = {0, 0, 0}
	},
	.calFreqPier5G = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5220, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5600, 0),
		FREQ2FBIN(5700, 0),
		FREQ2FBIN(5825, 0)
	},
	.calPierData5G = {
		{
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
		},
		{
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
		},
		{
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
		},

	},
	.calTarget_freqbin_5G = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5240, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5600, 0),
		FREQ2FBIN(5700, 0),
		FREQ2FBIN(5825, 0)
	},
	.calTarget_freqbin_5GHT20 = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5240, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5700, 0),
		FREQ2FBIN(5745, 0),
		FREQ2FBIN(5825, 0)
	},
	.calTarget_freqbin_5GHT40 = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5240, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5700, 0),
		FREQ2FBIN(5745, 0),
		FREQ2FBIN(5825, 0)
	},
	.calTargetPower5G = {
		/* 6-24,36,48,54 */
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
	},
	.calTargetPower5GHT20 = {
		/*
		 * 0_8_16,1-3_9-11_17-19,
		 * 4,5,6,7,12,13,14,15,20,21,22,23
		 */
		{ {30, 30, 30, 28, 24, 20, 30, 28, 24, 20, 20, 20, 20, 16} },
		{ {30, 30, 30, 28, 24, 20, 30, 28, 24, 20, 20, 20, 20, 16} },
		{ {30, 30, 30, 26, 22, 18, 30, 26, 22, 18, 18, 18, 18, 16} },
		{ {30, 30, 30, 26, 22, 18, 30, 26, 22, 18, 18, 18, 18, 16} },
		{ {30, 30, 30, 24, 20, 16, 30, 24, 20, 16, 16, 16, 16, 14} },
		{ {30, 30, 30, 24, 20, 16, 30, 24, 20, 16, 16, 16, 16, 14} },
		{ {30, 30, 30, 22, 18, 14, 30, 22, 18, 14, 14, 14, 14, 12} },
		{ {30, 30, 30, 22, 18, 14, 30, 22, 18, 14, 14, 14, 14, 12} },
	},
	.calTargetPower5GHT40 =  {
		/*
		 * 0_8_16,1-3_9-11_17-19,
		 * 4,5,6,7,12,13,14,15,20,21,22,23
		 */
		{ {28, 28, 28, 26, 22, 18, 28, 26, 22, 18, 18, 18, 18, 14} },
		{ {28, 28, 28, 26, 22, 18, 28, 26, 22, 18, 18, 18, 18, 14} },
		{ {28, 28, 28, 24, 20, 16, 28, 24, 20, 16, 16, 16, 16, 12} },
		{ {28, 28, 28, 24, 20, 16, 28, 24, 20, 16, 16, 16, 16, 12} },
		{ {28, 28, 28, 22, 18, 14, 28, 22, 18, 14, 14, 14, 14, 10} },
		{ {28, 28, 28, 22, 18, 14, 28, 22, 18, 14, 14, 14, 14, 10} },
		{ {28, 28, 28, 20, 16, 12, 28, 20, 16, 12, 12, 12, 12, 8} },
		{ {28, 28, 28, 20, 16, 12, 28, 20, 16, 12, 12, 12, 12, 8} },
	},
	.ctlIndex_5G =  {
		0x10, 0x16, 0x18, 0x40, 0x46,
		0x48, 0x30, 0x36, 0x38
	},
	.ctl_freqbin_5G =  {
		{
			/* Data[0].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[0].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[0].ctlEdges[2].bChannel */ FREQ2FBIN(5280, 0),
			/* Data[0].ctlEdges[3].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[0].ctlEdges[4].bChannel */ FREQ2FBIN(5600, 0),
			/* Data[0].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[0].ctlEdges[6].bChannel */ FREQ2FBIN(5745, 0),
			/* Data[0].ctlEdges[7].bChannel */ FREQ2FBIN(5825, 0)
		},
		{
			/* Data[1].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[1].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[1].ctlEdges[2].bChannel */ FREQ2FBIN(5280, 0),
			/* Data[1].ctlEdges[3].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[1].ctlEdges[4].bChannel */ FREQ2FBIN(5520, 0),
			/* Data[1].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[1].ctlEdges[6].bChannel */ FREQ2FBIN(5745, 0),
			/* Data[1].ctlEdges[7].bChannel */ FREQ2FBIN(5825, 0)
		},

		{
			/* Data[2].ctlEdges[0].bChannel */ FREQ2FBIN(5190, 0),
			/* Data[2].ctlEdges[1].bChannel */ FREQ2FBIN(5230, 0),
			/* Data[2].ctlEdges[2].bChannel */ FREQ2FBIN(5270, 0),
			/* Data[2].ctlEdges[3].bChannel */ FREQ2FBIN(5310, 0),
			/* Data[2].ctlEdges[4].bChannel */ FREQ2FBIN(5510, 0),
			/* Data[2].ctlEdges[5].bChannel */ FREQ2FBIN(5550, 0),
			/* Data[2].ctlEdges[6].bChannel */ FREQ2FBIN(5670, 0),
			/* Data[2].ctlEdges[7].bChannel */ FREQ2FBIN(5755, 0)
		},

		{
			/* Data[3].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[3].ctlEdges[1].bChannel */ FREQ2FBIN(5200, 0),
			/* Data[3].ctlEdges[2].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[3].ctlEdges[3].bChannel */ FREQ2FBIN(5320, 0),
			/* Data[3].ctlEdges[4].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[3].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[3].ctlEdges[6].bChannel */ 0xFF,
			/* Data[3].ctlEdges[7].bChannel */ 0xFF,
		},

		{
			/* Data[4].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[4].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[4].ctlEdges[2].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[4].ctlEdges[3].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[4].ctlEdges[4].bChannel */ 0xFF,
			/* Data[4].ctlEdges[5].bChannel */ 0xFF,
			/* Data[4].ctlEdges[6].bChannel */ 0xFF,
			/* Data[4].ctlEdges[7].bChannel */ 0xFF,
		},

		{
			/* Data[5].ctlEdges[0].bChannel */ FREQ2FBIN(5190, 0),
			/* Data[5].ctlEdges[1].bChannel */ FREQ2FBIN(5270, 0),
			/* Data[5].ctlEdges[2].bChannel */ FREQ2FBIN(5310, 0),
			/* Data[5].ctlEdges[3].bChannel */ FREQ2FBIN(5510, 0),
			/* Data[5].ctlEdges[4].bChannel */ FREQ2FBIN(5590, 0),
			/* Data[5].ctlEdges[5].bChannel */ FREQ2FBIN(5670, 0),
			/* Data[5].ctlEdges[6].bChannel */ 0xFF,
			/* Data[5].ctlEdges[7].bChannel */ 0xFF
		},

		{
			/* Data[6].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[6].ctlEdges[1].bChannel */ FREQ2FBIN(5200, 0),
			/* Data[6].ctlEdges[2].bChannel */ FREQ2FBIN(5220, 0),
			/* Data[6].ctlEdges[3].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[6].ctlEdges[4].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[6].ctlEdges[5].bChannel */ FREQ2FBIN(5600, 0),
			/* Data[6].ctlEdges[6].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[6].ctlEdges[7].bChannel */ FREQ2FBIN(5745, 0)
		},

		{
			/* Data[7].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[7].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[7].ctlEdges[2].bChannel */ FREQ2FBIN(5320, 0),
			/* Data[7].ctlEdges[3].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[7].ctlEdges[4].bChannel */ FREQ2FBIN(5560, 0),
			/* Data[7].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[7].ctlEdges[6].bChannel */ FREQ2FBIN(5745, 0),
			/* Data[7].ctlEdges[7].bChannel */ FREQ2FBIN(5825, 0)
		},

		{
			/* Data[8].ctlEdges[0].bChannel */ FREQ2FBIN(5190, 0),
			/* Data[8].ctlEdges[1].bChannel */ FREQ2FBIN(5230, 0),
			/* Data[8].ctlEdges[2].bChannel */ FREQ2FBIN(5270, 0),
			/* Data[8].ctlEdges[3].bChannel */ FREQ2FBIN(5510, 0),
			/* Data[8].ctlEdges[4].bChannel */ FREQ2FBIN(5550, 0),
			/* Data[8].ctlEdges[5].bChannel */ FREQ2FBIN(5670, 0),
			/* Data[8].ctlEdges[6].bChannel */ FREQ2FBIN(5755, 0),
			/* Data[8].ctlEdges[7].bChannel */ FREQ2FBIN(5795, 0)
		}
	},
	.ctlPowerData_5G = {
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
			}
		},
		{
			{
				CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 0),
				CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
				CTL(60, 0), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 0), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 0), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 0), CTL(60, 1),
			}
		},
	}
};


static const struct ar9300_eeprom ar9300_x112 = {
	.eepromVersion = 2,
	.templateVersion = 5,
	.macAddr = {0x00, 0x03, 0x7f, 0x0, 0x0, 0x0},
	.custData = {"x112-041-f0000"},
	.baseEepHeader = {
		.regDmn = { LE16(0), LE16(0x1f) },
		.txrxMask =  0x77, /* 4 bits tx and 4 bits rx */
		.opCapFlags = {
			.opFlags = AR5416_OPFLAGS_11G | AR5416_OPFLAGS_11A,
			.eepMisc = 0,
		},
		.rfSilent = 0,
		.blueToothOptions = 0,
		.deviceCap = 0,
		.deviceType = 5, /* takes lower byte in eeprom location */
		.pwrTableOffset = AR9300_PWR_TABLE_OFFSET,
		.params_for_tuning_caps = {0, 0},
		.featureEnable = 0x0d,
		/*
		 * bit0 - enable tx temp comp - disabled
		 * bit1 - enable tx volt comp - disabled
		 * bit2 - enable fastclock - enabled
		 * bit3 - enable doubling - enabled
		 * bit4 - enable internal regulator - disabled
		 * bit5 - enable pa predistortion - disabled
		 */
		.miscConfiguration = 0, /* bit0 - turn down drivestrength */
		.eepromWriteEnableGpio = 6,
		.wlanDisableGpio = 0,
		.wlanLedGpio = 8,
		.rxBandSelectGpio = 0xff,
		.txrxgain = 0x0,
		.swreg = 0,
	},
	.modalHeader2G = {
		/* ar9300_modal_eep_header  2g */
		/* 4 idle,t1,t2,b(4 bits per setting) */
		.antCtrlCommon = LE32(0x110),
		/* 4 ra1l1, ra2l1, ra1l2, ra2l2, ra12 */
		.antCtrlCommon2 = LE32(0x22222),

		/*
		 * antCtrlChain[ar9300_max_chains]; 6 idle, t, r,
		 * rx1, rx12, b (2 bits each)
		 */
		.antCtrlChain = { LE16(0x10), LE16(0x10), LE16(0x10) },

		/*
		 * xatten1DB[AR9300_max_chains];  3 xatten1_db
		 * for ar9280 (0xa20c/b20c 5:0)
		 */
		.xatten1DB = {0x1b, 0x1b, 0x1b},

		/*
		 * xatten1Margin[ar9300_max_chains]; 3 xatten1_margin
		 * for ar9280 (0xa20c/b20c 16:12
		 */
		.xatten1Margin = {0x15, 0x15, 0x15},
		.tempSlope = 50,
		.voltSlope = 0,

		/*
		 * spurChans[OSPrey_eeprom_modal_sPURS]; spur
		 * channels in usual fbin coding format
		 */
		.spurChans = {FREQ2FBIN(2464, 1), 0, 0, 0, 0},

		/*
		 * noiseFloorThreshch[ar9300_max_cHAINS]; 3 Check
		 * if the register is per chain
		 */
		.noiseFloorThreshCh = {-1, 0, 0},
		.ob = {1, 1, 1},/* 3 chain */
		.db_stage2 = {1, 1, 1}, /* 3 chain  */
		.db_stage3 = {0, 0, 0},
		.db_stage4 = {0, 0, 0},
		.xpaBiasLvl = 0,
		.txFrameToDataStart = 0x0e,
		.txFrameToPaOn = 0x0e,
		.txClip = 3, /* 4 bits tx_clip, 4 bits dac_scale_cck */
		.antennaGain = 0,
		.switchSettling = 0x2c,
		.adcDesiredSize = -30,
		.txEndToXpaOff = 0,
		.txEndToRxOn = 0x2,
		.txFrameToXpaOn = 0xe,
		.thresh62 = 28,
		.papdRateMaskHt20 = LE32(0x0c80c080),
		.papdRateMaskHt40 = LE32(0x0080c080),
		.futureModal = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		},
	},
	.base_ext1 = {
		.ant_div_control = 0,
		.future = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	},
	.calFreqPier2G = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1),
	},
	/* ar9300_cal_data_per_freq_op_loop 2g */
	.calPierData2G = {
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
	},
	.calTarget_freqbin_Cck = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2472, 1),
	},
	.calTarget_freqbin_2G = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	},
	.calTarget_freqbin_2GHT20 = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	},
	.calTarget_freqbin_2GHT40 = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	},
	.calTargetPowerCck = {
		/* 1L-5L,5S,11L,11s */
		{ {38, 38, 38, 38} },
		{ {38, 38, 38, 38} },
	},
	.calTargetPower2G = {
		/* 6-24,36,48,54 */
		{ {38, 38, 36, 34} },
		{ {38, 38, 36, 34} },
		{ {38, 38, 34, 32} },
	},
	.calTargetPower2GHT20 = {
		{ {36, 36, 36, 36, 36, 34, 34, 32, 30, 28, 28, 28, 28, 26} },
		{ {36, 36, 36, 36, 36, 34, 36, 34, 32, 30, 30, 30, 28, 26} },
		{ {36, 36, 36, 36, 36, 34, 34, 32, 30, 28, 28, 28, 28, 26} },
	},
	.calTargetPower2GHT40 = {
		{ {36, 36, 36, 36, 34, 32, 32, 30, 28, 26, 26, 26, 26, 24} },
		{ {36, 36, 36, 36, 34, 32, 34, 32, 30, 28, 28, 28, 28, 24} },
		{ {36, 36, 36, 36, 34, 32, 32, 30, 28, 26, 26, 26, 26, 24} },
	},
	.ctlIndex_2G =  {
		0x11, 0x12, 0x15, 0x17, 0x41, 0x42,
		0x45, 0x47, 0x31, 0x32, 0x35, 0x37,
	},
	.ctl_freqbin_2G = {
		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2457, 1),
			FREQ2FBIN(2462, 1)
		},
		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2462, 1),
			0xFF,
		},

		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2462, 1),
			0xFF,
		},
		{
			FREQ2FBIN(2422, 1),
			FREQ2FBIN(2427, 1),
			FREQ2FBIN(2447, 1),
			FREQ2FBIN(2452, 1)
		},

		{
			/* Data[4].ctledges[0].bchannel */ FREQ2FBIN(2412, 1),
			/* Data[4].ctledges[1].bchannel */ FREQ2FBIN(2417, 1),
			/* Data[4].ctledges[2].bchannel */ FREQ2FBIN(2472, 1),
			/* Data[4].ctledges[3].bchannel */ FREQ2FBIN(2484, 1),
		},

		{
			/* Data[5].ctledges[0].bchannel */ FREQ2FBIN(2412, 1),
			/* Data[5].ctledges[1].bchannel */ FREQ2FBIN(2417, 1),
			/* Data[5].ctledges[2].bchannel */ FREQ2FBIN(2472, 1),
			0,
		},

		{
			/* Data[6].ctledges[0].bchannel */ FREQ2FBIN(2412, 1),
			/* Data[6].ctledges[1].bchannel */ FREQ2FBIN(2417, 1),
			FREQ2FBIN(2472, 1),
			0,
		},

		{
			/* Data[7].ctledges[0].bchannel */ FREQ2FBIN(2422, 1),
			/* Data[7].ctledges[1].bchannel */ FREQ2FBIN(2427, 1),
			/* Data[7].ctledges[2].bchannel */ FREQ2FBIN(2447, 1),
			/* Data[7].ctledges[3].bchannel */ FREQ2FBIN(2462, 1),
		},

		{
			/* Data[8].ctledges[0].bchannel */ FREQ2FBIN(2412, 1),
			/* Data[8].ctledges[1].bchannel */ FREQ2FBIN(2417, 1),
			/* Data[8].ctledges[2].bchannel */ FREQ2FBIN(2472, 1),
		},

		{
			/* Data[9].ctledges[0].bchannel */ FREQ2FBIN(2412, 1),
			/* Data[9].ctledges[1].bchannel */ FREQ2FBIN(2417, 1),
			/* Data[9].ctledges[2].bchannel */ FREQ2FBIN(2472, 1),
			0
		},

		{
			/* Data[10].ctledges[0].bchannel */ FREQ2FBIN(2412, 1),
			/* Data[10].ctledges[1].bchannel */ FREQ2FBIN(2417, 1),
			/* Data[10].ctledges[2].bchannel */ FREQ2FBIN(2472, 1),
			0
		},

		{
			/* Data[11].ctledges[0].bchannel */ FREQ2FBIN(2422, 1),
			/* Data[11].ctledges[1].bchannel */ FREQ2FBIN(2427, 1),
			/* Data[11].ctledges[2].bchannel */ FREQ2FBIN(2447, 1),
			/* Data[11].ctledges[3].bchannel */ FREQ2FBIN(2462, 1),
		}
	},
	.ctlPowerData_2G = {
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		{ { CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 1) } },

		{ { CTL(60, 1), CTL(60, 0), CTL(0, 0), CTL(0, 0) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },

		{ { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 0) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },

		{ { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 1) } },
		{ { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 1) } },
	},
	.modalHeader5G = {
		/* 4 idle,t1,t2,b (4 bits per setting) */
		.antCtrlCommon = LE32(0x110),
		/* 4 ra1l1, ra2l1, ra1l2,ra2l2,ra12 */
		.antCtrlCommon2 = LE32(0x22222),
		/* antCtrlChain 6 idle, t,r,rx1,rx12,b (2 bits each) */
		.antCtrlChain = {
			LE16(0x0), LE16(0x0), LE16(0x0),
		},
		/* xatten1DB 3 xatten1_db for ar9280 (0xa20c/b20c 5:0) */
		.xatten1DB = {0x13, 0x19, 0x17},

		/*
		 * xatten1Margin[ar9300_max_chains]; 3 xatten1_margin
		 * for merlin (0xa20c/b20c 16:12
		 */
		.xatten1Margin = {0x19, 0x19, 0x19},
		.tempSlope = 70,
		.voltSlope = 15,
		/* spurChans spur channels in usual fbin coding format */
		.spurChans = {0, 0, 0, 0, 0},
		/* noiseFloorThreshch check if the register is per chain */
		.noiseFloorThreshCh = {-1, 0, 0},
		.ob = {3, 3, 3}, /* 3 chain */
		.db_stage2 = {3, 3, 3}, /* 3 chain */
		.db_stage3 = {3, 3, 3}, /* doesn't exist for 2G */
		.db_stage4 = {3, 3, 3},	 /* don't exist for 2G */
		.xpaBiasLvl = 0,
		.txFrameToDataStart = 0x0e,
		.txFrameToPaOn = 0x0e,
		.txClip = 3, /* 4 bits tx_clip, 4 bits dac_scale_cck */
		.antennaGain = 0,
		.switchSettling = 0x2d,
		.adcDesiredSize = -30,
		.txEndToXpaOff = 0,
		.txEndToRxOn = 0x2,
		.txFrameToXpaOn = 0xe,
		.thresh62 = 28,
		.papdRateMaskHt20 = LE32(0x0cf0e0e0),
		.papdRateMaskHt40 = LE32(0x6cf0e0e0),
		.futureModal = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		},
	},
	.base_ext2 = {
		.tempSlopeLow = 72,
		.tempSlopeHigh = 105,
		.xatten1DBLow = {0x10, 0x14, 0x10},
		.xatten1MarginLow = {0x19, 0x19 , 0x19},
		.xatten1DBHigh = {0x1d, 0x20, 0x24},
		.xatten1MarginHigh = {0x10, 0x10, 0x10}
	},
	.calFreqPier5G = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5220, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5600, 0),
		FREQ2FBIN(5700, 0),
		FREQ2FBIN(5785, 0)
	},
	.calPierData5G = {
		{
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
		},
		{
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
		},
		{
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0},
		},

	},
	.calTarget_freqbin_5G = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5220, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5600, 0),
		FREQ2FBIN(5725, 0),
		FREQ2FBIN(5825, 0)
	},
	.calTarget_freqbin_5GHT20 = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5220, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5600, 0),
		FREQ2FBIN(5725, 0),
		FREQ2FBIN(5825, 0)
	},
	.calTarget_freqbin_5GHT40 = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5220, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5600, 0),
		FREQ2FBIN(5725, 0),
		FREQ2FBIN(5825, 0)
	},
	.calTargetPower5G = {
		/* 6-24,36,48,54 */
		{ {32, 32, 28, 26} },
		{ {32, 32, 28, 26} },
		{ {32, 32, 28, 26} },
		{ {32, 32, 26, 24} },
		{ {32, 32, 26, 24} },
		{ {32, 32, 24, 22} },
		{ {30, 30, 24, 22} },
		{ {30, 30, 24, 22} },
	},
	.calTargetPower5GHT20 = {
		/*
		 * 0_8_16,1-3_9-11_17-19,
		 * 4,5,6,7,12,13,14,15,20,21,22,23
		 */
		{ {32, 32, 32, 32, 28, 26, 32, 28, 26, 24, 24, 24, 22, 22} },
		{ {32, 32, 32, 32, 28, 26, 32, 28, 26, 24, 24, 24, 22, 22} },
		{ {32, 32, 32, 32, 28, 26, 32, 28, 26, 24, 24, 24, 22, 22} },
		{ {32, 32, 32, 32, 28, 26, 32, 26, 24, 22, 22, 22, 20, 20} },
		{ {32, 32, 32, 32, 28, 26, 32, 26, 24, 22, 20, 18, 16, 16} },
		{ {32, 32, 32, 32, 28, 26, 32, 24, 20, 16, 18, 16, 14, 14} },
		{ {30, 30, 30, 30, 28, 26, 30, 24, 20, 16, 18, 16, 14, 14} },
		{ {30, 30, 30, 30, 28, 26, 30, 24, 20, 16, 18, 16, 14, 14} },
	},
	.calTargetPower5GHT40 =  {
		/*
		 * 0_8_16,1-3_9-11_17-19,
		 * 4,5,6,7,12,13,14,15,20,21,22,23
		 */
		{ {32, 32, 32, 30, 28, 26, 30, 28, 26, 24, 24, 24, 22, 22} },
		{ {32, 32, 32, 30, 28, 26, 30, 28, 26, 24, 24, 24, 22, 22} },
		{ {32, 32, 32, 30, 28, 26, 30, 28, 26, 24, 24, 24, 22, 22} },
		{ {32, 32, 32, 30, 28, 26, 30, 26, 24, 22, 22, 22, 20, 20} },
		{ {32, 32, 32, 30, 28, 26, 30, 26, 24, 22, 20, 18, 16, 16} },
		{ {32, 32, 32, 30, 28, 26, 30, 22, 20, 16, 18, 16, 14, 14} },
		{ {30, 30, 30, 30, 28, 26, 30, 22, 20, 16, 18, 16, 14, 14} },
		{ {30, 30, 30, 30, 28, 26, 30, 22, 20, 16, 18, 16, 14, 14} },
	},
	.ctlIndex_5G =  {
		0x10, 0x16, 0x18, 0x40, 0x46,
		0x48, 0x30, 0x36, 0x38
	},
	.ctl_freqbin_5G =  {
		{
			/* Data[0].ctledges[0].bchannel */ FREQ2FBIN(5180, 0),
			/* Data[0].ctledges[1].bchannel */ FREQ2FBIN(5260, 0),
			/* Data[0].ctledges[2].bchannel */ FREQ2FBIN(5280, 0),
			/* Data[0].ctledges[3].bchannel */ FREQ2FBIN(5500, 0),
			/* Data[0].ctledges[4].bchannel */ FREQ2FBIN(5600, 0),
			/* Data[0].ctledges[5].bchannel */ FREQ2FBIN(5700, 0),
			/* Data[0].ctledges[6].bchannel */ FREQ2FBIN(5745, 0),
			/* Data[0].ctledges[7].bchannel */ FREQ2FBIN(5825, 0)
		},
		{
			/* Data[1].ctledges[0].bchannel */ FREQ2FBIN(5180, 0),
			/* Data[1].ctledges[1].bchannel */ FREQ2FBIN(5260, 0),
			/* Data[1].ctledges[2].bchannel */ FREQ2FBIN(5280, 0),
			/* Data[1].ctledges[3].bchannel */ FREQ2FBIN(5500, 0),
			/* Data[1].ctledges[4].bchannel */ FREQ2FBIN(5520, 0),
			/* Data[1].ctledges[5].bchannel */ FREQ2FBIN(5700, 0),
			/* Data[1].ctledges[6].bchannel */ FREQ2FBIN(5745, 0),
			/* Data[1].ctledges[7].bchannel */ FREQ2FBIN(5825, 0)
		},

		{
			/* Data[2].ctledges[0].bchannel */ FREQ2FBIN(5190, 0),
			/* Data[2].ctledges[1].bchannel */ FREQ2FBIN(5230, 0),
			/* Data[2].ctledges[2].bchannel */ FREQ2FBIN(5270, 0),
			/* Data[2].ctledges[3].bchannel */ FREQ2FBIN(5310, 0),
			/* Data[2].ctledges[4].bchannel */ FREQ2FBIN(5510, 0),
			/* Data[2].ctledges[5].bchannel */ FREQ2FBIN(5550, 0),
			/* Data[2].ctledges[6].bchannel */ FREQ2FBIN(5670, 0),
			/* Data[2].ctledges[7].bchannel */ FREQ2FBIN(5755, 0)
		},

		{
			/* Data[3].ctledges[0].bchannel */ FREQ2FBIN(5180, 0),
			/* Data[3].ctledges[1].bchannel */ FREQ2FBIN(5200, 0),
			/* Data[3].ctledges[2].bchannel */ FREQ2FBIN(5260, 0),
			/* Data[3].ctledges[3].bchannel */ FREQ2FBIN(5320, 0),
			/* Data[3].ctledges[4].bchannel */ FREQ2FBIN(5500, 0),
			/* Data[3].ctledges[5].bchannel */ FREQ2FBIN(5700, 0),
			/* Data[3].ctledges[6].bchannel */ 0xFF,
			/* Data[3].ctledges[7].bchannel */ 0xFF,
		},

		{
			/* Data[4].ctledges[0].bchannel */ FREQ2FBIN(5180, 0),
			/* Data[4].ctledges[1].bchannel */ FREQ2FBIN(5260, 0),
			/* Data[4].ctledges[2].bchannel */ FREQ2FBIN(5500, 0),
			/* Data[4].ctledges[3].bchannel */ FREQ2FBIN(5700, 0),
			/* Data[4].ctledges[4].bchannel */ 0xFF,
			/* Data[4].ctledges[5].bchannel */ 0xFF,
			/* Data[4].ctledges[6].bchannel */ 0xFF,
			/* Data[4].ctledges[7].bchannel */ 0xFF,
		},

		{
			/* Data[5].ctledges[0].bchannel */ FREQ2FBIN(5190, 0),
			/* Data[5].ctledges[1].bchannel */ FREQ2FBIN(5270, 0),
			/* Data[5].ctledges[2].bchannel */ FREQ2FBIN(5310, 0),
			/* Data[5].ctledges[3].bchannel */ FREQ2FBIN(5510, 0),
			/* Data[5].ctledges[4].bchannel */ FREQ2FBIN(5590, 0),
			/* Data[5].ctledges[5].bchannel */ FREQ2FBIN(5670, 0),
			/* Data[5].ctledges[6].bchannel */ 0xFF,
			/* Data[5].ctledges[7].bchannel */ 0xFF
		},

		{
			/* Data[6].ctledges[0].bchannel */ FREQ2FBIN(5180, 0),
			/* Data[6].ctledges[1].bchannel */ FREQ2FBIN(5200, 0),
			/* Data[6].ctledges[2].bchannel */ FREQ2FBIN(5220, 0),
			/* Data[6].ctledges[3].bchannel */ FREQ2FBIN(5260, 0),
			/* Data[6].ctledges[4].bchannel */ FREQ2FBIN(5500, 0),
			/* Data[6].ctledges[5].bchannel */ FREQ2FBIN(5600, 0),
			/* Data[6].ctledges[6].bchannel */ FREQ2FBIN(5700, 0),
			/* Data[6].ctledges[7].bchannel */ FREQ2FBIN(5745, 0)
		},

		{
			/* Data[7].ctledges[0].bchannel */ FREQ2FBIN(5180, 0),
			/* Data[7].ctledges[1].bchannel */ FREQ2FBIN(5260, 0),
			/* Data[7].ctledges[2].bchannel */ FREQ2FBIN(5320, 0),
			/* Data[7].ctledges[3].bchannel */ FREQ2FBIN(5500, 0),
			/* Data[7].ctledges[4].bchannel */ FREQ2FBIN(5560, 0),
			/* Data[7].ctledges[5].bchannel */ FREQ2FBIN(5700, 0),
			/* Data[7].ctledges[6].bchannel */ FREQ2FBIN(5745, 0),
			/* Data[7].ctledges[7].bchannel */ FREQ2FBIN(5825, 0)
		},

		{
			/* Data[8].ctledges[0].bchannel */ FREQ2FBIN(5190, 0),
			/* Data[8].ctledges[1].bchannel */ FREQ2FBIN(5230, 0),
			/* Data[8].ctledges[2].bchannel */ FREQ2FBIN(5270, 0),
			/* Data[8].ctledges[3].bchannel */ FREQ2FBIN(5510, 0),
			/* Data[8].ctledges[4].bchannel */ FREQ2FBIN(5550, 0),
			/* Data[8].ctledges[5].bchannel */ FREQ2FBIN(5670, 0),
			/* Data[8].ctledges[6].bchannel */ FREQ2FBIN(5755, 0),
			/* Data[8].ctledges[7].bchannel */ FREQ2FBIN(5795, 0)
		}
	},
	.ctlPowerData_5G = {
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
			}
		},
		{
			{
				CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 0),
				CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
				CTL(60, 0), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 0), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 0), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 0), CTL(60, 1),
			}
		},
	}
};

static const struct ar9300_eeprom ar9300_h116 = {
	.eepromVersion = 2,
	.templateVersion = 4,
	.macAddr = {0x00, 0x03, 0x7f, 0x0, 0x0, 0x0},
	.custData = {"h116-041-f0000"},
	.baseEepHeader = {
		.regDmn = { LE16(0), LE16(0x1f) },
		.txrxMask =  0x33, /* 4 bits tx and 4 bits rx */
		.opCapFlags = {
			.opFlags = AR5416_OPFLAGS_11G | AR5416_OPFLAGS_11A,
			.eepMisc = 0,
		},
		.rfSilent = 0,
		.blueToothOptions = 0,
		.deviceCap = 0,
		.deviceType = 5, /* takes lower byte in eeprom location */
		.pwrTableOffset = AR9300_PWR_TABLE_OFFSET,
		.params_for_tuning_caps = {0, 0},
		.featureEnable = 0x0d,
		 /*
		  * bit0 - enable tx temp comp - disabled
		  * bit1 - enable tx volt comp - disabled
		  * bit2 - enable fastClock - enabled
		  * bit3 - enable doubling - enabled
		  * bit4 - enable internal regulator - disabled
		  * bit5 - enable pa predistortion - disabled
		  */
		.miscConfiguration = 0, /* bit0 - turn down drivestrength */
		.eepromWriteEnableGpio = 6,
		.wlanDisableGpio = 0,
		.wlanLedGpio = 8,
		.rxBandSelectGpio = 0xff,
		.txrxgain = 0x10,
		.swreg = 0,
	 },
	.modalHeader2G = {
	/* ar9300_modal_eep_header  2g */
		/* 4 idle,t1,t2,b(4 bits per setting) */
		.antCtrlCommon = LE32(0x110),
		/* 4 ra1l1, ra2l1, ra1l2, ra2l2, ra12 */
		.antCtrlCommon2 = LE32(0x44444),

		/*
		 * antCtrlChain[AR9300_MAX_CHAINS]; 6 idle, t, r,
		 * rx1, rx12, b (2 bits each)
		 */
		.antCtrlChain = { LE16(0x10), LE16(0x10), LE16(0x10) },

		/*
		 * xatten1DB[AR9300_MAX_CHAINS];  3 xatten1_db
		 * for ar9280 (0xa20c/b20c 5:0)
		 */
		.xatten1DB = {0x1f, 0x1f, 0x1f},

		/*
		 * xatten1Margin[AR9300_MAX_CHAINS]; 3 xatten1_margin
		 * for ar9280 (0xa20c/b20c 16:12
		 */
		.xatten1Margin = {0x12, 0x12, 0x12},
		.tempSlope = 25,
		.voltSlope = 0,

		/*
		 * spurChans[OSPREY_EEPROM_MODAL_SPURS]; spur
		 * channels in usual fbin coding format
		 */
		.spurChans = {FREQ2FBIN(2464, 1), 0, 0, 0, 0},

		/*
		 * noiseFloorThreshCh[AR9300_MAX_CHAINS]; 3 Check
		 * if the register is per chain
		 */
		.noiseFloorThreshCh = {-1, 0, 0},
		.ob = {1, 1, 1},/* 3 chain */
		.db_stage2 = {1, 1, 1}, /* 3 chain  */
		.db_stage3 = {0, 0, 0},
		.db_stage4 = {0, 0, 0},
		.xpaBiasLvl = 0,
		.txFrameToDataStart = 0x0e,
		.txFrameToPaOn = 0x0e,
		.txClip = 3, /* 4 bits tx_clip, 4 bits dac_scale_cck */
		.antennaGain = 0,
		.switchSettling = 0x2c,
		.adcDesiredSize = -30,
		.txEndToXpaOff = 0,
		.txEndToRxOn = 0x2,
		.txFrameToXpaOn = 0xe,
		.thresh62 = 28,
		.papdRateMaskHt20 = LE32(0x0c80C080),
		.papdRateMaskHt40 = LE32(0x0080C080),
		.futureModal = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		},
	 },
	 .base_ext1 = {
		.ant_div_control = 0,
		.future = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	 },
	.calFreqPier2G = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1),
	 },
	/* ar9300_cal_data_per_freq_op_loop 2g */
	.calPierData2G = {
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
		{ {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0} },
	 },
	.calTarget_freqbin_Cck = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2472, 1),
	 },
	.calTarget_freqbin_2G = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	 },
	.calTarget_freqbin_2GHT20 = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	 },
	.calTarget_freqbin_2GHT40 = {
		FREQ2FBIN(2412, 1),
		FREQ2FBIN(2437, 1),
		FREQ2FBIN(2472, 1)
	 },
	.calTargetPowerCck = {
		 /* 1L-5L,5S,11L,11S */
		 { {34, 34, 34, 34} },
		 { {34, 34, 34, 34} },
	},
	.calTargetPower2G = {
		 /* 6-24,36,48,54 */
		 { {34, 34, 32, 32} },
		 { {34, 34, 32, 32} },
		 { {34, 34, 32, 32} },
	},
	.calTargetPower2GHT20 = {
		{ {32, 32, 32, 32, 32, 30, 32, 32, 30, 28, 0, 0, 0, 0} },
		{ {32, 32, 32, 32, 32, 30, 32, 32, 30, 28, 0, 0, 0, 0} },
		{ {32, 32, 32, 32, 32, 30, 32, 32, 30, 28, 0, 0, 0, 0} },
	},
	.calTargetPower2GHT40 = {
		{ {30, 30, 30, 30, 30, 28, 30, 30, 28, 26, 0, 0, 0, 0} },
		{ {30, 30, 30, 30, 30, 28, 30, 30, 28, 26, 0, 0, 0, 0} },
		{ {30, 30, 30, 30, 30, 28, 30, 30, 28, 26, 0, 0, 0, 0} },
	},
	.ctlIndex_2G =  {
		0x11, 0x12, 0x15, 0x17, 0x41, 0x42,
		0x45, 0x47, 0x31, 0x32, 0x35, 0x37,
	},
	.ctl_freqbin_2G = {
		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2457, 1),
			FREQ2FBIN(2462, 1)
		},
		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2462, 1),
			0xFF,
		},

		{
			FREQ2FBIN(2412, 1),
			FREQ2FBIN(2417, 1),
			FREQ2FBIN(2462, 1),
			0xFF,
		},
		{
			FREQ2FBIN(2422, 1),
			FREQ2FBIN(2427, 1),
			FREQ2FBIN(2447, 1),
			FREQ2FBIN(2452, 1)
		},

		{
			/* Data[4].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[4].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[4].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			/* Data[4].ctlEdges[3].bChannel */ FREQ2FBIN(2484, 1),
		},

		{
			/* Data[5].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[5].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[5].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			0,
		},

		{
			/* Data[6].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[6].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			FREQ2FBIN(2472, 1),
			0,
		},

		{
			/* Data[7].ctlEdges[0].bChannel */ FREQ2FBIN(2422, 1),
			/* Data[7].ctlEdges[1].bChannel */ FREQ2FBIN(2427, 1),
			/* Data[7].ctlEdges[2].bChannel */ FREQ2FBIN(2447, 1),
			/* Data[7].ctlEdges[3].bChannel */ FREQ2FBIN(2462, 1),
		},

		{
			/* Data[8].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[8].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[8].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
		},

		{
			/* Data[9].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[9].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[9].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			0
		},

		{
			/* Data[10].ctlEdges[0].bChannel */ FREQ2FBIN(2412, 1),
			/* Data[10].ctlEdges[1].bChannel */ FREQ2FBIN(2417, 1),
			/* Data[10].ctlEdges[2].bChannel */ FREQ2FBIN(2472, 1),
			0
		},

		{
			/* Data[11].ctlEdges[0].bChannel */ FREQ2FBIN(2422, 1),
			/* Data[11].ctlEdges[1].bChannel */ FREQ2FBIN(2427, 1),
			/* Data[11].ctlEdges[2].bChannel */ FREQ2FBIN(2447, 1),
			/* Data[11].ctlEdges[3].bChannel */ FREQ2FBIN(2462, 1),
		}
	 },
	.ctlPowerData_2G = {
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 1) } },

		 { { CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },

		 { { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },

		 { { CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 0) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 1) } },
		 { { CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 1) } },
	 },
	.modalHeader5G = {
		/* 4 idle,t1,t2,b (4 bits per setting) */
		.antCtrlCommon = LE32(0x220),
		/* 4 ra1l1, ra2l1, ra1l2,ra2l2,ra12 */
		.antCtrlCommon2 = LE32(0x44444),
		 /* antCtrlChain 6 idle, t,r,rx1,rx12,b (2 bits each) */
		.antCtrlChain = {
			LE16(0x150), LE16(0x150), LE16(0x150),
		},
		 /* xatten1DB 3 xatten1_db for AR9280 (0xa20c/b20c 5:0) */
		.xatten1DB = {0x19, 0x19, 0x19},

		/*
		 * xatten1Margin[AR9300_MAX_CHAINS]; 3 xatten1_margin
		 * for merlin (0xa20c/b20c 16:12
		 */
		.xatten1Margin = {0x14, 0x14, 0x14},
		.tempSlope = 70,
		.voltSlope = 0,
		/* spurChans spur channels in usual fbin coding format */
		.spurChans = {0, 0, 0, 0, 0},
		/* noiseFloorThreshCh Check if the register is per chain */
		.noiseFloorThreshCh = {-1, 0, 0},
		.ob = {3, 3, 3}, /* 3 chain */
		.db_stage2 = {3, 3, 3}, /* 3 chain */
		.db_stage3 = {3, 3, 3}, /* doesn't exist for 2G */
		.db_stage4 = {3, 3, 3},	 /* don't exist for 2G */
		.xpaBiasLvl = 0,
		.txFrameToDataStart = 0x0e,
		.txFrameToPaOn = 0x0e,
		.txClip = 3, /* 4 bits tx_clip, 4 bits dac_scale_cck */
		.antennaGain = 0,
		.switchSettling = 0x2d,
		.adcDesiredSize = -30,
		.txEndToXpaOff = 0,
		.txEndToRxOn = 0x2,
		.txFrameToXpaOn = 0xe,
		.thresh62 = 28,
		.papdRateMaskHt20 = LE32(0x0cf0e0e0),
		.papdRateMaskHt40 = LE32(0x6cf0e0e0),
		.futureModal = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		},
	 },
	.base_ext2 = {
		.tempSlopeLow = 35,
		.tempSlopeHigh = 50,
		.xatten1DBLow = {0, 0, 0},
		.xatten1MarginLow = {0, 0, 0},
		.xatten1DBHigh = {0, 0, 0},
		.xatten1MarginHigh = {0, 0, 0}
	 },
	.calFreqPier5G = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5220, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5600, 0),
		FREQ2FBIN(5700, 0),
		FREQ2FBIN(5785, 0)
	},
	.calPierData5G = {
			{
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
			},
			{
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
			},
			{
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0},
			},

	},
	.calTarget_freqbin_5G = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5240, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5600, 0),
		FREQ2FBIN(5700, 0),
		FREQ2FBIN(5825, 0)
	},
	.calTarget_freqbin_5GHT20 = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5240, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5700, 0),
		FREQ2FBIN(5745, 0),
		FREQ2FBIN(5825, 0)
	},
	.calTarget_freqbin_5GHT40 = {
		FREQ2FBIN(5180, 0),
		FREQ2FBIN(5240, 0),
		FREQ2FBIN(5320, 0),
		FREQ2FBIN(5400, 0),
		FREQ2FBIN(5500, 0),
		FREQ2FBIN(5700, 0),
		FREQ2FBIN(5745, 0),
		FREQ2FBIN(5825, 0)
	 },
	.calTargetPower5G = {
		/* 6-24,36,48,54 */
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
		{ {30, 30, 28, 24} },
	 },
	.calTargetPower5GHT20 = {
		/*
		 * 0_8_16,1-3_9-11_17-19,
		 * 4,5,6,7,12,13,14,15,20,21,22,23
		 */
		{ {30, 30, 30, 28, 24, 20, 30, 28, 24, 20, 0, 0, 0, 0} },
		{ {30, 30, 30, 28, 24, 20, 30, 28, 24, 20, 0, 0, 0, 0} },
		{ {30, 30, 30, 26, 22, 18, 30, 26, 22, 18, 0, 0, 0, 0} },
		{ {30, 30, 30, 26, 22, 18, 30, 26, 22, 18, 0, 0, 0, 0} },
		{ {30, 30, 30, 24, 20, 16, 30, 24, 20, 16, 0, 0, 0, 0} },
		{ {30, 30, 30, 24, 20, 16, 30, 24, 20, 16, 0, 0, 0, 0} },
		{ {30, 30, 30, 22, 18, 14, 30, 22, 18, 14, 0, 0, 0, 0} },
		{ {30, 30, 30, 22, 18, 14, 30, 22, 18, 14, 0, 0, 0, 0} },
	 },
	.calTargetPower5GHT40 =  {
		/*
		 * 0_8_16,1-3_9-11_17-19,
		 * 4,5,6,7,12,13,14,15,20,21,22,23
		 */
		{ {28, 28, 28, 26, 22, 18, 28, 26, 22, 18, 0, 0, 0, 0} },
		{ {28, 28, 28, 26, 22, 18, 28, 26, 22, 18, 0, 0, 0, 0} },
		{ {28, 28, 28, 24, 20, 16, 28, 24, 20, 16, 0, 0, 0, 0} },
		{ {28, 28, 28, 24, 20, 16, 28, 24, 20, 16, 0, 0, 0, 0} },
		{ {28, 28, 28, 22, 18, 14, 28, 22, 18, 14, 0, 0, 0, 0} },
		{ {28, 28, 28, 22, 18, 14, 28, 22, 18, 14, 0, 0, 0, 0} },
		{ {28, 28, 28, 20, 16, 12, 28, 20, 16, 12, 0, 0, 0, 0} },
		{ {28, 28, 28, 20, 16, 12, 28, 20, 16, 12, 0, 0, 0, 0} },
	 },
	.ctlIndex_5G =  {
		0x10, 0x16, 0x18, 0x40, 0x46,
		0x48, 0x30, 0x36, 0x38
	},
	.ctl_freqbin_5G =  {
		{
			/* Data[0].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[0].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[0].ctlEdges[2].bChannel */ FREQ2FBIN(5280, 0),
			/* Data[0].ctlEdges[3].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[0].ctlEdges[4].bChannel */ FREQ2FBIN(5600, 0),
			/* Data[0].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[0].ctlEdges[6].bChannel */ FREQ2FBIN(5745, 0),
			/* Data[0].ctlEdges[7].bChannel */ FREQ2FBIN(5825, 0)
		},
		{
			/* Data[1].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[1].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[1].ctlEdges[2].bChannel */ FREQ2FBIN(5280, 0),
			/* Data[1].ctlEdges[3].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[1].ctlEdges[4].bChannel */ FREQ2FBIN(5520, 0),
			/* Data[1].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[1].ctlEdges[6].bChannel */ FREQ2FBIN(5745, 0),
			/* Data[1].ctlEdges[7].bChannel */ FREQ2FBIN(5825, 0)
		},

		{
			/* Data[2].ctlEdges[0].bChannel */ FREQ2FBIN(5190, 0),
			/* Data[2].ctlEdges[1].bChannel */ FREQ2FBIN(5230, 0),
			/* Data[2].ctlEdges[2].bChannel */ FREQ2FBIN(5270, 0),
			/* Data[2].ctlEdges[3].bChannel */ FREQ2FBIN(5310, 0),
			/* Data[2].ctlEdges[4].bChannel */ FREQ2FBIN(5510, 0),
			/* Data[2].ctlEdges[5].bChannel */ FREQ2FBIN(5550, 0),
			/* Data[2].ctlEdges[6].bChannel */ FREQ2FBIN(5670, 0),
			/* Data[2].ctlEdges[7].bChannel */ FREQ2FBIN(5755, 0)
		},

		{
			/* Data[3].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[3].ctlEdges[1].bChannel */ FREQ2FBIN(5200, 0),
			/* Data[3].ctlEdges[2].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[3].ctlEdges[3].bChannel */ FREQ2FBIN(5320, 0),
			/* Data[3].ctlEdges[4].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[3].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[3].ctlEdges[6].bChannel */ 0xFF,
			/* Data[3].ctlEdges[7].bChannel */ 0xFF,
		},

		{
			/* Data[4].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[4].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[4].ctlEdges[2].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[4].ctlEdges[3].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[4].ctlEdges[4].bChannel */ 0xFF,
			/* Data[4].ctlEdges[5].bChannel */ 0xFF,
			/* Data[4].ctlEdges[6].bChannel */ 0xFF,
			/* Data[4].ctlEdges[7].bChannel */ 0xFF,
		},

		{
			/* Data[5].ctlEdges[0].bChannel */ FREQ2FBIN(5190, 0),
			/* Data[5].ctlEdges[1].bChannel */ FREQ2FBIN(5270, 0),
			/* Data[5].ctlEdges[2].bChannel */ FREQ2FBIN(5310, 0),
			/* Data[5].ctlEdges[3].bChannel */ FREQ2FBIN(5510, 0),
			/* Data[5].ctlEdges[4].bChannel */ FREQ2FBIN(5590, 0),
			/* Data[5].ctlEdges[5].bChannel */ FREQ2FBIN(5670, 0),
			/* Data[5].ctlEdges[6].bChannel */ 0xFF,
			/* Data[5].ctlEdges[7].bChannel */ 0xFF
		},

		{
			/* Data[6].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[6].ctlEdges[1].bChannel */ FREQ2FBIN(5200, 0),
			/* Data[6].ctlEdges[2].bChannel */ FREQ2FBIN(5220, 0),
			/* Data[6].ctlEdges[3].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[6].ctlEdges[4].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[6].ctlEdges[5].bChannel */ FREQ2FBIN(5600, 0),
			/* Data[6].ctlEdges[6].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[6].ctlEdges[7].bChannel */ FREQ2FBIN(5745, 0)
		},

		{
			/* Data[7].ctlEdges[0].bChannel */ FREQ2FBIN(5180, 0),
			/* Data[7].ctlEdges[1].bChannel */ FREQ2FBIN(5260, 0),
			/* Data[7].ctlEdges[2].bChannel */ FREQ2FBIN(5320, 0),
			/* Data[7].ctlEdges[3].bChannel */ FREQ2FBIN(5500, 0),
			/* Data[7].ctlEdges[4].bChannel */ FREQ2FBIN(5560, 0),
			/* Data[7].ctlEdges[5].bChannel */ FREQ2FBIN(5700, 0),
			/* Data[7].ctlEdges[6].bChannel */ FREQ2FBIN(5745, 0),
			/* Data[7].ctlEdges[7].bChannel */ FREQ2FBIN(5825, 0)
		},

		{
			/* Data[8].ctlEdges[0].bChannel */ FREQ2FBIN(5190, 0),
			/* Data[8].ctlEdges[1].bChannel */ FREQ2FBIN(5230, 0),
			/* Data[8].ctlEdges[2].bChannel */ FREQ2FBIN(5270, 0),
			/* Data[8].ctlEdges[3].bChannel */ FREQ2FBIN(5510, 0),
			/* Data[8].ctlEdges[4].bChannel */ FREQ2FBIN(5550, 0),
			/* Data[8].ctlEdges[5].bChannel */ FREQ2FBIN(5670, 0),
			/* Data[8].ctlEdges[6].bChannel */ FREQ2FBIN(5755, 0),
			/* Data[8].ctlEdges[7].bChannel */ FREQ2FBIN(5795, 0)
		}
	 },
	.ctlPowerData_5G = {
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 0), CTL(60, 1), CTL(60, 0), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
			}
		},
		{
			{
				CTL(60, 0), CTL(60, 1), CTL(60, 1), CTL(60, 0),
				CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
				CTL(60, 0), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 0), CTL(60, 0), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 1),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 1), CTL(60, 0), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 1), CTL(60, 0),
			}
		},
		{
			{
				CTL(60, 1), CTL(60, 0), CTL(60, 1), CTL(60, 1),
				CTL(60, 1), CTL(60, 1), CTL(60, 0), CTL(60, 1),
			}
		},
	 }
};


static const struct ar9300_eeprom *ar9300_eep_templates[] = {
	&ar9300_default,
	&ar9300_x112,
	&ar9300_h116,
	&ar9300_h112,
	&ar9300_x113,
};

static const struct ar9300_eeprom *ar9003_eeprom_struct_find_by_id(int id)
{
#define N_LOOP (sizeof(ar9300_eep_templates) / sizeof(ar9300_eep_templates[0]))
	int it;

	for (it = 0; it < N_LOOP; it++)
		if (ar9300_eep_templates[it]->templateVersion == id)
			return ar9300_eep_templates[it];
	return NULL;
#undef N_LOOP
}


static u16 ath9k_hw_fbin2freq(u8 fbin, bool is2GHz)
{
	if (fbin == AR5416_BCHAN_UNUSED)
		return fbin;

	return (u16) ((is2GHz) ? (2300 + fbin) : (4800 + 5 * fbin));
}

static int ath9k_hw_ar9300_check_eeprom(struct ath_hw *ah)
{
	return 0;
}

static int interpolate(int x, int xa, int xb, int ya, int yb)
{
	int bf, factor, plus;

	bf = 2 * (yb - ya) * (x - xa) / (xb - xa);
	factor = bf / 2;
	plus = bf % 2;
	return ya + factor + plus;
}

static u32 ath9k_hw_ar9300_get_eeprom(struct ath_hw *ah,
				      enum eeprom_param param)
{
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;
	struct ar9300_base_eep_hdr *pBase = &eep->baseEepHeader;

	switch (param) {
	case EEP_MAC_LSW:
		return eep->macAddr[0] << 8 | eep->macAddr[1];
	case EEP_MAC_MID:
		return eep->macAddr[2] << 8 | eep->macAddr[3];
	case EEP_MAC_MSW:
		return eep->macAddr[4] << 8 | eep->macAddr[5];
	case EEP_REG_0:
		return le16_to_cpu(pBase->regDmn[0]);
	case EEP_REG_1:
		return le16_to_cpu(pBase->regDmn[1]);
	case EEP_OP_CAP:
		return pBase->deviceCap;
	case EEP_OP_MODE:
		return pBase->opCapFlags.opFlags;
	case EEP_RF_SILENT:
		return pBase->rfSilent;
	case EEP_TX_MASK:
		return (pBase->txrxMask >> 4) & 0xf;
	case EEP_RX_MASK:
		return pBase->txrxMask & 0xf;
	case EEP_DRIVE_STRENGTH:
#define AR9300_EEP_BASE_DRIV_STRENGTH	0x1
		return pBase->miscConfiguration & AR9300_EEP_BASE_DRIV_STRENGTH;
	case EEP_INTERNAL_REGULATOR:
		/* Bit 4 is internal regulator flag */
		return (pBase->featureEnable & 0x10) >> 4;
	case EEP_SWREG:
		return le32_to_cpu(pBase->swreg);
	case EEP_PAPRD:
		return !!(pBase->featureEnable & BIT(5));
	case EEP_CHAIN_MASK_REDUCE:
		return (pBase->miscConfiguration >> 0x3) & 0x1;
	case EEP_ANT_DIV_CTL1:
		return le32_to_cpu(eep->base_ext1.ant_div_control);
	default:
		return 0;
	}
}

static bool ar9300_eeprom_read_byte(struct ath_common *common, int address,
				    u8 *buffer)
{
	u16 val;

	if (unlikely(!ath9k_hw_nvram_read(common, address / 2, &val)))
		return false;

	*buffer = (val >> (8 * (address % 2))) & 0xff;
	return true;
}

static bool ar9300_eeprom_read_word(struct ath_common *common, int address,
				    u8 *buffer)
{
	u16 val;

	if (unlikely(!ath9k_hw_nvram_read(common, address / 2, &val)))
		return false;

	buffer[0] = val >> 8;
	buffer[1] = val & 0xff;

	return true;
}

static bool ar9300_read_eeprom(struct ath_hw *ah, int address, u8 *buffer,
			       int count)
{
	struct ath_common *common = ath9k_hw_common(ah);
	int i;

	if ((address < 0) || ((address + count) / 2 > AR9300_EEPROM_SIZE - 1)) {
		ath_dbg(common, ATH_DBG_EEPROM,
			"eeprom address not in range\n");
		return false;
	}

	/*
	 * Since we're reading the bytes in reverse order from a little-endian
	 * word stream, an even address means we only use the lower half of
	 * the 16-bit word at that address
	 */
	if (address % 2 == 0) {
		if (!ar9300_eeprom_read_byte(common, address--, buffer++))
			goto error;

		count--;
	}

	for (i = 0; i < count / 2; i++) {
		if (!ar9300_eeprom_read_word(common, address, buffer))
			goto error;

		address -= 2;
		buffer += 2;
	}

	if (count % 2)
		if (!ar9300_eeprom_read_byte(common, address, buffer))
			goto error;

	return true;

error:
	ath_dbg(common, ATH_DBG_EEPROM,
		"unable to read eeprom region at offset %d\n", address);
	return false;
}

static bool ar9300_otp_read_word(struct ath_hw *ah, int addr, u32 *data)
{
	REG_READ(ah, AR9300_OTP_BASE + (4 * addr));

	if (!ath9k_hw_wait(ah, AR9300_OTP_STATUS, AR9300_OTP_STATUS_TYPE,
			   AR9300_OTP_STATUS_VALID, 1000))
		return false;

	*data = REG_READ(ah, AR9300_OTP_READ_DATA);
	return true;
}

static bool ar9300_read_otp(struct ath_hw *ah, int address, u8 *buffer,
			    int count)
{
	u32 data;
	int i;

	for (i = 0; i < count; i++) {
		int offset = 8 * ((address - i) % 4);
		if (!ar9300_otp_read_word(ah, (address - i) / 4, &data))
			return false;

		buffer[i] = (data >> offset) & 0xff;
	}

	return true;
}


static void ar9300_comp_hdr_unpack(u8 *best, int *code, int *reference,
				   int *length, int *major, int *minor)
{
	unsigned long value[4];

	value[0] = best[0];
	value[1] = best[1];
	value[2] = best[2];
	value[3] = best[3];
	*code = ((value[0] >> 5) & 0x0007);
	*reference = (value[0] & 0x001f) | ((value[1] >> 2) & 0x0020);
	*length = ((value[1] << 4) & 0x07f0) | ((value[2] >> 4) & 0x000f);
	*major = (value[2] & 0x000f);
	*minor = (value[3] & 0x00ff);
}

static u16 ar9300_comp_cksum(u8 *data, int dsize)
{
	int it, checksum = 0;

	for (it = 0; it < dsize; it++) {
		checksum += data[it];
		checksum &= 0xffff;
	}

	return checksum;
}

static bool ar9300_uncompress_block(struct ath_hw *ah,
				    u8 *mptr,
				    int mdataSize,
				    u8 *block,
				    int size)
{
	int it;
	int spot;
	int offset;
	int length;
	struct ath_common *common = ath9k_hw_common(ah);

	spot = 0;

	for (it = 0; it < size; it += (length+2)) {
		offset = block[it];
		offset &= 0xff;
		spot += offset;
		length = block[it+1];
		length &= 0xff;

		if (length > 0 && spot >= 0 && spot+length <= mdataSize) {
			ath_dbg(common, ATH_DBG_EEPROM,
				"Restore at %d: spot=%d offset=%d length=%d\n",
				it, spot, offset, length);
			memcpy(&mptr[spot], &block[it+2], length);
			spot += length;
		} else if (length > 0) {
			ath_dbg(common, ATH_DBG_EEPROM,
				"Bad restore at %d: spot=%d offset=%d length=%d\n",
				it, spot, offset, length);
			return false;
		}
	}
	return true;
}

static int ar9300_compress_decision(struct ath_hw *ah,
				    int it,
				    int code,
				    int reference,
				    u8 *mptr,
				    u8 *word, int length, int mdata_size)
{
	struct ath_common *common = ath9k_hw_common(ah);
	const struct ar9300_eeprom *eep = NULL;

	switch (code) {
	case _CompressNone:
		if (length != mdata_size) {
			ath_dbg(common, ATH_DBG_EEPROM,
				"EEPROM structure size mismatch memory=%d eeprom=%d\n",
				mdata_size, length);
			return -1;
		}
		memcpy(mptr, (u8 *) (word + COMP_HDR_LEN), length);
		ath_dbg(common, ATH_DBG_EEPROM,
			"restored eeprom %d: uncompressed, length %d\n",
			it, length);
		break;
	case _CompressBlock:
		if (reference == 0) {
		} else {
			eep = ar9003_eeprom_struct_find_by_id(reference);
			if (eep == NULL) {
				ath_dbg(common, ATH_DBG_EEPROM,
					"can't find reference eeprom struct %d\n",
					reference);
				return -1;
			}
			memcpy(mptr, eep, mdata_size);
		}
		ath_dbg(common, ATH_DBG_EEPROM,
			"restore eeprom %d: block, reference %d, length %d\n",
			it, reference, length);
		ar9300_uncompress_block(ah, mptr, mdata_size,
					(u8 *) (word + COMP_HDR_LEN), length);
		break;
	default:
		ath_dbg(common, ATH_DBG_EEPROM,
			"unknown compression code %d\n", code);
		return -1;
	}
	return 0;
}

typedef bool (*eeprom_read_op)(struct ath_hw *ah, int address, u8 *buffer,
			       int count);

static bool ar9300_check_header(void *data)
{
	u32 *word = data;
	return !(*word == 0 || *word == ~0);
}

static bool ar9300_check_eeprom_header(struct ath_hw *ah, eeprom_read_op read,
				       int base_addr)
{
	u8 header[4];

	if (!read(ah, base_addr, header, 4))
		return false;

	return ar9300_check_header(header);
}

static int ar9300_eeprom_restore_flash(struct ath_hw *ah, u8 *mptr,
				       int mdata_size)
{
	struct ath_common *common = ath9k_hw_common(ah);
	u16 *data = (u16 *) mptr;
	int i;

	for (i = 0; i < mdata_size / 2; i++, data++)
		ath9k_hw_nvram_read(common, i, data);

	return 0;
}
/*
 * Read the configuration data from the eeprom.
 * The data can be put in any specified memory buffer.
 *
 * Returns -1 on error.
 * Returns address of next memory location on success.
 */
static int ar9300_eeprom_restore_internal(struct ath_hw *ah,
					  u8 *mptr, int mdata_size)
{
#define MDEFAULT 15
#define MSTATE 100
	int cptr;
	u8 *word;
	int code;
	int reference, length, major, minor;
	int osize;
	int it;
	u16 checksum, mchecksum;
	struct ath_common *common = ath9k_hw_common(ah);
	eeprom_read_op read;

	if (ath9k_hw_use_flash(ah))
		return ar9300_eeprom_restore_flash(ah, mptr, mdata_size);

	word = kzalloc(2048, GFP_KERNEL);
	if (!word)
		return -1;

	memcpy(mptr, &ar9300_default, mdata_size);

	read = ar9300_read_eeprom;
	if (AR_SREV_9485(ah))
		cptr = AR9300_BASE_ADDR_4K;
	else
		cptr = AR9300_BASE_ADDR;
	ath_dbg(common, ATH_DBG_EEPROM,
		"Trying EEPROM access at Address 0x%04x\n", cptr);
	if (ar9300_check_eeprom_header(ah, read, cptr))
		goto found;

	cptr = AR9300_BASE_ADDR_512;
	ath_dbg(common, ATH_DBG_EEPROM,
		"Trying EEPROM access at Address 0x%04x\n", cptr);
	if (ar9300_check_eeprom_header(ah, read, cptr))
		goto found;

	read = ar9300_read_otp;
	cptr = AR9300_BASE_ADDR;
	ath_dbg(common, ATH_DBG_EEPROM,
		"Trying OTP access at Address 0x%04x\n", cptr);
	if (ar9300_check_eeprom_header(ah, read, cptr))
		goto found;

	cptr = AR9300_BASE_ADDR_512;
	ath_dbg(common, ATH_DBG_EEPROM,
		"Trying OTP access at Address 0x%04x\n", cptr);
	if (ar9300_check_eeprom_header(ah, read, cptr))
		goto found;

	goto fail;

found:
	ath_dbg(common, ATH_DBG_EEPROM, "Found valid EEPROM data\n");

	for (it = 0; it < MSTATE; it++) {
		if (!read(ah, cptr, word, COMP_HDR_LEN))
			goto fail;

		if (!ar9300_check_header(word))
			break;

		ar9300_comp_hdr_unpack(word, &code, &reference,
				       &length, &major, &minor);
		ath_dbg(common, ATH_DBG_EEPROM,
			"Found block at %x: code=%d ref=%d length=%d major=%d minor=%d\n",
			cptr, code, reference, length, major, minor);
		if ((!AR_SREV_9485(ah) && length >= 1024) ||
		    (AR_SREV_9485(ah) && length > EEPROM_DATA_LEN_9485)) {
			ath_dbg(common, ATH_DBG_EEPROM,
				"Skipping bad header\n");
			cptr -= COMP_HDR_LEN;
			continue;
		}

		osize = length;
		read(ah, cptr, word, COMP_HDR_LEN + osize + COMP_CKSUM_LEN);
		checksum = ar9300_comp_cksum(&word[COMP_HDR_LEN], length);
		mchecksum = word[COMP_HDR_LEN + osize] |
		    (word[COMP_HDR_LEN + osize + 1] << 8);
		ath_dbg(common, ATH_DBG_EEPROM,
			"checksum %x %x\n", checksum, mchecksum);
		if (checksum == mchecksum) {
			ar9300_compress_decision(ah, it, code, reference, mptr,
						 word, length, mdata_size);
		} else {
			ath_dbg(common, ATH_DBG_EEPROM,
				"skipping block with bad checksum\n");
		}
		cptr -= (COMP_HDR_LEN + osize + COMP_CKSUM_LEN);
	}

	kfree(word);
	return cptr;

fail:
	kfree(word);
	return -1;
}

/*
 * Restore the configuration structure by reading the eeprom.
 * This function destroys any existing in-memory structure
 * content.
 */
static bool ath9k_hw_ar9300_fill_eeprom(struct ath_hw *ah)
{
	u8 *mptr = (u8 *) &ah->eeprom.ar9300_eep;

	if (ar9300_eeprom_restore_internal(ah, mptr,
			sizeof(struct ar9300_eeprom)) < 0)
		return false;

	return true;
}

/* XXX: review hardware docs */
static int ath9k_hw_ar9300_get_eeprom_ver(struct ath_hw *ah)
{
	return ah->eeprom.ar9300_eep.eepromVersion;
}

/* XXX: could be read from the eepromVersion, not sure yet */
static int ath9k_hw_ar9300_get_eeprom_rev(struct ath_hw *ah)
{
	return 0;
}

static s32 ar9003_hw_xpa_bias_level_get(struct ath_hw *ah, bool is2ghz)
{
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;

	if (is2ghz)
		return eep->modalHeader2G.xpaBiasLvl;
	else
		return eep->modalHeader5G.xpaBiasLvl;
}

static void ar9003_hw_xpa_bias_level_apply(struct ath_hw *ah, bool is2ghz)
{
	int bias = ar9003_hw_xpa_bias_level_get(ah, is2ghz);

	if (AR_SREV_9485(ah) || AR_SREV_9340(ah))
		REG_RMW_FIELD(ah, AR_CH0_TOP2, AR_CH0_TOP2_XPABIASLVL, bias);
	else {
		REG_RMW_FIELD(ah, AR_CH0_TOP, AR_CH0_TOP_XPABIASLVL, bias);
		REG_RMW_FIELD(ah, AR_CH0_THERM,
				AR_CH0_THERM_XPABIASLVL_MSB,
				bias >> 2);
		REG_RMW_FIELD(ah, AR_CH0_THERM,
				AR_CH0_THERM_XPASHORT2GND, 1);
	}
}

static u32 ar9003_hw_ant_ctrl_common_get(struct ath_hw *ah, bool is2ghz)
{
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;
	__le32 val;

	if (is2ghz)
		val = eep->modalHeader2G.antCtrlCommon;
	else
		val = eep->modalHeader5G.antCtrlCommon;
	return le32_to_cpu(val);
}

static u32 ar9003_hw_ant_ctrl_common_2_get(struct ath_hw *ah, bool is2ghz)
{
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;
	__le32 val;

	if (is2ghz)
		val = eep->modalHeader2G.antCtrlCommon2;
	else
		val = eep->modalHeader5G.antCtrlCommon2;
	return le32_to_cpu(val);
}

static u16 ar9003_hw_ant_ctrl_chain_get(struct ath_hw *ah,
					int chain,
					bool is2ghz)
{
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;
	__le16 val = 0;

	if (chain >= 0 && chain < AR9300_MAX_CHAINS) {
		if (is2ghz)
			val = eep->modalHeader2G.antCtrlChain[chain];
		else
			val = eep->modalHeader5G.antCtrlChain[chain];
	}

	return le16_to_cpu(val);
}

static void ar9003_hw_ant_ctrl_apply(struct ath_hw *ah, bool is2ghz)
{
	int chain;
	u32 regval;
	u32 ant_div_ctl1;
	static const u32 switch_chain_reg[AR9300_MAX_CHAINS] = {
			AR_PHY_SWITCH_CHAIN_0,
			AR_PHY_SWITCH_CHAIN_1,
			AR_PHY_SWITCH_CHAIN_2,
	};

	u32 value = ar9003_hw_ant_ctrl_common_get(ah, is2ghz);

	REG_RMW_FIELD(ah, AR_PHY_SWITCH_COM, AR_SWITCH_TABLE_COM_ALL, value);

	value = ar9003_hw_ant_ctrl_common_2_get(ah, is2ghz);
	REG_RMW_FIELD(ah, AR_PHY_SWITCH_COM_2, AR_SWITCH_TABLE_COM2_ALL, value);

	for (chain = 0; chain < AR9300_MAX_CHAINS; chain++) {
		if ((ah->rxchainmask & BIT(chain)) ||
		    (ah->txchainmask & BIT(chain))) {
			value = ar9003_hw_ant_ctrl_chain_get(ah, chain,
							     is2ghz);
			REG_RMW_FIELD(ah, switch_chain_reg[chain],
				      AR_SWITCH_TABLE_ALL, value);
		}
	}

	if (AR_SREV_9485(ah)) {
		value = ath9k_hw_ar9300_get_eeprom(ah, EEP_ANT_DIV_CTL1);
		/*
		 * main_lnaconf, alt_lnaconf, main_tb, alt_tb
		 * are the fields present
		 */
		regval = REG_READ(ah, AR_PHY_MC_GAIN_CTRL);
		regval &= (~AR_ANT_DIV_CTRL_ALL);
		regval |= (value & 0x3f) << AR_ANT_DIV_CTRL_ALL_S;
		/* enable_lnadiv */
		regval &= (~AR_PHY_9485_ANT_DIV_LNADIV);
		regval |= ((value >> 6) & 0x1) <<
				AR_PHY_9485_ANT_DIV_LNADIV_S;
		REG_WRITE(ah, AR_PHY_MC_GAIN_CTRL, regval);

		/*enable fast_div */
		regval = REG_READ(ah, AR_PHY_CCK_DETECT);
		regval &= (~AR_FAST_DIV_ENABLE);
		regval |= ((value >> 7) & 0x1) <<
				AR_FAST_DIV_ENABLE_S;
		REG_WRITE(ah, AR_PHY_CCK_DETECT, regval);
		ant_div_ctl1 =
			ah->eep_ops->get_eeprom(ah, EEP_ANT_DIV_CTL1);
		/* check whether antenna diversity is enabled */
		if ((ant_div_ctl1 >> 0x6) == 0x3) {
			regval = REG_READ(ah, AR_PHY_MC_GAIN_CTRL);
			/*
			 * clear bits 25-30 main_lnaconf, alt_lnaconf,
			 * main_tb, alt_tb
			 */
			regval &= (~(AR_PHY_9485_ANT_DIV_MAIN_LNACONF |
					AR_PHY_9485_ANT_DIV_ALT_LNACONF |
					AR_PHY_9485_ANT_DIV_ALT_GAINTB |
					AR_PHY_9485_ANT_DIV_MAIN_GAINTB));
			/* by default use LNA1 for the main antenna */
			regval |= (AR_PHY_9485_ANT_DIV_LNA1 <<
					AR_PHY_9485_ANT_DIV_MAIN_LNACONF_S);
			regval |= (AR_PHY_9485_ANT_DIV_LNA2 <<
					AR_PHY_9485_ANT_DIV_ALT_LNACONF_S);
			REG_WRITE(ah, AR_PHY_MC_GAIN_CTRL, regval);
		}


	}

}

static void ar9003_hw_drive_strength_apply(struct ath_hw *ah)
{
	int drive_strength;
	unsigned long reg;

	drive_strength = ath9k_hw_ar9300_get_eeprom(ah, EEP_DRIVE_STRENGTH);

	if (!drive_strength)
		return;

	reg = REG_READ(ah, AR_PHY_65NM_CH0_BIAS1);
	reg &= ~0x00ffffc0;
	reg |= 0x5 << 21;
	reg |= 0x5 << 18;
	reg |= 0x5 << 15;
	reg |= 0x5 << 12;
	reg |= 0x5 << 9;
	reg |= 0x5 << 6;
	REG_WRITE(ah, AR_PHY_65NM_CH0_BIAS1, reg);

	reg = REG_READ(ah, AR_PHY_65NM_CH0_BIAS2);
	reg &= ~0xffffffe0;
	reg |= 0x5 << 29;
	reg |= 0x5 << 26;
	reg |= 0x5 << 23;
	reg |= 0x5 << 20;
	reg |= 0x5 << 17;
	reg |= 0x5 << 14;
	reg |= 0x5 << 11;
	reg |= 0x5 << 8;
	reg |= 0x5 << 5;
	REG_WRITE(ah, AR_PHY_65NM_CH0_BIAS2, reg);

	reg = REG_READ(ah, AR_PHY_65NM_CH0_BIAS4);
	reg &= ~0xff800000;
	reg |= 0x5 << 29;
	reg |= 0x5 << 26;
	reg |= 0x5 << 23;
	REG_WRITE(ah, AR_PHY_65NM_CH0_BIAS4, reg);
}

static u16 ar9003_hw_atten_chain_get(struct ath_hw *ah, int chain,
				     struct ath9k_channel *chan)
{
	int f[3], t[3];
	u16 value;
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;

	if (chain >= 0 && chain < 3) {
		if (IS_CHAN_2GHZ(chan))
			return eep->modalHeader2G.xatten1DB[chain];
		else if (eep->base_ext2.xatten1DBLow[chain] != 0) {
			t[0] = eep->base_ext2.xatten1DBLow[chain];
			f[0] = 5180;
			t[1] = eep->modalHeader5G.xatten1DB[chain];
			f[1] = 5500;
			t[2] = eep->base_ext2.xatten1DBHigh[chain];
			f[2] = 5785;
			value = ar9003_hw_power_interpolate((s32) chan->channel,
							    f, t, 3);
			return value;
		} else
			return eep->modalHeader5G.xatten1DB[chain];
	}

	return 0;
}


static u16 ar9003_hw_atten_chain_get_margin(struct ath_hw *ah, int chain,
					    struct ath9k_channel *chan)
{
	int f[3], t[3];
	u16 value;
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;

	if (chain >= 0 && chain < 3) {
		if (IS_CHAN_2GHZ(chan))
			return eep->modalHeader2G.xatten1Margin[chain];
		else if (eep->base_ext2.xatten1MarginLow[chain] != 0) {
			t[0] = eep->base_ext2.xatten1MarginLow[chain];
			f[0] = 5180;
			t[1] = eep->modalHeader5G.xatten1Margin[chain];
			f[1] = 5500;
			t[2] = eep->base_ext2.xatten1MarginHigh[chain];
			f[2] = 5785;
			value = ar9003_hw_power_interpolate((s32) chan->channel,
							    f, t, 3);
			return value;
		} else
			return eep->modalHeader5G.xatten1Margin[chain];
	}

	return 0;
}

static void ar9003_hw_atten_apply(struct ath_hw *ah, struct ath9k_channel *chan)
{
	int i;
	u16 value;
	unsigned long ext_atten_reg[3] = {AR_PHY_EXT_ATTEN_CTL_0,
					  AR_PHY_EXT_ATTEN_CTL_1,
					  AR_PHY_EXT_ATTEN_CTL_2,
					 };

	/* Test value. if 0 then attenuation is unused. Don't load anything. */
	for (i = 0; i < 3; i++) {
		if (ah->txchainmask & BIT(i)) {
			value = ar9003_hw_atten_chain_get(ah, i, chan);
			REG_RMW_FIELD(ah, ext_atten_reg[i],
				      AR_PHY_EXT_ATTEN_CTL_XATTEN1_DB, value);

			value = ar9003_hw_atten_chain_get_margin(ah, i, chan);
			REG_RMW_FIELD(ah, ext_atten_reg[i],
				      AR_PHY_EXT_ATTEN_CTL_XATTEN1_MARGIN,
				      value);
		}
	}
}

static bool is_pmu_set(struct ath_hw *ah, u32 pmu_reg, int pmu_set)
{
	int timeout = 100;

	while (pmu_set != REG_READ(ah, pmu_reg)) {
		if (timeout-- == 0)
			return false;
		REG_WRITE(ah, pmu_reg, pmu_set);
		udelay(10);
	}

	return true;
}

static void ar9003_hw_internal_regulator_apply(struct ath_hw *ah)
{
	int internal_regulator =
		ath9k_hw_ar9300_get_eeprom(ah, EEP_INTERNAL_REGULATOR);

	if (internal_regulator) {
		if (AR_SREV_9485(ah)) {
			int reg_pmu_set;

			reg_pmu_set = REG_READ(ah, AR_PHY_PMU2) & ~AR_PHY_PMU2_PGM;
			REG_WRITE(ah, AR_PHY_PMU2, reg_pmu_set);
			if (!is_pmu_set(ah, AR_PHY_PMU2, reg_pmu_set))
				return;

			reg_pmu_set = (5 << 1) | (7 << 4) | (1 << 8) |
				      (2 << 14) | (6 << 17) | (1 << 20) |
				      (3 << 24) | (1 << 28);

			REG_WRITE(ah, AR_PHY_PMU1, reg_pmu_set);
			if (!is_pmu_set(ah, AR_PHY_PMU1, reg_pmu_set))
				return;

			reg_pmu_set = (REG_READ(ah, AR_PHY_PMU2) & ~0xFFC00000)
					| (4 << 26);
			REG_WRITE(ah, AR_PHY_PMU2, reg_pmu_set);
			if (!is_pmu_set(ah, AR_PHY_PMU2, reg_pmu_set))
				return;

			reg_pmu_set = (REG_READ(ah, AR_PHY_PMU2) & ~0x00200000)
					| (1 << 21);
			REG_WRITE(ah, AR_PHY_PMU2, reg_pmu_set);
			if (!is_pmu_set(ah, AR_PHY_PMU2, reg_pmu_set))
				return;
		} else {
			/* Internal regulator is ON. Write swreg register. */
			int swreg = ath9k_hw_ar9300_get_eeprom(ah, EEP_SWREG);
			REG_WRITE(ah, AR_RTC_REG_CONTROL1,
				  REG_READ(ah, AR_RTC_REG_CONTROL1) &
				  (~AR_RTC_REG_CONTROL1_SWREG_PROGRAM));
			REG_WRITE(ah, AR_RTC_REG_CONTROL0, swreg);
			/* Set REG_CONTROL1.SWREG_PROGRAM */
			REG_WRITE(ah, AR_RTC_REG_CONTROL1,
				  REG_READ(ah,
					   AR_RTC_REG_CONTROL1) |
					   AR_RTC_REG_CONTROL1_SWREG_PROGRAM);
		}
	} else {
		if (AR_SREV_9485(ah)) {
			REG_RMW_FIELD(ah, AR_PHY_PMU2, AR_PHY_PMU2_PGM, 0);
			while (REG_READ_FIELD(ah, AR_PHY_PMU2,
					      AR_PHY_PMU2_PGM))
				udelay(10);

			REG_RMW_FIELD(ah, AR_PHY_PMU1, AR_PHY_PMU1_PWD, 0x1);
			while (!REG_READ_FIELD(ah, AR_PHY_PMU1,
					       AR_PHY_PMU1_PWD))
				udelay(10);
			REG_RMW_FIELD(ah, AR_PHY_PMU2, AR_PHY_PMU2_PGM, 0x1);
			while (!REG_READ_FIELD(ah, AR_PHY_PMU2,
					      AR_PHY_PMU2_PGM))
				udelay(10);
		} else
			REG_WRITE(ah, AR_RTC_SLEEP_CLK,
				  (REG_READ(ah,
				   AR_RTC_SLEEP_CLK) |
				   AR_RTC_FORCE_SWREG_PRD));
	}

}

static void ar9003_hw_apply_tuning_caps(struct ath_hw *ah)
{
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;
	u8 tuning_caps_param = eep->baseEepHeader.params_for_tuning_caps[0];

	if (eep->baseEepHeader.featureEnable & 0x40) {
		tuning_caps_param &= 0x7f;
		REG_RMW_FIELD(ah, AR_CH0_XTAL, AR_CH0_XTAL_CAPINDAC,
			      tuning_caps_param);
		REG_RMW_FIELD(ah, AR_CH0_XTAL, AR_CH0_XTAL_CAPOUTDAC,
			      tuning_caps_param);
	}
}

static void ath9k_hw_ar9300_set_board_values(struct ath_hw *ah,
					     struct ath9k_channel *chan)
{
	ar9003_hw_xpa_bias_level_apply(ah, IS_CHAN_2GHZ(chan));
	ar9003_hw_ant_ctrl_apply(ah, IS_CHAN_2GHZ(chan));
	ar9003_hw_drive_strength_apply(ah);
	ar9003_hw_atten_apply(ah, chan);
	if (!AR_SREV_9330(ah) && !AR_SREV_9340(ah))
		ar9003_hw_internal_regulator_apply(ah);
	if (AR_SREV_9485(ah) || AR_SREV_9340(ah))
		ar9003_hw_apply_tuning_caps(ah);
}

static void ath9k_hw_ar9300_set_addac(struct ath_hw *ah,
				      struct ath9k_channel *chan)
{
}

/*
 * Returns the interpolated y value corresponding to the specified x value
 * from the np ordered pairs of data (px,py).
 * The pairs do not have to be in any order.
 * If the specified x value is less than any of the px,
 * the returned y value is equal to the py for the lowest px.
 * If the specified x value is greater than any of the px,
 * the returned y value is equal to the py for the highest px.
 */
static int ar9003_hw_power_interpolate(int32_t x,
				       int32_t *px, int32_t *py, u_int16_t np)
{
	int ip = 0;
	int lx = 0, ly = 0, lhave = 0;
	int hx = 0, hy = 0, hhave = 0;
	int dx = 0;
	int y = 0;

	lhave = 0;
	hhave = 0;

	/* identify best lower and higher x calibration measurement */
	for (ip = 0; ip < np; ip++) {
		dx = x - px[ip];

		/* this measurement is higher than our desired x */
		if (dx <= 0) {
			if (!hhave || dx > (x - hx)) {
				/* new best higher x measurement */
				hx = px[ip];
				hy = py[ip];
				hhave = 1;
			}
		}
		/* this measurement is lower than our desired x */
		if (dx >= 0) {
			if (!lhave || dx < (x - lx)) {
				/* new best lower x measurement */
				lx = px[ip];
				ly = py[ip];
				lhave = 1;
			}
		}
	}

	/* the low x is good */
	if (lhave) {
		/* so is the high x */
		if (hhave) {
			/* they're the same, so just pick one */
			if (hx == lx)
				y = ly;
			else	/* interpolate  */
				y = interpolate(x, lx, hx, ly, hy);
		} else		/* only low is good, use it */
			y = ly;
	} else if (hhave)	/* only high is good, use it */
		y = hy;
	else /* nothing is good,this should never happen unless np=0, ???? */
		y = -(1 << 30);
	return y;
}

static u8 ar9003_hw_eeprom_get_tgt_pwr(struct ath_hw *ah,
				       u16 rateIndex, u16 freq, bool is2GHz)
{
	u16 numPiers, i;
	s32 targetPowerArray[AR9300_NUM_5G_20_TARGET_POWERS];
	s32 freqArray[AR9300_NUM_5G_20_TARGET_POWERS];
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;
	struct cal_tgt_pow_legacy *pEepromTargetPwr;
	u8 *pFreqBin;

	if (is2GHz) {
		numPiers = AR9300_NUM_2G_20_TARGET_POWERS;
		pEepromTargetPwr = eep->calTargetPower2G;
		pFreqBin = eep->calTarget_freqbin_2G;
	} else {
		numPiers = AR9300_NUM_5G_20_TARGET_POWERS;
		pEepromTargetPwr = eep->calTargetPower5G;
		pFreqBin = eep->calTarget_freqbin_5G;
	}

	/*
	 * create array of channels and targetpower from
	 * targetpower piers stored on eeprom
	 */
	for (i = 0; i < numPiers; i++) {
		freqArray[i] = FBIN2FREQ(pFreqBin[i], is2GHz);
		targetPowerArray[i] = pEepromTargetPwr[i].tPow2x[rateIndex];
	}

	/* interpolate to get target power for given frequency */
	return (u8) ar9003_hw_power_interpolate((s32) freq,
						 freqArray,
						 targetPowerArray, numPiers);
}

static u8 ar9003_hw_eeprom_get_ht20_tgt_pwr(struct ath_hw *ah,
					    u16 rateIndex,
					    u16 freq, bool is2GHz)
{
	u16 numPiers, i;
	s32 targetPowerArray[AR9300_NUM_5G_20_TARGET_POWERS];
	s32 freqArray[AR9300_NUM_5G_20_TARGET_POWERS];
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;
	struct cal_tgt_pow_ht *pEepromTargetPwr;
	u8 *pFreqBin;

	if (is2GHz) {
		numPiers = AR9300_NUM_2G_20_TARGET_POWERS;
		pEepromTargetPwr = eep->calTargetPower2GHT20;
		pFreqBin = eep->calTarget_freqbin_2GHT20;
	} else {
		numPiers = AR9300_NUM_5G_20_TARGET_POWERS;
		pEepromTargetPwr = eep->calTargetPower5GHT20;
		pFreqBin = eep->calTarget_freqbin_5GHT20;
	}

	/*
	 * create array of channels and targetpower
	 * from targetpower piers stored on eeprom
	 */
	for (i = 0; i < numPiers; i++) {
		freqArray[i] = FBIN2FREQ(pFreqBin[i], is2GHz);
		targetPowerArray[i] = pEepromTargetPwr[i].tPow2x[rateIndex];
	}

	/* interpolate to get target power for given frequency */
	return (u8) ar9003_hw_power_interpolate((s32) freq,
						 freqArray,
						 targetPowerArray, numPiers);
}

static u8 ar9003_hw_eeprom_get_ht40_tgt_pwr(struct ath_hw *ah,
					    u16 rateIndex,
					    u16 freq, bool is2GHz)
{
	u16 numPiers, i;
	s32 targetPowerArray[AR9300_NUM_5G_40_TARGET_POWERS];
	s32 freqArray[AR9300_NUM_5G_40_TARGET_POWERS];
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;
	struct cal_tgt_pow_ht *pEepromTargetPwr;
	u8 *pFreqBin;

	if (is2GHz) {
		numPiers = AR9300_NUM_2G_40_TARGET_POWERS;
		pEepromTargetPwr = eep->calTargetPower2GHT40;
		pFreqBin = eep->calTarget_freqbin_2GHT40;
	} else {
		numPiers = AR9300_NUM_5G_40_TARGET_POWERS;
		pEepromTargetPwr = eep->calTargetPower5GHT40;
		pFreqBin = eep->calTarget_freqbin_5GHT40;
	}

	/*
	 * create array of channels and targetpower from
	 * targetpower piers stored on eeprom
	 */
	for (i = 0; i < numPiers; i++) {
		freqArray[i] = FBIN2FREQ(pFreqBin[i], is2GHz);
		targetPowerArray[i] = pEepromTargetPwr[i].tPow2x[rateIndex];
	}

	/* interpolate to get target power for given frequency */
	return (u8) ar9003_hw_power_interpolate((s32) freq,
						 freqArray,
						 targetPowerArray, numPiers);
}

static u8 ar9003_hw_eeprom_get_cck_tgt_pwr(struct ath_hw *ah,
					   u16 rateIndex, u16 freq)
{
	u16 numPiers = AR9300_NUM_2G_CCK_TARGET_POWERS, i;
	s32 targetPowerArray[AR9300_NUM_2G_CCK_TARGET_POWERS];
	s32 freqArray[AR9300_NUM_2G_CCK_TARGET_POWERS];
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;
	struct cal_tgt_pow_legacy *pEepromTargetPwr = eep->calTargetPowerCck;
	u8 *pFreqBin = eep->calTarget_freqbin_Cck;

	/*
	 * create array of channels and targetpower from
	 * targetpower piers stored on eeprom
	 */
	for (i = 0; i < numPiers; i++) {
		freqArray[i] = FBIN2FREQ(pFreqBin[i], 1);
		targetPowerArray[i] = pEepromTargetPwr[i].tPow2x[rateIndex];
	}

	/* interpolate to get target power for given frequency */
	return (u8) ar9003_hw_power_interpolate((s32) freq,
						 freqArray,
						 targetPowerArray, numPiers);
}

/* Set tx power registers to array of values passed in */
static int ar9003_hw_tx_power_regwrite(struct ath_hw *ah, u8 * pPwrArray)
{
#define POW_SM(_r, _s)     (((_r) & 0x3f) << (_s))
	/* make sure forced gain is not set */
	REG_WRITE(ah, AR_PHY_TX_FORCED_GAIN, 0);

	/* Write the OFDM power per rate set */

	/* 6 (LSB), 9, 12, 18 (MSB) */
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE(0),
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24], 24) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24], 16) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24], 8) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24], 0));

	/* 24 (LSB), 36, 48, 54 (MSB) */
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE(1),
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_54], 24) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_48], 16) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_36], 8) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24], 0));

	/* Write the CCK power per rate set */

	/* 1L (LSB), reserved, 2L, 2S (MSB) */
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE(2),
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_1L_5L], 24) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_1L_5L], 16) |
		  /* POW_SM(txPowerTimes2,  8) | this is reserved for AR9003 */
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_1L_5L], 0));

	/* 5.5L (LSB), 5.5S, 11L, 11S (MSB) */
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE(3),
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_11S], 24) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_11L], 16) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_5S], 8) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_1L_5L], 0)
	    );

        /* Write the power for duplicated frames - HT40 */

        /* dup40_cck (LSB), dup40_ofdm, ext20_cck, ext20_ofdm (MSB) */
	REG_WRITE(ah, 0xa3e0,
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24], 24) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_1L_5L], 16) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24],  8) |
		  POW_SM(pPwrArray[ALL_TARGET_LEGACY_1L_5L],  0)
	    );

	/* Write the HT20 power per rate set */

	/* 0/8/16 (LSB), 1-3/9-11/17-19, 4, 5 (MSB) */
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE(4),
		  POW_SM(pPwrArray[ALL_TARGET_HT20_5], 24) |
		  POW_SM(pPwrArray[ALL_TARGET_HT20_4], 16) |
		  POW_SM(pPwrArray[ALL_TARGET_HT20_1_3_9_11_17_19], 8) |
		  POW_SM(pPwrArray[ALL_TARGET_HT20_0_8_16], 0)
	    );

	/* 6 (LSB), 7, 12, 13 (MSB) */
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE(5),
		  POW_SM(pPwrArray[ALL_TARGET_HT20_13], 24) |
		  POW_SM(pPwrArray[ALL_TARGET_HT20_12], 16) |
		  POW_SM(pPwrArray[ALL_TARGET_HT20_7], 8) |
		  POW_SM(pPwrArray[ALL_TARGET_HT20_6], 0)
	    );

	/* 14 (LSB), 15, 20, 21 */
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE(9),
		  POW_SM(pPwrArray[ALL_TARGET_HT20_21], 24) |
		  POW_SM(pPwrArray[ALL_TARGET_HT20_20], 16) |
		  POW_SM(pPwrArray[ALL_TARGET_HT20_15], 8) |
		  POW_SM(pPwrArray[ALL_TARGET_HT20_14], 0)
	    );

	/* Mixed HT20 and HT40 rates */

	/* HT20 22 (LSB), HT20 23, HT40 22, HT40 23 (MSB) */
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE(10),
		  POW_SM(pPwrArray[ALL_TARGET_HT40_23], 24) |
		  POW_SM(pPwrArray[ALL_TARGET_HT40_22], 16) |
		  POW_SM(pPwrArray[ALL_TARGET_HT20_23], 8) |
		  POW_SM(pPwrArray[ALL_TARGET_HT20_22], 0)
	    );

	/*
	 * Write the HT40 power per rate set
	 * correct PAR difference between HT40 and HT20/LEGACY
	 * 0/8/16 (LSB), 1-3/9-11/17-19, 4, 5 (MSB)
	 */
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE(6),
		  POW_SM(pPwrArray[ALL_TARGET_HT40_5], 24) |
		  POW_SM(pPwrArray[ALL_TARGET_HT40_4], 16) |
		  POW_SM(pPwrArray[ALL_TARGET_HT40_1_3_9_11_17_19], 8) |
		  POW_SM(pPwrArray[ALL_TARGET_HT40_0_8_16], 0)
	    );

	/* 6 (LSB), 7, 12, 13 (MSB) */
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE(7),
		  POW_SM(pPwrArray[ALL_TARGET_HT40_13], 24) |
		  POW_SM(pPwrArray[ALL_TARGET_HT40_12], 16) |
		  POW_SM(pPwrArray[ALL_TARGET_HT40_7], 8) |
		  POW_SM(pPwrArray[ALL_TARGET_HT40_6], 0)
	    );

	/* 14 (LSB), 15, 20, 21 */
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE(11),
		  POW_SM(pPwrArray[ALL_TARGET_HT40_21], 24) |
		  POW_SM(pPwrArray[ALL_TARGET_HT40_20], 16) |
		  POW_SM(pPwrArray[ALL_TARGET_HT40_15], 8) |
		  POW_SM(pPwrArray[ALL_TARGET_HT40_14], 0)
	    );

	return 0;
#undef POW_SM
}

static void ar9003_hw_set_target_power_eeprom(struct ath_hw *ah, u16 freq,
					      u8 *targetPowerValT2)
{
	/* XXX: hard code for now, need to get from eeprom struct */
	u8 ht40PowerIncForPdadc = 0;
	bool is2GHz = false;
	unsigned int i = 0;
	struct ath_common *common = ath9k_hw_common(ah);

	if (freq < 4000)
		is2GHz = true;

	targetPowerValT2[ALL_TARGET_LEGACY_6_24] =
	    ar9003_hw_eeprom_get_tgt_pwr(ah, LEGACY_TARGET_RATE_6_24, freq,
					 is2GHz);
	targetPowerValT2[ALL_TARGET_LEGACY_36] =
	    ar9003_hw_eeprom_get_tgt_pwr(ah, LEGACY_TARGET_RATE_36, freq,
					 is2GHz);
	targetPowerValT2[ALL_TARGET_LEGACY_48] =
	    ar9003_hw_eeprom_get_tgt_pwr(ah, LEGACY_TARGET_RATE_48, freq,
					 is2GHz);
	targetPowerValT2[ALL_TARGET_LEGACY_54] =
	    ar9003_hw_eeprom_get_tgt_pwr(ah, LEGACY_TARGET_RATE_54, freq,
					 is2GHz);
	targetPowerValT2[ALL_TARGET_LEGACY_1L_5L] =
	    ar9003_hw_eeprom_get_cck_tgt_pwr(ah, LEGACY_TARGET_RATE_1L_5L,
					     freq);
	targetPowerValT2[ALL_TARGET_LEGACY_5S] =
	    ar9003_hw_eeprom_get_cck_tgt_pwr(ah, LEGACY_TARGET_RATE_5S, freq);
	targetPowerValT2[ALL_TARGET_LEGACY_11L] =
	    ar9003_hw_eeprom_get_cck_tgt_pwr(ah, LEGACY_TARGET_RATE_11L, freq);
	targetPowerValT2[ALL_TARGET_LEGACY_11S] =
	    ar9003_hw_eeprom_get_cck_tgt_pwr(ah, LEGACY_TARGET_RATE_11S, freq);
	targetPowerValT2[ALL_TARGET_HT20_0_8_16] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_0_8_16, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_1_3_9_11_17_19] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_1_3_9_11_17_19,
					      freq, is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_4] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_4, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_5] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_5, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_6] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_6, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_7] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_7, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_12] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_12, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_13] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_13, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_14] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_14, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_15] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_15, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_20] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_20, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_21] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_21, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_22] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_22, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT20_23] =
	    ar9003_hw_eeprom_get_ht20_tgt_pwr(ah, HT_TARGET_RATE_23, freq,
					      is2GHz);
	targetPowerValT2[ALL_TARGET_HT40_0_8_16] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_0_8_16, freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_1_3_9_11_17_19] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_1_3_9_11_17_19,
					      freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_4] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_4, freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_5] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_5, freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_6] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_6, freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_7] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_7, freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_12] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_12, freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_13] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_13, freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_14] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_14, freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_15] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_15, freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_20] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_20, freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_21] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_21, freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_22] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_22, freq,
					      is2GHz) + ht40PowerIncForPdadc;
	targetPowerValT2[ALL_TARGET_HT40_23] =
	    ar9003_hw_eeprom_get_ht40_tgt_pwr(ah, HT_TARGET_RATE_23, freq,
					      is2GHz) + ht40PowerIncForPdadc;

	for (i = 0; i < ar9300RateSize; i++) {
		ath_dbg(common, ATH_DBG_EEPROM,
			"TPC[%02d] 0x%08x\n", i, targetPowerValT2[i]);
	}
}

static int ar9003_hw_cal_pier_get(struct ath_hw *ah,
				  int mode,
				  int ipier,
				  int ichain,
				  int *pfrequency,
				  int *pcorrection,
				  int *ptemperature, int *pvoltage)
{
	u8 *pCalPier;
	struct ar9300_cal_data_per_freq_op_loop *pCalPierStruct;
	int is2GHz;
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;
	struct ath_common *common = ath9k_hw_common(ah);

	if (ichain >= AR9300_MAX_CHAINS) {
		ath_dbg(common, ATH_DBG_EEPROM,
			"Invalid chain index, must be less than %d\n",
			AR9300_MAX_CHAINS);
		return -1;
	}

	if (mode) {		/* 5GHz */
		if (ipier >= AR9300_NUM_5G_CAL_PIERS) {
			ath_dbg(common, ATH_DBG_EEPROM,
				"Invalid 5GHz cal pier index, must be less than %d\n",
				AR9300_NUM_5G_CAL_PIERS);
			return -1;
		}
		pCalPier = &(eep->calFreqPier5G[ipier]);
		pCalPierStruct = &(eep->calPierData5G[ichain][ipier]);
		is2GHz = 0;
	} else {
		if (ipier >= AR9300_NUM_2G_CAL_PIERS) {
			ath_dbg(common, ATH_DBG_EEPROM,
				"Invalid 2GHz cal pier index, must be less than %d\n",
				AR9300_NUM_2G_CAL_PIERS);
			return -1;
		}

		pCalPier = &(eep->calFreqPier2G[ipier]);
		pCalPierStruct = &(eep->calPierData2G[ichain][ipier]);
		is2GHz = 1;
	}

	*pfrequency = FBIN2FREQ(*pCalPier, is2GHz);
	*pcorrection = pCalPierStruct->refPower;
	*ptemperature = pCalPierStruct->tempMeas;
	*pvoltage = pCalPierStruct->voltMeas;

	return 0;
}

static int ar9003_hw_power_control_override(struct ath_hw *ah,
					    int frequency,
					    int *correction,
					    int *voltage, int *temperature)
{
	int tempSlope = 0;
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;
	int f[3], t[3];

	REG_RMW(ah, AR_PHY_TPC_11_B0,
		(correction[0] << AR_PHY_TPC_OLPC_GAIN_DELTA_S),
		AR_PHY_TPC_OLPC_GAIN_DELTA);
	if (ah->caps.tx_chainmask & BIT(1))
		REG_RMW(ah, AR_PHY_TPC_11_B1,
			(correction[1] << AR_PHY_TPC_OLPC_GAIN_DELTA_S),
			AR_PHY_TPC_OLPC_GAIN_DELTA);
	if (ah->caps.tx_chainmask & BIT(2))
		REG_RMW(ah, AR_PHY_TPC_11_B2,
			(correction[2] << AR_PHY_TPC_OLPC_GAIN_DELTA_S),
			AR_PHY_TPC_OLPC_GAIN_DELTA);

	/* enable open loop power control on chip */
	REG_RMW(ah, AR_PHY_TPC_6_B0,
		(3 << AR_PHY_TPC_6_ERROR_EST_MODE_S),
		AR_PHY_TPC_6_ERROR_EST_MODE);
	if (ah->caps.tx_chainmask & BIT(1))
		REG_RMW(ah, AR_PHY_TPC_6_B1,
			(3 << AR_PHY_TPC_6_ERROR_EST_MODE_S),
			AR_PHY_TPC_6_ERROR_EST_MODE);
	if (ah->caps.tx_chainmask & BIT(2))
		REG_RMW(ah, AR_PHY_TPC_6_B2,
			(3 << AR_PHY_TPC_6_ERROR_EST_MODE_S),
			AR_PHY_TPC_6_ERROR_EST_MODE);

	/*
	 * enable temperature compensation
	 * Need to use register names
	 */
	if (frequency < 4000)
		tempSlope = eep->modalHeader2G.tempSlope;
	else if (eep->base_ext2.tempSlopeLow != 0) {
		t[0] = eep->base_ext2.tempSlopeLow;
		f[0] = 5180;
		t[1] = eep->modalHeader5G.tempSlope;
		f[1] = 5500;
		t[2] = eep->base_ext2.tempSlopeHigh;
		f[2] = 5785;
		tempSlope = ar9003_hw_power_interpolate((s32) frequency,
							f, t, 3);
	} else
		tempSlope = eep->modalHeader5G.tempSlope;

	REG_RMW_FIELD(ah, AR_PHY_TPC_19, AR_PHY_TPC_19_ALPHA_THERM, tempSlope);
	REG_RMW_FIELD(ah, AR_PHY_TPC_18, AR_PHY_TPC_18_THERM_CAL_VALUE,
		      temperature[0]);

	return 0;
}

/* Apply the recorded correction values. */
static int ar9003_hw_calibration_apply(struct ath_hw *ah, int frequency)
{
	int ichain, ipier, npier;
	int mode;
	int lfrequency[AR9300_MAX_CHAINS],
	    lcorrection[AR9300_MAX_CHAINS],
	    ltemperature[AR9300_MAX_CHAINS], lvoltage[AR9300_MAX_CHAINS];
	int hfrequency[AR9300_MAX_CHAINS],
	    hcorrection[AR9300_MAX_CHAINS],
	    htemperature[AR9300_MAX_CHAINS], hvoltage[AR9300_MAX_CHAINS];
	int fdiff;
	int correction[AR9300_MAX_CHAINS],
	    voltage[AR9300_MAX_CHAINS], temperature[AR9300_MAX_CHAINS];
	int pfrequency, pcorrection, ptemperature, pvoltage;
	struct ath_common *common = ath9k_hw_common(ah);

	mode = (frequency >= 4000);
	if (mode)
		npier = AR9300_NUM_5G_CAL_PIERS;
	else
		npier = AR9300_NUM_2G_CAL_PIERS;

	for (ichain = 0; ichain < AR9300_MAX_CHAINS; ichain++) {
		lfrequency[ichain] = 0;
		hfrequency[ichain] = 100000;
	}
	/* identify best lower and higher frequency calibration measurement */
	for (ichain = 0; ichain < AR9300_MAX_CHAINS; ichain++) {
		for (ipier = 0; ipier < npier; ipier++) {
			if (!ar9003_hw_cal_pier_get(ah, mode, ipier, ichain,
						    &pfrequency, &pcorrection,
						    &ptemperature, &pvoltage)) {
				fdiff = frequency - pfrequency;

				/*
				 * this measurement is higher than
				 * our desired frequency
				 */
				if (fdiff <= 0) {
					if (hfrequency[ichain] <= 0 ||
					    hfrequency[ichain] >= 100000 ||
					    fdiff >
					    (frequency - hfrequency[ichain])) {
						/*
						 * new best higher
						 * frequency measurement
						 */
						hfrequency[ichain] = pfrequency;
						hcorrection[ichain] =
						    pcorrection;
						htemperature[ichain] =
						    ptemperature;
						hvoltage[ichain] = pvoltage;
					}
				}
				if (fdiff >= 0) {
					if (lfrequency[ichain] <= 0
					    || fdiff <
					    (frequency - lfrequency[ichain])) {
						/*
						 * new best lower
						 * frequency measurement
						 */
						lfrequency[ichain] = pfrequency;
						lcorrection[ichain] =
						    pcorrection;
						ltemperature[ichain] =
						    ptemperature;
						lvoltage[ichain] = pvoltage;
					}
				}
			}
		}
	}

	/* interpolate  */
	for (ichain = 0; ichain < AR9300_MAX_CHAINS; ichain++) {
		ath_dbg(common, ATH_DBG_EEPROM,
			"ch=%d f=%d low=%d %d h=%d %d\n",
			ichain, frequency, lfrequency[ichain],
			lcorrection[ichain], hfrequency[ichain],
			hcorrection[ichain]);
		/* they're the same, so just pick one */
		if (hfrequency[ichain] == lfrequency[ichain]) {
			correction[ichain] = lcorrection[ichain];
			voltage[ichain] = lvoltage[ichain];
			temperature[ichain] = ltemperature[ichain];
		}
		/* the low frequency is good */
		else if (frequency - lfrequency[ichain] < 1000) {
			/* so is the high frequency, interpolate */
			if (hfrequency[ichain] - frequency < 1000) {

				correction[ichain] = interpolate(frequency,
						lfrequency[ichain],
						hfrequency[ichain],
						lcorrection[ichain],
						hcorrection[ichain]);

				temperature[ichain] = interpolate(frequency,
						lfrequency[ichain],
						hfrequency[ichain],
						ltemperature[ichain],
						htemperature[ichain]);

				voltage[ichain] = interpolate(frequency,
						lfrequency[ichain],
						hfrequency[ichain],
						lvoltage[ichain],
						hvoltage[ichain]);
			}
			/* only low is good, use it */
			else {
				correction[ichain] = lcorrection[ichain];
				temperature[ichain] = ltemperature[ichain];
				voltage[ichain] = lvoltage[ichain];
			}
		}
		/* only high is good, use it */
		else if (hfrequency[ichain] - frequency < 1000) {
			correction[ichain] = hcorrection[ichain];
			temperature[ichain] = htemperature[ichain];
			voltage[ichain] = hvoltage[ichain];
		} else {	/* nothing is good, presume 0???? */
			correction[ichain] = 0;
			temperature[ichain] = 0;
			voltage[ichain] = 0;
		}
	}

	ar9003_hw_power_control_override(ah, frequency, correction, voltage,
					 temperature);

	ath_dbg(common, ATH_DBG_EEPROM,
		"for frequency=%d, calibration correction = %d %d %d\n",
		frequency, correction[0], correction[1], correction[2]);

	return 0;
}

static u16 ar9003_hw_get_direct_edge_power(struct ar9300_eeprom *eep,
					   int idx,
					   int edge,
					   bool is2GHz)
{
	struct cal_ctl_data_2g *ctl_2g = eep->ctlPowerData_2G;
	struct cal_ctl_data_5g *ctl_5g = eep->ctlPowerData_5G;

	if (is2GHz)
		return CTL_EDGE_TPOWER(ctl_2g[idx].ctlEdges[edge]);
	else
		return CTL_EDGE_TPOWER(ctl_5g[idx].ctlEdges[edge]);
}

static u16 ar9003_hw_get_indirect_edge_power(struct ar9300_eeprom *eep,
					     int idx,
					     unsigned int edge,
					     u16 freq,
					     bool is2GHz)
{
	struct cal_ctl_data_2g *ctl_2g = eep->ctlPowerData_2G;
	struct cal_ctl_data_5g *ctl_5g = eep->ctlPowerData_5G;

	u8 *ctl_freqbin = is2GHz ?
		&eep->ctl_freqbin_2G[idx][0] :
		&eep->ctl_freqbin_5G[idx][0];

	if (is2GHz) {
		if (ath9k_hw_fbin2freq(ctl_freqbin[edge - 1], 1) < freq &&
		    CTL_EDGE_FLAGS(ctl_2g[idx].ctlEdges[edge - 1]))
			return CTL_EDGE_TPOWER(ctl_2g[idx].ctlEdges[edge - 1]);
	} else {
		if (ath9k_hw_fbin2freq(ctl_freqbin[edge - 1], 0) < freq &&
		    CTL_EDGE_FLAGS(ctl_5g[idx].ctlEdges[edge - 1]))
			return CTL_EDGE_TPOWER(ctl_5g[idx].ctlEdges[edge - 1]);
	}

	return MAX_RATE_POWER;
}

/*
 * Find the maximum conformance test limit for the given channel and CTL info
 */
static u16 ar9003_hw_get_max_edge_power(struct ar9300_eeprom *eep,
					u16 freq, int idx, bool is2GHz)
{
	u16 twiceMaxEdgePower = MAX_RATE_POWER;
	u8 *ctl_freqbin = is2GHz ?
		&eep->ctl_freqbin_2G[idx][0] :
		&eep->ctl_freqbin_5G[idx][0];
	u16 num_edges = is2GHz ?
		AR9300_NUM_BAND_EDGES_2G : AR9300_NUM_BAND_EDGES_5G;
	unsigned int edge;

	/* Get the edge power */
	for (edge = 0;
	     (edge < num_edges) && (ctl_freqbin[edge] != AR5416_BCHAN_UNUSED);
	     edge++) {
		/*
		 * If there's an exact channel match or an inband flag set
		 * on the lower channel use the given rdEdgePower
		 */
		if (freq == ath9k_hw_fbin2freq(ctl_freqbin[edge], is2GHz)) {
			twiceMaxEdgePower =
				ar9003_hw_get_direct_edge_power(eep, idx,
								edge, is2GHz);
			break;
		} else if ((edge > 0) &&
			   (freq < ath9k_hw_fbin2freq(ctl_freqbin[edge],
						      is2GHz))) {
			twiceMaxEdgePower =
				ar9003_hw_get_indirect_edge_power(eep, idx,
								  edge, freq,
								  is2GHz);
			/*
			 * Leave loop - no more affecting edges possible in
			 * this monotonic increasing list
			 */
			break;
		}
	}
	return twiceMaxEdgePower;
}

static void ar9003_hw_set_power_per_rate_table(struct ath_hw *ah,
					       struct ath9k_channel *chan,
					       u8 *pPwrArray, u16 cfgCtl,
					       u8 twiceAntennaReduction,
					       u8 twiceMaxRegulatoryPower,
					       u16 powerLimit)
{
	struct ath_regulatory *regulatory = ath9k_hw_regulatory(ah);
	struct ath_common *common = ath9k_hw_common(ah);
	struct ar9300_eeprom *pEepData = &ah->eeprom.ar9300_eep;
	u16 twiceMaxEdgePower = MAX_RATE_POWER;
	static const u16 tpScaleReductionTable[5] = {
		0, 3, 6, 9, MAX_RATE_POWER
	};
	int i;
	int16_t  twiceLargestAntenna;
	u16 scaledPower = 0, minCtlPower, maxRegAllowedPower;
	static const u16 ctlModesFor11a[] = {
		CTL_11A, CTL_5GHT20, CTL_11A_EXT, CTL_5GHT40
	};
	static const u16 ctlModesFor11g[] = {
		CTL_11B, CTL_11G, CTL_2GHT20, CTL_11B_EXT,
		CTL_11G_EXT, CTL_2GHT40
	};
	u16 numCtlModes;
	const u16 *pCtlMode;
	u16 ctlMode, freq;
	struct chan_centers centers;
	u8 *ctlIndex;
	u8 ctlNum;
	u16 twiceMinEdgePower;
	bool is2ghz = IS_CHAN_2GHZ(chan);

	ath9k_hw_get_channel_centers(ah, chan, &centers);

	/* Compute TxPower reduction due to Antenna Gain */
	if (is2ghz)
		twiceLargestAntenna = pEepData->modalHeader2G.antennaGain;
	else
		twiceLargestAntenna = pEepData->modalHeader5G.antennaGain;

	twiceLargestAntenna = (int16_t)min((twiceAntennaReduction) -
				twiceLargestAntenna, 0);

	/*
	 * scaledPower is the minimum of the user input power level
	 * and the regulatory allowed power level
	 */
	maxRegAllowedPower = twiceMaxRegulatoryPower + twiceLargestAntenna;

	if (regulatory->tp_scale != ATH9K_TP_SCALE_MAX) {
		maxRegAllowedPower -=
			(tpScaleReductionTable[(regulatory->tp_scale)] * 2);
	}

	scaledPower = min(powerLimit, maxRegAllowedPower);

	/*
	 * Reduce scaled Power by number of chains active to get
	 * to per chain tx power level
	 */
	switch (ar5416_get_ntxchains(ah->txchainmask)) {
	case 1:
		break;
	case 2:
		if (scaledPower > REDUCE_SCALED_POWER_BY_TWO_CHAIN)
			scaledPower -= REDUCE_SCALED_POWER_BY_TWO_CHAIN;
		else
			scaledPower = 0;
		break;
	case 3:
		if (scaledPower > REDUCE_SCALED_POWER_BY_THREE_CHAIN)
			scaledPower -= REDUCE_SCALED_POWER_BY_THREE_CHAIN;
		else
			scaledPower = 0;
		break;
	}

	scaledPower = max((u16)0, scaledPower);

	/*
	 * Get target powers from EEPROM - our baseline for TX Power
	 */
	if (is2ghz) {
		/* Setup for CTL modes */
		/* CTL_11B, CTL_11G, CTL_2GHT20 */
		numCtlModes =
			ARRAY_SIZE(ctlModesFor11g) -
				   SUB_NUM_CTL_MODES_AT_2G_40;
		pCtlMode = ctlModesFor11g;
		if (IS_CHAN_HT40(chan))
			/* All 2G CTL's */
			numCtlModes = ARRAY_SIZE(ctlModesFor11g);
	} else {
		/* Setup for CTL modes */
		/* CTL_11A, CTL_5GHT20 */
		numCtlModes = ARRAY_SIZE(ctlModesFor11a) -
					 SUB_NUM_CTL_MODES_AT_5G_40;
		pCtlMode = ctlModesFor11a;
		if (IS_CHAN_HT40(chan))
			/* All 5G CTL's */
			numCtlModes = ARRAY_SIZE(ctlModesFor11a);
	}

	/*
	 * For MIMO, need to apply regulatory caps individually across
	 * dynamically running modes: CCK, OFDM, HT20, HT40
	 *
	 * The outer loop walks through each possible applicable runtime mode.
	 * The inner loop walks through each ctlIndex entry in EEPROM.
	 * The ctl value is encoded as [7:4] == test group, [3:0] == test mode.
	 */
	for (ctlMode = 0; ctlMode < numCtlModes; ctlMode++) {
		bool isHt40CtlMode = (pCtlMode[ctlMode] == CTL_5GHT40) ||
			(pCtlMode[ctlMode] == CTL_2GHT40);
		if (isHt40CtlMode)
			freq = centers.synth_center;
		else if (pCtlMode[ctlMode] & EXT_ADDITIVE)
			freq = centers.ext_center;
		else
			freq = centers.ctl_center;

		ath_dbg(common, ATH_DBG_REGULATORY,
			"LOOP-Mode ctlMode %d < %d, isHt40CtlMode %d, EXT_ADDITIVE %d\n",
			ctlMode, numCtlModes, isHt40CtlMode,
			(pCtlMode[ctlMode] & EXT_ADDITIVE));

		/* walk through each CTL index stored in EEPROM */
		if (is2ghz) {
			ctlIndex = pEepData->ctlIndex_2G;
			ctlNum = AR9300_NUM_CTLS_2G;
		} else {
			ctlIndex = pEepData->ctlIndex_5G;
			ctlNum = AR9300_NUM_CTLS_5G;
		}

		for (i = 0; (i < ctlNum) && ctlIndex[i]; i++) {
			ath_dbg(common, ATH_DBG_REGULATORY,
				"LOOP-Ctlidx %d: cfgCtl 0x%2.2x pCtlMode 0x%2.2x ctlIndex 0x%2.2x chan %d\n",
				i, cfgCtl, pCtlMode[ctlMode], ctlIndex[i],
				chan->channel);

				/*
				 * compare test group from regulatory
				 * channel list with test mode from pCtlMode
				 * list
				 */
				if ((((cfgCtl & ~CTL_MODE_M) |
				       (pCtlMode[ctlMode] & CTL_MODE_M)) ==
					ctlIndex[i]) ||
				    (((cfgCtl & ~CTL_MODE_M) |
				       (pCtlMode[ctlMode] & CTL_MODE_M)) ==
				     ((ctlIndex[i] & CTL_MODE_M) |
				       SD_NO_CTL))) {
					twiceMinEdgePower =
					  ar9003_hw_get_max_edge_power(pEepData,
								       freq, i,
								       is2ghz);

					if ((cfgCtl & ~CTL_MODE_M) == SD_NO_CTL)
						/*
						 * Find the minimum of all CTL
						 * edge powers that apply to
						 * this channel
						 */
						twiceMaxEdgePower =
							min(twiceMaxEdgePower,
							    twiceMinEdgePower);
						else {
							/* specific */
							twiceMaxEdgePower =
							  twiceMinEdgePower;
							break;
						}
				}
			}

			minCtlPower = (u8)min(twiceMaxEdgePower, scaledPower);

			ath_dbg(common, ATH_DBG_REGULATORY,
				"SEL-Min ctlMode %d pCtlMode %d 2xMaxEdge %d sP %d minCtlPwr %d\n",
				ctlMode, pCtlMode[ctlMode], twiceMaxEdgePower,
				scaledPower, minCtlPower);

			/* Apply ctl mode to correct target power set */
			switch (pCtlMode[ctlMode]) {
			case CTL_11B:
				for (i = ALL_TARGET_LEGACY_1L_5L;
				     i <= ALL_TARGET_LEGACY_11S; i++)
					pPwrArray[i] =
					  (u8)min((u16)pPwrArray[i],
						  minCtlPower);
				break;
			case CTL_11A:
			case CTL_11G:
				for (i = ALL_TARGET_LEGACY_6_24;
				     i <= ALL_TARGET_LEGACY_54; i++)
					pPwrArray[i] =
					  (u8)min((u16)pPwrArray[i],
						  minCtlPower);
				break;
			case CTL_5GHT20:
			case CTL_2GHT20:
				for (i = ALL_TARGET_HT20_0_8_16;
				     i <= ALL_TARGET_HT20_21; i++)
					pPwrArray[i] =
					  (u8)min((u16)pPwrArray[i],
						  minCtlPower);
				pPwrArray[ALL_TARGET_HT20_22] =
				  (u8)min((u16)pPwrArray[ALL_TARGET_HT20_22],
					  minCtlPower);
				pPwrArray[ALL_TARGET_HT20_23] =
				  (u8)min((u16)pPwrArray[ALL_TARGET_HT20_23],
					   minCtlPower);
				break;
			case CTL_5GHT40:
			case CTL_2GHT40:
				for (i = ALL_TARGET_HT40_0_8_16;
				     i <= ALL_TARGET_HT40_23; i++)
					pPwrArray[i] =
					  (u8)min((u16)pPwrArray[i],
						  minCtlPower);
				break;
			default:
			    break;
			}
	} /* end ctl mode checking */
}

static inline u8 mcsidx_to_tgtpwridx(unsigned int mcs_idx, u8 base_pwridx)
{
	u8 mod_idx = mcs_idx % 8;

	if (mod_idx <= 3)
		return mod_idx ? (base_pwridx + 1) : base_pwridx;
	else
		return base_pwridx + 4 * (mcs_idx / 8) + mod_idx - 2;
}

static void ath9k_hw_ar9300_set_txpower(struct ath_hw *ah,
					struct ath9k_channel *chan, u16 cfgCtl,
					u8 twiceAntennaReduction,
					u8 twiceMaxRegulatoryPower,
					u8 powerLimit, bool test)
{
	struct ath_regulatory *regulatory = ath9k_hw_regulatory(ah);
	struct ath_common *common = ath9k_hw_common(ah);
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;
	struct ar9300_modal_eep_header *modal_hdr;
	u8 targetPowerValT2[ar9300RateSize];
	u8 target_power_val_t2_eep[ar9300RateSize];
	unsigned int i = 0, paprd_scale_factor = 0;
	u8 pwr_idx, min_pwridx = 0;

	ar9003_hw_set_target_power_eeprom(ah, chan->channel, targetPowerValT2);

	if (ah->eep_ops->get_eeprom(ah, EEP_PAPRD)) {
		if (IS_CHAN_2GHZ(chan))
			modal_hdr = &eep->modalHeader2G;
		else
			modal_hdr = &eep->modalHeader5G;

		ah->paprd_ratemask =
			le32_to_cpu(modal_hdr->papdRateMaskHt20) &
			AR9300_PAPRD_RATE_MASK;

		ah->paprd_ratemask_ht40 =
			le32_to_cpu(modal_hdr->papdRateMaskHt40) &
			AR9300_PAPRD_RATE_MASK;

		paprd_scale_factor = ar9003_get_paprd_scale_factor(ah, chan);
		min_pwridx = IS_CHAN_HT40(chan) ? ALL_TARGET_HT40_0_8_16 :
						  ALL_TARGET_HT20_0_8_16;

		if (!ah->paprd_table_write_done) {
			memcpy(target_power_val_t2_eep, targetPowerValT2,
			       sizeof(targetPowerValT2));
			for (i = 0; i < 24; i++) {
				pwr_idx = mcsidx_to_tgtpwridx(i, min_pwridx);
				if (ah->paprd_ratemask & (1 << i)) {
					if (targetPowerValT2[pwr_idx] &&
					    targetPowerValT2[pwr_idx] ==
					    target_power_val_t2_eep[pwr_idx])
						targetPowerValT2[pwr_idx] -=
							paprd_scale_factor;
				}
			}
		}
		memcpy(target_power_val_t2_eep, targetPowerValT2,
		       sizeof(targetPowerValT2));
	}

	ar9003_hw_set_power_per_rate_table(ah, chan,
					   targetPowerValT2, cfgCtl,
					   twiceAntennaReduction,
					   twiceMaxRegulatoryPower,
					   powerLimit);

	if (ah->eep_ops->get_eeprom(ah, EEP_PAPRD)) {
		for (i = 0; i < ar9300RateSize; i++) {
			if ((ah->paprd_ratemask & (1 << i)) &&
			    (abs(targetPowerValT2[i] -
				target_power_val_t2_eep[i]) >
			    paprd_scale_factor)) {
				ah->paprd_ratemask &= ~(1 << i);
				ath_dbg(common, ATH_DBG_EEPROM,
					"paprd disabled for mcs %d\n", i);
			}
		}
	}

	regulatory->max_power_level = 0;
	for (i = 0; i < ar9300RateSize; i++) {
		if (targetPowerValT2[i] > regulatory->max_power_level)
			regulatory->max_power_level = targetPowerValT2[i];
	}

	if (test)
		return;

	for (i = 0; i < ar9300RateSize; i++) {
		ath_dbg(common, ATH_DBG_EEPROM,
			"TPC[%02d] 0x%08x\n", i, targetPowerValT2[i]);
	}

	/*
	 * This is the TX power we send back to driver core,
	 * and it can use to pass to userspace to display our
	 * currently configured TX power setting.
	 *
	 * Since power is rate dependent, use one of the indices
	 * from the AR9300_Rates enum to select an entry from
	 * targetPowerValT2[] to report. Currently returns the
	 * power for HT40 MCS 0, HT20 MCS 0, or OFDM 6 Mbps
	 * as CCK power is less interesting (?).
	 */
	i = ALL_TARGET_LEGACY_6_24; /* legacy */
	if (IS_CHAN_HT40(chan))
		i = ALL_TARGET_HT40_0_8_16; /* ht40 */
	else if (IS_CHAN_HT20(chan))
		i = ALL_TARGET_HT20_0_8_16; /* ht20 */

	ah->txpower_limit = targetPowerValT2[i];
	regulatory->max_power_level = targetPowerValT2[i];

	/* Write target power array to registers */
	ar9003_hw_tx_power_regwrite(ah, targetPowerValT2);
	ar9003_hw_calibration_apply(ah, chan->channel);

	if (IS_CHAN_2GHZ(chan)) {
		if (IS_CHAN_HT40(chan))
			i = ALL_TARGET_HT40_0_8_16;
		else
			i = ALL_TARGET_HT20_0_8_16;
	} else {
		if (IS_CHAN_HT40(chan))
			i = ALL_TARGET_HT40_7;
		else
			i = ALL_TARGET_HT20_7;
	}
	ah->paprd_target_power = targetPowerValT2[i];
}

static u16 ath9k_hw_ar9300_get_spur_channel(struct ath_hw *ah,
					    u16 i, bool is2GHz)
{
	return AR_NO_SPUR;
}

s32 ar9003_hw_get_tx_gain_idx(struct ath_hw *ah)
{
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;

	return (eep->baseEepHeader.txrxgain >> 4) & 0xf; /* bits 7:4 */
}

s32 ar9003_hw_get_rx_gain_idx(struct ath_hw *ah)
{
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;

	return (eep->baseEepHeader.txrxgain) & 0xf; /* bits 3:0 */
}

u8 *ar9003_get_spur_chan_ptr(struct ath_hw *ah, bool is_2ghz)
{
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;

	if (is_2ghz)
		return eep->modalHeader2G.spurChans;
	else
		return eep->modalHeader5G.spurChans;
}

unsigned int ar9003_get_paprd_scale_factor(struct ath_hw *ah,
					   struct ath9k_channel *chan)
{
	struct ar9300_eeprom *eep = &ah->eeprom.ar9300_eep;

	if (IS_CHAN_2GHZ(chan))
		return MS(le32_to_cpu(eep->modalHeader2G.papdRateMaskHt20),
			  AR9300_PAPRD_SCALE_1);
	else {
		if (chan->channel >= 5700)
		return MS(le32_to_cpu(eep->modalHeader5G.papdRateMaskHt20),
			  AR9300_PAPRD_SCALE_1);
		else if (chan->channel >= 5400)
			return MS(le32_to_cpu(eep->modalHeader5G.papdRateMaskHt40),
				   AR9300_PAPRD_SCALE_2);
		else
			return MS(le32_to_cpu(eep->modalHeader5G.papdRateMaskHt40),
				  AR9300_PAPRD_SCALE_1);
	}
}

const struct eeprom_ops eep_ar9300_ops = {
	.check_eeprom = ath9k_hw_ar9300_check_eeprom,
	.get_eeprom = ath9k_hw_ar9300_get_eeprom,
	.fill_eeprom = ath9k_hw_ar9300_fill_eeprom,
	.get_eeprom_ver = ath9k_hw_ar9300_get_eeprom_ver,
	.get_eeprom_rev = ath9k_hw_ar9300_get_eeprom_rev,
	.set_board_values = ath9k_hw_ar9300_set_board_values,
	.set_addac = ath9k_hw_ar9300_set_addac,
	.set_txpower = ath9k_hw_ar9300_set_txpower,
	.get_spur_channel = ath9k_hw_ar9300_get_spur_channel
};
