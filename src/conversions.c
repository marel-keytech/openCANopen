#include <stdio.h>
#include <bsd/string.h>
#include "canopen/byteorder.h"
#include "conversions.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define MAKE_TOSTRING(name, type_, fmt) \
char* canopen_ ## name ## _tostring(char* dst, size_t dst_size, \
				    struct canopen_data* src) \
{ \
	if (src->size > canopen_type_size(src->type)) \
		return NULL; \
	type_ value = 0; \
	byteorder2(&value, src->data, sizeof(value), src->size); \
	snprintf(dst, dst_size - 1, fmt, value); \
	dst[dst_size - 1] = '\0'; \
	return dst; \
}

MAKE_TOSTRING(uint, uint64_t, "%llu")
MAKE_TOSTRING(float, float, "%e")
MAKE_TOSTRING(double, double, "%e")

char* canopen_int_tostring(char* dst, size_t dst_size, struct canopen_data* src)
{
	size_t size = canopen_type_size(src->type);

	if (src->size > size)
		return NULL;

	/* The CAN bus network-order is little-endian */
	uint64_t buffer = 0;
	memcpy(&buffer, src->data, src->size);

	unsigned char* byte = (unsigned char*)&buffer;

	int64_t value = 0;
	byteorder(&value, &buffer, sizeof(value));

	if (size != 8 && byte[size - 1] & 0x80)
		value -= 1LLU << (size << 3);

	snprintf(dst, dst_size - 1, "%lld", value);
	dst[dst_size - 1] = '\0';

	return dst;
}

char* canopen_bool_tostring(char* dst, size_t dst_size,
			    struct canopen_data* src)
{
	if (src->size != 1)
		return NULL;

	char* byte = src->data;
	strlcpy(dst, byte[0] ? "true" : "false", dst_size);
	return dst;
}

char* canopen_string_tostring(char* dst, size_t dst_size,
			      struct canopen_data* src)
{
	size_t size = MIN(dst_size, src->size + 1);
	memcpy(dst, src->data, size);
	dst[size - 1] = '\0';
	return dst;
}

char* canopen_data_tostring(char* dst, size_t dst_size,
			    struct canopen_data* src)
{
	if (src->type == CANOPEN_BOOLEAN)
		return canopen_bool_tostring(dst, dst_size, src);

	if (canopen_type_is_unsigned_integer(src->type))
		return canopen_uint_tostring(dst, dst_size, src);

	if (canopen_type_is_signed_integer(src->type))
		return canopen_int_tostring(dst, dst_size, src);

	if (src->type == CANOPEN_REAL32)
		return canopen_float_tostring(dst, dst_size, src);

	if (src->type == CANOPEN_REAL64)
		return canopen_double_tostring(dst, dst_size, src);

	if (canopen_type_is_string(src->type))
		return canopen_string_tostring(dst, dst_size, src);

	return NULL;
}

int canopen_bool_fromstring(struct canopen_data* dst, char* str)
{
	dst->data = &dst->value;
	dst->size = 1;
	unsigned char* byte = dst->data;

	if (strcmp(str, "true") == 0)
		*byte = 1;
	else if (strcmp(str, "false") == 0)
		*byte = 0;
	else
		return -1;

	return 0;
}

int canopen_uint_fromstring(struct canopen_data* dst, enum canopen_type type,
			    char* str)
{
	if (*str == '-' || *str == ' ')
		return -1;

	dst->data = &dst->value;
	dst->size = canopen_type_size(type);
	dst->value = 0;

	char* end = NULL;
	uint64_t host_order = strtoull(str, &end, 0);
	byteorder(dst->data, &host_order, sizeof(host_order));

	return (*str != '\0' && *end == '\0') ? 0 : -1;
}

int canopen_int_fromstring(struct canopen_data* dst, enum canopen_type type,
			   char* str)
{
	if (*str == ' ')
		return -1;

	dst->data = &dst->value;
	dst->size = canopen_type_size(type);
	dst->value = 0;

	char* end = NULL;
	int64_t host_order = strtoll(str, &end, 0);
	byteorder(dst->data, &host_order, sizeof(host_order));

	return (*str != '\0' && *end == '\0') ? 0 : -1;
}

int canopen_float_fromstring(struct canopen_data* dst, enum canopen_type type,
			     char* str)
{
	if (*str == ' ')
		return -1;

	dst->data = &dst->value;
	dst->size = canopen_type_size(type);
	dst->value = 0;

	char* end = NULL;
	float host_order = strtof(str, &end);
	byteorder(dst->data, &host_order, sizeof(host_order));

	return (*str != '\0' && *end == '\0') ? 0 : -1;
}

int canopen_double_fromstring(struct canopen_data* dst, enum canopen_type type,
			      char* str)
{
	if (*str == ' ')
		return -1;

	dst->data = &dst->value;
	dst->size = canopen_type_size(type);
	dst->value = 0;

	char* end = NULL;
	double host_order = strtod(str, &end);
	byteorder(dst->data, &host_order, sizeof(host_order));

	return (*str != '\0' && *end == '\0') ? 0 : -1;
}

int canopen_string_fromstring(struct canopen_data* dst, char* str)
{
	dst->data = str;
	dst->size = strlen(str);
	return 0;
}

int canopen_data_fromstring(struct canopen_data* dst, enum canopen_type type,
			    char* str)
{
	dst->type = type;

	if (type == CANOPEN_BOOLEAN)
		return canopen_bool_fromstring(dst, str);

	if (canopen_type_is_unsigned_integer(type))
		return canopen_uint_fromstring(dst, type, str);

	if (canopen_type_is_signed_integer(type))
		return canopen_int_fromstring(dst, type, str);

	if (type == CANOPEN_REAL32)
		return canopen_float_fromstring(dst, type, str);

	if (type == CANOPEN_REAL64)
		return canopen_double_fromstring(dst, type, str);

	if (canopen_type_is_string(type))
		return canopen_string_fromstring(dst, str);

	return -1;
}

