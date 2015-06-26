#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <appcbase.h>
#include <ctype.h>

#include "main-loop.h"
#include "socketcan.h"
#include "canopen.h"
#include "canopen/sdo.h"
#include "canopen/sdo_client.h"
#include "canopen/network.h"
#include "canopen/nmt.h"
#include "canopen/heartbeat.h"
#include "canopen/emcy.h"
#include "frame_fifo.h"

#include "legacy-driver.h"

#define SDO_FIFO_LENGTH 64

#define HEARTBEAT_PERIOD 10000 /* ms */
#define HEARTBEAT_TIMEOUT 11000 /* ms */

static int socket_ = -1;
static char nodes_seen_[128];

enum master_state {
	MASTER_STATE_STARTUP = 0,
	MASTER_STATE_RUNNING
};

static enum master_state master_state_ = MASTER_STATE_STARTUP;

static void* master_iface_;
static void* driver_manager_;

static struct ml_handler mux_handler_;

struct canopen_node {
	void* driver;
	struct frame_fifo frame_fifo;
	struct sdo_req sdo_req;
	pthread_mutex_t sdo_channel;
	int device_type;
	int is_heartbeat_supported;
	struct ml_timer heartbeat_timer;
	struct ml_timer ping_timer;
};

struct driver_load_job {
	struct ml_job job;
	int nodeid;
};

enum sdo_job_type { SDO_JOB_DL, SDO_JOB_UL };

struct sdo_job {
	struct ml_job job;
	enum sdo_job_type type;
	int nodeid;
	int index;
	int subindex;
	unsigned char* data;
	size_t size;
};

struct ping_job {
	struct ml_job job;
	int nodeid;
	int ok;
};

static struct canopen_node node_[128];

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

void sdo_channel_lock(int nodeid)
{
	struct canopen_node* node = &node_[nodeid];
	pthread_mutex_lock(&node->sdo_channel);
}

void sdo_channel_unlock(int nodeid)
{
	struct canopen_node* node = &node_[nodeid];
	pthread_mutex_unlock(&node->sdo_channel);
}

int sdo_timeout(int nodeid, int index, int subindex)
{
	struct can_frame cf;
	sdo_clear_frame(&cf);
	sdo_abort(&cf, SDO_ABORT_TIMEOUT, index, subindex);
	cf.can_id = R_RSDO + nodeid;
	net_write_frame(socket_, &cf, -1);
	return -1;
}

int sdo_process_request(int nodeid, struct frame_fifo* fifo,
			struct sdo_req* req)
{
	struct can_frame cf;

	while (1) {
		if (frame_fifo_dequeue(fifo, &cf, 100) < 0)
			return sdo_timeout(nodeid, req->index, req->subindex);

		sdo_req_feed(req, &cf);

		if (req->have_frame) {
			req->frame.can_id = R_RSDO + nodeid;
			net_write_frame(socket_, &req->frame, -1);
		}

		if (req->state == SDO_REQ_DONE)
			break;

		if (req->state < 0)
			return -1;
	}

	return req->pos;
}

ssize_t sdo_read_fifo(int nodeid, int index, int subindex, void* buf,
		      size_t size)
{
	struct sdo_req req;
	struct canopen_node* node = &node_[nodeid];
	struct frame_fifo* fifo = &node->frame_fifo;

	sdo_channel_lock(nodeid);

	sdo_request_upload(&req, index, subindex);
	req.frame.can_id = R_RSDO + nodeid;
	net_write_frame(socket_, &req.frame, -1);

	req.pos = 0;
	req.addr = buf;
	req.size = size;

	int rc = sdo_process_request(nodeid, fifo, &req);

	sdo_channel_unlock(nodeid);

	return rc;
}

ssize_t sdo_write_fifo(int nodeid, int index, int subindex, const void* buf,
		       size_t size)
{
	struct sdo_req req;
	struct canopen_node* node = &node_[nodeid];
	struct frame_fifo* fifo = &node->frame_fifo;

	sdo_channel_lock(nodeid);

	sdo_request_download(&req, index, subindex, buf, size);
	req.frame.can_id = R_RSDO + nodeid;
	net_write_frame(socket_, &req.frame, -1);

	int rc = sdo_process_request(nodeid, fifo, &req);

	sdo_channel_unlock(nodeid);

	return rc;
}

uint32_t get_device_type(int nodeid)
{
	uint32_t device_type = 0;

	if (sdo_read_fifo(nodeid, 0x1000, 0, &device_type,
			  sizeof(device_type)) < 0)
		return 0;

	return device_type;
}

int node_supports_heartbeat(int nodeid)
{
	uint16_t dummy;

	if (sdo_read_fifo(nodeid, 0x1017, 0, &dummy, sizeof(dummy)) < 0)
		return 0;

	return 1;
}

static inline int set_heartbeat_period(int nodeid, uint16_t period)
{
	return sdo_write_fifo(nodeid, 0x1017, 0, &period, sizeof(period));
}

const char* get_name(int nodeid)
{
	static __thread char name[256];

	if (sdo_read_fifo(nodeid, 0x1008, 0, name, sizeof(name)) < 0)
		return NULL;

	clean_node_name(name, sizeof(name));

	return name;
}

static void stop_heartbeat_timer(int nodeid)
{
	struct canopen_node* node = &node_[nodeid];
	struct ml_timer* timer = &node->heartbeat_timer;
	ml_timer_stop(timer);
}

static void stop_ping_timer(int nodeid)
{
	struct canopen_node* node = &node_[nodeid];
	struct ml_timer* timer = &node->ping_timer;
	ml_timer_stop(timer);
}

static void unload_driver(int nodeid)
{
	struct canopen_node* node = &node_[nodeid];

	if (node->is_heartbeat_supported)
		stop_heartbeat_timer(nodeid);
	else
		stop_ping_timer(nodeid);

	legacy_driver_delete_handler(driver_manager_, node->device_type,
				     node->driver);
	node->driver = NULL;
	node->device_type = 0;
	node->is_heartbeat_supported = 0;


	net__send_nmt(socket_, NMT_CS_RESET_NODE, nodeid);
}

static void on_heartbeat_timeout(void* context)
{
	struct canopen_node* node = context;
	unload_driver(get_node_id(node));
}

static int start_heartbeat_timer(int nodeid)
{
	struct canopen_node* node = &node_[nodeid];
	struct ml_timer* timer = &node->heartbeat_timer;

	ml_timer_init(timer);

	timer->is_periodic = 0;
	timer->context = node;
	timer->timeout = HEARTBEAT_TIMEOUT;
	timer->fn = on_heartbeat_timeout;

	ml_timer_start(timer);

	return 0;
}

static int restart_heartbeat_timer(int nodeid)
{
	stop_heartbeat_timer(nodeid);
	return start_heartbeat_timer(nodeid);
}

static void run_ping_job(void* context)
{
	struct ping_job* job = context;

	uint32_t dummy;
	if (sdo_read_fifo(job->nodeid, 0x1000, 0, &dummy, sizeof(dummy)) < 0)
		job->ok = 0;
	else
		job->ok = 1;
}

static void on_ping_job_done(void* context)
{
	struct ping_job* job = context;

	if (!job->ok)
		unload_driver(job->nodeid);
}

static int schedule_ping_job(int nodeid)
{
	struct ping_job* ping_job = malloc(sizeof(*ping_job));
	if (!ping_job)
		return -1;

	struct ml_job* job = (struct ml_job*)ping_job;

	ml_job_init(job);

	job->priority = 127 + nodeid;
	job->context = job;
	job->work_fn = run_ping_job;
	job->after_work_fn = on_ping_job_done;
	ping_job->nodeid = nodeid;
	ping_job->ok = 0;

	if (ml_job_enqueue(job) < 0)
		goto failure;

	return 0;

failure:
	ml_job_clear(job);
	free(job);
	return -1;
}

static void on_ping_timeout(void* context)
{
	struct canopen_node* node = context;
	schedule_ping_job(get_node_id(node));
}

static int start_ping_timer(int nodeid, int period)
{
	struct canopen_node* node = &node_[nodeid];
	struct ml_timer* timer = &node->ping_timer;

	ml_timer_init(timer);

	timer->is_periodic = 1;
	timer->context = node;
	timer->timeout = period;
	timer->fn = on_ping_timeout;

	ml_timer_start(timer);

	return 0;
}

static int load_driver(int nodeid)
{
	struct canopen_node* node = &node_[nodeid];

	if (node->driver)
		unload_driver(nodeid);

	void* driver = NULL;

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

	int is_heartbeat_supported = node_supports_heartbeat(nodeid);
	if (is_heartbeat_supported)
		set_heartbeat_period(nodeid, HEARTBEAT_PERIOD);
	else
		start_ping_timer(nodeid, HEARTBEAT_PERIOD);

	node->driver = driver;
	node->device_type = device_type;
	node->is_heartbeat_supported = is_heartbeat_supported;

	if (master_state_ > MASTER_STATE_STARTUP)
		net__send_nmt(socket_, NMT_CS_START, nodeid);

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
	if (master_state_ == MASTER_STATE_STARTUP)
		return 0;

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

	if (master_state_ == MASTER_STATE_STARTUP)
		return 0;

	int nodeid = get_node_id(node);

	restart_heartbeat_timer(nodeid);

	/* Make sure the node is in operational state */
	if (heartbeat_get_state(frame) != NMT_STATE_OPERATIONAL)
		net__send_nmt(socket_, NMT_CS_START, nodeid);

	return 0;
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
		frame_fifo_enqueue(&node->frame_fifo, &cf);
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
	master_state_ = MASTER_STATE_RUNNING;
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

static void run_ul_sdo_job(struct sdo_job* job)
{
	sdo_read_fifo(job->nodeid, job->index, job->subindex, job->data,
		      job->size);
}

static void run_dl_sdo_job(struct sdo_job* job)
{
	sdo_write_fifo(job->nodeid, job->index, job->subindex, job->data,
		       job->size);
}

static void run_sdo_job(void* context)
{
	struct sdo_job* job = context;

	switch (job->type)
	{
	case SDO_JOB_DL:
		run_dl_sdo_job(job);
		break;
	case SDO_JOB_UL:
		run_ul_sdo_job(job);
		break;
	}

	abort();
}

static void on_sdo_job_done(void* context)
{
	struct sdo_job* job = context;
	struct canopen_node* node = &node_[job->nodeid];

	if (job->type == SDO_JOB_UL) {
		legacy_driver_iface_process_sdo(node->driver, job->index,
						job->subindex, job->data,
						job->size);
		free(job->data);
	}

	free(job);
}

static int schedule_sdo_job(int nodeid, enum sdo_job_type type, int index,
			    int subindex, unsigned char* data, size_t size)
{
	struct sdo_job* sdo_job = malloc(sizeof(*sdo_job));
	if (!sdo_job)
		return -1;

	struct ml_job* job = (struct ml_job*)sdo_job;

	ml_job_init(job);

	job->priority = nodeid;
	job->context = job;
	job->work_fn = run_sdo_job;
	job->after_work_fn = on_sdo_job_done;

	sdo_job->type = type;
	sdo_job->nodeid = nodeid;
	sdo_job->index = index;
	sdo_job->subindex = subindex;

	if (type == SDO_JOB_DL) {
		assert(data);

		sdo_job->data = data;
		sdo_job->size = size;
	} else {
		sdo_job->size = 1024;
		sdo_job->data = malloc(sdo_job->size);

		if (!sdo_job->data)
			goto failure;
	}

	if (ml_job_enqueue(job) < 0)
		goto failure;

	return 0;

failure:
	if (type == SDO_JOB_UL)
		free(sdo_job->data);

	ml_job_clear(job);
	free(job);
	return -1;
}

static int master_request_sdo(int nodeid, int index, int subindex)
{
	return schedule_sdo_job(nodeid, SDO_JOB_UL, index, subindex, NULL, 0);
}

static int master_send_sdo(int nodeid, int index, int subindex,
			   unsigned char* data, size_t size)
{
	return schedule_sdo_job(nodeid, SDO_JOB_DL, index, subindex, data,
				subindex);
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

	if (worker_init(4, 1024, 1024*1024) != 0) {
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

