#ifndef _CANOPEN_SDO_SRV_H
#define _CANOPEN_SDO_SRV_H

#include <string.h>

#include "canopen/sdo.h"

enum sdo_srv_state {
	SDO_SRV_PLEASE_RESET = -3,
	SDO_SRV_REMOTE_ABORT = -2,
	SDO_SRV_ABORT = -1,
	SDO_SRV_START = 0,
	SDO_SRV_INIT = 0,
	SDO_SRV_DL_SEG,
	SDO_SRV_DL_SEG_TOGGLED,
	SDO_SRV_UL_SEG,
	SDO_SRV_UL_SEG_TOGGLED,
	SDO_SRV_DONE,
};

struct sdo_srv_sm {
	enum sdo_srv_state state;
	struct sdo_obj obj;
	int index;
};

#define SDO_SRV_QUEUE_SIZE 8
#define SDO_SRV_QUEUE_MAX SDO_SRV_QUEUE_SIZE-1

struct sdo_srv {
	struct sdo_srv_sm sm;

	int have_frame;
	struct can_frame out_frame;
};

static inline struct can_frame* sdo_srv_next(struct sdo_srv* self)
{
	return self->have_frame ? &self->out_frame : NULL;
}

static inline void sdo_srv_init(struct sdo_srv* self)
{
	memset(self, 0, sizeof(*self));
}

int sdo_srv_feed(struct sdo_srv* self, struct can_frame* frame);

int sdo_srv_sm_abort(struct sdo_srv_sm* self, struct can_frame* frame_in,
		     struct can_frame* frame_out, enum sdo_abort_code code);

int sdo_srv_dl_sm_feed(struct sdo_srv_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out);

int sdo_srv_dl_sm_init(struct sdo_srv_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out);
int sdo_srv_dl_sm_seg(struct sdo_srv_sm* self, struct can_frame* frame_in,
		      struct can_frame* frame_out, int expect_toggled);

int sdo_srv_ul_sm_feed(struct sdo_srv_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out);

int sdo_srv_ul_sm_init(struct sdo_srv_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out);
int sdo_srv_ul_sm_seg(struct sdo_srv_sm* self, struct can_frame* frame_in,
		      struct can_frame* frame_out, int expect_toggled);

static inline void sdo_srv_dl_sm_reset(struct sdo_srv_sm* self)
{
	self->state = SDO_SRV_START;
}

int sdo_match_obj_size(struct sdo_obj* obj, size_t size,
		       enum sdo_abort_code* code);

#endif /* _CANOPEN_SDO_SRV_H */

