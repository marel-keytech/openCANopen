#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <stdio.h>
#include <linux/can.h>
#include <sys/param.h>

#include "canopen/sdo.h"
#include "canopen/sdo_srv.h"

void* (*sdo_srv_get_sdo_addr)(int index, int subindex, size_t*);
int (*sdo_srv_write_obj)(int index, int subindex, const void*, size_t*);

int sdo_srv_dl_sm_abort(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
			struct can_frame* frame_out, enum sdo_abort_code code)
{
	sdo_clear_frame(frame_out);
	sdo_set_cs(frame_out, SDO_SCS_ABORT);
	sdo_set_abort_code(frame_out, SDO_ABORT_INVALID_CS);
	return self->dl_state = SDO_SRV_DL_ABORT;
}

static int dl_expediated(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
			 struct can_frame* frame_out)
{
	if (sdo_is_size_indicated(frame_in) &&
	    sdo_get_expediated_size(frame_in) != self->size)
		return sdo_srv_dl_sm_abort(self, frame_in, frame_out, SDO_ABORT_SIZE);

	memcpy(self->ptr, &frame_in->data[SDO_EXPEDIATED_DATA_IDX], self->size);

	return self->dl_state = SDO_SRV_DL_DONE;
}

int sdo_srv_dl_sm_init(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out)
{
	void* addr;
	size_t size;

	enum sdo_ccs ccs = sdo_get_cs(frame_in);

	if (ccs == SDO_CCS_ABORT)
		return self->dl_state; /* ignore abort */

	if (ccs != SDO_CCS_DL_INIT_REQ)
		return sdo_srv_dl_sm_abort(self, frame_in, frame_out,
					   SDO_ABORT_INVALID_CS);

	sdo_clear_frame(frame_out);

	if (!sdo_srv_get_sdo_addr)
		goto no_write;

	addr = sdo_srv_get_sdo_addr(sdo_get_index(frame_in),
				    sdo_get_subindex(frame_in), &size);
	if (!addr)
		return sdo_srv_dl_sm_abort(self, frame_in, frame_out,
					   SDO_ABORT_NEXIST);
	assert(size != 0);

	self->ptr = addr;
	self->size = size;
	self->index = 0;

	if (sdo_is_expediated(frame_in))
		return dl_expediated(self, frame_in, frame_out);

	if (sdo_is_size_indicated(frame_in) &&
	    sdo_get_indicated_size(frame_in) != size)
		return sdo_srv_dl_sm_abort(self, frame_in, frame_out, SDO_ABORT_SIZE);

no_write:
	sdo_set_cs(frame_out, SDO_SCS_DL_INIT_RES);
	sdo_copy_multiplexer(frame_out, frame_in);

	return self->dl_state = SDO_SRV_DL_SEG;
}

int sdo_srv_dl_sm_seg(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
		      struct can_frame* frame_out, int expect_toggled)
{
	size_t size;

	enum sdo_ccs ccs = sdo_get_cs(frame_in);

	if (ccs == SDO_CCS_ABORT)
		return self->dl_state = SDO_SRV_DL_REMOTE_ABORT;

	if (ccs != SDO_CCS_DL_SEG_REQ)
		return sdo_srv_dl_sm_abort(self, frame_in, frame_out,
					   SDO_ABORT_INVALID_CS);

	if (sdo_is_toggled(frame_in) != expect_toggled)
		return sdo_srv_dl_sm_abort(self, frame_in, frame_out,
					   SDO_ABORT_TOGGLE);

	sdo_clear_frame(frame_out);

	if (!self->ptr)
		goto no_write;

	size = sdo_get_segment_size(frame_in);

	if (size > self->size)
		return sdo_srv_dl_sm_abort(self, frame_in, frame_out,
					   SDO_ABORT_TOO_LONG);

	memcpy(&self->ptr[self->index], &frame_in->data[SDO_SEGMENT_IDX], size);

	self->index += size;
	self->size -= size;

no_write:
	sdo_set_cs(frame_out, SDO_SCS_DL_SEG_RES);
	if (expect_toggled)
		sdo_toggle(frame_out);

	if (sdo_is_end_segment(frame_in)) {
		sdo_end_segment(frame_out);
		return self->dl_state = SDO_SRV_DL_DONE;
	}

	return self->dl_state = expect_toggled ? SDO_SRV_DL_SEG
					       : SDO_SRV_DL_SEG_TOGGLED;
}

int sdo_srv_dl_sm_feed(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out)
{
	switch (self->dl_state)
	{
	case SDO_SRV_DL_INIT:
		return sdo_srv_dl_sm_init(self, frame_in, frame_out);
	case SDO_SRV_DL_SEG:
		return sdo_srv_dl_sm_seg(self, frame_in, frame_out, 0);
	case SDO_SRV_DL_SEG_TOGGLED:
		return sdo_srv_dl_sm_seg(self, frame_in, frame_out, 1);
	default:
		return SDO_SRV_DL_PLEASE_RESET;
	}
}

int sdo_srv_ul_sm_abort(struct sdo_srv_ul_sm* self, struct can_frame* frame_in,
			struct can_frame* frame_out, enum sdo_abort_code code)
{
	sdo_clear_frame(frame_out);
	sdo_set_cs(frame_out, SDO_SCS_ABORT);
	sdo_set_abort_code(frame_out, SDO_ABORT_INVALID_CS);
	return self->ul_state = SDO_SRV_UL_ABORT;
}

int sdo_srv_ul_sm_init(struct sdo_srv_ul_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out)
{
	void* addr;
	size_t size;

	enum sdo_ccs ccs = sdo_get_cs(frame_in);

	if (ccs == SDO_CCS_ABORT)
		return self->ul_state; /* ignore abort */

	if (ccs != SDO_CCS_UL_INIT_REQ)
		return sdo_srv_ul_sm_abort(self, frame_in, frame_out,
					   SDO_ABORT_INVALID_CS);

	sdo_clear_frame(frame_out);

	if (!sdo_srv_get_sdo_addr)
		goto no_read;

	addr = sdo_srv_get_sdo_addr(sdo_get_index(frame_in),
				    sdo_get_subindex(frame_in), &size);
	if (!addr)
		return sdo_srv_ul_sm_abort(self, frame_in, frame_out,
					   SDO_ABORT_NEXIST);
	assert(size != 0);

	if (size <= SDO_EXPEDIATED_DATA_SIZE) {
		memcpy(&frame_out->data[SDO_EXPEDIATED_DATA_IDX], addr, size);
		sdo_indicate_size(frame_out);
		sdo_set_expediated_size(frame_out, size);
		sdo_expediate(frame_out);
		return self->ul_state = SDO_SRV_UL_DONE;
	}

	self->ptr = addr;
	self->size = size;
	self->index = 0;

	sdo_indicate_size(frame_out);
	sdo_set_indicated_size(frame_out, size);

no_read:
	sdo_set_cs(frame_out, SDO_SCS_UL_INIT_RES);
	sdo_copy_multiplexer(frame_out, frame_in);

	return self->ul_state = SDO_SRV_UL_SEG;
}

int sdo_srv_ul_sm_seg(struct sdo_srv_ul_sm* self, struct can_frame* frame_in,
		      struct can_frame* frame_out, int expect_toggled)
{
	size_t size;

	enum sdo_ccs ccs = sdo_get_cs(frame_in);

	if (ccs == SDO_CCS_ABORT)
		return self->ul_state = SDO_SRV_UL_REMOTE_ABORT;

	if (ccs != SDO_CCS_UL_SEG_REQ)
		return sdo_srv_ul_sm_abort(self, frame_in, frame_out,
					   SDO_ABORT_INVALID_CS);

	if (sdo_is_toggled(frame_in) != expect_toggled)
		return sdo_srv_ul_sm_abort(self, frame_in, frame_out,
					   SDO_ABORT_TOGGLE);

	sdo_clear_frame(frame_out);

	if (!self->ptr)
		goto no_read;

	size = MIN(self->size, SDO_SEGMENT_MAX_SIZE);
	memcpy(&frame_out->data[SDO_SEGMENT_IDX], &self->ptr[self->index], size);

	self->size -= size;
	self->index += size;

	sdo_set_segment_size(frame_out, size);

	if (self->size <= 0) {
		sdo_end_segment(frame_out);
		return self->ul_state = SDO_SRV_UL_DONE;
	}

no_read:
	sdo_set_cs(frame_out, SDO_SCS_UL_SEG_RES);
	if (expect_toggled)
		sdo_toggle(frame_out);

	return self->ul_state = expect_toggled ? SDO_SRV_UL_SEG
					       : SDO_SRV_UL_SEG_TOGGLED;
}

int sdo_srv_ul_sm_feed(struct sdo_srv_ul_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out)
{
	switch (self->ul_state)
	{
	case SDO_SRV_UL_INIT:
		return sdo_srv_ul_sm_init(self, frame_in, frame_out);
	case SDO_SRV_UL_SEG:
		return sdo_srv_ul_sm_seg(self, frame_in, frame_out, 0);
	case SDO_SRV_UL_SEG_TOGGLED:
		return sdo_srv_ul_sm_seg(self, frame_in, frame_out, 1);
	default:
		return SDO_SRV_UL_PLEASE_RESET;
	}
}

