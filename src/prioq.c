#include <assert.h>
#include "prioq.h"


int prioq_init(struct prioq* self, size_t size)
{
	self->size = size;
	self->index = 0;
	self->head = calloc(size, sizeof(*self->head));
	return self->head ? 0 : -1;
}

int prioq_grow(struct prioq* self, size_t size)
{
	if (self->size >= size)
		return 0;

	struct prioq_elem* new_head = realloc(self->head,
					      size*sizeof(*self->head));
	if (!new_head)
		return -1;

	self->head = new_head;

	return 1;
}

int prioq_insert(struct prioq* self, unsigned long priority, void* data)
{
	if (self->index >= self->size)
		if (prioq_grow(self, self->size*2))
			return -1;

	struct prioq_elem* elem = &self->head[self->index++];

	elem->priority = priority;
	elem->data = data;

	_prioq_bubble_up(self, self->index - 1);

	return 0;
}

void prioq_pop(struct prioq* self)
{
	assert(self->index > 0);

	self->head[0] = self->head[--self->index];

	_prioq_sink_down(self, 0);
}

void _prioq_bubble_up(struct prioq* self, unsigned long index)
{
	if (index == 0)
		return;

	struct prioq_elem* elem = &self->head[index];
	struct prioq_elem* parent = &self->head[_prioq_parent(index)];

	if (elem->priority < parent->priority) {
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

	if (right_index < self->index && right_elem->priority < elem->priority)
		return right_elem->priority < left_elem->priority ? right_index
								  : left_index;

	if (left_index < self->index && left_elem->priority < elem->priority)
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

