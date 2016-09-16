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

#include <unistd.h>
#include <stdint.h>

static inline char nibble_to_char(int nibble)
{
	static char map[] = {
		'0', '1', '2', '3',
		'4', '5', '6', '7',
		'8', '9', 'A', 'B',
		'C', 'D', 'E', 'F'
	};

	return map[nibble];
}

static inline uint8_t get_low_nibble(uint8_t data)
{
	return data & 0x0f;
}

static inline uint8_t get_high_nibble(uint8_t data)
{
	return (data & 0xf0) >> 4;
}

const char* hexdump(const void* data, size_t size)
{
	const char* p = data;
	static __thread char buffer[256];

	size_t k = 0;
	for(size_t i = 0; i < size && k + 2 < sizeof(buffer); ++i) {
		buffer[k++] = nibble_to_char(get_high_nibble(p[i]));
		buffer[k++] = nibble_to_char(get_low_nibble(p[i]));
	}
	buffer[k] = '\0';

	return buffer;
}
