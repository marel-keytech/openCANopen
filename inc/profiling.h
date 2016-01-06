#ifndef PROFILING_H_
#define PROFILING_H_

#include <time-utils.h>
#include <stdint.h>

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
	printf("%07llu\t" fmt, gettime_us(CLOCK_MONOTONIC) - profiling_start_time_, \
		## __VA_ARGS__)

#define profile(fmt, ...) \
	if (profiling_is_active()) tprintf(fmt, ## __VA_ARGS__)

#endif /* PROFILING_H_ */
