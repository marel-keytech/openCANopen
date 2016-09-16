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

#ifndef CANOPEN_EMCY_H_
#define CANOPEN_EMCY_H_

#include <string.h>
#include <stdint.h>
#include <linux/can.h>
#include "canopen/byteorder.h"

static inline unsigned int emcy_get_code(const struct can_frame* frame)
{
	uint16_t result = 0;
	byteorder(&result, frame->data, sizeof(result));
	return result;
}

static inline unsigned int emcy_get_register(const struct can_frame* frame)
{
	uint8_t result = 0;
	byteorder(&result, &frame->data[2], sizeof(result));
	return result;
}

static inline uint64_t emcy_get_manufacturer_error(const struct can_frame* frame)
{
	uint64_t tmp = 0;
	uint64_t result = 0;

	memcpy(&tmp, &frame->data[4], 5);

	byteorder(&result, &tmp, sizeof(result));

	return result;
}

#endif /* CANOPEN_EMCY_H_ */

