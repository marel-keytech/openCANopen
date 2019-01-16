#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "canopen/error.h"

static const char* cia302_lookup(uint16_t code)
{
	switch (code) {
#define X(c, str) \
	case c: return str;
#include "canopen/cia302-error-codes.h"
#undef X
	}

	return NULL;
}

static const char* cia402_lookup(uint16_t code)
{
	switch (code) {
#define X(c, str) \
	case c: return str;
#include "canopen/cia402-error-codes.h"
#undef X
	}

	return cia302_lookup(code);
}

static inline void
append_code(char* dst, uint16_t code, const char* (*lookup)(uint16_t))
{
	const char* s = lookup(code);

	if (s) {
		if (dst[0]) strcat(dst, ": ");
		strcat(dst, s);
	}
}

static const char*
convert_to_string(uint16_t code, const char* (*lookup)(uint16_t))
{
	static char buf[256];
	uint16_t current_code, last_code = 0;

	if (code == 0)
		return lookup(0);

	memset(buf, 0, sizeof(buf));

#define X(mask) \
	current_code = code & mask; \
	if (current_code != last_code) \
		append_code(buf, current_code, lookup); \
	last_code = current_code;

	X(0xf000)
	X(0xff00)
	X(0xfff0)
	X(0xffff)

#undef X

	return buf;
}

const char* error_code_to_string(uint16_t code, int profile)
{
	switch (profile) {
	case 402: return convert_to_string(code, cia402_lookup);
	case 302:
	default: return convert_to_string(code, cia302_lookup);
	}
}
