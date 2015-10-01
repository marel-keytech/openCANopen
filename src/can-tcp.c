#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mloop.h>

#include "socketcan.h"
#include "canopen/network.h"

size_t strlcpy(char*, const char*, size_t);

struct can_tcp__context {
	int is_can;
	int other_fd;
};

struct can_tcp__server_context {
	char iface[256];
};

static void can_tcp__forward_message(struct mloop_socket* socket)
{
	struct can_tcp__context* context = mloop_socket_get_context(socket);
	assert(context);

	int fd = mloop_socket_get_fd(socket);
	int other_fd = context->other_fd;
	int is_can = context->is_can;

	struct can_frame cf;

	if (net_read_frame(fd, &cf, 0) <= 0)
		mloop_socket_stop(socket);

	cf.can_id = is_can ? ntohl(cf.can_id) : htonl(cf.can_id);

	net_write_frame(other_fd, &cf, -1);
}

static struct mloop_socket*
can_tcp__setup_forward(int from, int to, int is_from_can)
{
	struct can_tcp__context* context;

	context = malloc(sizeof(*context));
	if (!context)
		return NULL;

	context->other_fd = to;
	context->is_can = is_from_can;

	struct mloop_socket* s = mloop_socket_new();
	if (!s)
		goto failure;

	mloop_socket_set_context(s, context, free);
	mloop_socket_set_callback(s, can_tcp__forward_message);
	mloop_socket_set_fd(s, from);

	int rc = mloop_start_socket(mloop_default(), s);
	mloop_socket_unref(s);

	return rc >= 0 ? s : NULL;

failure:
	free(context);
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

static int open_tcp_client(const char* address, int port)
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
	struct can_tcp__server_context* context;
	context = mloop_socket_get_context(socket);
	assert(context);

	int sfd = mloop_socket_get_fd(socket);
	const char* iface = context->iface;

	int connfd = accept(sfd, NULL, 0);
	if (connfd < 0) {
		perror("Could not accept connection");
		return;
	}

	int cfd = open_can(iface);
	if (cfd < 0) {
		perror("Could not open CAN interface");
		goto can_failure;
	}

	struct mloop_socket* s1 = can_tcp__setup_forward(connfd, cfd, 0);
	if (!s1) {
		perror("Could not create mloop handler for connection");
		goto s1_failure;
	}

	struct mloop_socket* s2 = can_tcp__setup_forward(cfd, connfd, 1);
	if (!s2) {
		perror("Could not create mloop handler for CAN interface");
		goto s2_failure;
	}

	return;

s2_failure:
	mloop_socket_stop(s1);
s1_failure:
	close(cfd);
can_failure:
	close(connfd);
}

__attribute__((visibility("default")))
int can_tcp_bridge_server(const char* can, int port)
{
	int sfd = open_tcp_server(port);
	if (sfd < 0)
		return -1;

	struct mloop_socket* s = mloop_socket_new();
	if (!s)
		goto handler_failure;

	struct can_tcp__server_context* context;
	context = malloc(sizeof(*context));
	if (!context)
		goto context_failure;

	strlcpy(context->iface, can, sizeof(context->iface));

	mloop_socket_set_context(s, context, free);
	mloop_socket_set_fd(s, sfd);
	mloop_socket_set_callback(s, can_tcp__on_connection);
	int rc = mloop_start_socket(mloop_default(), s);
	mloop_socket_unref(s);
	return rc;

context_failure:
	mloop_socket_unref(s);
handler_failure:
	close(sfd);
	return -1;
}

__attribute__((visibility("default")))
int can_tcp_bridge_client(const char* can, const char* address, int port)
{
	int cfd = open_can(can);
	if (cfd < 0)
		return -1;

	int connfd = open_tcp_client(address, port);
	if (connfd < 0)
		goto client_failure;

	struct mloop_socket* s1 = can_tcp__setup_forward(connfd, cfd, 0);
	if (!s1)
		goto s1_failure;

	struct mloop_socket* s2 = can_tcp__setup_forward(cfd, connfd, 1);
	if (!s2)
		goto s2_failure;

	return 0;

s2_failure:
	mloop_socket_stop(s1);
s1_failure:
	close(connfd);
client_failure:
	close(cfd);
	return -1;
}
