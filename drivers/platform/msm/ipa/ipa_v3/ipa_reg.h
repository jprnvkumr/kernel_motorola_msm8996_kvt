/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __IPA_REG_H__
#define __IPA_REG_H__

#define IPA_IRQ_STTS_EE_n_ADDR(n) (0x00003008 + 0x1000 * (n))

#define IPA_IRQ_EN_EE_n_ADDR(n) (0x0000300c + 0x1000 * (n))

#define IPA_IRQ_CLR_EE_n_ADDR(n) (0x00003010 + 0x1000 * (n))

#define IPA_IRQ_SUSPEND_INFO_EE_n_ADDR(n) (0x00003098 + 0x1000 * (n))

#define IPA_BCR_OFST 0x000001D0
#define IPA_COUNTER_CFG_OFST 0x000001f0
#define IPA_COUNTER_CFG_EOT_COAL_GRAN_BMSK 0xF
#define IPA_COUNTER_CFG_EOT_COAL_GRAN_SHFT 0x0
#define IPA_COUNTER_CFG_AGGR_GRAN_BMSK 0x1F0
#define IPA_COUNTER_CFG_AGGR_GRAN_SHFT 0x4

#define IPA_ENABLED_PIPES_OFST 0x00000038

#define IPA_REG_BASE_OFST_v3_0 0x00040000
#define IPA_COMP_SW_RESET_OFST 0x00000040

#define IPA_VERSION_OFST 0x00000034
#define IPA_COMP_HW_VERSION_OFST 0x00000030

#define IPA_SPARE_REG_1_OFST	(0x00005090)
#define IPA_SPARE_REG_2_OFST	(0x00005094)

#define IPA_SHARED_MEM_SIZE_OFST_v3_0 0x00000054
#define IPA_SHARED_MEM_SIZE_SHARED_MEM_BADDR_BMSK_v3_0 0xffff0000
#define IPA_SHARED_MEM_SIZE_SHARED_MEM_BADDR_SHFT_v3_0 0x10
#define IPA_SHARED_MEM_SIZE_SHARED_MEM_SIZE_BMSK_v3_0  0xffff
#define IPA_SHARED_MEM_SIZE_SHARED_MEM_SIZE_SHFT_v3_0  0x0

#define IPA_ENDP_INIT_ROUTE_N_OFST_v3_0(n) (0x00000828 + 0x70 * (n))
#define IPA_ENDP_INIT_ROUTE_N_ROUTE_TABLE_INDEX_BMSK 0x1f
#define IPA_ENDP_INIT_ROUTE_N_ROUTE_TABLE_INDEX_SHFT 0x0

#define IPA_ROUTE_OFST_v3_0 0x00000048

#define IPA_ROUTE_ROUTE_DIS_SHFT 0x0
#define IPA_ROUTE_ROUTE_DIS_BMSK 0x1
#define IPA_ROUTE_ROUTE_DEF_PIPE_SHFT 0x1
#define IPA_ROUTE_ROUTE_DEF_PIPE_BMSK 0x3e
#define IPA_ROUTE_ROUTE_DEF_HDR_TABLE_SHFT 0x6
#define IPA_ROUTE_ROUTE_DEF_HDR_TABLE_BMSK 0X40
#define IPA_ROUTE_ROUTE_DEF_HDR_OFST_SHFT 0x7
#define IPA_ROUTE_ROUTE_DEF_HDR_OFST_BMSK 0x1ff80
#define IPA_ROUTE_ROUTE_FRAG_DEF_PIPE_BMSK 0x3e0000
#define IPA_ROUTE_ROUTE_FRAG_DEF_PIPE_SHFT 0x11
#define IPA_ROUTE_ROUTE_DEF_RETAIN_HDR_BMSK  0x1000000
#define IPA_ROUTE_ROUTE_DEF_RETAIN_HDR_SHFT 0x18

#define IPA_SRAM_DIRECT_ACCESS_N_OFST_v3_0(n) (0x00007000 + 0x4 * (n))

#define IPA_COMP_CFG_OFST 0x0000003C

#define IPA_AGGR_FORCE_CLOSE_OFST 0x000001EC
#define IPA_AGGR_FORCE_CLOSE_OFST_AGGR_FORCE_CLOSE_PIPE_BITMAP_BMSK 0xFFFFF
#define IPA_AGGR_FORCE_CLOSE_OFST_AGGR_FORCE_CLOSE_PIPE_BITMAP_SHFT 0

#define IPA_ENDP_INIT_AGGR_N_OFST_v3_0(n) (0x00000824 + 0x70 * (n))
#define IPA_ENDP_INIT_AGGR_N_AGGR_HARD_BYTE_LIMIT_ENABLE_BMSK	0x1000000
#define IPA_ENDP_INIT_AGGR_N_AGGR_HARD_BYTE_LIMIT_ENABLE_SHFT	0x18
#define IPA_ENDP_INIT_AGGR_N_AGGR_FORCE_CLOSE_BMSK 0x400000
#define IPA_ENDP_INIT_AGGR_N_AGGR_FORCE_CLOSE_SHFT 0x16
#define IPA_ENDP_INIT_AGGR_N_AGGR_SW_EOF_ACTIVE_BMSK	0x200000
#define IPA_ENDP_INIT_AGGR_n_AGGR_SW_EOF_ACTIVE_SHFT	0x15
#define IPA_ENDP_INIT_AGGR_N_AGGR_PKT_LIMIT_BMSK 0x1f8000
#define IPA_ENDP_INIT_AGGR_N_AGGR_PKT_LIMIT_SHFT 0xf
#define IPA_ENDP_INIT_AGGR_N_AGGR_TIME_LIMIT_BMSK 0x7c00
#define IPA_ENDP_INIT_AGGR_N_AGGR_TIME_LIMIT_SHFT 0xa
#define IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_BMSK 0x3e0
#define IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_SHFT 0x5
#define IPA_ENDP_INIT_AGGR_N_AGGR_TYPE_BMSK 0x1c
#define IPA_ENDP_INIT_AGGR_N_AGGR_TYPE_SHFT 0x2
#define IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK 0x3
#define IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT 0x0

#define IPA_ENDP_INIT_MODE_N_OFST_v3_0(n) (0x00000820 + 0x70 * (n))
#define IPA_ENDP_INIT_MODE_N_DEST_PIPE_INDEX_BMSK_v3_0 0x1f0
#define IPA_ENDP_INIT_MODE_N_DEST_PIPE_INDEX_SHFT_v3_0 0x4
#define IPA_ENDP_INIT_MODE_N_MODE_BMSK 0x7
#define IPA_ENDP_INIT_MODE_N_MODE_SHFT 0x0

#define IPA_ENDP_INIT_HDR_N_OFST_v3_0(n) (0x00000810 + 0x70 * (n))
#define IPA_ENDP_INIT_HDR_N_HDR_LEN_BMSK 0x3f
#define IPA_ENDP_INIT_HDR_N_HDR_LEN_SHFT 0x0
#define IPA_ENDP_INIT_HDR_N_HDR_ADDITIONAL_CONST_LEN_BMSK 0x7e000
#define IPA_ENDP_INIT_HDR_N_HDR_ADDITIONAL_CONST_LEN_SHFT 0xd
#define IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_BMSK 0x3f00000
#define IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_SHFT 0x14
#define IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_VALID_BMSK 0x80000
#define IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_VALID_SHFT 0x13
#define IPA_ENDP_INIT_HDR_N_HDR_METADATA_REG_VALID_BMSK_v2 0x10000000
#define IPA_ENDP_INIT_HDR_N_HDR_METADATA_REG_VALID_SHFT_v2 0x1c
#define IPA_ENDP_INIT_HDR_N_HDR_LEN_INC_DEAGG_HDR_BMSK_v2 0x8000000
#define IPA_ENDP_INIT_HDR_N_HDR_LEN_INC_DEAGG_HDR_SHFT_v2 0x1b
#define IPA_ENDP_INIT_HDR_N_HDR_A5_MUX_BMSK 0x4000000
#define IPA_ENDP_INIT_HDR_N_HDR_A5_MUX_SHFT 0x1a
#define IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_VALID_BMSK 0x40
#define IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_VALID_SHFT 0x6
#define IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_SHFT 0x7
#define IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_BMSK 0x1f80

#define IPA_ENDP_INIT_NAT_N_OFST_v3_0(n) (0x0000080C + 0x70 * (n))
#define IPA_ENDP_INIT_NAT_N_NAT_EN_BMSK 0x3
#define IPA_ENDP_INIT_NAT_N_NAT_EN_SHFT 0x0

#define IPA_ENDP_INIT_HDR_EXT_n_OFST_v3_0(n) (0x00000814 + 0x70 * (n))
#define IPA_ENDP_INIT_HDR_EXT_n_HDR_ENDIANNESS_BMSK 0x1
#define IPA_ENDP_INIT_HDR_EXT_n_HDR_ENDIANNESS_SHFT 0x0
#define IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_VALID_BMSK 0x2
#define IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_VALID_SHFT 0x1
#define IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_BMSK 0x4
#define IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_SHFT 0x2
#define IPA_ENDP_INIT_HDR_EXT_n_HDR_PAYLOAD_LEN_INC_PADDING_BMSK 0x8
#define IPA_ENDP_INIT_HDR_EXT_n_HDR_PAYLOAD_LEN_INC_PADDING_SHFT 0x3
#define IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_OFFSET_BMSK 0x3f0
#define IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_OFFSET_SHFT 0x4
#define IPA_ENDP_INIT_HDR_EXT_n_HDR_PAD_TO_ALIGNMENT_SHFT 0xa
#define IPA_ENDP_INIT_HDR_EXT_n_HDR_PAD_TO_ALIGNMENT_BMSK_v3_0 0x3c00

#define IPA_SINGLE_NDP_MODE_OFST 0x00000068
#define IPA_QCNCM_OFST 0x00000064

#define IPA_ENDP_INIT_CTRL_N_OFST(n) (0x00000800 + 0x70 * (n))
#define IPA_ENDP_INIT_CTRL_N_ENDP_SUSPEND_BMSK 0x1
#define IPA_ENDP_INIT_CTRL_N_ENDP_SUSPEND_SHFT 0x0
#define IPA_ENDP_INIT_CTRL_N_ENDP_DELAY_BMSK 0x2
#define IPA_ENDP_INIT_CTRL_N_ENDP_DELAY_SHFT 0x1

#define IPA_ENDP_INIT_HOL_BLOCK_EN_N_OFST_v3_0(n) (0x0000082c + 0x70 * (n))
#define IPA_ENDP_INIT_HOL_BLOCK_EN_N_RMSK 0x1
#define IPA_ENDP_INIT_HOL_BLOCK_EN_N_MAX 19
#define IPA_ENDP_INIT_HOL_BLOCK_EN_N_EN_BMSK 0x1
#define IPA_ENDP_INIT_HOL_BLOCK_EN_N_EN_SHFT 0x0

#define IPA_ENDP_INIT_DEAGGR_n_OFST_v3_0(n) (0x00000834 + 0x70 * (n))
#define IPA_ENDP_INIT_DEAGGR_n_DEAGGR_HDR_LEN_BMSK 0x3F
#define IPA_ENDP_INIT_DEAGGR_n_DEAGGR_HDR_LEN_SHFT 0x0
#define IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_VALID_BMSK  0x80
#define IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_VALID_SHFT 0x7
#define IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_LOCATION_BMSK 0x3F00
#define IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_LOCATION_SHFT 0x8
#define IPA_ENDP_INIT_DEAGGR_n_MAX_PACKET_LEN_BMSK 0xFFFF0000
#define IPA_ENDP_INIT_DEAGGR_n_MAX_PACKET_LEN_SHFT 0x10

#define IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_OFST_v3_0(n) (0x00000830 + 0x70 * (n))
#define IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_RMSK 0x1ff
#define IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_MAX 19
#define IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_TIMER_BMSK 0x1ff
#define IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_TIMER_SHFT 0x0
#define IPA_DEBUG_CNT_REG_N_OFST_v3_0(n) (0x00000600 + 0x4 * (n))
#define IPA_DEBUG_CNT_REG_N_RMSK 0xffffffff
#define IPA_DEBUG_CNT_REG_N_MAX 15
#define IPA_DEBUG_CNT_REG_N_DBG_CNT_REG_BMSK 0xffffffff
#define IPA_DEBUG_CNT_REG_N_DBG_CNT_REG_SHFT 0x0

#define IPA_DEBUG_CNT_CTRL_N_OFST_v3_0(n) (0x00000640 + 0x4 * (n))
#define IPA_DEBUG_CNT_CTRL_N_RMSK 0x1ff1f171
#define IPA_DEBUG_CNT_CTRL_N_MAX 15
#define IPA_DEBUG_CNT_CTRL_N_DBG_CNT_RULE_INDEX_BMSK 0x1ff00000
#define IPA_DEBUG_CNT_CTRL_N_DBG_CNT_RULE_INDEX_SHFT 0x14
#define IPA_DEBUG_CNT_CTRL_N_DBG_CNT_SOURCE_PIPE_BMSK 0x1f000
#define IPA_DEBUG_CNT_CTRL_N_DBG_CNT_SOURCE_PIPE_SHFT 0xc
#define IPA_DEBUG_CNT_CTRL_N_DBG_CNT_PRODUCT_BMSK 0x100
#define IPA_DEBUG_CNT_CTRL_N_DBG_CNT_PRODUCT_SHFT 0x8
#define IPA_DEBUG_CNT_CTRL_N_DBG_CNT_TYPE_BMSK 0x70
#define IPA_DEBUG_CNT_CTRL_N_DBG_CNT_TYPE_SHFT 0x4
#define IPA_DEBUG_CNT_CTRL_N_DBG_CNT_EN_BMSK 0x1
#define IPA_DEBUG_CNT_CTRL_N_DBG_CNT_EN_SHFT 0x0

#define IPA_ENDP_STATUS_n_OFST(n) (0x00000840 + 0x70 * (n))
#define IPA_ENDP_STATUS_n_STATUS_LOCATION_BMSK 0x100
#define IPA_ENDP_STATUS_n_STATUS_LOCATION_SHFT 0x8
#define IPA_ENDP_STATUS_n_STATUS_ENDP_BMSK 0x3e
#define IPA_ENDP_STATUS_n_STATUS_ENDP_SHFT 0x1
#define IPA_ENDP_STATUS_n_STATUS_EN_BMSK 0x1
#define IPA_ENDP_STATUS_n_STATUS_EN_SHFT 0x0

#define IPA_ENDP_INIT_CFG_n_OFST(n) (0x00000808 + 0x70 * (n))
#define IPA_ENDP_INIT_CFG_n_RMSK 0x7f
#define IPA_ENDP_INIT_CFG_n_MAXn 19
#define IPA_ENDP_INIT_CFG_n_CS_METADATA_HDR_OFFSET_BMSK 0x78
#define IPA_ENDP_INIT_CFG_n_CS_METADATA_HDR_OFFSET_SHFT 0x3
#define IPA_ENDP_INIT_CFG_n_CS_OFFLOAD_EN_BMSK 0x6
#define IPA_ENDP_INIT_CFG_n_CS_OFFLOAD_EN_SHFT 0x1
#define IPA_ENDP_INIT_CFG_n_FRAG_OFFLOAD_EN_BMSK 0x1
#define IPA_ENDP_INIT_CFG_n_FRAG_OFFLOAD_EN_SHFT 0x0

#define IPA_ENDP_INIT_HDR_METADATA_MASK_n_OFST(n) (0x00000818 + 0x70 * (n))
#define IPA_ENDP_INIT_HDR_METADATA_MASK_n_RMSK 0xffffffff
#define IPA_ENDP_INIT_HDR_METADATA_MASK_n_MAXn 19
#define IPA_ENDP_INIT_HDR_METADATA_MASK_n_METADATA_MASK_BMSK 0xffffffff
#define IPA_ENDP_INIT_HDR_METADATA_MASK_n_METADATA_MASK_SHFT 0x0

#define IPA_ENDP_INIT_HDR_METADATA_n_OFST(n) (0x0000081c + 0x70 * (n))
#define IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_BMASK 0xFF0000
#define IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_SHFT 0x10

#define IPA_ENDP_INIT_RSRC_GRP_n(n) (0x00000838 + 0x70 * (n))
#define IPA_ENDP_INIT_RSRC_GRP_n_RSRC_GRP_BMSK 0x7
#define IPA_ENDP_INIT_RSRC_GRP_n_RSRC_GRP_SHFT 0
#define IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n(n) (0x00000400 + 0x20 * (n))
#define IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n(n) (0x00000404 + 0x20 * (n))
#define IPA_SRC_RSRC_GRP_45_RSRC_TYPE_n(n) (0x00000408 + 0x20 * (n))
#define IPA_SRC_RSRC_GRP_67_RSRC_TYPE_n(n) (0x0000040C + 0x20 * (n))
#define IPA_SRC_RSRC_GRP_XY_RSRC_TYPE_n_SRC_RSRC_GRP_Y_MAX_LIMIT_BMSK 0xFF000000
#define IPA_SRC_RSRC_GRP_XY_RSRC_TYPE_n_SRC_RSRC_GRP_Y_MAX_LIMIT_SHFT 24
#define IPA_SRC_RSRC_GRP_XY_RSRC_TYPE_n_SRC_RSRC_GRP_Y_MIN_LIMIT_BMSK 0xFF0000
#define IPA_SRC_RSRC_GRP_XY_RSRC_TYPE_n_SRC_RSRC_GRP_Y_MIN_LIMIT_SHFT 16
#define IPA_SRC_RSRC_GRP_XY_RSRC_TYPE_n_SRC_RSRC_GRP_X_MAX_LIMIT_BMSK 0xFF00
#define IPA_SRC_RSRC_GRP_XY_RSRC_TYPE_n_SRC_RSRC_GRP_X_MAX_LIMIT_SHFT 8
#define IPA_SRC_RSRC_GRP_XY_RSRC_TYPE_n_SRC_RSRC_GRP_X_MIN_LIMIT_BMSK 0xFF
#define IPA_SRC_RSRC_GRP_XY_RSRC_TYPE_n_SRC_RSRC_GRP_X_MIN_LIMIT_SHFT 0

#define IPA_IRQ_EE_UC_n_OFFS(n) (0x0000301c + 0x1000 * (n))

#define IPA_UC_MAILBOX_m_n_OFFS_v3_0(m, n) (0x00022000 + 0x80 * (m) + 0x4 * (n))

#define IPA_SYS_PKT_PROC_CNTXT_BASE_OFST (0x000001e0)
#define IPA_LOCAL_PKT_PROC_CNTXT_BASE_OFST (0x000001e8)

#define IPA_FILT_ROUT_HASH_FLUSH_OFST (0x00000090)
#define IPA_FILT_ROUT_HASH_FLUSH_IPv4_FILT_SHFT (12)
#define IPA_FILT_ROUT_HASH_FLUSH_IPv4_ROUT_SHFT (8)
#define IPA_FILT_ROUT_HASH_FLUSH_IPv6_FILT_SHFT (4)
#define IPA_FILT_ROUT_HASH_FLUSH_IPv6_ROUT_SHFT (0)

#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_OFST(n) (0x0000085C + 0x70 * (n))
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_ID_SHFT 0
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_ID_BMSK 0x1
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_IP_SHFT 1
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_IP_BMSK 0x2
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_DST_IP_SHFT 2
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_DST_IP_BMSK 0x4
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_PORT_SHFT 3
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_PORT_BMSK 0x8
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_DST_PORT_SHFT 4
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_DST_PORT_BMSK 0x10
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_PROTOCOL_SHFT 5
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_PROTOCOL_BMSK 0x20
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_METADATA_SHFT 6
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_METADATA_BMSK 0x40
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_ID_SHFT 16
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_ID_BMSK 0x10000
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_IP_SHFT 17
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_IP_BMSK 0x20000
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_DST_IP_SHFT 18
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_DST_IP_BMSK 0x40000
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_PORT_SHFT 19
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_PORT_BMSK 0x80000
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_DST_PORT_SHFT 20
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_DST_PORT_BMSK 0x100000
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_PROTOCOL_SHFT 21
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_PROTOCOL_BMSK 0x200000
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_METADATA_SHFT 22
#define IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_METADATA_BMSK 0x400000

#define IPA_ENDP_INIT_SEQ_n_OFST(n) (0x0000083C + 0x70*(n))
#define IPA_ENDP_INIT_SEQ_n_DPS_REP_SEQ_TYPE_BMSK 0xf000
#define IPA_ENDP_INIT_SEQ_n_DPS_REP_SEQ_TYPE_SHFT 0xc
#define IPA_ENDP_INIT_SEQ_n_HPS_REP_SEQ_TYPE_BMSK 0xf00
#define IPA_ENDP_INIT_SEQ_n_HPS_REP_SEQ_TYPE_SHFT 0x8
#define IPA_ENDP_INIT_SEQ_n_DPS_SEQ_TYPE_BMSK 0xf0
#define IPA_ENDP_INIT_SEQ_n_DPS_SEQ_TYPE_SHFT 0x4
#define IPA_ENDP_INIT_SEQ_n_HPS_SEQ_TYPE_BMSK 0xf
#define IPA_ENDP_INIT_SEQ_n_HPS_SEQ_TYPE_SHFT 0x0

#endif
