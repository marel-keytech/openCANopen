#include <string.h>
#include "canopen/types.h"

#define XSTR(s) STR(s)
#define STR(s) #s

size_t canopen_type_size(enum canopen_type type)
{
	switch (type) {
#define X(name, number, size) case CANOPEN_ ## name: return size;
	CANOPEN_TYPES
#undef X
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

const char* canopen_type_to_string(enum canopen_type type)
{
	switch (type) {
#define X(name, ...) case CANOPEN_ ## name: return XSTR(name);
	CANOPEN_TYPES
#undef X
	}

	return NULL;
}

enum canopen_type canopen_type_from_string(const char* str)
{
#define X(name, ...) \
	if (strcasecmp(str, XSTR(name)) == 0) \
		return CANOPEN_ ## name;

	CANOPEN_TYPES
#undef X

	return CANOPEN_UNKNOWN;
}
