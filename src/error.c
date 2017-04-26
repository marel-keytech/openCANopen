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

static const char*
convert_to_string(uint16_t code, const char* (*lookup)(uint16_t))
{
	static char buf[256];

	memset(buf, 0, sizeof(buf));

	for (uint16_t mask = 0xf000; mask != 0xffff; mask |= mask >> 4) {
		if (mask != 0xf000 && (code & mask) == (code & (mask << 4)))
			continue;

		const char* s = lookup(code & mask);

		if (s) {
			if (buf[0]) strcat(buf, ": ");
			strcat(buf, s);
		}
	}

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
