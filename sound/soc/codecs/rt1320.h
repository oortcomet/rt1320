/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * rt1320.h -- RT1320 I2S ALSA SoC audio driver header
 *
 * Copyright(c) 2025 Realtek Semiconductor Corp.
 */

#ifndef __RT1320_H__
#define __RT1320_H__

#include <linux/regmap.h>
#include <sound/soc.h>

/* registers */
#define RT1320_CAE_DATA_PATH		0x0000c5c3
#define RT1320_DSP_DATA_INB01_PATH	0x0000c5c4
#define RT1320_DSP_DATA_INB23_PATH	0x0000c5c5
#define RT1320_DSP_DATA_OUTB01_PATH	0x0000c5c6
#define RT1320_DSP_DATA_OUTB23_PATH	0x0000c5c7
#define RT1320_DA_FILTER_DATA		0x0000c5c8
#define RT1320_PDB_PIN_SET		0x0000c570
#define RT1320_SPK_POST_GAIN_R_LO	0x0000dd08
#define RT1320_SPK_POST_GAIN_R_HI	0x0000dd09
#define RT1320_SPK_POST_GAIN_L_LO	0x0000dd0a
#define RT1320_SPK_POST_GAIN_L_HI	0x0000dd0b
#define RT1320_HIFI3_DSP_CTRL_2		0x0000f01e

/* 0xc5c3: CAE DATA Select Setting */
#define RT1320_CAE_POST_R_SEL_MASK	0x3 << 6
#define RT1320_CAE_POST_R_SEL_T7	0x3 << 6
#define RT1320_CAE_POST_R_SEL_T6	0x2 << 6
#define RT1320_CAE_POST_R_SEL_T5	0x1 << 6
#define RT1320_CAE_POST_R_SEL_T4	0x0 << 6
#define RT1320_CAE_POST_L_SEL_MASK	0x3 << 4
#define RT1320_CAE_POST_L_SEL_T3	0x3 << 4
#define RT1320_CAE_POST_L_SEL_T2	0x2 << 4
#define RT1320_CAE_POST_L_SEL_T1	0x1 << 4
#define RT1320_CAE_POST_L_SEL_T0	0x0 << 4
#define RT1320_CAE_WDATA_SEL_MASK	0x7 << 0
#define RT1320_CAE_WDATA_SEL_OUTB1	0x4 << 0
#define RT1320_CAE_WDATA_SEL_OUTB0	0x3 << 0
#define RT1320_CAE_WDATA_SEL_SRCIN	0x2 << 0
#define RT1320_CAE_WDATA_SEL_I2S	0x1 << 0
#define RT1320_CAE_WDATA_SEL_DP1	0x0 << 0

/* DSP InBound/OutBound Select Setting */
#define RT1320_DSP_OUTB1_SEL_MASK	0x7 << 4
#define RT1320_DSP_OUTB1_SEL_SFT	4
#define RT1320_DSP_OUTB0_SEL_MASK	0x7 << 0
#define RT1320_DSP_OUTB0_SEL_SFT	0
#define RT1320_DSP_INB1_SEL_MASK	0xf << 4
#define RT1320_DSP_INB1_SEL_SFT		4
#define RT1320_DSP_INB0_SEL_MASK	0xf << 0
#define RT1320_DSP_INB0_SEL_SRCIN	0x2 << 0
#define RT1320_DSP_INB0_SEL_I2S		0x1 << 0
#define RT1320_DSP_INB0_SEL_DP1		0x0 << 0
#define RT1320_DSP_INB0_SEL_SFT		0

/* 0xc5c8: CAE DATA Select Setting */
#define RT1320_DA_FILTER_SEL_MASK	0x7
#define RT1320_DA_FILTER_SEL_OUTB1	0x5
#define RT1320_DA_FILTER_SEL_OUTB0	0x4
#define RT1320_DA_FILTER_SEL_CAE	0x3
#define RT1320_DA_FILTER_SEL_SRCIN	0x2
#define RT1320_DA_FILTER_SEL_I2S	0x1
#define RT1320_DA_FILTER_SEL_DP1	0x0

/* 0xc570: PDB Pin Setting */
#define RT1320_PDB_PIN_REG		0x1 << 3
#define RT1320_PDB_PIN_POLY		0x1 << 2
#define RT1320_PDB_PIN_SEL_MASK		0x1 << 1
#define RT1320_PDB_PIN_SEL_MNL		0x1 << 1
#define RT1320_PDB_PIN_SEL_PAD		0x0 << 1
#define RT1320_PDB_PIN_MNL_MASK		0x1 << 0
#define RT1320_PDB_PIN_MNL_ON		0x1 << 0
#define RT1320_PDB_PIN_MNL_OFF		0x0 << 0

/* 0xf01e: HIFI3 DSP CTRL 2 */
#define RT1320_HIFI3_DSP_MASK		0x1
#define RT1320_HIFI3_DSP_STALL		0x1
#define RT1320_HIFI3_DSP_RUN		0x0

struct rt1320_priv {
	struct snd_soc_component *component;
	struct regmap *regmap;
	int version_id;
	bool bypass_dsp;
	bool fu_dapm_mute;
	bool fu_mixer_mute[4];
};

int rt1320_afx_load(struct rt1320_priv *rt1320, unsigned char action);
#endif /* __RT1320_H__ */
