#ifndef _CANOPEN_SDO_H
#define _CANOPEN_SDO_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <linux/can.h>

#include "canopen/byteorder.h"

#define SDO_SEGMENT_IDX 1
#define SDO_SEGMENT_MAX_SIZE 7
#define SDO_EXPEDIATED_DATA_IDX 4
#define SDO_INDICATED_SIZE_IDX 4
#define SDO_EXPEDIATED_DATA_SIZE 4
#define SDO_MULTIPLEXER_IDX 1
#define SDO_MULTIPLEXER_SIZE 3

enum sdo_ccs {
	SDO_CCS_DL_SEG_REQ = 0,
	SDO_CCS_DL_INIT_REQ = 1,
	SDO_CCS_UL_INIT_REQ = 2,
	SDO_CCS_UL_SEG_REQ = 3,
	SDO_CCS_ABORT = 4,
};

enum sdo_scs {
	SDO_SCS_UL_SEG_RES = 0,
	SDO_SCS_DL_SEG_RES = 1,
	SDO_SCS_UL_INIT_RES = 2,
	SDO_SCS_DL_INIT_RES = 3,
	SDO_SCS_ABORT = 4,
};

enum sdo_srv_dl_state {
	SDO_SRV_DL_PLEASE_RESET = -3,
	SDO_SRV_DL_REMOTE_ABORT = -2,
	SDO_SRV_DL_ABORT = -1,
	SDO_SRV_DL_START = 0,
	SDO_SRV_DL_INIT = 0,
	SDO_SRV_DL_SEG,
	SDO_SRV_DL_SEG_TOGGLED,
	SDO_SRV_DL_DONE,
};

enum sdo_srv_ul_state {
	SDO_SRV_UL_PLEASE_RESET = -3,
	SDO_SRV_UL_REMOTE_ABORT = -2,
	SDO_SRV_UL_ABORT = -1,
	SDO_SRV_UL_START = 0,
	SDO_SRV_UL_INIT = 0,
	SDO_SRV_UL_SEG,
	SDO_SRV_UL_SEG_TOGGLED,
	SDO_SRV_UL_DONE
};

enum sdo_abort_code {
	SDO_ABORT_TOGGLE	= 0x05030000,
	SDO_ABORT_INVALID_CS	= 0x05040001,
	SDO_ABORT_NOMEM		= 0x05040005,
	SDO_ABORT_NEXIST	= 0x06020000,
	SDO_ABORT_SIZE          = 5, /* TODO: invalid size */
	SDO_ABORT_TOO_LONG      = 6, /* TODO: too long sdo */
};

enum sdo_obj_flags {
	SDO_OBJ_R = 1,
	SDO_OBJ_W = 2,
	SDO_OBJ_RW = SDO_OBJ_R | SDO_OBJ_W,

	SDO_OBJ_EQ = 4,
	SDO_OBJ_LT = 8,
	SDO_OBJ_GT = 16,
	
	SDO_OBJ_LE = SDO_OBJ_EQ | SDO_OBJ_LT,
	SDO_OBJ_GE = SDO_OBJ_EQ | SDO_OBJ_GT,
};

struct sdo_obj {
	void* addr;
	size_t size;
	enum sdo_obj_flags flags;
};

extern int (*sdo_get_obj)(struct sdo_obj* obj, int index, int subindex);

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

static inline enum sdo_abort_code sdo_get_abort_code(struct can_frame* frame)
{
	enum sdo_abort_code code;
	byteorder(&code, &frame->data[4], 4);
	return code;
}

static inline void sdo_set_abort_code(struct can_frame* frame,
				      enum sdo_abort_code code)
{
	byteorder(&frame->data[4], &code, 4);
}

static inline int sdo_is_expediated(struct can_frame* frame)
{
	return !!(frame->data[0] & 2);
}

static inline void sdo_expediate(struct can_frame* frame)
{
	frame->data[0] |= 2;
}

static inline int sdo_is_size_indicated(struct can_frame* frame)
{
	return frame->data[0] & 1;
}

static inline void sdo_indicate_size(struct can_frame* frame)
{
	frame->data[0] |= 1;
}

static inline void sdo_set_indicated_size(struct can_frame* frame, size_t size)
{
	byteorder(&frame->data[SDO_INDICATED_SIZE_IDX], &size, 4);
}

static inline size_t sdo_get_indicated_size(struct can_frame* frame)
{
	size_t size;
	byteorder(&size, &frame->data[SDO_INDICATED_SIZE_IDX], 4);
	return size;
}

static inline size_t sdo_get_expediated_size(struct can_frame* frame)
{
	int n = (frame->data[0] >> 2) & 3;
	return 4 - n;
}

static inline void sdo_set_expediated_size(struct can_frame* frame, size_t size)
{
	int n = 4 - size;
	frame->data[0] &= ~(3 << 2);
	frame->data[0] |= n << 2;
}

static inline int sdo_get_index(struct can_frame* frame)
{
	uint16_t value;
	byteorder(&value, &frame->data[SDO_MULTIPLEXER_IDX], 2);
	return value;
}

static inline int sdo_get_subindex(struct can_frame* frame)
{
	return frame->data[SDO_MULTIPLEXER_IDX+2];
}

static inline void sdo_set_index(struct can_frame* frame, int index)
{
	byteorder(&frame->data[SDO_MULTIPLEXER_IDX], &index, 2);
}

static inline void sdo_set_subindex(struct can_frame* frame, int subindex)
{
	frame->data[SDO_MULTIPLEXER_IDX+2] = subindex;
}

static inline void sdo_clear_frame(struct can_frame* frame)
{
	memset(frame, 0, sizeof(*frame));
}

static inline void sdo_copy_multiplexer(struct can_frame* dst,
					struct can_frame* src)
{
	memcpy(&dst->data[SDO_MULTIPLEXER_IDX], &src->data[SDO_MULTIPLEXER_IDX],
	       SDO_MULTIPLEXER_SIZE);
}


#endif /* _CANOPEN_SDO_H */

