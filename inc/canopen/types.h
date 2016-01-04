#ifndef _CANOPEN_TYPES_H
#define _CANOPEN_TYPES_H

#include <assert.h>
#include <stdint.h>
#include "canopen/byteorder.h"

#define CANOPEN_TYPES \
	X(UNKNOWN, 0x00, 0) \
	X(BOOLEAN, 0x01, 1) \
	X(INTEGER8, 0x02, 1) \
	X(INTEGER16, 0x03, 2) \
	X(INTEGER32, 0x04, 4) \
	X(UNSIGNED8, 0x05, 1) \
	X(UNSIGNED16, 0x06, 2) \
	X(UNSIGNED32, 0x07, 4) \
	X(REAL32, 0x08, 4) \
	X(VISIBLE_STRING, 0x09, 0) \
	X(OCTET_STRING, 0x0A, 0) \
	X(UNICODE_STRING, 0x0B, 0) \
	X(TIME_OF_DAY, 0x0C, 6) \
	X(TIME_DIFFERENCE, 0x0D, 6) \
	X(DOMAIN, 0x0F, 0) \
	X(INTEGER24, 0x10, 3) \
	X(REAL64, 0x11, 8) \
	X(INTEGER40, 0x12, 5) \
	X(INTEGER48, 0x13, 6) \
	X(INTEGER56, 0x14, 7) \
	X(INTEGER64, 0x15, 8) \
	X(UNSIGNED24, 0x16, 3) \
	X(UNSIGNED40, 0x18, 5) \
	X(UNSIGNED48, 0x19, 6) \
	X(UNSIGNED56, 0x1A, 7) \
	X(UNSIGNED64, 0x1B, 8) \
	X(PDO_COMMUNICATION_PARAMETER, 0x20, 0) \
	X(PDO_MAPPING, 0x21, 0) \
	X(SDO_PARAMETER, 0x22, 0) \
	X(IDENTITY, 0x23, 0)

enum canopen_type {
#define X(name, number, ...) CANOPEN_ ## name = number,
CANOPEN_TYPES
#undef X
};

size_t canopen_type_size(enum canopen_type type);

int canopen_type_is_signed_integer(enum canopen_type type);
int canopen_type_is_unsigned_integer(enum canopen_type type);

static inline int canopen_type_is_real(enum canopen_type type)
{
	return type == CANOPEN_REAL32 || type == CANOPEN_REAL64;
}

static inline int canopen_type_is_integer(enum canopen_type type)
{
	return canopen_type_is_signed_integer(type)
	    || canopen_type_is_unsigned_integer(type);
}

static inline int canopen_type_is_number(enum canopen_type type)
{
	return canopen_type_is_integer(type)
	    || canopen_type_is_real(type);
}

static inline int canopen_type_is_string(enum canopen_type type)
{
	return type == CANOPEN_VISIBLE_STRING
	    || type == CANOPEN_OCTET_STRING
	    || type == CANOPEN_UNICODE_STRING;
}

const char* canopen_type_to_string(enum canopen_type type);
enum canopen_type canopen_type_from_string(const char* str);

#endif /* _CANOPEN_TYPES_H */
