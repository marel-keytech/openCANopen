#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "socketcan.h"
#include "net-util.h"
#include "time-utils.h"

ssize_t net_write(int fd, const void* src, size_t size, int timeout)
{
	struct pollfd pollfd = { .fd = fd, .events = POLLOUT, .revents = 0 };
	if (poll(&pollfd, 1, timeout) == 1)
		return write(fd, src, size);
	return -1;
}

ssize_t net_write_frame(int fd, const struct can_frame* cf, int timeout)
{
	return net_write(fd, cf, sizeof(*cf), timeout);
}

ssize_t net_read(int fd, void* dst, size_t size, int timeout)
{
	struct pollfd pollfd = { .fd = fd, .events = POLLIN, .revents = 0 };
	if (poll(&pollfd, 1, timeout) == 1)
		return read(fd, dst, size);
	return -1;
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
