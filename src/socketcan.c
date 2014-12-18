#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <assert.h>

#include "canopen.h"
#include "socketcan.h"

static void set_sff_mask(struct can_filter* f, int len)
{
	int i;
	for (i = 0; i < len; ++i)
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

int socketcan_open(const char* iface)
{
	int fd;
	struct sockaddr_can addr;
	struct ifreq ifr;

	fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (fd < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, iface);
	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0)
		goto error;

	memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		goto error;

	return fd;

error:
	close(fd);
	return -1;
}

int socketcan_apply_filters(int fd, struct can_filter* filters, int n)
{
	return setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, filters,
			  n*sizeof(struct can_filter));
}

int socketcan_open_slave(const char* iface, int nodeid)
{
	struct can_filter filters[CANOPEN_SLAVE_FILTER_LENGTH];

	int fd = socketcan_open(iface);
	if (fd < 0)
		return -1;

	socketcan_make_slave_filters(filters, nodeid);

	if (socketcan_apply_filters(fd, filters,
				    CANOPEN_SLAVE_FILTER_LENGTH) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

int socketcan_open_master(const char* iface, int nodeid)
{
	struct can_filter filters[CANOPEN_MASTER_FILTER_LENGTH];

	int fd = socketcan_open(iface);
	if (fd < 0)
		return -1;

	socketcan_make_master_filters(filters, nodeid);

	if (socketcan_apply_filters(fd, filters,
				    CANOPEN_MASTER_FILTER_LENGTH) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

