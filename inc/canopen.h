#ifndef _CANOPEN_H
#define _CANOPEN_H


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


#endif /* _CANOPEN_H */

