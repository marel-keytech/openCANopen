#include <linux/can.h>
#include "canopen/sdo.h"

/* Note: size indication for segmented download is ignored */

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

int sdo_srv_dl_sm_abort(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
			struct can_frame* frame_out, enum sdo_abort_code code)
{
	clear_frame(frame_out);
	sdo_set_cs(frame_out, SDO_SCS_DL_ABORT);
	sdo_set_abort_code(frame_out, SDO_ABORT_INVALID_CS);
	return self->dl_state = SDO_SRV_DL_ABORT;
}

int sdo_srv_dl_sm_init(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out)
{
	enum sdo_ccs ccs = sdo_get_cs(frame_in);

	if (ccs == SDO_CCS_DL_ABORT)
		return self->dl_state; /* ignore abort */

	if (ccs != SDO_CCS_DL_INIT_REQ)
		return sdo_srv_dl_sm_abort(self, frame_in, frame_out,
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

	if (ccs == SDO_CCS_DL_ABORT)
		return self->dl_state = SDO_SRV_DL_REMOTE_ABORT;

	if (ccs != SDO_CCS_DL_SEG_REQ)
		return sdo_srv_dl_sm_abort(self, frame_in, frame_out,
					   SDO_ABORT_INVALID_CS);

	if (sdo_is_toggled(frame_in) != expect_toggled)
		return sdo_srv_dl_sm_abort(self, frame_in, frame_out,
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
	case SDO_SRV_DL_INIT:
		return sdo_srv_dl_sm_init(self, frame_in, frame_out);
	case SDO_SRV_DL_SEG:
		return sdo_srv_dl_sm_seg(self, frame_in, frame_out, 0);
	case SDO_SRV_DL_SEG_TOGGLED:
		return sdo_srv_dl_sm_seg(self, frame_in, frame_out, 1);
	case SDO_SRV_DL_ABORT:
	case SDO_SRV_DL_DONE:
	case SDO_SRV_DL_PLEASE_RESET:
	case SDO_SRV_DL_REMOTE_ABORT:
		return SDO_SRV_DL_PLEASE_RESET;
	}
}

