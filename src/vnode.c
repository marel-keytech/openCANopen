#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <mloop.h>

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

#define SDO_MUX(index, subindex) ((index << 16) | subindex)
#define HEARTBEAT_PERIOD SDO_MUX(0x1017, 0)

static struct sock vnode__sock;
static struct ini_file vnode__config;
static int vnode__nodeid = -1;
static enum nmt_state vnode__state;
static struct sdo_srv vnode__sdo_srv;
static uint16_t vnode__heartbeat_period = 0;
static struct mloop_timer* vnode__heartbeat_timer = 0;

static int vnode__have_heartbeat = 0;
static int vnode__have_node_guarding = 0;

static int vnode__config_is_true(const struct ini_section* s, const char* key)
{
	const char* value = ini_find_key(s, key);
	return value ? strcasecmp(value, "yes") == 0 : 0;
}

static void vnode__load_device_info(void)
{
	const struct ini_section* s;
	s = ini_find_section(&vnode__config, "device");
	if (!s)
		return;

	vnode__have_heartbeat = vnode__config_is_true(s, "heartbeat");
	vnode__have_node_guarding = vnode__config_is_true(s, "node_guarding");
}

static int vnode__load_config(const char* path)
{
	FILE* stream = fopen(path, "r");
	if (!stream) {
		perror("Could not open config");
		return -1;
	}

	int rc = ini_parse(&vnode__config, stream);
	if (rc < 0)
		perror("Could not parse config");

	fclose(stream);

	vnode__load_device_info();

	return rc;
}

static void vnode__send_state(void)
{
	struct can_frame cf = {
		.can_id = R_HEARTBEAT + vnode__nodeid,
		.can_dlc = 1
	};

	heartbeat_set_state(&cf, vnode__state);

	sock_send(&vnode__sock, &cf, -1);
}

static void vnode__reset_communication(void)
{
	vnode__state = NMT_STATE_BOOTUP;
	vnode__send_state();
	vnode__state = NMT_STATE_PREOPERATIONAL;
}

static void vnode__on_heartbeat(struct mloop_timer* timer)
{
	(void)timer;
	vnode__send_state();
}

static void vnode__heartbeat(const struct can_frame* cf)
{
	(void)cf;

	if (vnode__have_node_guarding)
		vnode__send_state();
}

static int vnode__start_heartbeat_timer(void)
{
	if (!vnode__have_heartbeat)
		return 0;

	if (vnode__state != NMT_STATE_OPERATIONAL
	 || vnode__heartbeat_period == 0)
		return 0;

	struct mloop_timer* timer = vnode__heartbeat_timer;
	mloop_timer_set_time(timer, (uint64_t)vnode__heartbeat_period * 1000000ULL);
	return mloop_timer_start(timer);
}

static int vnode__restart_heartbeat_timer(void)
{
	if (!vnode__have_heartbeat)
		return 0;

	mloop_timer_stop(vnode__heartbeat_timer);
	return vnode__start_heartbeat_timer();
}

static void vnode__nmt(const struct can_frame* cf)
{
	int nodeid = nmt_get_nodeid(cf);
	if (nodeid != vnode__nodeid || nodeid == 0)
		return;

	enum nmt_cs cs = nmt_get_cs(cf);
	switch (cs) {
	case NMT_CS_START:
		vnode__state = NMT_STATE_OPERATIONAL;
		vnode__start_heartbeat_timer();
		break;
	case NMT_CS_STOP:
		vnode__state = NMT_STATE_STOPPED;
		if (vnode__have_heartbeat)
			mloop_timer_stop(vnode__heartbeat_timer);
		break;
	case NMT_CS_ENTER_PREOPERATIONAL:
		vnode__state = NMT_STATE_PREOPERATIONAL;
		break;
	case NMT_CS_RESET_NODE:
	case NMT_CS_RESET_COMMUNICATION:
		if (vnode__have_heartbeat)
			mloop_timer_stop(vnode__heartbeat_timer);
		vnode__reset_communication();
		break;
	}
}

static int vnode__sdo_get_heartbeat(struct sdo_srv* srv)
{
	if (!vnode__have_heartbeat)
		return sdo_srv_abort(srv, SDO_ABORT_NEXIST);

	uint16_t netorder = 0;
	byteorder(&netorder, &vnode__heartbeat_period, sizeof(netorder));
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

static int vnode__sdo_get_config(struct sdo_srv* srv)
{
	const char* section;
	section = vnode__make_section_string(srv->index, srv->subindex);

	const struct ini_section* s = ini_find_section(&vnode__config, section);
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
	if (srv->req_type == SDO_REQ_DOWNLOAD)
		return 0;

	uint32_t index = srv->index;
	uint32_t subindex = srv->subindex;

	switch (SDO_MUX(index, subindex)) {
	case HEARTBEAT_PERIOD:
		return vnode__sdo_get_heartbeat(srv);
	default:
		return vnode__sdo_get_config(srv);
	}

	abort();
	return -1;
}

static int vnode__setup_heartbeat_timer(void)
{
	struct mloop_timer* timer = mloop_timer_new(mloop_default());
	if (!timer)
		return -1;

	mloop_timer_set_type(timer, MLOOP_TIMER_PERIODIC | MLOOP_TIMER_RELATIVE);
	mloop_timer_set_callback(timer, vnode__on_heartbeat);

	vnode__heartbeat_timer = timer;

	return 0;
}

static int vnode__sdo_set_heartbeat(struct sdo_srv* srv)
{
	if (!vnode__have_heartbeat)
		return sdo_srv_abort(srv, SDO_ABORT_NEXIST);

	uint16_t period = 0;
	if (srv->buffer.index > sizeof(period))
		return sdo_srv_abort(srv, SDO_ABORT_TOO_LONG);

	byteorder2(&period, srv->buffer.data, sizeof(period),
		   srv->buffer.index);

	vnode__heartbeat_period = period;
	vnode__restart_heartbeat_timer();

	return 0;
}

static int vnode__sdo_set_config(struct sdo_srv* srv)
{
	return sdo_srv_abort(srv, SDO_ABORT_NEXIST);
}

static int vnode__on_sdo_done(struct sdo_srv* srv)
{
	if (srv->req_type == SDO_REQ_UPLOAD)
		return 0;

	uint32_t index = srv->index;
	uint32_t subindex = srv->subindex;

	switch (SDO_MUX(index, subindex)) {
	case HEARTBEAT_PERIOD:
		return vnode__sdo_set_heartbeat(srv);
	default:
		return vnode__sdo_set_config(srv);
	}

	abort();
	return -1;

	return 0;
}

static void vnode__rsdo(const struct can_frame* cf)
{
	sdo_srv_feed(&vnode__sdo_srv, cf);
}

static void vnode__mux(struct mloop_socket* socket)
{
	struct sock* sock = mloop_socket_get_context(socket);
	assert(sock);

	struct can_frame cf;
	struct canopen_msg msg;

	if (sock_recv(sock, &cf, 0) <= 0) {
		mloop_socket_stop(socket);
		return;
	}

	if (canopen_get_object_type(&msg, &cf) < 0)
		return;

	if (msg.object != CANOPEN_NMT) {
		if (!(CANOPEN_NODEID_MIN <= msg.id
		      && msg.id <= CANOPEN_NODEID_MAX))
			return;

		if (msg.id != vnode__nodeid)
			return;
	}

	switch (msg.object) {
	case CANOPEN_HEARTBEAT:
		vnode__heartbeat(&cf);
		break;
	case CANOPEN_NMT:
		vnode__nmt(&cf);
		break;
	case CANOPEN_RSDO:
		vnode__rsdo(&cf);
		break;
	default:
		break;
	}
}

static int vnode__setup_mloop(void)
{
	struct mloop_socket* socket = mloop_socket_new(mloop_default());
	if (!socket)
		return -1;

	mloop_socket_set_fd(socket, vnode__sock.fd);
	mloop_socket_set_context(socket, &vnode__sock,
				 (mloop_free_fn)sock_close);
	mloop_socket_set_callback(socket, vnode__mux);

	int rc = mloop_socket_start(socket);
	mloop_socket_unref(socket);

	return rc;
}

__attribute__((visibility("default")))
int co_vnode_init(enum sock_type type, const char* iface,
		  const char* config_path, int nodeid)
{
	if (sock_open(&vnode__sock, type, iface) < 0) {
		perror("Failed to open CAN interface");
		return -1;
	}

	if (config_path)
		if (vnode__load_config(config_path) < 0)
			goto failure;

	vnode__nodeid = nodeid;
	vnode__state = NMT_STATE_BOOTUP;

	if (sdo_srv_init(&vnode__sdo_srv, &vnode__sock, nodeid,
			 vnode__on_sdo_init, vnode__on_sdo_done) < 0)
		goto srv_failure;

	if (vnode__setup_mloop() < 0)
		goto mloop_failure;

	if (vnode__have_heartbeat)
		if (vnode__setup_heartbeat_timer() < 0)
			goto mloop_failure;

	vnode__reset_communication();

	return 0;

mloop_failure:
	sdo_srv_destroy(&vnode__sdo_srv);
srv_failure:
	if (config_path)
		ini_destroy(&vnode__config);
failure:
	sock_close(&vnode__sock);
	return -1;
}

__attribute__((visibility("default")))
void co_vnode_destroy(void)
{
	if (vnode__have_heartbeat)
		mloop_timer_unref(vnode__heartbeat_timer);

	sdo_srv_destroy(&vnode__sdo_srv);
	ini_destroy(&vnode__config);
	sock_close(&vnode__sock);
}

