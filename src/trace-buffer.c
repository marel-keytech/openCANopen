/* Copyright (c) 2018, Marel
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

#include "trace-buffer.h"

#include "socketcan.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

static inline unsigned long clzl(unsigned long x)
{
	return __builtin_clzl(x);
}

static inline unsigned long round_up_to_power_of_2(unsigned long x)
{
	return 1UL << ((sizeof(x) << 3) - clzl(x - 1UL));
}

int tb_init(struct tracebuffer* self, size_t size)
{
	memset(self, 0, sizeof(*self));

	self->length = round_up_to_power_of_2(size / sizeof(self->data[0]));
	self->data = malloc(self->length * sizeof(self->data[0]));

	return self->data ? 0 : -1;
}

void tb_destroy(struct tracebuffer* self)
{
	free(self->data);
}

void tb_append(struct tracebuffer* self, const struct can_frame* frame)
{
	self->data[self->index++] = *frame;
	self->index &= (self->length - 1);

	if (self->count < self->length)
		self->count++;
}

void tb_dump(const struct tracebuffer* self, FILE* stream)
{
	if (self->count < self->length) {
		fwrite(self->data, sizeof(self->data[0]), self->count, stream);
	} else {
		fwrite(&self->data[self->index], sizeof(self->data[0]),
		       self->length - self->index, stream);

		fwrite(self->data, sizeof(self->data[0]), self->index, stream);
	}

	fflush(stream);
}
