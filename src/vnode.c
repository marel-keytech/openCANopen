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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <mloop.h>
#include <errno.h>
#include <stddef.h>

#include "socketcan.h"
#include "canopen.h"
#include "canopen/sdo_srv.h"
#include "canopen/nmt.h"
#include "canopen/heartbeat.h"
#include "canopen/byteorder.h"
#include "canopen/types.h"
#include "net-util.h"
#include "sock.h"
#include "ini_parser.h"
#include "conversions.h"
#include "vnode.h"
#include "type-macros.h"

#define SDO_MUX(index, subindex) ((index << 16) | subindex)
#define HEARTBEAT_PERIOD SDO_MUX(0x1017, 0)

enum vnode__bootup_method {
	VNODE_BOOT_UNSPEC = 0,
	VNODE_BOOT_STANDARD = 1,
	VNODE_BOOT_LEGACY = 2,
	VNODE_BOOT_BOTH = VNODE_BOOT_STANDARD | VNODE_BOOT_LEGACY,
};

struct vnode {
	int is_running;
	struct ini_file config;
	int nodeid;
	enum nmt_state state;
	struct sdo_srv sdo_srv;
	uint16_t heartbeat_period;
	struct mloop_timer* heartbeat_timer;
	int have_heartbeat;
	int have_node_guarding;
	int have_guard_status_bug;
	enum vnode__bootup_method bootup_method;
};

struct sock vnode__sock;
static struct mloop_socket* vnode__socket = NULL;

struct vnode vnode__node[127] = { 0 };

static inline struct vnode* vnode__get_node(int nodeid)
{
	return &vnode__node[nodeid - 1];
}

static void vnode__init(struct vnode* self)
{
	memset(self, 0, sizeof(*self));
	self->nodeid = -1;
	self->bootup_method = VNODE_BOOT_UNSPEC;
}

static enum vnode__bootup_method
vnode__get_bootup_method(const struct ini_section* s)
{
	const char* value = ini_find_key(s, "bootup");
	if (!value)
		return 0;

	if (0 == strcasecmp(value, "standard")) return VNODE_BOOT_STANDARD;
	if (0 == strcasecmp(value, "legacy"))   return VNODE_BOOT_LEGACY;
	if (0 == strcasecmp(value, "both"))     return VNODE_BOOT_BOTH;

	return VNODE_BOOT_UNSPEC;
}

static int vnode__config_is_true(const struct ini_section* s, const char* key)
{
	const char* value = ini_find_key(s, key);
	return value ? strcasecmp(value, "yes") == 0 : 0;
}

static void vnode__load_device_info(struct vnode* self)
{
	const struct ini_section* s;
	s = ini_find_section(&self->config, "device");
	if (!s)
		return;

	self->have_heartbeat = vnode__config_is_true(s, "heartbeat");
	self->have_node_guarding = vnode__config_is_true(s, "node_guarding");
	self->have_guard_status_bug
		= vnode__config_is_true(s, "guard_status_bug");
	self->bootup_method = vnode__get_bootup_method(s);
}

static int vnode__load_config(struct vnode* self, const char* path)
{
	FILE* stream = fopen(path, "r");
	if (!stream) {
		perror("Could not open config");
		return -1;
	}

	int rc = ini_parse(&self->config, stream);
	if (rc < 0)
		perror("Could not parse config");

	fclose(stream);

	vnode__load_device_info(self);

	return rc;
}

static void vnode__send_state(struct vnode* self)
{
	struct can_frame cf = {
		.can_id = R_HEARTBEAT + self->nodeid,
		.can_dlc = 1
	};

	if (!self->have_guard_status_bug)
		heartbeat_set_state(&cf, self->state);

	sock_send(&vnode__sock, &cf, 0);
}

static void vnode__send_legacy_bootup(struct vnode* self)
{
	struct can_frame cf = {
		.can_id = R_EMCY + self->nodeid,
		.can_dlc = 0
	};

	sock_send(&vnode__sock, &cf, 0);
}

static void vnode__reset_communication(struct vnode* self)
{
	if (!(self->bootup_method & VNODE_BOOT_STANDARD))
		return;

	self->state = NMT_STATE_BOOTUP;
	vnode__send_state(self);
	self->state = NMT_STATE_PREOPERATIONAL;
}

static void vnode__on_heartbeat(struct mloop_timer* timer)
{
	struct vnode* self = mloop_timer_get_context(timer);
	vnode__send_state(self);
}

static void vnode__heartbeat(struct vnode* self, const struct can_frame* cf)
{
	(void)cf;

	if (self->have_node_guarding)
		vnode__send_state(self);
}

static int vnode__start_heartbeat_timer(struct vnode* self)
{
	if (!self->have_heartbeat)
		return 0;

	if (self->state != NMT_STATE_OPERATIONAL
	 || self->heartbeat_period == 0)
		return 0;

	struct mloop_timer* timer = self->heartbeat_timer;
	mloop_timer_set_time(timer, (uint64_t)self->heartbeat_period * 1000000ULL);
	return mloop_timer_start(timer);
}

static int vnode__restart_heartbeat_timer(struct vnode* self)
{
	if (!self->have_heartbeat)
		return 0;

	mloop_timer_stop(self->heartbeat_timer);
	return vnode__start_heartbeat_timer(self);
}

static void vnode__nmt(struct vnode* self, const struct can_frame* cf)
{
	int nodeid = nmt_get_nodeid(cf);
	if (nodeid != self->nodeid && nodeid != 0)
		return;

	enum nmt_cs cs = nmt_get_cs(cf);
	switch (cs) {
	case NMT_CS_START:
		self->state = NMT_STATE_OPERATIONAL;
		vnode__start_heartbeat_timer(self);
		break;
	case NMT_CS_STOP:
		self->state = NMT_STATE_STOPPED;
		if (self->have_heartbeat)
			mloop_timer_stop(self->heartbeat_timer);
		break;
	case NMT_CS_ENTER_PREOPERATIONAL:
		self->state = NMT_STATE_PREOPERATIONAL;
		break;
	case NMT_CS_RESET_NODE:
	case NMT_CS_RESET_COMMUNICATION:
		if (self->have_heartbeat)
			mloop_timer_stop(self->heartbeat_timer);
		vnode__reset_communication(self);
		break;
	}
}

static int vnode__sdo_get_heartbeat(struct vnode* self, struct sdo_srv* srv)
{
	if (!self->have_heartbeat)
		return sdo_srv_abort(srv, SDO_ABORT_NEXIST);

	uint16_t netorder = 0;
	byteorder(&netorder, &self->heartbeat_period, sizeof(netorder));
	vector_assign(&srv->buffer, &netorder, sizeof(netorder));
	return 0;
}

static const char* vnode__make_section_string(int index, int subindex)
{
	static char buffer[256];
	snprintf(buffer, sizeof(buffer), "%xsub%x", index, subindex);
	return buffer;
}

static enum canopen_type vnode__get_section_type(const struct ini_section* s)
{
	const char* type_str = ini_find_key(s, "type");
	if (!type_str)
		return CANOPEN_UNKNOWN;

	return canopen_type_from_string(type_str);
}

static int vnode__sdo_get_config(struct vnode* self, struct sdo_srv* srv)
{
	const char* section;
	section = vnode__make_section_string(srv->index, srv->subindex);

	const struct ini_section* s = ini_find_section(&self->config, section);
	if (!s)
		return sdo_srv_abort(srv, SDO_ABORT_NEXIST);

	enum canopen_type type = vnode__get_section_type(s);
	if (type == CANOPEN_UNKNOWN)
		return sdo_srv_abort(srv, SDO_ABORT_NEXIST);

	const char* value = ini_find_key(s, "value");
	if (!value)
		return sdo_srv_abort(srv, SDO_ABORT_NEXIST);

	struct canopen_data data;
	if (canopen_data_fromstring(&data, type, value) < 0)
		return sdo_srv_abort(srv, SDO_ABORT_NOMEM);

	if (vector_assign(&srv->buffer, data.data, data.size) < 0)
		return sdo_srv_abort(srv, SDO_ABORT_NOMEM);

	return 0;
}

static int vnode__on_sdo_init(struct sdo_srv* srv)
{
	struct vnode* self = container_of(srv, struct vnode, sdo_srv);
	uint32_t index = srv->index;
	uint32_t subindex = srv->subindex;

	switch (SDO_MUX(index, subindex)) {
	case HEARTBEAT_PERIOD:
		return srv->req_type == SDO_REQ_UPLOAD
		     ? vnode__sdo_get_heartbeat(self, srv) : 0;
	default:
		return srv->req_type == SDO_REQ_UPLOAD
		     ? vnode__sdo_get_config(self, srv)
		     : sdo_srv_abort(srv, SDO_ABORT_RO);
	}

	abort();
	return -1;
}

static int vnode__setup_heartbeat_timer(struct vnode* self)
{
	struct mloop_timer* timer = mloop_timer_new(mloop_default());
	if (!timer)
		return -1;

	mloop_timer_set_type(timer, MLOOP_TIMER_PERIODIC | MLOOP_TIMER_RELATIVE);
	mloop_timer_set_callback(timer, vnode__on_heartbeat);
	mloop_timer_set_context(timer, self, NULL);

	self->heartbeat_timer = timer;

	return 0;
}

static int vnode__sdo_set_heartbeat(struct vnode* self, struct sdo_srv* srv)
{
	if (!self->have_heartbeat)
		return sdo_srv_abort(srv, SDO_ABORT_NEXIST);

	uint16_t period = 0;
	if (srv->buffer.index > sizeof(period))
		return sdo_srv_abort(srv, SDO_ABORT_TOO_LONG);

	byteorder2(&period, srv->buffer.data, sizeof(period),
		   srv->buffer.index);

	self->heartbeat_period = period;
	vnode__restart_heartbeat_timer(self);

	return 0;
}

static int vnode__sdo_set_config(struct sdo_srv* srv)
{
	return sdo_srv_abort(srv, SDO_ABORT_NEXIST);
}

static int vnode__on_sdo_done(struct sdo_srv* srv)
{
	struct vnode* self = container_of(srv, struct vnode, sdo_srv);

	if (srv->req_type == SDO_REQ_UPLOAD)
		return 0;

	uint32_t index = srv->index;
	uint32_t subindex = srv->subindex;

	switch (SDO_MUX(index, subindex)) {
	case HEARTBEAT_PERIOD:
		return vnode__sdo_set_heartbeat(self, srv);
	default:
		return vnode__sdo_set_config(srv);
	}

	abort();
	return -1;

	return 0;
}

static void vnode__rsdo(struct vnode* self, const struct can_frame* cf)
{
	sdo_srv_feed(&self->sdo_srv, cf);
}

static void vnode__on_frame(const struct can_frame* cf)
{
	struct vnode* self;
	struct canopen_msg msg;

	if (canopen_get_object_type(&msg, cf) < 0)
		return;

	if (msg.object != CANOPEN_NMT) {
		if (!(CANOPEN_NODEID_MIN <= msg.id
		      && msg.id <= CANOPEN_NODEID_MAX))
			return;

		self = vnode__get_node(msg.id);
		if (!self)
			return;

		if (msg.id != self->nodeid)
			return;
	} else {
		if (nmt_get_nodeid(cf) == 0) {
			for (int i = 1; i < 128; ++i) {
				self = vnode__get_node(i);
				if (self->is_running)
					vnode__nmt(self, cf);
				return;
			}
		} else {
			self = vnode__get_node(nmt_get_nodeid(cf));
		}
	}

	switch (msg.object) {
	case CANOPEN_HEARTBEAT:
		vnode__heartbeat(self, cf);
		break;
	case CANOPEN_NMT:
		vnode__nmt(self, cf);
		break;
	case CANOPEN_RSDO:
		vnode__rsdo(self, cf);
		break;
	default:
		break;
	}
}

static void vnode__mux(struct mloop_socket* socket)
{
	struct can_frame cf;

	while (1) {
		ssize_t rsize = sock_recv(&vnode__sock, &cf, MSG_DONTWAIT);
		if (rsize == 0)
			mloop_socket_stop(socket);

		if (rsize <= 0)
			return;

		vnode__on_frame(&cf);
	}
}

static int vnode__setup_mloop(struct vnode* self)
{
	struct mloop_socket* socket = mloop_socket_new(mloop_default());
	if (!socket)
		return -1;

	mloop_socket_set_fd(socket, vnode__sock.fd);
	mloop_socket_set_context(socket, &vnode__sock,
				 (mloop_free_fn)sock_close);
	mloop_socket_set_callback(socket, vnode__mux);

	if (mloop_socket_start(socket) < 0) {
		mloop_socket_unref(socket);
		return -1;
	}

	vnode__socket = socket;
	return 0;
}

static void vnode__cleanup_mloop(void)
{
	if (mloop_socket_unref(vnode__socket) == 1) {
		mloop_socket_stop(vnode__socket);
		vnode__socket = NULL;
	}
}

int vnode__init_socket(struct vnode* self, enum sock_type type,
		       const char* iface)
{
	if (vnode__socket) {
		mloop_socket_ref(vnode__socket);
		return 0;
	}

	if (sock_open(&vnode__sock, type, iface) < 0) {
		perror("Failed to open CAN interface");
		return -1;
	}

	if (vnode__setup_mloop(self) < 0)
		goto failure;

	return 0;

failure:
	sock_close(&vnode__sock);
	return -1;
}

__attribute__((visibility("default")))
struct vnode* co_vnode_new(enum sock_type type, const char* iface,
			   const char* config_path, int nodeid)
{
	struct vnode* self = vnode__get_node(nodeid);
	if (!self)
		return NULL;

	vnode__init(self);

	if (vnode__init_socket(self, type, iface) < 0)
		return NULL;

	if (config_path)
		if (vnode__load_config(self, config_path) < 0)
			goto config_failure;

	self->nodeid = nodeid;
	self->state = NMT_STATE_BOOTUP;

	if (sdo_srv_init(&self->sdo_srv, &vnode__sock, nodeid,
			 vnode__on_sdo_init, vnode__on_sdo_done) < 0)
		goto srv_failure;

	if (self->have_heartbeat)
		if (vnode__setup_heartbeat_timer(self) < 0)
			goto srv_failure;

	if (self->bootup_method & VNODE_BOOT_LEGACY)
		vnode__send_legacy_bootup(self);

	vnode__reset_communication(self);

	self->is_running = 1;
	return self;

	sdo_srv_destroy(&self->sdo_srv);
srv_failure:
	if (config_path)
		ini_destroy(&self->config);
config_failure:
	vnode__cleanup_mloop();
	return NULL;
}

__attribute__((visibility("default")))
void co_vnode_destroy(struct vnode* self)
{
	if (self->have_heartbeat)
		mloop_timer_unref(self->heartbeat_timer);

	sdo_srv_destroy(&self->sdo_srv);
	ini_destroy(&self->config);
	vnode__cleanup_mloop();
	self->is_running = 0;
}

