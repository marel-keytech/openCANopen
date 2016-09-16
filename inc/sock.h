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

#ifndef CAN_SOCK_H_
#define CAN_SOCK_H_

#include <unistd.h>

struct can_frame;

enum sock_type {
	SOCK_TYPE_UNSPEC = 0,
	SOCK_TYPE_CAN = 1,
	SOCK_TYPE_TCP = 2,
};

struct sock {
	enum sock_type type;
	int fd;
};

static inline void sock_init(struct sock* sock, enum sock_type type, int fd)
{
	sock->type = type;
	sock->fd = fd;
}

int sock_open(struct sock* sock, enum sock_type type, const char* addr);

ssize_t sock_send(const struct sock* sock, struct can_frame* cf, int flags);
int sock_timed_send(const struct sock* sock, struct can_frame* cf, int timeout);

ssize_t sock_recv(const struct sock* sock, struct can_frame* cf, int flags);
int sock_timed_recv(const struct sock* sock, struct can_frame* cf, int timeout);

static inline int sock_close(struct sock* sock)
{
	return close(sock->fd);
}

#endif /* CAN_SOCK_H_ */
