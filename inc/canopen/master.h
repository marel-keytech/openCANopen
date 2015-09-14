#ifndef CANOPEN_MASTER_H_
#define CANOPEN_MASTER_H_

#include <assert.h>
#include "canopen.h"

enum co_master_options_flags {
	CO_MASTER_OPTION_WITH_QUIRKS = 1
};

struct co_master_options {
	const char* iface;
	int nworkers;
	size_t worker_stack_size;
	size_t job_queue_length;
	size_t sdo_queue_length;
	int rest_port;
	size_t flags;
};

struct co_master_node {
	void* driver;
	void* master_iface;

	int device_type;
	int is_heartbeat_supported;

	uint32_t vendor_id, product_code, revision_number;

	struct mloop_timer* heartbeat_timer;
	struct mloop_timer* ping_timer;

	char name[64];

	int is_loading;
};

extern struct co_master_node co_master_node_[];

static inline int co_master_get_node_id(const struct co_master_node* node)
{
	return ((char*)node - (char*)co_master_node_)
		/ sizeof(struct co_master_node);
}

static inline struct co_master_node* co_master_get_node(int nodeid)
{
	assert(CANOPEN_NODEID_MIN <= nodeid && nodeid <= CANOPEN_NODEID_MAX);
	return &co_master_node_[nodeid];
}

int co_master_initialize(const struct co_master_options* options);

#endif /* CANOPEN_MASTER_H_ */
