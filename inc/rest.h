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

typedef void (*rest_fn)(FILE* output, const struct http_req* req,
			const void* content);

int rest_init();
void rest_cleanup();

int rest_register_service(enum http_method method, const char* path,
			  rest_fn fn);
void rest_reply(FILE* output, struct rest_reply_data* data);


#endif /* CANOPEN_REST_H_ */
