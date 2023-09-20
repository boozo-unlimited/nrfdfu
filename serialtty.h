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

#ifndef LIBI_SERIALTTY_H_
#define LIBI_SERIALTTY_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int serial_init(const char* device_name, int baud);
void serial_fini(int sock);
bool serial_wait_read_ready(int fd, int sec);
bool serial_wait_write_ready(int fd, int sec);
bool serial_write(int fd, const char* buf, size_t len, int timeout_sec);
bool serial_set_baudrate(int fd, int baud);

#ifdef __cplusplus
}
#endif

#endif
