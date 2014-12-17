#include <linux/can.h>
#include <assert.h>

#include "canopen.h"
#include "socketcan.h"

static void set_sff_mask(struct can_filter* f, int len)
{
	int i;
	for(i = 0; i < len; ++i)
		f[i].can_mask = CAN_SFF_MASK;
}

void socketcan_make_slave_filters(struct can_filter* f, int nodeid)
{
	assert(nodeid < 128);

	set_sff_mask(f, CANOPEN_SLAVE_FILTER_LENGTH);

	f[0].can_id = R_NMT;
	f[1].can_id = R_SYNC;
	f[2].can_id = R_TIMESTAMP;
	f[3].can_id = R_RPDO1 + nodeid;
	f[4].can_id = R_RPDO2 + nodeid;
	f[5].can_id = R_RPDO3 + nodeid;
	f[6].can_id = R_RPDO4 + nodeid;
	f[7].can_id = R_RSDO + nodeid;
}

void socketcan_make_master_filters(struct can_filter* f, int nodeid)
{
	assert(nodeid < 128);

	set_sff_mask(f, CANOPEN_MASTER_FILTER_LENGTH);

	f[0].can_id = R_NMT;
	f[1].can_id = R_SYNC;
	f[2].can_id = R_EMCY + nodeid;
	f[3].can_id = R_TIMESTAMP;
	f[4].can_id = R_TPDO1 + nodeid;
	f[5].can_id = R_TPDO2 + nodeid;
	f[6].can_id = R_TPDO3 + nodeid;
	f[7].can_id = R_TPDO4 + nodeid;
	f[8].can_id = R_TSDO + nodeid;
	f[9].can_id = R_HEARTBEAT + nodeid;
}

