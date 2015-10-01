#ifndef TIME_UTILS_H_
#define TIME_UTILS_H_

#include <stdint.h>
#include <time.h>

#define nSEC_IN_SEC 1000000000ULL

#define msec_to_nsec(t) ((t) * 1000000ULL)

static inline uint64_t timespec_to_ns(struct timespec* ts)
{
	return (uint64_t)ts->tv_sec * nSEC_IN_SEC + (uint64_t)ts->tv_nsec;
}

static inline struct timespec ns_to_timespec(uint64_t ns)
{
	struct timespec ts;
	ts.tv_nsec = ns % nSEC_IN_SEC;
	ts.tv_sec = ns / nSEC_IN_SEC;
	return ts;
}

static inline void add_to_timespec(struct timespec* ts, uint64_t addition)
{
	*ts = ns_to_timespec(timespec_to_ns(ts) + addition);
}

static inline uint64_t gettime_ns(void)
{
	struct timespec ts = { 0 };
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return timespec_to_ns(&ts);
}

static inline uint64_t gettime_us(void)
{
	return gettime_ns() / 1000ULL;
}

static inline uint64_t gettime_ms(void)
{
	return gettime_us() / 1000ULL;
}

static inline uint64_t gettime_s(void)
{
	return gettime_ms() / 1000ULL;
}

#endif /* TIME_UTILS_H_ */
