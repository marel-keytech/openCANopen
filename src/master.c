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
#include "sdo_fifo.h"
#include "frame_fifo.h"

#include "legacy-driver.h"

#define SDO_FIFO_LENGTH 64

static int socket_ = -1;
static char nodes_seen_[128];

static int ignore_bootup_ = 1;

static void* master_iface_;
static void* driver_manager_;

static struct ml_handler mux_handler_;

struct canopen_node {
	void* driver;
	struct frame_fifo frame_fifo;
	struct sdo_req sdo_req;
	pthread_mutex_t sdo_channel;
	struct sdo_fifo sdo_fifo;
	int is_sdo_locked;
	int device_type;
};

struct driver_load_job {
	struct ml_job job;
	int nodeid;
};

static struct canopen_node node_[128];

static char upload_buffer_[4096]; /* TODO: make dynamic */

static inline int get_node_id(const struct canopen_node* node)
{
	return ((char*)node - (char*)node_) / sizeof(struct canopen_node);
}

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

ssize_t sdo_read_fifo(int nodeid, int index, int subindex, void* buf,
		      size_t size)
{
	struct sdo_req req;
	struct can_frame cf;
	struct canopen_node* node = &node_[nodeid];
	struct frame_fifo* fifo = &node->frame_fifo;

	sdo_request_upload(&req, index, subindex);
	req.frame.can_id = R_RSDO + nodeid;

	net_write_frame(socket_, &req.frame, -1);

	req.pos = 0;
	req.addr = buf;
	req.size = size;

	while (1) {
		/* TODO: fix timeout and make this time out */
		if (frame_fifo_dequeue(fifo, &cf, -1) < 0)
			return -1; /* TODO: send sdo timeout abort */

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

	if (sdo_read_fifo(nodeid, 0x1000, 0, &device_type,
			  sizeof(device_type)) < 0)
		return 0;

	return device_type;
}

const char* get_name(int nodeid)
{
	static char name[256];

	if (sdo_read_fifo(nodeid, 0x1008, 0, name, sizeof(name)) < 0)
		return NULL;

	clean_node_name(name, sizeof(name));

	return name;
}

static int load_driver(int nodeid)
{
	void* driver = NULL;
	struct canopen_node* node = &node_[nodeid];

	memset(node, 0, sizeof(*node));

	if (pthread_mutex_init(&node->sdo_channel, NULL) < 0)
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
	node->device_type = device_type;

	return 0;

failure:
	pthread_mutex_destroy(&node->sdo_channel);
	return -1;
}

static void run_net_probe(void* context)
{
	(void)context;

	net_probe(socket_, nodes_seen_, 1, 127, 1000);
}

static void run_load_driver(void* context)
{
	struct driver_load_job* job = context;
	load_driver(job->nodeid);
}

static int schedule_load_driver(int nodeid)
{
	struct driver_load_job* load_job = malloc(sizeof(*load_job));
	if (!load_job)
		return -1;

	struct ml_job* job = (struct ml_job*)load_job;

	ml_job_init(job);

	job->priority = nodeid;
	job->context = job;
	job->work_fn = run_load_driver;
	job->after_work_fn = free;
	load_job->nodeid = nodeid;

	if (ml_job_enqueue(job) < 0)
		goto failure;

	return 0;

failure:
	ml_job_clear(job);
	free(job);
	return -1;
}

static int handle_bootup(struct canopen_node* node)
{
	if (ignore_bootup_)
		return 0;

	if (node->driver) {
		legacy_driver_delete_handler(driver_manager_, node->device_type,
					     node->driver);
		node->driver = NULL;
	}

	return schedule_load_driver(get_node_id(node));
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

static int schedule_dl_sdo(struct canopen_node* node, struct sdo_elem* sdo)
{
	struct sdo_req* req = &node->sdo_req;

	if (!sdo_request_download(req, sdo->index, sdo->subindex,
				  sdo->data->data, sdo->data->size))
		return -1;

	req->frame.can_id = get_node_id(node) + R_RSDO;
	net_write_frame(socket_, &node->sdo_req.frame, -1);

	node->is_sdo_locked = 1;
	return 0;
}

static int schedule_ul_sdo(struct canopen_node* node, struct sdo_elem* sdo)
{
	struct sdo_req* req = &node->sdo_req;

	if (!sdo_request_upload(req, sdo->index, sdo->subindex))
		return -1;

	req->frame.can_id = get_node_id(node) + R_RSDO;
	req->addr = upload_buffer_;
	req->size = sizeof(upload_buffer_);
	req->pos = 0;

	net_write_frame(socket_, &req->frame, -1);

	node->is_sdo_locked = 1;
	return 0;
}

static int schedule_sdo(struct canopen_node* node)
{
	struct sdo_fifo* fifo = &node->sdo_fifo;
	if (node->is_sdo_locked)
		return 0;

	if (sdo_fifo_length(fifo) == 0)
		return 0;

	struct sdo_elem* head = sdo_fifo_head(fifo);

	switch (head->type)
	{
	case SDO_ELEM_DL: return schedule_dl_sdo(node, head);
	case SDO_ELEM_UL: return schedule_ul_sdo(node, head);
	}

	return -1;
}

static int handle_tsdo(struct canopen_node* node, const struct can_frame* frame)
{
	struct sdo_req* req = &node->sdo_req;
	struct sdo_fifo* fifo = &node->sdo_fifo;

	if (sdo_req_feed(req, frame))
		net_write_frame(socket_, &req->frame, -1);

	if (!node->is_sdo_locked)
		return -1;

	if (req->state != SDO_REQ_DONE && req->state >= 0)
		return 0;

	if (req->type == SDO_REQ_DL) {
		free(sdo_fifo_head(fifo)->data);
	} else /* if (req->type == SDO_REQ_UL) */ {
		if (sdo_fifo_length(fifo) == 0)
			return -1;

		int index = sdo_fifo_head(fifo)->index;
		int subindex = sdo_fifo_head(fifo)->subindex;

		legacy_driver_iface_process_sdo(node->driver, index, subindex,
						(unsigned char*)req->addr,
						req->pos);
	}

	sdo_fifo_dequeue(fifo);
	node->is_sdo_locked = 0;

	return schedule_sdo(node);
}

static void handle_not_loaded(struct canopen_node* node,
			      const struct canopen_msg* msg,
			      const struct can_frame* frame)
{
	switch (msg->object)
	{
	case CANOPEN_NMT:
		break; /* TODO: this is illegal */
	case CANOPEN_EMCY:
		handle_emcy(node, frame);
		break;
	case CANOPEN_HEARTBEAT:
		handle_heartbeat(node, frame);
		break;
	case CANOPEN_TSDO:
		frame_fifo_enqueue(&node->frame_fifo, frame);
		break;
	default:
		break;
	}
}

static void mux_handler_fn(int fd, void* context)
{
	(void)context;

	struct can_frame cf;
	struct canopen_msg msg;

	net_read_frame(fd, &cf, -1);

	if (canopen_get_object_type(&msg, &cf) < 0)
		return;

	assert(msg.id < 128);

	struct canopen_node* node = &node_[msg.id];
	void* driver = node->driver;

	if (!driver) {
		handle_not_loaded(node, &msg, &cf);
		return;
	}

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
		handle_tsdo(node, &cf);
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

static void run_bootup(void* context)
{
	(void)context;

	for (int i = 1; i < 128; ++i)
		if (nodes_seen_[i])
			load_driver(i);
}

static void on_bootup_done(void* context)
{
	(void)context;
	net__send_nmt(socket_, NMT_CS_START, 0);
	ignore_bootup_ = 0;
}

static void on_net_probe_done(void* context)
{
	(void)context;

	init_multiplexer();

	static struct ml_job job;
	ml_job_init(&job);
	job.work_fn = run_bootup;
	job.after_work_fn = on_bootup_done;
	ml_job_enqueue(&job);
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
	job.work_fn = run_net_probe;
	job.after_work_fn = on_net_probe_done;
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
	struct canopen_node* node = &node_[nodeid];

	if (sdo_fifo_enqueue(&node->sdo_fifo, SDO_ELEM_UL, index, subindex,
			     NULL) < 0)
		return -1;

	return schedule_sdo(node);
}

static int master_send_sdo(int nodeid, int index, int subindex,
			   unsigned char* data, size_t size)
{
	struct canopen_node* node = &node_[nodeid];

	struct sdo_elem_data* payload = malloc(sizeof(*payload) + size);
	if (!payload)
		return -1;

	payload->size = size;
	memcpy(payload->data, data, size);

	if (sdo_fifo_enqueue(&node->sdo_fifo, SDO_ELEM_DL, index, subindex,
			     payload) < 0)
		return -1;

	return schedule_sdo(node);
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

static void init_frame_queues()
{
	for (int i = 0; i < 128; ++i)
		frame_fifo_init(&node_[i].frame_fifo);
}

static void clear_frame_queues()
{
	for (int i = 0; i < 128; ++i)
		frame_fifo_clear(&node_[i].frame_fifo);
}

int main(int argc, char* argv[])
{
	int rc = 0;
	const char* iface = argv[1];

	memset(nodes_seen_, 0, sizeof(nodes_seen_));
	memset(node_, 0, sizeof(node_));

	init_frame_queues();

	socket_ = socketcan_open(iface);
	if (socket_ < 0) {
		perror("Could not open interface");
		return 1;
	}

	net_dont_block(socket_);
	net_fix_sndbuf(socket_);

	rc = master_iface_init();
	if (rc != 0)
		goto master_iface_failure;

	driver_manager_ = legacy_driver_manager_new();
	if (!driver_manager_) {
		rc = 1;
		goto driver_manager_failure;
	}

	if (worker_init(1, 1024, 1024*1024) != 0) {
		rc = 1;
		goto worker_failure;
	}

	ml_init();

	rc = run_appbase(&argc, argv);

	worker_deinit();

worker_failure:
	legacy_driver_manager_delete(driver_manager_);

driver_manager_failure:
	legacy_master_iface_delete(master_iface_);

master_iface_failure:
	if (socket_ >= 0)
		close(socket_);

	clear_frame_queues();

	return rc;
}

