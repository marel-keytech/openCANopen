#ifndef CO_NODE_H_
#define CO_NODE_H_

#include "socketcan.h"
#include "canopen.h"
#include "prioq.h"

struct co_node {
	int nodeid;
	struct prioq input_queue;
	struct prioq* output_queue;
};

int co_node_init(struct co_node* self, int id, struct prioq* output_queue);
void co_nodo_clear(struct co_node* self);

int co_node_feed(struct co_node* self, const struct can_frame* frame);

ssize_t co_node_sdo_read(struct co_node* self, int index, int subindex,
			 void* buf, size_t size);
ssize_t co_node_sdo_write(struct co_node* self, int index, int subindex,
			  const void* buf, size_t size);

#endif /* CO_NODE_H_ */

