#ifndef SDO_FIFO_H_
#define SDO_FIFO_H_

#ifndef SDO_FIFO_LENGTH
#define SDO_FIFO_LENGTH 64
#endif

#include <stdint.h>

struct sdo_elem_data {
	size_t size;
	char data[0];
};

enum sdo_elem_type { SDO_ELEM_DL, SDO_ELEM_UL };

struct sdo_elem {
	enum sdo_elem_type type;
	int index, subindex;
	struct sdo_elem_data* data;
};

struct sdo_fifo {
	size_t count;
	unsigned int start, stop;
	struct sdo_elem data[SDO_FIFO_LENGTH];
};

static inline void sdo_fifo_init(struct sdo_fifo* self)
{
	self->count = 0;
	self->start = 0;
	self->stop = 0;
}

static inline int sdo_fifo_enqueue(struct sdo_fifo* self,
				   enum sdo_elem_type type, int index,
				   int subindex, struct sdo_elem_data* data)
{
	if (self->count >= SDO_FIFO_LENGTH)
		return -1;

	struct sdo_elem* elem = &self->data[self->stop];
	elem->type = type;
	elem->index = index;
	elem->subindex = subindex;
	elem->data = data;

	if (++self->stop >= SDO_FIFO_LENGTH)
		self->stop = 0;

	++self->count;

	return 0;
}

static inline int sdo_fifo_dequeue(struct sdo_fifo* self)
{
	if (self->count == 0)
		return -1;

	if (++self->start >= SDO_FIFO_LENGTH)
		self->start = 0;

	--self->count;

	return 0;
}

static inline struct sdo_elem* sdo_fifo_head(struct sdo_fifo* self)
{
	return &self->data[self->start];
}

static inline size_t sdo_fifo_length(const struct sdo_fifo* self)
{
	return self->count;
}

#endif /* SDO_FIFO_H_ */

