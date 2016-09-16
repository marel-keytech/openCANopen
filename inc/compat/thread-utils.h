/* Copyright (c) 2014-2016, Marel
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

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

