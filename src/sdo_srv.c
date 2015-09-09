#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <stdio.h>
#include <sys/socket.h> // for definition of sa_family_t
#include <linux/can.h>
#include <sys/param.h>

#include "canopen/sdo.h"
#include "canopen/sdo_srv.h"

int (*sdo_get_obj)(struct sdo_obj* obj, int index, int subindex);

int sdo_srv_sm_abort(struct sdo_srv_sm* self, struct can_frame* frame_out,
		     enum sdo_abort_code code)
{
	sdo_abort(frame_out, code, self->xindex, self->xsubindex);
	return self->state = SDO_SRV_ABORT;
}

static int dl_expediated(struct sdo_srv_sm* self, struct can_frame* frame_in,
			 struct can_frame* frame_out)
{
	enum sdo_abort_code abort_code;
	size_t size;

	if (sdo_is_size_indicated(frame_in)) {
		size = sdo_get_expediated_size(frame_in);
		if (!sdo_match_obj_size(&self->obj, size, &abort_code))
			return sdo_srv_sm_abort(self, frame_out, abort_code);
	}

	memcpy(self->obj.addr, &frame_in->data[SDO_EXPEDIATED_DATA_IDX],
	       self->obj.size);

	return self->state = SDO_SRV_DONE;
}

static int dl_init_write_frame(struct sdo_srv_sm* self,
			       struct can_frame* frame_in,
			       struct can_frame* frame_out)
{
	size_t size;
	enum sdo_abort_code abort_code;

	int index = sdo_get_index(frame_in);
	int subindex = sdo_get_subindex(frame_in);

	self->xindex = index;
	self->xsubindex = subindex;

	if (sdo_get_obj(&self->obj, index, subindex) < 0)
		return sdo_srv_sm_abort(self, frame_out, SDO_ABORT_NEXIST);

	if (!(self->obj.flags & SDO_OBJ_W))
		return sdo_srv_sm_abort(self, frame_out, SDO_ABORT_RO);

	self->index = 0;

	if (sdo_is_expediated(frame_in))
		return dl_expediated(self, frame_in, frame_out);

	if (sdo_is_size_indicated(frame_in)) {
		size = sdo_get_indicated_size(frame_in);
		if (!sdo_match_obj_size(&self->obj, size, &abort_code))
			return sdo_srv_sm_abort(self, frame_out, abort_code);
	}

	return self->state;
}

int sdo_srv_dl_sm_init(struct sdo_srv_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out)
{
	enum sdo_ccs ccs = sdo_get_cs(frame_in);

	if (ccs == SDO_CCS_ABORT)
		return self->state; /* ignore abort */

	if (ccs != SDO_CCS_DL_INIT_REQ)
		return sdo_srv_sm_abort(self, frame_out, SDO_ABORT_INVALID_CS);

	sdo_clear_frame(frame_out);
	sdo_set_cs(frame_out, SDO_SCS_DL_INIT_RES);
	sdo_copy_multiplexer(frame_out, frame_in);

	if (sdo_get_obj)
		if (dl_init_write_frame(self, frame_in, frame_out) != 0)
			return self->state;

	return self->state = SDO_SRV_DL_SEG;
}


static int dl_seg_write_frame(struct sdo_srv_sm* self,
			      struct can_frame* frame_in,
			      struct can_frame* frame_out)
{
	size_t size = sdo_get_segment_size(frame_in);
	enum sdo_abort_code abort_code;

	if (size > self->obj.size)
		return sdo_srv_sm_abort(self, frame_out, SDO_ABORT_TOO_LONG);

	if (sdo_is_end_segment(frame_in) &&
	    !sdo_match_obj_size(&self->obj, size, &abort_code))
		return sdo_srv_sm_abort(self, frame_out, abort_code);

	memcpy(&self->obj.addr[self->index], &frame_in->data[SDO_SEGMENT_IDX],
	       size);

	self->index += size;
	self->obj.size -= size;

	return 0;
}

int sdo_srv_dl_sm_seg(struct sdo_srv_sm* self, struct can_frame* frame_in,
		      struct can_frame* frame_out, int expect_toggled)
{

	enum sdo_ccs ccs = sdo_get_cs(frame_in);

	if (ccs == SDO_CCS_ABORT)
		return self->state = SDO_SRV_REMOTE_ABORT;

	if (ccs != SDO_CCS_DL_SEG_REQ)
		return sdo_srv_sm_abort(self, frame_out, SDO_ABORT_INVALID_CS);

	if (sdo_is_toggled(frame_in) != expect_toggled)
		return sdo_srv_sm_abort(self, frame_out, SDO_ABORT_TOGGLE);

	sdo_clear_frame(frame_out);
	sdo_set_cs(frame_out, SDO_SCS_DL_SEG_RES);
	if (expect_toggled)
		sdo_toggle(frame_out);

	if (self->obj.addr)
		if (dl_seg_write_frame(self, frame_in, frame_out) != 0)
			return self->state;

	if (sdo_is_end_segment(frame_in)) {
		sdo_end_segment(frame_out);
		return self->state = SDO_SRV_DONE;
	}

	return self->state = expect_toggled ? SDO_SRV_DL_SEG
					    : SDO_SRV_DL_SEG_TOGGLED;
}

int sdo_srv_dl_sm_feed(struct sdo_srv_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out)
{
	switch (self->state)
	{
	case SDO_SRV_INIT:
		return sdo_srv_dl_sm_init(self, frame_in, frame_out);
	case SDO_SRV_DL_SEG:
		return sdo_srv_dl_sm_seg(self, frame_in, frame_out, 0);
	case SDO_SRV_DL_SEG_TOGGLED:
		return sdo_srv_dl_sm_seg(self, frame_in, frame_out, 1);
	default:
		return SDO_SRV_PLEASE_RESET;
	}
}

static int ul_init_read_frame(struct sdo_srv_sm* self,
			      struct can_frame* frame_in,
			      struct can_frame* frame_out)
{
	int index = sdo_get_index(frame_in);
	int subindex = sdo_get_subindex(frame_in);

	self->xindex = index;
	self->xsubindex = subindex;

	if (sdo_get_obj(&self->obj, index, subindex) < 0)
		return sdo_srv_sm_abort(self, frame_out, SDO_ABORT_NEXIST);

	if (!(self->obj.flags & SDO_OBJ_R))
		return sdo_srv_sm_abort(self, frame_out, SDO_ABORT_WO);

	if (self->obj.size <= SDO_EXPEDIATED_DATA_SIZE) {
		memcpy(&frame_out->data[SDO_EXPEDIATED_DATA_IDX],
		       self->obj.addr, self->obj.size);
		sdo_indicate_size(frame_out);
		sdo_set_expediated_size(frame_out, self->obj.size);
		sdo_expediate(frame_out);
		return self->state = SDO_SRV_DONE;
	}

	self->index = 0;

	sdo_indicate_size(frame_out);
	sdo_set_indicated_size(frame_out, self->obj.size);

	return self->state;
}

int sdo_srv_ul_sm_init(struct sdo_srv_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out)
{
	enum sdo_ccs ccs = sdo_get_cs(frame_in);

	if (ccs == SDO_CCS_ABORT)
		return self->state; /* ignore abort */

	if (ccs != SDO_CCS_UL_INIT_REQ)
		return sdo_srv_sm_abort(self, frame_out, SDO_ABORT_INVALID_CS);

	sdo_clear_frame(frame_out);

	sdo_set_cs(frame_out, SDO_SCS_UL_INIT_RES);
	sdo_copy_multiplexer(frame_out, frame_in);

	if (sdo_get_obj)
		if (ul_init_read_frame(self, frame_in, frame_out) != 0)
			return self->state;

	return self->state = SDO_SRV_UL_SEG;
}

static int ul_seg_read_frame(struct sdo_srv_sm* self,
			     struct can_frame* frame_in,
			     struct can_frame* frame_out)
{
	size_t size = MIN(self->obj.size, SDO_SEGMENT_MAX_SIZE);

	memcpy(&frame_out->data[SDO_SEGMENT_IDX], &self->obj.addr[self->index],
	       size);

	self->obj.size -= size;
	self->index += size;

	sdo_set_segment_size(frame_out, size);

	if (self->obj.size <= 0) {
		sdo_end_segment(frame_out);
		return self->state = SDO_SRV_DONE;
	}

	return 0;
}

int sdo_srv_ul_sm_seg(struct sdo_srv_sm* self, struct can_frame* frame_in,
		      struct can_frame* frame_out, int expect_toggled)
{
	enum sdo_ccs ccs = sdo_get_cs(frame_in);

	if (ccs == SDO_CCS_ABORT)
		return self->state = SDO_SRV_REMOTE_ABORT;

	if (ccs != SDO_CCS_UL_SEG_REQ)
		return sdo_srv_sm_abort(self, frame_out, SDO_ABORT_INVALID_CS);

	if (sdo_is_toggled(frame_in) != expect_toggled)
		return sdo_srv_sm_abort(self, frame_out, SDO_ABORT_TOGGLE);

	sdo_clear_frame(frame_out);

	sdo_set_cs(frame_out, SDO_SCS_UL_SEG_RES);
	if (expect_toggled)
		sdo_toggle(frame_out);

	if (self->obj.addr)
		if (ul_seg_read_frame(self, frame_in, frame_out) != 0)
			return self->state;

	return self->state = expect_toggled ? SDO_SRV_UL_SEG
					    : SDO_SRV_UL_SEG_TOGGLED;
}

int sdo_srv_ul_sm_feed(struct sdo_srv_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out)
{
	switch (self->state)
	{
	case SDO_SRV_INIT:
		return sdo_srv_ul_sm_init(self, frame_in, frame_out);
	case SDO_SRV_UL_SEG:
		return sdo_srv_ul_sm_seg(self, frame_in, frame_out, 0);
	case SDO_SRV_UL_SEG_TOGGLED:
		return sdo_srv_ul_sm_seg(self, frame_in, frame_out, 1);
	default:
		return SDO_SRV_PLEASE_RESET;
	}
}

int sdo_match_obj_size(struct sdo_obj* obj, size_t size,
		       enum sdo_abort_code* code)
{
	switch (obj->flags & SDO_OBJ_MATCH_MASK)
	{
	case 0:
	case SDO_OBJ_EQ:
		if (size > obj->size)
			*code = SDO_ABORT_TOO_LONG;
		else if (size < obj->size)
			*code = SDO_ABORT_TOO_SHORT;
		else
			*code = 0;
		break;
	case SDO_OBJ_LT:
		*code = size < obj->size ? 0 : SDO_ABORT_TOO_LONG;
		break;
	case SDO_OBJ_LE:
		*code = size <= obj->size ? 0 : SDO_ABORT_TOO_LONG;
		break;
	case SDO_OBJ_GT:
		*code = size > obj->size ? 0 : SDO_ABORT_TOO_SHORT;
		break;
	case SDO_OBJ_GE:
		*code = size >= obj->size ? 0 : SDO_ABORT_TOO_SHORT;
		break;
	case SDO_OBJ_MATCH_MASK: /* don't care */
		*code = 0;
		break;
	}

	return *code == 0;
}

int sdo_srv_feed(struct sdo_srv* self, struct can_frame* frame)
{
	switch (sdo_get_cs(frame)) {
	case SDO_CCS_UL_INIT_REQ:
	case SDO_CCS_UL_SEG_REQ:
		sdo_srv_ul_sm_feed(&self->sm, frame, &self->out_frame);
		self->have_frame = 1;
		break;
	case SDO_CCS_DL_INIT_REQ:
	case SDO_CCS_DL_SEG_REQ:
		sdo_srv_dl_sm_feed(&self->sm, frame, &self->out_frame);
		self->have_frame = 1;
		break;
	case SDO_CCS_ABORT:
		self->sm.state = SDO_SRV_REMOTE_ABORT;
		self->have_frame = 0;
	default:
		sdo_srv_sm_abort(&self->sm, &self->out_frame,
				 SDO_ABORT_INVALID_CS);
		self->have_frame = 1;
		break;
	}

	if (self->sm.state < 0 || self->sm.state == SDO_SRV_DONE)
		memset(&self->sm, 0, sizeof(self->sm));

	return self->have_frame;
}

