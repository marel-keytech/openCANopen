#include <errno.h>
#include <time.h>
#include "frame_fifo.h"
#include "thread-utils.h"

int frame_fifo_init(struct frame_fifo* self)
{
	self->count = 0;
	self->start = 0;
	self->stop = 0;

	pthread_mutex_init(&self->mutex_, NULL);
	pthread_cond_init(&self->suspend_cond_, NULL);

	return 0;
}

void frame_fifo_clear(struct frame_fifo* self)
{
	pthread_mutex_destroy(&self->mutex_);
	pthread_cond_destroy(&self->suspend_cond_);
}

static inline void advance_stop(struct frame_fifo* self)
{
	if (++self->stop >= FRAME_FIFO_LENGTH)
		self->stop = 0;
}

static inline void advance_start(struct frame_fifo* self)
{
	if (++self->start >= FRAME_FIFO_LENGTH)
		self->start = 0;
}

void frame_fifo_enqueue(struct frame_fifo* self, const struct can_frame* frame)
{
	frame_fifo__lock(self);

	self->frame[self->stop] = *frame;

	advance_stop(self);

	if (self->count >= FRAME_FIFO_LENGTH)
		advance_start(self);
	else
		++self->count;

	pthread_cond_signal(&self->suspend_cond_);

	frame_fifo__unlock(self);
}

int frame_fifo_dequeue(struct frame_fifo* self, struct can_frame* frame,
		       int timeout)
{
	frame_fifo__lock(self);

	int rc = block_thread_while_empty(&self->suspend_cond_, &self->mutex_,
					  timeout, self->count == 0);
	if (rc < 0)
		goto done;

	*frame = *frame_fifo_head(self);

	advance_start(self);
	--self->count;

	rc = 1;
done:
	frame_fifo__unlock(self);
	return rc;
}

void frame_fifo_flush(struct frame_fifo* self)
{
	self->count = 0;
	self->start = 0;
	self->stop = 0;
}

