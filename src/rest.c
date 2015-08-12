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
#include <sys/queue.h>

#include "canopen/network.h"
#include "http.h"
#include "vector.h"
#include "rest.h"

#define REST_PORT 9191
#define REST_BACKLOG 16

struct rest_service {
	SLIST_ENTRY(rest_service) links;
	enum http_method method;
	const char* path;
	rest_fn fn;
};

SLIST_HEAD(rest_service_list, rest_service);

static struct rest_service_list rest_service_list_;

struct rest_service* rest__find_service(const struct http_req* req)
{
	struct rest_service* service;

	SLIST_FOREACH(service, &rest_service_list_, links)
		if (req->method & service->method
		 && req->url_index > 0
		 && strcasecmp(req->url[0], service->path) == 0)
			return service;

	return NULL;
}

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
	errno = 0;
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
	if (rc == 0)
		return 0;

	if (rc < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
		return -1;

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

	memset(self, 0, sizeof(*self));

	if (vector_init(&self->buffer, 256) < 0)
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
	if (self->state > REST_CLIENT_START)
		http_req_free(&self->req);
	fclose(self->output);
	free(self);
}

static inline void rest__print_status_code(FILE* output, const char* status)
{
	fprintf(output, "HTTP/1.1 %s\r\n", status);
}

static inline void rest__print_server(FILE* output)
{
	fprintf(output, "Server: CANopen master REST service\r\n");
}

static inline void rest__print_content_type(FILE* output, const char* type)
{
	fprintf(output, "Content-Type: %s\r\n", type);
}

static inline void rest__print_content_length(FILE* output, size_t length)
{
	fprintf(output, "Content-Length: %u\r\n", length);
}

static inline void rest__print_connection_type(FILE* output)
{
	fprintf(output, "Connection: close\r\n");
}

void rest_reply(FILE* output, struct rest_reply_data* data)
{
	rest__print_status_code(output, data->status_code);
	rest__print_server(output);
	rest__print_connection_type(output);
	rest__print_content_type(output, data->content_type);
	rest__print_content_length(output, data->content_length);
	fprintf(output, "\r\n");
	fwrite(data->content, 1, data->content_length, output);
	fflush(output);
}

void rest__not_found(struct rest_client* client)
{
	const char* content = "No service is implemented for the given path.\r\n";

	struct rest_reply_data reply = {
		.status_code = "404 Not Found",
		.content_type = "text/plain",
		.content = content,
		.content_length = strlen(content),
	};

	rest_reply(client->output, &reply);

	client->state = REST_CLIENT_DONE;
}

void rest__print_index(struct rest_client* client)
{
	const char* content = "This is the CANopen master REST service.\r\n";

	struct rest_reply_data reply = {
		.status_code = "200 OK",
		.content_type = "text/plain",
		.content = content,
		.content_length = strlen(content),
	};

	rest_reply(client->output, &reply);

	client->state = REST_CLIENT_DONE;
}

static inline int rest__have_full_content(struct rest_client* client)
{
	size_t full_length = client->req.header_length
			   + client->req.content_length;

	return client->buffer.index >= full_length;
}

void rest__process_content(struct rest_client* client)
{
	if (!rest__have_full_content(client))
		return;

	const struct rest_service* service = rest__find_service(&client->req);
	if (!service) {
		rest__not_found(client);
		return;
	}

	const void* content = (char*)client->buffer.data
			    + client->req.header_length;

	client->state = REST_CLIENT_SERVICING;
	service->fn(client, content);
}

void rest__handle_get(struct rest_client* client)
{
	if (client->req.url_index == 0) {
		rest__print_index(client);
		return;
	}

	const struct rest_service* service = rest__find_service(&client->req);
	if (!service) {
		rest__not_found(client);
		return;
	}

	client->state = REST_CLIENT_SERVICING;
	service->fn(client, NULL);
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
		rest__handle_get(client);
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
	case REST_CLIENT_SERVICING:
		if (fcntl(fd, F_GETFD) < 0 && errno == EBADF)
			mloop_socket_stop(socket);
		break;
	case REST_CLIENT_DONE:
		mloop_socket_stop(socket);
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

	int nfd = dup(cfd);
	if (nfd < 0)
		goto nfd_failure;

	state->output = fdopen(nfd, "w");
	if (!state->output)
		goto fdopen_failure;

	mloop_socket_set_fd(client, cfd);
	mloop_socket_set_callback(client, rest__on_client_data);
	mloop_socket_set_context(client, state,
				 (mloop_free_fn)rest_client_free);
	mloop_start_socket(mloop_default(), client);

	mloop_socket_unref(client);
	return;

fdopen_failure:
	close(nfd);
nfd_failure:
	free(state);
state_failure:
	mloop_socket_unref(client);
socket_failure:
	close(cfd);
}

int rest_register_service(enum http_method method, const char* path, rest_fn fn)
{
	struct rest_service *service = malloc(sizeof(*service));
	if (!service)
		return -1;

	service->method = method;
	service->path = path;
	service->fn = fn;

	SLIST_INSERT_HEAD(&rest_service_list_, service, links);

	return 0;
}

int rest_init()
{
	struct mloop* mloop = mloop_default();

	SLIST_INIT(&rest_service_list_);

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
	while (!SLIST_EMPTY(&rest_service_list_)) {
		struct rest_service* service = SLIST_FIRST(&rest_service_list_);
		SLIST_REMOVE_HEAD(&rest_service_list_, links);
		free(service);
	}
}

