#ifndef PRIOQ_H_
#define PRIOQ_H_

#include <stdlib.h>
#include <unistd.h>

struct prioq_elem {
	unsigned long priority;
	void* data;
};

struct prioq {
	size_t size;
	unsigned long index;
	struct prioq_elem* head;
};

int prioq_init(struct prioq* self, size_t size);

static inline void prioq_clear(struct prioq* self)
{
	free(self->head);
}

int prioq_grow(struct prioq* self, size_t size);

int prioq_insert(struct prioq* self, unsigned long priority, void* data);

void prioq_pop(struct prioq* self);

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

unsigned long _prioq_get_smaller_child(struct prioq* self, unsigned long index);
void _prioq_bubble_up(struct prioq* self, unsigned long index);
void _prioq_sink_down(struct prioq* self, unsigned long index);

#endif /* PRIOQ_H_ */

