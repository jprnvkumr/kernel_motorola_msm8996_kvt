/*
 * wm5102.c  --  WM5102 ALSA SoC Audio driver
 *
 * Copyright 2012 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/mfd/arizona/core.h>
#include <linux/mfd/arizona/registers.h>

#include "arizona.h"
#include "wm5102.h"
#include "wm_adsp.h"

struct wm5102_priv {
	struct arizona_priv core;
	struct arizona_fll fll[2];

	struct mutex fw_lock;
};

static DECLARE_TLV_DB_SCALE(ana_tlv, 0, 100, 0);
static DECLARE_TLV_DB_SCALE(eq_tlv, -1200, 100, 0);
static DECLARE_TLV_DB_SCALE(digital_tlv, -6400, 50, 0);
static DECLARE_TLV_DB_SCALE(noise_tlv, -13200, 600, 0);
static DECLARE_TLV_DB_SCALE(ng_tlv, -10200, 600, 0);

static const struct wm_adsp_region wm5102_dsp1_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x100000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x180000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x190000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x1a8000 },
};

static const struct reg_default wm5102_sysclk_reva_patch[] = {
	{ 0x3000, 0x2225 },
	{ 0x3001, 0x3a03 },
	{ 0x3002, 0x0225 },
	{ 0x3003, 0x0801 },
	{ 0x3004, 0x6249 },
	{ 0x3005, 0x0c04 },
	{ 0x3006, 0x0225 },
	{ 0x3007, 0x5901 },
	{ 0x3008, 0xe249 },
	{ 0x3009, 0x030d },
	{ 0x300a, 0x0249 },
	{ 0x300b, 0x2c01 },
	{ 0x300c, 0xe249 },
	{ 0x300d, 0x4342 },
	{ 0x300e, 0xe249 },
	{ 0x300f, 0x73c0 },
	{ 0x3010, 0x4249 },
	{ 0x3011, 0x0c00 },
	{ 0x3012, 0x0225 },
	{ 0x3013, 0x1f01 },
	{ 0x3014, 0x0225 },
	{ 0x3015, 0x1e01 },
	{ 0x3016, 0x0225 },
	{ 0x3017, 0xfa00 },
	{ 0x3018, 0x0000 },
	{ 0x3019, 0xf000 },
	{ 0x301a, 0x0000 },
	{ 0x301b, 0xf000 },
	{ 0x301c, 0x0000 },
	{ 0x301d, 0xf000 },
	{ 0x301e, 0x0000 },
	{ 0x301f, 0xf000 },
	{ 0x3020, 0x0000 },
	{ 0x3021, 0xf000 },
	{ 0x3022, 0x0000 },
	{ 0x3023, 0xf000 },
	{ 0x3024, 0x0000 },
	{ 0x3025, 0xf000 },
	{ 0x3026, 0x0000 },
	{ 0x3027, 0xf000 },
	{ 0x3028, 0x0000 },
	{ 0x3029, 0xf000 },
	{ 0x302a, 0x0000 },
	{ 0x302b, 0xf000 },
	{ 0x302c, 0x0000 },
	{ 0x302d, 0xf000 },
	{ 0x302e, 0x0000 },
	{ 0x302f, 0xf000 },
	{ 0x3030, 0x0225 },
	{ 0x3031, 0x1a01 },
	{ 0x3032, 0x0225 },
	{ 0x3033, 0x1e00 },
	{ 0x3034, 0x0225 },
	{ 0x3035, 0x1f00 },
	{ 0x3036, 0x6225 },
	{ 0x3037, 0xf800 },
	{ 0x3038, 0x0000 },
	{ 0x3039, 0xf000 },
	{ 0x303a, 0x0000 },
	{ 0x303b, 0xf000 },
	{ 0x303c, 0x0000 },
	{ 0x303d, 0xf000 },
	{ 0x303e, 0x0000 },
	{ 0x303f, 0xf000 },
	{ 0x3040, 0x2226 },
	{ 0x3041, 0x3a03 },
	{ 0x3042, 0x0226 },
	{ 0x3043, 0x0801 },
	{ 0x3044, 0x6249 },
	{ 0x3045, 0x0c06 },
	{ 0x3046, 0x0226 },
	{ 0x3047, 0x5901 },
	{ 0x3048, 0xe249 },
	{ 0x3049, 0x030d },
	{ 0x304a, 0x0249 },
	{ 0x304b, 0x2c01 },
	{ 0x304c, 0xe249 },
	{ 0x304d, 0x4342 },
	{ 0x304e, 0xe249 },
	{ 0x304f, 0x73c0 },
	{ 0x3050, 0x4249 },
	{ 0x3051, 0x0c00 },
	{ 0x3052, 0x0226 },
	{ 0x3053, 0x1f01 },
	{ 0x3054, 0x0226 },
	{ 0x3055, 0x1e01 },
	{ 0x3056, 0x0226 },
	{ 0x3057, 0xfa00 },
	{ 0x3058, 0x0000 },
	{ 0x3059, 0xf000 },
	{ 0x305a, 0x0000 },
	{ 0x305b, 0xf000 },
	{ 0x305c, 0x0000 },
	{ 0x305d, 0xf000 },
	{ 0x305e, 0x0000 },
	{ 0x305f, 0xf000 },
	{ 0x3060, 0x0000 },
	{ 0x3061, 0xf000 },
	{ 0x3062, 0x0000 },
	{ 0x3063, 0xf000 },
	{ 0x3064, 0x0000 },
	{ 0x3065, 0xf000 },
	{ 0x3066, 0x0000 },
	{ 0x3067, 0xf000 },
	{ 0x3068, 0x0000 },
	{ 0x3069, 0xf000 },
	{ 0x306a, 0x0000 },
	{ 0x306b, 0xf000 },
	{ 0x306c, 0x0000 },
	{ 0x306d, 0xf000 },
	{ 0x306e, 0x0000 },
	{ 0x306f, 0xf000 },
	{ 0x3070, 0x0226 },
	{ 0x3071, 0x1a01 },
	{ 0x3072, 0x0226 },
	{ 0x3073, 0x1e00 },
	{ 0x3074, 0x0226 },
	{ 0x3075, 0x1f00 },
	{ 0x3076, 0x6226 },
	{ 0x3077, 0xf800 },
	{ 0x3078, 0x0000 },
	{ 0x3079, 0xf000 },
	{ 0x307a, 0x0000 },
	{ 0x307b, 0xf000 },
	{ 0x307c, 0x0000 },
	{ 0x307d, 0xf000 },
	{ 0x307e, 0x0000 },
	{ 0x307f, 0xf000 },
	{ 0x3080, 0x2227 },
	{ 0x3081, 0x3a03 },
	{ 0x3082, 0x0227 },
	{ 0x3083, 0x0801 },
	{ 0x3084, 0x6255 },
	{ 0x3085, 0x0c04 },
	{ 0x3086, 0x0227 },
	{ 0x3087, 0x5901 },
	{ 0x3088, 0xe255 },
	{ 0x3089, 0x030d },
	{ 0x308a, 0x0255 },
	{ 0x308b, 0x2c01 },
	{ 0x308c, 0xe255 },
	{ 0x308d, 0x4342 },
	{ 0x308e, 0xe255 },
	{ 0x308f, 0x73c0 },
	{ 0x3090, 0x4255 },
	{ 0x3091, 0x0c00 },
	{ 0x3092, 0x0227 },
	{ 0x3093, 0x1f01 },
	{ 0x3094, 0x0227 },
	{ 0x3095, 0x1e01 },
	{ 0x3096, 0x0227 },
	{ 0x3097, 0xfa00 },
	{ 0x3098, 0x0000 },
	{ 0x3099, 0xf000 },
	{ 0x309a, 0x0000 },
	{ 0x309b, 0xf000 },
	{ 0x309c, 0x0000 },
	{ 0x309d, 0xf000 },
	{ 0x309e, 0x0000 },
	{ 0x309f, 0xf000 },
	{ 0x30a0, 0x0000 },
	{ 0x30a1, 0xf000 },
	{ 0x30a2, 0x0000 },
	{ 0x30a3, 0xf000 },
	{ 0x30a4, 0x0000 },
	{ 0x30a5, 0xf000 },
	{ 0x30a6, 0x0000 },
	{ 0x30a7, 0xf000 },
	{ 0x30a8, 0x0000 },
	{ 0x30a9, 0xf000 },
	{ 0x30aa, 0x0000 },
	{ 0x30ab, 0xf000 },
	{ 0x30ac, 0x0000 },
	{ 0x30ad, 0xf000 },
	{ 0x30ae, 0x0000 },
	{ 0x30af, 0xf000 },
	{ 0x30b0, 0x0227 },
	{ 0x30b1, 0x1a01 },
	{ 0x30b2, 0x0227 },
	{ 0x30b3, 0x1e00 },
	{ 0x30b4, 0x0227 },
	{ 0x30b5, 0x1f00 },
	{ 0x30b6, 0x6227 },
	{ 0x30b7, 0xf800 },
	{ 0x30b8, 0x0000 },
	{ 0x30b9, 0xf000 },
	{ 0x30ba, 0x0000 },
	{ 0x30bb, 0xf000 },
	{ 0x30bc, 0x0000 },
	{ 0x30bd, 0xf000 },
	{ 0x30be, 0x0000 },
	{ 0x30bf, 0xf000 },
	{ 0x30c0, 0x2228 },
	{ 0x30c1, 0x3a03 },
	{ 0x30c2, 0x0228 },
	{ 0x30c3, 0x0801 },
	{ 0x30c4, 0x6255 },
	{ 0x30c5, 0x0c06 },
	{ 0x30c6, 0x0228 },
	{ 0x30c7, 0x5901 },
	{ 0x30c8, 0xe255 },
	{ 0x30c9, 0x030d },
	{ 0x30ca, 0x0255 },
	{ 0x30cb, 0x2c01 },
	{ 0x30cc, 0xe255 },
	{ 0x30cd, 0x4342 },
	{ 0x30ce, 0xe255 },
	{ 0x30cf, 0x73c0 },
	{ 0x30d0, 0x4255 },
	{ 0x30d1, 0x0c00 },
	{ 0x30d2, 0x0228 },
	{ 0x30d3, 0x1f01 },
	{ 0x30d4, 0x0228 },
	{ 0x30d5, 0x1e01 },
	{ 0x30d6, 0x0228 },
	{ 0x30d7, 0xfa00 },
	{ 0x30d8, 0x0000 },
	{ 0x30d9, 0xf000 },
	{ 0x30da, 0x0000 },
	{ 0x30db, 0xf000 },
	{ 0x30dc, 0x0000 },
	{ 0x30dd, 0xf000 },
	{ 0x30de, 0x0000 },
	{ 0x30df, 0xf000 },
	{ 0x30e0, 0x0000 },
	{ 0x30e1, 0xf000 },
	{ 0x30e2, 0x0000 },
	{ 0x30e3, 0xf000 },
	{ 0x30e4, 0x0000 },
	{ 0x30e5, 0xf000 },
	{ 0x30e6, 0x0000 },
	{ 0x30e7, 0xf000 },
	{ 0x30e8, 0x0000 },
	{ 0x30e9, 0xf000 },
	{ 0x30ea, 0x0000 },
	{ 0x30eb, 0xf000 },
	{ 0x30ec, 0x0000 },
	{ 0x30ed, 0xf000 },
	{ 0x30ee, 0x0000 },
	{ 0x30ef, 0xf000 },
	{ 0x30f0, 0x0228 },
	{ 0x30f1, 0x1a01 },
	{ 0x30f2, 0x0228 },
	{ 0x30f3, 0x1e00 },
	{ 0x30f4, 0x0228 },
	{ 0x30f5, 0x1f00 },
	{ 0x30f6, 0x6228 },
	{ 0x30f7, 0xf800 },
	{ 0x30f8, 0x0000 },
	{ 0x30f9, 0xf000 },
	{ 0x30fa, 0x0000 },
	{ 0x30fb, 0xf000 },
	{ 0x30fc, 0x0000 },
	{ 0x30fd, 0xf000 },
	{ 0x30fe, 0x0000 },
	{ 0x30ff, 0xf000 },
	{ 0x3100, 0x222b },
	{ 0x3101, 0x3a03 },
	{ 0x3102, 0x222b },
	{ 0x3103, 0x5803 },
	{ 0x3104, 0xe26f },
	{ 0x3105, 0x030d },
	{ 0x3106, 0x626f },
	{ 0x3107, 0x2c01 },
	{ 0x3108, 0xe26f },
	{ 0x3109, 0x4342 },
	{ 0x310a, 0xe26f },
	{ 0x310b, 0x73c0 },
	{ 0x310c, 0x026f },
	{ 0x310d, 0x0c00 },
	{ 0x310e, 0x022b },
	{ 0x310f, 0x1f01 },
	{ 0x3110, 0x022b },
	{ 0x3111, 0x1e01 },
	{ 0x3112, 0x022b },
	{ 0x3113, 0xfa00 },
	{ 0x3114, 0x0000 },
	{ 0x3115, 0xf000 },
	{ 0x3116, 0x0000 },
	{ 0x3117, 0xf000 },
	{ 0x3118, 0x0000 },
	{ 0x3119, 0xf000 },
	{ 0x311a, 0x0000 },
	{ 0x311b, 0xf000 },
	{ 0x311c, 0x0000 },
	{ 0x311d, 0xf000 },
	{ 0x311e, 0x0000 },
	{ 0x311f, 0xf000 },
	{ 0x3120, 0x022b },
	{ 0x3121, 0x0a01 },
	{ 0x3122, 0x022b },
	{ 0x3123, 0x1e00 },
	{ 0x3124, 0x022b },
	{ 0x3125, 0x1f00 },
	{ 0x3126, 0x622b },
	{ 0x3127, 0xf800 },
	{ 0x3128, 0x0000 },
	{ 0x3129, 0xf000 },
	{ 0x312a, 0x0000 },
	{ 0x312b, 0xf000 },
	{ 0x312c, 0x0000 },
	{ 0x312d, 0xf000 },
	{ 0x312e, 0x0000 },
	{ 0x312f, 0xf000 },
	{ 0x3130, 0x0000 },
	{ 0x3131, 0xf000 },
	{ 0x3132, 0x0000 },
	{ 0x3133, 0xf000 },
	{ 0x3134, 0x0000 },
	{ 0x3135, 0xf000 },
	{ 0x3136, 0x0000 },
	{ 0x3137, 0xf000 },
	{ 0x3138, 0x0000 },
	{ 0x3139, 0xf000 },
	{ 0x313a, 0x0000 },
	{ 0x313b, 0xf000 },
	{ 0x313c, 0x0000 },
	{ 0x313d, 0xf000 },
	{ 0x313e, 0x0000 },
	{ 0x313f, 0xf000 },
	{ 0x3140, 0x0000 },
	{ 0x3141, 0xf000 },
	{ 0x3142, 0x0000 },
	{ 0x3143, 0xf000 },
	{ 0x3144, 0x0000 },
	{ 0x3145, 0xf000 },
	{ 0x3146, 0x0000 },
	{ 0x3147, 0xf000 },
	{ 0x3148, 0x0000 },
	{ 0x3149, 0xf000 },
	{ 0x314a, 0x0000 },
	{ 0x314b, 0xf000 },
	{ 0x314c, 0x0000 },
	{ 0x314d, 0xf000 },
	{ 0x314e, 0x0000 },
	{ 0x314f, 0xf000 },
	{ 0x3150, 0x0000 },
	{ 0x3151, 0xf000 },
	{ 0x3152, 0x0000 },
	{ 0x3153, 0xf000 },
	{ 0x3154, 0x0000 },
	{ 0x3155, 0xf000 },
	{ 0x3156, 0x0000 },
	{ 0x3157, 0xf000 },
	{ 0x3158, 0x0000 },
	{ 0x3159, 0xf000 },
	{ 0x315a, 0x0000 },
	{ 0x315b, 0xf000 },
	{ 0x315c, 0x0000 },
	{ 0x315d, 0xf000 },
	{ 0x315e, 0x0000 },
	{ 0x315f, 0xf000 },
	{ 0x3160, 0x0000 },
	{ 0x3161, 0xf000 },
	{ 0x3162, 0x0000 },
	{ 0x3163, 0xf000 },
	{ 0x3164, 0x0000 },
	{ 0x3165, 0xf000 },
	{ 0x3166, 0x0000 },
	{ 0x3167, 0xf000 },
	{ 0x3168, 0x0000 },
	{ 0x3169, 0xf000 },
	{ 0x316a, 0x0000 },
	{ 0x316b, 0xf000 },
	{ 0x316c, 0x0000 },
	{ 0x316d, 0xf000 },
	{ 0x316e, 0x0000 },
	{ 0x316f, 0xf000 },
	{ 0x3170, 0x0000 },
	{ 0x3171, 0xf000 },
	{ 0x3172, 0x0000 },
	{ 0x3173, 0xf000 },
	{ 0x3174, 0x0000 },
	{ 0x3175, 0xf000 },
	{ 0x3176, 0x0000 },
	{ 0x3177, 0xf000 },
	{ 0x3178, 0x0000 },
	{ 0x3179, 0xf000 },
	{ 0x317a, 0x0000 },
	{ 0x317b, 0xf000 },
	{ 0x317c, 0x0000 },
	{ 0x317d, 0xf000 },
	{ 0x317e, 0x0000 },
	{ 0x317f, 0xf000 },
	{ 0x3180, 0x2001 },
	{ 0x3181, 0xf101 },
	{ 0x3182, 0x0000 },
	{ 0x3183, 0xf000 },
	{ 0x3184, 0x0000 },
	{ 0x3185, 0xf000 },
	{ 0x3186, 0x0000 },
	{ 0x3187, 0xf000 },
	{ 0x3188, 0x0000 },
	{ 0x3189, 0xf000 },
	{ 0x318a, 0x0000 },
	{ 0x318b, 0xf000 },
	{ 0x318c, 0x0000 },
	{ 0x318d, 0xf000 },
	{ 0x318e, 0x0000 },
	{ 0x318f, 0xf000 },
	{ 0x3190, 0x0000 },
	{ 0x3191, 0xf000 },
	{ 0x3192, 0x0000 },
	{ 0x3193, 0xf000 },
	{ 0x3194, 0x0000 },
	{ 0x3195, 0xf000 },
	{ 0x3196, 0x0000 },
	{ 0x3197, 0xf000 },
	{ 0x3198, 0x0000 },
	{ 0x3199, 0xf000 },
	{ 0x319a, 0x0000 },
	{ 0x319b, 0xf000 },
	{ 0x319c, 0x0000 },
	{ 0x319d, 0xf000 },
	{ 0x319e, 0x0000 },
	{ 0x319f, 0xf000 },
	{ 0x31a0, 0x0000 },
	{ 0x31a1, 0xf000 },
	{ 0x31a2, 0x0000 },
	{ 0x31a3, 0xf000 },
	{ 0x31a4, 0x0000 },
	{ 0x31a5, 0xf000 },
	{ 0x31a6, 0x0000 },
	{ 0x31a7, 0xf000 },
	{ 0x31a8, 0x0000 },
	{ 0x31a9, 0xf000 },
	{ 0x31aa, 0x0000 },
	{ 0x31ab, 0xf000 },
	{ 0x31ac, 0x0000 },
	{ 0x31ad, 0xf000 },
	{ 0x31ae, 0x0000 },
	{ 0x31af, 0xf000 },
	{ 0x31b0, 0x0000 },
	{ 0x31b1, 0xf000 },
	{ 0x31b2, 0x0000 },
	{ 0x31b3, 0xf000 },
	{ 0x31b4, 0x0000 },
	{ 0x31b5, 0xf000 },
	{ 0x31b6, 0x0000 },
	{ 0x31b7, 0xf000 },
	{ 0x31b8, 0x0000 },
	{ 0x31b9, 0xf000 },
	{ 0x31ba, 0x0000 },
	{ 0x31bb, 0xf000 },
	{ 0x31bc, 0x0000 },
	{ 0x31bd, 0xf000 },
	{ 0x31be, 0x0000 },
	{ 0x31bf, 0xf000 },
	{ 0x31c0, 0x0000 },
	{ 0x31c1, 0xf000 },
	{ 0x31c2, 0x0000 },
	{ 0x31c3, 0xf000 },
	{ 0x31c4, 0x0000 },
	{ 0x31c5, 0xf000 },
	{ 0x31c6, 0x0000 },
	{ 0x31c7, 0xf000 },
	{ 0x31c8, 0x0000 },
	{ 0x31c9, 0xf000 },
	{ 0x31ca, 0x0000 },
	{ 0x31cb, 0xf000 },
	{ 0x31cc, 0x0000 },
	{ 0x31cd, 0xf000 },
	{ 0x31ce, 0x0000 },
	{ 0x31cf, 0xf000 },
	{ 0x31d0, 0x0000 },
	{ 0x31d1, 0xf000 },
	{ 0x31d2, 0x0000 },
	{ 0x31d3, 0xf000 },
	{ 0x31d4, 0x0000 },
	{ 0x31d5, 0xf000 },
	{ 0x31d6, 0x0000 },
	{ 0x31d7, 0xf000 },
	{ 0x31d8, 0x0000 },
	{ 0x31d9, 0xf000 },
	{ 0x31da, 0x0000 },
	{ 0x31db, 0xf000 },
	{ 0x31dc, 0x0000 },
	{ 0x31dd, 0xf000 },
	{ 0x31de, 0x0000 },
	{ 0x31df, 0xf000 },
	{ 0x31e0, 0x0000 },
	{ 0x31e1, 0xf000 },
	{ 0x31e2, 0x0000 },
	{ 0x31e3, 0xf000 },
	{ 0x31e4, 0x0000 },
	{ 0x31e5, 0xf000 },
	{ 0x31e6, 0x0000 },
	{ 0x31e7, 0xf000 },
	{ 0x31e8, 0x0000 },
	{ 0x31e9, 0xf000 },
	{ 0x31ea, 0x0000 },
	{ 0x31eb, 0xf000 },
	{ 0x31ec, 0x0000 },
	{ 0x31ed, 0xf000 },
	{ 0x31ee, 0x0000 },
	{ 0x31ef, 0xf000 },
	{ 0x31f0, 0x0000 },
	{ 0x31f1, 0xf000 },
	{ 0x31f2, 0x0000 },
	{ 0x31f3, 0xf000 },
	{ 0x31f4, 0x0000 },
	{ 0x31f5, 0xf000 },
	{ 0x31f6, 0x0000 },
	{ 0x31f7, 0xf000 },
	{ 0x31f8, 0x0000 },
	{ 0x31f9, 0xf000 },
	{ 0x31fa, 0x0000 },
	{ 0x31fb, 0xf000 },
	{ 0x31fc, 0x0000 },
	{ 0x31fd, 0xf000 },
	{ 0x31fe, 0x0000 },
	{ 0x31ff, 0xf000 },
	{ 0x024d, 0xff50 },
	{ 0x0252, 0xff50 },
	{ 0x0259, 0x0112 },
	{ 0x025e, 0x0112 },
};

static const struct reg_default wm5102_sysclk_revb_patch[] = {
	{ 0x3081, 0x08FE },
	{ 0x3083, 0x00ED },
	{ 0x30C1, 0x08FE },
	{ 0x30C3, 0x00ED },
};

static const struct reg_default wm5102t_sysclk_pwr[] = {
	{ 0x3125, 0x0A03 },
	{ 0x3127, 0x0A03 },
	{ 0x3129, 0x0A03 },
};

static int wm5102_sysclk_ev(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	struct regmap *regmap = arizona->regmap;
	const struct reg_default *patch = NULL;
	int patch_size;

	switch (arizona->rev) {
	case 0:
		patch = wm5102_sysclk_reva_patch;
		patch_size = ARRAY_SIZE(wm5102_sysclk_reva_patch);
		break;
	default:
		patch = wm5102_sysclk_revb_patch;
		patch_size = ARRAY_SIZE(wm5102_sysclk_revb_patch);
		break;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (patch)
			regmap_multi_reg_write_bypassed(regmap, patch, patch_size);

		if (arizona->pdata.wm5102t_output_pwr)
			regmap_multi_reg_write_bypassed(regmap,
							wm5102t_sysclk_pwr,
							ARRAY_SIZE(wm5102t_sysclk_pwr));
		break;

	default:
		break;
	}

	return 0;
}

static int wm5102_adsp_power_ev(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	unsigned int v;
	int ret;

	ret = regmap_read(arizona->regmap, ARIZONA_SYSTEM_CLOCK_1, &v);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to read SYSCLK state: %d\n", ret);
		return -EIO;
	}

	v = (v & ARIZONA_SYSCLK_FREQ_MASK) >> ARIZONA_SYSCLK_FREQ_SHIFT;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (v >= 3) {
			ret = arizona_dvfs_up(arizona, ARIZONA_DVFS_ADSP1_RQ);
			if (ret != 0) {
				dev_err(codec->dev,
					"Failed to raise DVFS: %d\n", ret);
				return ret;
			}
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
		ret = arizona_dvfs_down(arizona, ARIZONA_DVFS_ADSP1_RQ);
		if (ret != 0)
			dev_warn(codec->dev,
				 "Failed to lower DVFS: %d\n", ret);
		break;

	default:
		break;
	}

	return arizona_adsp_power_ev(w, kcontrol, event);
}

static int wm5102_out_comp_coeff_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	uint16_t data;

	mutex_lock(&codec->mutex);
	data = cpu_to_be16(arizona->dac_comp_coeff);
	memcpy(ucontrol->value.bytes.data, &data, sizeof(data));
	mutex_unlock(&codec->mutex);

	return 0;
}

static int wm5102_out_comp_coeff_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);

	mutex_lock(&codec->mutex);
	memcpy(&arizona->dac_comp_coeff, ucontrol->value.bytes.data,
	       sizeof(arizona->dac_comp_coeff));
	arizona->dac_comp_coeff = be16_to_cpu(arizona->dac_comp_coeff);
	mutex_unlock(&codec->mutex);

	return 0;
}

static int wm5102_out_comp_switch_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);

	mutex_lock(&codec->mutex);
	ucontrol->value.integer.value[0] = arizona->dac_comp_enabled;
	mutex_unlock(&codec->mutex);

	return 0;
}

static int wm5102_out_comp_switch_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);

	mutex_lock(&codec->mutex);
	arizona->dac_comp_enabled = ucontrol->value.integer.value[0];
	mutex_unlock(&codec->mutex);

	return 0;
}

static const char * const wm5102_osr_text[] = {
	"Low power", "Normal", "High performance",
};

static const unsigned int wm5102_osr_val[] = {
	0x0, 0x3, 0x5,
};

static const struct soc_enum wm5102_hpout_osr[] = {
	SOC_VALUE_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_1L,
			      ARIZONA_OUT1_OSR_SHIFT, 0x7,
			      ARRAY_SIZE(wm5102_osr_text),
			      wm5102_osr_text, wm5102_osr_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_2L,
			      ARIZONA_OUT2_OSR_SHIFT, 0x7,
			      ARRAY_SIZE(wm5102_osr_text),
			      wm5102_osr_text, wm5102_osr_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_3L,
			      ARIZONA_OUT3_OSR_SHIFT, 0x7,
			      ARRAY_SIZE(wm5102_osr_text),
			      wm5102_osr_text, wm5102_osr_val),
};

#define WM5102_NG_SRC(name, base) \
	SOC_SINGLE(name " NG HPOUT1L Switch",  base, 0, 1, 0), \
	SOC_SINGLE(name " NG HPOUT1R Switch",  base, 1, 1, 0), \
	SOC_SINGLE(name " NG HPOUT2L Switch",  base, 2, 1, 0), \
	SOC_SINGLE(name " NG HPOUT2R Switch",  base, 3, 1, 0), \
	SOC_SINGLE(name " NG EPOUT Switch",    base, 4, 1, 0), \
	SOC_SINGLE(name " NG SPKOUTL Switch",  base, 6, 1, 0), \
	SOC_SINGLE(name " NG SPKOUTR Switch",  base, 7, 1, 0), \
	SOC_SINGLE(name " NG SPKDAT1L Switch", base, 8, 1, 0), \
	SOC_SINGLE(name " NG SPKDAT1R Switch", base, 9, 1, 0)

static const struct snd_kcontrol_new wm5102_snd_controls[] = {
SOC_SINGLE("IN1 High Performance Switch", ARIZONA_IN1L_CONTROL,
	   ARIZONA_IN1_OSR_SHIFT, 1, 0),
SOC_SINGLE("IN2 High Performance Switch", ARIZONA_IN2L_CONTROL,
	   ARIZONA_IN2_OSR_SHIFT, 1, 0),
SOC_SINGLE("IN3 High Performance Switch", ARIZONA_IN3L_CONTROL,
	   ARIZONA_IN3_OSR_SHIFT, 1, 0),

SOC_SINGLE_RANGE_TLV("IN1L Volume", ARIZONA_IN1L_CONTROL,
		     ARIZONA_IN1L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN1R Volume", ARIZONA_IN1R_CONTROL,
		     ARIZONA_IN1R_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN2L Volume", ARIZONA_IN2L_CONTROL,
		     ARIZONA_IN2L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN2R Volume", ARIZONA_IN2R_CONTROL,
		     ARIZONA_IN2R_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN3L Volume", ARIZONA_IN3L_CONTROL,
		     ARIZONA_IN3L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN3R Volume", ARIZONA_IN3R_CONTROL,
		     ARIZONA_IN3R_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),

SOC_SINGLE_TLV("IN1L Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_1L,
	       ARIZONA_IN1L_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN1R Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_1R,
	       ARIZONA_IN1R_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN2L Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_2L,
	       ARIZONA_IN2L_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN2R Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_2R,
	       ARIZONA_IN2R_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN3L Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_3L,
	       ARIZONA_IN3L_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN3R Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_3R,
	       ARIZONA_IN3R_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),

SOC_ENUM("Input Ramp Up", arizona_in_vi_ramp),
SOC_ENUM("Input Ramp Down", arizona_in_vd_ramp),

ARIZONA_MIXER_CONTROLS("EQ1", ARIZONA_EQ1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("EQ2", ARIZONA_EQ2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("EQ3", ARIZONA_EQ3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("EQ4", ARIZONA_EQ4MIX_INPUT_1_SOURCE),

ARIZONA_EQ_CONTROL("EQ1 Coefficients", ARIZONA_EQ1_2),
SOC_SINGLE_TLV("EQ1 B1 Volume", ARIZONA_EQ1_1, ARIZONA_EQ1_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B2 Volume", ARIZONA_EQ1_1, ARIZONA_EQ1_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B3 Volume", ARIZONA_EQ1_1, ARIZONA_EQ1_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B4 Volume", ARIZONA_EQ1_2, ARIZONA_EQ1_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B5 Volume", ARIZONA_EQ1_2, ARIZONA_EQ1_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

ARIZONA_EQ_CONTROL("EQ2 Coefficients", ARIZONA_EQ2_2),
SOC_SINGLE_TLV("EQ2 B1 Volume", ARIZONA_EQ2_1, ARIZONA_EQ2_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B2 Volume", ARIZONA_EQ2_1, ARIZONA_EQ2_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B3 Volume", ARIZONA_EQ2_1, ARIZONA_EQ2_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B4 Volume", ARIZONA_EQ2_2, ARIZONA_EQ2_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B5 Volume", ARIZONA_EQ2_2, ARIZONA_EQ2_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

ARIZONA_EQ_CONTROL("EQ3 Coefficients", ARIZONA_EQ3_2),
SOC_SINGLE_TLV("EQ3 B1 Volume", ARIZONA_EQ3_1, ARIZONA_EQ3_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ3 B2 Volume", ARIZONA_EQ3_1, ARIZONA_EQ3_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ3 B3 Volume", ARIZONA_EQ3_1, ARIZONA_EQ3_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ3 B4 Volume", ARIZONA_EQ3_2, ARIZONA_EQ3_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ3 B5 Volume", ARIZONA_EQ3_2, ARIZONA_EQ3_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

ARIZONA_EQ_CONTROL("EQ4 Coefficients", ARIZONA_EQ4_2),
SOC_SINGLE_TLV("EQ4 B1 Volume", ARIZONA_EQ4_1, ARIZONA_EQ4_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ4 B2 Volume", ARIZONA_EQ4_1, ARIZONA_EQ4_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ4 B3 Volume", ARIZONA_EQ4_1, ARIZONA_EQ4_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ4 B4 Volume", ARIZONA_EQ4_2, ARIZONA_EQ4_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ4 B5 Volume", ARIZONA_EQ4_2, ARIZONA_EQ4_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

ARIZONA_MIXER_CONTROLS("DRC1L", ARIZONA_DRC1LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DRC1R", ARIZONA_DRC1RMIX_INPUT_1_SOURCE),

SND_SOC_BYTES_MASK("DRC1", ARIZONA_DRC1_CTRL1, 5,
		   ARIZONA_DRC1R_ENA | ARIZONA_DRC1L_ENA),

ARIZONA_MIXER_CONTROLS("LHPF1", ARIZONA_HPLP1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("LHPF2", ARIZONA_HPLP2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("LHPF3", ARIZONA_HPLP3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("LHPF4", ARIZONA_HPLP4MIX_INPUT_1_SOURCE),

SND_SOC_BYTES("LHPF1 Coefficients", ARIZONA_HPLPF1_2, 1),
SND_SOC_BYTES("LHPF2 Coefficients", ARIZONA_HPLPF2_2, 1),
SND_SOC_BYTES("LHPF3 Coefficients", ARIZONA_HPLPF3_2, 1),
SND_SOC_BYTES("LHPF4 Coefficients", ARIZONA_HPLPF4_2, 1),

ARIZONA_MIXER_CONTROLS("DSP1L", ARIZONA_DSP1LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP1R", ARIZONA_DSP1RMIX_INPUT_1_SOURCE),

SOC_ENUM("LHPF1 Mode", arizona_lhpf1_mode),
SOC_ENUM("LHPF2 Mode", arizona_lhpf2_mode),
SOC_ENUM("LHPF3 Mode", arizona_lhpf3_mode),
SOC_ENUM("LHPF4 Mode", arizona_lhpf4_mode),

ARIZONA_SAMPLE_RATE_CONTROL_DVFS("Sample Rate 2", 2),
ARIZONA_SAMPLE_RATE_CONTROL_DVFS("Sample Rate 3", 3),

SOC_ENUM("ISRC1 FSL", arizona_isrc_fsl[0]),
SOC_ENUM("ISRC2 FSL", arizona_isrc_fsl[1]),
SOC_ENUM("ASRC RATE 1", arizona_asrc_rate1),
SOC_ENUM("ASRC RATE 2", arizona_asrc_rate2),

SOC_ENUM("FX Rate", arizona_fx_rate),

ARIZONA_MIXER_CONTROLS("Mic", ARIZONA_MICMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("Noise", ARIZONA_NOISEMIX_INPUT_1_SOURCE),

SOC_SINGLE_TLV("Noise Generator Volume", ARIZONA_COMFORT_NOISE_GENERATOR,
	       ARIZONA_NOISE_GEN_GAIN_SHIFT, 0x16, 0, noise_tlv),

ARIZONA_MIXER_CONTROLS("HPOUT1L", ARIZONA_OUT1LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("HPOUT1R", ARIZONA_OUT1RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("HPOUT2L", ARIZONA_OUT2LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("HPOUT2R", ARIZONA_OUT2RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("EPOUT", ARIZONA_OUT3LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKOUTL", ARIZONA_OUT4LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKOUTR", ARIZONA_OUT4RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKDAT1L", ARIZONA_OUT5LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKDAT1R", ARIZONA_OUT5RMIX_INPUT_1_SOURCE),

SOC_SINGLE("Speaker High Performance Switch", ARIZONA_OUTPUT_PATH_CONFIG_4L,
	   ARIZONA_OUT4_OSR_SHIFT, 1, 0),
SOC_SINGLE("SPKDAT1 High Performance Switch", ARIZONA_OUTPUT_PATH_CONFIG_5L,
	   ARIZONA_OUT5_OSR_SHIFT, 1, 0),

SOC_DOUBLE_R("HPOUT1 Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_1L,
	     ARIZONA_DAC_DIGITAL_VOLUME_1R, ARIZONA_OUT1L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("HPOUT2 Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_2L,
	     ARIZONA_DAC_DIGITAL_VOLUME_2R, ARIZONA_OUT2L_MUTE_SHIFT, 1, 1),
SOC_SINGLE("EPOUT Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_3L,
	   ARIZONA_OUT3L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("Speaker Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_4L,
	     ARIZONA_DAC_DIGITAL_VOLUME_4R, ARIZONA_OUT4L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("SPKDAT1 Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_5L,
	     ARIZONA_DAC_DIGITAL_VOLUME_5R, ARIZONA_OUT5L_MUTE_SHIFT, 1, 1),

SOC_DOUBLE_R_TLV("HPOUT1 Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_1L,
		 ARIZONA_DAC_DIGITAL_VOLUME_1R, ARIZONA_OUT1L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("HPOUT2 Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_2L,
		 ARIZONA_DAC_DIGITAL_VOLUME_2R, ARIZONA_OUT2L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("EPOUT Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_3L,
	       ARIZONA_OUT3L_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("Speaker Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_4L,
		 ARIZONA_DAC_DIGITAL_VOLUME_4R, ARIZONA_OUT4L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("SPKDAT1 Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_5L,
		 ARIZONA_DAC_DIGITAL_VOLUME_5R, ARIZONA_OUT5L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),

SOC_ENUM("HPOUT1 OSR", wm5102_hpout_osr[0]),
SOC_ENUM("HPOUT2 OSR", wm5102_hpout_osr[1]),
SOC_ENUM("EPOUT OSR", wm5102_hpout_osr[2]),

SOC_DOUBLE("HPOUT1 DRE Switch", ARIZONA_DRE_ENABLE,
	   ARIZONA_DRE1L_ENA_SHIFT, ARIZONA_DRE1R_ENA_SHIFT, 1, 0),
SOC_DOUBLE("HPOUT2 DRE Switch", ARIZONA_DRE_ENABLE,
	   ARIZONA_DRE2L_ENA_SHIFT, ARIZONA_DRE2R_ENA_SHIFT, 1, 0),
SOC_SINGLE("EPOUT DRE Switch", ARIZONA_DRE_ENABLE,
	   ARIZONA_DRE3L_ENA_SHIFT, 1, 0),

SOC_SINGLE("DRE Threshold", ARIZONA_DRE_CONTROL_2,
	   ARIZONA_DRE_T_LOW_SHIFT, 63, 0),

SOC_SINGLE("DRE Low Level ABS", ARIZONA_DRE_CONTROL_3,
	   ARIZONA_DRE_LOW_LEVEL_ABS_SHIFT, 15, 0),

SOC_ENUM("Output Ramp Up", arizona_out_vi_ramp),
SOC_ENUM("Output Ramp Down", arizona_out_vd_ramp),

SOC_DOUBLE("SPKDAT1 Switch", ARIZONA_PDM_SPK1_CTRL_1, ARIZONA_SPK1L_MUTE_SHIFT,
	   ARIZONA_SPK1R_MUTE_SHIFT, 1, 1),

SOC_SINGLE("Noise Gate Switch", ARIZONA_NOISE_GATE_CONTROL,
	   ARIZONA_NGATE_ENA_SHIFT, 1, 0),
SOC_SINGLE_TLV("Noise Gate Threshold Volume", ARIZONA_NOISE_GATE_CONTROL,
	       ARIZONA_NGATE_THR_SHIFT, 7, 1, ng_tlv),
SOC_ENUM("Noise Gate Hold", arizona_ng_hold),

SND_SOC_BYTES_EXT("Output Compensation Coefficient", 2,
		  wm5102_out_comp_coeff_get, wm5102_out_comp_coeff_put),

SOC_SINGLE_EXT("Output Compensation Switch", 0, 0, 1, 0,
	       wm5102_out_comp_switch_get, wm5102_out_comp_switch_put),

SOC_ENUM("Output Rate 1", arizona_output_rate),
SOC_ENUM("In Rate", arizona_input_rate),

WM5102_NG_SRC("HPOUT1L", ARIZONA_NOISE_GATE_SELECT_1L),
WM5102_NG_SRC("HPOUT1R", ARIZONA_NOISE_GATE_SELECT_1R),
WM5102_NG_SRC("HPOUT2L", ARIZONA_NOISE_GATE_SELECT_2L),
WM5102_NG_SRC("HPOUT2R", ARIZONA_NOISE_GATE_SELECT_2R),
WM5102_NG_SRC("EPOUT", ARIZONA_NOISE_GATE_SELECT_3L),
WM5102_NG_SRC("SPKOUTL", ARIZONA_NOISE_GATE_SELECT_4L),
WM5102_NG_SRC("SPKOUTR", ARIZONA_NOISE_GATE_SELECT_4R),
WM5102_NG_SRC("SPKDAT1L", ARIZONA_NOISE_GATE_SELECT_5L),
WM5102_NG_SRC("SPKDAT1R", ARIZONA_NOISE_GATE_SELECT_5R),

ARIZONA_MIXER_CONTROLS("AIF1TX1", ARIZONA_AIF1TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX2", ARIZONA_AIF1TX2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX3", ARIZONA_AIF1TX3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX4", ARIZONA_AIF1TX4MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX5", ARIZONA_AIF1TX5MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX6", ARIZONA_AIF1TX6MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX7", ARIZONA_AIF1TX7MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX8", ARIZONA_AIF1TX8MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("AIF2TX1", ARIZONA_AIF2TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF2TX2", ARIZONA_AIF2TX2MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("AIF3TX1", ARIZONA_AIF3TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF3TX2", ARIZONA_AIF3TX2MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("SLIMTX1", ARIZONA_SLIMTX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX2", ARIZONA_SLIMTX2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX3", ARIZONA_SLIMTX3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX4", ARIZONA_SLIMTX4MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX5", ARIZONA_SLIMTX5MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX6", ARIZONA_SLIMTX6MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX7", ARIZONA_SLIMTX7MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX8", ARIZONA_SLIMTX8MIX_INPUT_1_SOURCE),
};

ARIZONA_MIXER_ENUMS(EQ1, ARIZONA_EQ1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(EQ2, ARIZONA_EQ2MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(EQ3, ARIZONA_EQ3MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(EQ4, ARIZONA_EQ4MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(DRC1L, ARIZONA_DRC1LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(DRC1R, ARIZONA_DRC1RMIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(LHPF1, ARIZONA_HPLP1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(LHPF2, ARIZONA_HPLP2MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(LHPF3, ARIZONA_HPLP3MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(LHPF4, ARIZONA_HPLP4MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(Mic, ARIZONA_MICMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(Noise, ARIZONA_NOISEMIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(PWM1, ARIZONA_PWM1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(PWM2, ARIZONA_PWM2MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(OUT1L, ARIZONA_OUT1LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(OUT1R, ARIZONA_OUT1RMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(OUT2L, ARIZONA_OUT2LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(OUT2R, ARIZONA_OUT2RMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(OUT3, ARIZONA_OUT3LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SPKOUTL, ARIZONA_OUT4LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SPKOUTR, ARIZONA_OUT4RMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SPKDAT1L, ARIZONA_OUT5LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SPKDAT1R, ARIZONA_OUT5RMIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(AIF1TX1, ARIZONA_AIF1TX1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX2, ARIZONA_AIF1TX2MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX3, ARIZONA_AIF1TX3MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX4, ARIZONA_AIF1TX4MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX5, ARIZONA_AIF1TX5MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX6, ARIZONA_AIF1TX6MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX7, ARIZONA_AIF1TX7MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX8, ARIZONA_AIF1TX8MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(AIF2TX1, ARIZONA_AIF2TX1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF2TX2, ARIZONA_AIF2TX2MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(AIF3TX1, ARIZONA_AIF3TX1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF3TX2, ARIZONA_AIF3TX2MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(SLIMTX1, ARIZONA_SLIMTX1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SLIMTX2, ARIZONA_SLIMTX2MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SLIMTX3, ARIZONA_SLIMTX3MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SLIMTX4, ARIZONA_SLIMTX4MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SLIMTX5, ARIZONA_SLIMTX5MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SLIMTX6, ARIZONA_SLIMTX6MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SLIMTX7, ARIZONA_SLIMTX7MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SLIMTX8, ARIZONA_SLIMTX8MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ASRC1L, ARIZONA_ASRC1LMIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ASRC1R, ARIZONA_ASRC1RMIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ASRC2L, ARIZONA_ASRC2LMIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ASRC2R, ARIZONA_ASRC2RMIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ISRC1INT1, ARIZONA_ISRC1INT1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1INT2, ARIZONA_ISRC1INT2MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ISRC1DEC1, ARIZONA_ISRC1DEC1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1DEC2, ARIZONA_ISRC1DEC2MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ISRC2INT1, ARIZONA_ISRC2INT1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC2INT2, ARIZONA_ISRC2INT2MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ISRC2DEC1, ARIZONA_ISRC2DEC1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC2DEC2, ARIZONA_ISRC2DEC2MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(DSP1L, ARIZONA_DSP1LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(DSP1R, ARIZONA_DSP1RMIX_INPUT_1_SOURCE);

ARIZONA_DSP_AUX_ENUMS(DSP1, ARIZONA_DSP1AUX1MIX_INPUT_1_SOURCE);

static const char * const wm5102_dsp_output_texts[] = {
	"None",
	"DSP1",
};

static const SOC_ENUM_SINGLE_DECL(wm5102_dsp_output_enum, 0, 0, wm5102_dsp_output_texts);

static const struct snd_kcontrol_new wm5102_dsp_output_mux =
	SOC_DAPM_ENUM("DSP Virtual Output Mux", wm5102_dsp_output_enum);

static const char * const wm5102_aec_loopback_texts[] = {
	"HPOUT1L", "HPOUT1R", "HPOUT2L", "HPOUT2R", "EPOUT",
	"SPKOUTL", "SPKOUTR", "SPKDAT1L", "SPKDAT1R",
};

static const unsigned int wm5102_aec_loopback_values[] = {
	0, 1, 2, 3, 4, 6, 7, 8, 9,
};

static const struct soc_enum wm5102_aec_loopback =
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DAC_AEC_CONTROL_1,
			      ARIZONA_AEC_LOOPBACK_SRC_SHIFT, 0xf,
			      ARRAY_SIZE(wm5102_aec_loopback_texts),
			      wm5102_aec_loopback_texts,
			      wm5102_aec_loopback_values);

static const struct snd_kcontrol_new wm5102_aec_loopback_mux =
	SOC_DAPM_ENUM("AEC Loopback", wm5102_aec_loopback);

static const struct snd_soc_dapm_widget wm5102_dapm_widgets[] = {
SND_SOC_DAPM_SUPPLY("SYSCLK", ARIZONA_SYSTEM_CLOCK_1, ARIZONA_SYSCLK_ENA_SHIFT,
		    0, wm5102_sysclk_ev, SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_SUPPLY("ASYNCCLK", ARIZONA_ASYNC_CLOCK_1,
		    ARIZONA_ASYNC_CLK_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("OPCLK", ARIZONA_OUTPUT_SYSTEM_CLOCK,
		    ARIZONA_OPCLK_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("ASYNCOPCLK", ARIZONA_OUTPUT_ASYNC_CLOCK,
		    ARIZONA_OPCLK_ASYNC_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_REGULATOR_SUPPLY("DBVDD2", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("DBVDD3", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("CPVDD", 20, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("MICVDD", 0, SND_SOC_DAPM_REGULATOR_BYPASS),
SND_SOC_DAPM_REGULATOR_SUPPLY("SPKVDDL", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("SPKVDDR", 0, 0),

SND_SOC_DAPM_SIGGEN("TONE"),
SND_SOC_DAPM_SIGGEN("NOISE"),
SND_SOC_DAPM_SIGGEN("HAPTICS"),

SND_SOC_DAPM_INPUT("IN1L"),
SND_SOC_DAPM_INPUT("IN1R"),
SND_SOC_DAPM_INPUT("IN2L"),
SND_SOC_DAPM_INPUT("IN2R"),
SND_SOC_DAPM_INPUT("IN3L"),
SND_SOC_DAPM_INPUT("IN3R"),

SND_SOC_DAPM_OUTPUT("DRC1 Signal Activity"),

SND_SOC_DAPM_PGA_E("IN1L PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN1L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN1R PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN1R_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN2L PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN2L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN2R PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN2R_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN3L PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN3L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN3R PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN3R_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_SUPPLY("MICBIAS1", ARIZONA_MIC_BIAS_CTRL_1,
		    ARIZONA_MICB1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2", ARIZONA_MIC_BIAS_CTRL_2,
		    ARIZONA_MICB2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS3", ARIZONA_MIC_BIAS_CTRL_3,
		    ARIZONA_MICB3_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("Noise Generator", ARIZONA_COMFORT_NOISE_GENERATOR,
		 ARIZONA_NOISE_GEN_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("Tone Generator 1", ARIZONA_TONE_GENERATOR_1,
		 ARIZONA_TONE1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("Tone Generator 2", ARIZONA_TONE_GENERATOR_1,
		 ARIZONA_TONE2_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("Mic Mute Mixer", ARIZONA_MIC_NOISE_MIX_CONTROL_1,
		 ARIZONA_MICMUTE_MIX_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("EQ1", ARIZONA_EQ1_1, ARIZONA_EQ1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ2", ARIZONA_EQ2_1, ARIZONA_EQ2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ3", ARIZONA_EQ3_1, ARIZONA_EQ3_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ4", ARIZONA_EQ4_1, ARIZONA_EQ4_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("DRC1L", ARIZONA_DRC1_CTRL1, ARIZONA_DRC1L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("DRC1R", ARIZONA_DRC1_CTRL1, ARIZONA_DRC1R_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("LHPF1", ARIZONA_HPLPF1_1, ARIZONA_LHPF1_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF2", ARIZONA_HPLPF2_1, ARIZONA_LHPF2_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF3", ARIZONA_HPLPF3_1, ARIZONA_LHPF3_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF4", ARIZONA_HPLPF4_1, ARIZONA_LHPF4_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("PWM1 Driver", ARIZONA_PWM_DRIVE_1, ARIZONA_PWM1_ENA_SHIFT,
		 0, NULL, 0),
SND_SOC_DAPM_PGA("PWM2 Driver", ARIZONA_PWM_DRIVE_1, ARIZONA_PWM2_ENA_SHIFT,
		 0, NULL, 0),

SND_SOC_DAPM_PGA("ASRC1L", ARIZONA_ASRC_ENABLE, ARIZONA_ASRC1L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("ASRC1R", ARIZONA_ASRC_ENABLE, ARIZONA_ASRC1R_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("ASRC2L", ARIZONA_ASRC_ENABLE, ARIZONA_ASRC2L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("ASRC2R", ARIZONA_ASRC_ENABLE, ARIZONA_ASRC2R_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("ISRC1INT1", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT2", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC1DEC1", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC2", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC2INT1", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_INT0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2INT2", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_INT1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC2DEC1", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_DEC0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2DEC2", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_DEC1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_AIF_OUT("AIF1TX1", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX2", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX3", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX4", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX5", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX6", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX7", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX8", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF1RX1", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX2", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX3", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX4", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX5", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX6", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX7", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX8", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("AIF2TX1", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX2", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF2RX1", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX2", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("AIF3TX1", NULL, 0,
		     ARIZONA_AIF3_TX_ENABLES, ARIZONA_AIF3TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF3TX2", NULL, 0,
		     ARIZONA_AIF3_TX_ENABLES, ARIZONA_AIF3TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF3RX1", NULL, 0,
		    ARIZONA_AIF3_RX_ENABLES, ARIZONA_AIF3RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF3RX2", NULL, 0,
		    ARIZONA_AIF3_RX_ENABLES, ARIZONA_AIF3RX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("SLIMTX1", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX2", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX3", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX4", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX5", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX6", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX7", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX8", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("SLIMRX1", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX2", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX3", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX4", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX5", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX6", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX7", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX8", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX8_ENA_SHIFT, 0),

ARIZONA_DSP_WIDGETS(DSP1, "DSP1"),

SND_SOC_DAPM_MUX("AEC Loopback", ARIZONA_DAC_AEC_CONTROL_1,
		  ARIZONA_AEC_LOOPBACK_ENA_SHIFT, 0,
		  &wm5102_aec_loopback_mux),

SND_SOC_DAPM_MUX("DSP Virtual Output Mux", SND_SOC_NOPM, 0, 0,
		   &wm5102_dsp_output_mux),

SND_SOC_DAPM_PGA_E("OUT1L", SND_SOC_NOPM,
		   ARIZONA_OUT1L_ENA_SHIFT, 0, NULL, 0, arizona_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT1R", SND_SOC_NOPM,
		   ARIZONA_OUT1R_ENA_SHIFT, 0, NULL, 0, arizona_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT2L", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT2L_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT2R", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT2R_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT3L", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT3L_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT5L", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT5L_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT5R", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT5R_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

ARIZONA_MIXER_WIDGETS(EQ1, "EQ1"),
ARIZONA_MIXER_WIDGETS(EQ2, "EQ2"),
ARIZONA_MIXER_WIDGETS(EQ3, "EQ3"),
ARIZONA_MIXER_WIDGETS(EQ4, "EQ4"),

ARIZONA_MIXER_WIDGETS(DRC1L, "DRC1L"),
ARIZONA_MIXER_WIDGETS(DRC1R, "DRC1R"),

ARIZONA_MIXER_WIDGETS(LHPF1, "LHPF1"),
ARIZONA_MIXER_WIDGETS(LHPF2, "LHPF2"),
ARIZONA_MIXER_WIDGETS(LHPF3, "LHPF3"),
ARIZONA_MIXER_WIDGETS(LHPF4, "LHPF4"),

ARIZONA_MIXER_WIDGETS(Mic, "Mic"),
ARIZONA_MIXER_WIDGETS(Noise, "Noise"),

ARIZONA_MIXER_WIDGETS(PWM1, "PWM1"),
ARIZONA_MIXER_WIDGETS(PWM2, "PWM2"),

ARIZONA_MIXER_WIDGETS(OUT1L, "HPOUT1L"),
ARIZONA_MIXER_WIDGETS(OUT1R, "HPOUT1R"),
ARIZONA_MIXER_WIDGETS(OUT2L, "HPOUT2L"),
ARIZONA_MIXER_WIDGETS(OUT2R, "HPOUT2R"),
ARIZONA_MIXER_WIDGETS(OUT3, "EPOUT"),
ARIZONA_MIXER_WIDGETS(SPKOUTL, "SPKOUTL"),
ARIZONA_MIXER_WIDGETS(SPKOUTR, "SPKOUTR"),
ARIZONA_MIXER_WIDGETS(SPKDAT1L, "SPKDAT1L"),
ARIZONA_MIXER_WIDGETS(SPKDAT1R, "SPKDAT1R"),

ARIZONA_MIXER_WIDGETS(AIF1TX1, "AIF1TX1"),
ARIZONA_MIXER_WIDGETS(AIF1TX2, "AIF1TX2"),
ARIZONA_MIXER_WIDGETS(AIF1TX3, "AIF1TX3"),
ARIZONA_MIXER_WIDGETS(AIF1TX4, "AIF1TX4"),
ARIZONA_MIXER_WIDGETS(AIF1TX5, "AIF1TX5"),
ARIZONA_MIXER_WIDGETS(AIF1TX6, "AIF1TX6"),
ARIZONA_MIXER_WIDGETS(AIF1TX7, "AIF1TX7"),
ARIZONA_MIXER_WIDGETS(AIF1TX8, "AIF1TX8"),

ARIZONA_MIXER_WIDGETS(AIF2TX1, "AIF2TX1"),
ARIZONA_MIXER_WIDGETS(AIF2TX2, "AIF2TX2"),

ARIZONA_MIXER_WIDGETS(AIF3TX1, "AIF3TX1"),
ARIZONA_MIXER_WIDGETS(AIF3TX2, "AIF3TX2"),

ARIZONA_MIXER_WIDGETS(SLIMTX1, "SLIMTX1"),
ARIZONA_MIXER_WIDGETS(SLIMTX2, "SLIMTX2"),
ARIZONA_MIXER_WIDGETS(SLIMTX3, "SLIMTX3"),
ARIZONA_MIXER_WIDGETS(SLIMTX4, "SLIMTX4"),
ARIZONA_MIXER_WIDGETS(SLIMTX5, "SLIMTX5"),
ARIZONA_MIXER_WIDGETS(SLIMTX6, "SLIMTX6"),
ARIZONA_MIXER_WIDGETS(SLIMTX7, "SLIMTX7"),
ARIZONA_MIXER_WIDGETS(SLIMTX8, "SLIMTX8"),

ARIZONA_MUX_WIDGETS(ASRC1L, "ASRC1L"),
ARIZONA_MUX_WIDGETS(ASRC1R, "ASRC1R"),
ARIZONA_MUX_WIDGETS(ASRC2L, "ASRC2L"),
ARIZONA_MUX_WIDGETS(ASRC2R, "ASRC2R"),

ARIZONA_MUX_WIDGETS(ISRC1DEC1, "ISRC1DEC1"),
ARIZONA_MUX_WIDGETS(ISRC1DEC2, "ISRC1DEC2"),

ARIZONA_MUX_WIDGETS(ISRC1INT1, "ISRC1INT1"),
ARIZONA_MUX_WIDGETS(ISRC1INT2, "ISRC1INT2"),

ARIZONA_MUX_WIDGETS(ISRC2DEC1, "ISRC2DEC1"),
ARIZONA_MUX_WIDGETS(ISRC2DEC2, "ISRC2DEC2"),

ARIZONA_MUX_WIDGETS(ISRC2INT1, "ISRC2INT1"),
ARIZONA_MUX_WIDGETS(ISRC2INT2, "ISRC2INT2"),

WM_ADSP2("DSP1", 0, wm5102_adsp_power_ev),

SND_SOC_DAPM_OUTPUT("DSP Virtual Output"),

SND_SOC_DAPM_OUTPUT("HPOUT1L"),
SND_SOC_DAPM_OUTPUT("HPOUT1R"),
SND_SOC_DAPM_OUTPUT("HPOUT2L"),
SND_SOC_DAPM_OUTPUT("HPOUT2R"),
SND_SOC_DAPM_OUTPUT("EPOUTN"),
SND_SOC_DAPM_OUTPUT("EPOUTP"),
SND_SOC_DAPM_OUTPUT("SPKOUTLN"),
SND_SOC_DAPM_OUTPUT("SPKOUTLP"),
SND_SOC_DAPM_OUTPUT("SPKOUTRN"),
SND_SOC_DAPM_OUTPUT("SPKOUTRP"),
SND_SOC_DAPM_OUTPUT("SPKDAT1L"),
SND_SOC_DAPM_OUTPUT("SPKDAT1R"),

SND_SOC_DAPM_OUTPUT("MICSUPP"),
};

#define ARIZONA_MIXER_INPUT_ROUTES(name)	\
	{ name, "Noise Generator", "Noise Generator" }, \
	{ name, "Tone Generator 1", "Tone Generator 1" }, \
	{ name, "Tone Generator 2", "Tone Generator 2" }, \
	{ name, "Haptics", "HAPTICS" }, \
	{ name, "AEC", "AEC Loopback" }, \
	{ name, "IN1L", "IN1L PGA" }, \
	{ name, "IN1R", "IN1R PGA" }, \
	{ name, "IN2L", "IN2L PGA" }, \
	{ name, "IN2R", "IN2R PGA" }, \
	{ name, "IN3L", "IN3L PGA" }, \
	{ name, "IN3R", "IN3R PGA" }, \
	{ name, "Mic Mute Mixer", "Mic Mute Mixer" }, \
	{ name, "AIF1RX1", "AIF1RX1" }, \
	{ name, "AIF1RX2", "AIF1RX2" }, \
	{ name, "AIF1RX3", "AIF1RX3" }, \
	{ name, "AIF1RX4", "AIF1RX4" }, \
	{ name, "AIF1RX5", "AIF1RX5" }, \
	{ name, "AIF1RX6", "AIF1RX6" }, \
	{ name, "AIF1RX7", "AIF1RX7" }, \
	{ name, "AIF1RX8", "AIF1RX8" }, \
	{ name, "AIF2RX1", "AIF2RX1" }, \
	{ name, "AIF2RX2", "AIF2RX2" }, \
	{ name, "AIF3RX1", "AIF3RX1" }, \
	{ name, "AIF3RX2", "AIF3RX2" }, \
	{ name, "SLIMRX1", "SLIMRX1" }, \
	{ name, "SLIMRX2", "SLIMRX2" }, \
	{ name, "SLIMRX3", "SLIMRX3" }, \
	{ name, "SLIMRX4", "SLIMRX4" }, \
	{ name, "SLIMRX5", "SLIMRX5" }, \
	{ name, "SLIMRX6", "SLIMRX6" }, \
	{ name, "SLIMRX7", "SLIMRX7" }, \
	{ name, "SLIMRX8", "SLIMRX8" }, \
	{ name, "EQ1", "EQ1" }, \
	{ name, "EQ2", "EQ2" }, \
	{ name, "EQ3", "EQ3" }, \
	{ name, "EQ4", "EQ4" }, \
	{ name, "DRC1L", "DRC1L" }, \
	{ name, "DRC1R", "DRC1R" }, \
	{ name, "LHPF1", "LHPF1" }, \
	{ name, "LHPF2", "LHPF2" }, \
	{ name, "LHPF3", "LHPF3" }, \
	{ name, "LHPF4", "LHPF4" }, \
	{ name, "ASRC1L", "ASRC1L" }, \
	{ name, "ASRC1R", "ASRC1R" }, \
	{ name, "ASRC2L", "ASRC2L" }, \
	{ name, "ASRC2R", "ASRC2R" }, \
	{ name, "ISRC1DEC1", "ISRC1DEC1" }, \
	{ name, "ISRC1DEC2", "ISRC1DEC2" }, \
	{ name, "ISRC1INT1", "ISRC1INT1" }, \
	{ name, "ISRC1INT2", "ISRC1INT2" }, \
	{ name, "ISRC2DEC1", "ISRC2DEC1" }, \
	{ name, "ISRC2DEC2", "ISRC2DEC2" }, \
	{ name, "ISRC2INT1", "ISRC2INT1" }, \
	{ name, "ISRC2INT2", "ISRC2INT2" }, \
	{ name, "DSP1.1", "DSP1" }, \
	{ name, "DSP1.2", "DSP1" }, \
	{ name, "DSP1.3", "DSP1" }, \
	{ name, "DSP1.4", "DSP1" }, \
	{ name, "DSP1.5", "DSP1" }, \
	{ name, "DSP1.6", "DSP1" }

static const struct snd_soc_dapm_route wm5102_dapm_routes[] = {
	{ "AIF2 Capture", NULL, "DBVDD2" },
	{ "AIF2 Playback", NULL, "DBVDD2" },

	{ "AIF3 Capture", NULL, "DBVDD3" },
	{ "AIF3 Playback", NULL, "DBVDD3" },

	{ "OUT1L", NULL, "CPVDD" },
	{ "OUT1R", NULL, "CPVDD" },
	{ "OUT2L", NULL, "CPVDD" },
	{ "OUT2R", NULL, "CPVDD" },
	{ "OUT3L", NULL, "CPVDD" },

	{ "OUT4L", NULL, "SPKVDDL" },
	{ "OUT4R", NULL, "SPKVDDR" },

	{ "OUT1L", NULL, "SYSCLK" },
	{ "OUT1R", NULL, "SYSCLK" },
	{ "OUT2L", NULL, "SYSCLK" },
	{ "OUT2R", NULL, "SYSCLK" },
	{ "OUT3L", NULL, "SYSCLK" },
	{ "OUT4L", NULL, "SYSCLK" },
	{ "OUT4R", NULL, "SYSCLK" },
	{ "OUT5L", NULL, "SYSCLK" },
	{ "OUT5R", NULL, "SYSCLK" },

	{ "IN1L", NULL, "SYSCLK" },
	{ "IN1R", NULL, "SYSCLK" },
	{ "IN2L", NULL, "SYSCLK" },
	{ "IN2R", NULL, "SYSCLK" },
	{ "IN3L", NULL, "SYSCLK" },
	{ "IN3R", NULL, "SYSCLK" },

	{ "MICBIAS1", NULL, "MICVDD" },
	{ "MICBIAS2", NULL, "MICVDD" },
	{ "MICBIAS3", NULL, "MICVDD" },

	{ "Noise Generator", NULL, "SYSCLK" },
	{ "Tone Generator 1", NULL, "SYSCLK" },
	{ "Tone Generator 2", NULL, "SYSCLK" },

	{ "Noise Generator", NULL, "NOISE" },
	{ "Tone Generator 1", NULL, "TONE" },
	{ "Tone Generator 2", NULL, "TONE" },

	{ "AIF1 Capture", NULL, "AIF1TX1" },
	{ "AIF1 Capture", NULL, "AIF1TX2" },
	{ "AIF1 Capture", NULL, "AIF1TX3" },
	{ "AIF1 Capture", NULL, "AIF1TX4" },
	{ "AIF1 Capture", NULL, "AIF1TX5" },
	{ "AIF1 Capture", NULL, "AIF1TX6" },
	{ "AIF1 Capture", NULL, "AIF1TX7" },
	{ "AIF1 Capture", NULL, "AIF1TX8" },

	{ "AIF1RX1", NULL, "AIF1 Playback" },
	{ "AIF1RX2", NULL, "AIF1 Playback" },
	{ "AIF1RX3", NULL, "AIF1 Playback" },
	{ "AIF1RX4", NULL, "AIF1 Playback" },
	{ "AIF1RX5", NULL, "AIF1 Playback" },
	{ "AIF1RX6", NULL, "AIF1 Playback" },
	{ "AIF1RX7", NULL, "AIF1 Playback" },
	{ "AIF1RX8", NULL, "AIF1 Playback" },

	{ "AIF2 Capture", NULL, "AIF2TX1" },
	{ "AIF2 Capture", NULL, "AIF2TX2" },

	{ "AIF2RX1", NULL, "AIF2 Playback" },
	{ "AIF2RX2", NULL, "AIF2 Playback" },

	{ "AIF3 Capture", NULL, "AIF3TX1" },
	{ "AIF3 Capture", NULL, "AIF3TX2" },

	{ "AIF3RX1", NULL, "AIF3 Playback" },
	{ "AIF3RX2", NULL, "AIF3 Playback" },

	{ "Slim1 Capture", NULL, "SLIMTX1" },
	{ "Slim1 Capture", NULL, "SLIMTX2" },
	{ "Slim1 Capture", NULL, "SLIMTX3" },
	{ "Slim1 Capture", NULL, "SLIMTX4" },

	{ "SLIMRX1", NULL, "Slim1 Playback" },
	{ "SLIMRX2", NULL, "Slim1 Playback" },
	{ "SLIMRX3", NULL, "Slim1 Playback" },
	{ "SLIMRX4", NULL, "Slim1 Playback" },

	{ "Slim2 Capture", NULL, "SLIMTX5" },
	{ "Slim2 Capture", NULL, "SLIMTX6" },

	{ "SLIMRX5", NULL, "Slim2 Playback" },
	{ "SLIMRX6", NULL, "Slim2 Playback" },

	{ "Slim3 Capture", NULL, "SLIMTX7" },
	{ "Slim3 Capture", NULL, "SLIMTX8" },

	{ "SLIMRX7", NULL, "Slim3 Playback" },
	{ "SLIMRX8", NULL, "Slim3 Playback" },

	{ "AIF1 Playback", NULL, "SYSCLK" },
	{ "AIF2 Playback", NULL, "SYSCLK" },
	{ "AIF3 Playback", NULL, "SYSCLK" },
	{ "Slim1 Playback", NULL, "SYSCLK" },
	{ "Slim2 Playback", NULL, "SYSCLK" },
	{ "Slim3 Playback", NULL, "SYSCLK" },

	{ "AIF1 Capture", NULL, "SYSCLK" },
	{ "AIF2 Capture", NULL, "SYSCLK" },
	{ "AIF3 Capture", NULL, "SYSCLK" },
	{ "Slim1 Capture", NULL, "SYSCLK" },
	{ "Slim2 Capture", NULL, "SYSCLK" },
	{ "Slim3 Capture", NULL, "SYSCLK" },

	{ "IN1L PGA", NULL, "IN1L" },
	{ "IN1R PGA", NULL, "IN1R" },

	{ "IN2L PGA", NULL, "IN2L" },
	{ "IN2R PGA", NULL, "IN2R" },

	{ "IN3L PGA", NULL, "IN3L" },
	{ "IN3R PGA", NULL, "IN3R" },

	ARIZONA_MIXER_ROUTES("OUT1L", "HPOUT1L"),
	ARIZONA_MIXER_ROUTES("OUT1R", "HPOUT1R"),
	ARIZONA_MIXER_ROUTES("OUT2L", "HPOUT2L"),
	ARIZONA_MIXER_ROUTES("OUT2R", "HPOUT2R"),
	ARIZONA_MIXER_ROUTES("OUT3L", "EPOUT"),

	ARIZONA_MIXER_ROUTES("OUT4L", "SPKOUTL"),
	ARIZONA_MIXER_ROUTES("OUT4R", "SPKOUTR"),
	ARIZONA_MIXER_ROUTES("OUT5L", "SPKDAT1L"),
	ARIZONA_MIXER_ROUTES("OUT5R", "SPKDAT1R"),

	ARIZONA_MIXER_ROUTES("PWM1 Driver", "PWM1"),
	ARIZONA_MIXER_ROUTES("PWM2 Driver", "PWM2"),

	ARIZONA_MIXER_ROUTES("AIF1TX1", "AIF1TX1"),
	ARIZONA_MIXER_ROUTES("AIF1TX2", "AIF1TX2"),
	ARIZONA_MIXER_ROUTES("AIF1TX3", "AIF1TX3"),
	ARIZONA_MIXER_ROUTES("AIF1TX4", "AIF1TX4"),
	ARIZONA_MIXER_ROUTES("AIF1TX5", "AIF1TX5"),
	ARIZONA_MIXER_ROUTES("AIF1TX6", "AIF1TX6"),
	ARIZONA_MIXER_ROUTES("AIF1TX7", "AIF1TX7"),
	ARIZONA_MIXER_ROUTES("AIF1TX8", "AIF1TX8"),

	ARIZONA_MIXER_ROUTES("AIF2TX1", "AIF2TX1"),
	ARIZONA_MIXER_ROUTES("AIF2TX2", "AIF2TX2"),

	ARIZONA_MIXER_ROUTES("AIF3TX1", "AIF3TX1"),
	ARIZONA_MIXER_ROUTES("AIF3TX2", "AIF3TX2"),

	ARIZONA_MIXER_ROUTES("SLIMTX1", "SLIMTX1"),
	ARIZONA_MIXER_ROUTES("SLIMTX2", "SLIMTX2"),
	ARIZONA_MIXER_ROUTES("SLIMTX3", "SLIMTX3"),
	ARIZONA_MIXER_ROUTES("SLIMTX4", "SLIMTX4"),
	ARIZONA_MIXER_ROUTES("SLIMTX5", "SLIMTX5"),
	ARIZONA_MIXER_ROUTES("SLIMTX6", "SLIMTX6"),
	ARIZONA_MIXER_ROUTES("SLIMTX7", "SLIMTX7"),
	ARIZONA_MIXER_ROUTES("SLIMTX8", "SLIMTX8"),

	ARIZONA_MIXER_ROUTES("EQ1", "EQ1"),
	ARIZONA_MIXER_ROUTES("EQ2", "EQ2"),
	ARIZONA_MIXER_ROUTES("EQ3", "EQ3"),
	ARIZONA_MIXER_ROUTES("EQ4", "EQ4"),

	ARIZONA_MIXER_ROUTES("DRC1L", "DRC1L"),
	ARIZONA_MIXER_ROUTES("DRC1R", "DRC1R"),

	ARIZONA_MIXER_ROUTES("LHPF1", "LHPF1"),
	ARIZONA_MIXER_ROUTES("LHPF2", "LHPF2"),
	ARIZONA_MIXER_ROUTES("LHPF3", "LHPF3"),
	ARIZONA_MIXER_ROUTES("LHPF4", "LHPF4"),

	ARIZONA_MIXER_ROUTES("Mic Mute Mixer", "Noise"),
	ARIZONA_MIXER_ROUTES("Mic Mute Mixer", "Mic"),

	ARIZONA_MUX_ROUTES("ASRC1L", "ASRC1L"),
	ARIZONA_MUX_ROUTES("ASRC1R", "ASRC1R"),
	ARIZONA_MUX_ROUTES("ASRC2L", "ASRC2L"),
	ARIZONA_MUX_ROUTES("ASRC2R", "ASRC2R"),

	ARIZONA_MUX_ROUTES("ISRC1INT1", "ISRC1INT1"),
	ARIZONA_MUX_ROUTES("ISRC1INT2", "ISRC1INT2"),

	ARIZONA_MUX_ROUTES("ISRC1DEC1", "ISRC1DEC1"),
	ARIZONA_MUX_ROUTES("ISRC1DEC2", "ISRC1DEC2"),

	ARIZONA_MUX_ROUTES("ISRC2INT1", "ISRC2INT1"),
	ARIZONA_MUX_ROUTES("ISRC2INT2", "ISRC2INT2"),

	ARIZONA_MUX_ROUTES("ISRC2DEC1", "ISRC2DEC1"),
	ARIZONA_MUX_ROUTES("ISRC2DEC2", "ISRC2DEC2"),

	ARIZONA_DSP_ROUTES("DSP1"),

	{ "DSP Virtual Output", NULL, "DSP Virtual Output Mux" },
	{ "DSP Virtual Output Mux", "DSP1", "DSP1" },
	{ "DSP Virtual Output", NULL, "SYSCLK" },

	{ "AEC Loopback", "HPOUT1L", "OUT1L" },
	{ "AEC Loopback", "HPOUT1R", "OUT1R" },
	{ "HPOUT1L", NULL, "OUT1L" },
	{ "HPOUT1R", NULL, "OUT1R" },

	{ "AEC Loopback", "HPOUT2L", "OUT2L" },
	{ "AEC Loopback", "HPOUT2R", "OUT2R" },
	{ "HPOUT2L", NULL, "OUT2L" },
	{ "HPOUT2R", NULL, "OUT2R" },

	{ "AEC Loopback", "EPOUT", "OUT3L" },
	{ "EPOUTN", NULL, "OUT3L" },
	{ "EPOUTP", NULL, "OUT3L" },

	{ "AEC Loopback", "SPKOUTL", "OUT4L" },
	{ "SPKOUTLN", NULL, "OUT4L" },
	{ "SPKOUTLP", NULL, "OUT4L" },

	{ "AEC Loopback", "SPKOUTR", "OUT4R" },
	{ "SPKOUTRN", NULL, "OUT4R" },
	{ "SPKOUTRP", NULL, "OUT4R" },

	{ "AEC Loopback", "SPKDAT1L", "OUT5L" },
	{ "AEC Loopback", "SPKDAT1R", "OUT5R" },
	{ "SPKDAT1L", NULL, "OUT5L" },
	{ "SPKDAT1R", NULL, "OUT5R" },

	{ "MICSUPP", NULL, "SYSCLK" },

	{ "DRC1 Signal Activity", NULL, "DRC1L" },
	{ "DRC1 Signal Activity", NULL, "DRC1R" },
};

static int wm5102_set_fll(struct snd_soc_codec *codec, int fll_id, int source,
			  unsigned int Fref, unsigned int Fout)
{
	struct wm5102_priv *wm5102 = snd_soc_codec_get_drvdata(codec);

	switch (fll_id) {
	case WM5102_FLL1:
		return arizona_set_fll(&wm5102->fll[0], source, Fref, Fout);
	case WM5102_FLL2:
		return arizona_set_fll(&wm5102->fll[1], source, Fref, Fout);
	case WM5102_FLL1_REFCLK:
		return arizona_set_fll_refclk(&wm5102->fll[0], source, Fref,
					      Fout);
	case WM5102_FLL2_REFCLK:
		return arizona_set_fll_refclk(&wm5102->fll[1], source, Fref,
					      Fout);
	default:
		return -EINVAL;
	}
}

#define WM5102_RATES SNDRV_PCM_RATE_8000_192000

#define WM5102_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver wm5102_dai[] = {
	{
		.name = "wm5102-aif1",
		.id = 1,
		.base = ARIZONA_AIF1_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = WM5102_RATES,
			.formats = WM5102_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF1 Capture",
			 .channels_min = 1,
			 .channels_max = 8,
			 .rates = WM5102_RATES,
			 .formats = WM5102_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
		.symmetric_samplebits = 1,
	},
	{
		.name = "wm5102-aif2",
		.id = 2,
		.base = ARIZONA_AIF2_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = WM5102_RATES,
			.formats = WM5102_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF2 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = WM5102_RATES,
			 .formats = WM5102_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
		.symmetric_samplebits = 1,
	},
	{
		.name = "wm5102-aif3",
		.id = 3,
		.base = ARIZONA_AIF3_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = WM5102_RATES,
			.formats = WM5102_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF3 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = WM5102_RATES,
			 .formats = WM5102_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
		.symmetric_samplebits = 1,
	},
	{
		.name = "wm5102-slim1",
		.id = 4,
		.playback = {
			.stream_name = "Slim1 Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = WM5102_RATES,
			.formats = WM5102_FORMATS,
		},
		.capture = {
			 .stream_name = "Slim1 Capture",
			 .channels_min = 1,
			 .channels_max = 4,
			 .rates = WM5102_RATES,
			 .formats = WM5102_FORMATS,
		 },
		.ops = &arizona_simple_dai_ops,
	},
	{
		.name = "wm5102-slim2",
		.id = 5,
		.playback = {
			.stream_name = "Slim2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = WM5102_RATES,
			.formats = WM5102_FORMATS,
		},
		.capture = {
			 .stream_name = "Slim2 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = WM5102_RATES,
			 .formats = WM5102_FORMATS,
		 },
		.ops = &arizona_simple_dai_ops,
	},
	{
		.name = "wm5102-slim3",
		.id = 6,
		.playback = {
			.stream_name = "Slim3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = WM5102_RATES,
			.formats = WM5102_FORMATS,
		},
		.capture = {
			 .stream_name = "Slim3 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = WM5102_RATES,
			 .formats = WM5102_FORMATS,
		 },
		.ops = &arizona_simple_dai_ops,
	},
};

static irqreturn_t adsp2_irq(int irq, void *data)
{
	struct wm5102_priv *wm5102 = data;

	if (wm5102->core.arizona->pdata.ez2ctrl_trigger &&
	    wm5102->core.adsp[0].fw_features.ez2control_trigger)
		wm5102->core.arizona->pdata.ez2ctrl_trigger();

	return IRQ_HANDLED;
}

static int wm5102_codec_probe(struct snd_soc_codec *codec)
{
	struct wm5102_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->core.arizona;
	int ret;

	ret = snd_soc_add_codec_controls(codec, wm_adsp2_fw_controls, 2);
	if (ret != 0)
		return ret;

	wm_adsp_init_debugfs(&priv->core.adsp[0], codec);

	arizona_init_spk(codec);
	arizona_init_gpio(codec);

	snd_soc_dapm_disable_pin(&codec->dapm, "HAPTICS");

	priv->core.arizona->dapm = &codec->dapm;

	ret = arizona_request_irq(arizona, ARIZONA_IRQ_DSP_IRQ1,
				  "ADSP2 interrupt 1", adsp2_irq, priv);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to request DSP IRQ: %d\n", ret);
		return ret;
	}

	ret = irq_set_irq_wake(arizona->irq, 1);
	if (ret)
		dev_err(arizona->dev,
			"Failed to set DSP IRQ to wake source: %d\n",
			ret);

	snd_soc_dapm_enable_pin(&codec->dapm, "DRC1 Signal Activity");
	ret = regmap_update_bits(arizona->regmap, ARIZONA_IRQ2_STATUS_3_MASK,
				 ARIZONA_IM_DRC1_SIG_DET_EINT2, 0);
	if (ret != 0) {
		dev_err(arizona->dev,
			"Failed to unmask DRC1 IRQ for DSP: %d\n",
			ret);
		return ret;
	}

	return 0;
}

static int wm5102_codec_remove(struct snd_soc_codec *codec)
{
	struct wm5102_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->core.arizona;

	wm_adsp_cleanup_debugfs(&priv->core.adsp[0]);

	irq_set_irq_wake(arizona->irq, 0);
	arizona_free_irq(arizona, ARIZONA_IRQ_DSP_IRQ1, priv);
	regmap_update_bits(arizona->regmap, ARIZONA_IRQ2_STATUS_3_MASK,
				 ARIZONA_IM_DRC1_SIG_DET_EINT2,
				 ARIZONA_IM_DRC1_SIG_DET_EINT2);

	priv->core.arizona->dapm = NULL;

	return 0;
}

#define WM5102_DIG_VU 0x0200

static unsigned int wm5102_digital_vu[] = {
	ARIZONA_DAC_DIGITAL_VOLUME_1L,
	ARIZONA_DAC_DIGITAL_VOLUME_1R,
	ARIZONA_DAC_DIGITAL_VOLUME_2L,
	ARIZONA_DAC_DIGITAL_VOLUME_2R,
	ARIZONA_DAC_DIGITAL_VOLUME_3L,
	ARIZONA_DAC_DIGITAL_VOLUME_4L,
	ARIZONA_DAC_DIGITAL_VOLUME_4R,
	ARIZONA_DAC_DIGITAL_VOLUME_5L,
	ARIZONA_DAC_DIGITAL_VOLUME_5R,
};

static struct regmap *wm5102_get_regmap(struct device *dev)
{
	struct wm5102_priv *priv = dev_get_drvdata(dev);

	return priv->core.arizona->regmap;
}

static struct snd_soc_codec_driver soc_codec_dev_wm5102 = {
	.probe = wm5102_codec_probe,
	.remove = wm5102_codec_remove,
	.get_regmap = wm5102_get_regmap,

	.idle_bias_off = true,

	.set_sysclk = arizona_set_sysclk,
	.set_pll = wm5102_set_fll,

	.controls = wm5102_snd_controls,
	.num_controls = ARRAY_SIZE(wm5102_snd_controls),
	.dapm_widgets = wm5102_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(wm5102_dapm_widgets),
	.dapm_routes = wm5102_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(wm5102_dapm_routes),
};

static int wm5102_probe(struct platform_device *pdev)
{
	struct arizona *arizona = dev_get_drvdata(pdev->dev.parent);
	struct wm5102_priv *wm5102;
	int i, ret;

	wm5102 = devm_kzalloc(&pdev->dev, sizeof(struct wm5102_priv),
			      GFP_KERNEL);
	if (wm5102 == NULL)
		return -ENOMEM;
	platform_set_drvdata(pdev, wm5102);

	mutex_init(&wm5102->fw_lock);

	wm5102->core.arizona = arizona;
	wm5102->core.num_inputs = 6;

	wm5102->core.adsp[0].part = "wm5102";
	wm5102->core.adsp[0].num = 1;
	wm5102->core.adsp[0].type = WMFW_ADSP2;
	wm5102->core.adsp[0].base = ARIZONA_DSP1_CONTROL_1;
	wm5102->core.adsp[0].dev = arizona->dev;
	wm5102->core.adsp[0].regmap = arizona->regmap;
	wm5102->core.adsp[0].mem = wm5102_dsp1_regions;
	wm5102->core.adsp[0].num_mems = ARRAY_SIZE(wm5102_dsp1_regions);

	if (arizona->pdata.num_fw_defs[0]) {
		wm5102->core.adsp[0].firmwares
			= arizona->pdata.fw_defs[0];

		wm5102->core.adsp[0].num_firmwares
			= arizona->pdata.num_fw_defs[0];
	}

	ret = wm_adsp2_init(&wm5102->core.adsp[0], &wm5102->fw_lock);
	if (ret != 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(wm5102->fll); i++)
		wm5102->fll[i].vco_mult = 1;

	arizona_init_fll(arizona, 1, ARIZONA_FLL1_CONTROL_1 - 1,
			 ARIZONA_IRQ_FLL1_LOCK, ARIZONA_IRQ_FLL1_CLOCK_OK,
			 &wm5102->fll[0]);
	arizona_init_fll(arizona, 2, ARIZONA_FLL2_CONTROL_1 - 1,
			 ARIZONA_IRQ_FLL2_LOCK, ARIZONA_IRQ_FLL2_CLOCK_OK,
			 &wm5102->fll[1]);

	for (i = 0; i < ARRAY_SIZE(wm5102_dai); i++)
		arizona_init_dai(&wm5102->core, i);

	/* Latch volume update bits */
	for (i = 0; i < ARRAY_SIZE(wm5102_digital_vu); i++)
		regmap_update_bits(arizona->regmap, wm5102_digital_vu[i],
				   WM5102_DIG_VU, WM5102_DIG_VU);

	pm_runtime_enable(&pdev->dev);
	pm_runtime_idle(&pdev->dev);

	return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_wm5102,
				      wm5102_dai, ARRAY_SIZE(wm5102_dai));
}

static int wm5102_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static struct platform_driver wm5102_codec_driver = {
	.driver = {
		.name = "wm5102-codec",
		.owner = THIS_MODULE,
	},
	.probe = wm5102_probe,
	.remove = wm5102_remove,
};

module_platform_driver(wm5102_codec_driver);

MODULE_DESCRIPTION("ASoC WM5102 driver");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:wm5102-codec");
