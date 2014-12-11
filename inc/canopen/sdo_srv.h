#ifndef _CANOPEN_SDO_SRV_H
#define _CANOPEN_SDO_SRV_H

#include <string.h>

struct sdo_srv_dl_sm {
	enum sdo_srv_dl_state dl_state;
	void* ptr;
	size_t size;
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

extern void* (*sdo_srv_get_sdo_addr)(int index, int subindex, size_t*);
extern int (*sdo_srv_write_obj)(int index, int subindex, const void*, size_t*);

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

#endif /* _CANOPEN_SDO_SRV_H */

