#ifndef THREAD_UTILS_H_
#define THREAD_UTILS_H_

#include <pthread.h>
#include <errno.h>
#include <time.h>

#include "time-utils.h"

#define block_thread_while_empty(cond, mutex, timeout, cmp) \
({ \
 	struct timespec deadline_; \
 	int rc_ = 0; \
	if ((timeout) == 0) { \
		if (cmp) { \
			errno = EAGAIN; \
 			rc_ = -1; \
 		} \
	} else if ((timeout) < 0) { \
 		while (cmp) \
 			pthread_cond_wait((cond), (mutex)); \
 	} else { \
 		while (cmp) { \
 			clock_gettime(CLOCK_REALTIME, &deadline_); \
 			add_to_timespec(&deadline_, msec_to_nsec(timeout)); \
 			if (pthread_cond_timedwait((cond), (mutex), \
						   &deadline_) == ETIMEDOUT) \
 				if (cmp) { \
 					errno = ETIMEDOUT; \
 					rc_ = -1; \
 					break; \
 				} \
		} \
 	} \
 	rc_; \
})

#endif /* THREAD_UTILS_H_ */

