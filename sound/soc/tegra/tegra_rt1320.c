// SPDX-License-Identifier: GPL-2.0-only
/*
* tegra_rt1320.c - Tegra machine ASoC driver for boards using RT1320 codec.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Based on code copyright/by:
 *
 * Copyright (C) 2010-2012 - NVIDIA, Inc.
 * Copyright (C) 2011 The AC100 Kernel Team <ac100@lists.lauchpad.net>
 * (c) 2009, 2010 Nvidia Graphics Pvt. Ltd.
 * Copyright 2007 Wolfson Microelectronics PLC.
 */
#define DEBUG
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/input.h>

#include "../codecs/rt1320.h"

#include "tegra_asoc_utils.h"

#define DRV_NAME "tegra-snd-rt1320-dev"

struct tegra_rt1320 {
	struct tegra_asoc_utils_data util_data;
	int gpio_hp_det;
	enum of_gpio_flags gpio_hp_det_flags;
};

static int tegra_rt1320_asoc_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	struct tegra_rt1320 *machine = snd_soc_card_get_drvdata(card);
	int srate, mclk, sysclk;
	int err;

	srate = params_rate(params);
	mclk = 256 * srate;
	sysclk = 512 * srate;

	err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

#if 1
	// dev_dbg(card->dev, "set codec_dai sysclk\n");
	// err = snd_soc_dai_set_sysclk(codec_dai, RT1320_SCLK_S_PLL1, sysclk,
	// 				SND_SOC_CLOCK_IN);
	// if (err < 0) {
	// 	dev_err(card->dev, "codec_dai clock not set\n");
	// 	return err;
	// }

	// dev_dbg(card->dev, "set codec_dai pll\n");
	// err = snd_soc_dai_set_pll(codec_dai, RT1320_PLLA, RT1320_PLL1_S_MCLK, mclk,
	// 				sysclk);
	// if (err < 0) {
	// 	dev_err(card->dev, "codec_dai pll not set\n");
	// 	return err;
	// }
#else
	dev_dbg(card->dev, "set codec_dai sysclk\n");
	err = snd_soc_dai_set_sysclk(codec_dai, RT1320_SCLK_S_MCLK, mclk,
					SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return err;
	}
#endif

	return 0;
}

static const struct snd_soc_ops tegra_rt1320_ops = {
	.hw_params = tegra_rt1320_asoc_hw_params,
};

static const struct snd_soc_dapm_widget tegra_rt1320_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Speakers", NULL),
};

static const struct snd_soc_dapm_route tegra_rt1320_dapm_routes[] = {
	{"Speakers", NULL, "SPOL"},
	{"Speakers", NULL, "SPOR"},
};

static const struct snd_kcontrol_new tegra_rt1320_controls[] = {
	SOC_DAPM_PIN_SWITCH("Speakers"),
};

static int tegra_rt1320_asoc_init(struct snd_soc_pcm_runtime *rtd)
{
	// struct tegra_rt1320 *machine = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "-> %s\n", __func__);

	return 0;
}

static struct snd_soc_dai_link tegra_rt1320_dais[] = {
	[0] = {
		.name = "RT1320",
		.stream_name = "RT1320 PCM",
		.codec_dai_name = "rt1320-aif1",
		.init = tegra_rt1320_asoc_init,
		.ops = &tegra_rt1320_ops,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
	},
};

static struct snd_soc_card snd_soc_tegra_rt1320 = {
	.name = "tegra-rt1320",
	.owner = THIS_MODULE,
	.dai_link = tegra_rt1320_dais,
	.num_links = ARRAY_SIZE(tegra_rt1320_dais),
	.controls = tegra_rt1320_controls,
	.num_controls = ARRAY_SIZE(tegra_rt1320_controls),
	.dapm_widgets = tegra_rt1320_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra_rt1320_dapm_widgets),
	.dapm_routes = tegra_rt1320_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra_rt1320_dapm_routes),
	// .fully_routed = true,
};

static int tegra_rt1320_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &snd_soc_tegra_rt1320;
	struct tegra_rt1320 *machine;
	int ret, i = 0;

	dev_dbg(&pdev->dev, "-> %s\n", __func__);

	machine = devm_kzalloc(&pdev->dev,
			sizeof(struct tegra_rt1320), GFP_KERNEL);
	if (!machine)
		return -ENOMEM;

	card->dev = &pdev->dev;
	snd_soc_card_set_drvdata(card, machine);

	for (i = 0; i < 1; i++) {
		tegra_rt1320_dais[i].codec_of_node = of_parse_phandle(np,
				"nvidia,audio-codec", 0);
		if (!tegra_rt1320_dais[i].codec_of_node) {
			dev_err(&pdev->dev,
				"Property 'nvidia,audio-codec %d' missing or invalid\n", i);
			ret = -EINVAL;
			goto err;
		}

		tegra_rt1320_dais[i].cpu_of_node = of_parse_phandle(np,
				"nvidia,i2s-controller", 0);
		if (!tegra_rt1320_dais[i].cpu_of_node) {
			dev_err(&pdev->dev,
				"Property 'nvidia,i2s-controller %d' missing or invalid\n", i);
			ret = -EINVAL;
			goto err;
		}

		tegra_rt1320_dais[i].platform_of_node = tegra_rt1320_dais[i].cpu_of_node;
	}

	ret = tegra_asoc_utils_init(&machine->util_data, &pdev->dev);
	if (ret)
		goto err;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_fini_utils;
	}

	return 0;

err_fini_utils:
	tegra_asoc_utils_fini(&machine->util_data);
err:
	return ret;
}

static int tegra_rt1320_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_rt1320 *machine = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);

	tegra_asoc_utils_fini(&machine->util_data);

	return 0;
}

static const struct of_device_id tegra_rt1320_of_match[] = {
	{ .compatible = "nvidia,tegra-audio-rt1320", },
	{},
};

static struct platform_driver tegra_rt1320_driver = {
	.driver = {
		.name = DRV_NAME,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tegra_rt1320_of_match,
	},
	.probe = tegra_rt1320_probe,
	.remove = tegra_rt1320_remove,
};
module_platform_driver(tegra_rt1320_driver);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra+RT1320 machine ASoC driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra_rt1320_of_match);
