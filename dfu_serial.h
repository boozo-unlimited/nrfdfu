/*
 * nrfdfu - Nordic DFU Upgrade Utility
 *
 * Copyright (C) 2019 Bruno Randolf (br1@einfach.org)
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef DFUSER_H
#define DFUSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SLIP buffer size should be bigger than MTU */
#define BUF_SIZE	  1050
#define SLIP_BUF_SIZE (BUF_SIZE * 2 + 1)

bool ser_enter_dfu(void);
bool ser_encode_write(uint8_t* req, size_t len, int timeout_sec);
const uint8_t* ser_read_decode(int timeout_sec);
void ser_fini(void);
void ser_reopen(int sleep_time);

#ifdef __cplusplus
}
#endif

#endif
