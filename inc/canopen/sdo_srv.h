#ifndef _CANOPEN_SDO_SRV_H
#define _CANOPEN_SDO_SRV_H

#include <string.h>

#include "canopen/sdo.h"

struct sdo_srv_dl_sm {
	enum sdo_srv_dl_state dl_state;
	uint8_t* ptr;
	size_t size;
	int index;
};

struct sdo_srv_ul_sm {
	enum sdo_srv_ul_state ul_state;
	uint8_t* ptr;
	size_t size;
	int index;
};

struct sdo_srv {
	struct sdo_srv_dl_sm client[128];
};

static inline void sdo_srv_init(struct sdo_srv* self)
{
	memset(self, 0, sizeof(*self));
}

int sdo_srv_dl_sm_abort(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
			struct can_frame* frame_out, enum sdo_abort_code code);

int sdo_srv_dl_sm_feed(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out);

int sdo_srv_dl_sm_init(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out);
int sdo_srv_dl_sm_seg(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
		      struct can_frame* frame_out, int expect_toggled);

int sdo_srv_ul_sm_abort(struct sdo_srv_ul_sm* self, struct can_frame* frame_in,
			struct can_frame* frame_out, enum sdo_abort_code code);

int sdo_srv_ul_sm_feed(struct sdo_srv_ul_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out);

int sdo_srv_ul_sm_init(struct sdo_srv_ul_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out);
int sdo_srv_ul_sm_seg(struct sdo_srv_ul_sm* self, struct can_frame* frame_in,
		      struct can_frame* frame_out, int expect_toggled);

static inline void sdo_srv_dl_sm_reset(struct sdo_srv_dl_sm* self)
{
	self->dl_state = SDO_SRV_DL_START;
}

int sdo_srv_feed(struct sdo_srv* self, struct can_frame* frame);

int sdo_match_obj_size(struct sdo_obj* obj, size_t size,
		       enum sdo_abort_code* code);

#endif /* _CANOPEN_SDO_SRV_H */

