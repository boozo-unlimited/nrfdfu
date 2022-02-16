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

#ifndef CONF_H_
#define CONF_H_

#include <stdbool.h>
#include <stdint.h>

#define CONF_MAX_LEN 200

enum DFU_TYPE { DFU_SERIAL, DFU_BLE };

/* same as enum blz_addr_type */
enum BLE_ATYPE { BAT_UNKNOWN, BAT_PUBLIC, BAT_RANDOM };

struct config {
	int loglevel;
	char* serport;
	int serspeed;
	bool ser_acm;
	char* zipfile;
	char* dfucmd;
	bool dfucmd_hex;
	int timeout;
	enum DFU_TYPE dfu_type;
	char* interface;
	char* ble_addr;
	enum BLE_ATYPE ble_atype;
	char* ble_passkey;
};

extern struct config conf;

#endif
