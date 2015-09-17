#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include "socketcan.h"
#include "canopen/master.h"
#include "canopen/sdo_req.h"
#include "canopen/emcy.h"
#include "canopen-driver.h"

struct co_drv {
	struct co_master_node* node;
	struct sdo_req_queue* sdo_queue;

	void* context;
	co_free_fn free_fn;

	co_pdo_fn pdo1_fn, pdo2_fn, pdo3_fn, pdo4_fn;
	co_emcy_fn emcy_fn;

	char iface[256];
};

struct co_sdo_req {
	struct sdo_req req;
	struct co_drv* drv;
	co_sdo_done_fn on_done;
};

#pragma GCC visibility push(default)

int co_get_nodeid(const struct co_drv* self)
{
	return co_master_get_node_id(self->node);
}

uint32_t co_get_vendor_id(const struct co_drv* self)
{
	return self->node->vendor_id;
}

uint32_t co_get_product_code(const struct co_drv* self)
{
	return self->node->product_code;
}

uint32_t co_get_revision_number(const struct co_drv* self)
{
	return self->node->revision_number;
}

const char* co_get_name(const struct co_drv* self)
{
	return self->node->name;
}

void co_set_context(struct co_drv* self, void* context, co_free_fn fn)
{
	self->context = context;
	self->free_fn = fn;
}

void* co_get_context(struct co_drv* self)
{
	return self->context;
}

void co_set_pdo1_fn(struct co_drv* self, co_pdo_fn fn)
{
	self->pdo1_fn = fn;
}

void co_set_pdo2_fn(struct co_drv* self, co_pdo_fn fn)
{
	self->pdo2_fn = fn;
}

void co_set_pdo3_fn(struct co_drv* self, co_pdo_fn fn)
{
	self->pdo3_fn = fn;
}

void co_set_pdo4_fn(struct co_drv* self, co_pdo_fn fn)
{
	self->pdo4_fn = fn;
}

void co_set_emcy_fn(struct co_drv* self, co_emcy_fn fn)
{
	self->emcy_fn = fn;
}

const char* co_get_network_name(const struct co_drv* self)
{
	return self->iface;
}

void co_sdo_req_ref(struct co_sdo_req* self)
{
	sdo_req_ref(&self->req);
}

int co_sdo_req_unref(struct co_sdo_req* self)
{
	return sdo_req_unref(&self->req);
}

static void co__sdo_req_on_done(struct sdo_req* req)
{
	struct co_sdo_req* self = (void*)req;

	co_sdo_done_fn on_done = self->on_done;
	if (on_done)
		on_done(self->drv, self);

	co_sdo_req_unref(self);
}

struct co_sdo_req* co_sdo_req_new(struct co_drv* drv)
{
	struct co_sdo_req* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	struct sdo_req* req = &self->req;

	req->ref = 1;
	req->on_done = co__sdo_req_on_done;
	self->drv = drv;

	return self;
}

void co_sdo_req_set_indices(struct co_sdo_req* self, int index, int subindex)
{
	self->req.index = index;
	self->req.subindex = subindex;
}

void co_sdo_req_set_type(struct co_sdo_req* self, enum co_sdo_type type)
{
	switch (type) {
	case CO_SDO_DOWNLOAD:
		self->req.type = SDO_REQ_DOWNLOAD;
		break;
	case CO_SDO_UPLOAD:
		self->req.type = SDO_REQ_UPLOAD;
		break;
	default:
		abort();
	}
}

void co_sdo_req_set_data(struct co_sdo_req* self, const void* data, size_t size)
{
	vector_assign(&self->req.data, data, size);
}

void co_sdo_req_set_done_fn(struct co_sdo_req* self, co_sdo_done_fn fn)
{
	self->on_done = fn;
}

int co_sdo_req_start(struct co_sdo_req* self)
{
	int nodeid = co_get_nodeid(self->drv);
	return sdo_req_start(&self->req, sdo_req_queue_get(nodeid));
}

#pragma GCC visibility pop
