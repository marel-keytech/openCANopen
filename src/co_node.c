#include <unistd.h>
#include <string.h>
#include "co_node.h"
#include "worker.h"
#include "canopen/sdo_client.h"

#define MSG_QUEUE_SIZE 32
#define SDO_SEGMENT_TIMEOUT 100 /* ms */

int co_node_init(struct co_node* self, int id, struct prioq* output_queue)
{
	memset(self, 0, sizeof(*self));

	self->nodeid = id;
	self->output_queue = output_queue;

	if (prioq_init(&self->input_queue, MSG_QUEUE_SIZE) < 0)
		return -1;

	return 0;
}

static void clear_queue(struct prioq* queue)
{
	struct prioq_elem elem;
	while (prioq_pop(queue, &elem, 0) >= 0)
		free(elem.data);
	prioq_clear(queue);
}

void co_node_clear(struct co_node* self)
{
	clear_queue(&self->input_queue);
}

struct can_frame* clone_frame(const struct can_frame* src)
{
	struct can_frame* dst = malloc(sizeof(*src));
	if (!dst)
		return NULL;
	memcpy(dst, src, sizeof(*dst));
	return dst;
}

int put_frame(struct prioq* queue, const struct can_frame* frame)
{
	struct can_frame* cf = clone_frame(frame);
	if (!cf)
		return -1;

	if (prioq_insert(queue, cf->can_id, cf) < 0)
		goto failure;

	return 0;

failure:
	free(cf);
	return -1;
}

int get_frame(struct prioq* queue, struct can_frame* dst)
{
	struct prioq_elem elem;
	if (prioq_pop(queue, &elem, SDO_SEGMENT_TIMEOUT) < 0)
		return -1;

	if (elem.data == NULL)
		return -1;

	memcpy(dst, elem.data, sizeof(*dst));
	free(elem.data);

	return 0;
}

int co_node_feed(struct co_node* self, const struct can_frame* frame)
{
	/* XXX: make a split between SDO and other stuff for sdo_read/sdo_write
	 * to work synchronously
	 */
	return put_frame(&self->input_queue, frame);
}

ssize_t co_node_sdo_read(struct co_node* self, int index, int subindex,
			 void* buf, size_t size)
{
	struct sdo_ul_req req;
	struct can_frame cf;

	req.pos = 0;
	req.addr = buf;
	req.size = size;

	sdo_request_upload(&req, index, subindex);
	req.frame.can_id = R_RSDO + self->nodeid;

	if (put_frame(self->output_queue, &req.frame) < 0)
		return -1;

	while (1) {
		if (get_frame(&self->input_queue, &cf) < 0) {
			/* TODO: send abort */
			return -1;
		}

		if (sdo_ul_req_feed(&req, &cf) < 0)
			return -1;

		if (req.have_frame)
			if (put_frame(self->output_queue, &cf) < 0)
				return -1;

		if (req.state == SDO_REQ_DONE)
			return req.pos;
		if (req.state < 0)
			return -1;
	}

	return 0;
}

