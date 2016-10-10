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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include "plog.h"

#include "mloop.h"
#include "socketcan.h"
#include "canopen.h"
#include "canopen/sdo.h"
#include "canopen/sdo_req.h"
#include "canopen/network.h"
#include "canopen/nmt.h"
#include "canopen/heartbeat.h"
#include "canopen/emcy.h"
#include "canopen/eds.h"
#include "canopen/master.h"
#include "canopen/sdo_sync.h"
#include "rest.h"
#include "sdo-rest.h"
#include "time-utils.h"
#include "profiling.h"
#include "string-utils.h"
#include "net-util.h"
#include "sock.h"
#include "cfg.h"

#ifndef NO_MAREL_CODE
#include <appcbase.h>
#include "legacy-driver.h"
#include "canopen_info.h"
#endif /* NO_MAREL_CODE */

#ifndef CAN_MAX_DLEN
#define CAN_MAX_DLEN 8
#endif

#define __unused __attribute__((unused))

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define for_each_node(index) \
	for(index = nodeid_min(); index <= nodeid_max(); ++index)

#define for_each_node_reverse(index) \
	for(index = nodeid_max(); index >= nodeid_min(); --index)

size_t strlcpy(char* dst, const char* src, size_t dsize);

static struct sock socket_ = { .fd = -1 };
static char nodes_seen_[CANOPEN_NODEID_MAX + 1];
static char nodes_seen_late_[CANOPEN_NODEID_MAX + 1];
/* Note: nodes_seen_[0] is unused */

static unsigned int n_scheduled_bootups = 0;

enum master_state {
	MASTER_STATE_STARTUP = 0,
	MASTER_STATE_RUNNING,
	MASTER_STATE_STOPPING,
};

static enum master_state master_state_ = MASTER_STATE_STARTUP;

static void* driver_manager_;
pthread_mutex_t driver_manager_lock_ = PTHREAD_MUTEX_INITIALIZER;

static struct mloop* mloop_ = NULL;
static struct mloop_socket* mux_handler_ = NULL;

static void* master_iface_init(int nodeid);
static int master_request_sdo(int nodeid, int index, int subindex);
static int master_send_sdo(int nodeid, int index, int subindex,
			   unsigned char* data, size_t size);
static int master_send_pdo(int nodeid, int n, unsigned char* data, size_t size);
static void unload_legacy_module(int device_type, void* driver);
static void on_bootup_done(struct mloop_work* self);

struct co_master_node co_master_node_[CANOPEN_NODEID_MAX + 1];
/* Note: node_[0] is unused */

static inline int nodeid_min(void)
{
	return cfg.range_start == 0 ? CANOPEN_NODEID_MIN : cfg.range_start;
}

static inline int nodeid_max(void)
{
	return cfg.range_stop == 0 ? CANOPEN_NODEID_MAX : cfg.range_stop;
}

static inline uint32_t get_device_type(int nodeid)
{
	return sdo_sync_read_u32(nodeid, 0x1000, 0);
}

static inline int node_has_identity(int nodeid)
{
	return !!sdo_sync_read_u32(nodeid, 0x1018, 0);
}

static inline uint32_t get_vendor_id(int nodeid)
{
	return sdo_sync_read_u32(nodeid, 0x1018, 1);
}

static inline uint32_t get_product_code(int nodeid)
{
	return sdo_sync_read_u32(nodeid, 0x1018, 2);
}

static inline uint32_t get_revision_number(int nodeid)
{
	return sdo_sync_read_u32(nodeid, 0x1018, 3);
}

static inline int set_heartbeat_period(int nodeid, uint16_t period)
{
	struct sdo_req_info info = { .index = 0x1017, .subindex = 0 };
	return sdo_sync_write_u16(nodeid, &info, period);
}

static char* get_string(int nodeid, int index, int subindex)
{
	static __thread char buffer[256];

	struct sdo_req* req = sdo_sync_read(nodeid, index, subindex);
	if (!req)
		return NULL;

	memcpy(buffer, req->data.data, MIN(req->data.index, sizeof(buffer)));
	buffer[MIN(req->data.index, sizeof(buffer) - 1)] = '\0';

	sdo_req_unref(req);
	return buffer;
}

static void stop_heartbeat_timer(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);
	struct mloop_timer* timer = node->heartbeat_timer;
	mloop_timer_stop(timer);
}

static void stop_ping_timer(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);
	struct mloop_timer* timer = node->ping_timer;
	mloop_timer_stop(timer);
}

#ifndef NO_MAREL_CODE
static void unload_legacy_driver(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);

	unload_legacy_module(node->device_type, node->driver);

	legacy_master_iface_delete(node->master_iface);

	node->driver = NULL;
	node->master_iface = NULL;
}
#endif /* NO_MAREL_CODE */

/* This is an expediated fire-and-forget SDO message to tell buggy Marel
 * CANopen nodes that don't have a driver to shut up.
 *
 * The sdo_req interface is not used for this because we're not interested in
 * the response and it would require more complicated code.
 */
static void turn_off_heartbeat(int nodeid)
{
	struct can_frame cf = { .can_id = R_RSDO + nodeid };
	sdo_set_cs(&cf, SDO_CCS_DL_INIT_REQ);
	sdo_set_index(&cf, 0x1017);
	sdo_set_subindex(&cf, 0);
	sdo_indicate_size(&cf);
	sdo_expediate(&cf);
	sdo_set_expediated_size(&cf, sizeof(uint16_t));
	cf.can_dlc = SDO_EXPEDIATED_DATA_IDX + sizeof(uint16_t);
	sock_send(&socket_, &cf, 0);
}

static void unload_driver(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);

	stop_heartbeat_timer(nodeid);

	if (!node->is_heartbeat_supported)
		stop_ping_timer(nodeid);
	else
		turn_off_heartbeat(nodeid);

	sdo_req_queue_flush(sdo_req_queue_get(nodeid));

	switch (node->driver_type) {
#ifndef NO_MAREL_CODE
	case CO_MASTER_DRIVER_LEGACY:
		unload_legacy_driver(nodeid);
		break;
#endif /* NO_MAREL_CODE */
	case CO_MASTER_DRIVER_NEW:
		co_drv_unload(&node->ndrv);
		break;
	case CO_MASTER_DRIVER_NONE:
	default:
		abort();
		break;
	}

	node->device_type = 0;
	node->is_heartbeat_supported = 0;
	node->driver_type = CO_MASTER_DRIVER_NONE;

	if (master_state_ == MASTER_STATE_STOPPING)
		co_net_send_nmt(&socket_, NMT_CS_STOP, nodeid);
	else
		co_net_send_nmt(&socket_, NMT_CS_RESET_NODE, nodeid);

#ifndef NO_MAREL_CODE
	struct canopen_info* info = canopen_info_get(nodeid);
	info->is_active = 0;
#endif /* NO_MAREL_CODE */
}

static void on_heartbeat_timeout(struct mloop_timer* timer)
{
	struct co_master_node* node = mloop_timer_get_context(timer);
	int nodeid = co_master_get_node_id(node);

	node->ntimeouts++;

	plog(LOG_DEBUG, "Node \"%s\" with id %d has missed %u heartbeats",
	     node->name, nodeid, node->ntimeouts);

	if (node->ntimeouts <= cfg.node[nodeid].n_timeouts_max)
		return;

	plog(LOG_NOTICE, "Node \"%s\" with id %d has timed out; unloading...",
	     node->name, nodeid);

	unload_driver(co_master_get_node_id(node));
}

static int start_heartbeat_timer(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);
	struct mloop_timer* timer = node->heartbeat_timer;
	node->ntimeouts = 0;
	return mloop_timer_start(timer);
}

static int restart_heartbeat_timer(int nodeid)
{
	stop_heartbeat_timer(nodeid);
	return start_heartbeat_timer(nodeid);
}

static void on_ping_timeout(struct mloop_timer* timer)
{
	struct co_master_node* node = mloop_timer_get_context(timer);
	int nodeid = co_master_get_node_id(node);

	struct can_frame cf = { 0 };

	cf.can_id = R_HEARTBEAT + nodeid;
	cf.can_id |= CAN_RTR_FLAG;
	cf.can_dlc = 1;
	heartbeat_set_state(&cf, 1);

	sock_send(&socket_, &cf, 0);
}

static int start_ping_timer(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);
	struct mloop_timer* timer = node->ping_timer;
	return mloop_timer_start(timer);
}

static void start_nodeguarding(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);

	if (!node->is_heartbeat_supported)
		start_ping_timer(nodeid);

	start_heartbeat_timer(nodeid);
}

#ifndef NO_MAREL_CODE
static void* load_legacy_module(const char* name, int device_type,
				void* master_iface)
{
	void* driver = NULL;
	pthread_mutex_lock(&driver_manager_lock_);
	int rc = legacy_driver_manager_create_handler(driver_manager_, name,
						      device_type, master_iface,
						      &driver);
	pthread_mutex_unlock(&driver_manager_lock_);
	return rc >= 0 ? driver : NULL;
}

static void unload_legacy_module(int device_type, void* driver)
{
	pthread_mutex_lock(&driver_manager_lock_);
	legacy_driver_delete_handler(driver_manager_, device_type, driver);
	pthread_mutex_unlock(&driver_manager_lock_);
}
#endif /* NO_MAREL_CODE */

static int load_new_driver(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);

	if (co_drv_load(&node->ndrv, node->name) < 0)
		return -1;

	node->driver_type = CO_MASTER_DRIVER_NEW;

	return 0;
}

#ifndef NO_MAREL_CODE
static int load_legacy_driver(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);

	void* master_iface = master_iface_init(nodeid);
	if (!master_iface)
		return -1;

	void* driver = load_legacy_module(node->name, node->device_type,
					  master_iface);
	if (!driver)
		goto failure;

	node->driver_type = CO_MASTER_DRIVER_LEGACY;
	node->driver = driver;
	node->master_iface = master_iface;

	return 0;

failure:
	legacy_master_iface_delete(master_iface);
	node->master_iface = NULL;
	return -1;
}
#endif /* NO_MAREL_CODE */

static int is_nodename_char(int c)
{
	return isalnum(c) || c == '-' || c == '_';
}

#ifndef NO_MAREL_CODE
static void initialize_info_structure(int nodeid)
{
	struct canopen_info* info = canopen_info_get(nodeid);
	struct co_master_node* node = co_master_get_node(nodeid);

	info->device_type = node->device_type;
	strlcpy(info->name, node->name, sizeof(info->name));
	strlcpy(info->hw_version, node->hw_version, sizeof(info->hw_version));
	strlcpy(info->sw_version, node->sw_version, sizeof(info->sw_version));
}

static void load_error_register(int nodeid)
{
	struct canopen_info* info = canopen_info_get(nodeid);

	errno = 0;
	info->error_register = sdo_sync_read_u8(nodeid, 0x1001, 0);
	if (info->error_register == 0 && errno != 0) {
		plog(LOG_WARNING, "load_driver: Could not get/convert error register for node %d",
		     nodeid);
	}
}
#endif /* NO_MAREL_CODE */

static const char* driver_type_str(enum co_master_driver_type type)
{
	switch (type) {
	case CO_MASTER_DRIVER_NONE: return "none";
	case CO_MASTER_DRIVER_NEW: return "driver";
	case CO_MASTER_DRIVER_LEGACY: return "legacy driver";
	}

	return "UNKNOWN";
}

static void apply_quirks(struct co_master_node* node)
{
	int nodeid = co_master_get_node_id(node);

	if (cfg.node[nodeid].has_zero_guard_status)
		node->quirks |= CO_NODE_QUIRK_ZERO_GUARD_STATUS;
	else
		node->quirks &= ~CO_NODE_QUIRK_ZERO_GUARD_STATUS;

	struct sdo_req_queue* sdo_queue = sdo_req_queue_get(nodeid);
	struct sdo_async* sdo_client = &sdo_queue->sdo_client;

	if (cfg.node[nodeid].ignore_sdo_multiplexer)
		sdo_client->quirks |= SDO_ASYNC_QUIRK_IGNORE_MULTIPLEXER;
	else
		sdo_client->quirks &= ~SDO_ASYNC_QUIRK_IGNORE_MULTIPLEXER;

	if (cfg.node[nodeid].send_full_sdo_frame)
		sdo_client->quirks |= SDO_ASYNC_QUIRK_NEEDS_FULL_FRAME;
	else
		sdo_client->quirks &= ~SDO_ASYNC_QUIRK_NEEDS_FULL_FRAME;


}

static int load_any_driver(int nodeid)
{
	if (load_new_driver(nodeid) >= 0)
		return 0;

#ifndef NO_MAREL_CODE
	if (load_legacy_driver(nodeid) >= 0)
		return 0;
#endif /* NO_MAREL_CODE */

	return -1;
}

static int load_driver(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);
	if (node->driver_type != CO_MASTER_DRIVER_NONE) {
		plog(LOG_ERROR, "load_driver: A driver is already loaded for node %d",
		     nodeid);
		return -1;
	}

	errno = 0;
	node->device_type = get_device_type(nodeid);
	if (node->device_type == 0 && errno != 0) {
		plog(LOG_WARNING, "load_driver: Could not get/convert device type for node %d",
		     nodeid);
		return -1;
	}

	char* name = get_string(nodeid, 0x1008, 0);
	if (!name) {
		plog(LOG_WARNING, "load_driver: Could not get name of node %d",
		     nodeid);
		return -1;
	}

	string_keep_if(is_nodename_char, name);
	strlcpy(node->name, name, sizeof(node->name));

	if (node_has_identity(nodeid)) {
		node->vendor_id = get_vendor_id(nodeid);
		node->product_code = get_product_code(nodeid);
		node->revision_number = get_revision_number(nodeid);
	}

	cfg_load_node(nodeid);

	node->is_heartbeat_supported =
		set_heartbeat_period(nodeid, cfg.node[nodeid].heartbeat_period) >= 0;

	char* hw_version = get_string(nodeid, 0x1009, 0);
	if (!hw_version)
		hw_version = "";

	strlcpy(node->hw_version, string_trim(hw_version),
		sizeof(node->hw_version));

	char* sw_version = get_string(nodeid, 0x100A, 0);
	if (!sw_version)
		sw_version = "";

	strlcpy(node->sw_version, string_trim(sw_version),
		sizeof(node->sw_version));

#ifndef NO_MAREL_CODE
	initialize_info_structure(nodeid);

	load_error_register(nodeid);
#endif /* NO_MAREL_CODE */

	apply_quirks(node);

	if (load_any_driver(nodeid) < 0) {
		if (node->is_heartbeat_supported)
			turn_off_heartbeat(nodeid);

		co_net_send_nmt(&socket_, NMT_CS_STOP, nodeid);
		plog(LOG_NOTICE, "load_driver: There is no driver available for \"%s\" at id %d",
		     node->name, nodeid);
		return -1;
	}

	plog(LOG_DEBUG, "load_driver: Successfully loaded %s for \"%s\" at id %d",
	     driver_type_str(node->driver_type), node->name, nodeid);

	return 0;

}

#ifndef NO_MAREL_CODE
static int initialize_legacy_driver(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);
	struct canopen_info* info = canopen_info_get(nodeid);

	int rc = legacy_driver_iface_initialize(node->driver);
	if (rc >= 0) {
		legacy_driver_iface_process_node_state(node->driver, 1);
		info->is_active = 1;
		info->last_seen = time(NULL);
	} else {
		if (node->is_heartbeat_supported)
			turn_off_heartbeat(nodeid);

		co_net_send_nmt(&socket_, NMT_CS_STOP, nodeid);

		plog(LOG_ERROR, "initialize_legacy_driver: Failed to initialize \"%s\" with id %d",
		     node->name, nodeid);

		unload_legacy_module(node->device_type, node->driver);
		legacy_master_iface_delete(node->master_iface);
		node->driver = NULL;
		node->master_iface = NULL;
		node->driver_type = CO_MASTER_DRIVER_NONE;
	}

	return rc;
}
#endif /* NO_MAREL_CODE */

static int initialize_new_driver(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);
	int rc = co_drv_init(&node->ndrv);
	if (rc >= 0) {
#ifndef NO_MAREL_CODE
		struct canopen_info* info = canopen_info_get(nodeid);
		info->is_active = 1;
		info->last_seen = time(NULL);
#endif /* NO_MAREL_CODE */
	} else {
		if (node->is_heartbeat_supported)
			turn_off_heartbeat(nodeid);

		co_net_send_nmt(&socket_, NMT_CS_STOP, nodeid);

		plog(LOG_ERROR, "initialize_new_driver: Failed to initialize \"%s\" with id %d",
		     node->name, nodeid);

		co_drv_unload(&node->ndrv);
		node->driver_type = CO_MASTER_DRIVER_NONE;
	}

	return rc;
}

static int initialize_driver(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);

	switch (node->driver_type) {
	case CO_MASTER_DRIVER_NEW:
		return initialize_new_driver(nodeid);
#ifndef NO_MAREL_CODE
	case CO_MASTER_DRIVER_LEGACY:
		return initialize_legacy_driver(nodeid);
#endif /* NO_MAREL_CODE */
	case CO_MASTER_DRIVER_NONE:
		return -1;
	default:
		abort();
		break;
	}
}

static void run_net_probe(struct mloop_work* self)
{
	(void)self;

	profile("Probe network...\n");

	int start = CANOPEN_NODEID_MIN, stop = CANOPEN_NODEID_MAX;

	if (cfg.range_start == 0 && cfg.range_stop == 0) {
		co_net_reset(&socket_, nodes_seen_, 100);
	} else  {
		start = cfg.range_start;
		stop = cfg.range_stop;
		co_net_reset_range(&socket_, nodes_seen_, start, stop,
				   100 * (start - stop + 1));
	}

	co_net_probe(&socket_, nodes_seen_, start, stop, 100);
}

static void run_load_driver(struct mloop_work* self)
{
	struct co_master_node* node = mloop_work_get_context(self);
	int nodeid = co_master_get_node_id(node);
	load_driver(nodeid);
	node->is_loading = 0;
}

static void on_load_driver_done(struct mloop_work* self)
{
	struct co_master_node* node = mloop_work_get_context(self);
	int nodeid = co_master_get_node_id(node);

	--n_scheduled_bootups;

	if (node->driver_type == CO_MASTER_DRIVER_NONE)
		return;

	if (initialize_driver(nodeid) < 0)
		return;

	if (master_state_ == MASTER_STATE_STARTUP)
		return;

	co_net_send_nmt(&socket_, NMT_CS_START, nodeid);
	start_nodeguarding(nodeid);
}

static int schedule_load_driver(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);
	if (node->is_loading)
		return 0;

	struct mloop_work* work = mloop_work_new(mloop_default());
	if (!work)
		return -1;

	if (node->driver_type != CO_MASTER_DRIVER_NONE)
		unload_driver(nodeid);

	mloop_work_set_context(work, node, NULL);
	mloop_work_set_work_fn(work, run_load_driver);
	mloop_work_set_done_fn(work, on_load_driver_done);

	int rc = mloop_work_start(work);
	if (rc >= 0)
		++n_scheduled_bootups;

	mloop_work_unref(work);

	node->is_loading = 1;

	return rc;
}

static int handle_bootup(struct co_master_node* node)
{
	int nodeid = co_master_get_node_id(node);

	if (master_state_ == MASTER_STATE_STARTUP) {
		nodes_seen_late_[nodeid] = 1;
		return 0;
	}

	return schedule_load_driver(nodeid);
}

static int handle_emcy(struct co_master_node* node,
		       const struct can_frame* frame)
{
	if (frame->can_dlc == 0)
		return handle_bootup(node);

	if (frame->can_dlc != 8)
		return -1;

	uint32_t error_register = emcy_get_register(frame);

	int nodeid = co_master_get_node_id(node);

#ifndef NO_MAREL_CODE
	canopen_info_get(nodeid)->error_register = error_register;
#endif /* NO_MAREL_CODE */

	struct co_emcy emcy = {
		.code = emcy_get_code(frame),
		.reg = error_register,
		.manufacturer_error = emcy_get_manufacturer_error(frame)
	};

	switch (node->driver_type) {
	case CO_MASTER_DRIVER_NONE:
		return -1;
#ifndef NO_MAREL_CODE
	case CO_MASTER_DRIVER_LEGACY:
		legacy_driver_iface_process_emr(node->driver, emcy.code,
						emcy.reg,
						emcy.manufacturer_error);
		break;
#endif /* NO_MAREL_CODE */
	case CO_MASTER_DRIVER_NEW:
		if (node->ndrv.emcy_fn)
			node->ndrv.emcy_fn(&node->ndrv, &emcy);
		break;
	}

	return 0;
}

static int handle_heartbeat(struct co_master_node* node,
			     const struct can_frame* frame)
{
	if (!heartbeat_is_valid(frame))
		return -1;

	if (heartbeat_is_bootup(frame)
	 && !(node->quirks & CO_NODE_QUIRK_ZERO_GUARD_STATUS))
		return handle_bootup(node);

	if (master_state_ == MASTER_STATE_STARTUP)
		return 0;

	int nodeid = co_master_get_node_id(node);

	/* This can happen if the CAN bus is disconnected but not the power to
	 * the node. We reset communication to refresh the state.
	 */
	if (node->driver_type == CO_MASTER_DRIVER_NONE) {
		if (heartbeat_get_state(frame) != NMT_STATE_STOPPED)
			co_net_send_nmt(&socket_, NMT_CS_RESET_COMMUNICATION,
					nodeid);

		return 0;
	}

	restart_heartbeat_timer(nodeid);

	/* Make sure the node is in operational state */
	if (heartbeat_get_state(frame) != NMT_STATE_OPERATIONAL)
		co_net_send_nmt(&socket_, NMT_CS_START, nodeid);

#ifndef NO_MAREL_CODE
	struct canopen_info* info = canopen_info_get(nodeid);
	info->last_seen = time(NULL);
#endif /* NO_MAREL_CODE */

	return 0;
}

static int handle_sdo(struct co_master_node* node, const struct can_frame* cf)
{
	int nodeid = co_master_get_node_id(node);
	struct sdo_async* sdo_proc = &sdo_req_queue_get(nodeid)->sdo_client;
	return sdo_async_feed(sdo_proc, cf);
}

static int handle_not_loaded(struct co_master_node* node,
			     const struct canopen_msg* msg,
			     const struct can_frame* frame)
{
	switch (msg->object)
	{
	case CANOPEN_EMCY:
		return handle_emcy(node, frame);
	case CANOPEN_HEARTBEAT:
		return handle_heartbeat(node, frame);
	case CANOPEN_TSDO:
		return handle_sdo(node, frame);
	default:
		break;
	}

	return -1;
}

#ifndef NO_MAREL_CODE
static int handle_with_legacy(struct co_master_node* node,
			      const struct canopen_msg* msg,
			      const struct can_frame* cf)
{
	void* driver = node->driver;
	if (!driver)
		return -1;

	switch (msg->object)
	{
	case CANOPEN_TPDO1:
		return legacy_driver_iface_process_pdo(driver, 1, cf->data,
						       cf->can_dlc);
	case CANOPEN_TPDO2:
		return legacy_driver_iface_process_pdo(driver, 2, cf->data,
						       cf->can_dlc);
	case CANOPEN_TPDO3:
		return legacy_driver_iface_process_pdo(driver, 3, cf->data,
						       cf->can_dlc);
	case CANOPEN_TPDO4:
		return legacy_driver_iface_process_pdo(driver, 4, cf->data,
						       cf->can_dlc);
	case CANOPEN_TSDO:
		return handle_sdo(node, cf);
	case CANOPEN_EMCY:
		return handle_emcy(node, cf);
	case CANOPEN_HEARTBEAT:
		return handle_heartbeat(node, cf);
	default:
		break;
	}

	return -1;
}
#endif /* NO_MAREL_CODE */

static int handle_with_new_driver(struct co_master_node* node,
				  const struct canopen_msg* msg,
				  const struct can_frame* cf)
{
	struct co_drv* drv = &node->ndrv;

	switch (msg->object)
	{
	case CANOPEN_TPDO1:
		if (drv->pdo1_fn)
			drv->pdo1_fn(drv, cf->data, cf->can_dlc);
		return 0;
	case CANOPEN_TPDO2:
		if (drv->pdo2_fn)
			drv->pdo2_fn(drv, cf->data, cf->can_dlc);
		return 0;
	case CANOPEN_TPDO3:
		if (drv->pdo3_fn)
			drv->pdo3_fn(drv, cf->data, cf->can_dlc);
		return 0;
	case CANOPEN_TPDO4:
		if (drv->pdo4_fn)
			drv->pdo4_fn(drv, cf->data, cf->can_dlc);
		return 0;
	case CANOPEN_TSDO:
		return handle_sdo(node, cf);
	case CANOPEN_EMCY:
		return handle_emcy(node, cf);
	case CANOPEN_HEARTBEAT:
		return handle_heartbeat(node, cf);
	default:
		break;
	}

	return -1;
}

static void mux_on_frame(const struct can_frame* cf)
{
	struct canopen_msg msg;

	if (canopen_get_object_type(&msg, cf) < 0)
		return;

	if (msg.object == CANOPEN_NMT) {
		plog(LOG_ALERT, "Received NMT! Another CANopen master is not allowed on the bus!");
		return;
	}

	if (!(nodeid_min() <= msg.id && msg.id <= nodeid_max()))
		return;

	struct co_master_node* node = co_master_get_node(msg.id);

	switch (node->driver_type) {
	case CO_MASTER_DRIVER_NONE:
		handle_not_loaded(node, &msg, cf);
		break;
#ifndef NO_MAREL_CODE
	case CO_MASTER_DRIVER_LEGACY:
		handle_with_legacy(node, &msg, cf);
		break;
#endif /* NO_MAREL_CODE */
	case CO_MASTER_DRIVER_NEW:
		handle_with_new_driver(node, &msg, cf);
		break;
	}
}

static void mux_handler_fn(struct mloop_socket* self)
{
	struct can_frame cf;

	while (1) {
		ssize_t rsize = sock_recv(&socket_, &cf, MSG_DONTWAIT);
		if (rsize == 0)
			mloop_socket_stop(self);

		if (rsize <= 0)
			return;

		mux_on_frame(&cf);
	}
}

static int init_multiplexer()
{
	mux_handler_ = mloop_socket_new(mloop_default());
	if (!mux_handler_)
		return -1;

	mloop_socket_set_fd(mux_handler_, socket_.fd);
	mloop_socket_set_callback(mux_handler_, mux_handler_fn);

	return mloop_socket_start(mux_handler_);
}

static void wait_for_bootup(struct mloop_work* self)
{
	(void)self;
	while (n_scheduled_bootups > 0)
		usleep(100);
}

static void schedule_bootup_done(void)
{
	struct mloop_work* work = mloop_work_new(mloop_default());
	assert(work);
	mloop_work_set_work_fn(work, wait_for_bootup);
	mloop_work_set_done_fn(work, on_bootup_done);

	int rc = mloop_work_start(work);
	assert(rc == 0);

	mloop_work_unref(work);
}


static void run_bootup(void)
{
	int i;
	profile("Load drivers...\n");
	for_each_node(i)
		if (nodes_seen_[i])
			schedule_load_driver(i);

	schedule_bootup_done();
}

static void load_late_nodes(void)
{
	int i;
	for_each_node(i)
		if (nodes_seen_late_[i]) {
			plog(LOG_WARNING, "Node %d was late", i);
			schedule_load_driver(i);
		}
}

static void on_bootup_done(struct mloop_work* self)
{
	int i;

	/* We start each node individually because we don't want to start nodes
	 * that were not properly registered.
	 */
	profile("Start nodes...\n");
	for_each_node_reverse(i)
		if (co_master_get_node(i)->driver_type != CO_MASTER_DRIVER_NONE)
			co_net_send_nmt(&socket_, NMT_CS_START, i);

	profile("Start node guarding...\n");
	for_each_node(i)
		if (co_master_get_node(i)->driver_type != CO_MASTER_DRIVER_NONE)
			start_nodeguarding(i);

	profile("Boot-up finished!\n");

	master_state_ = MASTER_STATE_RUNNING;

	load_late_nodes();
}

static void on_net_probe_done(struct mloop_work* self)
{
	profile("Initialize multiplexer...\n");
	int __unused rc = init_multiplexer();
	assert(rc == 0);

	run_bootup();
}

static void set_priority(void)
{
	struct sched_param prio = { .sched_priority = 25 };
	sched_setscheduler(getpid(), SCHED_FIFO, &prio);
}

static int start_bootup(void)
{
	struct mloop_work* work = mloop_work_new(mloop_default());
	if (!work)
		return -1;

	mloop_work_set_work_fn(work, run_net_probe);
	mloop_work_set_done_fn(work, on_net_probe_done);

	int rc = mloop_work_start(work);
	mloop_work_unref(work);

	return rc;
}

#ifndef NO_MAREL_CODE
static int on_tickermaster_alive(void)
{
	return start_bootup();
}

static int appbase_dummy()
{
	return 0;
}

static int run_appbase()
{
	appcbase_t cb = { 0 };
	cb.queue_timeout_ms = -1;
	cb.usetm = 1;
	cb.initialize = appbase_dummy;
	cb.deinitialize = appbase_dummy;
	cb.on_tickmaster_alive = on_tickermaster_alive;

	if (appbase_initialize("canopen", appbase_get_instance(), &cb) != 0)
		return 1;

	set_priority();

	mloop_run(mloop_);

	appbase_deinitialize();

	return 0;
}

static int master_set_node_state(int nodeid, int state)
{
	/* not allowed */
	return 0;
}

static inline
struct co_master_node*
get_node_from_sdo_queue(const struct sdo_req_queue* queue)
{
	return co_master_get_node(queue->nodeid);
}

static void on_master_sdo_request_done(struct sdo_req* req)
{
	struct co_master_node* node = get_node_from_sdo_queue(req->parent);
	void* driver = node->driver;
	assert(driver);

	if (req->status == SDO_REQ_OK) {
		legacy_driver_iface_process_sdo(driver,
						req->index,
						req->subindex,
						(unsigned char*)req->data.data,
						req->data.index);
	} else {
		legacy_driver_iface_process_sdo(driver,
						req->index,
						req->subindex,
						NULL, 0);
	}
}

static int master_request_sdo(int nodeid, int index, int subindex)
{
	struct sdo_req_info info = {
		.type = SDO_REQ_UPLOAD,
		.index = index,
		.subindex = subindex,
		.on_done = on_master_sdo_request_done
	};

	struct sdo_req* req = sdo_req_new(&info);
	if (!req)
		return -1;

	int rc = sdo_req_start(req, sdo_req_queue_get(nodeid));

	sdo_req_unref(req);
	return rc;
}

static void on_master_sdo_send_done(struct sdo_req* req)
{
	(void)req;
}

static int master_send_sdo(int nodeid, int index, int subindex,
			   unsigned char* data, size_t size)
{
	struct sdo_req_info info = {
		.type = SDO_REQ_DOWNLOAD,
		.index = index,
		.subindex = subindex,
		.dl_data = data,
		.dl_size = size,
		.on_done = on_master_sdo_send_done
	};

	struct sdo_req* req = sdo_req_new(&info);
	if (!req)
		return -1;

	int rc = sdo_req_start(req, sdo_req_queue_get(nodeid));

	sdo_req_unref(req);
	return rc;
}
#endif /* NO_MAREL_CODE */

int co__rpdox(int nodeid, int type, const void* data, size_t size)
{
	if (!data || size > CAN_MAX_DLEN)
		return -1;

	struct can_frame cf = {
		.can_id = type + nodeid,
		.can_dlc = size
	};

	memcpy(cf.data, data, size);

	return sock_send(&socket_, &cf, 0);

}

#ifndef NO_MAREL_CODE
static int master_send_pdo(int nodeid, int n, unsigned char* data, size_t size)
{
	switch (n) {
	case 1: return co__rpdox(nodeid, R_RPDO1, data, size);
	case 2: return co__rpdox(nodeid, R_RPDO2, data, size);
	case 3: return co__rpdox(nodeid, R_RPDO3, data, size);
	case 4: return co__rpdox(nodeid, R_RPDO4, data, size);
	}

	abort();
	return -1;
}

static void* master_iface_init(int nodeid)
{
	struct legacy_master_iface cb;
	cb.set_node_state = master_set_node_state;
	cb.request_sdo = master_request_sdo;
	cb.send_sdo = master_send_sdo;
	cb.send_pdo = master_send_pdo;
	cb.nodeid = nodeid;

	return legacy_master_iface_new(&cb);
}
#endif /* NO_MAREL_CODE */

static int init_heartbeat_timer(struct co_master_node* node)
{
	int nodeid = co_master_get_node_id(node);

	struct mloop_timer* timer = mloop_timer_new(mloop_default());
	if (!timer)
		return -1;

	uint64_t period = cfg.node[nodeid].heartbeat_period
			+ cfg.node[nodeid].heartbeat_timeout;

	if (cfg.node[nodeid].n_timeouts_max > 0)
		mloop_timer_set_type(timer, MLOOP_TIMER_PERIODIC);

	mloop_timer_set_context(timer, node, NULL);
	mloop_timer_set_time(timer, period * 1000000LL);
	mloop_timer_set_callback(timer, on_heartbeat_timeout);
	node->heartbeat_timer = timer;

	return 0;
}

static int init_ping_timer(struct co_master_node* node)
{
	int nodeid = co_master_get_node_id(node);

	struct mloop_timer* timer = mloop_timer_new(mloop_default());
	if (!timer)
		return -1;

	mloop_timer_set_type(timer, MLOOP_TIMER_PERIODIC);
	mloop_timer_set_context(timer, node, NULL);
	mloop_timer_set_time(timer, cfg.node[nodeid].heartbeat_period * 1000000LL);
	mloop_timer_set_callback(timer, on_ping_timeout);
	node->ping_timer = timer;

	return 0;
}

static int init_node_structure(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);

	memset(node, 0, sizeof(*node));

	if (init_heartbeat_timer(node) < 0)
		return -1;;

	if (init_ping_timer(node) < 0)
		goto ping_timer_failure;

	return 0;

ping_timer_failure:
	mloop_timer_unref(node->heartbeat_timer);
	return -1;
}

static void destroy_node_structure(int nodeid)
{
	struct co_master_node* node = co_master_get_node(nodeid);

	mloop_timer_unref(node->ping_timer);
	mloop_timer_unref(node->heartbeat_timer);
}

static int init_all_node_structures()
{
	int i;
	for_each_node(i)
		if (init_node_structure(i) < 0)
			goto failure;

	return 0;

failure:
	for (; i > 0; --i)
		destroy_node_structure(i);

	return -1;
}

static void destroy_all_node_structures()
{
	int i;
	for_each_node(i)
		destroy_node_structure(i);
}

static void unload_all_drivers()
{
	int i;
	for_each_node(i)
		if (co_master_get_node(i)->driver_type != CO_MASTER_DRIVER_NONE)
			unload_driver(i);
}

__attribute__((visibility("default")))
int co_master_run(void)
{
	int rc = 0;

	profiling_reset();
	profile("Starting up canopen-master...\n");

	memset(nodes_seen_, 0, sizeof(nodes_seen_));
	memset(nodes_seen_late_, 0, sizeof(nodes_seen_));
	memset(co_master_node_, 0, sizeof(co_master_node_));

	mloop_ = mloop_default();
	mloop_ref(mloop_);

	profile("Load EDS database...\n");
	eds_db_load();

	profile("Initialize and register SDO REST service...\n");
	if (rest_init(cfg.rest_port) < 0) {
		perror("Could not initialize rest service");
		goto rest_init_failure;
	}

	if (rest_register_service(HTTP_GET | HTTP_PUT,
				  "sdo", sdo_rest_service) < 0)
		goto rest_service_failure;

	profile("Open interface...\n");
	enum sock_type sock_type = cfg.use_tcp ? SOCK_TYPE_TCP : SOCK_TYPE_CAN;
	if (sock_open(&socket_, sock_type, cfg.iface) < 0) {
		perror("Could not open CAN bus");
		goto socketcan_open_failure;
	}

#ifndef NO_MAREL_CODE
	if (canopen_info_init(cfg.iface) < 0) {
		perror("Could not initialize info structure");
		goto info_failure;
	}
#endif /* NO_MAREL_CODE */

	enum sdo_async_quirks_flags sdo_quirks;
	sdo_quirks = cfg.be_strict ? SDO_ASYNC_QUIRK_NONE : SDO_ASYNC_QUIRK_ALL;

	profile("Initialize SDO queues...\n");
	if (sdo_req_queues_init(&socket_, cfg.sdo_queue_length, sdo_quirks)
			< 0)
		goto sdo_req_queues_failure;

	profile("Initialize node structure...\n");
	if (init_all_node_structures() < 0)
		goto node_init_failure;

	if (sock_type == SOCK_TYPE_CAN)
		net_fix_sndbuf(socket_.fd);

#ifndef NO_MAREL_CODE
	profile("Create legacy driver manager...\n");
	driver_manager_ = legacy_driver_manager_new();
	if (!driver_manager_) {
		rc = 1;
		goto driver_manager_failure;
	}
#endif /* NO_MAREL_CODE */

	mloop_set_job_queue_size(cfg.job_queue_length);
	mloop_set_worker_stack_size(cfg.job_queue_length);

	profile("Start worker threads...\n");
	if (mloop_require_workers(cfg.n_workers) != 0) {
		rc = 1;
		goto worker_failure;
	}

#ifndef NO_MAREL_CODE
	rc = run_appbase();
#else
	if (start_bootup() < 0)
		goto bootup_failure;

	rc = mloop_run(mloop_);
#endif /* NO_MAREL_CODE */

	master_state_ = MASTER_STATE_STOPPING;

	unload_all_drivers();

	if (mux_handler_) {
		mloop_socket_set_fd(mux_handler_, -1);
		mloop_socket_unref(mux_handler_);
	}

bootup_failure:
worker_failure:
#ifndef NO_MAREL_CODE
	legacy_driver_manager_delete(driver_manager_);
#endif /* NO_MAREL_CODE */

driver_manager_failure:
	destroy_all_node_structures();

node_init_failure:
	sdo_req_queues_cleanup();

sdo_req_queues_failure:
	if (socket_.fd >= 0)
		sock_close(&socket_);

#ifndef NO_MAREL_CODE
	canopen_info_cleanup();
#endif /* NO_MAREL_CODE */
info_failure:
socketcan_open_failure:
rest_service_failure:
	rest_cleanup();

rest_init_failure:
	eds_db_unload();

	mloop_unref(mloop_);
	return rc;
}

