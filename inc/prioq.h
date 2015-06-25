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

void prioq_clear(struct prioq* self);

int prioq_grow(struct prioq* self, size_t size);

int prioq_insert(struct prioq* self, unsigned long priority, void* data);

int prioq_pop(struct prioq* self, struct prioq_elem* elem, int timeout);

static inline unsigned long _prioq_parent(unsigned long index)
{
	return (index - 1) >> 1;
}

static inline unsigned long _prioq_lchild(unsigned long index)
{
	return (index << 1) + 1;
}

static inline unsigned long _prioq_rchild(unsigned long index)
{
	return (index << 1) + 2;
}

static inline void _prioq_swap(struct prioq_elem* a, struct prioq_elem* b)
{
	struct prioq_elem tmp = *a;
	*a = *b;
	*b = tmp;
}

static inline void _prioq_lock(struct prioq* self)
{
	pthread_mutex_lock(&self->mutex);
}

static inline void _prioq_unlock(struct prioq* self)
{
	pthread_mutex_unlock(&self->mutex);
}

int _prioq_is_seq_lt(unsigned long a, unsigned long b);

unsigned long _prioq_get_smaller_child(struct prioq* self, unsigned long index);
void _prioq_bubble_up(struct prioq* self, unsigned long index);
void _prioq_sink_down(struct prioq* self, unsigned long index);

#endif /* PRIOQ_H_ */

