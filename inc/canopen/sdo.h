#ifndef _CANOPEN_SDO_H
#define _CANOPEN_SDO_H

#include <stdlib.h>
#include <string.h>
#include <linux/can.h>

#define SDO_SEGMENT_IDX 1
#define SDO_SEGMENT_MAX_SIZE 7
#define SDO_MULTIPLEXER_IDX 1
#define SDO_MULTIPLEXER_SIZE 3

enum sdo_ccs {
	SDO_CCS_DL_SEG_REQ = 0,
	SDO_CCS_DL_INIT_REQ = 1,
	SDO_CCS_DL_ABORT = 4,
};

enum sdo_scs {
	SDO_SCS_DL_SEG_RES = 1,
	SDO_SCS_DL_INIT_RES = 3,
	SDO_SCS_DL_ABORT = 4,
};

enum sdo_srv_dl_state {
	SDO_SRV_DL_PLEASE_RESET = -2,
	SDO_SRV_DL_ABORT = -1,
	SDO_SRV_DL_START = 0,
	SDO_SRV_DL_INIT = 0,
	SDO_SRV_DL_SEG,
	SDO_SRV_DL_SEG_TOGGLED,
	SDO_SRV_DL_DONE
};

enum sdo_abort_code {
	SDO_ABORT_TOGGLE	= 0x05030000,
	SDO_ABORT_INVALID_CS	= 0x05040001,
};

struct sdo_srv_dl_sm {
	enum sdo_srv_dl_state dl_state;
};

struct sdo_srv {
	struct sdo_srv_dl_sm client[128];
};

static inline void sdo_srv_init(struct sdo_srv* self)
{
	memset(self, 0, sizeof(*self));
}

int sdo_srv_dl_sm_feed(struct sdo_srv_dl_sm* self, struct can_frame* frame_in,
		       struct can_frame* frame_out);

static inline void sdo_srv_dl_sm_reset(struct sdo_srv_dl_sm* self)
{
	self->dl_state = SDO_SRV_DL_START;
}

int sdo_srv_feed(struct sdo_srv* self, struct can_frame* frame);

static inline int sdo_get_cs(struct can_frame* frame)
{
	return frame->data[0] >> 5;
}

static inline void sdo_set_cs(struct can_frame* frame, int cs)
{
	frame->data[0] &= 0x1f;
	frame->data[0] |= cs << 5;
}

static inline size_t sdo_get_segment_size(struct can_frame* frame)
{
	int n = (frame->data[0] >> 1) & 7;
	return 7 - n;
}

static inline void sdo_set_segment_size(struct can_frame* frame, size_t size)
{
	int n = 7 - size;
	frame->data[0] &= ~(7 << 1);
	frame->data[0] |= n << 1;
}

static inline void sdo_toggle(struct can_frame* frame)
{
	frame->data[0] ^= 1 << 4;
}

static inline int sdo_is_toggled(struct can_frame* frame)
{
	return !!(frame->data[0] & 1 << 4);
}

static inline void sdo_end_segment(struct can_frame* frame)
{
	frame->data[0] |= 1;
}

static inline int sdo_is_end_segment(struct can_frame* frame)
{
	return frame->data[0] & 1;
}

#endif /* _CANOPEN_SDO_H */

