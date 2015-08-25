#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "canopen/sdo_req.h"
#include "canopen/eds.h"
#include "canopen.h"
#include "canopen/master.h"
#include "vector.h"
#include "rest.h"

struct rest_context {
	struct rest_client* client;
	const struct eds_obj* eds_obj;
};

void sdo_rest_not_found(struct rest_client* client, const char* message)
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

void sdo_rest_server_error(struct rest_client* client, const char* message)
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

void on_sdo_rest_string(struct sdo_req* req, struct rest_client* client)
{
	struct rest_reply_data reply = {
		.status_code = "200 OK",
		.content_type = "text/plain",
		.content_length = req->data.index,
		.content = req->data.data
	};

	rest_reply(client->output, &reply);
}

void on_sdo_rest_u64(struct sdo_req* req, struct rest_client* client)
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

void on_sdo_rest_s64(struct sdo_req* req, struct rest_client* client)
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

void on_sdo_rest_r64(struct sdo_req* req, struct rest_client* client)
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

void on_sdo_rest_upload_done(struct sdo_req* req)
{
	struct rest_context* context = req->context;
	assert(context);
	struct rest_client* client = context->client;
	const struct eds_obj* eds_obj = context->eds_obj;

	if (req->status != SDO_REQ_OK) {
		sdo_rest_server_error(client, sdo_strerror(req->abort_code));
		goto done;
	}

	if (eds_obj) {
		if (canopen_type_is_signed_integer(eds_obj->type))
			on_sdo_rest_s64(req, client);
		else if (canopen_type_is_unsigned_integer(eds_obj->type))
			on_sdo_rest_u64(req, client);
		else if (canopen_type_is_real(eds_obj->type))
			on_sdo_rest_r64(req, client);
		else if (canopen_type_is_string(eds_obj->type))
			on_sdo_rest_string(req, client);

	} else {
		if (req->data.index <= 8)
			on_sdo_rest_u64(req, client);
		else
			on_sdo_rest_string(req, client);
	}

	client->state = REST_CLIENT_DONE;

done:
	free(context);
	sdo_req_free(req);
}

void sdo_rest_service(struct rest_client* client, const void* content)
{
	(void)content;

	if (client->req.url_index < 4) {
		sdo_rest_not_found(client, "Wrong URL format. Must be /sdo/<nodeid>/<index>/<subindex>\r\n");
		return;
	}

	int nodeid = strtoul(client->req.url[1], NULL, 10);
	int index = strtoul(client->req.url[2], NULL, 16);
	int subindex = strtoul(client->req.url[3], NULL, 10);

	if (!(CANOPEN_NODEID_MIN <= nodeid && nodeid <= CANOPEN_NODEID_MAX)) {
		sdo_rest_not_found(client, "Invalid node id\r\n");
		return;
	}

	struct rest_context* context = malloc(sizeof(*context));
	context->client = client;

	struct co_master_node* node = co_master_get_node(nodeid);

	const struct canopen_eds* eds = eds_db_find(node->vendor_id,
						    node->product_code,
						    -1);
	if (eds)
		context->eds_obj = eds_obj_find(eds, index, subindex);

	struct sdo_req_info info = {
		.type = SDO_REQ_UPLOAD,
		.index = index,
		.subindex = subindex,
		.on_done = on_sdo_rest_upload_done,
		.context = context
	};

	struct sdo_req* req = sdo_req_new(&info);
	if (!req)
		goto req_failure;

	if (sdo_req_start(req, sdo_req_queue_get(nodeid)) < 0)
		goto req_start_failure;

	return;

req_start_failure:
	sdo_req_free(req);
req_failure:
	free(context);

	sdo_rest_server_error(client, "Failed to start sdo request\r\n");
}
