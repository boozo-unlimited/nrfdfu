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

#ifndef DFU_H
#define DFU_H

#include <stdbool.h>
#include <stddef.h>
#include <zip.h>
#include "nrf_dfu_req_handler.h"
#include "nrf_dfu_handling_error.h"

#ifdef __cplusplus
extern "C" {
#endif

enum dfu_ret { DFU_RET_SUCCESS, DFU_RET_ERROR, DFU_RET_FW_VERSION };

bool dfu_ping(void);
bool dfu_bootloader_enter(void);
enum dfu_ret dfu_upgrade(zip_file_t* init_zip, size_t init_size,
						 zip_file_t* fw_zip, size_t fw_size);

size_t dfu_request_size(nrf_dfu_request_t* req);
const char* dfu_err_str(nrf_dfu_result_t res);
const char* dfu_ext_err_str(nrf_dfu_ext_error_code_t res);

#ifdef __cplusplus
}
#endif

#endif
