#include "canopen/types.h"

size_t canopen_type_size(enum canopen_type type)
{
	switch (type) {
	case CANOPEN_BOOLEAN:
	case CANOPEN_INTEGER8:
	case CANOPEN_UNSIGNED8:
		return 1;
	case CANOPEN_INTEGER16:
	case CANOPEN_UNSIGNED16:
		return 2;
	case CANOPEN_INTEGER24:
	case CANOPEN_UNSIGNED24:
		return 3;
	case CANOPEN_INTEGER32:
	case CANOPEN_UNSIGNED32:
	case CANOPEN_REAL32:
		return 4;
	case CANOPEN_INTEGER40:
	case CANOPEN_UNSIGNED40:
		return 5;
	case CANOPEN_INTEGER48:
	case CANOPEN_UNSIGNED48:
	case CANOPEN_TIME_OF_DAY:
	case CANOPEN_TIME_DIFFERENCE:
		return 6;
	case CANOPEN_INTEGER56:
	case CANOPEN_UNSIGNED56:
		return 7;
	case CANOPEN_INTEGER64:
	case CANOPEN_UNSIGNED64:
	case CANOPEN_REAL64:
		return 8;
	default:
		break;
	}

	return 0;
}

int canopen_type_is_signed_integer(enum canopen_type type)
{
	switch (type) {
	case CANOPEN_INTEGER8:
	case CANOPEN_INTEGER16:
	case CANOPEN_INTEGER24:
	case CANOPEN_INTEGER32:
	case CANOPEN_INTEGER40:
	case CANOPEN_INTEGER48:
	case CANOPEN_INTEGER56:
	case CANOPEN_INTEGER64:
		return 1;
	default:
		return 0;
	}
}

int canopen_type_is_unsigned_integer(enum canopen_type type)
{
	switch (type) {
	case CANOPEN_UNSIGNED8:
	case CANOPEN_UNSIGNED16:
	case CANOPEN_UNSIGNED24:
	case CANOPEN_UNSIGNED32:
	case CANOPEN_UNSIGNED40:
	case CANOPEN_UNSIGNED48:
	case CANOPEN_UNSIGNED56:
	case CANOPEN_UNSIGNED64:
		return 1;
	default:
		return 0;
	}
}

