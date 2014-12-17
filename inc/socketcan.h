#ifndef _CANOPEN_SOCKETCAN_H
#define _CANOPEN_SOCKETCAN_H

#include <linux/can.h>

#define CANOPEN_SLAVE_FILTER_LENGTH 8
#define CANOPEN_MASTER_FILTER_LENGTH 10

void socketcan_make_slave_filters(struct can_filter* filters, int nodeid);
void socketcan_make_master_filters(struct can_filter* filters, int nodeid);

#endif /* _CANOPEN_SOCKETCAN_H */

