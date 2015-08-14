#ifndef _CANOPEN_TYPES_H
#define _CANOPEN_TYPES_H

#include <assert.h>
#include <stdint.h>
#include "canopen/byteorder.h"

enum canopen_type {
	CANOPEN_BOOLEAN          = 0x01,
	CANOPEN_INTEGER8         = 0x02,
	CANOPEN_INTEGER16        = 0x03,
	CANOPEN_INTEGER32        = 0x04,
	CANOPEN_UNSIGNED8        = 0x05,
	CANOPEN_UNSIGNED16       = 0x06,
	CANOPEN_UNSIGNED32       = 0x07,
	CANOPEN_REAL32           = 0x08,
	CANOPEN_VISIBLE_STRING   = 0x09,
	CANOPEN_OCTET_STRING     = 0x0A,
	CANOPEN_UNICODE_STRING   = 0x0B,
	CANOPEN_TIME_OF_DAY      = 0x0C,
	CANOPEN_TIME_DIFFERENCE  = 0x0D,
/*	RESERVED                 = 0x0E, */
	CANOPEN_DOMAIN           = 0x0F,
	CANOPEN_INTEGER24        = 0x10,
	CANOPEN_REAL64           = 0x11,
	CANOPEN_INTEGER40        = 0x12,
	CANOPEN_INTEGER48        = 0x13,
	CANOPEN_INTEGER56        = 0x14,
	CANOPEN_INTEGER64        = 0x15,
	CANOPEN_UNSIGNED24       = 0x16,
/*	RESERVED                 = 0x17, */
	CANOPEN_UNSIGNED40       = 0x18,
	CANOPEN_UNSIGNED48       = 0x19,
	CANOPEN_UNSIGNED56       = 0x1A,
	CANOPEN_UNSIGNED64       = 0x1B,
/*	RESERVED                 = 0x1C -- 0x001F, */
	CANOPEN_PDO_COMMUNICATION_PARAMETER = 0x20,
	CANOPEN_PDO_MAPPING      = 0x21,
	CANOPEN_SDO_PARAMETER    = 0x22,
	CANOPEN_IDENTITY         = 0x23,
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

#endif /* _CANOPEN_TYPES_H */
