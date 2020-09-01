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

#ifndef __SWDPTAP_H
#define __SWDPTAP_H
typedef struct swd_proc_s {
	uint32_t (*swdptap_seq_in)(int ticks);
	bool (*swdptap_seq_in_parity)(uint32_t *data, int ticks);
	void (*swdptap_seq_out)(uint32_t MS, int ticks);
	void (*swdptap_seq_out_parity)(uint32_t MS, int ticks);
} swd_proc_t;
extern swd_proc_t swd_proc;

#ifdef PLATFORM_HAS_SWCLK_WIDTH
void swdptap_set_delay(unsigned int turnaround,
		       unsigned int seq_in,
		       unsigned int seq_in_parity_loop,
		       unsigned int seq_in_parity_end,
		       unsigned int seq_out,
		       unsigned int seq_out_parity_loop,
		       unsigned int seq_out_parity_end);
#endif

# if PC_HOSTED == 1
int platform_swdptap_init(void);
# else
int swdptap_init(void);
# endif
#endif
