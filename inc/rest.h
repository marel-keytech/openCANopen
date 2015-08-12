#ifndef CANOPEN_REST_H_
#define CANOPEN_REST_H_

#include <stdio.h>
#include "http.h"

struct rest_reply_data {
	const char* status_code;
	const char* content_type;
	size_t content_length;
	const void* content;
};

enum rest_client_state {
	REST_CLIENT_START = 0,
	REST_CLIENT_CONTENT,
	REST_CLIENT_SERVICING,
	REST_CLIENT_DONE
};

struct rest_client {
	enum rest_client_state state;
	struct vector buffer;
	struct http_req req;
	FILE* output;
};

typedef void (*rest_fn)(struct rest_client* client, const void* content);

int rest_init();
void rest_cleanup();

int rest_register_service(enum http_method method, const char* path,
			  rest_fn fn);
void rest_reply(FILE* output, struct rest_reply_data* data);


#endif /* CANOPEN_REST_H_ */
