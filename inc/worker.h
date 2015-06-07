#ifndef WORKER_H_
#define WORKER_H_

#include <pthread.h>

int worker_init(int nthreads, size_t qsize, size_t stacksize);
void worker_deinit();

int worker_add_job(unsigned long priority, void (*job)(void*), void* context);

#endif /* WORKER_H_ */

