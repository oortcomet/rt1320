// SPDX-License-Identifier: GPL-2.0-only
//
// rt1320-sdw.c -- rt1320 SDCA ALSA SoC amplifier audio driver
//
// Copyright(c) 2024 Realtek Semiconductor Corp.
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
#include <linux/math64.h>
#include "rt1320.h"
#include "rt1320-sdw.h"
#include "rt1320-spi.h"
#include "rt1320_bind_write_333_20.h"

// #define RT1320_I2C_FW_WR
// #define RT1320_I2C_FW_RD
struct file *log_fp = NULL;
const char *log_path = "/lib/firmware/rt1320_boot.log";
loff_t log_pos = 0;

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
	{ 0x0000e824, 0x7f },
	{ 0x0000e825, 0x7f },
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
	case 0x0000c480 ... 0x0000c48f:
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
	case 0x0000e824:
	case 0x0000e825:
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
#if 0
	case 0x3fc000c0 ... 0x3fc01110:
	case 0x3fc2ab80 ... 0x3fc2abd4:
	case 0x3fc2bf80 ... 0x3fc2bf83:
	case 0x3fc2bfc0 ... 0x3fc2bfc7:
	case 0x3fe2e000 ... 0x3fe2e003:
#else
	case 0x3fc00000 ... 0x3fe3b000: // DSP Memory
#endif
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
	case 0x0000c044:
	case 0x0000c400 ... 0x0000c40b:
	case 0x0000c480 ... 0x0000c483:
	case 0x0000c48c ... 0x0000c48f:
	case 0x0000c560:
	case 0x0000c570:
	case 0x0000c680:
	case 0x0000c900:
	case 0x0000d486:
	case 0x0000d487:
	case 0x1000cd91 ... 0x1000cd96:
	case 0x1000f008:
	case 0x0000f015:
	case 0x1000f021:
	case 0x0000f01c ... 0x0000f01f:
#if 0
	case 0x3fc000c0 ... 0x3fc01110:
	case 0x3fc29d80 ... 0x3fc2a254:
	case 0x3fc2ab80 ... 0x3fc2abd4:
	case 0x3fc2b780 ... 0x3fc2bdac:
	case 0x3fc2bf80 ... 0x3fc2bf83:
	case 0x3fc2bfc0 ... 0x3fc2bfc7:
	case 0x3fe00000 ... 0x3fe01ed4:
	case 0x3fe02000 ... 0x3fe0def0:
	case 0x3fe2e000 ... 0x3fe2e003:
#else
	case 0x3fc00000 ... 0x3fe3b000: // DSP Memory
#endif
		return true;
	default:
		return false;
	}

	return true;
}

/*
 * The 'patch code' is written to the patch code area.
 */
static void rt1320_load_mcu_patch(struct rt1320_priv *rt1320)
{
	struct regmap *regmap = rt1320->regmap;
	struct device *dev = regmap_get_device(regmap);
	const struct firmware *patch;
	const char *filename;
#define BIN_IS_BIG_ENDIAN
#if 1
	unsigned int addr, val;
	const unsigned char *ptr;
#endif
	int ret, i;

	if (rt1320->version_id <= RT1320_VB)
		filename = RT1320_VAB_MCU_PATCH;
	else
		filename = "rt1320/mcu_patch_333_20.bin";
		// filename = "mcu_patch_1119.bin";

	/* load the patch code here */
	ret = request_firmware(&patch, filename, dev);
	if (ret) {
		dev_err(dev, "%s: Failed to load %s firmware", __func__, filename);
#if 0
		regmap_write(rt1320->regmap, 0xc598, 0x00);
		regmap_write(rt1320->regmap, 0x10007000, 0x67);
		regmap_write(rt1320->regmap, 0x10007001, 0x80);
		regmap_write(rt1320->regmap, 0x10007002, 0x00);
		regmap_write(rt1320->regmap, 0x10007003, 0x00);
#endif
	} else {
#if 0
		for (i = 0; i < RT1320_MCU_PATCH_LEN; i++)
			regmap_write(rt1320->regmap, mcu_patch_code[i].reg, mcu_patch_code[i].def);
#else
		ptr = (const unsigned char *)patch->data;
		if ((patch->size % 8) == 0) {
			for (i = 0; i < patch->size; i += 8) {
#ifdef BIN_IS_BIG_ENDIAN
				addr = (ptr[i] & 0xff) << 24 | (ptr[i + 1] & 0xff) << 16 |
					(ptr[i + 2] & 0xff) << 8 | (ptr[i + 3] & 0xff);
				val = (ptr[i + 4] & 0xff) << 24 | (ptr[i + 5] & 0xff) << 16 |
					(ptr[i + 6] & 0xff) << 8 | (ptr[i + 7] & 0xff);
#else
				addr = (ptr[i] & 0xff) | (ptr[i + 1] & 0xff) << 8 |
					(ptr[i + 2] & 0xff) << 16 | (ptr[i + 3] & 0xff) << 24;
				val = (ptr[i + 4] & 0xff) | (ptr[i + 5] & 0xff) << 8 |
					(ptr[i + 6] & 0xff) << 16 | (ptr[i + 7] & 0xff) << 24;
#endif
				if (addr > 0x10007fff || addr < 0x10007000) {
					dev_err(dev, "%s: the address 0x%x is wrong", __func__, addr);
					goto _exit_;
				}
				if (val > 0xff) {
					dev_err(dev, "%s: the value 0x%x is wrong", __func__, val);
					goto _exit_;
				}
				regmap_write(rt1320->regmap, addr, val);
			}
		}
_exit_:
#endif
		release_firmware(patch);
	}
}

static void log_fp_write(char *str, int slen)
{
	int ret;
	char buf[100] = {0};

	memcpy(buf, str, slen);
	buf[slen] = '\n';

	if (log_fp && !IS_ERR(log_fp)) {
		ret = kernel_write(log_fp, buf, slen + 1, &log_pos);
		if (ret < 0)
			pr_err("write (%s) to log file failed: %d\n", str, ret);
	}
}

static void rt1320_vc_preset(struct rt1320_priv *rt1320)
{
	unsigned int i, reg, val, delay, retry, tmp;
	struct device *dev = regmap_get_device(rt1320->regmap);
	dev_dbg(dev, "-> %s\n", __func__);

	for (i = 0; i < RT1320_BIND_WRITE_LEN; i++) {
		reg = rt1320_bind_write[i].reg;
		val = rt1320_bind_write[i].def;
		delay = rt1320_bind_write[i].delay_us;

		if ((reg == 0x1000db00) && (val == 0x05)) {
			retry = 200;
#if 1
			while (retry) {
				regmap_read(rt1320->regmap, RT1320_KR0_INT_READY, &tmp);
				dev_dbg(dev, "%s, RT1320_KR0_INT_READY=0x%x, retry=%d\n", __func__, tmp, retry);
				if (tmp == 0x1f)
					break;
				usleep_range(1000, 1500);
				retry--;
			}
			if (!retry)
				dev_warn(dev, "%s MCU is NOT ready!", __func__);
#endif
		}

		regmap_write(rt1320->regmap, reg, val);
		if (delay)
			usleep_range(delay, delay + 1000);

		if (reg == 0x0000d486 && val == 0xc3) {
			dev_dbg(dev, "Load MCU patch start\n");
			rt1320_load_mcu_patch(rt1320);
			dev_dbg(dev, "Load MCU patch end\n");
		}
	}
}

static const char * const rt1320_dsp_ib0_select[] = {
	"DP1",
	"I2S",
	"SRCIN",
	"CAE32",
};

static SOC_ENUM_SINGLE_DECL(rt1320_dsp_ib0_enum,
	RT1320_DSP_DATA_INB01_PATH, RT1320_DSP_INB0_SEL_SFT, rt1320_dsp_ib0_select);
#if 0
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
	int ret;
#ifdef RT1320_I2C_FW_WR
	int i;
	for (i = 0; i < buf_size; i++)
		regmap_write(rt1320->regmap, start_addr + i, buf[i]);
#else // SPI
	ret = rt1320_spi_burst_write(start_addr, buf, buf_size);
	if (ret)
		dev_err(rt1320->component->dev,
			"%s: SPI write FW failed, ret=%d\n", __func__, ret);
#endif
	printk("%s, done\n", __func__);
}

static void rt1320_fw_param_read(struct rt1320_priv *rt1320,
	unsigned int start_addr, char *buf, unsigned int buf_size)
{
	int ret;
#ifdef RT1320_I2C_FW_RD
	int i;
	for (i = 0; i < buf_size; i++) {
		ret = regmap_read(rt1320->regmap, start_addr + i, (unsigned int *)&buf[i]);
		if (ret) {
			dev_err(rt1320->component->dev,
				"%s: I2C read FW failed, ret=%d\n", __func__, ret);
			break;
		}
	}
#else // SPI
	ret = rt1320_spi_burst_read(start_addr, (u8 *)buf, buf_size);
	if (ret) {
		dev_err(rt1320->component->dev,
			"%s: SPI read FW failed, ret=%d\n", __func__, ret);
	}
#endif
	printk("%s, done\n", __func__);
}

static int rt1320_dsp_fw_check(struct rt1320_priv *rt1320, unsigned int start_addr, const u8 *txbuf,
	unsigned int fw_size, bool dump_fw, bool compare)
{
	struct device *dev = regmap_get_device(rt1320->regmap);
	struct file *fp;
	loff_t pos = 0;
	int i, ret = 0, len;
	unsigned int val, done;
	const unsigned int count = 64;
	u8 *rxbuf = NULL;
	char dumpfile[100] = {0};
	if (dump_fw)
		sprintf(dumpfile, "/lib/firmware/rt1320/0x%08x.dump", start_addr);

	rxbuf = kmalloc(fw_size, GFP_KERNEL);
	if (!rxbuf) {
		pr_err("Can't create rxbuf!\n");
		return -ENOMEM;
	}

#ifdef RT1320_I2C_FW_RD
	// regcache_cache_bypass(rt1320->regmap, true);
	for(i = 0; i < fw_size; i++) {
		regmap_read(rt1320->regmap, start_addr + i, &val);
		rxbuf[i] = val;
	}
	// regcache_cache_bypass(rt1320->regmap, false);
#else
	rt1320_spi_burst_read(start_addr, rxbuf, fw_size);
#endif
	if (dump_fw) {
		fp = filp_open(dumpfile, O_WRONLY | O_CREAT, 0644);
		if (!IS_ERR(fp)) {
			done = 0;
			while (done < fw_size) {
				len = min(count, fw_size - done);
				ret = kernel_write(fp, &rxbuf[done], len, &pos);
				if (ret < 0) {
					dev_err(dev, "write %s error: %d\n", dumpfile, ret);
					break;
				}
				done += len;
			}
			if (ret >= 0)
				ret = 0;
		} else {
			dev_err(dev, "open %s error: %d\n", dumpfile, (int)PTR_ERR(fp));
			ret = (int)PTR_ERR(fp);
		}
	}

	if (compare && memcmp(txbuf, rxbuf, fw_size)){
		pr_err("%s: fw_addr:%x update fail!\n", __func__, start_addr);
		ret = -EINVAL;
	}

	kfree(rxbuf);
	if (dump_fw)
		filp_close(fp, NULL);

	return ret;
}

static int rt1320_afx_load(struct rt1320_priv *rt1320, unsigned char action)
{
	struct device *dev = regmap_get_device(rt1320->regmap);
	char afx_names[3][50] = {
			"rt1320/AFX0_Ram.bin",
			"rt1320/AFX1_Ram.bin",
			"rt1320/AFX1_Ram_RTLSM.bin" }; // modify the correct paths
	unsigned int afx_addrs[3] = {
			RT1320_AFX0_LOAD_ADDR,
			RT1320_AFX1_LOAD_ADDR,
			RT1320_AFXRTLSM_LOAD_ADDR };
	char hdr_start[] = "AFX";
	const struct firmware *fw = NULL;
	struct firmware fmw;
	int i, ret, hdr_size = 0;
	bool dump_fw = (action == 2 || action == 3) ? true : false;
	bool compare = (action == 3) ? true : false;

	for (i = 0; i < 3; i++) {

		ret = request_firmware(&fw, afx_names[i], dev);
		if (ret) {
			dev_err(dev, "Request firmware %s failed\n", afx_names[i]);
			ret = -ENOENT;
			goto out;
		}

		if (!fw->size) {
			dev_err(dev, "\"%s\" file read error\n", afx_names[i]);
			ret = -EINVAL;
			goto out;
		}

		if (memcmp(fw->data, hdr_start, sizeof(hdr_start)) == 0)
			hdr_size = 64; // The bin file has a header of 64 bytes
		else
			hdr_size = 0;
		dev_dbg(dev, "AFX%d FW_0x%08x, FW size is 0x%x, Extra Heder size is %d\n", i, afx_addrs[i],
			fw->size, hdr_size);

		fmw.size = fw->size - hdr_size;
		fmw.data = fw->data + hdr_size;

		rt1320_fw_param_write(rt1320, afx_addrs[i], fmw.data, fmw.size);

		if (dump_fw || compare) {
			if (rt1320_dsp_fw_check(rt1320, afx_addrs[i], fmw.data, fmw.size, dump_fw, compare))
				pr_err("%s %s failed!\n",
					afx_names[i], action == 2 ? "dump" : "update");
			else
				pr_err("%s %s succeeded!\n",
					afx_names[i], action == 2 ? "dump" : "update");
		}
	}
out:
	if (fw)
		release_firmware(fw);

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

static void rt1320_pr_read(struct rt1320_priv *rt1320, unsigned int reg, unsigned int *val);

static void rt1320_get_rsgain(struct rt1320_priv *rt1320, unsigned short *rs)
{
	struct snd_soc_component *component = rt1320->component;
	struct reg_default pr1058 = {0x1058, 0};
	struct reg_default pr1059 = {0x1059, 0};
	struct reg_default pr105a = {0x105a, 0};

	rt1320_pr_read(rt1320, pr1058.reg, &pr1058.def);
	rt1320_pr_read(rt1320, pr1059.reg, &pr1059.def);
	rt1320_pr_read(rt1320, pr105a.reg, &pr105a.def);

	dev_dbg(component->dev, "PR[%X %X %X] = {%02X, %02X, %02X}\n", pr1058.reg, pr1059.reg, pr105a.reg,
		pr1058.def & 0xff, pr1059.def & 0xff, pr105a.def & 0xff);

	rs[0] = (pr1059.def & 0x7f) << 2 | (pr105a.def & 0xc0) >> 6;
	rs[1] = (pr1058.def & 0xff) << 1 | (pr1059.def & 0x80) >> 7;
}

static u32 rt1320_rsgain_to_rsratio(struct rt1320_priv *rt1320, unsigned int rsgain)
{
	u64 base = 1000000000ULL;
	u32 step = 1960784;
	u32 tmp;

	if (rsgain == 0 || rsgain == 0x1ff)
		return div_u64(base, 1000);
	else if (rsgain & 0x100) {
		tmp = 0xff - (rsgain & 0xff);
		tmp = tmp * step;
		return div_u64(base + tmp, 1000);
	} else { // if ((rsgain & 0x100)==0)
		tmp = (rsgain & 0xff);
		tmp = tmp * step;
		return div_u64(base - tmp, 1000);
	}
}

static u32 rs_ratio_mx[2] = {0};
static int rt1320_load_dsp_fw(struct rt1320_priv *rt1320, unsigned char action)
{
	struct regmap *regmap = rt1320->regmap;
	struct device *dev = regmap_get_device(regmap);
	const struct firmware *firmware;
	int ret, i;
	unsigned short rs_gain[2] = {0};
	bool dump_fw = (action == 2 || action == 3) ? true : false;
	bool compare = (action == 3) ? true : false;

	printk("%s(%d) FW update start. \n", __func__, __LINE__);
	regmap_update_bits(rt1320->regmap, 0xf01e, 0x1, 0x1); // let DSP stall
	regmap_update_bits(rt1320->regmap, 0xf01e, (0x1 << 7), (0x0 << 7));

	if (log_fp)
		kernel_write(log_fp, "RT1320 DSP FW update start\n", 27, &log_pos);

	ret = request_firmware(&firmware, "rt1320/0x3fc000c0.dat", dev);
	if (ret == 0) {
		dev_info(dev, "%s: FW_0x3fc000c0 size=0x%x\n", __func__, firmware->size);
		rt1320_fw_param_write(rt1320, 0x3fc000c0, firmware->data, firmware->size);
		if (action == 2 || action == 3) {
			if (rt1320_dsp_fw_check(rt1320, 0x3fc000c0, firmware->data, firmware->size, dump_fw, compare))
				pr_err("%s: 0x3fc000c0 %s failed!\n",
					__func__, action == 2 ? "dump" : "update");
			else
				pr_err("%s: 0x3fc000c0 %s succeeded!\n",
					__func__, action == 2 ? "dump" : "update");
		}
		release_firmware(firmware);
	} else
		dev_err(dev, "%s: Failed to get firmware 0x3fc000c0\n", __func__);

	ret = request_firmware(&firmware, "rt1320/0x3fc29d80.dat", dev);
	if (ret == 0) {
		dev_info(dev, "%s: FW_0x3fc29d80 size=0x%x\n", __func__, firmware->size);
		rt1320_fw_param_write(rt1320, 0x3fc29d80, firmware->data, firmware->size);
		if (action == 2 || action == 3) {
			if (rt1320_dsp_fw_check(rt1320, 0x3fc29d80, firmware->data, firmware->size, dump_fw, compare))
				pr_err("%s: 0x3fc29d80 %s failed!\n",
					__func__, action == 2 ? "dump" : "update");
			else
				pr_err("%s: 0x3fc29d80 %s succeeded!\n",
					__func__, action == 2 ? "dump" : "update");
		}
		release_firmware(firmware);
	} else
		dev_err(dev, "%s: Failed to get firmware 0x3fc29d80\n", __func__);

	ret = request_firmware(&firmware, "rt1320/0x3fe00000.dat", dev);
	if (ret == 0) {
		dev_info(dev, "%s: FW_0x3fe00000 size=0x%x\n", __func__, firmware->size);
		rt1320_fw_param_write(rt1320, 0x3fe00000, firmware->data, firmware->size);
		if (action == 2 || action == 3) {
			if (rt1320_dsp_fw_check(rt1320, 0x3fe00000, firmware->data, firmware->size, dump_fw, compare))
				pr_err("%s: 0x3fe00000 %s failed!\n",
					__func__, action == 2 ? "dump" : "update");
			else
				pr_err("%s: 0x3fe00000 %s succeeded!\n",
					__func__, action == 2 ? "dump" : "update");
		}
		release_firmware(firmware);
	} else
		dev_err(dev, "%s: Failed to get firmware 0x3fe00000\n", __func__);

	ret = request_firmware(&firmware, "rt1320/0x3fe02000.dat", dev);
	if (ret == 0) {
		dev_info(dev, "%s: FW_0x3fe02000 size=0x%x\n", __func__, firmware->size);
		rt1320_fw_param_write(rt1320, 0x3fe02000, firmware->data, firmware->size);
		if (action == 2 || action == 3) {
			if (rt1320_dsp_fw_check(rt1320, 0x3fe02000, firmware->data, firmware->size, dump_fw, compare))
				pr_err("%s: 0x3fe02000 %s failed!\n",
					__func__, action == 2 ? "dump" : "update");
			else
				pr_err("%s: 0x3fe02000 %s succeeded!\n",
					__func__, action == 2 ? "dump" : "update");
		}
		release_firmware(firmware);
	} else
		dev_err(dev, "%s: Failed to get firmware 0x3fe02000\n", __func__);

	msleep(1000);
	// for (i = 0; i < 4; i++) {
	// 	regmap_write(rt1320->regmap, 0x3fc2bfc7 - i, 0x00);
	// 	regmap_write(rt1320->regmap, 0x3fc2bfcb - i, 0x00);
	// 	regmap_write(rt1320->regmap, 0x3fc2bf83 - i, 0x00);
	// }

	/* load AFX0/1 FW */
	rt1320_afx_load(rt1320, action);

	rt1320_get_rsgain(rt1320, rs_gain);
	rs_ratio_mx[0] = rt1320_rsgain_to_rsratio(rt1320, rs_gain[0]);
	rs_ratio_mx[1] = rt1320_rsgain_to_rsratio(rt1320, rs_gain[1]);
	dev_dbg(dev, "Rs Gain: [L]=0x%04X, [R]=0x%04X\n", rs_gain[0], rs_gain[1]);
	dev_dbg(dev, "Rs Ratio magnified a mega: [L]=%u, [R]=%u\n", rs_ratio_mx[0], rs_ratio_mx[1]);

	// for (i = 0; i < 4; i++)
	// 	regmap_write(rt1320->regmap, 0x3fc2bfc3 - i, ((i == 3) ? 0x0b : 0x00) );
	msleep(1000);
	regmap_write(rt1320->regmap, 0x3fc2bfc0, 0x0b);

	printk("%s(%d) FW update end. \n", __func__, __LINE__);
	if (log_fp)
		kernel_write(log_fp, "RT1320 DSP FW update end\n", 25, &log_pos);

	rt1320->fw_update = true;
	regmap_update_bits(rt1320->regmap, 0xc081, 0x3, 0x2); // set DSP clk from RC
	regmap_update_bits(rt1320->regmap, 0xf01e, 0x1, 0x0); // let DSP run

	return 0;
}

static int rt1320_i2c_read(void *context, unsigned int reg, unsigned int *val)
{
	struct i2c_client *client = context;
	struct rt1320_priv *rt1320 = i2c_get_clientdata(client);
	struct regmap *regmap_phy = rt1320->regmap_physical;
	struct device *dev = &client->dev;
	char log_str[32] = {0};

	if (rt1320_readable_register(dev, reg)) {
		regmap_read(regmap_phy, reg, val);
		if (log_fp && !IS_ERR(log_fp)) {
			sprintf(log_str, "%08X => %02X", reg, *val);
			log_fp_write(log_str, strlen(log_str));
		}
	} else
		dev_err(dev, "Not readable register %x\n", reg);

	return 0;
}

static int rt1320_i2c_write(void *context, unsigned int reg, unsigned int val)
{
	struct i2c_client *client = context;
	struct rt1320_priv *rt1320 = i2c_get_clientdata(client);
	struct regmap *regmap_phy = rt1320->regmap_physical;
	struct device *dev = &client->dev;
	int ret;
	char buf[17];

	// dev_info(dev, "%s, write reg %x, val %x\n", __func__, reg, val);
	ret = regmap_write(regmap_phy, reg, val);
	if (!ret) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "WrL1 %08X %02X", reg, val);
		buf[sizeof(buf)-1] = '\n';
		if (log_fp && !IS_ERR(log_fp)) {
			ret = kernel_write(log_fp, &buf, sizeof(buf), &log_pos);
			if (ret < 0)
				dev_err(dev, "write file %s failed: %d\n", log_path, ret);
		}
	}

	return 0;
}

static void rt1320_pr_read(struct rt1320_priv *rt1320, unsigned int reg, unsigned int *val)
{
	unsigned int byte3, byte2, byte1, byte0;

	regmap_write(rt1320->regmap, 0xc483, 0x80);
	regmap_write(rt1320->regmap, 0xc482, 0x40);
	regmap_write(rt1320->regmap, 0xc481, 0x0c);
	regmap_write(rt1320->regmap, 0xc480, 0x10);

	regmap_write(rt1320->regmap, 0xc487, ((reg & 0xff000000)>>24));
	regmap_write(rt1320->regmap, 0xc486, ((reg & 0x00ff0000)>>16));
	regmap_write(rt1320->regmap, 0xc485, ((reg & 0x0000ff00)>>8));
	regmap_write(rt1320->regmap, 0xc484, (reg & 0x000000ff));

	regmap_write(rt1320->regmap, 0xc482, 0xc0);

	regmap_read(rt1320->regmap, 0xc48f, &byte3);
	regmap_read(rt1320->regmap, 0xc48e, &byte2);
	regmap_read(rt1320->regmap, 0xc48d, &byte1);
	regmap_read(rt1320->regmap, 0xc48c, &byte0);

	*val = (byte3 << 24) | (byte2 << 16) | (byte1 << 8) | byte0;
}

static void rt1320_dump_regs(struct rt1320_priv *rt1320)
{
	struct device *dev = regmap_get_device(rt1320->regmap);
	struct file *fp;
	unsigned int i, val;
	unsigned int regs[] = {0xc044, 0xc560, 0xc570, 0xc5c2, 0xc5c3, 0xc5c4, 0xc5c8, 0xcd00, 0xd470, 0xf01e,
				0x3fc2bfc0, 0x3fc2bfc1, 0x3fc2bfc2, 0x3fc2bfc3, 0x3fc2bfc4,
				0x3fc000c0, 0x3fc29d80, 0x3fe00000, 0x3fe02000,
				RT1320_AFX0_LOAD_ADDR, RT1320_AFX1_LOAD_ADDR, RT1320_AFXRTLSM_LOAD_ADDR,
			};
	int size = ARRAY_SIZE(regs), ret;
	const char reg_dump_path[] = "/lib/firmware/rt1320/rt1320_regs_dump.txt";
	loff_t pos = 0;
	char buf[15];
	char buf1[15];

	dev_info(dev, "RT1320 dump registers\n");

	fp = filp_open(reg_dump_path, //Please modify to the correct path
		O_WRONLY | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		ret = PTR_ERR(fp);
		goto file_fail;
	}

	dev_info(dev, "open file %s\n", reg_dump_path);

	regcache_cache_bypass(rt1320->regmap, true);
	for (i = 0; i < size; i++) {
		if (rt1320_readable_register(NULL, regs[i])) {
			regmap_read(rt1320->regmap, regs[i], &val);
			memset(buf, 0, sizeof(buf));
			// snprintf(buf, 15, "%08x: %02x", regs[i], val);
			sprintf(buf, "%08x: %02x", regs[i], val);
			dev_dbg(dev, "%s, %08x: %02x\n", __func__, regs[i], val);
			if (!IS_ERR(fp)) {
				buf[14] = '\n';
				ret = kernel_write(fp, &buf, sizeof(buf), &pos);
				if (ret < 0)
					break;
			}

			if (log_fp && !IS_ERR(log_fp)) {
				memset(buf1, 0, sizeof(buf1));
				snprintf(buf1, sizeof(buf1), "%08X => %02X", regs[i], val);
				buf1[sizeof(buf1)-1] = '\n';
				ret = kernel_write(log_fp, &buf1, sizeof(buf1), &log_pos);
				if (ret < 0)
					dev_err(dev, "write to log file %s failed: %d\n", log_path, ret);
			}
		}
	}
	regcache_cache_bypass(rt1320->regmap, false);

file_fail:
	if (!IS_ERR(fp))
		filp_close(fp, NULL);
	if (ret < 0)
		dev_err(dev, "dump registers failed: %d\n", ret);
	else
		dev_info(dev, "dump registers done\n");
}

static int rt1320_dsp_fw_update_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int rt1320_dsp_fw_update_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);
	int ret;
	unsigned char action = ucontrol->value.bytes.data[0];

	if (!action)
		return 0;
	/* action:
	 * 1: write only
	 * 2: read and dump memorys
	 * 3: write and read compare
	 * 4: boot FW from ROM
	 * 5: Just set and run DSP
	 */

	if (action == 2) {
		rt1320_dump_regs(rt1320); // debug for getting R0 failed
		return 0;
	}

	if (action == 4) {
		// Noting to do
		return 0;
	}

	if (action == 5) {
		// set regs and run DSP
		// regmap_update_bits(rt1320->regmap, 0xc081, 0x1 << 1, 0x0 << 1);
		// regmap_update_bits(rt1320->regmap, 0xf01e, 0x1, 0x0);
		// regmap_update_bits(rt1320->regmap, 0xc044, 0xe0, 0x00);
		// regmap_update_bits(rt1320->regmap, RT1320_PDB_PIN_SET,
		// 	RT1320_PDB_PIN_SEL_MASK | RT1320_PDB_PIN_MNL_MASK,
		// 	RT1320_PDB_PIN_SEL_MNL | RT1320_PDB_PIN_MNL_ON);
		// regmap_write(rt1320->regmap, 0xc044, 0x1f);
		// regmap_write(rt1320->regmap, 0xcd00, 0xc5);

		return 0;
	}

	ret = rt1320_load_dsp_fw(rt1320, action);
	if (ret)
		dev_err(component->dev, "%s: Failed to load DSP firmwares!!\n", __func__);

	return 0;
}

static int rt1320_kR0_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	dev_dbg(component->dev, "-> %s\n", __func__);

	return 0;
}

static int rt1320_calc_caliR0(struct rt1320_priv *rt1320, unsigned char *data, int size, u32 *re, u32 *caliR0, int ch)
{
	struct snd_soc_component *component = rt1320->component;
	const unsigned int factor = 1 << 27;
	u64 val = 0;
	int i;
	unsigned int int_part = 0, decimal_1st = 0, decimal_2nd = 0, decimal_3rd = 0;
	const char chn[] = {'L', 'R'};

	if (size < 4) {
		pr_err("%s: Invalid data size!\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < size; i++)
		*re += ((data[i] & 0xff) << (i * 8));

	val = *re;
	int_part = val / factor;
	val %= factor;
	val *= 10;
	decimal_1st = val / factor;
	val %= factor;
	val *= 10;
	decimal_2nd = val / factor;
	val %= factor;
	val *= 10;
	decimal_3rd = val / factor;
	dev_dbg(component->dev, "%s, %cch Re = %u.%u%u%u ohm\n",
		__func__, chn[ch], int_part, decimal_1st, decimal_2nd, decimal_3rd);

	val = (*re) * 1000000ULL;
	val = div_u64(val, rs_ratio_mx[ch]);
	*caliR0 = val;

	int_part = val / factor;
	val %= factor;
	val *= 10;
	decimal_1st = val / factor;
	val %= factor;
	val *= 10;
	decimal_2nd = val / factor;
	val %= factor;
	val *= 10;
	decimal_3rd = val / factor;

	dev_info(component->dev, "%s, CaliR0 = %u.%u%u%u ohm\n",
		__func__, int_part, decimal_1st, decimal_2nd, decimal_3rd);

	return 0;
}

#define RT1320_FW_PARAM_ADDR	0x3fc2ab80
#define RT1320_CMD_ID 		0x3fc2ab81
#define RT1320_CMD_PARAM_ADDR	0x3fc2ab90

typedef enum {
	RT1320_FW_READY,
	RT1320_SET_PARAM,
	RT1320_GET_PARAM,
} rt1320_fw_cmdid;

static int rt1320_check_fw_ready(struct rt1320_priv *rt1320)
{
	unsigned int tmp, retry = 0;
	// check the value of RT1320_CMD_ID becomes to zero
	while (retry < 50) {
		regmap_read(rt1320->regmap, RT1320_CMD_ID, &tmp);
		if (tmp == 0)
			break;
		usleep_range(10000, 11000);
		retry++;
	}
	if (retry == 50) {
		dev_warn(regmap_get_device(rt1320->regmap), "%s FW is NOT ready!", __func__);
		return -ETIMEDOUT;
	}

	return 0;
}

static int rt1320_process_fw_param(struct rt1320_priv *rt1320, unsigned int cmdType, unsigned int paramId,
				unsigned char *param_buf, unsigned int param_size)
{
	struct device *dev = regmap_get_device(rt1320->regmap);
	int i, ret = 0;
	unsigned int cmdhdr_size = 8;
	unsigned int buf_size = param_size + cmdhdr_size;
	unsigned char *buf = kmalloc(buf_size, GFP_KERNEL);
	if (!buf) {
		dev_err(dev, "%s: Failed to allocate memory for buf!\n", __func__);
		ret = -ENOMEM;
		goto __exit__;
	}
	memset(buf, 0, buf_size);

	printk("%s: cmdType=%d, paramId=%d, param_size=%d\n", __func__, cmdType, paramId, param_size);

	buf[0] = paramId;
	buf[4] = param_size;

	// clear the parameters in the memory
	for (i = 0; i < buf_size; i++)
		regmap_write(rt1320->regmap, RT1320_CMD_PARAM_ADDR + i, 0);

	if (cmdType == RT1320_SET_PARAM) {
		memcpy(buf + cmdhdr_size, param_buf, param_size);
		// dev_dbg(dev, "%s: buf[]=", __func__);
		// for (i = 0; i < buf_size; i++)
		// 	printk(" %02X", buf[i]);
		// printk("\n");

		for (i = 0; i < buf_size / 4; i++)
			printk("%s SET: 0x%x : 0x%08x (%u)\n", __func__, RT1320_CMD_PARAM_ADDR + i * 4, *(unsigned int *)(buf + i * 4), *(unsigned int *)(buf + i * 4));
		rt1320_fw_param_write(rt1320, RT1320_CMD_PARAM_ADDR, (u8 *)buf, buf_size);

		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 4, buf_size);
		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 5, 0x00);
		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 6, 0x00);
		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 7, 0x00);

		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR, 0x01); // module ID
		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 1, 0x00);
		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 2, 0x00);
		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 3, 0x00);
		regmap_write(rt1320->regmap, RT1320_CMD_ID, cmdType);
		ret = rt1320_check_fw_ready(rt1320);
		if (ret < 0) {
			dev_err(dev, "%s: FW is NOT ready after setting param!\n", __func__);
			goto __exit__;
		}
	}
	if (cmdType == RT1320_GET_PARAM) {
		regmap_write(rt1320->regmap, RT1320_CMD_PARAM_ADDR, paramId);
		regmap_write(rt1320->regmap, RT1320_CMD_PARAM_ADDR + 4, param_size);

		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 4, buf_size);
		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 5, 0x00);
		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 6, 0x00);
		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 7, 0x00);

		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR, 0x01); // module ID
		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 1, 0x00);
		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 2, 0x00);
		regmap_write(rt1320->regmap, RT1320_FW_PARAM_ADDR + 3, 0x00);
		regmap_write(rt1320->regmap, RT1320_CMD_ID, cmdType);

		ret = rt1320_check_fw_ready(rt1320);
		if (ret < 0) {
			dev_err(dev, "%s: FW is NOT ready before getting param!\n", __func__);
			goto __exit__;
		}
		rt1320_fw_param_read(rt1320, RT1320_CMD_PARAM_ADDR, (u8 *)buf, buf_size);
		memcpy(param_buf, buf + cmdhdr_size, param_size);

		for (i = 0; i < buf_size / 4; i++)
			printk("%s GET: 0x%x : 0x%08x (%u)\n", __func__, RT1320_CMD_PARAM_ADDR + i * 4, *(unsigned int *)(buf + i * 4), *(unsigned int *)(buf + i * 4));
	}

__exit__:
	kfree(buf);
	return ret;
}

static int rt1320_set_R0(struct rt1320_priv *rt1320, unsigned char *r0_data, int size)
{
	unsigned int params[2][8] = {0};
	unsigned int paramId;
	int i, ch, ret;

	if (size != 8) {
		pr_err("%s: Invalid R0 data size! Need 8 bytes\n", __func__);
		return -EINVAL;
	}

	for (ch = 0; ch < 2; ch++) {
		if (ch == 0)
			paramId = 0x06; // LCH
		else
			paramId = 0x07; // RCH

		ret = rt1320_process_fw_param(rt1320, RT1320_GET_PARAM, paramId, (unsigned char *)params[ch], sizeof(params[ch]));
		if (ret < 0) {
			pr_err("%s: Failed to process FW param!\n", __func__);
			return ret;
		}

		for (i = 0; i < ARRAY_SIZE(params[ch]); i++) {
			printk("%s: %cch params[%d]=%08X\n", __func__, (ch == 0)? 'L':'R', i, params[ch][i]);
		}

		// Modify the struct of params and Write them back
		params[ch][0] = 0; // Enable channel protection
		params[ch][1] = r0_data[0] | (r0_data[1] << 8) |
				(r0_data[2] << 16) | (r0_data[3] << 24); // R0 value

		ret = rt1320_process_fw_param(rt1320, RT1320_SET_PARAM, paramId,
					(u8 *)params[ch], sizeof(params[ch]));
		if (ret < 0) {
			pr_err("%s: Failed to set ch%d R0 data!\n", __func__, ch);
			return ret;
		}
	}
	return 0;
}

static int rt1320_set_R0_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);

	dev_dbg(component->dev, "-> %s\n", __func__);

	return 0;
}

typedef union {
	unsigned char u8[4];
	unsigned int u32;
} param;

static void rt1320_set_advanceMode(struct rt1320_priv *rt1320)
{
	struct snd_soc_component *component = rt1320->component;

	if (!rt1320->advGain[0] || !rt1320->advGain[1]) {
		dev_err(component->dev, "Advance mode not supported\n");
		return;
	}

	snd_soc_component_write(component, RT1320_SPK_POST_GAIN_L_HI, (rt1320->advGain[0] >> 8) & 0xf);
	snd_soc_component_write(component, RT1320_SPK_POST_GAIN_L_LO, rt1320->advGain[0] & 0xff);
	snd_soc_component_write(component, RT1320_SPK_POST_GAIN_R_HI, (rt1320->advGain[1] >> 8) & 0xf);
	snd_soc_component_write(component, RT1320_SPK_POST_GAIN_R_LO, rt1320->advGain[1] & 0xff);
}

static void rt1320_calibrate(struct rt1320_priv *rt1320, u8 *quirk_data, int quirk_size)
{
#define NUM_READ_PARAM 16 // Number of parameters to read for R0 calibration
	struct snd_soc_component *component = rt1320->component;
	int ch, ret, retry;
	unsigned char r0_data[8] = {0}; // [0-3] = Lch R0 value, [4-7] = Rch R0 value
	const char chn[2] = {'L', 'R'};
	param params[NUM_READ_PARAM] = {0};
	unsigned int param_id;
	unsigned int buf_size = 4 * NUM_READ_PARAM; // Param struct size in bytes
	unsigned int vol_reg[4] = {0};
	u32 re[2] = {0}, caliR0[2] = {0};
	const u32 factor = 1 << 27;
	u64 caliR0_min[2] = {0}, caliR0_max[2] = {0};

	while (!component->card->instantiated) {
		dev_err(component->dev, "Card is not instantiated yet!\n");
		usleep_range(10000, 11000);
	}

	snd_soc_dapm_mutex_lock(&component->dapm);
	if (!rt1320->fw_update) {
		dev_err(component->dev, "DSP firmware is not updated yet!\n");
		return;
	}

	// set volume 0dB
	regmap_read(rt1320->regmap, 0xdd0b, &vol_reg[3]);
	regmap_read(rt1320->regmap, 0xdd0a, &vol_reg[2]);
	regmap_read(rt1320->regmap, 0xdd09, &vol_reg[1]);
	regmap_read(rt1320->regmap, 0xdd08, &vol_reg[0]);
	regmap_write(rt1320->regmap, 0xdd0b, 0x0f);
	regmap_write(rt1320->regmap, 0xdd0a, 0xff);
	regmap_write(rt1320->regmap, 0xdd09, 0x0f);
	regmap_write(rt1320->regmap, 0xdd08, 0xff);

	msleep(5000);

	// Get MeanR0 and AdvanceGain values
	for (ch = 0; ch < 2; ch++) {
		if (ch == 0)
			param_id = 0x06; // LCH
		else
			param_id = 0x07; // RCH
		ret = rt1320_process_fw_param(rt1320, RT1320_GET_PARAM, param_id,
						(unsigned char *)params, buf_size);
		if (ret < 0) {
			dev_err(component->dev, "%cch: Read params failed: %d\n", chn[ch], ret);
			rt1320->calib_result = 0;
			goto cali_exit;
		}
		rt1320->meanR0[ch] = params[2].u32 / factor; // Get the meanR0 value
		dev_dbg(component->dev, "%cch MeanR0: %02X %02X %02X %02X,(%u)\n",
			chn[ch], params[2].u8[0], params[2].u8[1], params[2].u8[2], params[2].u8[3],
			rt1320->meanR0[ch]);

		rt1320->advGain[ch] = params[3].u32; // Get the advGain value
		dev_dbg(component->dev, "%cch advance gain: %04X\n", chn[ch], rt1320->advGain[ch]);
	}

	if (!quirk_data || quirk_size != 8) {
		dev_info(component->dev, "Quirk data is missing => Get R0 and calibrate\n");
		// Get Re & CaliR0 values
		for (retry = 0; retry < 1; retry++) { // loop for monitor R0
			for (ch = 0; ch < 2; ch++) {
				if (ch == 0)
					param_id = 0x0b; // LCH
				else
					param_id = 0x0c; // RCH

				ret = rt1320_process_fw_param(rt1320, RT1320_GET_PARAM, param_id,
						(unsigned char *)params, buf_size);
				if (ret < 0) {
					dev_err(component->dev, "%cch: Read param R0 failed: %d\n", chn[ch], ret);
					rt1320->calib_result = 0;
					goto cali_exit;
				} else {
					dev_info(component->dev, "%cch: Read param R0 succeeded. retry = %d\n", chn[ch], retry);
					memcpy(r0_data + ch * 4, &params[4].u8[0], 4);
				}

				ret = rt1320_calc_caliR0(rt1320, r0_data + ch * 4, sizeof(param), &re[ch], &caliR0[ch], ch);
				if (ret < 0) {
					dev_err(component->dev, "%cch: Calculate CaliR0 failed: %d\n", chn[ch], ret);
					rt1320->calib_result = 0;
					goto cali_exit;
				}

				pr_info("%cch R0 = { %02X %02X %02X %02X }\n", chn[ch],
					re[ch] & 0xff, (re[ch] >> 8) & 0xff, (re[ch] >> 16) & 0xff, (re[ch] >> 24) & 0xff);

				caliR0_min[ch] = rt1320->meanR0[ch] * factor * 85ULL;
				caliR0_max[ch] = rt1320->meanR0[ch] * factor * 115ULL;
			}
			usleep_range(100000, 110000);
		}
		dev_dbg(component->dev, "CaliR0: [L]=%llu, [R]=%llu\n", caliR0[0] * 100ULL, caliR0[1] * 100ULL);
		dev_dbg(component->dev, "CaliR0 normal range: [L]=%llu ~ %llu, [R]=%llu ~ %llu\n", caliR0_min[0], caliR0_max[0], caliR0_min[1], caliR0_max[1]);

		if (caliR0[0] * 100ULL <= caliR0_min[0] ||
		    caliR0[0] * 100ULL >= caliR0_max[0] ||
		    caliR0[1] * 100ULL <= caliR0_min[1] ||
		    caliR0[1] * 100ULL >= caliR0_max[1]) {
			dev_dbg(component->dev, "CaliR0 is out of the tolerance => Basic Mode\n");
			rt1320->calib_result = 1; // Set to basic mode
		} else {
			dev_dbg(component->dev, "CaliR0 is within the tolerance => Advance Mode\n");
			rt1320->calib_result = 2; // Set to advance mode
		}
	}

	if (rt1320->calib_result == 1) {
		// Set BasicGain mode
		dev_dbg(component->dev, "Set Basic Mode\n");
		// rt1320_set_basicMode(rt1320);
	} else {
		// Set AdvanceGain mode
		dev_dbg(component->dev, "Set Advance Mode\n");
		rt1320_set_advanceMode(rt1320);
		// Set R0 and enable protect
		dev_dbg(component->dev, "Set R0 and enable protect\n");
		if (quirk_data && quirk_size == 8) {
			dev_dbg(component->dev, "Use quirk data to set R0\n");
			ret = rt1320_set_R0(rt1320, quirk_data, quirk_size);
		} else {
			dev_dbg(component->dev, "Use read-back R0 data to set R0\n");
			ret = rt1320_set_R0(rt1320, r0_data, sizeof(r0_data));
		}
		if (ret < 0) {
			dev_err(component->dev, "Failed to set R0 data: %d\n", ret);
			rt1320->calib_result = 0;
			goto cali_exit;
		}
	}

cali_exit:
	// Restore volume
	regmap_write(rt1320->regmap, 0xdd0b, vol_reg[3]);
	regmap_write(rt1320->regmap, 0xdd0a, vol_reg[2]);
	regmap_write(rt1320->regmap, 0xdd09, vol_reg[1]);
	regmap_write(rt1320->regmap, 0xdd08, vol_reg[0]);

	if (rt1320->calib_result > 0)
		dev_info(component->dev, "RT1320 calibration done");
	else
		dev_err(component->dev, "RT1320 calibration failed");

	snd_soc_dapm_mutex_unlock(&component->dapm);

	if (log_fp)
		kernel_write(log_fp, "RT1320 get R0 end\n", 18, &log_pos);
}

static int rt1320_set_R0_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);
	unsigned char r0_data[8] = {0}; // [0-3] = Lch R0 value, [4-7] = Rch R0 value
	int i;

	dev_dbg(component->dev, "%s, R0 data[]=", __func__);
	for (i = 0; i < ARRAY_SIZE(r0_data); i++) {
		r0_data[i] = ucontrol->value.bytes.data[i] & 0xff;
		dev_dbg(component->dev, " %02X", r0_data[i]);
	}
	dev_dbg(component->dev, "\n");

	rt1320_calibrate(rt1320, r0_data, ARRAY_SIZE(r0_data));

	return 0;
}

static int rt1320_kR0_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "-> %s, value=%u\n", __func__, ucontrol->value.bytes.data[0]);

	if (!ucontrol->value.bytes.data[0])
		return 0;

	if (ucontrol->value.bytes.data[0] == 1) {
		// dump registers to check pilot tone
		rt1320_dump_regs(rt1320);
		return 0;
	}

	rt1320_calibrate(rt1320, NULL, 0);

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

static int rt1320_lpk_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int rt1320_lpk_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.bytes.data[0])
		regmap_write(rt1320->regmap, 0xc5d3, 0x09);
	else
		regmap_write(rt1320->regmap, 0xc5d3, 0x05);

	return 0;
}

static const DECLARE_TLV_DB_SCALE(cae_tlv, -9525, 75, 0);

static const struct snd_kcontrol_new rt1320_snd_controls[] = {

	SOC_ENUM("DSP IB0 Sel", rt1320_dsp_ib0_enum),
	SOC_ENUM_EXT("DSP Path Select", rt1320_dac_data_enum, rt1320_dsp_path_get,
		rt1320_dsp_path_put),
	SND_SOC_BYTES_EXT("DSP FW Update", 1, rt1320_dsp_fw_update_get,
		rt1320_dsp_fw_update_put),
	SOC_DOUBLE_EXT("Amp Playback Volume", SND_SOC_NOPM, 0, 1, 255, 0,
		rt1320_post_dgain_get, rt1320_post_dgain_put),

	SND_SOC_BYTES_EXT("RT1320 Get R0", 1, rt1320_kR0_get, rt1320_kR0_put),
	SND_SOC_BYTES_EXT("RT1320 Set R0", 8, rt1320_set_R0_get, rt1320_set_R0_put),
	SND_SOC_BYTES_EXT("Enable Loopback", 1, rt1320_lpk_get, rt1320_lpk_put),
	SOC_SINGLE("MS R Switch", RT1320_CAE_R_CTRL, 7,
		1, 1),
	SOC_SINGLE("MS L Switch", RT1320_CAE_L_CTRL, 7,
		1, 1),
	SOC_SINGLE_TLV("MS R Volume", RT1320_CAE_R_CTRL,
		0, 127, 0, cae_tlv),
	SOC_SINGLE_TLV("MS L Volume", RT1320_CAE_L_CTRL,
		0, 127, 0, cae_tlv),
};

static int rt1320_pdb_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);
	unsigned int val, val2;

	regmap_read(rt1320->regmap, 0xf01e, &val);

	dev_dbg(component->dev, "%s, event=%d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		regmap_update_bits(rt1320->regmap, 0xc044, 0xe0, 0x00);
		regmap_update_bits(rt1320->regmap, RT1320_PDB_PIN_SET,
			RT1320_PDB_PIN_SEL_MASK | RT1320_PDB_PIN_MNL_MASK,
			RT1320_PDB_PIN_SEL_MNL | RT1320_PDB_PIN_MNL_ON);
		regmap_update_bits(rt1320->regmap, 0xcd00, 0x30, 0x0);
		break;

	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(rt1320->regmap, 0xc044, 0xe0, 0xe0);
		regmap_update_bits(rt1320->regmap, RT1320_PDB_PIN_SET,
			RT1320_PDB_PIN_SEL_MASK | RT1320_PDB_PIN_MNL_MASK,
			RT1320_PDB_PIN_SEL_MNL | RT1320_PDB_PIN_MNL_OFF);
		break;
	default:
		break;
	}

	return 0;
}

// static const struct snd_kcontrol_new rt1320_spk_l_dac =
// 	SOC_DAPM_SINGLE_AUTODISABLE("Switch",
// 		0xcd00, 4, 1, 1);
// static const struct snd_kcontrol_new rt1320_spk_r_dac =
// 	SOC_DAPM_SINGLE_AUTODISABLE("Switch",
// 		0xcd00, 5, 1, 1);

static const struct snd_soc_dapm_widget rt1320_dapm_widgets[] = {

	/* Audio Interface */
	SND_SOC_DAPM_AIF_IN("AIF1RX", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	/* Output */
	SND_SOC_DAPM_PGA_E("CAE", SND_SOC_NOPM, 0, 0, NULL, 0,
		rt1320_pdb_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	// SND_SOC_DAPM_SWITCH("DAC DMIX L", SND_SOC_NOPM, 0, 0, &rt1320_spk_l_dac),
	// SND_SOC_DAPM_SWITCH("DAC DMIX R", SND_SOC_NOPM, 0, 0, &rt1320_spk_r_dac),
	SND_SOC_DAPM_DAC("DAC DMIX L", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC DMIX R", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_OUTPUT("SPOL"),
	SND_SOC_DAPM_OUTPUT("SPOR"),
};

static const struct snd_soc_dapm_route rt1320_dapm_routes[] = {
	{ "CAE", NULL, "AIF1RX" },
	// { "DAC DMIX L", "Switch", "CAE" },
	// { "DAC DMIX R", "Switch", "CAE" },
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

static int rt1320_component_probe(struct snd_soc_component *component)
{
	// int ret;
	struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);

	rt1320->component = component;

	dev_dbg(component->dev, "%s\n", __func__);
	rt1320_vc_preset(rt1320);
	// regmap_update_bits(rt1320->regmap, 0xf01e, (0x1 << 7), (0x1 << 7));

	// rt1320_load_dsp_fw(rt1320, 3);
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

static int rt1320_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	// struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s %s\n", __func__, dai->name);

	return 0;
}

static void rt1320_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	// struct rt1320_priv *rt1320 = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s %s\n", __func__, dai->name);
}

static const struct snd_soc_dai_ops rt1320_aif_dai_ops = {
	.hw_params = rt1320_hw_params,
	.startup = rt1320_startup,
	.shutdown = rt1320_shutdown,
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

	if (addr > 0xffffffff || val > 0xff)
		return count;

	printk("%s, i = %d, count = %zu, addr = 0x%08x, val = 0x%02x\n", __func__, i, count, addr, val);
	if (i == count) {
		// rt1320_read(addr, &val);
		regmap_read(rt1320->regmap, addr, &val);
		pr_info("0x%08x = 0x%02x\n", addr, val);
	} else {
		// rt1320_write(addr, val);
		regmap_write(rt1320->regmap, addr, val);
	}

	return count;
}
static DEVICE_ATTR(codec_reg, 0644, rt1320_codec_show, rt1320_codec_store);

static ssize_t rt1320_dsp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct rt1320_priv *rt1320 = dev_get_drvdata(dev);

	pr_info("%d\n", rt1320->fw_update);

	return 1;
}

static ssize_t rt1320_dsp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return 0;
}
static DEVICE_ATTR(dsp, 0444, rt1320_dsp_show, rt1320_dsp_store);

static const struct regmap_config rt1320_regmap_physical = {
	.name = "physical",
	.reg_bits = 32,
	.val_bits = 8,
	.readable_reg = rt1320_readable_register,
	.max_register = 0x41181880,
	.cache_type = REGCACHE_NONE,
	.use_single_rw = true,
};

static const struct regmap_config rt1320_regmap = {
	.reg_bits = 32,
	.val_bits = 8,
	.readable_reg = rt1320_readable_register,
	.volatile_reg = rt1320_volatile_register,
	.max_register = 0x41181880,
	.reg_defaults = rt1320_regs,
	.num_reg_defaults = ARRAY_SIZE(rt1320_regs),
	.cache_type = REGCACHE_RBTREE,
	.reg_write = rt1320_i2c_write,
	.reg_read = rt1320_i2c_read,
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

static void rt1320_calib_handler(struct work_struct *work)
{
	struct rt1320_priv *rt1320 = container_of(work, struct rt1320_priv,
		calib_work.work);

	while (!rt1320->component->card->instantiated) {
		pr_debug("%s\n", __func__);
		usleep_range(10000, 15000);
	}

	// rt1320_calibrate(rt1320);
}

static void rt1320_init(struct rt1320_priv *rt1320)
{
	/* Through DSP */
	regmap_write(rt1320->regmap, RT1320_CAE_DATA_PATH, 0xf3);
	regmap_write(rt1320->regmap, RT1320_DSP_DATA_INB01_PATH, 0x12);
	regmap_write(rt1320->regmap, RT1320_DA_FILTER_DATA, 0x05);
	rt1320->bypass_dsp = false;

	regmap_update_bits(rt1320->regmap, 0xc680, 0xb, 0xb);

	rt1320->calib_result = 0;
	rt1320->fw_update = false;
}

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
	rt1320->regmap_physical = devm_regmap_init_i2c(i2c, &rt1320_regmap_physical);
	if (IS_ERR(rt1320->regmap_physical))
		return PTR_ERR(rt1320->regmap_physical);
	dev_dbg(&i2c->dev, "regmap_physical initialized\n");

	rt1320->regmap = devm_regmap_init(&i2c->dev, NULL, i2c, &rt1320_regmap);
	if (IS_ERR(rt1320->regmap))
		return PTR_ERR(rt1320->regmap);
	dev_dbg(&i2c->dev, "regmap initialized\n");

	log_fp = filp_open(log_path, O_WRONLY | O_CREAT, 0644);
	if (IS_ERR(log_fp)) {
		dev_err(&i2c->dev, "open file %s failed: %ld\n", log_path, PTR_ERR(log_fp));
		return -EPROBE_DEFER;
	} else
		dev_info(&i2c->dev, "open file %s\n", log_path);

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

	ret = device_create_file(&i2c->dev, &dev_attr_dsp);
	if (ret != 0) {
		dev_err(&i2c->dev,
			"Failed to create dsp sysfs files: %d\n", ret);
		return ret;
	}

	regmap_read(rt1320->regmap, 0xc680, &val);

	/* initialization write */
	if (rt1320->version_id < RT1320_VC)
		; //rt1320_vab_preset(rt1320);
	else {
		dev_dbg(&i2c->dev, "This is VC version!\n");
	}

	rt1320_init(rt1320);
	INIT_DELAYED_WORK(&rt1320->calib_work, rt1320_calib_handler);

	regmap_read(rt1320->regmap, 0xc680, &val);

	return devm_snd_soc_register_component(&i2c->dev,
		&soc_component_rt1320, rt1320_dai, ARRAY_SIZE(rt1320_dai));
}

static struct i2c_driver rt1320_i2c_driver = {
	.driver = {
		.name = "rt1320",
		.of_match_table = of_match_ptr(rt1320_of_match),
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(rt1320_acpi_match),
#endif
	},
	.probe_new = rt1320_i2c_probe,
	.id_table = rt1320_i2c_id,
};
module_i2c_driver(rt1320_i2c_driver);

MODULE_DESCRIPTION("ASoC RT1320 driver");
MODULE_AUTHOR("Derek Fang <derek.fang@realtek.com>");
MODULE_LICENSE("GPL");
