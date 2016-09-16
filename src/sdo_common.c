/* Copyright (c) 2014-2016, Marel
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "canopen/sdo.h"

#ifndef CAN_MAX_DLC
#define CAN_MAX_DLC 8
#endif

void sdo_abort(struct can_frame* frame, enum sdo_abort_code code, int index,
	       int subindex)
{
	sdo_set_cs(frame, SDO_SCS_ABORT);
	sdo_set_abort_code(frame, code);

	sdo_set_index(frame, index);
	sdo_set_subindex(frame, subindex);

	frame->can_dlc = CAN_MAX_DLC;
}

const char* sdo_strerror(enum sdo_abort_code code)
{
	switch (code) {
	case SDO_ABORT_TOGGLE:
		return "Toggle bit not alternated";
	case SDO_ABORT_TIMEOUT:
		return "SDO protocol timed out";
	case SDO_ABORT_INVALID_CS:
		return "Client/server command specifier not valid or unknown";
	case SDO_ABORT_BLOCKSZ:
		return "Invalid block size";
	case SDO_ABORT_SEQNR:
		return "Invalid sequence number";
	case SDO_ABORT_CRCERR:
		return "CRC error";
	case SDO_ABORT_NOMEM:
		return "Out of memory";
	case SDO_ABORT_ACCESS:
		return "Unsupported access";
	case SDO_ABORT_RO:
		return "Attempt to write a read only object";
	case SDO_ABORT_WO:
		return "Attempt to read a write only object";
	case SDO_ABORT_NEXIST:
		return "Object does not exist in the object dictionary";
	case SDO_ABORT_NOPDO:
		return "Object cannot be mapped to PDO";
	case SDO_ABORT_PDOSZ:
		return "The number and length of objects to be mapped would exceed PDO length";
	case SDO_ABORT_PARCOMPAT:
		return "General parameter incompatibility error";
	case SDO_ABORT_DEVCOMPAT:
		return "General internal incompatibility in device";
	case SDO_ABORT_HWERROR:
		return "Access failed due to a hardware error";
	case SDO_ABORT_SIZE:
		return "Data type does not match, length of service parameter does not match";
	case SDO_ABORT_TOO_LONG:
		return "Data type does not match, length of service parameter too high";
	case SDO_ABORT_TOO_SHORT:
		return "Data type does not match, length of service parameter too low";
	case SDO_ABORT_SUBNEXIST:
		return "Sub-index does not exist";
	case SDO_ABORT_NVAL:
		return "Invalid value for parameter";
	case SDO_ABORT_GENERAL:
		return "General error";
	}
	return "UNKNOWN";
}

