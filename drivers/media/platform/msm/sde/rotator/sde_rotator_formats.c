/* Copyright (c) 2012, 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <media/msm_sde_rotator.h>

#include "sde_rotator_formats.h"
#include "sde_rotator_util.h"

#define FMT_RGB_565(fmt, frame_fmt, flag_arg, e0, e1, e2, isubwc)	\
	{							\
		.format = (fmt),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = 0,				\
		.unpack_count = 3,				\
		.bpp = 2,					\
		.frame_format = (frame_fmt),			\
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,		\
		.element = { (e0), (e1), (e2) },		\
		.bits = {					\
			[C2_R_Cr] = COLOR_5BIT,			\
			[C0_G_Y] = COLOR_6BIT,			\
			[C1_B_Cb] = COLOR_5BIT,			\
		},						\
		.is_ubwc = isubwc,				\
	}

#define FMT_RGB_888(fmt, frame_fmt, flag_arg, e0, e1, e2, isubwc)	\
	{							\
		.format = (fmt),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = 0,				\
		.unpack_count = 3,				\
		.bpp = 3,					\
		.frame_format = (frame_fmt),			\
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,		\
		.element = { (e0), (e1), (e2) },		\
		.bits = {					\
			[C2_R_Cr] = COLOR_8BIT,			\
			[C0_G_Y] = COLOR_8BIT,			\
			[C1_B_Cb] = COLOR_8BIT,			\
		},						\
		.is_ubwc = isubwc,				\
	}

#define FMT_RGB_8888(fmt, frame_fmt, flag_arg,			\
		alpha_en, e0, e1, e2, e3, isubwc)		\
	{							\
		.format = (fmt),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 4,					\
		.frame_format = (frame_fmt),			\
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,		\
		.element = { (e0), (e1), (e2), (e3) },		\
		.bits = {					\
			[C3_ALPHA] = COLOR_8BIT,		\
			[C2_R_Cr] = COLOR_8BIT,			\
			[C0_G_Y] = COLOR_8BIT,			\
			[C1_B_Cb] = COLOR_8BIT,			\
		},						\
		.is_ubwc = isubwc,				\
	}

#define FMT_YUV_COMMON(fmt)					\
		.format = (fmt),				\
		.is_yuv = 1,					\
		.bits = {					\
			[C2_R_Cr] = COLOR_8BIT,			\
			[C0_G_Y] = COLOR_8BIT,			\
			[C1_B_Cb] = COLOR_8BIT,			\
		},						\
		.alpha_enable = 0,				\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0

#define FMT_YUV_PSEUDO(fmt, frame_fmt, samp, pixel_type,	\
		flag_arg, e0, e1, isubwc)			\
	{							\
		FMT_YUV_COMMON(fmt),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_PSEUDO_PLANAR,	\
		.chroma_sample = samp,				\
		.unpack_count = 2,				\
		.bpp = 2,					\
		.frame_format = (frame_fmt),			\
		.pixel_mode = (pixel_type),			\
		.element = { (e0), (e1) },			\
		.is_ubwc = isubwc,				\
	}

#define FMT_YUV_PLANR(fmt, frame_fmt, samp, \
		flag_arg, e0, e1)		\
	{							\
		FMT_YUV_COMMON(fmt),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_PLANAR,		\
		.chroma_sample = samp,				\
		.bpp = 1,					\
		.unpack_count = 1,				\
		.frame_format = (frame_fmt),			\
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,		\
		.element = { (e0), (e1) },			\
		.is_ubwc = SDE_MDP_COMPRESS_NONE,		\
	}

#define FMT_RGB_1555(fmt, alpha_en, flag_arg, e0, e1, e2, e3)	\
	{							\
		.format = (fmt),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 2,					\
		.element = { (e0), (e1), (e2), (e3) },		\
		.frame_format = SDE_MDP_FMT_LINEAR,		\
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,		\
		.bits = {					\
			[C3_ALPHA] = COLOR_ALPHA_1BIT,		\
			[C2_R_Cr] = COLOR_5BIT,			\
			[C0_G_Y] = COLOR_5BIT,			\
			[C1_B_Cb] = COLOR_5BIT,			\
		},						\
		.is_ubwc = SDE_MDP_COMPRESS_NONE,		\
	}

#define FMT_RGB_4444(fmt, alpha_en, flag_arg, e0, e1, e2, e3)		\
	{							\
		.format = (fmt),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 2,					\
		.frame_format = SDE_MDP_FMT_LINEAR,		\
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,		\
		.element = { (e0), (e1), (e2), (e3) },		\
		.bits = {					\
			[C3_ALPHA] = COLOR_ALPHA_4BIT,		\
			[C2_R_Cr] = COLOR_4BIT,			\
			[C0_G_Y] = COLOR_4BIT,			\
			[C1_B_Cb] = COLOR_4BIT,			\
		},						\
		.is_ubwc = SDE_MDP_COMPRESS_NONE,		\
	}

#define FMT_RGB_1010102(fmt, frame_fmt, flag_arg,		\
			alpha_en, e0, e1, e2, e3, isubwc)	\
	{							\
		.format = (fmt),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 4,					\
		.frame_format = frame_fmt,			\
		.pixel_mode = SDE_MDP_PIXEL_10BIT,		\
		.element = { (e0), (e1), (e2), (e3) },		\
		.bits = {					\
			[C3_ALPHA] = COLOR_8BIT,		\
			[C2_R_Cr] = COLOR_8BIT,			\
			[C0_G_Y] = COLOR_8BIT,			\
			[C1_B_Cb] = COLOR_8BIT,			\
		},						\
		.is_ubwc = isubwc,				\
	}

#define VALID_ROT_WB_ALL (VALID_ROT_WB_FORMAT | VALID_ROT_R3_WB_FORMAT)
/*
 * UBWC formats table:
 * This table holds the UBWC formats supported.
 * If a compression ratio needs to be used for this or any other format,
 * the data will be passed by user-space.
 */
static struct sde_mdp_format_params_ubwc sde_mdp_format_ubwc_map[] = {
	{
		.mdp_format = FMT_RGB_565(SDE_PIX_FMT_RGB_565_UBWC,
			SDE_MDP_FMT_TILE_A5X, VALID_ROT_WB_ALL,
			C2_R_Cr, C0_G_Y, C1_B_Cb, SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_RGB_8888(SDE_PIX_FMT_RGBA_8888_UBWC,
			SDE_MDP_FMT_TILE_A5X, VALID_ROT_WB_ALL, 1,
			C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
			SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_RGB_8888(SDE_PIX_FMT_RGBX_8888_UBWC,
			SDE_MDP_FMT_TILE_A5X, VALID_ROT_WB_ALL, 0,
			C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
			SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H2V2_UBWC,
			SDE_MDP_FMT_TILE_A5X, SDE_MDP_CHROMA_420,
			SDE_MDP_PIXEL_NORMAL,
			VALID_ROT_WB_ALL, C1_B_Cb, C2_R_Cr,
			SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 8,
			.tile_width = 32,
		},
	},
	{
		.mdp_format = FMT_RGB_1010102(SDE_PIX_FMT_RGBA_1010102_UBWC,
			SDE_MDP_FMT_TILE_A5X, VALID_ROT_R3_WB_FORMAT, 1,
			C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
			SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_RGB_1010102(SDE_PIX_FMT_RGBX_1010102_UBWC,
			SDE_MDP_FMT_TILE_A5X, VALID_ROT_R3_WB_FORMAT, 0,
			C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
			SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H2V2_TP10_UBWC,
			SDE_MDP_FMT_TILE_A5X, SDE_MDP_CHROMA_420,
			SDE_MDP_PIXEL_10BIT,
			VALID_ROT_R3_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT,
			C1_B_Cb, C2_R_Cr, SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 4,
			.tile_width = 48,
		},
	},
};

static struct sde_mdp_format_params sde_mdp_format_map[] = {
	FMT_RGB_565(
		SDE_PIX_FMT_RGB_565, SDE_MDP_FMT_LINEAR, VALID_ROT_WB_ALL |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C0_G_Y, C2_R_Cr,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_565(
		SDE_PIX_FMT_BGR_565, SDE_MDP_FMT_LINEAR, VALID_ROT_WB_ALL |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C0_G_Y, C1_B_Cb,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_888(
		SDE_PIX_FMT_RGB_888, SDE_MDP_FMT_LINEAR, VALID_ROT_WB_ALL |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C0_G_Y, C1_B_Cb,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_888(
		SDE_PIX_FMT_BGR_888, SDE_MDP_FMT_LINEAR, VALID_ROT_WB_ALL |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C0_G_Y, C2_R_Cr,
		SDE_MDP_COMPRESS_NONE),

	FMT_RGB_8888(
		SDE_PIX_FMT_ABGR_8888, SDE_MDP_FMT_LINEAR,
		VALID_ROT_WB_ALL, 1, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr,
		SDE_MDP_COMPRESS_NONE),

	FMT_RGB_8888(
		SDE_PIX_FMT_XRGB_8888, SDE_MDP_FMT_LINEAR,
		VALID_ROT_WB_ALL | VALID_MDP_WB_INTF_FORMAT,
		0, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb, SDE_MDP_COMPRESS_NONE),
	FMT_RGB_8888(
		SDE_PIX_FMT_ARGB_8888, SDE_MDP_FMT_LINEAR,
		VALID_ROT_WB_ALL, 1, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_8888(
		SDE_PIX_FMT_RGBA_8888, SDE_MDP_FMT_LINEAR,
		VALID_ROT_WB_ALL, 1, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_8888(
		SDE_PIX_FMT_RGBX_8888, SDE_MDP_FMT_LINEAR,
		VALID_ROT_WB_ALL | VALID_MDP_WB_INTF_FORMAT,
		0, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, SDE_MDP_COMPRESS_NONE),
	FMT_RGB_8888(
		SDE_PIX_FMT_BGRA_8888, SDE_MDP_FMT_LINEAR,
		VALID_ROT_WB_ALL, 1, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_8888(
		SDE_PIX_FMT_BGRX_8888, SDE_MDP_FMT_LINEAR,
		VALID_ROT_WB_ALL | VALID_MDP_WB_INTF_FORMAT,
		0, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, SDE_MDP_COMPRESS_NONE),

	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CRCB_H2V1, SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_H2V1, SDE_MDP_PIXEL_NORMAL,
		VALID_ROT_WB_ALL, C2_R_Cr, C1_B_Cb, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H2V1, SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_H2V1, SDE_MDP_PIXEL_NORMAL,
		VALID_ROT_WB_ALL, C1_B_Cb, C2_R_Cr, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CRCB_H1V2, SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_H1V2, SDE_MDP_PIXEL_NORMAL,
		VALID_ROT_WB_ALL, C2_R_Cr, C1_B_Cb, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H1V2, SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_H1V2, SDE_MDP_PIXEL_NORMAL,
		VALID_ROT_WB_ALL, C1_B_Cb, C2_R_Cr, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CRCB_H2V2, SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, SDE_MDP_PIXEL_NORMAL,
		VALID_ROT_WB_ALL | VALID_MDP_WB_INTF_FORMAT,
		C2_R_Cr, C1_B_Cb, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H2V2, SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, SDE_MDP_PIXEL_NORMAL,
		VALID_ROT_WB_ALL | VALID_MDP_WB_INTF_FORMAT,
		C1_B_Cb, C2_R_Cr, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H2V2_VENUS, SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, SDE_MDP_PIXEL_NORMAL,
		VALID_ROT_WB_ALL | VALID_MDP_WB_INTF_FORMAT,
		C1_B_Cb, C2_R_Cr, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CRCB_H2V2_VENUS, SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, SDE_MDP_PIXEL_NORMAL,
		VALID_ROT_WB_ALL | VALID_MDP_WB_INTF_FORMAT,
		C2_R_Cr, C1_B_Cb, SDE_MDP_COMPRESS_NONE),

	{
		FMT_YUV_COMMON(SDE_PIX_FMT_Y_CBCR_H2V2_P010),
		.flag = VALID_ROT_R3_WB_FORMAT,
		.fetch_planes = SDE_MDP_PLANE_PSEUDO_PLANAR,
		.chroma_sample = SDE_MDP_CHROMA_420,
		.unpack_count = 2,
		.bpp = 2,
		.frame_format = SDE_MDP_FMT_LINEAR,
		.pixel_mode = SDE_MDP_PIXEL_10BIT,
		.element = { C1_B_Cb, C2_R_Cr },
		.unpack_tight = 0,
		.unpack_align_msb = 1,
		.is_ubwc = SDE_MDP_COMPRESS_NONE,
	},
	{
		FMT_YUV_COMMON(SDE_PIX_FMT_Y_CBCR_H2V2_TP10),
		.flag = VALID_ROT_R3_WB_FORMAT,
		.fetch_planes = SDE_MDP_PLANE_PSEUDO_PLANAR,
		.chroma_sample = SDE_MDP_CHROMA_420,
		.unpack_count = 2,
		.bpp = 2,
		.frame_format = SDE_MDP_FMT_TILE_A5X,
		.pixel_mode = SDE_MDP_PIXEL_10BIT,
		.element = { C1_B_Cb, C2_R_Cr },
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.is_ubwc = SDE_MDP_COMPRESS_NONE,
	},

	FMT_YUV_PLANR(SDE_PIX_FMT_Y_CB_CR_H2V2, SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C1_B_Cb),
	FMT_YUV_PLANR(SDE_PIX_FMT_Y_CR_CB_H2V2, SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PLANR(SDE_PIX_FMT_Y_CR_CB_GH2V2, SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C2_R_Cr),

	{
		FMT_YUV_COMMON(SDE_PIX_FMT_YCBYCR_H2V1),
		.flag = VALID_ROT_WB_FORMAT,
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,
		.chroma_sample = SDE_MDP_CHROMA_H2V1,
		.unpack_count = 4,
		.bpp = 2,
		.frame_format = SDE_MDP_FMT_LINEAR,
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,
		.element = { C2_R_Cr, C0_G_Y, C1_B_Cb, C0_G_Y },
		.is_ubwc = SDE_MDP_COMPRESS_NONE,
	},
	FMT_RGB_1555(SDE_PIX_FMT_RGBA_5551, 1, VALID_ROT_WB_ALL,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_4444(SDE_PIX_FMT_RGBA_4444, 1, VALID_ROT_WB_ALL,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_4444(SDE_PIX_FMT_ARGB_4444, 1, VALID_ROT_WB_ALL,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),
	FMT_RGB_1010102(SDE_PIX_FMT_RGBA_1010102, SDE_MDP_FMT_LINEAR,
		VALID_ROT_R3_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT,
		1, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_RGBX_1010102, SDE_MDP_FMT_LINEAR,
		VALID_ROT_R3_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT,
		0, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_BGRA_1010102, SDE_MDP_FMT_LINEAR,
		VALID_ROT_R3_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT,
		1, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_BGRX_1010102, SDE_MDP_FMT_LINEAR,
		VALID_ROT_R3_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT,
		0, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_ARGB_2101010, SDE_MDP_FMT_LINEAR,
		INVALID_WB_FORMAT, 1, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_XRGB_2101010, SDE_MDP_FMT_LINEAR,
		INVALID_WB_FORMAT, 0, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_ABGR_2101010, SDE_MDP_FMT_LINEAR,
		INVALID_WB_FORMAT, 1, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_XBGR_2101010, SDE_MDP_FMT_LINEAR,
		INVALID_WB_FORMAT, 0, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr,
		SDE_MDP_COMPRESS_NONE),
};

/*
 * sde_get_format_params - return format parameter of the given format
 * @format: format to lookup
 */
struct sde_mdp_format_params *sde_get_format_params(u32 format)
{
	struct sde_mdp_format_params *fmt = NULL;
	int i;
	bool fmt_found = false;

	for (i = 0; i < ARRAY_SIZE(sde_mdp_format_map); i++) {
		fmt = &sde_mdp_format_map[i];
		if (format == fmt->format) {
			fmt_found = true;
			break;
		}
	}

	if (!fmt_found) {
		for (i = 0; i < ARRAY_SIZE(sde_mdp_format_ubwc_map); i++) {
			fmt = &sde_mdp_format_ubwc_map[i].mdp_format;
			if (format == fmt->format)
				break;
		}
	}

	return fmt;
}

/*
 * sde_rot_get_ubwc_micro_dim - return micro dimension of the given ubwc format
 * @format: format to lookup
 * @w: Pointer to returned width dimension
 * @h: Pointer to returned height dimension
 */
int sde_rot_get_ubwc_micro_dim(u32 format, u16 *w, u16 *h)
{
	struct sde_mdp_format_params_ubwc *fmt = NULL;
	bool fmt_found = false;
	int i;

	for (i = 0; i < ARRAY_SIZE(sde_mdp_format_ubwc_map); i++) {
		fmt = &sde_mdp_format_ubwc_map[i];
		if (format == fmt->mdp_format.format) {
			fmt_found = true;
			break;
		}
	}

	if (!fmt_found)
		return -EINVAL;

	*w = fmt->micro.tile_width;
	*h = fmt->micro.tile_height;

	return 0;
}

/*
 * sde_mdp_is_wb_format - determine if the given fmt is supported by writeback
 * @fmt: Pointer to format parameter
 */
bool sde_mdp_is_wb_format(struct sde_mdp_format_params *fmt)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();

	if (!mdata || !fmt)
		return false;
	else if (test_bit(SDE_CAPS_R1_WB, mdata->sde_caps_map) &&
			(fmt->flag & VALID_ROT_WB_FORMAT))
		return true;
	else if (test_bit(SDE_CAPS_R3_WB, mdata->sde_caps_map) &&
			(fmt->flag & VALID_ROT_R3_WB_FORMAT))
		return true;
	else
		return false;
}

