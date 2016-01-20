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

int sock_timed_send(const struct sock* sock, struct can_frame* cf, int timeout);

ssize_t sock_recv(const struct sock* sock, struct can_frame* cf, int flags);
int sock_timed_recv(const struct sock* sock, struct can_frame* cf, int timeout);

static inline int sock_close(struct sock* sock)
{
	return close(sock->fd);
}

#endif /* CAN_SOCK_H_ */
