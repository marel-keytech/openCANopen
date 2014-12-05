#include <linux/can.h>
#include "canopen/sdo.h"

static inline void clear_frame(struct can_frame* frame)
{
	memset(frame, 0, sizeof(*frame));
}

static inline void copy_multiplexer(struct can_frame* dst,
				    struct can_frame* src)
{
	memcpy(&dst[SDO_MULTIPLEXER_IDX], &src[SDO_MULTIPLEXER_IDX],
	       SDO_MULTIPLEXER_SIZE);
}

static int srv_dl_abort(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
			struct can_frame* frame_out, enum sdo_abort_code code)
{
	/* TODO */
	return self->dl_state = SDO_SRV_DL_ABORT;
}

static int srv_dl_init(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out)
{
	if (sdo_get_cs(frame_in) != SDO_CCS_DL_INIT_REQ)
		return srv_dl_abort(self, frame_in, frame_out,
				    SDO_ABORT_INVALID_CS);

	clear_frame(frame_out);
	sdo_set_cs(frame_out, SDO_SCS_DL_INIT_RES);
	copy_multiplexer(frame_out, frame_in);

	return self->dl_state = SDO_SRV_DL_SEG;
}

static int srv_dl_seg(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
		      struct can_frame* frame_out, int expect_toggled)
{
	if (sdo_get_cs(frame_in) != SDO_CCS_DL_SEG_REQ)
		return srv_dl_abort(self, frame_in, frame_out,
				    SDO_ABORT_INVALID_CS);

	if (sdo_is_toggled(frame_in) != expect_toggled)
		return srv_dl_abort(self, frame_in, frame_out,
				    SDO_ABORT_TOGGLE);

	clear_frame(frame_out);
	sdo_set_cs(frame_out, SDO_SCS_DL_SEG_RES);
	if(expect_toggled)
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
	case SDO_SRV_DL_INIT:	return srv_dl_init(self, frame_in, frame_out);
	case SDO_SRV_DL_SEG:	return srv_dl_seg(self, frame_in, frame_out, 0);
	case SDO_SRV_DL_SEG_TOGGLED:
				return srv_dl_seg(self, frame_in, frame_out, 1);
	case SDO_SRV_DL_ABORT:
	case SDO_SRV_DL_DONE:
	case SDO_SRV_DL_PLEASE_RESET:
				return SDO_SRV_DL_PLEASE_RESET;
	}
}

