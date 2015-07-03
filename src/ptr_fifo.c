#include <assert.h>
#include "ptr_fifo.h"

int ptr_fifo_init(struct ptr_fifo* self)
{
	self->count = 0;
	self->start = 0;
	self->stop = 0;

	pthread_mutex_init(&self->mutex_, NULL);

	return 0;
}

void ptr_fifo_flush(struct ptr_fifo* self)
{
	ptr_fifo__lock(self);

	for (int i = 0; i < self->count; ++i) {
		struct ptr_fifo__elem* elem = &self->data[i];

		if (elem->free_fn)
			elem->free_fn(elem->ptr_);
	}

	self->count = 0;
	self->start = 0;
	self->stop = 0;

	ptr_fifo__unlock(self);
}

void ptr_fifo_destroy(struct ptr_fifo* self)
{
	ptr_fifo_flush(self);
	pthread_mutex_destroy(&self->mutex_);
}

static inline void advance_stop(struct ptr_fifo* self)
{
	if (++self->stop >= PTR_FIFO_LENGTH)
		self->stop = 0;
}

static inline void advance_start(struct ptr_fifo* self)
{
	if (++self->start >= PTR_FIFO_LENGTH)
		self->start = 0;
}

void ptr_fifo_enqueue(struct ptr_fifo* self, void* ptr, ptr_fifo_free_fn fn)
{
	ptr_fifo__lock(self);

	struct ptr_fifo__elem* elem = &self->data[self->stop];

	if (self->count >= PTR_FIFO_LENGTH && elem->ptr_ && elem->free_fn)
		elem->free_fn(elem->ptr_);

	elem->ptr_ = ptr;
	elem->free_fn = fn;

	advance_stop(self);

	if (self->count >= PTR_FIFO_LENGTH)
		advance_start(self);
	else
		++self->count;

	ptr_fifo__unlock(self);
}

void* ptr_fifo_dequeue(struct ptr_fifo* self)
{
	void* result = NULL;
	ptr_fifo__lock(self);

	if (self->count == 0)
		goto done;

	struct ptr_fifo__elem* elem = &self->data[self->start];
	assert (elem->ptr_);
	result = elem->ptr_;

	elem->ptr_ = NULL;
	elem->free_fn = NULL;

	advance_start(self);
	--self->count;

done:
	ptr_fifo__unlock(self);
	return result;
}

void* ptr_fifo_peek(struct ptr_fifo* self)
{
	void* result = NULL;
	ptr_fifo__lock(self);

	if (self->count == 0)
		goto done;

	const struct ptr_fifo__elem* elem = &self->data[self->start];
	assert (elem->ptr_);
	result = elem->ptr_;

done:
	ptr_fifo__unlock(self);
	return result;
}

