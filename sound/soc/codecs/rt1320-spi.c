/*
 * rt1320-spi.c  --  ALC1320 SPI driver
 *
 * Copyright 2025 Realtek Semiconductor Corp.
 * Author: Jack Yu <jack.yu@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_qos.h>
#include <linux/sysfs.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "rt1320-spi-3.h"

static struct spi_device *rt1320_spi;

int rt1320_spi_read_addr(unsigned int addr, unsigned int *val)
{
	struct spi_message message;
	struct spi_transfer x[3];
	u8 spi_cmd = RT1320_SPI_CMD_32_READ;
	int status;
	u8 write_buf[5];
	u8 read_buf[4];

	write_buf[0] = spi_cmd;
	write_buf[1] = (addr & 0x000000ff) >> 0;
	write_buf[2] = (addr & 0x0000ff00) >> 8;
	write_buf[3] = (addr & 0x00ff0000) >> 16;
	write_buf[4] = (addr & 0xff000000) >> 24;

	spi_message_init(&message);
	memset(x, 0, sizeof(x));

	x[0].len = 5;
	x[0].tx_buf = write_buf;
	spi_message_add_tail(&x[0], &message);

	x[1].len = 4;
	x[1].tx_buf = write_buf;
	spi_message_add_tail(&x[1], &message);

	x[2].len = 4;
	x[2].rx_buf = read_buf;
	spi_message_add_tail(&x[2], &message);

	status = spi_sync(rt1320_spi, &message);

	*val = read_buf[0] | read_buf[1] << 8 | read_buf[2] << 16 |
		read_buf[3] << 24;

	return status;
}

int rt1320_spi_write_addr(unsigned int addr, unsigned int val)
{
	u8 spi_cmd = RT1320_SPI_CMD_32_WRITE;
	int status;
	u8 write_buf[10];

	write_buf[0] = spi_cmd;
	write_buf[1] = (addr & 0x000000ff) >> 0;
	write_buf[2] = (addr & 0x0000ff00) >> 8;
	write_buf[3] = (addr & 0x00ff0000) >> 16;
	write_buf[4] = (addr & 0xff000000) >> 24;
	write_buf[5] = (val & 0x000000ff) >> 0;
	write_buf[6] = (val & 0x0000ff00) >> 8;
	write_buf[7] = (val & 0x00ff0000) >> 16;
	write_buf[8] = (val & 0xff000000) >> 24;
	write_buf[9] = spi_cmd;

	status = spi_write(rt1320_spi, write_buf, sizeof(write_buf));

	if (status)
		dev_err(&rt1320_spi->dev, "%s error %d\n", __func__, status);

	return status;
}

int rt1320_spi_burst_read(u32 addr, u8 *rxbuf, size_t len)
{
	u8 spi_cmd = RT1320_SPI_CMD_BURST_READ;
	int status;
	u8 write_buf[8];
	unsigned int end, offset = 0;

	struct spi_message message;
	struct spi_transfer x[3];

	while (offset < len) {
		if (offset + RT1320_SPI_BUF_LEN <= len)
			end = RT1320_SPI_BUF_LEN;
		else
			end = len % RT1320_SPI_BUF_LEN;

		write_buf[0] = spi_cmd;
		write_buf[1] = ((addr + offset) & 0x000000ff) >> 0;
		write_buf[2] = ((addr + offset) & 0x0000ff00) >> 8;
		write_buf[3] = ((addr + offset) & 0x00ff0000) >> 16;
		write_buf[4] = ((addr + offset) & 0xff000000) >> 24;

		spi_message_init(&message);
		memset(x, 0, sizeof(x));

		x[0].len = 5;
		x[0].tx_buf = write_buf;
		spi_message_add_tail(&x[0], &message);

		x[1].len = 4;
		x[1].tx_buf = write_buf;
		spi_message_add_tail(&x[1], &message);

		x[2].len = end;
		x[2].rx_buf = rxbuf + offset;
		spi_message_add_tail(&x[2], &message);

		status = spi_sync(rt1320_spi, &message);

		if (status)
			return false;

		offset += RT1320_SPI_BUF_LEN;
	}

	return 0;
}

/**
 * rt1320_spi_burst_write - Write data to SPI by rt1320 address.
 * @addr: Start address.
 * @txbuf: Data Buffer for writng.
 * @len: Data length.
 *
 *
 * Returns true for success.
 */
int rt1320_spi_burst_write(u32 addr, const u8 *txbuf, size_t len)
{
	u8 spi_cmd = RT1320_SPI_CMD_BURST_WRITE;
	u8 *write_buf;
	unsigned int end, offset = 0;

	write_buf = kmalloc(RT1320_SPI_BUF_LEN + 6, GFP_KERNEL);

	while (offset < len) {
		if (offset + RT1320_SPI_BUF_LEN <= len)
			end = RT1320_SPI_BUF_LEN;
		else
			end = len % RT1320_SPI_BUF_LEN;

		write_buf[0] = spi_cmd;
		write_buf[1] = ((addr + offset) & 0x000000ff) >> 0;
		write_buf[2] = ((addr + offset) & 0x0000ff00) >> 8;
		write_buf[3] = ((addr + offset) & 0x00ff0000) >> 16;
		write_buf[4] = ((addr + offset) & 0xff000000) >> 24;

		memcpy(&write_buf[5], &txbuf[offset], end);

		if (end % 8)
			end = (end / 8 + 1) * 8;

		spi_write(rt1320_spi, write_buf, end + 6);

		offset += RT1320_SPI_BUF_LEN;
	}

	kfree(write_buf);

	return 0;
}

int rt1320_spi_burst_write_exp(const u8 *txbuf,unsigned int end)
{
	int ret;

	ret = spi_write(rt1320_spi, txbuf, end + 6);

	return ret;
}

static int rt1320_spi_probe(struct spi_device *spi)
{
	pr_info("rt1320_spi_probe is probed!\n");
	rt1320_spi = spi;

	return 0;
}

static const struct of_device_id rt1320_of_match[] = {
	{ .compatible = "realtek,rt1320-spi", },
	{},
};
MODULE_DEVICE_TABLE(of, rt1320_of_match);

static struct spi_driver rt1320_spi_driver = {
	.driver = {
		.name = "rt1320",
		.of_match_table = of_match_ptr(rt1320_of_match),
	},
	.probe = rt1320_spi_probe,
};
module_spi_driver(rt1320_spi_driver);

MODULE_DESCRIPTION("RT1320 SPI driver");
MODULE_AUTHOR("Jack Yu <jack.yu@realtek.com>");
MODULE_LICENSE("GPL v2");
