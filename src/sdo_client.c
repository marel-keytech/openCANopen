#include <assert.h>
#include "canopen/sdo.h"
#include "canopen/sdo_client.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void request_segmented_download(struct sdo_dl_req* self, int index,
				       int subindex, const void* addr,
				       size_t size)
{
	struct can_frame* cf = &self->frame;

	self->state = SDO_REQ_INIT;

	self->addr = addr;
	self->size = size;

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

	memcpy(&cf->data[SDO_EXPEDIATED_DATA_IDX], addr, size);
}

int sdo_request_download(struct sdo_dl_req* self, int index,
			 int subindex, const void* addr, size_t size)
{
	memset(self, 0, sizeof(*self));
	struct can_frame* cf = &self->frame;

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
				    const struct can_frame* frame,
				    int is_toggled)
{
	struct can_frame* cf = &self->frame;

	sdo_clear_frame(cf);

	sdo_set_cs(cf, SDO_CCS_DL_SEG_REQ);

	if (is_toggled)
		sdo_toggle(cf);

	size_t segment_size = MIN(SDO_SEGMENT_MAX_SIZE, self->size - self->pos);
	assert(segment_size > 0);
	sdo_set_segment_size(cf, segment_size);

	memcpy(&cf->data[SDO_SEGMENT_IDX], &self->addr[self->pos],segment_size);

	self->pos += segment_size;
	if (self->pos >= self->size)
		self->state = SDO_REQ_END_SEGMENT;
	else
		self->state = is_toggled ? SDO_REQ_SEG : SDO_REQ_SEG_TOGGLED;

	self->have_frame = 1;
	return self->have_frame;
}

int sdo_dl_req_feed(struct sdo_dl_req* self, const struct can_frame* frame)
{
	if (sdo_get_cs(frame) == SDO_SCS_ABORT) {
		self->state = SDO_REQ_ABORTED;
		self->have_frame = 0;
		return 0;
	}

	switch (self->state) {
	case SDO_REQ_INIT:
	case SDO_REQ_SEG:
		return request_download_segment(self, frame, 0);
	case SDO_REQ_SEG_TOGGLED:
		return request_download_segment(self, frame, 1);
	case SDO_REQ_INIT_EXPEDIATED:
	case SDO_REQ_END_SEGMENT:
		self->state = SDO_REQ_DONE;
		self->have_frame = 0;
		return 0;
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

