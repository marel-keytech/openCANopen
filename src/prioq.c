#include <limits.h>
#include <errno.h>
#include "prioq.h"

int prioq_init(struct prioq* self, size_t size)
{
	self->sequence = 0;
	self->size = size;
	self->index = 0;
	self->head = calloc(size, sizeof(*self->head));

	pthread_mutex_init(&self->mutex, NULL);

	return self->head ? 0 : -1;
}

int prioq_grow(struct prioq* self, size_t size)
{
	if (self->size >= size)
		return 0;

	_prioq_lock(self);

	struct prioq_elem* new_head = realloc(self->head,
					      size*sizeof(*self->head));
	if (!new_head)
		return -1;

	self->head = new_head;

	_prioq_unlock(self);

	return 1;
}

int prioq_insert(struct prioq* self, unsigned long priority, void* data)
{
	if (self->index >= self->size)
		if (prioq_grow(self, self->size*2))
			return -1;

	_prioq_lock(self);

	struct prioq_elem* elem = &self->head[self->index++];

	elem->priority = priority;
	elem->data = data;
	elem->sequence_ = self->sequence++;

	_prioq_bubble_up(self, self->index - 1);

	_prioq_unlock(self);

	return 0;
}

int prioq_pop(struct prioq* self, struct prioq_elem* elem)
{
	_prioq_lock(self);

	if (self->index == 0)
		return 0;

	*elem = self->head[0];
	self->head[0] = self->head[--self->index];

	_prioq_sink_down(self, 0);

	_prioq_unlock(self);

	return 1;
}

static inline int is_on_4th_quadrant(unsigned long i)
{
	return i <= ULONG_MAX / 4;
}

static inline int is_on_1st_quadrant(unsigned long i)
{
	return ULONG_MAX / 4 * 3 < i && i <= ULONG_MAX;
}

static int is_seq_lt(struct prioq_elem* a, struct prioq_elem* b)
{
	if (is_on_1st_quadrant(a->sequence_)
	 && is_on_4th_quadrant(b->sequence_))
		return 1;

	if (is_on_4th_quadrant(a->sequence_)
	 && is_on_1st_quadrant(b->sequence_))
		return 0;

	return a->sequence_ < b->sequence_;
}

static inline int is_lt(struct prioq_elem* a, struct prioq_elem* b)
{
	if (a->priority < b->priority)
		return 1;

	if (a->priority > b->priority)
		return 0;

	return is_seq_lt(a, b);
}

void _prioq_bubble_up(struct prioq* self, unsigned long index)
{
	if (index == 0)
		return;

	struct prioq_elem* elem = &self->head[index];
	struct prioq_elem* parent = &self->head[_prioq_parent(index)];

	if (is_lt(elem, parent)) {
		_prioq_swap(elem, parent);
		_prioq_bubble_up(self, _prioq_parent(index));
	}
}

unsigned long _prioq_get_smaller_child(struct prioq* self, unsigned long index)
{
	unsigned long left_index = _prioq_lchild(index);
	unsigned long right_index = _prioq_rchild(index);

	struct prioq_elem* elem = &self->head[index];
	struct prioq_elem* left_elem = &self->head[left_index];
	struct prioq_elem* right_elem = &self->head[right_index];

	if (right_index < self->index && is_lt(right_elem, elem))
		return is_lt(right_elem, left_elem) ? right_index : left_index;

	if (left_index < self->index && is_lt(left_elem, elem))
		return left_index;

	return 0;
}

void _prioq_sink_down(struct prioq* self, unsigned long index)
{
	unsigned long smaller_child = _prioq_get_smaller_child(self, index);
	if (smaller_child == 0)
		return;

	struct prioq_elem* parent = &self->head[index];
	struct prioq_elem* child = &self->head[smaller_child];

	_prioq_swap(parent, child);
	_prioq_sink_down(self, smaller_child);
}

