#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <plog.h>
#include <errno.h>
#include <string.h>

#include "socketcan.h"
#include "net-util.h"
#include "time-utils.h"

static ssize_t net__write(int fd, const void* src, size_t size, int timeout)
{
	struct pollfd pollfd = { .fd = fd, .events = POLLOUT, .revents = 0 };
	if (poll(&pollfd, 1, timeout) == 1)
		return write(fd, src, size);
	return -1;
}

ssize_t net_write(int fd, const void* src, size_t size, int timeout)
{
	errno = 0;
	ssize_t rc = net__write(fd, src, size, timeout);
	if (rc < 0 && errno && errno != EAGAIN && errno != EWOULDBLOCK)
		plog(LOG_ERROR, "Failed to write to CAN bus: %s",
		     strerror(errno));
	return rc;
}

ssize_t net_write_frame(int fd, const struct can_frame* cf, int timeout)
{
	return net_write(fd, cf, sizeof(*cf), timeout);
}

static ssize_t net__read(int fd, void* dst, size_t size, int timeout)
{
	struct pollfd pollfd = { .fd = fd, .events = POLLIN, .revents = 0 };
	if (poll(&pollfd, 1, timeout) == 1)
		return read(fd, dst, size);
	return -1;
}

ssize_t net_read(int fd, void* dst, size_t size, int timeout)
{
	errno = 0;
	ssize_t rc = net__read(fd, dst, size, timeout);
	if (rc < 0 && errno && errno != EAGAIN && errno != EWOULDBLOCK)
		plog(LOG_ERROR, "Failed to read from CAN bus: %s",
		     strerror(errno));
	return rc;
}

ssize_t net_read_frame(int fd, struct can_frame* cf, int timeout)
{
	return net_read(fd, cf, sizeof(*cf), timeout);
}

ssize_t net_filtered_read_frame(int fd, struct can_frame* cf, int timeout,
				uint32_t can_id)
{
	int t = gettime_ms();
	int t_end = t + timeout;
	ssize_t size;

	while (1) {
		size = net_read_frame(fd, cf, t_end - t);
		if (size < 0)
			return -1;

		if (cf->can_id == can_id)
			return size;

		t = gettime_ms();
	}

	return -1;
}

int net_fix_sndbuf(int fd)
{
	int sndbuf = 0;
	return setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
}

int net_reuse_addr(int fd)
{
	int one = 1;
	return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
}

int net_dont_delay(int fd)
{
	int one = 1;
	return setsockopt(fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
}
