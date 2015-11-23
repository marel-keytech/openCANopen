#ifndef CANOPEN_REST_H_
#define CANOPEN_REST_H_

#include <stdio.h>
#include <sys/queue.h>
#include "http.h"
#include "vector.h"

struct rest_reply_data {
	const char* status_code;
	const char* content_type;
	ssize_t content_length;
	const void* content;
};

enum rest_client_state {
	REST_CLIENT_START = 0,
	REST_CLIENT_CONTENT,
	REST_CLIENT_SERVICING,
	REST_CLIENT_DISCONNECTED,
	REST_CLIENT_DONE
};

struct rest_client {
	int ref;
	enum rest_client_state state;
	struct vector buffer;
	struct http_req req;
	FILE* output;
};

typedef void (*rest_fn)(struct rest_client* client, const void* content);

struct rest_service {
	SLIST_ENTRY(rest_service) links;
	enum http_method method;
	const char* path;
	rest_fn fn;
};

int rest_init(int port);
void rest_cleanup();

int rest_register_service(enum http_method method, const char* path,
			  rest_fn fn);
void rest_reply(FILE* output, struct rest_reply_data* data);
void rest_reply_header(FILE* output, struct rest_reply_data* data);

void rest_client_ref(struct rest_client* self);
int rest_client_unref(struct rest_client* self);

int rest__service_is_match(const struct rest_service* service,
			   const struct http_req* req);
struct rest_service* rest__find_service(const struct http_req* req);
void rest__init_service_list(void);

int rest__open_server(int port);
int rest__read(struct vector* buffer, int fd);

#endif /* CANOPEN_REST_H_ */
