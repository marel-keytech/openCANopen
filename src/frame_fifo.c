#include <errno.h>
#include <time.h>
#include "frame_fifo.h"

#define nSEC_IN_SEC 1000000000

#define msec_to_nsec(t) ((t) * 1000000)

int frame_fifo_init(struct frame_fifo* self)
{
	self->count = 0;
	self->start = 0;
	self->stop = 0;

	pthread_mutex_init(&self->lock_mutex_, NULL);
	pthread_mutex_init(&self->suspend_mutex_, NULL);
	pthread_cond_init(&self->suspend_cond_, NULL);

	return 0;
}

void frame_fifo_clear(struct frame_fifo* self)
{
	pthread_mutex_destroy(&self->lock_mutex_);
	pthread_mutex_destroy(&self->suspend_mutex_);
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

	frame_fifo__unlock(self);

	pthread_mutex_lock(&self->suspend_mutex_);
	pthread_cond_broadcast(&self->suspend_cond_);
	pthread_mutex_unlock(&self->suspend_mutex_);
}

static inline uint64_t timespec_to_ns(struct timespec* ts)
{
	return ts->tv_sec * nSEC_IN_SEC + ts->tv_nsec;
}

static inline struct timespec ns_to_timespec(uint64_t ns)
{
	struct timespec ts;
	ts.tv_nsec = ns % nSEC_IN_SEC;
	ts.tv_sec = ns / nSEC_IN_SEC;
	return ts;
}

static inline void add_to_timespec(struct timespec* ts, uint64_t addition)
{
	*ts = ns_to_timespec(timespec_to_ns(ts) + addition);
}

static int block_while_empty(struct frame_fifo* self, int timeout)
{
	struct timespec deadline;
	int r = 0;

	if (timeout >= 0) {
		clock_gettime(CLOCK_MONOTONIC, &deadline);
		add_to_timespec(&deadline, msec_to_nsec(timeout));
	}

	pthread_mutex_lock(&self->suspend_mutex_);

	if (timeout < 0) {
		while (self->count == 0)
			pthread_cond_wait(&self->suspend_cond_,
					  &self->suspend_mutex_);
	} else {
		while (self->count == 0)
			if (pthread_cond_timedwait(&self->suspend_cond_,
						   &self->suspend_mutex_,
						   &deadline) == ETIMEDOUT) {
				r = ETIMEDOUT;
				goto done;
			}
	}

done:
	pthread_mutex_unlock(&self->suspend_mutex_);
	return r;
}

int frame_fifo_dequeue(struct frame_fifo* self, struct can_frame* frame,
		       int timeout)
{
	if (timeout == 0) {
		if (self->count == 0) {
			errno = EAGAIN;
			return -1;
		}
	} else {
		if (block_while_empty(self, timeout) == ETIMEDOUT) {
			errno = ETIMEDOUT;
			return -1;
		}
	}

	*frame = *frame_fifo_head(self);

	frame_fifo__lock(self);

	if (self->count > 0) {
		advance_start(self);
		--self->count;
	}

	frame_fifo__unlock(self);

	return 1;
}

void frame_fifo_flush(struct frame_fifo* self)
{
	self->count = 0;
	self->start = 0;
	self->stop = 0;
}

