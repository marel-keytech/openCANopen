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

#ifndef PRIOQ_H_
#define PRIOQ_H_

#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

struct prioq_elem {
	unsigned long priority;
	unsigned long sequence_;
	void* data;
};

struct prioq {
	size_t size;
	unsigned long sequence;
	unsigned long index;
	pthread_mutex_t mutex;
	pthread_cond_t suspend_cond;
	struct prioq_elem* head;
};

int prioq_init(struct prioq* self, size_t size);

void prioq_destroy(struct prioq* self);

void prioq_clear(struct prioq* self);

int prioq_copy(struct prioq* dst, struct prioq* src);
int prioq_move(struct prioq* dst, struct prioq* src);

int prioq_grow(struct prioq* self, size_t size);

int prioq_insert(struct prioq* self, unsigned long priority, void* data);

int prioq_pop(struct prioq* self, struct prioq_elem* elem, int timeout);

static inline unsigned long prioq__parent(unsigned long index)
{
	return (index - 1) >> 1;
}

static inline unsigned long prioq__lchild(unsigned long index)
{
	return (index << 1) + 1;
}

static inline unsigned long prioq__rchild(unsigned long index)
{
	return (index << 1) + 2;
}

static inline void prioq__swap(struct prioq_elem* a, struct prioq_elem* b)
{
	struct prioq_elem tmp = *a;
	*a = *b;
	*b = tmp;
}

static inline void prioq__lock(struct prioq* self)
{
	pthread_mutex_lock(&self->mutex);
}

static inline void prioq__unlock(struct prioq* self)
{
	pthread_mutex_unlock(&self->mutex);
}

int prioq__is_seq_lt(unsigned long a, unsigned long b);

unsigned long prioq__get_smaller_child(struct prioq* self, unsigned long index);
void prioq__bubble_up(struct prioq* self, unsigned long index);
void prioq__sink_down(struct prioq* self, unsigned long index);

#endif /* PRIOQ_H_ */

