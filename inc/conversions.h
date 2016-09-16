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

#ifndef CONVERSIONS_H_
#define CONVERSIONS_H_

#include <stdint.h>
#include "canopen/types.h"

struct canopen_data {
	enum canopen_type type;
	void* data;
	size_t size;
	uint64_t value;
	int is_size_unknown;
};

char* canopen_data_tostring(char* dst, size_t size, struct canopen_data* src);
int canopen_data_fromstring(struct canopen_data* dst,
			    enum canopen_type expected_type, const char* str);

#endif /* CONVERSIONS_H_ */
