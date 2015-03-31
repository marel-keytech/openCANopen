#include "canopen/sdo.h"

void sdo_abort(struct can_frame* frame, enum sdo_abort_code code, int index,
	       int subindex)
{
	sdo_clear_frame(frame);

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
	case SDO_ABORT_INVALID_CS:
		return "Client/server command specifier not valid or unknown";
	case SDO_ABORT_NOMEM:
		return "Out of memory";
	case SDO_ABORT_NEXIST:
		return "Object does not exist in the object dictionary";
	case SDO_ABORT_SIZE:
		return "Data type does not match, length of service parameter does not match";
	case SDO_ABORT_TOO_LONG:
		return "Data type does not match, length of service parameter too high";
	case SDO_ABORT_TOO_SHORT:
		return "Data type does not match, length of service parameter too low";
	case SDO_ABORT_RO:
		return "Attempt to write a read only object";
	case SDO_ABORT_WO:
		return "Attempt to read a write only object";
	}
	return "UNKNOWN";
}

