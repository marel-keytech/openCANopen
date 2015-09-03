#ifndef CANOPEN_HTTP_H_
#define CANOPEN_HTTP_H_

#define URL_INDEX_MAX 32
#define URL_QUERY_INDEX_MAX 32

#include <stddef.h>

enum http_method {
	HTTP_GET = 1,
	HTTP_PUT = 2,
};

struct http_url_query {
	char* key;
	char* value;
};

struct http_req {
	enum http_method method;
	size_t header_length;
	size_t content_length;
	char* content_type;
	size_t url_index;
	char* url[URL_INDEX_MAX];
	size_t url_query_index;
	struct http_url_query url_query[URL_QUERY_INDEX_MAX];
};

int http_req_parse(struct http_req* req, const char* head);
void http_req_free(struct http_req* req);

#endif /* CANOPEN_HTTP_H_ */
