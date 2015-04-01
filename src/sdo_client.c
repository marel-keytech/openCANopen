#include <assert.h>
#include "canopen/sdo.h"
#include "canopen/sdo_client.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#ifndef CAN_MAX_DLC
#define CAN_MAX_DLC 8
#endif

static int sdo_dl_req_abort(struct sdo_dl_req* self, enum sdo_abort_code code)
{
	struct can_frame* cf = &self->frame;

	sdo_abort(cf, SDO_ABORT_TOGGLE, self->index, self->subindex);

	self->state = SDO_REQ_ABORTED;
	self->have_frame = 1;

	return self->have_frame;
}

static int sdo_ul_req_abort(struct sdo_ul_req* self, enum sdo_abort_code code)
{
	struct can_frame* cf = &self->frame;

	sdo_abort(cf, SDO_ABORT_TOGGLE, self->index, self->subindex);

	self->state = SDO_REQ_ABORTED;
	self->have_frame = 1;

	return self->have_frame;
}

static void request_segmented_download(struct sdo_dl_req* self, int index,
				       int subindex, const void* addr,
				       size_t size)
{
	struct can_frame* cf = &self->frame;

	self->state = SDO_REQ_INIT;

	self->addr = addr;
	self->size = size;

	cf->can_dlc = CAN_MAX_DLC;

	sdo_set_indicated_size(cf, size);
}

static void request_expediated_download(struct sdo_dl_req* self, int index,
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

int sdo_request_download(struct sdo_dl_req* self, int index, int subindex,
			 const void* addr, size_t size)
{
	memset(self, 0, sizeof(*self));
	struct can_frame* cf = &self->frame;

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

static int request_download_segment(struct sdo_dl_req* self,
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
	assert(segment_size > 0);
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

int sdo_dl_req_feed(struct sdo_dl_req* self, const struct can_frame* frame)
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
			return sdo_dl_req_abort(self, SDO_ABORT_INVALID_CS);

		if (self->state == SDO_REQ_INIT_EXPEDIATED) {
			self->state = SDO_REQ_DONE;
			self->have_frame = 0;
			return 0;
		}

		return request_download_segment(self, frame);

	case SDO_REQ_SEG:
		if (self->is_toggled != sdo_is_toggled(frame))
			return sdo_dl_req_abort(self, SDO_ABORT_TOGGLE);

	case SDO_REQ_END_SEGMENT:
		if (sdo_get_cs(frame) != SDO_SCS_DL_SEG_RES)
			return sdo_dl_req_abort(self, SDO_ABORT_INVALID_CS);

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

int sdo_request_upload(struct sdo_ul_req* self, int index, int subindex)
{
	memset(self, 0, sizeof(*self));
	struct can_frame* cf = &self->frame;

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

static int feed_expediated_init_upload_response(struct sdo_ul_req* self,
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

static int feed_segmented_init_upload_response(struct sdo_ul_req* self,
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

static int feed_upload_segment_response(struct sdo_ul_req* self,
					const struct can_frame* frame)
{
	struct can_frame* cf = &self->frame;

	size_t size = sdo_is_size_indicated(frame) ? sdo_get_segment_size(frame)
						   : SDO_SEGMENT_MAX_SIZE;

	assert(self->size - self->pos >= size);

	memcpy(&self->addr[self->pos], &frame->data[SDO_SEGMENT_IDX], size);

	self->pos += size;

	if (sdo_is_end_segment(frame)) {
		self->state = SDO_REQ_DONE;
		self->have_frame = 0;
		return self->have_frame;
	}

	sdo_clear_frame(cf);
	sdo_set_cs(cf, SDO_CCS_UL_SEG_REQ);

	if (self->is_toggled)
		sdo_toggle(cf);

	cf->can_dlc = 1;

	self->is_toggled ^= 1;

	self->have_frame = 1;
	return self->have_frame;
}

int sdo_ul_req_feed(struct sdo_ul_req* self, const struct can_frame* frame)
{
	if (sdo_get_cs(frame) == SDO_SCS_ABORT) {
		self->state = SDO_REQ_REMOTE_ABORT;
		self->have_frame = 0;
		return 0;
	}

	switch (self->state) {
	case SDO_REQ_INIT:
		if (sdo_get_cs(frame) != SDO_SCS_UL_INIT_RES)
			return sdo_ul_req_abort(self, SDO_ABORT_INVALID_CS);

		return sdo_is_expediated(frame)
		       ? feed_expediated_init_upload_response(self, frame)
		       : feed_segmented_init_upload_response(self, frame);

	case SDO_REQ_SEG:
		if (sdo_get_cs(frame) != SDO_SCS_UL_SEG_RES)
			return sdo_ul_req_abort(self, SDO_ABORT_INVALID_CS);

		if (!sdo_is_end_segment(frame) &&
		    sdo_is_toggled(frame) == self->is_toggled)
			return sdo_ul_req_abort(self, SDO_ABORT_TOGGLE);

		return feed_upload_segment_response(self, frame);
	default:
		break;
	}
	abort();

	return -1;
}
