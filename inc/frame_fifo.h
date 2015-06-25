#ifndef FRAME_FIFO_H_
#define FRAME_FIFO_H_

#ifndef FRAME_FIFO_LENGTH
#define FRAME_FIFO_LENGTH 64
#endif

#include <stdint.h>
#include <pthread.h>
#include "socketcan.h"

struct frame_fifo {
	pthread_mutex_t mutex_;
	pthread_cond_t suspend_cond_;

	size_t count;
	unsigned int start, stop;
	struct can_frame frame[FRAME_FIFO_LENGTH];
};

int frame_fifo_init(struct frame_fifo* self);
void frame_fifo_clear(struct frame_fifo* self);

void frame_fifo_enqueue(struct frame_fifo* self, const struct can_frame* frame);
int frame_fifo_dequeue(struct frame_fifo* self, struct can_frame* frame,
		       int timeout);

static inline const
struct can_frame* frame_fifo_head(const struct frame_fifo* self)
{
	return &self->frame[self->start];
}

static inline
size_t frame_fifo_length(const struct frame_fifo* self)
{
	return self->count;
}

static inline
void frame_fifo__lock(struct frame_fifo* self)
{
	pthread_mutex_lock(&self->mutex_);
}

static inline
void frame_fifo__unlock(struct frame_fifo* self)
{
	pthread_mutex_unlock(&self->mutex_);
}

void frame_fifo_flush(struct frame_fifo* self);

#endif /* FRAME_FIFO_H_ */
