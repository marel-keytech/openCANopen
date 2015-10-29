#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mloop.h>

#include "sys/queue.h"
#include "socketcan.h"
#include "net-util.h"
#include "sock.h"

size_t strlcpy(char*, const char*, size_t);

struct can_tcp;

struct can_tcp_entry {
	struct sock sock;
	struct can_tcp* parent;
	LIST_ENTRY(can_tcp_entry) links;
};

LIST_HEAD(can_tcp_list, can_tcp_entry);

enum can_tcp_state {
	CAN_TCP_NEW = 0,
	CAN_TCP_READY
};

struct can_tcp {
	struct can_tcp_list list;
	size_t ref;
	enum can_tcp_state state;
};

static struct can_tcp* can_tcp__new(void)
{
	struct can_tcp* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	LIST_INIT(&self->list);
	self->ref = 1;
	self->state = CAN_TCP_NEW;

	return self;
}

static void can_tcp__free(struct can_tcp* self)
{
	free(self);
}

static void can_tcp__ref(struct can_tcp* self)
{
	++self->ref;
}

static void can_tcp__unref(struct can_tcp* self)
{
	if (--self->ref == 0)
		can_tcp__free(self);
}

void my_sock_send(struct sock* sock, const struct can_frame* cf)
{
	struct can_frame cp;
	memcpy(&cp, cf, sizeof(cp));
	sock_send(sock, &cp, -1);
}

static void can_tcp__send_to_others(struct can_tcp_entry* entry,
				    struct can_frame* cf)
{
	struct can_tcp* parent = entry->parent;
	struct can_tcp_entry* elem = NULL;

	LIST_FOREACH(elem, &parent->list, links)
		if (elem != entry)
			my_sock_send(&elem->sock, cf);
}

static void can_tcp__forward_message(struct mloop_socket* socket)
{
	struct can_frame cf;
	struct can_tcp_entry* entry = mloop_socket_get_context(socket);
	assert(entry);

	if (sock_recv(&entry->sock, &cf, 0) <= 0)
		mloop_socket_stop(socket);

	can_tcp__send_to_others(entry, &cf);
}

static void can_tcp_entry__free(void* ptr)
{
	struct can_tcp_entry* entry = ptr;
	LIST_REMOVE(entry, links);
	sock_close(&entry->sock);
	can_tcp__unref(entry->parent);
	free(entry);
}

static struct mloop_socket*
can_tcp__add_entry(struct can_tcp* self, const struct sock* sock)
{
	struct can_tcp_entry* entry = malloc(sizeof(*entry));
	if (!entry)
		return NULL;

	struct mloop_socket* s = mloop_socket_new(mloop_default());
	if (!s)
		goto failure;

	entry->parent = self;
	entry->sock = *sock;
	LIST_INSERT_HEAD(&self->list, entry, links);

	mloop_socket_set_context(s, entry, can_tcp_entry__free);
	mloop_socket_set_callback(s, can_tcp__forward_message);
	mloop_socket_set_fd(s, sock->fd);

	int rc = mloop_socket_start(s);
	mloop_socket_unref(s);

	if (rc >= 0)
		can_tcp__ref(self);

	return rc >= 0 ? s : NULL;

failure:
	free(entry);
	return NULL;
}

static int open_can(const char* iface)
{
	int fd = socketcan_open(iface);
	if (fd < 0)
		return -1;

	net_dont_block(fd);
	net_fix_sndbuf(fd);

	return fd;
}

static int open_tcp_server(int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	net_reuse_addr(fd);
	net_dont_block(fd);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (bind(fd, &addr, sizeof(addr)) < 0)
		goto failure;

	if (listen(fd, 16) < 0)
		goto failure;

	return fd;

failure:
	close(fd);
	return -1;
}

int can_tcp_open(const char* address, int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(address);

	if (connect(fd, &addr, sizeof(addr)) < 0)
		goto failure;

	net_dont_block(fd);

	return fd;

failure:
	close(fd);
	return -1;
}

static void can_tcp__on_connection(struct mloop_socket* socket)
{
	int sfd = mloop_socket_get_fd(socket);
	struct can_tcp* can_tcp = mloop_socket_get_context(socket);
	assert(can_tcp);

	int connfd = accept(sfd, NULL, 0);
	if (connfd < 0) {
		perror("Could not accept connection");
		return;
	}

	struct sock sock = { .fd = connfd, .type = SOCK_TYPE_TCP };

	struct mloop_socket* s = can_tcp__add_entry(can_tcp, &sock);
	if (!s) {
		perror("Could not create mloop handler for CAN interface");
		goto failure;
	}

	return;

failure:
	close(connfd);
}

struct mloop_socket* can_tcp__setup_server(int port)
{
	struct can_tcp* can_tcp = can_tcp__new();
	if (!can_tcp) {
		perror("ERROR");
		return NULL;
	}

	int sfd = open_tcp_server(port);
	if (sfd < 0) {
		perror("Could not open tcp server");
		goto server_failure;
	}

	struct mloop_socket* s = mloop_socket_new(mloop_default());
	if (!s) {
		perror("Could not create server socket handler");
		goto handler_failure;
	}

	mloop_socket_set_context(s, can_tcp, (mloop_free_fn)can_tcp__unref);
	mloop_socket_set_fd(s, sfd);
	mloop_socket_set_callback(s, can_tcp__on_connection);

	int rc = mloop_socket_start(s);
	if (rc < 0)
		perror("Could not start server socket handler");

	mloop_socket_unref(s);

	return rc >= 0 ? s : NULL;

handler_failure:
	close(sfd);
server_failure:
	can_tcp__free(can_tcp);
	return NULL;
}

__attribute__((visibility("default")))
int can_tcp_bridge_server(const char* can, int port)
{
	struct sock cansock;

	if (can) {
		if (sock_open(&cansock, SOCK_TYPE_CAN, can) < 0) {
			perror("Could not open CAN interface");
			return -1;
		}
		net_fix_sndbuf(cansock.fd);
	}

	struct mloop_socket* s = can_tcp__setup_server(port);
	if (!s)
		goto server_failure;

	struct can_tcp* can_tcp = mloop_socket_get_context(s);

	if (can) {
		if (can_tcp__add_entry(can_tcp, &cansock) == NULL) {
			perror("Could not create mloop handler for connection");
			goto entry_failure;
		}
	}

	return 0;

entry_failure:
	mloop_socket_stop(s);
server_failure:
	if (can)
		sock_close(&cansock);
	return -1;
}

__attribute__((visibility("default")))
int can_tcp_bridge_client(const char* can, const char* address, int port)
{
	struct sock cansock;

	struct can_tcp* can_tcp = can_tcp__new();
	if (!can_tcp)
		return -1;

	if (can) {
		if (sock_open(&cansock, SOCK_TYPE_CAN, can) < 0)
			goto cansock_failure;
		net_fix_sndbuf(cansock.fd);
	}

	int connfd = can_tcp_open(address, port);
	if (connfd < 0)
		goto tcpsock_failure;

	struct sock connsock = { .fd = connfd, .type = SOCK_TYPE_TCP };

	struct mloop_socket* s1;
	if (can)  {
		s1 = can_tcp__add_entry(can_tcp, &cansock);
		if (!s1)
			goto s1_failure;
	}

	struct mloop_socket* s2 = can_tcp__add_entry(can_tcp, &connsock);
	if (!s2)
		goto s2_failure;

	can_tcp__unref(can_tcp);

	return 0;

s2_failure:
	if (can)
		mloop_socket_stop(s1);
s1_failure:
	sock_close(&connsock);
tcpsock_failure:
	if (can)
		sock_close(&cansock);
cansock_failure:
	can_tcp__free(can_tcp);
	return -1;
}
