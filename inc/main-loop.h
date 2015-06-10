#ifndef MAIN_LOOP_H_
#define MAIN_LOOP_H_

#include <eloop.h>
#include <string.h>
#include "worker.h"

extern int ml_errno;

struct ml_timer {
	void (*fn)(void* context);
	void* context;
	int timeout;
	int is_periodic;
	eloop_timer_t handle_;
};

struct ml_handler {
	void (*fn)(int fd, void* context);
	void* context;
	int fd;
	eloop_hdlr_t handle_;
};

struct ml_job {
	void (*work_fn)(void* context);
	void (*after_work_fn)(void* context);
	unsigned long priority;
	void* context;
};

int ml_init();
void ml_deinit();

int ml_run();
void ml_stop();

static inline int ml_timer_init(struct ml_timer* timer)
{
	memset(timer, 0, sizeof(*timer));
	return 0;
}

static inline void ml_timer_clear(struct ml_timer* timer)
{
	(void)timer;
	/* do nothing */
}

int ml_timer_start(struct ml_timer* timer);
int ml_timer_stop(struct ml_timer* timer);

static inline int ml_handler_init(struct ml_handler* handler)
{
	memset(handler, 0, sizeof(*handler));
	return 0;
}

static inline void ml_handler_clear(struct ml_handler* handler)
{
	(void)handler;
	/* do nothing */
}

int ml_handler_start(struct ml_handler* handler);
int ml_handler_stop(struct ml_handler* handler);

static inline int ml_job_init(struct ml_job* job)
{
	memset(job, 0, sizeof(*job));
	return 0;
}

static inline void  ml_job_clear(struct ml_job* job)
{
	(void)job;
	/* do nothing */
}

int ml_job_enqueue(struct ml_job* job);

static inline const char* ml_strerror(int error)
{
	return eloop_strerror((eloop_error_t)error);
}

#endif /* MAIN_LOOP_H_ */

