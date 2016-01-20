#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#include "sock.h"
#include "socketcan.h"
#include "net-util.h"
#include "can-tcp.h"

size_t strlcpy(char* dst, const char* src, size_t size);

static int sock__open_tcp(const char* addr)
{
	char buffer[256];
	strlcpy(buffer, addr, sizeof(buffer));
	char* portptr = strchr(buffer, ':');
	int port = 5555;
	if (portptr) {
		*portptr++ = '\0';
		port = atoi(portptr);
	}
	return can_tcp_open(buffer, port);
}

int sock_open(struct sock* sock, enum sock_type type, const char* addr)
{
	int fd = -1;
	switch (type) {
	case SOCK_TYPE_CAN: fd = socketcan_open(addr); break;
	case SOCK_TYPE_TCP: fd = sock__open_tcp(addr); break;
	default: abort();
	}
	sock_init(sock, type, fd);
	return fd;
}

static inline struct can_frame*
sock__frame_htonl(const struct sock* sock, struct can_frame* cf)
{
	if (sock->type != SOCK_TYPE_CAN)
		cf->can_id = htonl(cf->can_id);
	return cf;
}

static inline struct can_frame*
sock__frame_ntohl(const struct sock* sock, struct can_frame* cf)
{
	if (sock->type != SOCK_TYPE_CAN)
		cf->can_id = ntohl(cf->can_id);
	return cf;
}

int sock_send(const struct sock* sock, struct can_frame* cf, int timeout)
{
	return net_write_frame(sock->fd, sock__frame_htonl(sock, cf), timeout);
}

int sock_timed_recv(const struct sock* sock, struct can_frame* cf, int timeout)
{
	int rc = net_read_frame(sock->fd, cf, timeout);
	sock__frame_ntohl(sock, cf);
	return rc;
}

