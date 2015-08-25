#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "canopen/sdo_req.h"
#include "canopen/eds.h"
#include "canopen.h"
#include "canopen/master.h"
#include "vector.h"
#include "rest.h"

#define is_in_range(x, min, max) ((min) <= (x) && (x) <= (max))

struct sdo_rest_context {
	struct rest_client* client;
	const struct eds_obj* eds_obj;
};

struct sdo_rest_path {
	int nodeid, index, subindex;
};

static inline int str_to_number(unsigned int* dst, const char* str, int base)
{
	char* end = NULL;
	*dst = strtoul(str, &end, base);
	return (*str != '\0' && *end == '\0') ? 0 : -1;
}

static int sdo_rest__convert_path(struct sdo_rest_path* dst,
				  const struct rest_client* client)
{
	dst->nodeid = strtoul(client->req.url[1], NULL, 10);
	dst->index = strtoul(client->req.url[2], NULL, 16);
	dst->subindex = strtoul(client->req.url[3], NULL, 10);

	return (is_in_range(dst->nodeid, CANOPEN_NODEID_MIN, CANOPEN_NODEID_MAX)
	     && dst->index >= 0x1000) ? 0 : -1;
}

static const struct canopen_eds* sdo_rest__find_eds(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);
	assert(node);

	const struct canopen_eds* eds;

	eds = eds_db_find(node->vendor_id, node->product_code,
			  node->revision_number);
	if (eds)
		return eds;

	return eds_db_find(node->vendor_id, node->product_code, -1);
}

static struct sdo_rest_context* sdo_rest_context_new(struct rest_client* client,
						     const struct eds_obj* obj)
{
	struct sdo_rest_context* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	self->client = client;
	self->eds_obj = obj;

	return self;
}

static void sdo_rest_not_found(struct rest_client* client, const char* message)
{
	struct rest_reply_data reply = {
		.status_code = "404 Not Found",
		.content_type = "text/plain",
		.content_length = strlen(message),
		.content = message
	};

	rest_reply(client->output, &reply);

	client->state = REST_CLIENT_DONE;
}

static void sdo_rest_server_error(struct rest_client* client,
				  const char* message)
{
	struct rest_reply_data reply = {
		.status_code = "500 Internal Server Error",
		.content_type = "text/plain",
		.content_length = strlen(message),
		.content = message
	};

	rest_reply(client->output, &reply);

	client->state = REST_CLIENT_DONE;
}

static void on_sdo_rest_string(struct sdo_req* req, struct rest_client* client)
{
	struct rest_reply_data reply = {
		.status_code = "200 OK",
		.content_type = "text/plain",
		.content_length = req->data.index,
		.content = req->data.data
	};

	rest_reply(client->output, &reply);
}

static void on_sdo_rest_u64(struct sdo_req* req, struct rest_client* client)
{
	char buffer[32];

	uint64_t number = 0;

	byteorder2(&number, req->data.data, sizeof(number), req->data.index);

	sprintf(buffer, "%llu\r\n", number);

	struct rest_reply_data reply = {
		.status_code = "200 OK",
		.content_type = "text/plain",
		.content_length = strlen(buffer),
		.content = buffer
	};

	rest_reply(client->output, &reply);
}

static void on_sdo_rest_s64(struct sdo_req* req, struct rest_client* client)
{
	char buffer[32];

	int64_t number = 0;

	byteorder2(&number, req->data.data, sizeof(number), req->data.index);

	sprintf(buffer, "%lld\r\n", number);

	struct rest_reply_data reply = {
		.status_code = "200 OK",
		.content_type = "text/plain",
		.content_length = strlen(buffer),
		.content = buffer
	};

	rest_reply(client->output, &reply);
}

static void on_sdo_rest_r64(struct sdo_req* req, struct rest_client* client)
{
	char buffer[32];

	double number = 0;

	byteorder2(&number, req->data.data, sizeof(number), req->data.index);

	sprintf(buffer, "%lf\r\n", number);

	struct rest_reply_data reply = {
		.status_code = "200 OK",
		.content_type = "text/plain",
		.content_length = strlen(buffer),
		.content = buffer
	};

	rest_reply(client->output, &reply);
}

static void on_sdo_rest_upload_done(struct sdo_req* req)
{
	struct sdo_rest_context* context = req->context;
	assert(context);
	struct rest_client* client = context->client;
	const struct eds_obj* eds_obj = context->eds_obj;

	if (req->status != SDO_REQ_OK) {
		sdo_rest_server_error(client, sdo_strerror(req->abort_code));
		goto done;
	}

	if (canopen_type_is_signed_integer(eds_obj->type))
		on_sdo_rest_s64(req, client);
	else if (canopen_type_is_unsigned_integer(eds_obj->type))
		on_sdo_rest_u64(req, client);
	else if (canopen_type_is_real(eds_obj->type))
		on_sdo_rest_r64(req, client);
	else if (canopen_type_is_string(eds_obj->type))
		on_sdo_rest_string(req, client);

	client->state = REST_CLIENT_DONE;

done:
	free(context);
	sdo_req_free(req);
}

static void sdo_rest__get(struct rest_client* client)
{
	if (client->req.url_index < 4) {
		sdo_rest_not_found(client, "Wrong URL format. Must be /sdo/<nodeid>/<index>/<subindex>\r\n");
		return;
	}

	struct sdo_rest_path path;
	if (sdo_rest__convert_path(&path, client) < 0) {
		sdo_rest_not_found(client, "URL is out of range\r\n");
		return;
	}

	const struct canopen_eds* eds = sdo_rest__find_eds(path.nodeid);
	if (!eds) {
		sdo_rest_server_error(client, "Could not find EDS for node\r\n");
		return;
	}

	const struct eds_obj* eds_obj = eds_obj_find(eds, path.index,
						     path.subindex);
	if (!eds_obj) {
		sdo_rest_not_found(client, "Index/subindex not found in EDS\r\n");
		return;
	}

	struct sdo_rest_context* context = sdo_rest_context_new(client,eds_obj);
	if (!context) {
		sdo_rest_server_error(client, "Out of memory\r\n");
		return;
	}

	struct sdo_req_info info = {
		.type = SDO_REQ_UPLOAD,
		.index = path.index,
		.subindex = path.subindex,
		.on_done = on_sdo_rest_upload_done,
		.context = context
	};

	struct sdo_req* req = sdo_req_new(&info);
	if (!req)
		goto req_failure;

	if (sdo_req_start(req, sdo_req_queue_get(path.nodeid)) < 0)
		goto req_start_failure;

	return;

req_start_failure:
	sdo_req_free(req);
req_failure:
	free(context);

	sdo_rest_server_error(client, "Failed to start sdo request\r\n");
}

static void sdo_rest__put(struct rest_client* client, const void* content)
{
}

void sdo_rest_service(struct rest_client* client, const void* content)
{
	switch (client->req.method) {
	case HTTP_GET:
		sdo_rest__get(client);
		break;
	case HTTP_PUT:
		sdo_rest__put(client, content);
		break;
	default:
		abort();
		break;
	}
}
