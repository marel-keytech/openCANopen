#ifndef _CANOPEN_SDO_CLIENT_H
#define _CANOPEN_SDO_CLIENT_H

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <linux/can.h>
#include <mloop.h>

#include <pthread.h>

#include "frame_fifo.h"
#include "ptr_fifo.h"

enum sdo_req_state {
	SDO_REQ_REMOTE_ABORT = -2,
	SDO_REQ_ABORTED = -1,
	SDO_REQ_EMPTY = 0,
	SDO_REQ_INIT,
	SDO_REQ_INIT_EXPEDIATED,
	SDO_REQ_SEG,
	SDO_REQ_END_SEGMENT,
	SDO_REQ_DONE,
};

enum sdo_req_type { SDO_REQ_NONE = 0, SDO_REQ_DL, SDO_REQ_UL };

struct sdo_req {
	enum sdo_req_type type;
	enum sdo_req_state state;
	struct can_frame frame;
	int is_toggled;

	int index;
	int subindex;

	ssize_t indicated_size;

	char* addr;
	size_t size;
	int pos;

	int have_frame;
};

struct sdo_proc_req;

typedef void (*sdo_proc_req_fn)(struct sdo_proc_req*);

struct sdo_proc_req {
	struct sdo_proc* parent;
	enum sdo_req_type type;
	int index, subindex;
	int timeout;
	sdo_proc_req_fn on_done;
	ssize_t rc;
	int is_done;
	size_t size;
	char data[0];
};

struct sdo_proc {
	int nodeid;
	pthread_mutex_t mutex;

	pthread_cond_t suspend_cond;

	struct ptr_fifo sdo_req_input;

	struct sdo_req req;
	struct sdo_proc_req* current_req_data;

	struct mloop* mloop;
	struct mloop_timer* async_timer;

	ssize_t (*do_write_frame)(const struct can_frame* cf);
};

struct sdo_info {
	int index, subindex;
	void* addr;
	size_t size;
	int timeout;
	sdo_proc_req_fn on_done;
};

int sdo_request_download(struct sdo_req* self, int index, int subindex,
			 const void* addr, size_t size);

int sdo_request_upload(struct sdo_req* self, int index, int subindex);

int sdo_dl_req_next_frame(struct sdo_req* self, struct can_frame* out);
int sdo_ul_req_next_frame(struct sdo_req* self, struct can_frame* out);

int sdo_dl_req_feed(struct sdo_req* self, const struct can_frame* frame);
int sdo_ul_req_feed(struct sdo_req* self, const struct can_frame* frame);

int sdo_req_feed(struct sdo_req* self, const struct can_frame* frame);

int sdo_proc_init(struct sdo_proc* self);
void sdo_proc_destroy(struct sdo_proc* self);

void sdo_proc_feed(struct sdo_proc* self, const struct can_frame* frame);
int sdo_proc_run(struct sdo_proc* self);

ssize_t sdo_proc_sync_read(struct sdo_proc* self, struct sdo_info* args);
ssize_t sdo_proc_sync_write(struct sdo_proc* self, struct sdo_info* args);

struct sdo_proc_req* sdo_proc_async_read(struct sdo_proc* self,
					 const struct sdo_info* args);
struct sdo_proc_req* sdo_proc_async_write(struct sdo_proc* self,
					  const struct sdo_info* args);

void sdo_proc__setup_dl_req(struct sdo_proc* self);
void sdo_proc__setup_ul_req(struct sdo_proc* self);
void sdo_proc__try_next_request(struct sdo_proc* self);
void sdo_proc__async_done(struct sdo_proc* self);
void sdo_proc__async_timeout(struct sdo_proc* self);

void sdo_proc__process_frame(struct sdo_proc* self, const struct can_frame* cf);

static inline int sdo_proc__is_req_done(struct sdo_proc* self)
{
	return self->req.state == SDO_REQ_DONE || self->req.state < 0;
}

static inline void sdo_proc__start_timer(struct sdo_proc* self)
{
	assert(self->current_req_data);
	int timeout = self->current_req_data->timeout;
	mloop_timer_set_context(self->async_timer, self->current_req_data,
				NULL);
	mloop_timer_set_time(self->async_timer, timeout * 1000000LL);
	mloop_timer_start(self->async_timer);
}

static inline void sdo_proc__stop_timer(struct sdo_proc* self)
{
	mloop_timer_stop(self->async_timer);
}

static inline void sdo_proc__restart_timer(struct sdo_proc* self)
{
	sdo_proc__stop_timer(self);
	sdo_proc__start_timer(self);
}

#endif /* _CANOPEN_SDO_CLIENT_H */

