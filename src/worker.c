#include <assert.h>
#include "prioq.h"
#include "worker.h"

static struct prioq job_queue_;
static int nthreads_;
static pthread_t* threads_;
static pthread_mutex_t mutex_;
static pthread_cond_t cond_;
static unsigned long njobs_;
static int please_exit_;

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

static inline void lock()
{
	pthread_mutex_lock(&mutex_);
}

static inline void unlock()
{
	pthread_mutex_unlock(&mutex_);
}

static void wait_for_jobs()
{
	lock();
	while (!please_exit_ && njobs_ > 0)
		pthread_cond_wait(&cond_, &mutex_);
	unlock();
}

static void decrement_njobs()
{
	lock();
	njobs_ -= njobs_ > 0 ? 1 : 0;
	unlock();
}

static void* thread_fn(void* unused)
{
	(void)unused;
	struct prioq_elem elem;

	while (1) {
		wait_for_jobs();

		if (please_exit_)
			break;

		while (prioq_pop(&job_queue_, &elem)) {
			decrement_njobs();
			struct job* job = (struct job*)elem.data;
			job->fn(job->context);
			job_del(job);
		}
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
		pthread_create(&threads_[i], &attr, thread_fn, NULL);

	pthread_attr_destroy(&attr);
	return 0;
}

int worker_init(int nthreads, size_t qsize, size_t stacksize)
{
	njobs_ = 0;
	please_exit_ = 0;

	if (prioq_init(&job_queue_, qsize) < 0)
		return -1;

	pthread_mutex_init(&mutex_, NULL);
	pthread_cond_init(&cond_, NULL);

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
	pthread_cond_destroy(&cond_);
	pthread_mutex_destroy(&mutex_);
	prioq_clear(&job_queue_);
	return -1;
}

static void clear_jobs()
{
	struct prioq_elem elem;

	while (prioq_pop(&job_queue_, &elem))
		job_del(elem.data);
}

static void reap_threads()
{
	pthread_cond_broadcast(&cond_);
	please_exit_ = 1;

	for (int i = 0; i < nthreads_; ++i)
		pthread_join(threads_[i], NULL);
}

void worker_deinit()
{
	reap_threads();
	free(threads_);
	clear_jobs();
	prioq_clear(&job_queue_);
	pthread_cond_destroy(&cond_);
	pthread_mutex_destroy(&mutex_);
}

static void increment_njobs()
{
	lock();
	++njobs_;
	pthread_cond_broadcast(&cond_);
	unlock();
}

int worker_add_job(unsigned long priority, void (*job_fn)(void*), void* context)
{
	struct job* job = job_new(job_fn, context);
	if (!job)
		return -1;

	if (prioq_insert(&job_queue_, priority, job) < 0)
		return -1;

	increment_njobs();

	return 0;
}

