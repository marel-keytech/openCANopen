#ifndef _CANOPEN_H
#define _CANOPEN_H

enum canopen_range {
	R_NMT = 0,
	R_SYNC = 0x80,
	R_EMCY = 0x80, EMCY_LOW = R_EMCY + 1, EMCY_HIGH = R_EMCY + 0x7f,
	R_TIMESTAMP = 0x100,
	R_TPDO1 = 0x180, TPDO1_LOW = R_TPDO1, TPDO1_HIGH = R_TPDO1 + 0x7f,
	R_RPDO1 = 0x200, RPDO1_LOW = R_RPDO1, RPDO1_HIGH = R_RPDO1 + 0x7f,
	R_TPDO2 = 0x280, TPDO2_LOW = R_TPDO2, TPDO2_HIGH = R_TPDO2 + 0x7f,
	R_RPDO2 = 0x300, RPDO2_LOW = R_RPDO2, RPDO2_HIGH = R_RPDO2 + 0x7f,
	R_TPDO3 = 0x380, TPDO3_LOW = R_TPDO3, TPDO3_HIGH = R_TPDO3 + 0x7f,
	R_RPDO3 = 0x400, RPDO3_LOW = R_RPDO3, RPDO3_HIGH = R_RPDO3 + 0x7f,
	R_TPDO4 = 0x480, TPDO4_LOW = R_TPDO4, TPDO4_HIGH = R_TPDO4 + 0x7f,
	R_RPDO4 = 0x500, RPDO4_LOW = R_RPDO4, RPDO4_HIGH = R_RPDO4 + 0x7f,
	R_TSDO  = 0x580, TSDO_LOW  = R_TSDO , TSDO_HIGH  = R_TSDO  + 0x7f,
	R_RSDO  = 0x600, RSDO_LOW  = R_RSDO , RSDO_HIGH  = R_RSDO  + 0x7f,
	R_HEARTBEAT = 0x700, HEARTBEAT_LOW = R_HEARTBEAT,
	HEARTBEAT_HIGH = R_HEARTBEAT + 0x7f,
};

enum canopen_object {
	CANOPEN_UNSPEC		= 0,
	CANOPEN_NONE		= 0,
	CANOPEN_NMT		= 1 << 0,
	CANOPEN_SYNC		= 1 << 1,
	CANOPEN_EMCY		= 1 << 2,
	CANOPEN_TIMESTAMP	= 1 << 3,
	CANOPEN_TPDO1		= 1 << 4,
	CANOPEN_RPDO1		= 1 << 5,
	CANOPEN_TPDO2		= 1 << 6,
       	CANOPEN_RPDO2		= 1 << 7,
	CANOPEN_TPDO3		= 1 << 8,
       	CANOPEN_RPDO3		= 1 << 9,
       	CANOPEN_TPDO4		= 1 << 10,
       	CANOPEN_RPDO4		= 1 << 11,
       	CANOPEN_TSDO		= 1 << 12,
	CANOPEN_RSDO		= 1 << 13,
       	CANOPEN_HEARTBEAT	= 1 << 14,
	CANOPEN_MASK		= (CANOPEN_HEARTBEAT << 1) - 1
};

struct canopen_msg {
	int id;
	enum canopen_object object;
};

int canopen_get_object_type(struct canopen_msg* msg,
			    const struct can_frame* frame);

#endif /* _CANOPEN_H */

