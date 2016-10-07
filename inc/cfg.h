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

#ifndef CFG_H_
#define CFG_H_

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int cfg_load(void);
void cfg_unload(void);

const char* cfg__get_string(int nodeid, const char* key);

static inline const char* cfg_get_string(int nodeid, const char* key,
					 const char* def)
{
	const char* v = cfg__get_string(nodeid, key);
	return v ? v : def;
}

static inline int cfg_get_bool(int nodeid, const char* key, int def)
{
	const char* v = cfg__get_string(nodeid, key);
	return v ? strcmp(v, "yes") == 0 : def;
}

static inline int64_t cfg_get_int(int nodeid, const char* key, int64_t def)
{
	const char* v = cfg__get_string(nodeid, key);
	return v ? strtoll(v, NULL, 0) : def;
}

static inline uint64_t cfg_get_uint(int nodeid, const char* key, uint64_t def)
{
	const char* v = cfg__get_string(nodeid, key);
	return v ? strtoull(v, NULL, 0) : def;
}

#endif /* CFG_H_ */
