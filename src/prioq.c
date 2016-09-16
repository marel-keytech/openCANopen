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

#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include "prioq.h"
#include "thread-utils.h"

int prioq_init(struct prioq* self, size_t size)
{
	self->sequence = 0;
	self->size = size;
	self->index = 0;
	self->head = calloc(size, sizeof(*self->head));

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&self->mutex, &attr);
	pthread_mutexattr_destroy(&attr);

	pthread_cond_init(&self->suspend_cond, NULL);

	return self->head ? 0 : -1;
}

void prioq_destroy(struct prioq* self)
{
	pthread_mutex_destroy(&self->mutex);
	pthread_cond_destroy(&self->suspend_cond);
	free(self->head);
}

void prioq_clear(struct prioq* self)
{
	prioq__lock(self);
	self->sequence = 0;
	self->index = 0;
	prioq__unlock(self);
}

int prioq_copy(struct prioq* dst, struct prioq* src)
{
	int rc;

	prioq__lock(dst);
	prioq__lock(src);

	rc = prioq_grow(dst, src->index);
	if (rc < 0)
		goto done;

	dst->index = src->index;
	dst->sequence = src->sequence;

	memcpy(dst->head, src->head, dst->index * sizeof(*dst->head));

	rc = 0;
done:
	prioq__unlock(src);
	prioq__unlock(dst);

	return rc;
}

int prioq_move(struct prioq* dst, struct prioq* src)
{
	int rc;

	prioq__lock(dst);
	prioq__lock(src);

	rc = prioq_copy(dst, src);
	if (rc < 0)
		goto done;

	prioq_clear(src);

	rc = 0;
done:
	prioq__unlock(src);
	prioq__unlock(dst);

	return rc;
}

int prioq_grow(struct prioq* self, size_t size)
{
	int rc;

	prioq__lock(self);

	if (self->size >= size) {
		rc = 0;
		goto done;
	}

	struct prioq_elem* new_head = realloc(self->head,
					      size * sizeof(*self->head));
	if (!new_head) {
		rc = -1;
		goto done;
	}

	self->size = size;
	self->head = new_head;

	rc = 1;
done:
	prioq__unlock(self);
	return rc;
}

int prioq_insert(struct prioq* self, unsigned long priority, void* data)
{
	int rc = -1;

	prioq__lock(self);

	if (self->index >= self->size)
		if (prioq_grow(self, self->size*2))
			goto done;

	struct prioq_elem* elem = &self->head[self->index++];

	elem->priority = priority;
	elem->data = data;
	elem->sequence_ = self->sequence++;

	prioq__bubble_up(self, self->index - 1);

	pthread_cond_signal(&self->suspend_cond);

	rc = 0;
done:
	prioq__unlock(self);
	return rc;
}

int prioq_pop(struct prioq* self, struct prioq_elem* elem, int timeout)
{
	prioq__lock(self);

	int rc = block_thread_while_empty(&self->suspend_cond, &self->mutex,
					  timeout, self->index == 0);
	if (rc < 0)
		goto done;

	assert(self->index > 0);

	*elem = self->head[0];
	self->head[0] = self->head[--self->index];

	prioq__sink_down(self, 0);

	rc = 1;
done:
	prioq__unlock(self);
	return rc;
}

/*
 * Imagine the integer runs in a circle in the middle of a field and the
 * quarters on the field are thusly named:
 *
 * 4 | 1
 * -----
 * 3 | 2
 */
static inline int is_on_1st_quarter(unsigned long i)
{
	return i <= ULONG_MAX / 4;
}

static inline int is_on_4th_quarter(unsigned long i)
{
	return ULONG_MAX / 4 * 3 < i && i <= ULONG_MAX;
}

int prioq__is_seq_lt(unsigned long a, unsigned long b)
{
	if (is_on_4th_quarter(a) && is_on_1st_quarter(b))
		return 1;

	if (is_on_1st_quarter(a) && is_on_4th_quarter(b))
		return 0;

	return a < b;
}

static inline int is_lt(struct prioq_elem* a, struct prioq_elem* b)
{
	if (a->priority < b->priority)
		return 1;

	if (a->priority > b->priority)
		return 0;

	return prioq__is_seq_lt(a->sequence_, b->sequence_);
}

void prioq__bubble_up(struct prioq* self, unsigned long index)
{
	if (index == 0)
		return;

	struct prioq_elem* elem = &self->head[index];
	struct prioq_elem* parent = &self->head[prioq__parent(index)];

	if (is_lt(elem, parent)) {
		prioq__swap(elem, parent);
		prioq__bubble_up(self, prioq__parent(index));
	}
}

unsigned long prioq__get_smaller_child(struct prioq* self, unsigned long index)
{
	unsigned long left_index = prioq__lchild(index);
	unsigned long right_index = prioq__rchild(index);

	struct prioq_elem* elem = &self->head[index];
	struct prioq_elem* left_elem = &self->head[left_index];
	struct prioq_elem* right_elem = &self->head[right_index];

	if (right_index < self->index && is_lt(right_elem, elem))
		return is_lt(right_elem, left_elem) ? right_index : left_index;

	if (left_index < self->index && is_lt(left_elem, elem))
		return left_index;

	return 0;
}

void prioq__sink_down(struct prioq* self, unsigned long index)
{
	unsigned long smaller_child = prioq__get_smaller_child(self, index);
	if (smaller_child == 0)
		return;

	struct prioq_elem* parent = &self->head[index];
	struct prioq_elem* child = &self->head[smaller_child];

	prioq__swap(parent, child);
	prioq__sink_down(self, smaller_child);
}

