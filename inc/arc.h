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

#ifndef ARC_H_
#define ARC_H_

#include "co_atomic.h"

#ifdef ARC_DEBUG
#include <stdio.h>
#define ARC_PRINT_DEBUG(name, type) \
	printf(ARC_STR(name) "_" type "(%p) = %d\n", self, ref);
#else
#define ARC_PRINT_DEBUG(...)
#endif

#define ARC_STR(x) #x

#define ARC_GENERATE_REF(name) \
int name ## _ref(struct name* self) \
{ \
	int ref = co_atomic_add_fetch(&self->ref, 1); \
	ARC_PRINT_DEBUG(name, "ref") \
	return ref; \
}

#define ARC_GENERATE_UNREF(name, free_fn) \
int name ## _unref(struct name* self) \
{ \
	int ref = co_atomic_sub_fetch(&self->ref, 1); \
	ARC_PRINT_DEBUG(name, "unref") \
	if (ref == 0) \
		free_fn(self); \
	return ref; \
}

#define ARC_GENERATE(name, free_fn) \
	ARC_GENERATE_REF(name) \
	ARC_GENERATE_UNREF(name, free_fn)

#define ARC_PROTOTYPE(name) \
int name ## _ref(struct name* self); \
int name ## _unref(struct name* self);

#endif /* ARC_H_ */
