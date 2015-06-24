#include <unistd.h>
#include "main-loop.h"
#include "worker.h"

int ml_errno;

static int job_read_fd_;
static int job_write_fd_;

struct ml_handler job_handler_;

static void ml__job_handler_fn(int fd, void* context)
{
	(void)context;

	struct ml_job* job;
	read(fd, &job, sizeof(job));

	job->after_work_fn(job->context);
}

int ml_init()
{
	eloop_do_not_clean();

	int fds[2];
	if (pipe(fds) < 0)
		return -1;

	job_read_fd_ = fds[0];
	job_write_fd_ = fds[1];

	ml_handler_init(&job_handler_);

	job_handler_.fn = ml__job_handler_fn;
	job_handler_.fd = job_read_fd_;

	ml_handler_start(&job_handler_);

	return 0;
}

void ml_deinit()
{
	ml_handler_stop(&job_handler_);
	ml_handler_clear(&job_handler_);

	close(job_read_fd_);
	close(job_write_fd_);

	eloop_destroy(eloop_default_loop());
}

int ml_run()
{
	eloop_error_t rc;

	rc = eloop_enter_loop(eloop_default_loop());
	if (rc == eloop_ok)
		return 0;

	ml_errno = rc;
	return -1;
}

void ml_stop()
{
	eloop_terminate_loop(eloop_default_loop());
}

static eloop_cb_error_t ml__timer_fn(eloop_timer_t handle, void* context)
{
	(void)handle;
	struct ml_timer* timer = (struct ml_timer*)context;

	timer->fn(timer->context);

	return eloop_cb_ok;
}

int ml_timer_start(struct ml_timer* timer)
{
	eloop_error_t rc;

	rc = eloop_timer_new(eloop_default_loop(),
			     timer->timeout,
			     timer->is_periodic,
			     ml__timer_fn,
			     NULL,
			     timer,
			     &timer->handle_);
	if (rc == eloop_ok)
		return 0;

	ml_errno = rc;
	return -1;
}

int ml_timer_stop(struct ml_timer* timer)
{
	eloop_error_t rc;

	rc = eloop_timer_del(eloop_default_loop(), timer->handle_);
	if (rc == eloop_ok)
		return 0;

	ml_errno = rc;
	return -1;
}

static eloop_cb_error_t ml__handler_fn(eloop_hdlr_t handle, void* context)
{
	(void)handle;
	struct ml_handler* handler = (struct ml_handler*)context;

	handler->fn(handler->fd, handler->context);

	return eloop_cb_ok;
}

int ml_handler_start(struct ml_handler* handler)
{
	eloop_error_t rc;

	rc = eloop_register_handler(eloop_default_loop(),
				    handler->fd,
				    ml__handler_fn,
				    NULL,
				    handler,
				    &handler->handle_);
	if (rc == eloop_ok)
		return 0;

	ml_errno = rc;
	return -1;
}

int ml_handler_stop(struct ml_handler* handler)
{
	eloop_error_t rc;

	rc = eloop_remove_handler(handler->handle_);
	if (rc == eloop_ok)
		return 0;

	ml_errno = rc;
	return -1;
}

static void ml__job_fn(void* context)
{
	struct ml_job* job = (struct ml_job*)context;

	job->work_fn(job->context);

	/* send the pointer to the main thread */
	write(job_write_fd_, &job, sizeof(job));
}

int ml_job_enqueue(struct ml_job* job)
{
	return worker_add_job(job->priority, ml__job_fn, job);
}

