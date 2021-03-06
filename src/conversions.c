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

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "canopen/byteorder.h"
#include "conversions.h"

size_t strlcpy(char* dst, const char* src, size_t dsize);

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define MAKE_TOSTRING(name, type_, fmt) \
char* canopen_ ## name ## _tostring(char* dst, size_t dst_size, \
				    struct canopen_data* src) \
{ \
	if (!src->is_size_unknown && src->size > canopen_type_size(src->type)) \
		return NULL; \
	type_ value = 0; \
	byteorder2(&value, src->data, sizeof(value), src->size); \
	snprintf(dst, dst_size - 1, fmt, value); \
	dst[dst_size - 1] = '\0'; \
	return dst; \
}

MAKE_TOSTRING(uint, uint64_t, "%"PRIu64)
MAKE_TOSTRING(float, float, "%e")
MAKE_TOSTRING(double, double, "%e")

char* canopen_int_tostring(char* dst, size_t dst_size, struct canopen_data* src)
{
	size_t size = canopen_type_size(src->type);

	if (!src->is_size_unknown && src->size > size)
		return NULL;

	/* The CAN bus network-order is little-endian */
	uint64_t buffer = 0;
	memcpy(&buffer, src->data, src->size);

	unsigned char* byte = (unsigned char*)&buffer;

	int64_t value = 0;
	byteorder(&value, &buffer, sizeof(value));

	if (size != 8 && byte[size - 1] & 0x80)
		value -= 1LLU << (size << 3);

	snprintf(dst, dst_size - 1, "%"PRId64, value);
	dst[dst_size - 1] = '\0';

	return dst;
}

char* canopen_bool_tostring(char* dst, size_t dst_size,
			    struct canopen_data* src)
{
	if (!src->is_size_unknown && src->size != 1)
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

int canopen_bool_fromstring(struct canopen_data* dst, const char* str)
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

#define uint64_t_tonumber(str, end) strtoull(str, end, 0);
#define int64_t_tonumber(str, end) strtoll(str, end, 0);
#define float_tonumber(str, end) strtof(str, end);
#define double_tonumber(str, end) strtod(str, end);

#define tonumber(type, str, end) type ## _tonumber(str, end)

#define MAKE_FROMSTRING(name, type_, reject) \
int canopen_ ## name ## _fromstring(struct canopen_data* dst, \
				    enum canopen_type type, \
				    const char* str) \
{ \
	if (reject) \
		return -1; \
\
	dst->data = &dst->value; \
	dst->size = canopen_type_size(type); \
	dst->value = 0; \
\
	char* end = NULL; \
	type_ host_order = tonumber(type_, str, &end); \
	byteorder(dst->data, &host_order, sizeof(host_order)); \
\
	return (*str != '\0' && *end == '\0') ? 0 : -1; \
}

MAKE_FROMSTRING(uint, uint64_t, *str == ' ' || *str == '-')
MAKE_FROMSTRING(int, int64_t, *str == ' ')
MAKE_FROMSTRING(float, float, *str == ' ')
MAKE_FROMSTRING(double, double, *str == ' ')

int canopen_string_fromstring(struct canopen_data* dst, const char* str)
{
	dst->data = (char*)str;
	dst->size = strlen(str);
	return 0;
}

int canopen_data_fromstring(struct canopen_data* dst, enum canopen_type type,
			    const char* str)
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

