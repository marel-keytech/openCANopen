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

#ifndef _VECTOR_H_INCLUDED
#define _VECTOR_H_INCLUDED

#include <stdlib.h>
#include <string.h>

struct vector {
	void* data;
	size_t index;
	size_t size;
};

static inline int vector_init(struct vector* self, size_t size)
{
	memset(self, 0, sizeof(*self));
	self->size = size;
	self->data = malloc(size);
	return self->data ? 0 : -1;
}

static inline void vector_destroy(struct vector* self)
{
	free(self->data);
	self->data = NULL;
}

static inline int vector__grow(struct vector* self, size_t size)
{
	void* data = realloc(self->data, size);
	if (!data)
		return -1;
	self->data = data;
	self->size = size;
	return 1;
}

static inline int vector_reserve(struct vector* self, size_t size)
{
	return self->size < size ? vector__grow(self, size * 2) : 0;
}

static inline int vector_append(struct vector* self, const void* data,
				size_t size)
{
	if (vector_reserve(self, self->index + size) < 0)
		return -1;
	memcpy((char*)self->data + self->index, data, size);
	self->index += size;
	return 0;
}

static inline void vector_clear(struct vector* self)
{
	self->index = 0;
}

static inline int vector_assign(struct vector* self, const void* data,
				size_t size)
{
	vector_clear(self);
	if (vector_reserve(self, size) < 0)
		return -1;
	memcpy(self->data, data, size);
	self->index = size;
	return 0;
}

static inline int vector_fill(struct vector* self, int c, size_t size)
{
	vector_clear(self);
	if (vector_reserve(self, size) < 0)
		return -1;
	memset(self->data, c, size);
	self->index = size;
	return 0;
}

static inline int vector_copy(struct vector* dst, const struct vector* src)
{
	return vector_assign(dst, src->data, src->index);
}

#endif /* _VECTOR_H_INCLUDED */
