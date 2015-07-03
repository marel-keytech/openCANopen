#ifndef PTR_FIFO_H_
#define PTR_FIFO_H_

#ifndef PTR_FIFO_LENGTH
#define PTR_FIFO_LENGTH 64
#endif

#include <pthread.h>

typedef void (*ptr_fifo_free_fn)(void*);

struct ptr_fifo__elem {
	void* ptr_;
	ptr_fifo_free_fn free_fn;
};

struct ptr_fifo {
	pthread_mutex_t mutex_;
	unsigned int start, stop;
	size_t count;
	struct ptr_fifo__elem data[PTR_FIFO_LENGTH];
};


int ptr_fifo_init(struct ptr_fifo* self);
void ptr_fifo_flush(struct ptr_fifo* self);
void ptr_fifo_destroy(struct ptr_fifo* self);

void ptr_fifo_enqueue(struct ptr_fifo* self, void* ptr, ptr_fifo_free_fn fn);
void* ptr_fifo_dequeue(struct ptr_fifo* self);

void* ptr_fifo_peek(struct ptr_fifo* self);

static inline
void ptr_fifo__lock(struct ptr_fifo* self)
{
	pthread_mutex_lock(&self->mutex_);
}

static inline
void ptr_fifo__unlock(struct ptr_fifo* self)
{
	pthread_mutex_unlock(&self->mutex_);
}

#endif /* PTR_FIFO_H_ */

