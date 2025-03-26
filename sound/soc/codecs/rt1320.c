// SPDX-License-Identifier: GPL-2.0-only
//
// rt1320.c -- rt1320 I2S ALSA SoC amplifier audio driver
//
// Copyright(c) 2025 Realtek Semiconductor Corp.
//
//
#define DEBUG
#include <linux/acpi.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/dmi.h>
#include <linux/firmware.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include "rt1320.h"
#include "rt1320-sdw.h"
#include "rt1320-spi.h"
#include "rt1320_mcu.h"

#define DSP_FW_CHK

static const struct reg_sequence vc_init_list[] = {
	//IC version VAC
	//MCU patch code : State Machine, SMPU Clear, Clk Stop, DMIC CS, ADC Temp, XU 24 Bypass, tfb_en timing control
	{ 0x0000c000, 0x03 }, //vendor define hard reset
	{ 0x0000c003, 0xe0 }, //sys_en=1, sys_need_div2=1, sys_clk_sel=rc
	{ 0x0000c01b, 0xfd }, //clock_alive, pll_sel1=rc_osc, pll_sel0=bclk
	// { 0x0000c01a, 0x41 }, // derek kR0 debug, PLL input = RC clk
	{ 0x0000c5c3, 0xf3 }, //cae_rdata_r_sel=cae_test_out7, cae_rdata_l_sel=cae_test_out3, cae_wdata_sel=ob0
	{ 0x0000c5c2, 0x50 }, //srcin_wdata_sel = i2s, silence_det_0_sel = i2s
	{ 0x0000c5d3, 0x40 }, // i2s rx0 from iv sync
	{ 0x0000c58d, 0x11 }, // 4 ch, tag en, codec=00, total channel=01
	{ 0x0000c58c, 0x98 }, // v on ch0, i on ch1
	{ 0x0000c057, 0x51 }, //silence_det_clk_div=5, ulsd_clk_div=2
	{ 0x0000c054, 0x35 }, //iv_phase_comp_clk_div=4, pdm_tx_clk_div=8 (default)
	{ 0x0000ca05, 0xd6 }, //ad stereo1 dmic src enable, op_rate=6.144m
	{ 0x0000ca07, 0x07 }, //ad_stereo1_vol_adcl = -12db
	{ 0x0000ca25, 0xd6 }, //ad stereo2 dmic src enable, op_rate=6.144m
	{ 0x0000ca27, 0x07 }, //ad_stereo2_vol_adcl = -12db

	//Auto mode讓power state自動控制pow_pllb
	{ 0x0000c604, 0x40 }, //POW_PLLB
	{ 0x0000c609, 0x40 }, //SEL_PLLOUT_PLLB, Fout=196.608mhz
	{ 0x0000c600, 0x05 }, // BYP_PS_PLLB = 1
	{ 0x0000c601, 0x80 }, // PLL M code = 0

	{ 0x0000c046, 0xfc }, //iv_il_inb_en, iv_il_i2s_en, sdca_mcu_wdg_0_en, sdca_mcu_wdg_1_en, gain_sync_en
	{ 0x0000c045, 0xff }, //dmic1_float_det_en, dmic2_float_det_en, t0_en, mis_cmpsat_en, rs_cali_en
	{ 0x0000c044, 0x3f }, //dmic2_en, adf_mono1_en, adf_mono2_en, silence_det_en = 1
	{ 0x0000c043, 0xff }, //iv_phase_comp_en, dmic1_en, adf3_en
	{ 0x0000c042, 0xff }, //daf_dvol_en, daf_dvol_mod_en, ulsd_en, adf1_en, adf2_en, adf_temp_en
	{ 0x0000c041, 0x7f }, //cae_en, cae_sram_en, daf_dmix_en, daf_dmix_mod_en, pdm_en
	{ 0x0000c040, 0xff }, //dp1_fifo_en, dp5_fifo_en, i2s_fifo_en, srcin_en, spk_dsp_en
	{ 0x0000cc10, 0x01 }, //srcin1_en=1
	{ 0x0000c901, 0x09 }, //silence_level=-102db
	{ 0x0000c900, 0xa0 }, //dp1 silence debounce time = 320ms at sample rate 48khz
	{ 0x0000cf02, 0x0f }, //after delay/after rs calibration
	{ 0x0000de03, 0x05 }, //sin_gen_mute=1
	{ 0x0000dd0b, 0x0e }, //-9db
	{ 0x0000dd0a, 0x7d },
	{ 0x0000dd09, 0x0e }, //-9db
	{ 0x0000dd08, 0x7d },
	{ 0x0000c570, 0x0b }, //reg_pdb=1
	{ 0x0000c086, 0x02 }, //fre_trans_en = 1
	{ 0x0000c085, 0x7f }, //fre_trans_amplitude = c000 (ratio = 0.25)
	{ 0x0000c084, 0x00 },
	{ 0x0000c081, 0xfe }, //turn on hifi3 dsp, dsp clock source = rc osc
	{ 0x0000f084, 0x0f },
	{ 0x0000f083, 0xff },
	{ 0x0000f082, 0xff },
	{ 0x0000f081, 0xff },
	{ 0x0000f080, 0xff },
	//CAE setting
	{ 0x0000e802, 0xf8 }, //cae_out_sel
	{ 0x0000e803, 0xbe }, //CAE run, reg_cae_pm_crc8_en=1
	{ 0x0000c003, 0xc0 }, //sys_clk=PLL
	//Enable KR0 Setting
	{ 0x0000d470, 0xec }, //Pwait + IV convergence + SW pilot
	{ 0x0000d471, 0x3a },
	{ 0x0000d474, 0x11 }, //adf_clock speed up
	{ 0x0000d475, 0x32 }, //clk_convergence_delay=50ms
	{ 0x0000d478, 0x64 },
	{ 0x0000d479, 0x20 },
	{ 0x0000d47a, 0x10 },
	{ 0x0000c019, 0x10 }, //Enable CLK gateing

	// Run KR0 ROM
	{ 0x0000d487, 0x0b },
	{ 0x0000d487, 0x3b },
	{ 0x0000d486, 0xc3 },
	/*
	 * Load the patch code here
	 */
	{ 0x3fc2bf83, 0x00 },
	{ 0x3fc2bf82, 0x00 },
	{ 0x3fc2bf81, 0x00 },
	{ 0x3fc2bf80, 0x00 },
	{ 0x3fc2bfc7, 0x00 },
	{ 0x3fc2bfc6, 0x00 },
	{ 0x3fc2bfc5, 0x00 },
	{ 0x3fc2bfc4, 0x00 },
	{ 0x3fc2bfc3, 0x00 },
	{ 0x3fc2bfc2, 0x00 },
	{ 0x3fc2bfc1, 0x00 },
	{ 0x3fc2bfc0, 0x03 },

	{ 0x0000d486, 0x43 },

	{ 0x1000db00, 0x07 }, //HV write 4-bytes
	{ 0x1000db01, 0x00 }, //Abnormal_det_en=0
	{ 0x1000db02, 0x11 },
	{ 0x1000db03, 0x00 },
	{ 0x1000db04, 0x00 },
	{ 0x1000db05, 0x82 },
	{ 0x1000db06, 0x04 }, //pow_otp_mode=0
	{ 0x1000db07, 0xf1 },
	{ 0x1000db08, 0x00 },
	{ 0x1000db09, 0x00 },
	{ 0x1000db0a, 0x40 },
	{ 0x1000db0b, 0x02 }, //pwm_dc_det_flag_sel=instant
	{ 0x1000db0c, 0xf2 },
	{ 0x1000db0d, 0x00 },
	{ 0x1000db0e, 0x00 },
	{ 0x1000db0f, 0xe0 },
	{ 0x1000db10, 0x00 }, //efuse_read_en
	{ 0x1000db11, 0x10 },
	{ 0x1000db12, 0x00 },
	{ 0x1000db13, 0x00 },
	{ 0x1000db14, 0x45 },
	{ 0x1000db15, 0x0d }, //dre rising switching disable
	{ 0x1000db16, 0x01 },
	{ 0x1000db17, 0x00 },
	{ 0x1000db18, 0x00 },
	{ 0x1000db19, 0xbf },
	{ 0x1000db1a, 0x13 },
	{ 0x1000db1b, 0x09 },
	{ 0x1000db1c, 0x00 },
	{ 0x1000db1d, 0x00 },
	{ 0x1000db1e, 0x00 },
	{ 0x1000db1f, 0x12 },
	{ 0x1000db20, 0x09 },
	{ 0x1000db21, 0x00 },
	{ 0x1000db22, 0x00 },
	{ 0x1000db23, 0x00 },

	{ 0x1000d540, 0x21 }, //reload_efuse, blindwrite_irq

	//SDCA amp PDE control
	{ 0x41001988, 0x00 }, //pde23=ps0
	//SDCA amp FU control
	{ 0x41000189, 0x00 }, //fu unmute
	{ 0x4100018a, 0x00 }, //fu unmute
	{ 0x410018c9, 0x01 }, //mfpu_algorithm_prepare
	{ 0x410018a9, 0x01 }, //mfpu_algorithm_enable  0'b  bypass dsp  ; 1'b non bypass
	{ 0x41181880, 0x00 }, //not bypass
	{ 0x0000c5fb, 0x00 }, //tv_scalar_mode_mnl => 00: tv mode(i2s)
};

static const struct reg_default rt1320_regs[] = {
	{ 0x00000100, 0 },
	{ 0x0000c000, 0x00 },
	{ 0x0000c003, 0x00 },
	{ 0x0000c019, 0x00 },
	{ 0x0000c01b, 0xfc },
	{ 0x0000c040, 0x00 },
	{ 0x0000c041, 0x00 },
	{ 0x0000c042, 0x00 },
	{ 0x0000c043, 0x00 },
	{ 0x0000c044, 0x00 },
	{ 0x0000c045, 0x00 },
	{ 0x0000c046, 0x00 },
	{ 0x0000c047, 0x00 },
	{ 0x0000c054, 0x53 },
	{ 0x0000c057, 0x55 },
	{ 0x0000c081, 0xc8 },
	{ 0x0000c084, 0x00 },
	{ 0x0000c085, 0x00 },
	{ 0x0000c086, 0x01 },
	{ 0x0000c408, 0x00 },
	{ 0x0000c409, 0x00 },
	{ 0x0000c40a, 0x00 },
	{ 0x0000c40b, 0x00 },
	{ 0x0000c570, 0x00 },
	{ 0x0000c58c, 0x10 },
	{ 0x0000c58d, 0x10 },
	{ 0x0000c5c2, 0x00 },
	{ 0x0000c5c3, 0x02 },
	{ 0x0000c5c4, 0x12 },
	{ 0x0000c5c8, 0x05 },
	{ 0x0000c5d3, 0x00 },
	{ 0x0000c5fb, 0x02 },
	{ 0x0000c600, 0x04 },
	{ 0x0000c601, 0x83 },
	{ 0x0000c604, 0x30 },
	{ 0x0000c609, 0x42 },
	{ 0x0000c700, 0x00 },
	{ 0x0000c701, 0x11 },
	{ 0x0000c900, 0x30 },
	{ 0x0000c901, 0x04 },
	{ 0x0000ca05, 0x66 },
	{ 0x0000ca07, 0x17 },
	{ 0x0000ca25, 0x66 },
	{ 0x0000ca27, 0x17 },
	{ 0x0000cc10, 0x00 },
	{ 0x0000cd00, 0xc5 },
	{ 0x0000cf02, 0x00 },
	{ 0x0000d470, 0x00 },
	{ 0x0000d471, 0x00 },
	{ 0x0000d474, 0x00 },
	{ 0x0000d475, 0x00 },
	{ 0x0000d478, 0x00 },
	{ 0x0000d479, 0x00 },
	{ 0x0000d47a, 0x00 },
	{ 0x0000d486, 0x80 },
	{ 0x0000d487, 0x03 },
	{ 0x0000dd08, 0xff },
	{ 0x0000dd09, 0x0f },
	{ 0x0000dd0a, 0xff },
	{ 0x0000dd0b, 0x0f },
	{ 0x0000de03, 0x01 },
	{ 0x0000e802, 0xf8 },
	{ 0x0000e803, 0x3e },
	/*
	{ 0x0000f080, 0xef },
	{ 0x0000f081, 0xbe },
	{ 0x0000f082, 0xad },
	{ 0x0000f083, 0xde },
	{ 0x0000f084, 0xef },
	{ 0x1000d540, 0xef },
	{ 0x1000db00, 0xef },
	{ 0x1000db01, 0xbe },
	{ 0x1000db02, 0xad },
	{ 0x1000db03, 0xde },
	{ 0x1000db04, 0xef },
	{ 0x1000db05, 0xbe },
	{ 0x1000db06, 0xad },
	{ 0x1000db07, 0xde },
	{ 0x1000db08, 0xef },
	{ 0x1000db09, 0xbe },
	{ 0x1000db0a, 0xad },
	{ 0x1000db0b, 0xde },
	{ 0x1000db0c, 0xef },
	{ 0x1000db0d, 0xbe },
	{ 0x1000db0e, 0xad },
	{ 0x1000db0f, 0xde },
	{ 0x1000db10, 0xef },
	{ 0x1000db11, 0xbe },
	{ 0x1000db12, 0xad },
	{ 0x1000db13, 0xde },
	{ 0x1000db14, 0xef },
	{ 0x1000db15, 0xbe },
	{ 0x1000db16, 0xad },
	{ 0x1000db17, 0xde },
	{ 0x1000db18, 0xef },
	{ 0x1000db19, 0xbe },
	{ 0x1000db1a, 0xad },
	{ 0x1000db1b, 0xde },
	{ 0x1000db1c, 0xef },
	{ 0x1000db1d, 0xbe },
	{ 0x1000db1e, 0xad },
	{ 0x1000db1f, 0xde },
	{ 0x1000db20, 0xef },
	{ 0x1000db21, 0xbe },
	{ 0x1000db22, 0xad },
	{ 0x1000db23, 0xde },
	{ 0x3fc2bf80, 0xef },
	{ 0x3fc2bf81, 0xbe },
	{ 0x3fc2bf82, 0xad },
	{ 0x3fc2bf83, 0xde },
	{ 0x3fc2bfc0, 0xef },
	{ 0x3fc2bfc1, 0xbe },
	{ 0x3fc2bfc2, 0xad },
	{ 0x3fc2bfc3, 0xde },
	{ 0x3fc2bfc4, 0xef },
	{ 0x3fc2bfc5, 0xbe },
	{ 0x3fc2bfc6, 0xad },
	{ 0x3fc2bfc7, 0xde },
	 */
};

static bool rt1320_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x00000100:
	case 0x0000c000:
	case 0x0000c003:
	case 0x0000c019:
	case 0x0000c01a:
	case 0x0000c01b:
	case 0x0000c040:
	case 0x0000c041:
	case 0x0000c042:
	case 0x0000c043:
	case 0x0000c044:
	case 0x0000c045:
	case 0x0000c046:
	case 0x0000c047:
	case 0x0000c054:
	case 0x0000c057:
	case 0x0000c081:
	case 0x0000c084:
	case 0x0000c085:
	case 0x0000c086:
	case 0x0000c400 ... 0x0000c40b:
	case 0x0000c560:
	case 0x0000c570:
	case 0x0000c58c:
	case 0x0000c58d:
	case 0x0000c5c0:
	case 0x0000c5c1:
	case 0x0000c5c2:
	case 0x0000c5c3:
	case 0x0000c5c4:
	case 0x0000c5c8:
	case 0x0000c5d3:
	case 0x0000c5fb:
	case 0x0000c600:
	case 0x0000c601:
	case 0x0000c604:
	case 0x0000c609:
	case 0x0000c680:
	case 0x0000c700:
	case 0x0000c701:
	case 0x0000c900:
	case 0x0000c901:
	case 0x0000ca05:
	case 0x0000ca07:
	case 0x0000ca25:
	case 0x0000ca27:
	case 0x0000cc10:
	case 0x0000cd00:
	case 0x0000cf02:
	case 0x0000d470:
	case 0x0000d471:
	case 0x0000d474:
	case 0x0000d475:
	case 0x0000d478:
	case 0x0000d479:
	case 0x0000d47a:
	case 0x0000d486:
	case 0x0000d487:
	case 0x0000dd08 ... 0x0000dd0b:
	case 0x0000de03:
	case 0x0000e802:
	case 0x0000e803:
	case 0x0000f080:
	case 0x0000f081:
	case 0x0000f082:
	case 0x0000f083:
	case 0x0000f084:
	case 0x0000f015:
	case 0x0000f01c ... 0x0000f01f:
	case 0x1000cd91 ... 0x1000cd96:
	case 0x1000f008:
	case 0x1000f021:
	case 0x3fc2ab80 ... 0x3fc2abd4:
	case 0x3fc2bf80 ... 0x3fc2bf83:
	case 0x3fc2bfc0 ... 0x3fc2bfc7:
	case 0x3fe2e000 ... 0x3fe2e003:
	/*
	case 0x1000d540:
	case 0x1000db00:
	case 0x1000db01:
	case 0x1000db02:
	case 0x1000db03:
	case 0x1000db04:
	case 0x1000db05:
	case 0x1000db06:
	case 0x1000db07:
	case 0x1000db08:
	case 0x1000db09:
	case 0x1000db0a:
	case 0x1000db0b:
	case 0x1000db0c:
	case 0x1000db0d:
	case 0x1000db0e:
	case 0x1000db0f:
	case 0x1000db10:
	case 0x1000db11:
	case 0x1000db12:
	case 0x1000db13:
	case 0x1000db14:
	case 0x1000db15:
	case 0x1000db16:
	case 0x1000db17:
	case 0x1000db18:
	case 0x1000db19:
	case 0x1000db1a:
	case 0x1000db1b:
	case 0x1000db1c:
	case 0x1000db1d:
	case 0x1000db1e:
	case 0x1000db1f:
	case 0x1000db20:
	case 0x1000db21:
	case 0x1000db22:
	case 0x1000db23:
	case 0x3fc2bf80:
	case 0x3fc2bf81:
	case 0x3fc2bf82:
	case 0x3fc2bf83:
	case 0x3fc2bfc0:
	case 0x3fc2bfc1:
	case 0x3fc2bfc2:
	case 0x3fc2bfc3:
	case 0x3fc2bfc4:
	case 0x3fc2bfc5:
	case 0x3fc2bfc6:
	case 0x3fc2bfc7:
	*/
	case 0x41000189:
	case 0x4100018a:
	case 0x410018a9:
	case 0x410018c9:
	case 0x41001988:
	case 0x41081980:
	case 0x41181880:
		return true;
	default:
		break;
	}

	return false;
}

static bool rt1320_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x00000100:
	case 0x0000c000:
	// case 0x0000c003:
	// case 0x0000c019:
	// case 0x0000c01a:
	// case 0x0000c01b:

	// case 0x0000c040:
	// case 0x0000c041:
	// case 0x0000c042:
	// case 0x0000c043:
	case 0x0000c044:
	// case 0x0000c045:
	// case 0x0000c046:
	// case 0x0000c047:
	// case 0x0000c054:
	// case 0x0000c057:
	// case 0x0000c081:
	// case 0x0000c084:
	// case 0x0000c085:
	// case 0x0000c086:
	case 0x0000c400 ... 0x0000c40b:
	case 0x0000c560:
	// case 0x0000c570:
	// case 0x0000c58c:
	// case 0x0000c58d:
	// case 0x0000c5c2:
	// case 0x0000c5c3:
	// case 0x0000c5c4:
	// case 0x0000c5c8:
	// case 0x0000c5d3:
	// case 0x0000c5fb:
	// case 0x0000c600:
	// case 0x0000c601:
	// case 0x0000c604:
	// case 0x0000c609:
	case 0x0000c680:
	// case 0x0000c700:
	// case 0x0000c701:

	case 0x0000c900:
	// case 0x0000c901:
	// case 0x0000ca05:
	// case 0x0000ca07:
	// case 0x0000ca25:
	// case 0x0000ca27:
	// case 0x0000cc10:
	// case 0x0000cd00:
	// case 0x0000cf02:

	// case 0x0000d470:
	// case 0x0000d471:
	// case 0x0000d474:
	// case 0x0000d475:
	// case 0x0000d478:
	// case 0x0000d479:
	// case 0x0000d47a:
	case 0x0000d486:
	case 0x0000d487:

	// case 0x0000dd08:
	// case 0x0000dd09:
	// case 0x0000dd0a:
	// case 0x0000dd0b:
	// case 0x0000de03:
	// case 0x0000e802:
	// case 0x0000e803:

	case 0x1000cd91 ... 0x1000cd96:
	case 0x1000f008:
	case 0x0000f015:
	case 0x1000f021:
	case 0x0000f01c ... 0x0000f01f:
	case 0x3fc2ab80 ... 0x3fc2abd4:
	case 0x3fc2bf80 ... 0x3fc2bf83:
	case 0x3fc2bfc0 ... 0x3fc2bfc7:
	case 0x3fe2e000 ... 0x3fe2e003:
	case 0x41000189:
	case 0x4100018a:
	case 0x410018a9:
	case 0x410018c9:
	case 0x41001988:
	case 0x41081980:
	case 0x41181880:
		return true;
	default:
		return false;
	}

	return true;
}

/*
 * The 'patch code' is written to the patch code area.
 */
static int rt1320_load_mcu_patch(struct rt1320_priv *rt1320)
{
	struct regmap *regmap = rt1320->regmap;
	struct device *dev = regmap_get_device(regmap);
	const struct firmware *patch;
	const char *filename;
	unsigned int addr, val;
	const unsigned char *ptr;
	int ret, i;
	bool patch_same = true;

	if (rt1320->version_id <= RT1320_VB)
		filename = RT1320_VAB_MCU_PATCH;
	else {
		filename = "realtek/rt1320/PatchCode_forWIKIItem9_1119.bin";
	}

	/* load the patch code here */
	ret = request_firmware(&patch, filename, dev);
	if (ret) {
		dev_err(dev, "%s: Failed to load %s firmware", __func__, filename);
		return ret;
#if 0
		regmap_write(rt1320->regmap, 0xc598, 0x00);
		regmap_write(rt1320->regmap, 0x10007000, 0x67);
		regmap_write(rt1320->regmap, 0x10007001, 0x80);
		regmap_write(rt1320->regmap, 0x10007002, 0x00);
		regmap_write(rt1320->regmap, 0x10007003, 0x00);
#endif
	} else {
		ptr = (const unsigned char *)patch->data;

		if (patch->size != RT1320_MCU_PATCH_LEN * 8) {
			dev_err(dev, "%s: the patch's size is diff\n", __func__);
			patch_same = false;
		}

		if ((patch->size % 8) == 0) {
			for (i = 0; i < patch->size; i += 8) {
				addr = (ptr[i] & 0xff) | (ptr[i + 1] & 0xff) << 8 |
					(ptr[i + 2] & 0xff) << 16 | (ptr[i + 3] & 0xff) << 24;
				val = (ptr[i + 4] & 0xff) | (ptr[i + 5] & 0xff) << 8 |
					(ptr[i + 6] & 0xff) << 16 | (ptr[i + 7] & 0xff) << 24;

				if (addr != mcu_patch_code[i / 8].reg || val != mcu_patch_code[i / 8].def) {
					dev_err(dev, "%s: the patch's content is diff\n", __func__);
					patch_same = false;
					dev_err(dev, "%s: bin: addr=0x%x, val=0x%x; patch: reg=0x%x, def=0x%x\n",
						__func__, addr, val, mcu_patch_code[i / 8].reg, mcu_patch_code[i / 8].def);
				}
				if (addr > 0x10007fff || addr < 0x10007000) {
					dev_err(dev, "%s: the address 0x%x is wrong\n", __func__, addr);
					goto _exit_;
				}
				if (val > 0xff) {
					dev_err(dev, "%s: the value 0x%x is wrong\n", __func__, val);
					goto _exit_;
				}
				regmap_write(rt1320->regmap, addr, val);
			}
		}

		dev_dbg(dev, "%s: The patch code size is %d, the bin size is %d\n", __func__, RT1320_MCU_PATCH_LEN, patch->size);
		if (patch_same)
			dev_info(dev, "%s: The patch code and bin are SAME\n", __func__);
		else
			dev_info(dev, "%s: The patch code and bin are DIFF!!\n", __func__);
_exit_:
		release_firmware(patch);
	}

	return 0;
}

static int rt1320_vc_preset(struct rt1320_priv *rt1320)
{
	unsigned int i, reg, val, delay, retry, tmp;
	int ret;
	struct device *dev = regmap_get_device(rt1320->regmap);
	dev_dbg(dev, "-> %s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(vc_init_list); i++) {
		reg = vc_init_list[i].reg;
		val = vc_init_list[i].def;
		delay = vc_init_list[i].delay_us;

		if (reg == 0x3fc2bf83) {
			dev_dbg(dev, "Load MCU patch start\n");
			ret = rt1320_load_mcu_patch(rt1320);
			dev_dbg(dev, "Load MCU patch end\n");

			if (ret) {
				dev_err(dev, "Load MCU patch failed\n");
				return ret;
			}
		}
		if ((reg == 0x1000db00) && (val == 0x07)) {
			retry = 200;
			while (retry) {
				regmap_read(rt1320->regmap, RT1320_KR0_INT_READY, &tmp);
				dev_dbg(dev, "%s, RT1320_KR0_INT_READY=0x%x\n", __func__, tmp);
				if (tmp == 0x1f)
					break;
				usleep_range(1000, 1500);
				retry--;
			}
			if (!retry)
				dev_warn(dev, "%s MCU is NOT ready!", __func__);
		}

		regmap_write(rt1320->regmap, reg, val);
		if (delay)
			usleep_range(delay, delay + 1000);
#if 0
		if ((reg == 0xc081) && (val == 0xfe)) {
			// delay 1ms
			usleep_range(1000, 1500);
			// load AFX0/1
			rt1320_afx_load(rt1320);
		}
#endif
	}

	return 0;
}

#if 0
static const char * const rt1320_dsp_ib0_select[] = {
	"DP1",
	"I2S",
	"SRCIN",
	"CAE32",
};

static SOC_ENUM_SINGLE_DECL(rt1320_dsp_ib0_enum,
	RT1320_DSP_DATA_INB01_PATH, RT1320_DSP_INB0_SEL_SFT, rt1320_dsp_ib0_select);

static const char * const rt1320_dsp_ib1_select[] = {
	"SRCIN",
	"CAE32",
	"CAE28",
};

static SOC_ENUM_SINGLE_DECL(rt1320_dsp_ib1_enum,
	RT1320_DSP_DATA_INB01_PATH, RT1320_DSP_INB1_SEL_SFT, rt1320_dsp_ib1_select);

static const char * const rt1320_dsp_ob_select[] = {
	"CAE",
	"DMIX",
	"I2S",
};

static SOC_ENUM_SINGLE_DECL(rt1320_dsp_ob0_enum,
	RT1320_DSP_DATA_OUTB01_PATH, RT1320_DSP_OUTB0_SEL_SFT, rt1320_dsp_ob_select);

static SOC_ENUM_SINGLE_DECL(rt1320_dsp_ob1_enum,
	RT1320_DSP_DATA_OUTB01_PATH, RT1320_DSP_OUTB1_SEL_SFT, rt1320_dsp_ob_select);
#endif
static const char * const rt1320_dac_data_path[] = {
	"Pass", "Bypass",
};

static SOC_ENUM_SINGLE_DECL(rt1320_dac_data_enum, SND_SOC_NOPM,
	0, rt1320_dac_data_path);

static void rt1320_fw_param_write(struct rt1320_priv *rt1320,
	unsigned int start_addr, const char *buf, unsigned int buf_size)
{
	printk("%s, start\n", __func__);
#if 0 // I2C
	int i;
	for (i = 0; i < buf_size; i++) {
		regmap_write(rt1320->regmap, start_addr, buf[i]);
		start_addr++;
	}
#else // SPI
	rt1320_spi_burst_write(start_addr, buf, buf_size);
#endif
	printk("%s, done\n", __func__);
}

#ifdef DSP_FW_CHK
static int rt1320_dsp_fw_cmp(struct rt1320_priv *rt1320, unsigned int addr, const u8 *txbuf, unsigned int size)
{
	struct snd_soc_component *component = rt1320->component;
	int ret, i;
	u8 *rxbuf = kmalloc(size, GFP_KERNEL);
	if (!rxbuf)
		return -ENOMEM;

	ret = rt1320_spi_burst_read(addr, rxbuf, size);
	if (ret < 0) {
		dev_err(component->dev, "Addr %x: read error: %d\n", addr, ret);
	} else {
		if (memcmp(txbuf, rxbuf, size)) {
			dev_err(component->dev, "Addr %x: compare failed\n", addr);
			for (i = 0; i < size; i++) {
				if (txbuf[i] != rxbuf[i]) {
					dev_err(component->dev, "Diff: [0x%x] 0x%x != 0x%x\n",
						addr + i, txbuf[i], rxbuf[i]);
					break;
				}
			}
			ret = -EINVAL;
		} else {
			dev_info(component->dev, "Addr %x: compare succeeded\n", addr);
		}
	}

	kfree(rxbuf);
	return ret;
};
#endif

int rt1320_afx_load(struct rt1320_priv *rt1320)
{
	char afx0_name[] = "realtek/rt1320/AFX0_Ram.bin";
	char afx1_name[] = "realtek/rt1320/AFX1_Ram.bin";
	const struct firmware *fw0 = NULL, *fw1 = NULL;
	struct firmware fmw;
	int ret;

	// afx0
	ret = request_firmware(&fw0, afx0_name, rt1320->component->dev);
	if (ret) {
		dev_err(rt1320->component->dev, "%s: Request firmware %s failed\n",
			__func__, afx0_name);
		goto out;
	}

	if (!fw0->size) {
		dev_err(rt1320->component->dev, "%s: file read error: size = %lu\n",
			__func__, (unsigned long)fw0->size);
		ret = -EINVAL;
		goto out;
	}
	fmw.size = fw0->size;
	fmw.data = fw0->data;
	printk("%s, afx0 size=%zu, data[0]=0x%x\n", __func__, fmw.size, fmw.data[0]);

	rt1320_fw_param_write(rt1320, RT1320_AFX0_LOAD_ADDR, fmw.data, fmw.size);

#ifdef DSP_FW_CHK
	if (rt1320_dsp_fw_cmp(rt1320, RT1320_AFX0_LOAD_ADDR, fmw.data, fmw.size))
		pr_err("%s: RT1320_AFX0_LOAD_ADDR update failed!\n", __func__);
	else
		pr_err("%s: RT1320_AFX0_LOAD_ADDR update succeeded!\n", __func__);
#endif

	// afx1
	ret = request_firmware(&fw1, afx1_name, rt1320->component->dev);
	if (ret) {
		dev_err(rt1320->component->dev, "%s: Request firmware %s failed\n",
			__func__, afx1_name);
		goto out;
	}

	if (!fw1->size) {
		dev_err(rt1320->component->dev, "%s: file read error: size = %lu\n",
			__func__, (unsigned long)fw1->size);
		ret = -EINVAL;
		goto out;
	}
	fmw.size = fw1->size;
	fmw.data = fw1->data;
	printk("%s, afx1 size=%zu, data[4]=0x%x\n", __func__, fmw.size, fmw.data[4]);

	rt1320_fw_param_write(rt1320, RT1320_AFX1_LOAD_ADDR, fmw.data, fmw.size);

#ifdef DSP_FW_CHK
	if (rt1320_dsp_fw_cmp(rt1320, RT1320_AFX1_LOAD_ADDR, fmw.data, fmw.size))
		pr_err("%s: RT1320_AFX1_LOAD_ADDR update failed!\n", __func__);
	else
		pr_err("%s: RT1320_AFX1_LOAD_ADDR update succeeded!\n", __func__);
#endif

out:
	if (fw0)
		release_firmware(fw0);
	if (fw1)
		release_firmware(fw1);

	return ret;
}

int rt1320_afx_load_rom(struct rt1320_priv *rt1320)
{
	char afx0_name[] = "realtek/rt1320/AFX0.bin";
	char afx1_name[] = "realtek/rt1320/AFX1.bin";
	const struct firmware *fw0 = NULL, *fw1 = NULL;
	struct firmware fmw;
	int ret;

	// afx0
	ret = request_firmware(&fw0, afx0_name, rt1320->component->dev);
	if (ret) {
		dev_err(rt1320->component->dev, "%s: Request firmware %s failed\n",
			__func__, afx0_name);
		goto out;
	}

	if (!fw0->size) {
		dev_err(rt1320->component->dev, "%s: file read error: size = %lu\n",
			__func__, (unsigned long)fw0->size);
		ret = -EINVAL;
		goto out;
	}
	fmw.size = fw0->size;
	fmw.data = fw0->data;
	printk("%s, afx0 size=%zu, data[0]=0x%x\n", __func__, fmw.size, fmw.data[0]);

	rt1320_fw_param_write(rt1320, RT1320_AFX0_LOAD_ADDR, fmw.data, fmw.size);

#ifdef DSP_FW_CHK
	if (rt1320_dsp_fw_cmp(rt1320, RT1320_AFX0_LOAD_ADDR, fmw.data, fmw.size))
		pr_err("%s: RT1320_AFX0_LOAD_ADDR update failed!\n", __func__);
	else
		pr_err("%s: RT1320_AFX0_LOAD_ADDR update succeeded!\n", __func__);
#endif

	// afx1
	ret = request_firmware(&fw1, afx1_name, rt1320->component->dev);
	if (ret) {
		dev_err(rt1320->component->dev, "%s: Request firmware %s failed\n",
			__func__, afx1_name);
		goto out;
	}

	if (!fw1->size) {
		dev_err(rt1320->component->dev, "%s: file read error: size = %lu\n",
			__func__, (unsigned long)fw1->size);
		ret = -EINVAL;
		goto out;
	}
	fmw.size = fw1->size;
	fmw.data = fw1->data;
	printk("%s, afx1 size=%zu, data[4]=0x%x\n", __func__, fmw.size, fmw.data[4]);

	rt1320_fw_param_write(rt1320, RT1320_AFX1_LOAD_ADDR, fmw.data, fmw.size);

#ifdef DSP_FW_CHK
	if (rt1320_dsp_fw_cmp(rt1320, RT1320_AFX1_LOAD_ADDR, fmw.data, fmw.size))
		pr_err("%s: RT1320_AFX1_LOAD_ADDR update failed!\n", __func__);
	else
		pr_err("%s: RT1320_AFX1_LOAD_ADDR update succeeded!\n", __func__);
#endif

out:
	if (fw0)
		release_firmware(fw0);
	if (fw1)
		release_firmware(fw1);

	return ret;
}

static int rt1320_dsp_path_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);
	unsigned int val, val1;

	regmap_read(rt1320->regmap, RT1320_CAE_DATA_PATH, &val);
	regmap_read(rt1320->regmap, RT1320_DA_FILTER_DATA, &val1);

	dev_dbg(component->dev, "%s, bypass=%d, %x=%X, %x=%X\n",
		__func__, rt1320->bypass_dsp, RT1320_CAE_DATA_PATH, val, RT1320_DA_FILTER_DATA, val1);

	ucontrol->value.integer.value[0] = rt1320->bypass_dsp ? 1 : 0;

	return 0;
}

static int rt1320_dsp_path_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);
	unsigned int bypass;
	int changed = 0;

	dev_dbg(component->dev, "%s, bypass=%ld\n", __func__, ucontrol->value.integer.value[0]);
	bypass = ucontrol->value.integer.value[0];

	if (rt1320->bypass_dsp == !!bypass)
		changed = 1;

	if (bypass == 1) {
		regmap_update_bits(rt1320->regmap, RT1320_CAE_DATA_PATH,
			RT1320_CAE_POST_R_SEL_MASK | RT1320_CAE_POST_L_SEL_MASK | RT1320_CAE_WDATA_SEL_MASK,
			RT1320_CAE_POST_R_SEL_T7 | RT1320_CAE_POST_L_SEL_T3 | RT1320_CAE_WDATA_SEL_SRCIN);
		regmap_update_bits(rt1320->regmap, RT1320_DA_FILTER_DATA,
			RT1320_DA_FILTER_SEL_MASK, RT1320_DA_FILTER_SEL_CAE);
		rt1320->bypass_dsp = true;
	} else {
		regmap_update_bits(rt1320->regmap, RT1320_CAE_DATA_PATH,
			RT1320_CAE_POST_R_SEL_MASK | RT1320_CAE_POST_L_SEL_MASK | RT1320_CAE_WDATA_SEL_MASK,
			RT1320_CAE_POST_R_SEL_T7 | RT1320_CAE_POST_L_SEL_T3 | RT1320_CAE_WDATA_SEL_OUTB0);
		regmap_update_bits(rt1320->regmap, RT1320_DA_FILTER_DATA,
			RT1320_DA_FILTER_SEL_MASK, RT1320_DA_FILTER_SEL_OUTB1);
		rt1320->bypass_dsp = false;
	}

	return changed;
}

static const DECLARE_TLV_DB_SCALE(out_vol_tlv, -6525, 75, 0);
static const DECLARE_TLV_DB_SCALE(in_vol_tlv, -1725, 75, 0);

static int rt1320_dsp_fw_update_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	dev_dbg(component->dev, "-> %s\n", __func__);

	return 0;
}

static int rt1320_load_dsp_fw(struct rt1320_priv *rt1320)
{
	struct regmap *regmap = rt1320->regmap;
	struct device *dev = regmap_get_device(regmap);
	const struct firmware *firmware;
	const char *filename;
	unsigned int addr;
	int ret, i;

	dev_dbg(dev, "-> %s\n", __func__);

	regmap_update_bits(regmap, 0xf01e, 1 << 7, 0);

	/* 0x3fc000c0 */
	addr = 0x3fc000c0;
	filename = "realtek/rt1320/0x3fc000c0.dat";
	ret = request_firmware(&firmware, filename, dev);
	if (ret) {
		dev_err(dev, "%s request error\n", filename);
		ret = -ENOENT;
		goto _exit_;
	} else {
		dev_info(dev, "%s, size=%d\n", filename, firmware->size);
		ret = rt1320_spi_burst_write(addr, firmware->data,
			firmware->size);
		if (ret < 0) {
			dev_err(dev, "%s write error\n", filename);
			release_firmware(firmware);
			goto _exit_;
		}
	}
#ifdef DSP_FW_CHK
	ret = rt1320_dsp_fw_cmp(rt1320, addr, firmware->data, firmware->size);
#endif
	release_firmware(firmware);

	/* 0x3fc29d80 */
	addr = 0x3fc29d80;
	filename = "realtek/rt1320/0x3fc29d80.dat";
	ret = request_firmware(&firmware, filename, dev);
	if (ret) {
		dev_err(dev, "%s request error\n", filename);
		ret = -ENOENT;
		goto _exit_;
	} else {
		dev_info(dev, "%s, size=%d\n", filename, firmware->size);
		ret = rt1320_spi_burst_write(addr, firmware->data,
			firmware->size);
		if (ret < 0) {
			dev_err(dev, "%s write error\n", filename);
			release_firmware(firmware);
			goto _exit_;
		}
	}
#ifdef DSP_FW_CHK
	ret = rt1320_dsp_fw_cmp(rt1320, addr, firmware->data, firmware->size);
#endif
	release_firmware(firmware);

	/* 0x3fe00000 */
	addr = 0x3fe00000;
	filename = "realtek/rt1320/0x3fe00000.dat";
	ret = request_firmware(&firmware, filename, dev);
	if (ret) {
		dev_err(dev, "%s request error\n", filename);
		ret = -ENOENT;
		goto _exit_;
	} else {
		dev_info(dev, "%s, size=%d\n", filename, firmware->size);
		ret = rt1320_spi_burst_write(addr, firmware->data,
			firmware->size);
		if (ret < 0) {
			dev_err(dev, "%s write error\n", filename);
			release_firmware(firmware);
			goto _exit_;
		}
	}
#ifdef DSP_FW_CHK
	ret = rt1320_dsp_fw_cmp(rt1320, addr, firmware->data, firmware->size);
#endif
	release_firmware(firmware);

	/* 0x3fe02000 */
	addr = 0x3fe02000;
	filename = "realtek/rt1320/0x3fe02000.dat";
	ret = request_firmware(&firmware, filename, dev);
	if (ret) {
		dev_err(dev, "%s request error\n", filename);
		ret = -ENOENT;
		goto _exit_;
	} else {
		dev_info(dev, "%s, size=%d\n", filename, firmware->size);
		ret = rt1320_spi_burst_write(addr, firmware->data,
			firmware->size);
		if (ret < 0) {
			dev_err(dev, "%s write error\n", filename);
			release_firmware(firmware);
			goto _exit_;
		}
	}
#ifdef DSP_FW_CHK
	ret = rt1320_dsp_fw_cmp(rt1320, addr, firmware->data, firmware->size);
#endif
	release_firmware(firmware);

	/* load AFX0/1 FW */
	rt1320_afx_load(rt1320);
#if 1
	for (i = 0; i < 4; i++) {
		regmap_write(regmap, 0x3fc2bfc7 - i, 0x00);
		regmap_write(regmap, 0x3fc2bfcb - i, 0x00);
		regmap_write(regmap, 0x3fc2bf83 - i, 0x00);
	}

	for (i = 0; i < 4; i++)
		regmap_write(regmap, 0x3fc2bfc3 - i, ((i == 3) ? 0x0b : 0x00) );
#else
	regmap_write(rt1320->regmap, 0x3fc2bfc0, 0x0b);
	regmap_write(rt1320->regmap, 0xc081, 0xfc);
#endif
	printk("%s(%d) FW update end. \n", __func__, __LINE__);

	regmap_update_bits(regmap, RT1320_HIFI3_DSP_CTRL_2,
			RT1320_HIFI3_DSP_MASK, RT1320_HIFI3_DSP_RUN);
_exit_:
	if (ret)
		dev_err(dev, "%s: Load DSP FW failed\n", __func__);

	return ret;
}

static int rt1320_dsp_fw_update_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "-> %s, value=%u\n", __func__, ucontrol->value.bytes.data[0]);

	if (!ucontrol->value.bytes.data[0])
		return 0;

	return rt1320_load_dsp_fw(rt1320);
}

static int rt1320_kR0_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	dev_dbg(component->dev, "-> %s\n", __func__);

	return 0;
}

static int rt1320_kR0_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);
	int i, retry = 50;
	unsigned int val;

	dev_dbg(component->dev, "-> %s, value=%u\n", __func__, ucontrol->value.bytes.data[0]);

	if (!ucontrol->value.bytes.data[0])
		return 0;

	msleep(3000);

	for (i = 0; i < retry; i++) {
		regmap_write(rt1320->regmap, 0x3fc2ab80, 0x01);
		regmap_read(rt1320->regmap, 0x3fc2ab80, &val);
		regmap_write(rt1320->regmap, 0x3fc2ab90, 0x0b); // LCH
		regmap_read(rt1320->regmap, 0x3fc2ab90, &val);
		regmap_write(rt1320->regmap, 0x3fc2ab94, 0x40); // Struct Data Length
		regmap_read(rt1320->regmap, 0x3fc2ab94, &val);
		regmap_write(rt1320->regmap, 0x3fc2ab84, 0x48); // Struct + Cmd Length
		regmap_read(rt1320->regmap, 0x3fc2ab84, &val);
		regmap_write(rt1320->regmap, 0x3fc2ab81, 0x02); // Trigger to read data
		regmap_read(rt1320->regmap, 0x3fc2ab81, &val);
		if (val == 0)
			break;
		regmap_read(rt1320->regmap, 0x3fc2ab80, &val);
		regmap_read(rt1320->regmap, 0x3fc2ab90, &val);
		regmap_read(rt1320->regmap, 0x3fc2ab94, &val);
		regmap_read(rt1320->regmap, 0x3fc2ab84, &val);
		msleep(100);
	}

	regmap_read(rt1320->regmap, 0x3fc2aba8, &val);
	regmap_read(rt1320->regmap, 0x3fc2aba9, &val);
	regmap_read(rt1320->regmap, 0x3fc2abaa, &val);
	regmap_read(rt1320->regmap, 0x3fc2abab, &val);

	if (i == retry)
		dev_err(component->dev, "L R0 read failed\n");
	else
		dev_info(component->dev, "L R0 read succeeded\n");

	return 0;
}

static int rt1320_post_dgain_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);
	struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int vol = 0, rval, val_inter = 0x10;

	regmap_read(rt1320->regmap, RT1320_SPK_POST_GAIN_L_HI, &rval);
	vol = (rval & 0xf) << 8;
	regmap_read(rt1320->regmap, RT1320_SPK_POST_GAIN_L_LO, &rval);
	vol |= (rval & 0xff);
	ucontrol->value.integer.value[0] = mc->max - (0xfff - vol) / val_inter;

	regmap_read(rt1320->regmap, RT1320_SPK_POST_GAIN_R_HI, &rval);
	vol = (rval & 0xf) << 8;
	regmap_read(rt1320->regmap, RT1320_SPK_POST_GAIN_R_LO, &rval);
	vol |= (rval & 0xff);
	ucontrol->value.integer.value[1] = mc->max - (0xfff - vol) / val_inter;

	dev_dbg(component->dev, "%s, L=%ld, R=%ld\n", __func__,
		ucontrol->value.integer.value[0], ucontrol->value.integer.value[1]);

	return 0;
}

static int rt1320_post_dgain_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);
	struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int rval, val_inter = 0x10;

	dev_dbg(component->dev, "%s, L=%ld, R=%ld\n", __func__,
		ucontrol->value.integer.value[0], ucontrol->value.integer.value[1]);

	rval = (mc->max - ucontrol->value.integer.value[0]) * val_inter;
	rval = 0xfff - (rval & 0xfff);
	regmap_write(rt1320->regmap, RT1320_SPK_POST_GAIN_L_HI, (rval >> 8) & 0xf);
	regmap_write(rt1320->regmap, RT1320_SPK_POST_GAIN_L_LO, rval & 0xff);

	rval = (mc->max - ucontrol->value.integer.value[1]) * val_inter;
	rval = 0xfff - (rval & 0xfff);
	regmap_write(rt1320->regmap, RT1320_SPK_POST_GAIN_R_HI, (rval >> 8) & 0xf);
	regmap_write(rt1320->regmap, RT1320_SPK_POST_GAIN_R_LO, rval & 0xff);

	return 0;
}

static const struct snd_kcontrol_new rt1320_snd_controls[] = {

	SOC_ENUM_EXT("DSP Path Select", rt1320_dac_data_enum, rt1320_dsp_path_get,
		rt1320_dsp_path_put),

	SND_SOC_BYTES_EXT("DSP FW Update", 1, rt1320_dsp_fw_update_get,
		rt1320_dsp_fw_update_put),

	SOC_DOUBLE_EXT("Amp Playback Volume", SND_SOC_NOPM, 0, 1, 255, 0,
		rt1320_post_dgain_get, rt1320_post_dgain_put),

	SOC_DOUBLE("DMIX DAC Switch", 0xcd00, 4, 5, 1, 1),

	SND_SOC_BYTES_EXT("Get R0", 1, rt1320_kR0_get,
		rt1320_kR0_put),
};

static int rt1320_pdb_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s, event=%d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		// if (!rt1320->bypass_dsp)
		// 	regmap_update_bits(rt1320->regmap, RT1320_HIFI3_DSP_CTRL_2,
		// 			RT1320_HIFI3_DSP_MASK, RT1320_HIFI3_DSP_RUN);
		// regmap_update_bits(rt1320->regmap, 0xc044,
		// 	0xe0, 0x00);
		// regmap_update_bits(rt1320->regmap, RT1320_PDB_PIN_SET,
		// 	/*RT1320_PDB_PIN_SEL_MASK |*/ RT1320_PDB_PIN_MNL_MASK,
		// 	/*RT1320_PDB_PIN_SEL_MNL |*/ RT1320_PDB_PIN_MNL_ON);
		break;
	case SND_SOC_DAPM_POST_PMD:
		// if (!rt1320->bypass_dsp)
		// 	regmap_update_bits(rt1320->regmap, RT1320_HIFI3_DSP_CTRL_2,
		// 			RT1320_HIFI3_DSP_MASK, RT1320_HIFI3_DSP_STALL);
		// regmap_update_bits(rt1320->regmap, 0xc044,
		// 	0xe0, 0xe0);
		// regmap_update_bits(rt1320->regmap, RT1320_PDB_PIN_SET,
		// 	/*RT1320_PDB_PIN_SEL_MASK |*/ RT1320_PDB_PIN_MNL_MASK,
		// 	/*RT1320_PDB_PIN_SEL_MNL |*/ RT1320_PDB_PIN_MNL_OFF);
		break;
	default:
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget rt1320_dapm_widgets[] = {

	/* Audio Interface */
	SND_SOC_DAPM_AIF_IN("AIF1RX", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	/* Digital  Interface*/
#if 0
	SND_SOC_DAPM_SWITCH("OT23 L", SND_SOC_NOPM, 0, 0, &rt1320_spk_l_dac),
	SND_SOC_DAPM_SWITCH("OT23 R", SND_SOC_NOPM, 0, 0, &rt1320_spk_r_dac),
#endif
	SND_SOC_DAPM_PGA_E("CAE", SND_SOC_NOPM, 0, 0, NULL, 0,
		rt1320_pdb_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC("DAC DMIX L", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC DMIX R", NULL, SND_SOC_NOPM, 0, 0),

	/* Output */
	SND_SOC_DAPM_OUTPUT("SPOL"),
	SND_SOC_DAPM_OUTPUT("SPOR"),
};

static const struct snd_soc_dapm_route rt1320_dapm_routes[] = {
	{ "CAE", NULL, "AIF1RX" },
	{ "DAC DMIX L", NULL, "CAE" },
	{ "DAC DMIX R", NULL, "CAE" },
	{ "SPOL", NULL, "DAC DMIX L" },
	{ "SPOR", NULL, "DAC DMIX R" },
};

static int rt1320_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	// struct snd_soc_component *component = dai->component;
	// struct rt1320_priv *rt1320 =
	// 	snd_soc_component_get_drvdata(component);
	// int retval;
	// unsigned int sampling_rate;

	dev_dbg(dai->dev, "%s %s", __func__, dai->name);
#if 0
	/* sampling rate configuration */
	switch (params_rate(params)) {
	case 16000:
		sampling_rate = RT1320_SDCA_RATE_16000HZ;
		break;
	case 32000:
		sampling_rate = RT1320_SDCA_RATE_32000HZ;
		break;
	case 44100:
		sampling_rate = RT1320_SDCA_RATE_44100HZ;
		break;
	case 48000:
		sampling_rate = RT1320_SDCA_RATE_48000HZ;
		break;
	case 96000:
		sampling_rate = RT1320_SDCA_RATE_96000HZ;
		break;
	case 192000:
		sampling_rate = RT1320_SDCA_RATE_192000HZ;
		break;
	default:
		dev_err(component->dev, "%s: Rate %d is not supported\n",
			__func__, params_rate(params));
		return -EINVAL;
	}

	/* set sampling frequency */
	if (dai->id == RT1320_AIF1)
		regmap_write(rt1320->regmap,
			SDW_SDCA_CTL(FUNC_NUM_AMP, RT1320_SDCA_ENT_CS21, RT1320_SDCA_CTL_SAMPLE_FREQ_INDEX, 0),
			sampling_rate);
	else {
		regmap_write(rt1320->regmap,
			SDW_SDCA_CTL(FUNC_NUM_MIC, RT1320_SDCA_ENT_CS113, RT1320_SDCA_CTL_SAMPLE_FREQ_INDEX, 0),
			sampling_rate);
		regmap_write(rt1320->regmap,
			SDW_SDCA_CTL(FUNC_NUM_MIC, RT1320_SDCA_ENT_CS14, RT1320_SDCA_CTL_SAMPLE_FREQ_INDEX, 0),
			sampling_rate);
	}
#endif
	return 0;
}

static void rt1320_init(struct rt1320_priv *rt1320)
{
	/* Through DSP */
	regmap_write(rt1320->regmap, RT1320_CAE_DATA_PATH, 0xf3);
	regmap_write(rt1320->regmap, RT1320_DSP_DATA_INB01_PATH, 0x12);
	regmap_write(rt1320->regmap, RT1320_DA_FILTER_DATA, 0x05);
	rt1320->bypass_dsp = false;

	regmap_update_bits(rt1320->regmap, 0xc680, 0xb, 0xb);
}

static int rt1320_component_probe(struct snd_soc_component *component)
{
	// int ret;
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);
	unsigned int val;
	int ret, i, retry = 30;
	rt1320->component = component;

	dev_dbg(component->dev, "%s\n", __func__);

	/* initialization write */
	rt1320_init(rt1320);

	if (rt1320->version_id < RT1320_VC)
		; //rt1320_vab_preset(rt1320);
	else {
		dev_dbg(component->dev, "This is VC version!\n");
		ret = rt1320_vc_preset(rt1320);
		if (ret)
			return -EPROBE_DEFER;
	}

	regmap_read(rt1320->regmap, 0xc680, &val);

	ret = rt1320_load_dsp_fw(rt1320);
	if (ret)
		printk("%s: Load DSP FW failed\n", __func__);

	/* Get R0 */
#if 0
	regmap_update_bits(rt1320->regmap, 0xc044, 0xe0, 0x00);
	regmap_write(rt1320->regmap, 0xc570, 0x0b);
	regmap_write(rt1320->regmap, 0xcd00, 0xc5);
	msleep(3500);

	regmap_write(rt1320->regmap, 0x3fc2ab80, 0x01);
	regmap_read(rt1320->regmap, 0x3fc2ab80, &val);
	regmap_write(rt1320->regmap, 0x3fc2ab90, 0x0b); // LCH
	regmap_read(rt1320->regmap, 0x3fc2ab90, &val);
	regmap_write(rt1320->regmap, 0x3fc2ab94, 0x40); // Struct Data Length
	regmap_read(rt1320->regmap, 0x3fc2ab94, &val);
	regmap_write(rt1320->regmap, 0x3fc2ab84, 0x48); // Struct + Cmd Length
	regmap_read(rt1320->regmap, 0x3fc2ab84, &val);
	regmap_write(rt1320->regmap, 0x3fc2ab81, 0x02); // Trigger to read data
	for (i = 0; i < retry; i++) {
		regmap_read(rt1320->regmap, 0x3fc2ab81, &val);
		if (val == 0)
			break;
		regmap_read(rt1320->regmap, 0x3fc2ab80, &val);
		regmap_read(rt1320->regmap, 0x3fc2ab90, &val);
		regmap_read(rt1320->regmap, 0x3fc2ab94, &val);
		regmap_read(rt1320->regmap, 0x3fc2ab84, &val);
		msleep(100);
	}
	if (i == retry)
		dev_err(component->dev, "L R0 read failed\n");
	else
		dev_info(component->dev, "L R0 read succeeded\n");
#endif

	regmap_update_bits(rt1320->regmap, 0xc044, 0xe0, 0x0);

	return 0;
}

static const struct snd_soc_component_driver soc_component_rt1320 = {
	.probe = rt1320_component_probe,
	.controls = rt1320_snd_controls,
	.num_controls = ARRAY_SIZE(rt1320_snd_controls),
	.dapm_widgets = rt1320_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt1320_dapm_widgets),
	.dapm_routes = rt1320_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt1320_dapm_routes),
	.endianness = 1,
};

static const struct snd_soc_dai_ops rt1320_aif_dai_ops = {
	.hw_params = rt1320_hw_params,
};

#define RT1320_STEREO_RATES (SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define RT1320_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | \
	SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver rt1320_dai[] = {
	{
		.name = "rt1320-aif1",
		.id = RT1320_AIF1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT1320_STEREO_RATES,
			.formats = RT1320_FORMATS,
		},
		.ops = &rt1320_aif_dai_ops,
	},
};

#define RT1320_REG_DISP_LEN 16
static ssize_t rt1320_codec_show_range(struct rt1320_priv *rt1320,
	char *buf, int start, int end)
{
	unsigned int val;
	int cnt = 0, i;

	for (i = start; i <= end; i++) {
		if (cnt + RT1320_REG_DISP_LEN >= PAGE_SIZE)
			break;

		if (rt1320_readable_register(NULL, i)) {
			regmap_read(rt1320->regmap, i, &val);

			cnt += snprintf(buf + cnt, RT1320_REG_DISP_LEN,
					"%08x: %02x\n", i, val);
		}
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}

static ssize_t rt1320_codec_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct rt1320_priv *rt1320 = dev_get_drvdata(dev);
	ssize_t cnt;

	cnt = rt1320_codec_show_range(rt1320, buf, 0, 0xffff);

	return cnt;
}

static ssize_t rt1320_codec_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct rt1320_priv *rt1320 = dev_get_drvdata(dev);
	unsigned int val = 0, addr = 0;
	int i;

	pr_info("register \"%s\" count = %zu\n", buf, count);
	for (i = 0; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i)-'A') + 0xa);
		else
			break;
	}

	for (i = i + 1; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}

	pr_info("addr = 0x%08x val = 0x%02x\n", addr, val);
	if (/*addr > 0xffff ||*/ val > 0xff || val < 0)
		return count;

	if (i == count) {
		// rt1320_read(addr, &val);
		regmap_read(rt1320->regmap, addr, &val);
		pr_info("0x%08x = 0x%02x\n", addr, val);
	} else
		// rt1320_write(addr, val);
		regmap_write(rt1320->regmap, addr, val);

	return count;
}
static DEVICE_ATTR(codec_reg, 0644, rt1320_codec_show, rt1320_codec_store);

static const struct regmap_config rt1320_regmap = {
	.reg_bits = 32,
	.val_bits = 8,
	.readable_reg = rt1320_readable_register,
	.volatile_reg = rt1320_volatile_register,
	// .max_register = 0x3fb00810,
	// .max_register = 0x0000c068,
	// .max_register = 0x3fc2bfc7,
	// .max_register = 0x3fe36fff,
	// .max_register = 0x3fc2abd4,
	.max_register = 0x41181880,
	.reg_defaults = rt1320_regs,
	.num_reg_defaults = ARRAY_SIZE(rt1320_regs),
	.cache_type = REGCACHE_RBTREE,
	.use_single_rw = true,
};

static const struct i2c_device_id rt1320_i2c_id[] = {
	{ "rt1320" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt1320_i2c_id);

static const struct of_device_id rt1320_of_match[] = {
	{ .compatible = "realtek,rt1320", },
	{},
};
MODULE_DEVICE_TABLE(of, rt1320_of_match);

#ifdef CONFIG_ACPI
static const struct acpi_device_id rt1320_acpi_match[] = {
	{ "10EC1320", 0},
	{ },
};
MODULE_DEVICE_TABLE(acpi, rt1320_acpi_match);
#endif

static int rt1320_i2c_probe(struct i2c_client *i2c)
{
	struct rt1320_priv *rt1320;
	unsigned int val;
	int ret;

	dev_dbg(&i2c->dev, "%s, dev: %s\n", __func__, dev_name(&i2c->dev));
	rt1320 = devm_kzalloc(&i2c->dev, sizeof(struct rt1320_priv),
				GFP_KERNEL);
	if (!rt1320)
		return -ENOMEM;

	i2c_set_clientdata(i2c, rt1320);

	/* Regmap Initialization */
	rt1320->regmap = devm_regmap_init_i2c(i2c, &rt1320_regmap);
	if (IS_ERR(rt1320->regmap))
		return PTR_ERR(rt1320->regmap);

	dev_dbg(&i2c->dev, "regmap initialized\n");

	/* Reset */
	regmap_write(rt1320->regmap, 0xc000, 0x03);

	ret = regmap_read(rt1320->regmap, RT1320_DEV_VERSION_ID_1, &val);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to read version id: %d\n", ret);
		return ret;
	}
	rt1320->version_id = val;

	ret = device_create_file(&i2c->dev, &dev_attr_codec_reg);
	if (ret != 0) {
		dev_err(&i2c->dev,
			"Failed to create codec_reg sysfs files: %d\n", ret);
		return ret;
	}

	// regmap_read(rt1320->regmap, RT1320_CAE_DATA_PATH, &val);
	// regmap_read(rt1320->regmap, RT1320_DSP_DATA_INB01_PATH, &val);
	// regmap_read(rt1320->regmap, RT1320_DA_FILTER_DATA, &val);
	regmap_read(rt1320->regmap, 0xc680, &val);

	return devm_snd_soc_register_component(&i2c->dev,
		&soc_component_rt1320, rt1320_dai, ARRAY_SIZE(rt1320_dai));
}

static int rt1320_i2c_remove(struct i2c_client *i2c)
{
	dev_dbg(&i2c->dev, "%s\n", __func__);
	return 0;
}

static struct i2c_driver rt1320_i2c_driver = {
	.driver = {
		.name = "rt1320",
		// .pm = &rt1320_pm,
		.of_match_table = of_match_ptr(rt1320_of_match),
		.acpi_match_table = ACPI_PTR(rt1320_acpi_match),
	},
	.probe_new = rt1320_i2c_probe,
	.remove = rt1320_i2c_remove,
	.id_table = rt1320_i2c_id,
};
module_i2c_driver(rt1320_i2c_driver);

MODULE_DESCRIPTION("ASoC RT1320 driver");
MODULE_AUTHOR("Derek Fang <derek.fang@realtek.com>");
MODULE_LICENSE("GPL");
