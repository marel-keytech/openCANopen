#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>

#include "socketcan.h"
#include "canopen.h"
#include "canopen/nmt.h"
#include "canopen/heartbeat.h"
#include "canopen/network.h"
#include "canopen/sdo.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))

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
	int t = net__gettime_ms();
	int t_end = t + timeout;
	ssize_t size;

	while (1) {
		size = net_read_frame(fd, cf, t_end - t);
		if (size < 0)
			return -1;

		if (cf->can_id == can_id)
			return size;

		t = net__gettime_ms();
	}

	return -1;
}

int net__send_nmt(int fd, int cs, int nodeid)
{
	struct can_frame cf = { 0 };
	cf.can_id = 0;
	cf.can_dlc = 2;
	nmt_set_cs(&cf, cs);
	nmt_set_nodeid(&cf, nodeid);
	return net_write_frame(fd, &cf, -1);
}

int net__request_heartbeat(int fd, int nodeid)
{
	struct can_frame cf = { 0 };
	cf.can_id = R_HEARTBEAT + nodeid;
	cf.can_dlc = 0;
	return net_write_frame(fd, &cf, -1);
}

int net__gettime_ms()
{
	struct timespec ts = { 0 };
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int net__wait_for_bootup(int fd, char* nodes_seen, int start, int end,
			 int timeout)
{
	struct can_frame cf;
	struct canopen_msg msg;

	int t = net__gettime_ms();
	int t_end = t + timeout;

	while (net_read(fd, &cf, sizeof(cf), MAX(0, t_end - t)) > 0) {
		t = net__gettime_ms();

		canopen_get_object_type(&msg, &cf);

		if (!(start <= msg.id && msg.id <= end))
			continue;

		if (msg.object != CANOPEN_HEARTBEAT)
			continue;

		nodes_seen[msg.id] = 1;
		t_end = t + timeout;
	}

	return 0;
}

int net_reset(int fd, char* nodes_seen, int timeout)
{
	net__send_nmt(fd, NMT_CS_RESET_COMMUNICATION, 0);
	return net__wait_for_bootup(fd, nodes_seen, 0, 127, timeout);
}

int net_reset_range(int fd, char* nodes_seen, int start, int end, int timeout)
{
	for (int i = start; i <= end; ++i)
		net__send_nmt(fd, NMT_CS_RESET_COMMUNICATION, i);

	return net__wait_for_bootup(fd, nodes_seen, start, end, timeout);
}

int net_probe(int fd, char* nodes_seen, int start, int end, int timeout)
{
	for (int i = start; i <= end; ++i)
		if (!nodes_seen[i])
			net__request_heartbeat(fd, i);

	return net__wait_for_bootup(fd, nodes_seen, start, end, timeout);
}

int net_fix_sndbuf(int fd)
{
	int sndbuf = 0;
	return setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
}
