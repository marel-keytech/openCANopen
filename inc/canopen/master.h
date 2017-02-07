/* Copyright (c) 2014-2016, Marel
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef CANOPEN_MASTER_H_
#define CANOPEN_MASTER_H_

#include <stddef.h>
#include <assert.h>
#include "canopen.h"
#include "canopen-driver.h"
#include "type-macros.h"

enum co_master_driver_type {
	CO_MASTER_DRIVER_NONE = 0,
	CO_MASTER_DRIVER_LEGACY,
	CO_MASTER_DRIVER_NEW
};

typedef int (*co_drv_init_fn)(struct co_drv*);

struct co_drv {
	void* dso;
	co_drv_init_fn init_fn;

	struct sdo_req_queue* sdo_queue;

	void* context;
	co_free_fn free_fn;

	co_pdo_fn pdo1_fn, pdo2_fn, pdo3_fn, pdo4_fn;
	co_emcy_fn emcy_fn;
	enum co_options options;

	char iface[256];
};

struct co_master_node {
	enum co_master_driver_type driver_type;

	void* driver;
	void* master_iface;

	struct co_drv ndrv;

	uint32_t device_type;
	int is_heartbeat_supported;

	uint32_t vendor_id, product_code, revision_number;

	struct mloop_timer* heartbeat_timer;
	struct mloop_timer* ping_timer;

	char name[64];
	char hw_version[64];
	char sw_version[64];

	int is_loading;
	int is_initialized;

	uint32_t ntimeouts;
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

int co_master_run(void);

int co_drv_load(struct co_drv* drv, const char* name);
int co_drv_init(struct co_drv* drv);
void co_drv_unload(struct co_drv* drv);

int co__rpdox(int nodeid, int type, const void* data, size_t size);
int co__start(int nodeid);

static inline struct co_master_node* co_drv_node(const struct co_drv* drv)
{
	return container_of(drv, struct co_master_node, ndrv);
}

static inline uint16_t
co_master_get_device_profile(const struct co_master_node* node)
{
	return node->device_type & 0xffff;
}

#endif /* CANOPEN_MASTER_H_ */
