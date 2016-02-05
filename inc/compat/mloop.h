#ifndef _MLOOP_H
#define _MLOOP_H

#include <stdint.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

enum mloop_timer_type {
	MLOOP_TIMER_RELATIVE = 0,
	MLOOP_TIMER_ABSOLUTE = 1,
	MLOOP_TIMER_PERIODIC = 2,
};

enum mloop_socket_event {
	MLOOP_SOCKET_EVENT_NONE = 0,
	MLOOP_SOCKET_EVENT_IN = 1 << 0,
	MLOOP_SOCKET_EVENT_ERR = 1 << 1,
	MLOOP_SOCKET_EVENT_HUP = 1 << 2,
	MLOOP_SOCKET_EVENT_PRI = 1 << 3,
	MLOOP_SOCKET_EVENT_OUT = 1 << 4,
	MLOOP_SOCKET_EVENT_ALL = 0xff
};

struct mloop;
struct mloop_timer;
struct mloop_socket;
struct mloop_async;
struct mloop_work;
struct mloop_signal;
struct mloop_idle;

typedef void (*mloop_timer_fn)(struct mloop_timer*);
typedef void (*mloop_socket_fn)(struct mloop_socket*);
typedef void (*mloop_async_fn)(struct mloop_async*);
typedef void (*mloop_work_fn)(struct mloop_work*);
typedef void (*mloop_signal_fn)(struct mloop_signal*, int);
typedef void (*mloop_idle_fn)(struct mloop_idle*);
typedef int (*mloop_idle_cond_fn)(struct mloop_idle*);
typedef void (*mloop_free_fn)(void*);

extern int mloop_errno;

/* Create a new mloop to be run in a thread
 */
struct mloop* mloop_new(void);

/* Create a new mloop object scope into the mloop provided as argument
 *
 * This is used for resource management in C++. This is not very useful for
 * basic C code.
 */
struct mloop* mloop_scope_new(struct mloop* mloop);

/* Get the default mloop. This is what you want most of the time.
 */
struct mloop* mloop_default(void);

/* Set the length of the job queue for the global thread pool
 */
void mloop_set_job_queue_size(size_t qsize);

/* Set the stack size of new threads in the global thread pool
 */
void mloop_set_worker_stack_size(size_t stack_size);

/* Start worker threads if they have not already been started
 *
 * The thread pool is cleaned up when no mloop object exists anymore.
 */
int mloop_require_workers(int nthreads);

/* Clean up mloop.
 *
 */
void mloop_free(struct mloop* self);

/* Reference an mloop.
 */
void mloop_ref(struct mloop* self);

/* Unreference an mloop.
 */
int mloop_unref(struct mloop* self);

/* Run the mloop until mloop_exit() is called
 */
int mloop_run(struct mloop* self);

/* Iterate through the mloop once. This can be used in conjunction with other
 * main loops, e.g. libuv, libevent or Qt.
 *
 * You can even add this as a socket event handler to the event loop of your
 * choice using the file descriptor from mloop_get_pollfd().
 */
int mloop_run_once(struct mloop* self);

/* Iterate once through a running main loop.
 */
void mloop_iterate(struct mloop* self);

/* Break out of the main loop and return from mloop_run().
 */
void mloop_exit(struct mloop* self);

/* Get epoll file descriptor. This file descriptor will be marked as readable
 * when an event occurs.
 *
 * See mloop_run_once().
 */
int mloop_get_pollfd(const struct mloop* self);

/* Create a new timer.
 */
struct mloop_timer* mloop_timer_new(struct mloop* self);

/* Clean up timer. Use mloop_timer_unref() instead.
 *
 * This will call the free_fn on the context pointer if both are defined.
 *
 * See: mloop_timer_ref() and mloop_timer_unref().
 */
void mloop_timer_free(struct mloop_timer* self);

/* Add a reference to a timer object.
 */
void mloop_timer_ref(struct mloop_timer* self);

/* Remove a reference to timer object.
 *
 * This calls mloop_timer_free() when the reference count reaches zero.
 */
int mloop_timer_unref(struct mloop_timer* self);

/* Start the timer.
 */
int mloop_timer_start(struct mloop_timer* timer);

/* Stop the timer.
 */
int mloop_timer_stop(struct mloop_timer* timer);

/* Set the type of a timer.
 *
 * A timer can be of the following types:
 * - Periodic.
 * - Single-shot with a relative timeout.
 * - Single-shot with an absolute timeout.
 *
 * A relative timeout is relative to the time at which the timer was started,
 * but an absolute timeout is relative to the value of the system's monotonic
 * clock.
 */
void mloop_timer_set_type(struct mloop_timer* timer,
			  enum mloop_timer_type type);

/* Get the type of the timer.
 */
enum mloop_timer_type mloop_timer_get_type(const struct mloop_timer* timer);

/* Set the period/timeout/abolute time.
 */
void mloop_timer_set_time(struct mloop_timer* timer, uint64_t time);

/* Set the context pointer. Can point to whatever you want.
 *
 * If you specify a free_fn, it will be called with the context pointer as an
 * argument when this object is freed.
 */
void mloop_timer_set_context(struct mloop_timer* timer, void* context,
			     mloop_free_fn free_context);

/* Set the timeout callback.
 */
void mloop_timer_set_callback(struct mloop_timer* timer, mloop_timer_fn fn);

/* Get the context pointer.
 */
void* mloop_timer_get_context(const struct mloop_timer* timer);

/* Check if the timer has been started.
 */
int mloop_timer_is_started(const struct mloop_timer* timer);

/* Create a socket event notifier.
 */
struct mloop_socket* mloop_socket_new(struct mloop* self);

/* Clean up socket. Use mloop_socket_unref() instead.
 *
 * This will call the free_fn on the context pointer if both are defined.
 *
 * See: mloop_socket_ref() and mloop_socket_unref().
 *
 * Note: This will also close the file-handle unless you set it to -1 first.
 */
void mloop_socket_free(struct mloop_socket* self);

/* Add a reference to a socket object
 */
void mloop_socket_ref(struct mloop_socket* self);

/* Remove a reference to socket object.
 *
 * This calls mloop_socket_free() when the reference count reaches zero.
 */
int mloop_socket_unref(struct mloop_socket* self);

/* Start handling events for a socket.
 */
int mloop_socket_start(struct mloop_socket* socket);

/* Stop handling events for a socket.
 */
int mloop_socket_stop(struct mloop_socket* socket);

/* Set the socket object's file descriptor.
 */
void mloop_socket_set_fd(struct mloop_socket* socket, int fd);

/* Get the file descriptor.
 */
int mloop_socket_get_fd(const struct mloop_socket* socket);

/* Set the context pointer. Can point to whatever you want.
 *
 * If you specify a free_fn, it will be called with the context pointer as an
 * argument when this object is freed.
 */
void mloop_socket_set_context(struct mloop_socket* socket, void* context,
			      mloop_free_fn free_context);

/* Set the socket event callback.
 */
void mloop_socket_set_callback(struct mloop_socket* socket, mloop_socket_fn fn);

/* Check if the socket has been started.
 */
int mloop_socket_is_started(const struct mloop_socket* socket);

/* Get the context pointer.
 */
void* mloop_socket_get_context(const struct mloop_socket* socket);

/* Get the event type currently pending on the socket
 */
enum mloop_socket_event
mloop_socket_get_event(const struct mloop_socket* socket);

/* Set the event type to be monitored. By default this is set to
 * MLOOP_SOCKET_EVENT_IN | MLOOP_SOCKET_EVENT_PRI
 *
 * This can be set to any combination of IN, PRI and OUT but you will always
 * receive events for ERR and HUP.
 */
void mloop_socket_set_event(struct mloop_socket* socket,
			    enum mloop_socket_event event);

/* Create an async job object.
 *
 * An async job is a single non-blocking task that will be run once at the end of
 * the next main loop iteration.
 */
struct mloop_async* mloop_async_new(struct mloop* self);

/* Clean up async job object. Use mloop_async_unref() instead.
 *
 * This will call the free_fn on the context pointer if both are defined.
 *
 * See: mloop_async_ref() and mloop_async_unref().
 */
void mloop_async_free(struct mloop_async* self);

/* Add a reference to an async object.
 */
void mloop_async_ref(struct mloop_async* self);

/* Remove a reference to an async object.
 *
 * This calls mloop_async_free() when the reference count reaches zero.
 */
int mloop_async_unref(struct mloop_async* self);

/* Enqueue an async job on the async job queue.
 */
int mloop_async_start(struct mloop_async* async);

/* Cancel async job.
 *
 * Note: This only cancels jobs that are still on the queue.
 */
void mloop_async_cancel(struct mloop_async* async);

/* Set the context pointer. Can point to whatever you want.
 *
 * If you specify a free_fn, it will be called with the context pointer as an
 * argument when this object is freed.
 */
void mloop_async_set_context(struct mloop_async* async, void* context,
			    mloop_free_fn free_context);

/* Specify the task to be carried out.
 */
void mloop_async_set_callback(struct mloop_async* async, mloop_async_fn fn);

/* Get the context pointer.
 */
void* mloop_async_get_context(const struct mloop_async* async);

/* Set the priority of the task. Zero is the highest priority.
 *
 * Range: 0 - ULONG_MAX.
 *
 * Warning: Setting the priority to anything above the lowest priority and
 * re-starting the job within the event callback will STARVE async jobs with
 * lower prorities.
 */
void mloop_async_set_priority(struct mloop_async* async,
			      unsigned long priority);

/* Check if the async has been started.
 */
int mloop_async_is_started(const struct mloop_async* async);

/* Create a worker thread job object.
 *
 * A worker job is something that will typically take some time to compute; i.e.
 * a blocking call or some operation on a large data-set.
 *
 * The work object has two callbacks: work_fn and done_fn.
 * work_fn does the work and it's executed within a worker thread, and done_fn
 * will be called afterwards from within the main loop (mloop).
 */
struct mloop_work* mloop_work_new(struct mloop* self);

/* Clean up work object. Use mloop_work_unref() instead.
 *
 * This will call the free_fn on the context pointer if both are defined.
 *
 * See: mloop_work_ref() and mloop_work_unref().
 */
void mloop_work_free(struct mloop_work* self);

/* Add a reference to a work object.
 */
void mloop_work_ref(struct mloop_work* self);

/* Remove a reference to a work object.
 *
 * This calls mloop_work_free() when the reference count reaches zero.
 */
int mloop_work_unref(struct mloop_work* self);

/* Enqueue a worker thread job.
 */
int mloop_work_start(struct mloop_work* work);

/* Cancel work.
 *
 * Note: This only cancels jobs that are still on the queue.
 */
void mloop_work_cancel(struct mloop_work* work);

/* Set the context pointer. Can point to whatever you want.
 *
 * If you specify a free_fn, it will be called with the context pointer as an
 * argument when this object is freed.
 */
void mloop_work_set_context(struct mloop_work* work, void* context,
			    mloop_free_fn free_context);

/* Set the work function. This function is run in the worker thread.
 */
void mloop_work_set_work_fn(struct mloop_work* work, mloop_work_fn fn);

/* Set the notification function. This function is run in the main loop.
 */
void mloop_work_set_done_fn(struct mloop_work* work, mloop_work_fn fn);

/* Get the context pointer.
 */
void* mloop_work_get_context(const struct mloop_work* work);

/* Set the priority of a job. Zero is the highest priority.
 *
 * Range: 0 - ULONG_MAX.
 */
void mloop_work_set_priority(struct mloop_work* work, unsigned long priority);

/* Check if the work has been started.
 */
int mloop_work_is_started(const struct mloop_work* work);

/* Create a signal handler object.
 */
struct mloop_signal* mloop_signal_new(struct mloop* self);

/* Clean up signal handler object. Use mloop_signal_unref() instead.
 * This will call the free_fn on the context pointer if both are defined.
 *
 * See: mloop_signal_ref() and mloop_signal_unref().
 */
void mloop_signal_free(struct mloop_signal* self);

/* Add a reference to a signal object.
 */
void mloop_signal_ref(struct mloop_signal* self);

/* Remove a reference to a signal object.
 *
 * This calls mloop_signal_free() when the reference count reaches zero.
 */
int mloop_signal_unref(struct mloop_signal* self);

/* Start signal handler.
 */
int mloop_signal_start(struct mloop_signal* signal);

/* Stop signal handler.
 */
int mloop_signal_stop(struct mloop_signal* signal);

/* Set signal even callback
 */
void mloop_signal_set_callback(struct mloop_signal* self, mloop_signal_fn fn);

/* Set the context pointer.
 */
void mloop_signal_set_context(struct mloop_signal* self, void* context,
			      mloop_free_fn free_context);

/* Get the context pointer.
 */
void* mloop_signal_get_context(const struct mloop_signal* self);

/* Set signal mask.
 *
 * See sigaddset(3).
 */
void mloop_signal_set_signals(struct mloop_signal* self, const sigset_t* mask);

/* Check if the signal has been started.
 */
int mloop_signal_is_started(const struct mloop_signal* self);

/* Create an idle object.
 */
struct mloop_idle* mloop_idle_new(struct mloop* self);

/* Clean up idle object. Use mloop_idle_unref() instead.
 *
 * This will call the free_fn on the context pointer if both are defined.
 *
 * See: mloop_idle_ref() and mloop_idle_unref().
 */
void mloop_idle_free(struct mloop_idle* self);

/* Add a reference to a signal object.
 */
void mloop_idle_ref(struct mloop_idle* self);

/* Remove a reference to a idle object.
 *
 * This calls mloop_idle_free() when the reference count reaches zero.
 */
int mloop_idle_unref(struct mloop_idle* self);

/* Start idle handler.
 */
int mloop_idle_start(struct mloop_idle* idle);

/* Stop idle handler.
 */
int mloop_idle_stop(struct mloop_idle* idle);

/* Set the context pointer. Can point to whatever you want.
 *
 * If you specify a free_fn, it will be called with the context pointer as an
 * argument when this object is freed.
 */
void mloop_idle_set_context(struct mloop_idle* idle, void* context,
			    mloop_free_fn free_context);

/* Set the idle function. This function is run between main loop iterations.
 */
void mloop_idle_set_idle_fn(struct mloop_idle* idle, mloop_idle_fn fn);

/* Set the condition function. The idle function is only run if the condition
 * function returns true.
 */
void mloop_idle_set_cond_fn(struct mloop_idle* idle, mloop_idle_cond_fn fn);

/* Get the context pointer.
 */
void* mloop_idle_get_context(const struct mloop_idle* idle);

/* Check if the idle has been started.
 */
int mloop_idle_is_started(const struct mloop_idle* idle);

#ifdef __cplusplus
}
#endif

#endif /* _MLOOP_H */
