#ifndef _CANOPEN_NMT_H
#define _CANOPEN_NMT_H

#include <sys/socket.h>
#include <linux/can.h>

#define NMT_ALL_NODES 0

enum nmt_state {
	NMT_STATE_BOOTUP = 0,
	NMT_STATE_STOPPED = 4,
	NMT_STATE_OPERATIONAL = 5,
	NMT_STATE_PREOPERATIONAL = 127,
};

enum nmt_cs {
	NMT_CS_START = 1,
	NMT_CS_STOP = 2,
	NMT_CS_ENTER_PREOPERATIONAL = 128,
	NMT_CS_RESET_NODE = 129,
	NMT_CS_RESET_COMMUNICATION = 130,
};

struct nmt_slave {
	enum nmt_state state;
};

static inline int nmt_is_valid(const struct can_frame* frame)
{
	return frame->can_dlc == 2;
}

static inline enum nmt_cs nmt_get_cs(const struct can_frame* frame)
{
	return (enum nmt_cs)frame->data[0];
}

static inline void nmt_set_cs(struct can_frame* frame, enum nmt_cs cs)
{
	frame->data[0] = cs;
}

static inline int nmt_get_nodeid(const struct can_frame* frame)
{
	return frame->data[1];
}

static inline void nmt_set_nodeid(struct can_frame* frame, int nodeid)
{
	frame->data[1] = nodeid;
}

#endif /* _CANOPEN_NMT_H */

