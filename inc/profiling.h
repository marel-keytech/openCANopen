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

#ifndef PROFILING_H_
#define PROFILING_H_

#include <time-utils.h>
#include <stdint.h>
#include <inttypes.h>

extern int profiling_is_active_;
extern uint64_t profiling_start_time_;

int profiling_getenv(void);

static inline int profiling_is_active(void)
{
	if (profiling_is_active_ < 0)
		profiling_is_active_ = profiling_getenv();
	return profiling_is_active_;
}

static inline void profiling_reset(void)
{
	profiling_start_time_ = gettime_us(CLOCK_MONOTONIC);
}

#define tprintf(fmt, ...) \
	printf("%07"PRIu64"\t" fmt, gettime_us(CLOCK_MONOTONIC) - profiling_start_time_, \
		## __VA_ARGS__)

#define profile(fmt, ...) \
	if (profiling_is_active()) tprintf(fmt, ## __VA_ARGS__)

#endif /* PROFILING_H_ */
