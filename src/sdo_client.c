#include <assert.h>
#include "canopen/sdo.h"
#include "canopen/sdo_client.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static int sdo_dl_req_abort(struct sdo_dl_req* self, enum sdo_abort_code code)
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
	case SDO_REQ_END_SEGMENT:
		if (sdo_get_cs(frame) != SDO_SCS_DL_SEG_RES)
			return sdo_dl_req_abort(self, SDO_ABORT_INVALID_CS);

		if (self->is_toggled != sdo_is_toggled(frame))
			return sdo_dl_req_abort(self, SDO_ABORT_TOGGLE);

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

int sdo_request_upload(struct sdo_ul_req* self, int index, int subindex,
		       ssize_t expected_size)
{
	return 0;
}

