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

#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sock.h"
#include "socketcan.h"
#include "net-util.h"
#include "can-tcp.h"
#include "trace-buffer.h"

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

int sock_open(struct sock* sock, enum sock_type type, const char* addr,
	      struct tracebuffer* tb)
{
	int fd = -1;
	switch (type) {
	case SOCK_TYPE_CAN: fd = socketcan_open(addr); break;
	case SOCK_TYPE_TCP: fd = sock__open_tcp(addr); break;
	default: abort();
	}
	sock_init(sock, type, fd, tb);
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

ssize_t sock_send(const struct sock* sock, struct can_frame* cf, int flags)
{
	if (sock->tb)
		tb_append(sock->tb, cf);

	return send(sock->fd, sock__frame_htonl(sock, cf), sizeof(*cf), flags);
}

int sock_timed_send(const struct sock* sock, struct can_frame* cf, int timeout)
{
	if (sock->tb)
		tb_append(sock->tb, cf);

	return net_write_frame(sock->fd, sock__frame_htonl(sock, cf), timeout);
}

ssize_t sock_recv(const struct sock* sock, struct can_frame* cf, int flags)
{
	ssize_t rsize = recv(sock->fd, cf, sizeof(*cf), flags);
	if (rsize <= 0)
		return rsize;

	if (sock->tb)
		tb_append(sock->tb, cf);

	sock__frame_ntohl(sock, cf);
	return rsize;
}

int sock_timed_recv(const struct sock* sock, struct can_frame* cf, int timeout)
{
	int rc = net_read_frame(sock->fd, cf, timeout);

	if (rc >= 0 && sock->tb)
		tb_append(sock->tb, cf);

	sock__frame_ntohl(sock, cf);
	return rc;
}

