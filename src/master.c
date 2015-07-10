#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
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

#include "legacy-driver.h"

#define SDO_FIFO_LENGTH 64

#define SDO_TIMEOUT 100 /* ms */
#define HEARTBEAT_PERIOD 10000 /* ms */
#define HEARTBEAT_TIMEOUT 11000 /* ms */

static int socket_ = -1;
static char nodes_seen_[128];

typedef void (*void_cb_fn)(void*);

enum master_state {
	MASTER_STATE_STARTUP = 0,
	MASTER_STATE_RUNNING
};

static enum master_state master_state_ = MASTER_STATE_STARTUP;

static void* driver_manager_;

static struct ml_handler mux_handler_;

struct canopen_node;

struct canopen_node {
	void* driver;
	void* master_iface;

	struct frame_fifo frame_fifo;

	struct sdo_proc sdo_proc;
	struct ml_timer sdo_timer;

	int device_type;
	int is_heartbeat_supported;

	struct ml_timer heartbeat_timer;
	struct ml_timer ping_timer;
};

struct driver_load_job {
	struct ml_job job;
	int nodeid;
};

struct ping_job {
	struct ml_job job;
	int nodeid;
	int ok;
};

static void* master_iface_init(int nodeid);
static int master_request_sdo(int nodeid, int index, int subindex);
static int master_send_sdo(int nodeid, int index, int subindex,
			   unsigned char* data, size_t size);
static int send_pdo(int nodeid, int type, unsigned char* data, size_t size);
static int master_send_pdo(int nodeid, int n, unsigned char* data, size_t size);

static struct canopen_node node_[128];

static inline int get_node_id(const struct canopen_node* node)
{
	return ((char*)node - (char*)node_) / sizeof(struct canopen_node);
}

static inline struct canopen_node*
get_node_from_proc_req(struct sdo_proc_req* proc)
{
	void* addr = (char*)proc - offsetof(struct canopen_node, sdo_proc);
	return addr;
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

uint32_t get_device_type(int nodeid)
{
	struct canopen_node* node = &node_[nodeid];
	uint32_t device_type = 0;

	struct sdo_info sdo_info = {
		.index = 0x1000,
		.subindex = 0,
		.timeout = SDO_TIMEOUT,
		.addr = &device_type,
		.size = sizeof(device_type)
	};

	if (sdo_proc_sync_read(&node->sdo_proc, &sdo_info) < 0)
		return 0;

	return device_type;
}

int node_supports_heartbeat(int nodeid)
{
	struct canopen_node* node = &node_[nodeid];
	uint16_t dummy;

	struct sdo_info sdo_info = {
		.index = 0x1017,
		.subindex = 0,
		.timeout = SDO_TIMEOUT,
		.addr = &dummy,
		.size = sizeof(dummy)
	};

	return sdo_proc_sync_read(&node->sdo_proc, &sdo_info) < 0 ? 0 : 1;
}

static inline int set_heartbeat_period(int nodeid, uint16_t period)
{
	struct canopen_node* node = &node_[nodeid];

	struct sdo_info sdo_info = {
		.index = 0x1017,
		.subindex = 0,
		.timeout = SDO_TIMEOUT,
		.addr = &period,
		.size = sizeof(period)
	};

	return sdo_proc_sync_write(&node->sdo_proc, &sdo_info);
}

const char* get_name(int nodeid)
{
	struct canopen_node* node = &node_[nodeid];
	static __thread char name[256];

	struct sdo_info sdo_info = {
		.index = 0x1008,
		.subindex = 0,
		.timeout = SDO_TIMEOUT,
		.addr = name,
		.size = sizeof(name)
	};

	if (sdo_proc_sync_read(&node->sdo_proc, &sdo_info) < 0)
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

	stop_heartbeat_timer(nodeid);

	if (!node->is_heartbeat_supported)
		stop_ping_timer(nodeid);

	legacy_driver_delete_handler(driver_manager_, node->device_type,
				     node->driver);

	legacy_master_iface_delete(node->master_iface);

	node->driver = NULL;
	node->master_iface = NULL;
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

static void on_ping_timeout(void* context)
{
	struct canopen_node* node = context;
	int nodeid = get_node_id(node);

	struct can_frame cf = { 0 };

	cf.can_id = R_HEARTBEAT + nodeid;
	cf.can_dlc = 1;
	heartbeat_set_state(&cf, 1);

	net_write_frame(socket_, &cf, -1);
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

void start_sdo_timer(void* timer, int timeout)
{
	struct ml_timer* x = timer;
	x->timeout = timeout,
	ml_timer_start(timer);
}

void stop_sdo_timer(void* timer)
{
	ml_timer_stop(timer);
}

void set_sdo_timeout_fn(void* timer, sdo_proc_req_fn fn)
{
	struct ml_timer* x = timer;
	x->fn = (void_cb_fn)fn;
}

ssize_t write_frame(const struct can_frame* frame)
{
	return net_write_frame(socket_, frame, -1);
}

static int load_driver(int nodeid)
{
	struct canopen_node* node = &node_[nodeid];

	if (node->driver)
		unload_driver(nodeid);

	void* driver = NULL;

	uint32_t device_type = get_device_type(nodeid);
	if (device_type == 0)
		goto failure;

	const char* name = get_name(nodeid);
	if (!name)
		goto failure;

	void* master_iface = master_iface_init(nodeid);
	if (!master_iface)
		goto failure;

	if (legacy_driver_manager_create_handler(driver_manager_, name,
						 device_type, master_iface,
						 &driver) < 0)
		goto driver_create_failure;

	node->driver = driver;
	node->master_iface = master_iface;

	if (legacy_driver_iface_initialize(driver) < 0)
		goto driver_init_failure;

	legacy_driver_iface_process_node_state(driver, 1);

	int is_heartbeat_supported =
		set_heartbeat_period(nodeid, HEARTBEAT_PERIOD) >= 0;

	if (!is_heartbeat_supported)
		start_ping_timer(nodeid, HEARTBEAT_PERIOD);

	node->device_type = device_type;
	node->is_heartbeat_supported = is_heartbeat_supported;

	if (master_state_ > MASTER_STATE_STARTUP)
		net__send_nmt(socket_, NMT_CS_START, nodeid);

	return 0;

driver_init_failure:
	legacy_driver_delete_handler(driver_manager_, device_type, driver);
driver_create_failure:
	legacy_master_iface_delete(master_iface);

	node->driver = NULL;
	node->master_iface = NULL;
failure:

	return -1;
}

static void run_net_probe(void* context)
{
	(void)context;

	net_reset(socket_, nodes_seen_, 100);
	net_probe(socket_, nodes_seen_, 1, 127, 100);
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

	if (!node->driver)
		return -1;

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
	if (!heartbeat_is_valid(frame))
		return -1;

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

static void handle_sdo(struct canopen_node* node, const struct can_frame* cf)
{
	struct sdo_proc* sdo_proc = &node->sdo_proc;
	sdo_proc_feed(sdo_proc, cf);
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
		handle_sdo(node, frame);
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
		handle_sdo(node, &cf);
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


static void on_master_sdo_request_done(struct sdo_proc_req* proc_req)
{
	struct canopen_node* node = get_node_from_proc_req(proc_req);
	void* driver = node->driver;

	if (proc_req->rc >= 0) {
		legacy_driver_iface_process_sdo(driver,
						proc_req->index,
						proc_req->subindex,
						(unsigned char*)proc_req->data,
						proc_req->rc);
	} else {
		legacy_driver_iface_process_sdo(driver,
						proc_req->index,
						proc_req->subindex,
						NULL, 0);
	}
}

static int master_request_sdo(int nodeid, int index, int subindex)
{
	struct canopen_node* node = &node_[nodeid];

	struct sdo_info sdo_info = {
		.index = index,
		.subindex = subindex,
		.timeout = SDO_TIMEOUT,
		.on_done = on_master_sdo_request_done
	};

	return sdo_proc_async_read(&node->sdo_proc, &sdo_info) == NULL ? -1 : 0;
}

static void on_master_sdo_send_done(struct sdo_proc_req* proc_req)
{
	(void)proc_req;
}

static int master_send_sdo(int nodeid, int index, int subindex,
			   unsigned char* data, size_t size)
{
	struct canopen_node* node = &node_[nodeid];

	struct sdo_info sdo_info = {
		.index = index,
		.subindex = subindex,
		.timeout = SDO_TIMEOUT,
		.addr = data,
		.size = size,
		.on_done = on_master_sdo_send_done
	};

	return sdo_proc_async_write(&node->sdo_proc, &sdo_info) == NULL ? -1 :0;
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

static int init_single_sdo_proc(int nodeid)
{
	struct canopen_node* node = &node_[nodeid];

	ml_timer_init(&node->sdo_timer);

	struct sdo_proc* sdo_proc = &node->sdo_proc;
	memset(sdo_proc, 0, sizeof(*sdo_proc));

	node->sdo_timer.context = sdo_proc;

	sdo_proc->nodeid = nodeid;
	sdo_proc->async_timer = &node->sdo_timer;
	sdo_proc->do_start_timer = start_sdo_timer;
	sdo_proc->do_stop_timer = stop_sdo_timer;
	sdo_proc->do_set_timer_fn = set_sdo_timeout_fn;
	sdo_proc->do_write_frame = write_frame;

	return sdo_proc_init(sdo_proc);
}

static int init_sdo_processors()
{
	int i;
	for (i = 1; i < 128; ++i)
		if (init_single_sdo_proc(i) < 0)
			goto failure;
	return 0;

failure:
	for (; i > 0; --i) {
		struct canopen_node* node = &node_[i];
		sdo_proc_destroy(&node->sdo_proc);
		ml_timer_clear(&node->sdo_timer);
	}
	return -1;
}

static void destroy_sdo_processors()
{
	for (int i = 1; i < 128; ++i) {
		struct canopen_node* node = &node_[i];
		sdo_proc_destroy(&node->sdo_proc);
		ml_timer_clear(&node->sdo_timer);
	}
}

int main(int argc, char* argv[])
{
	int rc = 0;
	const char* iface = argv[1];

	memset(nodes_seen_, 0, sizeof(nodes_seen_));
	memset(node_, 0, sizeof(node_));

	if (init_sdo_processors() < 0)
		return -1;

	socket_ = socketcan_open(iface);
	if (socket_ < 0) {
		perror("Could not open interface");
		goto iface_failure;
	}

	net_dont_block(socket_);
	net_fix_sndbuf(socket_);

	driver_manager_ = legacy_driver_manager_new();
	if (!driver_manager_) {
		rc = 1;
		goto driver_manager_failure;
	}

	if (worker_init(4, 1024, 4096) != 0) {
		rc = 1;
		goto worker_failure;
	}

	ml_init();

	rc = run_appbase(&argc, argv);

	worker_deinit();

worker_failure:
	legacy_driver_manager_delete(driver_manager_);

driver_manager_failure:
	if (socket_ >= 0)
		close(socket_);

iface_failure:
	destroy_sdo_processors();

	return rc;
}

