#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <mloop.h>

#include "canopen/network.h"
#include "http.h"
#include "vector.h"

#define REST_PORT 9191
#define REST_BACKLOG 16

enum rest_client_state {
	REST_CLIENT_START = 0,
	REST_CLIENT_CONTENT,
	REST_CLIENT_DONE

};

struct rest_client {
	enum rest_client_state state;
	struct vector buffer;
	struct http_req req;
};

int rest__open_server(int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (bind(fd, &addr, sizeof(addr)) < 0)
		goto failure;

	if (listen(fd, REST_BACKLOG) < 0)
		goto failure;

	net_dont_block(fd);
	return fd;

failure:
	close(fd);
	return -1;
}

int rest__read(struct vector* buffer, int fd)
{
	char input[256];
	ssize_t size = read(fd, &input, sizeof(input));
	if (size <= 0)
		return size;

	if (vector_append(buffer, input, size) < 0)
		return -1;

	return rest__read(buffer, fd);
}

int rest__read_head(struct vector* buffer, int fd)
{
	int rc = rest__read(buffer, fd);
	if (rc <= 0)
		return rc;

	vector_append(buffer, "", 1);
	rc = strstr(buffer->data, "\r\n\r\n") != NULL;
	buffer->index--;

	return rc;
}

static struct rest_client* rest_client_new()
{
	struct rest_client* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	if (vector_reserve(&self->buffer, 256) < 0)
		goto failure;

	return self;

failure:
	free(self);
	return NULL;
}

void rest_client_free(struct rest_client* self)
{
	if (!self) return;
	vector_destroy(&self->buffer);
	if (self->state == REST_CLIENT_CONTENT)
		http_req_free(&self->req);
	free(self);
}

void rest__process_content(struct rest_client* client)
{
	/* TODO */
}

void rest__handle_header(int fd, struct rest_client* client,
			 struct mloop_socket* socket)
{
	int rc = rest__read_head(&client->buffer, fd);
	if (rc == 0) {
		mloop_socket_stop(socket);
		return;
	}

	if (rc < 0)
		return;

	if (http_req_parse(&client->req, client->buffer.data) < 0)
		mloop_socket_stop(socket);

	switch (client->req.method) {
	case HTTP_GET:
		/* TODO */
		client->state = REST_CLIENT_DONE;
		break;
	case HTTP_PUT:
		client->state = REST_CLIENT_CONTENT;
		rest__process_content(client);
		break;
	}
}

void rest__handle_content(int fd, struct rest_client* client,
			  struct mloop_socket* socket)
{
	int rc = rest__read(&client->buffer, fd);
	if (rc == 0) {
		mloop_socket_stop(socket);
		return;
	}

	if (rc > 0)
		rest__process_content(client);
}

static void rest__on_client_data(struct mloop_socket* socket)
{
	struct rest_client* client = mloop_socket_get_context(socket);
	int fd = mloop_socket_get_fd(socket);

	switch (client->state) {
	case REST_CLIENT_START:
		rest__handle_header(fd, client, socket);
		break;
	case REST_CLIENT_CONTENT:
		rest__handle_content(fd, client, socket);
		break;
	case REST_CLIENT_DONE:
		break;
	default:
		abort();
	}
}

static void rest__on_connection(struct mloop_socket* socket)
{
	int sfd = mloop_socket_get_fd(socket);

	int cfd = accept(sfd, NULL, 0);
	if (cfd < 0)
		return;

	net_dont_block(cfd);
	struct mloop_socket* client = mloop_socket_new();
	if (!client)
		goto socket_failure;

	struct rest_client* state = rest_client_new();
	if (!state)
		goto state_failure;

	mloop_socket_set_fd(client, cfd);
	mloop_socket_set_callback(client, rest__on_client_data);
	mloop_socket_set_context(client, state,
				 (mloop_free_fn)rest_client_free);
	mloop_start_socket(mloop_default(), client);

	mloop_socket_unref(client);
	return;

state_failure:
	mloop_socket_unref(client);
socket_failure:
	close(cfd);
}

int rest_init()
{
	struct mloop* mloop = mloop_default();

	int lfd = rest__open_server(REST_PORT);
	if (lfd < 0)
		return -1;

	struct mloop_socket* socket = mloop_socket_new();
	if (!socket)
		goto socket_failure;

	mloop_socket_set_fd(socket, lfd);
	mloop_socket_set_callback(socket, rest__on_connection);
	if (mloop_start_socket(mloop, socket) < 0)
		goto start_failure;

	mloop_socket_unref(socket);
	return 0;

start_failure:
	mloop_socket_unref(socket);
socket_failure:
	close(lfd);
	return -1;
}

void rest_cleanup()
{
}

