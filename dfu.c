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

#ifdef __APPLE__
#include "mac_endian.h"
#elif _NEWLIB_VERSION
/* Bare-metal toolchain build with newlib */
#include "arm_endian.h"
#else
#include <endian.h>
#endif

#include <zlib.h>

#include "conf.h"
#include "dfu.h"
#include "dfu_ble.h"
#include "dfu_serial.h"
#include "log.h"
#include "nrf_dfu_handling_error.h"
#include "nrf_dfu_req_handler.h"
#include "util.h"

/* Timeout on Serial in seconds */
#define SER_TIMEOUT_DEFAULT 1
#define SER_TIMEOUT_OBJ_EXE 10

static uint16_t dfu_mtu;
static uint32_t dfu_max_size;
static uint32_t dfu_current_crc;

size_t dfu_request_size(nrf_dfu_request_t* req)
{
	switch (req->request) {
	case NRF_DFU_OP_OBJECT_CREATE:
		return 1 + sizeof(req->create);
	case NRF_DFU_OP_RECEIPT_NOTIF_SET:
		return 1 + sizeof(req->prn);
	case NRF_DFU_OP_OBJECT_SELECT:
		return 1 + sizeof(req->select);
	case NRF_DFU_OP_MTU_GET:
		return 1; // NOT sizeof(req->mtu);
	case NRF_DFU_OP_OBJECT_WRITE:
		return 1 + sizeof(req->write);
	case NRF_DFU_OP_PING:
		return 1 + sizeof(req->ping);
	case NRF_DFU_OP_FIRMWARE_VERSION:
		return 1 + sizeof(req->firmware);
	case NRF_DFU_OP_PROTOCOL_VERSION:
	case NRF_DFU_OP_CRC_GET:
	case NRF_DFU_OP_OBJECT_EXECUTE:
	case NRF_DFU_OP_HARDWARE_VERSION:
	case NRF_DFU_OP_ABORT:
	case NRF_DFU_OP_RESPONSE:
	case NRF_DFU_OP_INVALID:
		return 1;
	}
	return 0;
}

static bool send_request(nrf_dfu_request_t* req)
{
	size_t size = dfu_request_size(req);
	if (size == 0) {
		LOG_ERR("Unknown size");
		return false;
	}

	if (conf.dfu_type == DFU_SERIAL) {
		return ser_encode_write((uint8_t*)req, size, SER_TIMEOUT_DEFAULT);
	} else {
		return ble_write_ctrl((uint8_t*)req, size);
	}
}

const char* dfu_err_str(nrf_dfu_result_t res)
{
	switch (res) {
	case NRF_DFU_RES_CODE_INVALID:
		return "Invalid opcode";
	case NRF_DFU_RES_CODE_SUCCESS:
		return "Operation successful";
	case NRF_DFU_RES_CODE_OP_CODE_NOT_SUPPORTED:
		return "Opcode not supported";
	case NRF_DFU_RES_CODE_INVALID_PARAMETER:
		return "Missing or invalid parameter value";
	case NRF_DFU_RES_CODE_INSUFFICIENT_RESOURCES:
		return "Not enough memory for the data object";
	case NRF_DFU_RES_CODE_INVALID_OBJECT:
		return "Data object does not match the firmware and "
			   "hardware requirements, the signature is wrong, "
			   "or parsing the command failed";
	case NRF_DFU_RES_CODE_UNSUPPORTED_TYPE:
		return "Not a valid object type for a Create request";
	case NRF_DFU_RES_CODE_OPERATION_NOT_PERMITTED:
		return "The state of the DFU process does not allow this "
			   "operation";
	case NRF_DFU_RES_CODE_OPERATION_FAILED:
		return "Operation failed";
	case NRF_DFU_RES_CODE_EXT_ERROR:
		return "Extended error";
		/* The next byte of the response contains the error code
		 * of the extended error (see @ref nrf_dfu_ext_error_code_t. */
	}
	return "Unknown error";
}

const char* dfu_ext_err_str(nrf_dfu_ext_error_code_t res)
{
	switch (res) {
	case NRF_DFU_EXT_ERROR_NO_ERROR:
		return "No extended error code has been set.";
	case NRF_DFU_EXT_ERROR_INVALID_ERROR_CODE:
		return "Invalid error code.";
	case NRF_DFU_EXT_ERROR_WRONG_COMMAND_FORMAT:
		return "The format of the command was incorrect.";
	case NRF_DFU_EXT_ERROR_UNKNOWN_COMMAND:
		return "The command was successfully parsed, but it is "
			   "not supported or unknown";
	case NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID:
		return "The init command is invalid. The init packet "
			   "either has an invalid update type or it is "
			   "missing required fields for the update type "
			   "for example, the init packet for a SoftDevice "
			   "update is missing the SoftDevice size field.";
	case NRF_DFU_EXT_ERROR_FW_VERSION_FAILURE:
		return "The firmware version is too low. For an "
			   "application or SoftDevice, the version must be "
			   "greater than or equal to the current version. "
			   "For a bootloader, it must be greater than the "
			   "current version. This requirement prevents "
			   "downgrade attacks.";
	case NRF_DFU_EXT_ERROR_HW_VERSION_FAILURE:
		return "The hardware version of the device does not "
			   "match the required hardware version for the "
			   "update.";
	case NRF_DFU_EXT_ERROR_SD_VERSION_FAILURE:
		return "The array of supported SoftDevices for the "
			   "update does not contain the FWID of the "
			   "current SoftDevice or the first FWID is '0' on "
			   "a bootloader which requires the SoftDevice to "
			   "be present.";
	case NRF_DFU_EXT_ERROR_SIGNATURE_MISSING:
		return "The init packet does not contain a signature.";
	case NRF_DFU_EXT_ERROR_WRONG_HASH_TYPE:
		return "The hash type that is specified by the init "
			   "packet is not supported by the DFU bootloader.";
	case NRF_DFU_EXT_ERROR_HASH_FAILED:
		return "The hash of the firmware image cannot be "
			   "calculated.";
	case NRF_DFU_EXT_ERROR_WRONG_SIGNATURE_TYPE:
		return "The type of the signature is unknown or not "
			   "supported by the DFU bootloader.";
	case NRF_DFU_EXT_ERROR_VERIFICATION_FAILED:
		return "The hash of the received firmware image does "
			   "not match the hash in the init packet.";
	case NRF_DFU_EXT_ERROR_INSUFFICIENT_SPACE:
		return "The available space on the device is "
			   "insufficient to hold the firmware.";
	}
	return "Unknown extended error";
}

static nrf_dfu_response_t* get_response(nrf_dfu_op_t request)
{
	const uint8_t* buf = NULL;
	if (conf.dfu_type == DFU_SERIAL) {
		/* object execute needs more time when updating bootloader/SD */
		buf = ser_read_decode(request == NRF_DFU_OP_OBJECT_EXECUTE
								  ? SER_TIMEOUT_OBJ_EXE
								  : SER_TIMEOUT_DEFAULT);
	} else {
		buf = ble_read();
	}

	if (!buf) {
		/* error printed in function above */
		return NULL;
	}

	if (buf[0] != NRF_DFU_OP_RESPONSE) {
		LOG_ERR("No response");
		return NULL;
	}

	nrf_dfu_response_t* resp = (nrf_dfu_response_t*)(buf + 1);

	if (resp->request != request) {
		LOG_ERR("Response does not match request (0x%x vs 0x%x)", resp->request,
				request);
		return NULL;
	}

	return resp;
}

static bool response_is_error(nrf_dfu_response_t* resp)
{
	if (resp == NULL) {
		return true;
	}

	if (resp->result != NRF_DFU_RES_CODE_SUCCESS) {
		if (resp->result == NRF_DFU_RES_CODE_EXT_ERROR) {
			LOG_ERR("\nERROR: %s", dfu_ext_err_str(resp->ext_err));
		} else {
			LOG_ERR("\nERROR: %s", dfu_err_str(resp->result));
		}
		return true;
	}

	return false;
}

/* serial only */
bool dfu_ping(void)
{
	static uint8_t ping_id = 1;
	LOG_INF_("Sending ping %d: ", ping_id);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_PING,
		.ping.id = ping_id++,
	};

	if (!send_request(&req)) {
		return false;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (response_is_error(resp)) {
		return false;
	}

	if (resp->ping.id == ping_id - 1) {
		LOG_INF("OK");
	} else {
		LOG_INF("Wrong ID");
	}
	return (resp->ping.id == ping_id - 1);
}

static bool dfu_set_packet_receive_notification(uint16_t prn)
{
	LOG_INF_("Set packet receive notification %d: ", prn);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_RECEIPT_NOTIF_SET,
		.prn.target = htole16(prn),
	};

	if (!send_request(&req)) {
		return false;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (response_is_error(resp)) {
		return false;
	}

	LOG_INF("OK");
	return true;
}

/* serial only */
static bool dfu_get_serial_mtu(void)
{
	LOG_INF_("Get serial MTU: ");
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_MTU_GET,
	};

	if (!send_request(&req)) {
		return false;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (response_is_error(resp)) {
		return false;
	}

	dfu_mtu = le16toh(resp->mtu.size);
	if (dfu_mtu > SLIP_BUF_SIZE) {
		LOG_WARN("MTU of %d limited to buffer size %d", dfu_mtu, SLIP_BUF_SIZE);
		dfu_mtu = SLIP_BUF_SIZE;
	}
	/* use MTU without SLIP overhead */
	LOG_INF("%d with SLIP => %d", dfu_mtu, (dfu_mtu - 1) / 2);
	dfu_mtu = (dfu_mtu - 1) / 2;
	return true;
}

static void dfu_set_mtu(uint16_t mtu)
{
	dfu_mtu = mtu;
}

static uint32_t dfu_get_crc(void)
{
	LOG_INF_("Get CRC: ");
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_CRC_GET,
	};

	if (!send_request(&req)) {
		return 0;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (response_is_error(resp)) {
		return 0;
	}

	LOG_INF("0x%X (offset %u)", le32toh(resp->crc.crc),
			le32toh(resp->crc.offset));
	return le32toh(resp->crc.crc);
}

static bool dfu_object_select(uint8_t type, uint32_t* offset, uint32_t* crc)
{
	LOG_INF_("Select object %d: ", type);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_OBJECT_SELECT,
		.select.object_type = type,
	};

	if (!send_request(&req)) {
		return false;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (response_is_error(resp)) {
		return false;
	}

	dfu_max_size = le32toh(resp->select.max_size);
	*offset = le32toh(resp->select.offset);
	*crc = le32toh(resp->select.crc);
	LOG_INF("offset %u max_size %u CRC 0x%X", *offset, dfu_max_size, *crc);
	return true;
}

static bool dfu_object_create(uint8_t type, uint32_t size)
{
	LOG_INF_("Create object %d (size %u): ", type, size);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_OBJECT_CREATE,
		.create.object_type = type,
		.create.object_size = htole32(size),
	};

	if (!send_request(&req)) {
		return false;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (response_is_error(resp)) {
		return false;
	}

	LOG_INF("OK");
	return true;
}

static bool dfu_object_write(zip_file_t* zf, size_t size)
{
	uint8_t buf[dfu_mtu];
	uint8_t* fbuf = buf;
	size_t written = 0;
	zip_int64_t len;
	size_t to_read;

	LOG_INF_("Write data (size %zd MTU %d): ", size, dfu_mtu);

	do {
		if (conf.dfu_type == DFU_SERIAL) {
			/* we need to put the write command first, so that leaves one
			 * byte less for data */
			buf[0] = NRF_DFU_OP_OBJECT_WRITE;
			fbuf = buf + 1;
			to_read = MIN(sizeof(buf) - 1, size - written);
		} else {
			to_read = MIN(sizeof(buf), size - written);
		}
		len = zip_fread(zf, fbuf, to_read);
		if (len < 0) {
			LOG_ERR("zip_fread error");
			break;
		}
		if (len == 0) { // EOF
			break;
		}
		bool b;
		if (conf.dfu_type == DFU_SERIAL) {
			b = ser_encode_write(buf, len + 1, SER_TIMEOUT_DEFAULT);
		} else {
			b = ble_write_data(fbuf, len);
		}
		if (!b) {
			LOG_ERR("write failed");
			return false;
		}
		written += len;
		dfu_current_crc = crc32(dfu_current_crc, fbuf, len);
	} while (len > 0 && written < size && written < dfu_max_size);

	// No response expected
	LOG_INF("%zd bytes CRC: 0x%X", written, dfu_current_crc);

	if (conf.loglevel < LL_INFO) {
		printf(".");
		fflush(stdout);
	}

	return true;
}

/** this writes the object to flash
 * return: failed, success, fw_version too low */
static enum dfu_ret dfu_object_execute(void)
{
	LOG_INF_("Object Execute: ");
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_OBJECT_EXECUTE,
	};

	if (!send_request(&req)) {
		return DFU_RET_ERROR;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (response_is_error(resp)) {
		if (resp && resp->result == NRF_DFU_RES_CODE_EXT_ERROR
			&& resp->ext_err == NRF_DFU_EXT_ERROR_FW_VERSION_FAILURE) {
			return DFU_RET_FW_VERSION;
		} else {
			return DFU_RET_ERROR;
		}
	}

	LOG_INF("OK");
	return DFU_RET_SUCCESS;
}

/* get CRC of contents of ZIP file until size */
static uint32_t zip_crc_move(zip_file_t* zf, size_t size)
{
	uint8_t fbuf[200];
	size_t read = 0;
	size_t to_read;
	zip_int64_t len;
	uint32_t crc = crc32(0L, Z_NULL, 0);

	do {
		to_read = MIN(sizeof(fbuf), size - read);
		len = zip_fread(zf, fbuf, to_read);
		if (len < 0) {
			LOG_ERR("zip_fread error");
			break;
		}
		if (len == 0) { // EOF
			break;
		}
		read += len;
		crc = crc32(crc, fbuf, len);
	} while (len > 0 && read < size);

	return crc;
}

/** return: failed, success, fw_version too low */
static enum dfu_ret dfu_object_write_procedure(uint8_t type, zip_file_t* zf,
											   size_t sz)
{
	uint32_t offset;
	uint32_t crc;
	enum dfu_ret ret;

	if (!dfu_object_select(type, &offset, &crc)) {
		return DFU_RET_ERROR;
	}

	/* object with same length and CRC already received */
	if (offset == sz && zip_crc_move(zf, sz) == crc) {
		LOG_NOTI_("Object already received");
		/* Don't transfer anything and skip to the Execute command */
		return dfu_object_execute();
	}

	/* parts already received */
	if (offset > 0) {
		uint32_t remain = offset % dfu_max_size;
		LOG_WARN("Object partially received (offset %u remaining %u)", offset,
				 remain);

		dfu_current_crc = zip_crc_move(zf, offset);
		if (crc != dfu_current_crc) {
			/* invalid crc, remove corrupted data, rewind and
			 * create new object below */
			offset -= remain > 0 ? remain : dfu_max_size;
			LOG_WARN("CRC does not match (restarting from %u)", offset);
			zip_fseek(zf, 0, 0);
			dfu_current_crc = zip_crc_move(zf, offset);
		} else if (offset < sz) { /* CRC matches */
			/* transfer remaining data if necessary */
			if (remain > 0) {
				size_t end = offset + dfu_max_size - remain;
				if (!dfu_object_write(zf, end)) {
					return DFU_RET_ERROR;
				}
			}
			ret = dfu_object_execute();
			if (ret != DFU_RET_SUCCESS) {
				return ret;
			}
		}
	} else if (offset == 0) {
		dfu_current_crc = crc32(0L, Z_NULL, 0);
	}

	/* create and write objects of max_size */
	for (int i = offset; i < sz; i += dfu_max_size) {
		size_t osz = MIN(sz - i, dfu_max_size);
		if (!dfu_object_create(type, osz)) {
			return DFU_RET_ERROR;
		}

		if (!dfu_object_write(zf, osz)) {
			return DFU_RET_ERROR;
		}

		uint32_t rcrc = dfu_get_crc();
		if (rcrc != dfu_current_crc) {
			LOG_ERR("CRC failed 0x%X vs 0x%X", rcrc, dfu_current_crc);
			return DFU_RET_ERROR;
		}

		ret = dfu_object_execute();
		if (ret != DFU_RET_SUCCESS) {
			return ret;
		}
	}

	return DFU_RET_SUCCESS;
}

bool dfu_bootloader_enter(void)
{
	if (conf.dfu_type == DFU_SERIAL) {
		if (!ser_enter_dfu()) {
			return false;
		}
		if (!dfu_get_serial_mtu()) {
			return false;
		}
	} else {
		int e = ble_enter_dfu(conf.interface, conf.ble_addr, conf.ble_atype);
		if (!e) {
			return false;
		}

		/* Normally the device entered the bootloader and is now available as
		 * as DfuTarg with a MAC address + 1 and we need to connect to it.
		 * In the special case that we we already connected to the bootloader
		 * above, this is detected and ble_enter_dfu() returns 2. */
		if (e != 2) {
			if (!ble_connect_dfu_targ(conf.interface, conf.ble_addr,
									  conf.ble_atype)) {
				return false;
			}
		}

		dfu_set_mtu(244);
	}
	return true;
}

/** return: failed, success, fw_version too low */
enum dfu_ret dfu_upgrade(zip_file_t* init_zip, size_t init_size,
						 zip_file_t* fw_zip, size_t fw_size)
{
	if (!dfu_set_packet_receive_notification(0)) {
		return DFU_RET_ERROR;
	}

	LOG_NOTI_("Sending Init: ");
	enum dfu_ret ret = dfu_object_write_procedure(1, init_zip, init_size);
	if (ret != DFU_RET_SUCCESS) {
		return ret;
	}
	LOG_NL(LL_NOTICE);

	LOG_NOTI_("Sending Data: ");
	ret = dfu_object_write_procedure(2, fw_zip, fw_size);
	if (ret != DFU_RET_SUCCESS) {
		return ret;
	}

	LOG_NL(LL_NOTICE);
	LOG_NOTI("Done");
	return DFU_RET_SUCCESS;
}
