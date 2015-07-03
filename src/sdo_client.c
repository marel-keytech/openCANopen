#include <assert.h>
#include "canopen/sdo.h"
#include "canopen/sdo_client.h"
#include "canopen.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#ifndef CAN_MAX_DLC
#define CAN_MAX_DLC 8
#endif

void sdo_proc__async_timeout(struct sdo_proc* self);

static int sdo_req_abort(struct sdo_req* self, enum sdo_abort_code code)
{
	struct can_frame* cf = &self->frame;

	sdo_abort(cf, SDO_ABORT_TOGGLE, self->index, self->subindex);

	self->state = SDO_REQ_ABORTED;
	self->have_frame = 1;

	return self->have_frame;
}

static void request_segmented_download(struct sdo_req* self, int index,
				       int subindex, const void* addr,
				       size_t size)
{
	struct can_frame* cf = &self->frame;

	self->state = SDO_REQ_INIT;

	self->addr = (char*)addr;
	self->size = size;

	cf->can_dlc = CAN_MAX_DLC;

	sdo_set_indicated_size(cf, size);
}

static void request_expediated_download(struct sdo_req* self, int index,
				        int subindex, const void* addr,
				        size_t size)
{
	struct can_frame* cf = &self->frame;

	self->state = SDO_REQ_INIT_EXPEDIATED;
	sdo_expediate(cf);
	sdo_set_expediated_size(cf, size);

	cf->can_dlc = SDO_EXPEDIATED_DATA_IDX + size;

	memcpy(&cf->data[SDO_EXPEDIATED_DATA_IDX], addr, size);
}

int sdo_request_download(struct sdo_req* self, int index, int subindex,
			 const void* addr, size_t size)
{
	memset(self, 0, sizeof(*self));
	struct can_frame* cf = &self->frame;

	self->type = SDO_REQ_DL;

	self->state = SDO_REQ_INIT;
	self->index = index;
	self->subindex = subindex;

	sdo_set_index(cf, index);
	sdo_set_subindex(cf, subindex);
	sdo_indicate_size(cf);

	sdo_set_cs(cf, SDO_CCS_DL_INIT_REQ);

	if (size > SDO_EXPEDIATED_DATA_SIZE)
		request_segmented_download(self, index, subindex, addr, size);
	else
		request_expediated_download(self, index, subindex, addr, size);

	self->have_frame = 1;
	return self->have_frame;
}

static int request_download_segment(struct sdo_req* self,
				    const struct can_frame* frame)
{
	struct can_frame* cf = &self->frame;

	sdo_clear_frame(cf);

	sdo_set_cs(cf, SDO_CCS_DL_SEG_REQ);

	if (self->is_toggled)
		sdo_toggle(cf);

	if (self->state != SDO_REQ_INIT)
		self->is_toggled ^= 1;

	size_t segment_size = MIN(SDO_SEGMENT_MAX_SIZE, self->size - self->pos);
	if (segment_size == 0)
		return -1;

	sdo_set_segment_size(cf, segment_size);

	memcpy(&cf->data[SDO_SEGMENT_IDX], &self->addr[self->pos],segment_size);

	cf->can_dlc = SDO_SEGMENT_IDX + segment_size;

	self->pos += segment_size;
	if (self->pos >= self->size) {
		self->state = SDO_REQ_END_SEGMENT;
		sdo_end_segment(cf);
	} else {
		self->state = SDO_REQ_SEG;
	}

	self->have_frame = 1;
	return self->have_frame;
}

int sdo_dl_req_feed(struct sdo_req* self, const struct can_frame* frame)
{
	if (sdo_get_cs(frame) == SDO_SCS_ABORT) {
		self->state = SDO_REQ_REMOTE_ABORT;
		self->have_frame = 0;
		return 0;
	}

	switch (self->state) {
	case SDO_REQ_INIT:
	case SDO_REQ_INIT_EXPEDIATED:
		if (sdo_get_cs(frame) != SDO_SCS_DL_INIT_RES)
			return sdo_req_abort(self, SDO_ABORT_INVALID_CS);

		if (self->state == SDO_REQ_INIT_EXPEDIATED) {
			self->state = SDO_REQ_DONE;
			self->have_frame = 0;
			return 0;
		}

		return request_download_segment(self, frame);

	case SDO_REQ_SEG:
		if (self->is_toggled != sdo_is_toggled(frame))
			return sdo_req_abort(self, SDO_ABORT_TOGGLE);

	case SDO_REQ_END_SEGMENT:
		if (sdo_get_cs(frame) != SDO_SCS_DL_SEG_RES)
			return sdo_req_abort(self, SDO_ABORT_INVALID_CS);

		if (self->state == SDO_REQ_END_SEGMENT) {
			self->state = SDO_REQ_DONE;
			self->have_frame = 0;
			return 0;
		}

		return request_download_segment(self, frame);

	default:
		break;
	}
	abort();

	return -1;
}

int sdo_request_upload(struct sdo_req* self, int index, int subindex)
{
	memset(self, 0, sizeof(*self));
	struct can_frame* cf = &self->frame;

	self->type = SDO_REQ_UL;

	self->state = SDO_REQ_INIT;
	self->index = index;
	self->subindex = subindex;
	self->indicated_size = -1;

	sdo_set_cs(cf, SDO_CCS_UL_INIT_REQ);
	sdo_set_index(cf, index);
	sdo_set_subindex(cf, subindex);
	cf->can_dlc = 4;

	self->have_frame = 1;
	return self->have_frame;
}

static int feed_expediated_init_upload_response(struct sdo_req* self,
						const struct can_frame* frame)
{
	if (sdo_is_size_indicated(frame))
		self->indicated_size = sdo_get_expediated_size(frame);

	size_t size = self->indicated_size > 0 ? self->indicated_size
					       : SDO_EXPEDIATED_DATA_SIZE;

	memcpy(self->addr, &frame->data[SDO_EXPEDIATED_DATA_IDX], size);

	self->state = SDO_REQ_DONE;

	self->have_frame = 0;
	return self->have_frame;
}

static int feed_segmented_init_upload_response(struct sdo_req* self,
					       const struct can_frame* frame)
{
	struct can_frame* cf = &self->frame;
	sdo_clear_frame(cf);

	if (sdo_is_size_indicated(frame))
		self->indicated_size = sdo_get_indicated_size(frame);

	self->state = SDO_REQ_SEG;
	sdo_set_cs(cf, SDO_CCS_UL_SEG_REQ);
	cf->can_dlc = 1;

	self->have_frame = 1;
	return self->have_frame;
}

static int feed_upload_segment_response(struct sdo_req* self,
					const struct can_frame* frame)
{
	struct can_frame* cf = &self->frame;

	size_t size = sdo_is_size_indicated(frame) ? sdo_get_segment_size(frame)
						   : SDO_SEGMENT_MAX_SIZE;

	if (!(self->size - self->pos >= size))
		return sdo_req_abort(self, SDO_ABORT_TOO_LONG);

	memcpy(&self->addr[self->pos], &frame->data[SDO_SEGMENT_IDX], size);

	self->pos += size;

	if (sdo_is_end_segment(frame)) {
		self->state = SDO_REQ_DONE;
		self->have_frame = 0;
		return self->have_frame;
	}

	sdo_clear_frame(cf);
	sdo_set_cs(cf, SDO_CCS_UL_SEG_REQ);

	self->is_toggled ^= 1;
	if (self->is_toggled)
		sdo_toggle(cf);

	cf->can_dlc = 1;

	self->have_frame = 1;
	return self->have_frame;
}

int sdo_ul_req_feed(struct sdo_req* self, const struct can_frame* frame)
{
	if (sdo_get_cs(frame) == SDO_SCS_ABORT) {
		self->state = SDO_REQ_REMOTE_ABORT;
		self->have_frame = 0;
		return 0;
	}

	switch (self->state) {
	case SDO_REQ_INIT:
		if (sdo_get_cs(frame) != SDO_SCS_UL_INIT_RES)
			return sdo_req_abort(self, SDO_ABORT_INVALID_CS);

		return sdo_is_expediated(frame)
		       ? feed_expediated_init_upload_response(self, frame)
		       : feed_segmented_init_upload_response(self, frame);

	case SDO_REQ_SEG:
		if (sdo_get_cs(frame) != SDO_SCS_UL_SEG_RES)
			return sdo_req_abort(self, SDO_ABORT_INVALID_CS);

		if (!sdo_is_end_segment(frame) &&
		    sdo_is_toggled(frame) != self->is_toggled)
			return sdo_req_abort(self, SDO_ABORT_TOGGLE);

		return feed_upload_segment_response(self, frame);
	default:
		break;
	}
	abort();

	return -1;
}

int sdo_req_feed(struct sdo_req* self, const struct can_frame* frame)
{
	switch (self->type)
	{
	case SDO_REQ_DL: return sdo_dl_req_feed(self, frame);
	case SDO_REQ_UL: return sdo_ul_req_feed(self, frame);
	case SDO_REQ_NONE: return -1;
	default: break;
	}
	abort();
	return -1;
}

static inline void sdo_proc__lock(struct sdo_proc* self)
{
#ifndef TESTING
	pthread_mutex_lock(&self->mutex);
#else
	assert(self->lock_owner != LOCK_OWNER_OTHER);
	self->lock_owner = LOCK_OWNER_THIS;
	self->lock_count++;
#endif
}

static inline int sdo_proc__trylock(struct sdo_proc* self)
{
#ifndef TESTING
	return pthread_mutex_trylock(&self->mutex);
#else
	if (self->lock_owner == LOCK_OWNER_OTHER)
		return -1;

	self->lock_owner = LOCK_OWNER_THIS;
	self->lock_count++;
	return 0;
#endif
}

static inline void sdo_proc__unlock(struct sdo_proc* self)
{
#ifndef TESTING
	pthread_mutex_unlock(&self->mutex);
#else
	assert(self->lock_owner == LOCK_OWNER_THIS);
	if (--self->lock_count <= 0)
		self->lock_owner = LOCK_OWNER_NONE;
#endif
}

int sdo_proc_init(struct sdo_proc* self)
{
	if (frame_fifo_init(&self->frame_input) < 0)
		return -1;

	if (ptr_fifo_init(&self->sdo_req_input) < 0)
		goto ptr_fifo_failure;

#ifndef TESTING
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&self->mutex, &attr);
#else
	self->lock_owner = LOCK_OWNER_NONE;
	self->lock_count = 0;
#endif

	self->do_set_timer_fn(self->async_timer, sdo_proc__async_timeout);

	return 0;

ptr_fifo_failure:
	frame_fifo_clear(&self->frame_input);
	return -1;
}

void sdo_proc_destroy(struct sdo_proc* self)
{
	if (self->current_req_data)
		free(self->current_req_data);
	ptr_fifo_destroy(&self->sdo_req_input);
	frame_fifo_clear(&self->frame_input);
#ifndef TESTING
	pthread_mutex_destroy(&self->mutex);
#endif
}

void sdo_proc_feed(struct sdo_proc* self, const struct can_frame* cf)
{
	frame_fifo_enqueue(&self->frame_input, cf);
}

void sdo_proc__write_req_frame(struct sdo_proc* self)
{
	/* TODO: reset the frame fifo before sending. In fact we don't need a
	 * fifo for the frames because there should only ever be one frame on
	 * the queue.
	 */
	struct sdo_req* req = &self->req;
	if (req->have_frame) {
		req->frame.can_id = R_RSDO + self->nodeid;
		self->do_write_frame(&req->frame);
	}
}

void sdo_proc__setup_dl_req(struct sdo_proc* self)
{
	struct sdo_req* req = &self->req;
	struct sdo_proc__req* data = self->current_req_data;

	sdo_request_download(req, data->index, data->subindex, data->data,
			     data->size);
	sdo_proc__write_req_frame(self);
}

void sdo_proc__setup_ul_req(struct sdo_proc* self)
{
	struct sdo_req* req = &self->req;
	struct sdo_proc__req* data = self->current_req_data;

	sdo_request_upload(req, data->index, data->subindex);
	sdo_proc__write_req_frame(self);

	req->pos = 0;
	req->addr = data->data;
	req->size = data->size;
}

void sdo_proc__try_next_request(struct sdo_proc* self)
{
	if (sdo_proc__trylock(self) != 0)
		return;

	if (self->req.state != SDO_REQ_EMPTY)
		goto done;

	assert(self->current_req_data == NULL);

	self->current_req_data = ptr_fifo_dequeue(&self->sdo_req_input);
	if (!self->current_req_data)
		goto done;

	switch (self->current_req_data->type)
	{
	case SDO_REQ_DL: sdo_proc__setup_dl_req(self); break;
	case SDO_REQ_UL: sdo_proc__setup_ul_req(self); break;
	default: abort(); break;
	}

	self->is_new_req = 1; /* tell main thread to keep the lock */

done:
	sdo_proc__unlock(self);
}

void sdo_proc__async_done(struct sdo_proc* self)
{
	assert(self->current_req_data);

	sdo_proc__stop_timer(self);

	sdo_proc_fn on_done = self->current_req_data->on_done;
	if (on_done)
		on_done(self);

	free(self->current_req_data);
	self->current_req_data = NULL;
	memset(&self->req, 0, sizeof(self->req));
	sdo_proc__try_next_request(self);
	sdo_proc__unlock(self);
}

void sdo_proc__async_timeout(struct sdo_proc* self)
{
	struct can_frame cf;
	struct sdo_req* req = &self->req;
	sdo_clear_frame(&cf);
	sdo_abort(&cf, SDO_ABORT_TIMEOUT, req->index, req->subindex);
	cf.can_id = R_RSDO + self->nodeid;
	self->do_write_frame(&cf);

	sdo_proc__async_done(self);
}

void sdo_proc__process_frame(struct sdo_proc* self, const struct can_frame* cf)
{
	struct sdo_req* req = &self->req;

	if (self->req.state == SDO_REQ_EMPTY)
		return;

	assert(self->current_req_data != NULL);

	sdo_req_feed(req, cf);

	sdo_proc__write_req_frame(self);

	if (sdo_proc__is_req_done(self))
		sdo_proc__async_done(self);
	else
		sdo_proc__restart_timer(self);
}


/* This must be run again later if it fails to lock */
int sdo_proc_run(struct sdo_proc* self)
{
	if (sdo_proc__trylock(self) != 0)
		return -1;

	struct frame_fifo* fifo = &self->frame_input;
	struct can_frame cf;

	if (self->req.state == SDO_REQ_EMPTY)
		goto done;

	assert(self->current_req_data != NULL);

	/* lock must be held by main thread until done/timeout */
	if (self->is_new_req) {
		assert(self->req.state == SDO_REQ_INIT
		    || self->req.state == SDO_REQ_INIT_EXPEDIATED);
		sdo_proc__lock(self);
		sdo_proc__start_timer(self);
		self->is_new_req = 0;
	}

	while (frame_fifo_dequeue(fifo, &cf, 0) >= 0)
		sdo_proc__process_frame(self, &cf);

done:
	sdo_proc__unlock(self);
	return 0;
}

int sdo_proc_async_read(struct sdo_proc* self, const struct sdo_info* info)
{
	size_t size = 1024; /* TODO: make dynamic */
	struct sdo_proc__req* req = malloc(sizeof(*req) + size);
	if (!req)
		return -1;

	memset(req, 0, sizeof(*req) + size);

	req->type = SDO_REQ_UL;
	req->index = info->index;
	req->subindex = info->subindex;
	req->on_done = info->on_done;
	req->timeout = info->timeout;
	req->size = size;

	ptr_fifo_enqueue(&self->sdo_req_input, req, free);

	sdo_proc__try_next_request(self);
	return 0;
}

int sdo_proc_async_write(struct sdo_proc* self, const struct sdo_info* info)
{
	struct sdo_proc__req* req = malloc(sizeof(*req) + info->size);
	if (!req)
		return -1;

	memset(req, 0, sizeof(*req) + info->size);

	req->type = SDO_REQ_DL;
	req->index = info->index;
	req->subindex = info->subindex;
	req->on_done = info->on_done;
	req->timeout = info->timeout;
	req->size = info->size;

	memcpy(req->data, info->addr, req->size);

	ptr_fifo_enqueue(&self->sdo_req_input, req, free);

	sdo_proc__try_next_request(self);
	return 0;
}

int sdo_proc__sync_timeout(struct sdo_proc* self)
{
	struct can_frame cf;
	struct sdo_req* req = &self->req;
	sdo_clear_frame(&cf);
	sdo_abort(&cf, SDO_ABORT_TIMEOUT, req->index, req->subindex);
	cf.can_id = R_RSDO + self->nodeid;
	self->do_write_frame(&cf);
	return -1;
}

ssize_t sdo_proc__process_request(struct sdo_proc* self, int timeout)
{
	struct can_frame cf;
	struct sdo_req* req = &self->req;
	struct frame_fifo* fifo = &self->frame_input;

	while (1) {
		if (frame_fifo_dequeue(fifo, &cf, timeout) < 0)
			return sdo_proc__sync_timeout(self);

		sdo_req_feed(req, &cf);

		sdo_proc__write_req_frame(self);

		if (req->state == SDO_REQ_DONE)
			break;

		if (req->state < 0)
			return -1;
	}

	return req->pos;
}

ssize_t sdo_proc_sync_read(struct sdo_proc* self, struct sdo_info* info)
{
	struct sdo_req* req = &self->req;

	sdo_proc__lock(self);

	sdo_request_upload(req, info->index, info->subindex);
	sdo_proc__write_req_frame(self);

	req->pos = 0;
	req->addr = info->addr;
	req->size = info->size;

	int rc = sdo_proc__process_request(self, info->timeout);

	req->state = SDO_REQ_EMPTY;

	sdo_proc__unlock(self);
	return rc;
}

ssize_t sdo_proc_sync_write(struct sdo_proc* self, const struct sdo_info* info)
{
	struct sdo_req* req = &self->req;

	sdo_proc__lock(self);

	sdo_request_download(req, info->index, info->subindex, info->addr,
			     info->size);
	sdo_proc__write_req_frame(self);

	int rc = sdo_proc__process_request(self, info->timeout);

	req->state = SDO_REQ_EMPTY;

	sdo_proc__unlock(self);
	return rc;
}

