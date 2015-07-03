#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "socketcan.h"
#include "canopen/network.h"

static int open_can(char* iface)
{
	int fd = socketcan_open(iface);
	if (fd < 0) {
		perror("Could not open interface");
		return -1;
	}
	net_dont_block(fd);
	net_fix_sndbuf(fd);
	return fd;
}

static int open_tcp_server(int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("Could not open socket");
		return -1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (bind(fd, &addr, sizeof(addr)) < 0) {
		perror("Failed to bind to address");
		return -1;
	}

	if (listen(fd, 16) < 0) {
		perror("Failed to listen");
		return -1;
	}

	return fd;
}

static int open_tcp_client(const char* address, int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("Could not open socket");
		return -1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(address);

	if (connect(fd, &addr, sizeof(addr)) < 0) {
		perror("Failed to connect");
		return -1;
	}

	net_dont_block(fd);

	return fd;
}

static int wait_for_connection(int port)
{
	int srv = open_tcp_server(port);
	if (srv < 0)
		return -1;

	int fd = accept(srv, NULL, 0);

	if (fd >= 0)
		net_dont_block(fd);

	return fd;
}

static int forward_to_network(int src, int dst)
{
	struct can_frame rcf, tcf;

	ssize_t rsize = net_read_frame(src, &rcf, 0);
	if (rsize == 0) {
		return 0;
	} else if (rsize < 0) {
		perror("Read frame");
		return -1;
	}

	tcf.can_id = htonl(rcf.can_id);
	tcf.can_dlc = rcf.can_dlc;
	memcpy(tcf.data, rcf.data, 8);

	if (net_write_frame(dst, &tcf, -1) < 0)
		perror("Write frame");

	return rsize;
}

static int forward_to_can(int src, int dst)
{
	struct can_frame rcf, tcf;

	ssize_t rsize = net_read_frame(src, &rcf, 0);
	if (rsize == 0) {
		return 0;
	} else if (rsize < 0) {
		perror("Read frame");
		return -1;
	}

	tcf.can_id = ntohl(rcf.can_id);
	tcf.can_dlc = rcf.can_dlc;
	memcpy(tcf.data, rcf.data, 8);

	if (net_write_frame(dst, &tcf, -1) < 0)
		perror("Write frame");

	return rsize;
}


static int forward_all_messages(int network, int can)
{
	struct pollfd fds[2];

	fds[0].fd = network;
	fds[0].events = POLLIN;
	fds[1].fd = can;
	fds[1].events = POLLIN;

	while (1) {
		fds[0].revents = 0;
		fds[1].revents = 0;

		int n = poll(fds, 2, -1);
		if (n < 0) {
			perror("poll()");
			return -1;
		}

		if (fds[0].revents == POLLIN)
			if (forward_to_can(network, can) == 0)
				return -1;

		if (fds[1].revents == POLLIN)
			if (forward_to_network(can, network) == 0)
				return -1;
	}

	return 0;
}

static int server(int argc, char* argv[])
{
	char* iface = argv[1];
	int port = atoi(argv[2]);

	int can_fd = open_can(iface);
	if (can_fd < 0)
		return -1;

	int conn_fd = wait_for_connection(port);
	if (conn_fd < 0)
		return -1;

	forward_all_messages(conn_fd, can_fd);
	close(conn_fd);
	close(can_fd);

	return 0;
}

static int client(int argc, char* argv[])
{
	char* iface = argv[1];
	char* address = argv[2];
	int port = atoi(argv[3]);

	int can_fd = open_can(iface);
	if (can_fd < 0)
		return -1;

	int conn_fd = open_tcp_client(address, port);
	if (conn_fd < 0)
		return -1;

	forward_all_messages(conn_fd, can_fd);
	close(can_fd);
	close(conn_fd);

	return 0;
}

int main(int argc, char* argv[])
{
	char* what = argv[1];

	if (strcmp(what, "client") == 0)
		return client(argc-1, &argv[1]);

	if (strcmp(what, "server") == 0)
		return server(argc-1, &argv[1]);

	return 1;
}
