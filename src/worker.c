#include <assert.h>
#include "prioq.h"
#include "worker.h"

static struct prioq job_queue_;
static int nthreads_;
static pthread_t* threads_;

struct job {
	void (*fn)(void*);
	void* context;
};

static struct job* job_new(void (*job_fn)(void*), void* context)
{
	struct job* job = malloc(sizeof(*job));
	if (!job)
		return NULL;

	job->fn = job_fn;
	job->context = context;

	return job;
}

static void job_del(struct job* job)
{
	free(job);
}

static void* thread_fn(void* context)
{
	(void)context;

	while (1) {
		struct prioq_elem elem;
		if (prioq_pop(&job_queue_, &elem, -1) < 0)
			continue;

		if (elem.data == NULL)
			break;

		struct job* job = (struct job*)elem.data;
		job->fn(job->context);
		job_del(job);

	}

	return NULL;
}

static int start_threads(size_t stacksize)
{
	int i;
	pthread_attr_t attr;

	pthread_attr_init(&attr);

	if (stacksize > 0)
		pthread_attr_setstacksize(&attr, stacksize);

	for (i = 0; i < nthreads_; ++i)
		pthread_create(&threads_[i], &attr, thread_fn, (void*)i);

	pthread_attr_destroy(&attr);
	return 0;
}

int worker_init(int nthreads, size_t qsize, size_t stacksize)
{
	if (prioq_init(&job_queue_, qsize) < 0)
		return -1;

	nthreads_ = nthreads;
	threads_ = calloc(nthreads_, sizeof(pthread_t));
	if (!threads_)
		goto thread_alloc_failure;

	if (start_threads(stacksize) < 0)
		goto thread_start_failure;

	return 0;

thread_start_failure:
	free(threads_);
thread_alloc_failure:
	prioq_clear(&job_queue_);
	return -1;
}

static void clear_jobs()
{
	struct prioq_elem elem;

	while (prioq_pop(&job_queue_, &elem, 0) == 0)
		if (elem.data)
			job_del(elem.data);
}

static void reap_threads()
{
	for (int i = 0; i < nthreads_; ++i)
		prioq_insert(&job_queue_, 0, NULL);

	for (int i = 0; i < nthreads_; ++i)
		pthread_join(threads_[i], NULL);
}

void worker_deinit()
{
	reap_threads();
	free(threads_);
	clear_jobs();
	prioq_clear(&job_queue_);
}

int worker_add_job(unsigned long priority, void (*job_fn)(void*), void* context)
{
	struct job* job = job_new(job_fn, context);
	if (!job)
		return -1;

	if (prioq_insert(&job_queue_, priority, job) < 0)
		return -1;

	return 0;
}

