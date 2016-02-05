#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <limits.h>
#include <errno.h>
#include <execinfo.h>
#include <sys/queue.h>

#include "atomic_compat.h"
#include "mloop.h"
#include "prioq.h"

#define EXPORT __attribute__((visibility("default")))

#define MAX_EVENTS 16

#define mloop__cas(ptr, expected, desired) \
({ \
	__typeof__(expected) expected_ = (expected); \
	__atomic_compare_exchange_n((ptr), &expected_, desired, 0, \
				    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
})

#define mloop__atomic_load(ptr) \
	__atomic_load_n((ptr), __ATOMIC_SEQ_CST)

#define mloop__atomic_store(ptr, val) \
	__atomic_store_n(ptr, val, __ATOMIC_SEQ_CST)

enum mloop_type {
	MLOOP_INIT   = -1,
	MLOOP_NONE   = 0,
	MLOOP_SOCKET = 1 << 0,
	MLOOP_TIMER  = 1 << 1,
	MLOOP_ASYNC  = 1 << 2,
	MLOOP_WORK   = 1 << 3,
	MLOOP_SIGNAL = 1 << 4,
	MLOOP_IDLE   = 1 << 5,
	MLOOP_MLOOP  = 1 << 6,
	MLOOP_ANY    = 0xff
};

enum mloop_state {
	MLOOP_STOPPED = 0,
	MLOOP_STOPPING,
	MLOOP_STARTING,
	MLOOP_STARTED,
};

struct mloop_core;

#define MLOOP_COMMON \
	enum mloop_type type; \
	enum mloop_state state; \
	LIST_ENTRY(mloop_common) object_links; \
	LIST_ENTRY(mloop_common) free_links; \
	int ref; \
	struct mloop* creator; \
	struct mloop* parent; \
	struct mloop_core* parent_core; \
	void* context; \
	mloop_free_fn free_fn;

struct mloop_common {
	MLOOP_COMMON
};

struct mloop_socket {
	MLOOP_COMMON /* Do not move */
	mloop_socket_fn callback_fn; /* Do not move */
	/* Members specific to socket can be added below */
	int fd;
	enum mloop_socket_event revents;
	enum mloop_socket_event events;
};

struct mloop_timer {
	struct mloop_socket socket; /* Do not move */
	/* Members specific to timer can be added below */
	enum mloop_timer_type timer_type;
	uint64_t time;
};

#define MLOOP_JOB_COMMON \
	unsigned long priority; \
	int is_cancelled;

struct mloop_async {
	MLOOP_COMMON /* Do not move */
	mloop_async_fn callback_fn; /* Do not move */
	MLOOP_JOB_COMMON /* Do not move */
	/* Members specific to async can be added below */
};

struct mloop_work {
	MLOOP_COMMON /* Do not move */
	mloop_work_fn done_fn; /* Do not move */
	MLOOP_JOB_COMMON /* Do not move */
	/* Members specific to work can be added below */
	mloop_work_fn work_fn;
};

struct mloop_signal {
	struct mloop_socket socket; /* Do not move */
	mloop_signal_fn signal_fn; /* Do not move */
	/* Members specific to signal can be added below */
	sigset_t sigset;
};

struct mloop_idle {
	MLOOP_COMMON /* Do not move */
	/* Members specific to idle can be added below */
	mloop_idle_fn idle_fn;
	mloop_idle_cond_fn cond_fn;
	TAILQ_ENTRY(mloop_idle) idle_links;
};

LIST_HEAD(mloop_object_list, mloop_common);
TAILQ_HEAD(mloop_idle_list, mloop_idle);

struct mloop_core {
	int ref;
	int epollfd;
	struct mloop_socket break_out_socket;
	int do_exit;
	struct prioq async_jobs;
	struct mloop_idle_list idle_jobs;
	pthread_mutex_t idle_list_mutex;
	struct mloop_object_list free_list;
	pthread_mutex_t free_list_mutex;
};

struct mloop {
	int ref;
	struct mloop_core* core;
	struct mloop_object_list objects;
	pthread_mutex_t object_list_mutex;
};

int mloop_errno;

static enum mloop_type mloop__debug = MLOOP_INIT;

#define NTHREADS_MAX 32

static struct prioq mloop__job_queue;
static int mloop__nthreads = 0;
static size_t mloop__qsize = 64;
static size_t mloop__stacksize = 0;
static pthread_t mloop__threads[NTHREADS_MAX];

static struct mloop* mloop__default = NULL;
static size_t mloop__core_count = 0;

enum mloop__debug_parser_token {
	MLOOP__START = 0,
	MLOOP__NAME,
	MLOOP__COMMA,
	MLOOP__WS,
	MLOOP__END
};

struct mloop__debug_parser {
	enum mloop__debug_parser_token tok_type;
	const char* pos;
	const char* tok_start;
	const char* tok_end;
};

static int mloop__unref_any(void* ptr);
static void mloop__ref_any(void* ptr);
static int mloop__start_socket(struct mloop* self, struct mloop_socket* socket);
static int mloop__socket_stop(struct mloop_socket* self);
static int mloop__start_async(struct mloop* self, struct mloop_async* async);

static int mloop__debug_parse_expect(struct mloop__debug_parser* parser,
				     enum mloop__debug_parser_token token,
				     const char* str)
{
	if (parser->tok_type != token)
		return 0;

	if (parser->tok_type == MLOOP__END)
		return 1;

	if (str) {
		if (strlen(str) != parser->tok_end - parser->tok_start)
			return 0;

		if (strncmp(str, parser->tok_start,
			    parser->tok_end - parser->tok_start) != 0)
			return 0;
	}

	parser->tok_start = parser->tok_end;
	switch (parser->tok_start[0]) {
	case ',':
		parser->tok_end = parser->tok_start + 1;
		parser->tok_type = MLOOP__COMMA;
		break;
	case ' ':
	case '\t':
	case '\n':
		parser->tok_end = parser->tok_start
				+ strspn(parser->tok_start, " \t\n");
		parser->tok_type = MLOOP__WS;
		break;
	case '\0':
		parser->tok_start = NULL;
		parser->tok_end = NULL;
		parser->tok_type = MLOOP__END;
		break;
	default:
		parser->tok_type = MLOOP__NAME;
		parser->tok_end = parser->tok_start
				+ strcspn(parser->tok_start, ", \t\n");
		break;
	}

	return 1;
}

static int mloop__debug_parse_one_type(struct mloop__debug_parser* parser)
{
	if (mloop__debug_parse_expect(parser, MLOOP__NAME, "full")) {
		mloop__debug |= MLOOP_ANY;
		return 1;
	}

	if (mloop__debug_parse_expect(parser, MLOOP__NAME, "socket")) {
		mloop__debug |= MLOOP_SOCKET;
		return 1;
	}

	if (mloop__debug_parse_expect(parser, MLOOP__NAME, "timer")) {
		mloop__debug |= MLOOP_TIMER;
		return 1;
	}

	if (mloop__debug_parse_expect(parser, MLOOP__NAME, "async")) {
		mloop__debug |= MLOOP_ASYNC;
		return 1;
	}

	if (mloop__debug_parse_expect(parser, MLOOP__NAME, "work")) {
		mloop__debug |= MLOOP_WORK;
		return 1;
	}

	if (mloop__debug_parse_expect(parser, MLOOP__NAME, "signal")) {
		mloop__debug |= MLOOP_SIGNAL;
		return 1;
	}

	if (mloop__debug_parse_expect(parser, MLOOP__NAME, "idle")) {
		mloop__debug |= MLOOP_IDLE;
		return 1;
	}

	if (mloop__debug_parse_expect(parser, MLOOP__NAME, "mloop")) {
		mloop__debug |= MLOOP_MLOOP;
		return 1;
	}

	return 0;
}

static int mloop__debug_parse_types(struct mloop__debug_parser* parser)
{
	return (mloop__debug_parse_expect(parser, MLOOP__WS, NULL) || 1)
	       && mloop__debug_parse_one_type(parser)
	       && (mloop__debug_parse_expect(parser, MLOOP__WS, NULL) || 1)
	       && mloop__debug_parse_expect(parser, MLOOP__COMMA, NULL) ?
			mloop__debug_parse_types(parser) : 1;
}

static int mloop__debug_parse(const char* str)
{
	struct mloop__debug_parser parser = {
		.tok_type = MLOOP__START,
		.tok_start = str,
		.tok_end = str
	};

	return mloop__debug_parse_expect(&parser, MLOOP__START, NULL)
		&& (mloop__debug_parse_expect(&parser, MLOOP__WS, NULL) || 1)
		&& mloop__debug_parse_types(&parser)
		&& (mloop__debug_parse_expect(&parser, MLOOP__WS, NULL) || 1)
		&& mloop__debug_parse_expect(&parser, MLOOP__END, NULL);
}

static void mloop__initialize_debug()
{
	mloop__debug = MLOOP_NONE;

	char* str = getenv("MLOOP_DEBUG");

	if (str)
		mloop__debug_parse(str);

	if (mloop__debug) {
		fprintf(stderr, "mloop debug:");
		if (mloop__debug & MLOOP_SOCKET) fprintf(stderr, " socket");
		if (mloop__debug & MLOOP_TIMER) fprintf(stderr, " timer");
		if (mloop__debug & MLOOP_ASYNC) fprintf(stderr, " async");
		if (mloop__debug & MLOOP_WORK) fprintf(stderr, " work");
		if (mloop__debug & MLOOP_SIGNAL) fprintf(stderr, " signal");
		if (mloop__debug & MLOOP_IDLE) fprintf(stderr, " idle");
		if (mloop__debug & MLOOP_MLOOP) fprintf(stderr, " mloop");
		fprintf(stderr, ".\n");
	}
}

static enum mloop_type mloop__get_debug_type()
{
	if (mloop__debug == MLOOP_INIT)
		mloop__initialize_debug();

	return mloop__debug;
}

static inline int mloop__is_exiting(const struct mloop* self)
{
	return mloop__atomic_load(&self->core->do_exit);
}

static inline void mloop__do_exit(struct mloop* self)
{
	mloop__atomic_store(&self->core->do_exit, 1);
}

static inline void mloop__break_out(struct mloop* self)
{
	uint64_t one = 1;
	write(self->core->break_out_socket.fd, &one, sizeof(one));
}

static inline int mloop__change_state(void* obj_ptr, enum mloop_state expected,
				      enum mloop_state desired)
{
	struct mloop_common* obj = obj_ptr;

	return mloop__cas(&obj->state, expected, desired) ? 0 : -1;
}

static inline void mloop__object_list_add(void* obj_ptr)
{
	struct mloop_common* obj = obj_ptr;
	struct mloop* mloop = obj->parent;
	mloop__ref_any(obj);
	pthread_mutex_lock(&mloop->object_list_mutex);
	LIST_INSERT_HEAD(&mloop->objects, obj, object_links);
	pthread_mutex_unlock(&mloop->object_list_mutex);
}

static inline int mloop__object_list_remove(void* obj_ptr)
{
	struct mloop_common* obj = obj_ptr;
	struct mloop* mloop = obj->parent;
	pthread_mutex_lock(&mloop->object_list_mutex);
	LIST_REMOVE(obj, object_links);
	pthread_mutex_unlock(&mloop->object_list_mutex);
	return mloop__unref_any(obj);
}

static inline void mloop__object_list_clear(struct mloop* self)
{
	while (!LIST_EMPTY(&self->objects)) {
		struct mloop_common* obj = LIST_FIRST(&self->objects);

		if (mloop__object_list_remove(obj) != 0)
			obj->parent = NULL;
	}
}

static inline void mloop__idle_list_lock(struct mloop_core* core)
{
	pthread_mutex_lock(&core->idle_list_mutex);
}

static inline void mloop__idle_list_unlock(struct mloop_core* core)
{
	pthread_mutex_unlock(&core->idle_list_mutex);
}

static inline void mloop__idle_list_add(struct mloop_idle* obj)
{
	struct mloop_core* core = obj->parent_core;
	mloop__idle_list_lock(core);
	mloop_idle_ref(obj);
	TAILQ_INSERT_TAIL(&core->idle_jobs, obj, idle_links);
	mloop__idle_list_unlock(core);
}

static inline void mloop__idle_list_remove(struct mloop_idle* obj)
{
	struct mloop_core* core = obj->parent_core;
	mloop__idle_list_lock(core);
	TAILQ_REMOVE(&core->idle_jobs, obj, idle_links);
	mloop_idle_unref(obj);
	mloop__idle_list_unlock(core);
}

static inline struct mloop_idle* mloop__idle_list_pop(struct mloop_core* core)
{
	mloop__idle_list_lock(core);
	struct mloop_idle* idle = TAILQ_FIRST(&core->idle_jobs);
	if (idle)
		TAILQ_REMOVE(&core->idle_jobs, idle, idle_links);
	mloop__idle_list_unlock(core);
	return idle;
}

static inline void mloop__idle_list_clear(struct mloop_core* self)
{
	while (!TAILQ_EMPTY(&self->idle_jobs))
		mloop__idle_list_remove(TAILQ_FIRST(&self->idle_jobs));
}

static void mloop__free_context(void* ptr)
{
	struct mloop_common* common = ptr;

	mloop_free_fn free_fn = common->free_fn;
	if (free_fn)
		free_fn(common->context);
}

static void mloop__print_backtrace(int depth)
{
	void* buffer[128] = { 0 };
	int nptrs;
	char** strings;

	nptrs = backtrace(buffer, 128);

	strings = backtrace_symbols(buffer + depth, 128 - depth);
	assert(strings);

	for (int i = 0; i < nptrs; ++i)
		fprintf(stderr, "\t%s\n", strings[i]);

	free(strings);
}

static inline void mloop__print_debug(void* ptr, const char* action, int ref,
				      int depth)
{
	struct mloop_common* self = ptr;
	if (mloop__get_debug_type() & self->type) {
		fprintf(stderr, "%s %p: %d @ %lx\n", action, self, ref,
			pthread_self());
		mloop__print_backtrace(depth);
		fprintf(stderr, "\n");
	}
}

static void mloop__free_any(void* ptr)
{
	struct mloop_common* common = ptr;
	mloop__print_debug(ptr, "free", 0, 2);

	switch (common->type)
	{
	case MLOOP_SOCKET: mloop_socket_free(ptr); break;
	case MLOOP_TIMER: mloop_timer_free(ptr); break;
	case MLOOP_ASYNC: mloop_async_free(ptr); break;
	case MLOOP_WORK: mloop_work_free(ptr); break;
	case MLOOP_SIGNAL: mloop_signal_free(ptr); break;
	case MLOOP_IDLE: mloop_idle_free(ptr); break;
	default: abort(); break;
	}
}

static void mloop__schedule_free(void* ptr)
{
	struct mloop_common* common = ptr;
	struct mloop* mloop = common->parent;
	if (!mloop || mloop__is_exiting(mloop)) {
		mloop__free_any(ptr);
		return;
	}

	mloop__print_debug(ptr, "schedule free", 0, 2);
	pthread_mutex_lock(&mloop->core->free_list_mutex);
	LIST_INSERT_HEAD(&mloop->core->free_list, common, free_links);
	pthread_mutex_unlock(&mloop->core->free_list_mutex);
}

static void mloop__redeem(void* ptr)
{
	struct mloop_common* common = ptr;
	struct mloop* mloop = common->parent;
	if (!mloop)
		return;

	mloop__print_debug(ptr, "redeem", 1, 2);

	pthread_mutex_lock(&mloop->core->free_list_mutex);
	LIST_REMOVE(common, free_links);
	pthread_mutex_unlock(&mloop->core->free_list_mutex);
}

static void mloop__collect(struct mloop_core* core)
{
	struct mloop_object_list copy;
	LIST_INIT(&copy);

	pthread_mutex_lock(&core->free_list_mutex);
	while (!LIST_EMPTY(&core->free_list)) {
		struct mloop_common* obj = LIST_FIRST(&core->free_list);
		LIST_REMOVE(obj, free_links);
		if (obj->parent_core == core)
			LIST_INSERT_HEAD(&copy, obj, free_links);
	}
	pthread_mutex_unlock(&core->free_list_mutex);

	while (!LIST_EMPTY(&copy)) {
		struct mloop_common* obj = LIST_FIRST(&copy);
		LIST_REMOVE(obj, free_links);
		mloop__free_any(obj);
	}
}

static inline void mloop__print_mloop_debug(struct mloop* self,
					    const char* action,
					    int ref, int depth)
{
	if (mloop__get_debug_type() & MLOOP_MLOOP) {
		fprintf(stderr, "%s %p: %d\n", action, self, ref);
		mloop__print_backtrace(depth);
		fprintf(stderr, "\n");
	}
}

static int mloop__unref_any(void* ptr)
{
	struct mloop_common* common = ptr;

	assert(common->ref > 0);

	int ref = __atomic_sub_fetch(&common->ref, 1, __ATOMIC_SEQ_CST);
	mloop__print_debug(ptr, "unref", ref, 2);
	if (ref == 0)
		mloop__schedule_free(ptr);

	return ref;
}

static inline void mloop__ref_any(void* ptr)
{
	struct mloop_common* common = ptr;

	int ref = __atomic_add_fetch(&common->ref, 1, __ATOMIC_SEQ_CST);
	mloop__print_debug(ptr, "ref", ref, 2);
	if (ref == 1)
		mloop__redeem(ptr);
}

EXPORT
void mloop_timer_ref(struct mloop_timer* self)
{
	mloop__ref_any(self);
}

EXPORT
void mloop_socket_ref(struct mloop_socket* self)
{
	mloop__ref_any(self);
}

EXPORT
void mloop_async_ref(struct mloop_async* self)
{
	mloop__ref_any(self);
}

EXPORT
void mloop_work_ref(struct mloop_work* self)
{
	mloop__ref_any(self);
}

EXPORT
void mloop_signal_ref(struct mloop_signal* self)
{
	mloop__ref_any(self);
}

EXPORT
void mloop_idle_ref(struct mloop_idle* self)
{
	mloop__ref_any(self);
}

EXPORT
int mloop_timer_unref(struct mloop_timer* self)
{
	return mloop__unref_any(self);
}

EXPORT
int mloop_socket_unref(struct mloop_socket* self)
{
	return mloop__unref_any(self);
}

EXPORT
int mloop_async_unref(struct mloop_async* self)
{
	return mloop__unref_any(self);
}

EXPORT
int mloop_work_unref(struct mloop_work* self)
{
	return mloop__unref_any(self);
}

EXPORT
int mloop_signal_unref(struct mloop_signal* self)
{
	return mloop__unref_any(self);
}

EXPORT
int mloop_idle_unref(struct mloop_idle* self)
{
	return mloop__unref_any(self);
}

static void mloop__block_all_signals()
{
	sigset_t mask;
	sigfillset(&mask);
	sigdelset(&mask, SIGABRT); /* except abort() */
	pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

static int mloop__forward_work(struct mloop_work* work)
{
	struct mloop* mloop = work->parent;
	struct prioq* queue = &mloop->core->async_jobs;
	struct mloop_async* async = (struct mloop_async*)work;

	/* We lock the async queue here so that we can keep it blocked until the
	 * state has been changed to MLOOP_STARTED. Otherwise the main thread
	 * can start processing the job before the state has been changed.
	 */
	prioq__lock(queue);
	if (prioq_insert(queue, async->priority, async) < 0)
		goto failure;

	mloop__break_out(mloop);
	mloop__change_state(work, MLOOP_STARTING, MLOOP_STARTED);

	prioq__unlock(queue);
	return 0;

failure:
	mloop__change_state(work, MLOOP_STARTING, MLOOP_STOPPED);
	prioq__unlock(queue);
	return -1;
}

static void* mloop__worker_fn(void* context)
{
	(void)context;

	mloop__block_all_signals();

	while (1) {
		struct prioq_elem elem;
		if (prioq_pop(&mloop__job_queue, &elem, -1) < 0)
			continue;

		if (elem.data == NULL)
			break;

		struct mloop_work* work = elem.data;
		if (work->is_cancelled) {
			if (mloop__object_list_remove(work) == 0)
				continue;

			int rc = mloop__change_state(work, MLOOP_STARTING,
						     MLOOP_STOPPED);
			assert(rc == 0);
			continue;
		}

		mloop_work_ref(work);

		mloop_work_fn work_fn = work->work_fn;
		if (work_fn)
			work_fn(work);

		if (mloop_work_unref(work) == 0)
			continue; /* No one is interested in the result */

		if (work->done_fn)
			mloop__forward_work(work);
		else
			mloop__change_state(work, MLOOP_STARTED, MLOOP_STOPPED);
	};

	return NULL;
}

static void mloop__reap_threads()
{
	struct timespec ts;

	/* A thread returns if it's sent a null-job */
	for (int i = 0; i < mloop__nthreads; ++i)
		prioq_insert(&mloop__job_queue, 0, NULL);

	int rc = clock_gettime(CLOCK_REALTIME, &ts);
	assert(rc == 0);
	ts.tv_sec += 1;

	for (int i = 0; i < mloop__nthreads; ++i)
		pthread_timedjoin_np(mloop__threads[i], NULL, &ts);
}

void mloop__stop_workers()
{
	mloop__reap_threads();
	prioq_destroy(&mloop__job_queue);
}

static int mloop__start_threads(size_t stacksize, int required)
{
	int rc = 0;

	pthread_attr_t attr;

	pthread_attr_init(&attr);

	if (stacksize != 0)
		pthread_attr_setstacksize(&attr, stacksize);

	int i;
	for (i = mloop__nthreads; i < required; ++i) {
		rc = pthread_create(&mloop__threads[i], &attr,
				    mloop__worker_fn, NULL);
		if (rc < 0) {
			errno = rc;
			mloop__nthreads = i;
			mloop__reap_threads();
			break;
		}
	}

	pthread_attr_destroy(&attr);
	return rc;
}

EXPORT
void mloop_set_job_queue_size(size_t qsize)
{
	mloop__qsize = qsize;
}

EXPORT
void mloop_set_worker_stack_size(size_t stack_size)
{
	mloop__stacksize = stack_size;
}

EXPORT
int mloop_require_workers(int nthreads)
{
	if (nthreads <= 0 || nthreads >= NTHREADS_MAX)
		return -1;

	if (nthreads <= mloop__nthreads)
		return 0;

	if (mloop__nthreads == 0)
		if (prioq_init(&mloop__job_queue, mloop__qsize) < 0)
			return -1;

	if (mloop__start_threads(mloop__stacksize, nthreads) < 0)
		goto thread_start_failure;

	mloop__nthreads = nthreads;

	return 0;

thread_start_failure:
	mloop__nthreads = 0;
	return -1;
}

void mloop__on_break_out_event(struct mloop_socket* socket)
{
	uint64_t count = 0;
	read(socket->fd, &count, sizeof(count));
}

static struct mloop_core* mloop_core__new(struct mloop* mloop)
{
	struct mloop_core* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	self->epollfd = epoll_create(MAX_EVENTS);
	if (self->epollfd < 0)
		goto epoll_failure;

	mloop->core = self;

	struct mloop_socket* break_out_socket = &self->break_out_socket;
	break_out_socket->parent = mloop;
	break_out_socket->parent_core = self;
	break_out_socket->ref = 1;
	break_out_socket->callback_fn = mloop__on_break_out_event;
	break_out_socket->events = MLOOP_SOCKET_EVENT_IN
				 | MLOOP_SOCKET_EVENT_PRI;
	break_out_socket->fd = eventfd(0, EFD_NONBLOCK);
	if (break_out_socket->fd < 0)
		goto break_out_socket_fd_failure;

	if (mloop__start_socket(mloop, break_out_socket) < 0)
		goto break_out_socket_add_failure;

	if (prioq_init(&self->async_jobs, 64) < 0)
		goto async_job_queue_failure;

	pthread_mutex_init(&mloop->object_list_mutex, NULL);
	pthread_mutex_init(&self->idle_list_mutex, NULL);
	pthread_mutex_init(&self->free_list_mutex, NULL);
	LIST_INIT(&mloop->objects);
	LIST_INIT(&self->free_list);
	TAILQ_INIT(&self->idle_jobs);

	self->ref = 1;

	__atomic_add_fetch(&mloop__core_count, 1, __ATOMIC_SEQ_CST);

	return self;

async_job_queue_failure:
	mloop__socket_stop(break_out_socket);
break_out_socket_add_failure:
	close(break_out_socket->fd);
break_out_socket_fd_failure:
	close(self->epollfd);
epoll_failure:
	free(self);
	return NULL;
}

EXPORT
struct mloop* mloop_new(void)
{
	struct mloop* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	self->core = mloop_core__new(self);
	if (!self->core)
		goto failure;

	self->ref = 1;

	return self;

failure:
	free(self);
	return NULL;
}

static void mloop_core__free(struct mloop_core* self)
{
	if (__atomic_sub_fetch(&mloop__core_count, 1, __ATOMIC_SEQ_CST) == 0)
		mloop__stop_workers();

	mloop__idle_list_clear(self);
	mloop__collect(self);
	pthread_mutex_destroy(&self->idle_list_mutex);
	prioq_destroy(&self->async_jobs);
	close(self->break_out_socket.fd);
	close(self->epollfd);
	free(self);
}

static int mloop_core__ref(struct mloop_core* self)
{
	return __atomic_add_fetch(&self->ref, 1, __ATOMIC_SEQ_CST);
}

static int mloop_core__unref(struct mloop_core* self)
{
	int ref = __atomic_sub_fetch(&self->ref, 1, __ATOMIC_SEQ_CST);
	if (ref <= 0)
		mloop_core__free(self);

	return ref;
}

EXPORT
struct mloop* mloop_scope_new(struct mloop* other)
{
	struct mloop* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	pthread_mutex_init(&self->object_list_mutex, NULL);
	LIST_INIT(&self->objects);

	self->ref = 1;
	self->core = other->core;

	mloop_core__ref(self->core);

	return self;
}

EXPORT
void mloop_free(struct mloop* self)
{
	mloop__object_list_clear(self);
	pthread_mutex_destroy(&self->object_list_mutex);
	mloop_core__unref(self->core);
	free(self);
}

EXPORT
void mloop_ref(struct mloop* self)
{
	int ref = __atomic_add_fetch(&self->ref, 1, __ATOMIC_SEQ_CST);
	mloop__print_mloop_debug(self, "ref", ref, 2);
}

EXPORT
int mloop_unref(struct mloop* self)
{
	int ref = __atomic_sub_fetch(&self->ref, 1, __ATOMIC_SEQ_CST);
	mloop__print_mloop_debug(self, "unref", ref, 2);
	if (ref <= 0)
		mloop_free(self);

	return ref;
}

EXPORT
struct mloop* mloop_default()
{
	if (mloop__default)
		return mloop__default;

	mloop__default = mloop_new();
	assert(mloop__default);

	/* Initially the default loop has a zero reference count so that we
	 * don't have to unreference the object in a dtor.
	 */
	mloop__default->ref = 0;

	return mloop__default;
}

EXPORT
struct mloop_socket* mloop_socket_new(struct mloop* creator)
{
	struct mloop_socket* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	self->type = MLOOP_SOCKET;
	self->fd = -1;
	self->ref = 1;
	self->creator = creator;
	self->events = MLOOP_SOCKET_EVENT_IN | MLOOP_SOCKET_EVENT_PRI;

	mloop__print_debug(self, "new", 1, 1);

	return self;
}

EXPORT
struct mloop_timer* mloop_timer_new(struct mloop* creator)
{
	struct mloop_timer* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	struct mloop_socket* socket = &self->socket;
	socket->type = MLOOP_TIMER;
	socket->ref = 1;
	socket->creator = creator;
	socket->events = MLOOP_SOCKET_EVENT_IN | MLOOP_SOCKET_EVENT_PRI;

	socket->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if (socket->fd < 0)
		goto timerfd_create_failure;

	mloop__print_debug(self, "new", 1, 1);

	return self;

timerfd_create_failure:
	free(self);
	return NULL;
}

void mloop__signal_reader(struct mloop_socket* socket)
{
	struct mloop_signal* sig = (struct mloop_signal*)socket;
	struct signalfd_siginfo fdsi;
	ssize_t size = read(socket->fd, &fdsi, sizeof(fdsi));

	if (size < 0)
		return;

	assert(size == sizeof(fdsi));

	mloop_signal_fn signal_fn = sig->signal_fn;
	if (signal_fn)
		signal_fn(sig, fdsi.ssi_signo);
}

EXPORT
struct mloop_signal* mloop_signal_new(struct mloop* creator)
{
	struct mloop_signal* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	struct mloop_socket* socket = &self->socket;
	socket->type = MLOOP_SIGNAL;
	socket->fd = -1;
	socket->ref = 1;
	socket->callback_fn = mloop__signal_reader;
	socket->creator = creator;
	socket->events = MLOOP_SOCKET_EVENT_IN | MLOOP_SOCKET_EVENT_PRI;

	sigemptyset(&self->sigset);
	socket->fd = signalfd(-1, &self->sigset, SFD_NONBLOCK);
	if (socket->fd < 0)
		goto failure;

	mloop__print_debug(self, "new", 1, 1);

	return self;

failure:
	free(self);
	return NULL;
}

EXPORT
struct mloop_async* mloop_async_new(struct mloop* creator)
{
	struct mloop_async* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	self->type = MLOOP_ASYNC;
	self->priority = ULONG_MAX;
	self->ref = 1;
	self->creator = creator;

	return self;
}

EXPORT
struct mloop_work* mloop_work_new(struct mloop* creator)
{
	struct mloop_work* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	self->type = MLOOP_WORK;
	self->priority = ULONG_MAX;
	self->ref = 1;
	self->creator = creator;

	mloop__print_debug(self, "new", 1, 1);

	return self;
}

EXPORT
struct mloop_idle* mloop_idle_new(struct mloop* creator)
{
	struct mloop_idle* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	self->type = MLOOP_IDLE;
	self->ref = 1;
	self->creator = creator;

	mloop__print_debug(self, "new", 1, 1);

	return self;
}

EXPORT
void mloop_socket_free(struct mloop_socket* self)
{
	mloop__free_context(self);
	if (self->fd >= 0)
		close(self->fd);
	free(self);
}

EXPORT
void mloop_timer_free(struct mloop_timer* self)
{
	mloop_socket_free(&self->socket);
}

EXPORT
void mloop_async_free(struct mloop_async* self)
{
	mloop__free_context(self);
	free(self);
}

EXPORT
void mloop_work_free(struct mloop_work* self)
{
	mloop__free_context(self);
	free(self);
}

EXPORT
void mloop_signal_free(struct mloop_signal* self)
{
	mloop_socket_free(&self->socket);
}

EXPORT
void mloop_idle_free(struct mloop_idle* self)
{
	mloop__free_context(self);
	free(self);
}

void mloop__read_timer(struct mloop_socket* timer)
{
	uint64_t dummy;
	read(timer->fd, &dummy, sizeof(dummy));
}

void mloop__process_timer(struct mloop_socket* socket)
{
	struct mloop_timer* timer = (struct mloop_timer*)socket;

	mloop__read_timer(socket);

	int is_periodic = timer->timer_type & MLOOP_TIMER_PERIODIC;
	if (is_periodic)
		return;

	if (mloop__change_state(timer, MLOOP_STARTED, MLOOP_STOPPING) < 0)
		return;

	mloop__socket_stop(socket);

	int rc = mloop__change_state(timer, MLOOP_STOPPING, MLOOP_STOPPED);
	assert(rc == 0);
}

enum mloop_socket_event
mloop_socket_get_event(const struct mloop_socket* socket)
{
	return socket->revents;
}

EXPORT
void mloop_socket_set_event(struct mloop_socket* socket,
			    enum mloop_socket_event events)
{
	socket->events = events;
}

enum mloop_socket_event
mloop__get_socket_event(uint32_t events)
{
	enum mloop_socket_event e = MLOOP_SOCKET_EVENT_NONE;
	e |= events & EPOLLIN  ? MLOOP_SOCKET_EVENT_IN  : 0;
	e |= events & EPOLLERR ? MLOOP_SOCKET_EVENT_ERR : 0;
	e |= events & EPOLLHUP ? MLOOP_SOCKET_EVENT_HUP : 0;
	e |= events & EPOLLPRI ? MLOOP_SOCKET_EVENT_PRI : 0;
	e |= events & EPOLLOUT ? MLOOP_SOCKET_EVENT_OUT : 0;
	return e;
}

uint32_t mloop__get_epoll_event(enum mloop_socket_event events)
{
	uint32_t e = 0;
	e |= events & MLOOP_SOCKET_EVENT_IN  ? EPOLLIN  : 0;
	e |= events & MLOOP_SOCKET_EVENT_ERR ? EPOLLERR : 0;
	e |= events & MLOOP_SOCKET_EVENT_HUP ? EPOLLHUP : 0;
	e |= events & MLOOP_SOCKET_EVENT_PRI ? EPOLLPRI : 0;
	e |= events & MLOOP_SOCKET_EVENT_OUT ? EPOLLOUT : 0;
	return e;
}

void mloop__process_events(struct mloop* self, struct epoll_event* events,
			   int nfds)
{
	int i;

	/* All active events are referenced/unreferenced before/after
	 * processing. This is so that they don't get freed if they are
	 * unreferenced within callbacks because that would cause dangling
	 * pointers in the events structure.
	 */
	for (i = 0; i < nfds; ++i)
		mloop__ref_any(events[i].data.ptr);

	for (i = 0; i < nfds; ++i) {
		struct epoll_event* event = &events[i];
		struct mloop_socket* socket = event->data.ptr;

		/* Let's not process this if it's already freed */
		if (mloop_socket_unref(socket) == 0)
			continue;

		socket->revents = mloop__get_socket_event(event->events);

		mloop_socket_fn callback_fn = socket->callback_fn;
		if (callback_fn)
			callback_fn(socket);

		if (socket->type == MLOOP_TIMER)
			mloop__process_timer(socket);
	}
}

void mloop__process_async_jobs(struct mloop* self)
{
	struct prioq_elem elem;

	if (prioq_pop(&self->core->async_jobs, &elem, 0) < 0)
		return;

	struct mloop_async* async = elem.data;
	assert(async);
	assert(async->state == MLOOP_STARTED);

	if (async->is_cancelled)
		goto cancelled;

	mloop_async_fn callback_fn = async->callback_fn;
	if (callback_fn)
		callback_fn(async);

cancelled:
	if (mloop__object_list_remove(async) == 0)
		return;

	int rc = mloop__change_state(async, MLOOP_STARTED, MLOOP_STOPPED);
	assert(rc == 0);
}

void mloop__process_idle_jobs(struct mloop* self)
{
	/* Note: pop() does not unreference the job and this is crucial for the
	 * sake of concurrency. */
	struct mloop_idle* job = mloop__idle_list_pop(self->core);
	if (!job)
		return;

	mloop_idle_cond_fn cond_fn = job->cond_fn;
	if (cond_fn && cond_fn(job)) {
		mloop_idle_fn idle_fn = job->idle_fn;
		if (idle_fn)
			idle_fn(job);
	}

	if (mloop_idle_unref(job) > 0)
		mloop__idle_list_add(job);
}

static inline int mloop__have_idle_jobs_nolocks(const struct mloop* self)
{
	struct mloop_idle* idle;
	TAILQ_FOREACH(idle, &self->core->idle_jobs, idle_links)
		if (idle->cond_fn && idle->cond_fn(idle))
			return 1;
	return 0;
}

static inline int mloop__have_idle_jobs(struct mloop* self)
{
	mloop__idle_list_lock(self->core);
	int rc = mloop__have_idle_jobs_nolocks(self);
	mloop__idle_list_unlock(self->core);
	return rc;
}

static inline int mloop__have_async_or_idle_jobs(struct mloop* self)
{
	return self->core->async_jobs.index > 0 || mloop__have_idle_jobs(self);
}

EXPORT
int mloop_run(struct mloop* self)
{
	struct epoll_event events[MAX_EVENTS];
	memset(events, 0, sizeof(events));

	self->core->do_exit = 0;

	int old_cancel_type = 0;
	int old_cancel_state = 0;

	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &old_cancel_type);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_cancel_state);

	while (!mloop__is_exiting(self)) {
		int timeout = mloop__have_async_or_idle_jobs(self) ? 0 : -1;

		int nfds = epoll_wait(self->core->epollfd, events, MAX_EVENTS,
				      timeout);

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		if (nfds > 0)
			mloop__process_events(self, events, nfds);

		mloop__process_async_jobs(self);
		mloop__process_idle_jobs(self);
		mloop__collect(self->core);


		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	}

	pthread_setcancelstate(old_cancel_state, NULL);
	pthread_setcanceltype(old_cancel_type, NULL);

	return 0;
}

EXPORT
int mloop_run_once(struct mloop* self)
{
	struct epoll_event events[MAX_EVENTS];

	int nfds = epoll_wait(self->core->epollfd, events, MAX_EVENTS, 0);
	if (nfds > 0)
		mloop__process_events(self, events, nfds);

	mloop__process_async_jobs(self);
	mloop__process_idle_jobs(self);
	mloop__collect(self->core);

	return 0;
}

EXPORT
void mloop_iterate(struct mloop* self)
{
	mloop__break_out(self);
}

EXPORT
void mloop_exit(struct mloop* self)
{
	mloop__do_exit(self);
	mloop__break_out(self);
}

static int mloop__start_socket(struct mloop* self, struct mloop_socket* socket)
{
	struct epoll_event event = {
		.events = mloop__get_epoll_event(socket->events),
		.data.ptr = socket
	};

	socket->parent = self;
	socket->parent_core = self->core;

	if (epoll_ctl(self->core->epollfd, EPOLL_CTL_ADD, socket->fd, &event) < 0)
		return -1;

	mloop__object_list_add(socket);

	return 0;
}

EXPORT
int mloop_socket_start(struct mloop_socket* socket)
{
	struct mloop* mloop = socket->creator;

	if (mloop__change_state(socket, MLOOP_STOPPED, MLOOP_STARTING) < 0)
		return -1;

	if (mloop__start_socket(mloop, socket) < 0)
		goto failure;

	int rc = mloop__change_state(socket, MLOOP_STARTING, MLOOP_STARTED);
	assert(rc == 0);

	return 0;

failure:
	rc = mloop__change_state(socket, MLOOP_STARTING, MLOOP_STOPPED);
	assert(rc == 0);
	return -1;
}

static int mloop__socket_stop(struct mloop_socket* self)
{
	struct mloop* mloop = self->parent;

	int rc = epoll_ctl(mloop->core->epollfd, EPOLL_CTL_DEL, self->fd, NULL);
	mloop__object_list_remove(self);

	return rc;
}

EXPORT
int mloop_socket_stop(struct mloop_socket* self)
{
	if (mloop__change_state(self, MLOOP_STARTED, MLOOP_STOPPING) < 0)
		return -1;

	int src = mloop__socket_stop(self);

	int rc = mloop__change_state(self, MLOOP_STOPPING, MLOOP_STOPPED);
	assert(rc == 0);

	return src;
}

EXPORT
int mloop_timer_start(struct mloop_timer* timer)
{
	struct mloop_socket* socket = &timer->socket;
	struct mloop* mloop = socket->creator;

	if (mloop__change_state(socket, MLOOP_STOPPED, MLOOP_STARTING) < 0)
		return -1;

	struct timespec ts = {
		.tv_sec = timer->time / 1000000000LL,
		.tv_nsec = timer->time % 1000000000LL
	};

	struct itimerspec its;
	memset(&its, 0, sizeof(its));

	its.it_value = ts;

	if (timer->timer_type & MLOOP_TIMER_PERIODIC)
		its.it_interval = ts;

	int flags = timer->timer_type & MLOOP_TIMER_ABSOLUTE ? TFD_TIMER_ABSTIME : 0;

	if (timerfd_settime(socket->fd, flags, &its, NULL) < 0)
		goto failure;

	if (mloop__start_socket(mloop, socket) < 0)
		goto failure;

	int rc = mloop__change_state(socket, MLOOP_STARTING, MLOOP_STARTED);
	assert(rc == 0);

	return 0;

failure:
	rc = mloop__change_state(socket, MLOOP_STARTING, MLOOP_STOPPED);
	assert(rc == 0);
	return -1;
}

EXPORT
int mloop_timer_stop(struct mloop_timer* self)
{
	struct mloop_socket* socket = &self->socket;

	if (mloop__change_state(self, MLOOP_STARTED, MLOOP_STOPPING) < 0)
		return -1;

	struct itimerspec its;
	memset(&its, 0, sizeof(its));

	if (timerfd_settime(socket->fd, 0, &its, NULL) < 0)
		goto failure;

	mloop_socket_ref(socket);
	mloop__socket_stop(socket);
	if (mloop_socket_unref(socket) == 0)
		return 0;

	int rc = mloop__change_state(self, MLOOP_STOPPING, MLOOP_STOPPED);
	assert(rc == 0);

	return 0;

failure:
	rc = mloop__change_state(self, MLOOP_STOPPING, MLOOP_STARTED);
	assert(rc == 0);
	return -1;
}

EXPORT
int mloop_signal_start(struct mloop_signal* sig)
{
	struct mloop_socket* socket = &sig->socket;
	struct mloop* mloop = socket->creator;
	int fd;

	if (mloop__change_state(sig, MLOOP_STOPPED, MLOOP_STARTING) < 0)
		return -1;

	fd = signalfd(socket->fd, &sig->sigset, SFD_NONBLOCK);
	if (fd < 0)
		goto signalfd_failure;

	if (mloop__start_socket(mloop, socket) < 0)
		goto socket_start_failure;

	int rc = mloop__change_state(sig, MLOOP_STARTING, MLOOP_STARTED);
	assert(rc == 0);

	return 0;

socket_start_failure:
	close(fd);
signalfd_failure:
	rc = mloop__change_state(sig, MLOOP_STARTING, MLOOP_STOPPED);
	assert(rc == 0);
	return -1;
}

EXPORT
int mloop_signal_stop(struct mloop_signal* self)
{
	if (mloop__change_state(self, MLOOP_STARTED, MLOOP_STOPPING) < 0)
		return -1;

	mloop__socket_stop(&self->socket);

	int rc = mloop__change_state(self, MLOOP_STOPPING, MLOOP_STOPPED);
	assert(rc == 0);

	return 0;
}

static int mloop__start_idle(struct mloop* self, struct mloop_idle* idle)
{
	idle->parent = self;
	idle->parent_core = self->core;
	mloop__object_list_add(idle);
	mloop__idle_list_add(idle);
	mloop__break_out(self);
	return 0;
}

EXPORT
int mloop_idle_start(struct mloop_idle* idle)
{
	struct mloop* mloop = idle->creator;

	if (mloop__change_state(idle, MLOOP_STOPPED, MLOOP_STARTING) < 0)
		return -1;

	if (mloop__start_idle(mloop, idle) < 0)
		goto failure;

	int rc = mloop__change_state(idle, MLOOP_STARTING, MLOOP_STARTED);
	assert(rc == 0);

	return 0;

failure:
	rc = mloop__change_state(idle, MLOOP_STARTING, MLOOP_STOPPED);
	assert(rc == 0);
	return -1;
}

static int mloop__idle_stop(struct mloop_idle* self)
{
	mloop__idle_list_remove(self);
	mloop__object_list_remove(self);
	return 0;
}

EXPORT
int mloop_idle_stop(struct mloop_idle* self)
{
	if (mloop__change_state(self, MLOOP_STARTED, MLOOP_STOPPING) < 0)
		return -1;

	mloop__idle_stop(self);

	int rc = mloop__change_state(self, MLOOP_STOPPING, MLOOP_STOPPED);
	assert(rc == 0);

	return 0;
}

static int mloop__start_async(struct mloop* self, struct mloop_async* async)
{
	struct prioq* queue = &self->core->async_jobs;

	async->parent = self;
	async->parent_core = self->core;
	async->is_cancelled = 0;

	if (prioq_insert(queue, async->priority, async) < 0)
		return -1;

	mloop__object_list_add(async);
	mloop__break_out(self);

	return 0;
}

EXPORT
int mloop_async_start(struct mloop_async* async)
{
	struct mloop* mloop = async->creator;

	if (mloop__change_state(async, MLOOP_STOPPED, MLOOP_STARTING) < 0)
		return -1;

	/* We lock the async queue here so that we can keep it blocked until the
	 * state has been changed to MLOOP_STARTED. Otherwise the main thread
	 * can start processing the job before the state has been changed.
	 */
	prioq__lock(&mloop->core->async_jobs);
	if (mloop__start_async(mloop, async) < 0)
		goto failure;

	int rc = mloop__change_state(async, MLOOP_STARTING, MLOOP_STARTED);
	assert(rc == 0);
	prioq__unlock(&mloop->core->async_jobs);

	return 0;

failure:
	prioq__unlock(&mloop__job_queue);
	rc = mloop__change_state(async, MLOOP_STARTING, MLOOP_STOPPED);
	assert(rc == 0);
	return -1;
}

EXPORT
void mloop_async_cancel(struct mloop_async* self)
{
	self->is_cancelled = 1;
}

EXPORT
int mloop_work_start(struct mloop_work* work)
{
	int rc;
	struct mloop* mloop = work->creator;

	if (mloop__change_state(work, MLOOP_STOPPED, MLOOP_STARTING) < 0)
		return -1;

	work->parent = mloop;
	work->parent_core = mloop->core;
	work->is_cancelled = 0;

	/* We lock the work queue here so that we can keep it blocked until the
	 * object has been added to the list of active objects.
	 *
	 * We could also reverse the order and add "mloop__object_list_remove()"
	 * to the failure case for prioq_insert()
	 */
	prioq__lock(&mloop__job_queue);
	if (prioq_insert(&mloop__job_queue, work->priority, work) < 0)
		goto failure;

	mloop__object_list_add(work);
	prioq__unlock(&mloop__job_queue);

	return 0;

failure:
	prioq__unlock(&mloop__job_queue);
	rc = mloop__change_state(work, MLOOP_STARTING, MLOOP_STOPPED);
	assert(rc == 0);
	return -1;
}

EXPORT
void mloop_work_cancel(struct mloop_work* self)
{
	self->is_cancelled = 1;
}

EXPORT
void mloop_timer_set_type(struct mloop_timer* self,
			  enum mloop_timer_type type)
{
	assert(type != (MLOOP_TIMER_ABSOLUTE | MLOOP_TIMER_PERIODIC));

	self->timer_type = type;
}

EXPORT
enum mloop_timer_type mloop_timer_get_type(const struct mloop_timer* self)
{
	return self->timer_type;
}

EXPORT
void mloop_timer_set_time(struct mloop_timer* self, uint64_t time)
{
	self->time = time;
}

EXPORT
void mloop_timer_set_context(struct mloop_timer* self, void* context,
			     mloop_free_fn free_fn)
{
	self->socket.context = context;
	self->socket.free_fn = free_fn;
}

EXPORT
void mloop_timer_set_callback(struct mloop_timer* self, mloop_timer_fn fn)
{
	self->socket.callback_fn = (mloop_socket_fn)fn;
}

EXPORT
void* mloop_timer_get_context(const struct mloop_timer* self)
{
	return self->socket.context;
}

EXPORT
int mloop_timer_is_started(const struct mloop_timer* self)
{
	return mloop__atomic_load(&self->socket.state) == MLOOP_STARTED;
}

EXPORT
int mloop_socket_is_started(const struct mloop_socket* self)
{
	return mloop__atomic_load(&self->state) == MLOOP_STARTED;
}

EXPORT
int mloop_async_is_started(const struct mloop_async* self)
{
	return mloop__atomic_load(&self->state) != MLOOP_STOPPED;
}

EXPORT
int mloop_work_is_started(const struct mloop_work* self)
{
	return mloop__atomic_load(&self->state) != MLOOP_STOPPED;
}

EXPORT
int mloop_signal_is_started(const struct mloop_signal* self)
{
	return mloop__atomic_load(&self->socket.state) == MLOOP_STARTED;
}

EXPORT
int mloop_idle_is_started(const struct mloop_idle* self)
{
	return mloop__atomic_load(&self->state) == MLOOP_STARTED;
}

EXPORT
void mloop_socket_set_fd(struct mloop_socket* self, int fd)
{
	self->fd = fd;
}

EXPORT
int mloop_socket_get_fd(const struct mloop_socket* self)
{
	return self->fd;
}

EXPORT
void mloop_socket_set_context(struct mloop_socket* self, void* context,
			      mloop_free_fn free_fn)
{
	self->context = context;
	self->free_fn = free_fn;
}

EXPORT
void mloop_socket_set_callback(struct mloop_socket* self, mloop_socket_fn fn)
{
	self->callback_fn = fn;
}

EXPORT
void* mloop_socket_get_context(const struct mloop_socket* self)
{
	return self->context;
}

EXPORT
void mloop_async_set_context(struct mloop_async* self, void* context,
			    mloop_free_fn free_fn)
{
	self->context = context;
	self->free_fn = free_fn;
}

EXPORT
void mloop_async_set_callback(struct mloop_async* self, mloop_async_fn fn)
{
	self->callback_fn = fn;
}

EXPORT
void* mloop_async_get_context(const struct mloop_async* self)
{
	return self->context;
}

EXPORT
void mloop_async_set_priority(struct mloop_async* self, unsigned long priority)
{
	self->priority = priority;
}

EXPORT
void mloop_work_set_context(struct mloop_work* self, void* context,
			    mloop_free_fn free_fn)
{
	self->context = context;
	self->free_fn = free_fn;
}

EXPORT
void mloop_work_set_work_fn(struct mloop_work* self, mloop_work_fn fn)
{
	self->work_fn = fn;
}

EXPORT
void mloop_work_set_done_fn(struct mloop_work* self, mloop_work_fn fn)
{
	self->done_fn = fn;
}

EXPORT
void* mloop_work_get_context(const struct mloop_work* self)
{
	return self->context;
}

EXPORT
void mloop_work_set_priority(struct mloop_work* self, unsigned long priority)
{
	self->priority = priority;
}

EXPORT
void mloop_signal_set_callback(struct mloop_signal* self, mloop_signal_fn fn)
{
	self->signal_fn = fn;
}

EXPORT
void mloop_signal_set_context(struct mloop_signal* self, void* context,
			      mloop_free_fn free_fn)
{
	self->socket.context = context;
	self->socket.free_fn = free_fn;
}

EXPORT
void* mloop_signal_get_context(const struct mloop_signal* self)
{
	return self->socket.context;
}

EXPORT
void mloop_signal_set_signals(struct mloop_signal* self, const sigset_t* mask)
{
	sigemptyset(&self->sigset);
	sigorset(&self->sigset, &self->sigset, mask);
}

EXPORT
void mloop_idle_set_context(struct mloop_idle* self, void* context,
			    mloop_free_fn fn)
{
	self->context = context;
	self->free_fn = fn;
}

EXPORT
void mloop_idle_set_idle_fn(struct mloop_idle* self, mloop_idle_fn fn)
{
	self->idle_fn = fn;
}

EXPORT
void mloop_idle_set_cond_fn(struct mloop_idle* self, mloop_idle_cond_fn fn)
{
	self->cond_fn = fn;
}

EXPORT
void* mloop_idle_get_context(const struct mloop_idle* idle)
{
	return idle->context;
}
