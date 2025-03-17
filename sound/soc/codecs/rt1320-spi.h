/*
 * rt1320-spi.h  --  ALC1320 SPI driver
 *
 * Copyright 2025 Realtek Semiconductor Corp.
 * Author: Jack Yu <jack.yu@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RT1320_SPI_H__
#define __RT1320_SPI_H__

#define RT1320_SPI_BUF_LEN		56 //240

/* SPI Command */
enum {
	RT1320_SPI_CMD_16_READ = 0,
	RT1320_SPI_CMD_16_WRITE,
	RT1320_SPI_CMD_32_READ,
	RT1320_SPI_CMD_32_WRITE,
	RT1320_SPI_CMD_BURST_READ,
	RT1320_SPI_CMD_BURST_WRITE,
};

int rt1320_spi_burst_write(u32 addr, const u8 *txbuf, size_t len);
int rt1320_spi_burst_read(u32 addr, u8 *rxbuf, size_t len);
int rt1320_spi_burst_write_exp(const u8 *txbuf,unsigned int end);
int rt1320_spi_read_addr(unsigned int addr, unsigned int *val);
int rt1320_spi_write_addr(unsigned int addr, unsigned int val);

#endif /* __RT1320_SPI_H__ */
