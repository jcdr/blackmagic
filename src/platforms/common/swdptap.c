/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2011  Black Sphere Technologies Ltd.
 * Written by Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* This file implements the SW-DP interface. */

#include "general.h"
#include "swdptap.h"

#ifdef PLATFORM_HAS_SWCLK_WIDTH
static unsigned int delay_turnaround = 0;
static unsigned int delay_seq_in = 0;
static unsigned int delay_seq_in_parity_loop = 0;
static unsigned int delay_seq_in_parity_end = 0;
static unsigned int delay_seq_out = 0;
static unsigned int delay_seq_out_parity_loop = 0;
static unsigned int delay_seq_out_parity_end = 0;

void swdptap_set_delay(unsigned int turnaround,
		       unsigned int seq_in,
		       unsigned int seq_in_parity_loop,
		       unsigned int seq_in_parity_end,
		       unsigned int seq_out,
		       unsigned int seq_out_parity_loop,
		       unsigned int seq_out_parity_end)
{
	delay_turnaround = turnaround;
	delay_seq_in = seq_in;
	delay_seq_in_parity_loop = seq_in_parity_loop;
	delay_seq_in_parity_end = seq_in_parity_end;
	delay_seq_out = seq_out;
	delay_seq_out_parity_loop = seq_out_parity_loop;
	delay_seq_out_parity_end = seq_out_parity_end;
}

static void delay(unsigned int x) {
	while(x--) {
		asm("nop");
	}
}
#define DELAY(x) delay(x);
#else
#define DELAY(x)
#endif


enum {
	SWDIO_STATUS_FLOAT = 0,
	SWDIO_STATUS_DRIVE
};
static void swdptap_turnaround(int dir) __attribute__ ((optimize(3)));
static uint32_t swdptap_seq_in(int ticks) __attribute__ ((optimize(3)));
static bool swdptap_seq_in_parity(uint32_t *ret, int ticks)
	__attribute__ ((optimize(3)));
static void swdptap_seq_out(uint32_t MS, int ticks)
	__attribute__ ((optimize(3)));
static void swdptap_seq_out_parity(uint32_t MS, int ticks)
	__attribute__ ((optimize(3)));

static void swdptap_turnaround(int dir)
{
	static int olddir = SWDIO_STATUS_FLOAT;

	/* Don't turnaround if direction not changing */
	if(dir == olddir) return;
	olddir = dir;

#ifdef DEBUG_SWD_BITS
	DEBUG("%s", dir ? "\n-> ":"\n<- ");
#endif

	if(dir == SWDIO_STATUS_FLOAT)
		SWDIO_MODE_FLOAT();
	gpio_set(SWCLK_PORT, SWCLK_PIN);
#ifndef PLATFORM_HAS_SWCLK_WIDTH
	gpio_set(SWCLK_PORT, SWCLK_PIN);
#endif
	DELAY(delay_turnaround);
	gpio_clear(SWCLK_PORT, SWCLK_PIN);
	if(dir == SWDIO_STATUS_DRIVE)
		SWDIO_MODE_DRIVE();
}

static uint32_t swdptap_seq_in(int ticks)
{
	uint32_t index = 1;
	uint32_t ret = 0;
	int len = ticks;

	swdptap_turnaround(SWDIO_STATUS_FLOAT);
	while (len--) {
		int res;
		res = gpio_get(SWDIO_PORT, SWDIO_PIN);
		gpio_set(SWCLK_PORT, SWCLK_PIN);
		if (res)
			ret |= index;
		index <<= 1;
		DELAY(delay_seq_in);
		gpio_clear(SWCLK_PORT, SWCLK_PIN);
	}

#ifdef DEBUG_SWD_BITS
	for (int i = 0; i < len; i++)
		DEBUG("%d", (ret & (1 << i)) ? 1 : 0);
#endif
	return ret;
}

static bool swdptap_seq_in_parity(uint32_t *ret, int ticks)
{
	uint32_t index = 1;
	uint32_t res = 0;
	bool bit;
	int len = ticks;

	swdptap_turnaround(SWDIO_STATUS_FLOAT);
	while (len--) {
		gpio_clear(SWCLK_PORT, SWCLK_PIN);
		bit = gpio_get(SWDIO_PORT, SWDIO_PIN);
		gpio_set(SWCLK_PORT, SWCLK_PIN);
		if (bit) {
			res |= index;
		}
		index <<= 1;
		DELAY(delay_seq_in_parity_loop);
	}
	gpio_clear(SWCLK_PORT, SWCLK_PIN);
	int parity = __builtin_popcount(res);
	bit = gpio_get(SWDIO_PORT, SWDIO_PIN);
	gpio_set(SWCLK_PORT, SWCLK_PIN);
	if (bit)
		parity++;
	DELAY(delay_seq_in_parity_end);
	gpio_clear(SWCLK_PORT, SWCLK_PIN);
#ifdef DEBUG_SWD_BITS
	for (int i = 0; i < len; i++)
		DEBUG("%d", (res & (1 << i)) ? 1 : 0);
#endif
	*ret = res;
	return (parity & 1);
}

static void swdptap_seq_out(uint32_t MS, int ticks)
{
	int data = MS & 1;
#ifdef DEBUG_SWD_BITS
	for (int i = 0; i < ticks; i++)
		DEBUG("%d", (MS & (1 << i)) ? 1 : 0);
#endif
	swdptap_turnaround(SWDIO_STATUS_DRIVE);
	while (ticks--) {
		gpio_set_val(SWDIO_PORT, SWDIO_PIN, data);
		gpio_clear(SWCLK_PORT, SWCLK_PIN);
		MS >>= 1;
		data = MS & 1;
		gpio_set(SWCLK_PORT, SWCLK_PIN);
		DELAY(delay_seq_out);
	}
	gpio_clear(SWCLK_PORT, SWCLK_PIN);
}

static void swdptap_seq_out_parity(uint32_t MS, int ticks)
{
	int parity = __builtin_popcount(MS);
	int data = MS & 1;
#ifdef DEBUG_SWD_BITS
	for (int i = 0; i < ticks; i++)
		DEBUG("%d", (MS & (1 << i)) ? 1 : 0);
#endif
	swdptap_turnaround(SWDIO_STATUS_DRIVE);
	gpio_set_val(SWDIO_PORT, SWDIO_PIN, data);
	MS >>= 1;
	while (ticks--) {
		data = MS & 1;
		gpio_set(SWCLK_PORT, SWCLK_PIN);
		gpio_set_val(SWDIO_PORT, SWDIO_PIN, data);
		MS >>= 1;
		DELAY(delay_seq_out_parity_loop);
		gpio_clear(SWCLK_PORT, SWCLK_PIN);
	}
	gpio_set_val(SWDIO_PORT, SWDIO_PIN, parity & 1);
	gpio_clear(SWCLK_PORT, SWCLK_PIN);
	DELAY(delay_seq_out_parity_loop);
	gpio_set(SWCLK_PORT, SWCLK_PIN);
#ifndef PLATFORM_HAS_SWCLK_WIDTH
	gpio_set(SWCLK_PORT, SWCLK_PIN);
#endif
	DELAY(delay_seq_out_parity_end);
	gpio_clear(SWCLK_PORT, SWCLK_PIN);
}

swd_proc_t swd_proc;

int swdptap_init(void)
{
	swd_proc.swdptap_seq_in  = swdptap_seq_in;
	swd_proc.swdptap_seq_in_parity  = swdptap_seq_in_parity;
	swd_proc.swdptap_seq_out = swdptap_seq_out;
	swd_proc.swdptap_seq_out_parity  = swdptap_seq_out_parity;

	return 0;
}
