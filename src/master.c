#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <appcbase.h>
#include <ctype.h>

#include "main-loop.h"
#include "socketcan.h"
#include "canopen.h"
#include "canopen/sdo_client.h"
#include "canopen/network.h"
#include "canopen/nmt.h"
#include "canopen/heartbeat.h"
#include "canopen/emcy.h"
#include "mux.h"

#include "legacy-driver.h"

static int socket_ = -1;
static char nodes_seen_[128];

static void* master_iface_;
static void* driver_manager_;

static struct ml_handler mux_handler_;

struct canopen_node {
	void* driver;
	struct sdo_channel sdo_channel;
};

static struct canopen_node node_[128];

void clean_node_name(char* name, size_t size)
{
	int i, k = 0;

	for (i = 0; i < size && name[i]; ++i)
		if (isalnum(name[i])) {
			int c = name[i];
			name[k++] = c;
		}

	if (i == size)
		name[i-1] = '\0';
	else if (k < i)
		name[k] = '\0';

}

ssize_t sdo_read_limited(int nodeid, int index, int subindex, void* buf,
			 size_t size)
{
	struct sdo_ul_req req;
	struct can_frame cf;

	sdo_request_upload(&req, index, subindex);
	req.frame.can_id = R_RSDO + nodeid;

	net_write_frame(socket_, &req.frame, -1);

	req.pos = 0;
	req.addr = buf;
	req.size = size;

	while (1) {
		if (net_filtered_read_frame(socket_, &cf, 100, R_TSDO + nodeid) < 0)
			return -1;

		if (sdo_ul_req_feed(&req, &cf) < 0)
			return -1;

		req.frame.can_id = R_RSDO + nodeid;

		if (req.have_frame)
			net_write_frame(socket_, &req.frame, -1);

		if (req.state == SDO_REQ_DONE)
			break;

		if (req.state < 0)
			return -1;
	}

	return req.pos;
}

uint32_t get_device_type(int nodeid)
{
	uint32_t device_type = 0;

	if (sdo_read_limited(nodeid, 0x1000, 0, &device_type,
			     sizeof(device_type)) < 0)
		return 0;

	return device_type;
}

const char* get_name(int nodeid)
{
	static char name[256];

	if (sdo_read_limited(nodeid, 0x1008, 0, name, sizeof(name)) < 0)
		return NULL;

	clean_node_name(name, sizeof(name));

	return name;
}

static int load_driver(int nodeid)
{
	void* driver = NULL;
	struct canopen_node* node = &node_[nodeid];

	if (sdo_channel_init(&node->sdo_channel) < 0)
		return -1;

	uint32_t device_type = get_device_type(nodeid);
	if (device_type == 0)
		goto failure;

	const char* name = get_name(nodeid);
	if (!name)
		goto failure;

	if (legacy_driver_manager_create_handler(driver_manager_, name,
						 device_type, master_iface_,
						 &driver) < 0)
		goto failure;

	node->driver = driver;

	return 0;

failure:
	sdo_channel_clear(&node->sdo_channel);
	return -1;
}

static void run_bootup_sequence(void* context)
{
	(void)context;

	net_probe(socket_, nodes_seen_, 1, 127, 1000);

	for (int i = 1; i < 128; ++i)
		if (nodes_seen_[i])
			load_driver(i);
}

static int handle_bootup(struct canopen_node* node)
{
	/* TODO */
	return 0;
}

static int handle_emcy(struct canopen_node* node, const struct can_frame* frame)
{
	if (frame->can_dlc == 0)
		return handle_bootup(node);

	if (frame->can_dlc != 8)
		return -1;

	legacy_driver_iface_process_emr(node->driver,
					emcy_get_code(frame),
					emcy_get_register(frame),
					emcy_get_manufacturer_error(frame));

	return 0;
}

static int handle_heartbeat(struct canopen_node* node,
			     const struct can_frame* frame)
{
	if (heartbeat_is_bootup(frame))
		return handle_bootup(node);

	/* TODO */
	return 0;
}

static void mux_handler_fn(int fd, void* context)
{
	(void)context;

	struct can_frame cf;
	struct canopen_msg msg;

	net_read_frame(fd, &cf, -1);

	canopen_get_object_type(&msg, &cf);

	assert(msg.id < 128);

	struct canopen_node* node = &node_[msg.id];
	void* driver = node->driver;

	switch (msg.object)
	{
	case CANOPEN_NMT:
		break; /* TODO: this is illegal */
	case CANOPEN_TPDO1:
		legacy_driver_iface_process_pdo(driver, 1, cf.data, cf.can_dlc);
		break;
	case CANOPEN_TPDO2:
		legacy_driver_iface_process_pdo(driver, 2, cf.data, cf.can_dlc);
		break;
	case CANOPEN_TPDO3:
		legacy_driver_iface_process_pdo(driver, 3, cf.data, cf.can_dlc);
		break;
	case CANOPEN_TPDO4:
		legacy_driver_iface_process_pdo(driver, 4, cf.data, cf.can_dlc);
		break;
	case CANOPEN_TSDO:
		sdo_channel_feed(node, &cf);
		break;
	case CANOPEN_EMCY:
		handle_emcy(node, &cf);
		break;
	case CANOPEN_HEARTBEAT:
		handle_heartbeat(node, &cf);
		break;
	default:
		break;
	}
}

static void init_multiplexer()
{
	ml_handler_init(&mux_handler_);
	mux_handler_.fd = socket_;
	mux_handler_.fn = mux_handler_fn;
	ml_handler_start(&mux_handler_);
}

static void on_bootup_done(void* context)
{
	(void)context;

	init_multiplexer();

	net__send_nmt(socket_, NMT_CS_START, 0);
}

static int initialize()
{
	return 0;
}

static int cleanup()
{
	return 0;
}

static int on_tickermaster_alive()
{
	net__send_nmt(socket_, NMT_CS_RESET_COMMUNICATION, 0);

	static struct ml_job job;
	ml_job_init(&job);
	job.work_fn = run_bootup_sequence;
	job.after_work_fn = on_bootup_done;
	ml_job_enqueue(&job);

	return 0;
}

static int run_appbase(int* argc, char* argv[])
{
	appcbase_t cb = { 0 };
	cb.queue_timeout_ms = -1;
	cb.usetm = 1;
	cb.initialize = initialize;
	cb.deinitialize = cleanup;
	cb.on_tickmaster_alive = on_tickermaster_alive;

	return appbase_start(argc, argv, &cb) == 0 ? 0 : 1;
}

static int master_set_node_state(int nodeid, int state)
{
	/* not allowed */
	return 0;
}

static int master_request_sdo(int nodeid, int index, int subindex)
{
	/* TODO: Create fifo queue for sdo per node */
	return 0;
}

static int master_send_sdo(int nodeid, int index, int subindex,
			   unsigned char* data, size_t size)
{
	/* TODO: Create fifo queue for sdo per node */
	return 0;
}

static int send_pdo(int nodeid, int type, unsigned char* data, size_t size)
{
	struct can_frame cf = { 0 };

	cf.can_id = type + nodeid;
	cf.can_dlc = size;

	if (data)
		memcpy(cf.data, data, size);

	return net_write_frame(socket_, &cf, -1);

}

static int master_send_pdo(int nodeid, int n, unsigned char* data, size_t size)
{
	switch (n) {
	case 1: return send_pdo(nodeid, R_RPDO1, data, size);
	case 2: return send_pdo(nodeid, R_RPDO2, data, size);
	case 3: return send_pdo(nodeid, R_RPDO3, data, size);
	case 4: return send_pdo(nodeid, R_RPDO4, data, size);
	}

	abort();
	return -1;
}

static int master_iface_init()
{
	struct legacy_master_iface cb;
	cb.set_node_state = master_set_node_state;
	cb.request_sdo = master_request_sdo;
	cb.send_sdo = master_send_sdo;
	cb.send_pdo = master_send_pdo;

	master_iface_ = legacy_master_iface_new(&cb);

	return master_iface_ ? 0 : 1;
}

int main(int argc, char* argv[])
{
	int rc = 0;
	const char* iface = argv[1];

	memset(nodes_seen_, 0, sizeof(nodes_seen_));
	memset(node_, 0, sizeof(node_));

	socket_ = socketcan_open(iface);
	if (socket_ < 0)
		return 1;

	net_dont_block(socket_);

	rc = master_iface_init();
	if (rc != 0)
		goto master_iface_failure;

	driver_manager_ = legacy_driver_manager_new();
	if (!driver_manager_) {
		rc = 1;
		goto driver_manager_failure;
	}

	if (worker_init(4, 1024, 0) != 0) {
		rc = 1;
		goto worker_failure;
	}

	rc = run_appbase(&argc, argv);

	worker_deinit();

worker_failure:
	legacy_driver_manager_delete(driver_manager_);

driver_manager_failure:
	legacy_master_iface_delete(master_iface_);

master_iface_failure:
	if (socket_ >= 0)
		close(socket_);

	return rc;
}

