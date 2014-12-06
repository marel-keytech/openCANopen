#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <linux/can.h>

#include "canopen/sdo.h"
#include "canopen/byteorder.h"

/* Note: size indication for segmented download is ignored */

void* (*sdo_srv_get_sdo_addr)(int index, int subindex, size_t*);

static inline void clear_frame(struct can_frame* frame)
{
	memset(frame, 0, sizeof(*frame));
}

static inline void copy_multiplexer(struct can_frame* dst,
				    struct can_frame* src)
{
	memcpy(&dst->data[SDO_MULTIPLEXER_IDX], &src->data[SDO_MULTIPLEXER_IDX],
	       SDO_MULTIPLEXER_SIZE);
}

int sdo_srv_sm_abort(void* self, struct can_frame* frame_in,
		     struct can_frame* frame_out, enum sdo_abort_code code)
{
	clear_frame(frame_out);
	sdo_set_cs(frame_out, SDO_SCS_ABORT);
	sdo_set_abort_code(frame_out, SDO_ABORT_INVALID_CS);
	return ((struct sdo_srv_dl_sm*)self)->dl_state = SDO_SRV_DL_ABORT;
}

int sdo_srv_dl_sm_init(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out)
{
	enum sdo_ccs ccs = sdo_get_cs(frame_in);

	if (ccs == SDO_CCS_ABORT)
		return self->dl_state; /* ignore abort */

	if (ccs != SDO_CCS_DL_INIT_REQ)
		return sdo_srv_sm_abort(self, frame_in, frame_out,
					SDO_ABORT_INVALID_CS);

	clear_frame(frame_out);
	sdo_set_cs(frame_out, SDO_SCS_DL_INIT_RES);
	copy_multiplexer(frame_out, frame_in);

	return self->dl_state = SDO_SRV_DL_SEG;
}

int sdo_srv_dl_sm_seg(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
		      struct can_frame* frame_out, int expect_toggled)
{
	enum sdo_ccs ccs = sdo_get_cs(frame_in);

	if (ccs == SDO_CCS_ABORT)
		return self->dl_state = SDO_SRV_DL_REMOTE_ABORT;

	if (ccs != SDO_CCS_DL_SEG_REQ)
		return sdo_srv_sm_abort(self, frame_in, frame_out,
					SDO_ABORT_INVALID_CS);

	if (sdo_is_toggled(frame_in) != expect_toggled)
		return sdo_srv_sm_abort(self, frame_in, frame_out,
					SDO_ABORT_TOGGLE);

	clear_frame(frame_out);
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

static inline void sdo_srv_ul_sm_close_mem(struct sdo_srv_ul_sm* self)
{
	if(self->memfd)
		fclose(self->memfd);
	self->memfd = NULL;
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
		return sdo_srv_sm_abort(self, frame_in, frame_out,
					SDO_ABORT_INVALID_CS);

	clear_frame(frame_out);

	if (!sdo_srv_get_sdo_addr)
		goto no_read;

	addr = sdo_srv_get_sdo_addr(sdo_get_index(frame_in),
				    sdo_get_subindex(frame_in), &size);
	if (!addr)
		return sdo_srv_sm_abort(self, frame_in, frame_out,
					SDO_ABORT_NEXIST);
	assert(size != 0);

	if (size <= 4) {
		memcpy(&frame_out->data[SDO_EXPEDIATED_DATA_IDX], addr, size);
		sdo_indicate_size(frame_out);
		sdo_set_expediated_size(frame_out, size);
		sdo_expediate(frame_out);
		return self->ul_state = SDO_SRV_UL_DONE;
	}

	self->memfd = fmemopen(addr, size, "r");
	if (!self->memfd)
		return sdo_srv_sm_abort(self, frame_in, frame_out,
					SDO_ABORT_NOMEM);

no_read:
	sdo_set_cs(frame_out, SDO_SCS_UL_INIT_RES);
	copy_multiplexer(frame_out, frame_in);

	return self->ul_state = SDO_SRV_UL_SEG;
}

int sdo_srv_ul_sm_seg(struct sdo_srv_ul_sm* self, struct can_frame* frame_in,
		      struct can_frame* frame_out, int expect_toggled)
{
	size_t size;

	enum sdo_ccs ccs = sdo_get_cs(frame_in);

	if (ccs == SDO_CCS_ABORT)
		return self->ul_state = SDO_SRV_UL_REMOTE_ABORT;

	if (ccs != SDO_CCS_UL_SEG_REQ) {
		sdo_srv_ul_sm_close_mem(self);
		return sdo_srv_sm_abort(self, frame_in, frame_out,
					SDO_ABORT_INVALID_CS);
	}

	if (sdo_is_toggled(frame_in) != expect_toggled) {
		sdo_srv_ul_sm_close_mem(self);
		return sdo_srv_sm_abort(self, frame_in, frame_out,
					SDO_ABORT_TOGGLE);
	}

	clear_frame(frame_out);

	if (!self->memfd)
		goto no_read;

	size = fread(&frame_out->data[SDO_SEGMENT_IDX], SDO_SEGMENT_MAX_SIZE, 1,
		     self->memfd);

	sdo_set_segment_size(frame_out, size);

	if (feof(self->memfd)) {
		sdo_end_segment(frame_out);
		sdo_srv_ul_sm_close_mem(self);
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

