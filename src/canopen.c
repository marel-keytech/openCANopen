#include <sys/socket.h>
#include <linux/can.h>
#include <assert.h>

#include "canopen.h"

int canopen_get_object_type(struct canopen_msg* msg, struct can_frame* frame)
{
	enum canopen_range id = (enum canopen_range)frame->can_id;

	switch (id) {
	case R_NMT:
		msg->id = 0;
		msg->object = CANOPEN_NMT;
		return 0;
	case R_SYNC:
		msg->id = 0;
		msg->object = CANOPEN_SYNC;
		return 0;
	case R_TIMESTAMP:
		msg->id = 0;
		msg->object = CANOPEN_TIMESTAMP;
		return 0;
	case EMCY_LOW ... EMCY_HIGH:
		msg->id = id - R_EMCY;
		msg->object = CANOPEN_EMCY;
		return 0;
	case TPDO1_LOW ... TPDO1_HIGH:
		msg->id = id - R_TPDO1;
		msg->object = CANOPEN_TPDO1;
		return 0;
	case RPDO1_LOW ... RPDO1_HIGH:
		msg->id = id - R_RPDO1;
		msg->object = CANOPEN_RPDO1;
		return 0;
	case TPDO2_LOW ... TPDO2_HIGH:
		msg->id = id - R_TPDO2;
		msg->object = CANOPEN_TPDO2;
		return 0;
	case RPDO2_LOW ... RPDO2_HIGH:
		msg->id = id - R_RPDO2;
		msg->object = CANOPEN_RPDO2;
		return 0;
	case TPDO3_LOW ... TPDO3_HIGH:
		msg->id = id - R_TPDO3;
		msg->object = CANOPEN_TPDO3;
		return 0;
	case RPDO3_LOW ... RPDO3_HIGH:
		msg->id = id - R_RPDO3;
		msg->object = CANOPEN_RPDO3;
		return 0;
	case TPDO4_LOW ... TPDO4_HIGH:
		msg->id = id - R_TPDO4;
		msg->object = CANOPEN_TPDO4;
		return 0;
	case RPDO4_LOW ... RPDO4_HIGH:
		msg->id = id - R_RPDO4;
		msg->object = CANOPEN_RPDO4;
		return 0;
	case TSDO_LOW ... TSDO_HIGH:
		msg->id = id - R_TSDO;
		msg->object = CANOPEN_TSDO;
		return 0;
	case RSDO_LOW ... RSDO_HIGH:
		msg->id = id - R_RSDO;
		msg->object = CANOPEN_RSDO;
		return 0;
	case HEARTBEAT_LOW ... HEARTBEAT_HIGH:
		msg->id = id - R_HEARTBEAT;
		msg->object = CANOPEN_HEARTBEAT;
		return 0;
	default:
		msg->id = id;
		msg->object = CANOPEN_UNSPEC;
		return -1;
	}
} 

int canopen_get_object_type_simple(struct canopen_msg* msg,
				   struct can_frame* frame, int nodeid)
{
	enum canopen_range cob = (enum canopen_range)frame->can_id;

	assert(nodeid < 128);

	msg->id = nodeid;
	msg->object = nodeid > cob ? cob - nodeid : (enum canopen_object)cob;

	return 0;
}

const char* canopen_object_type_to_string(enum canopen_object obj)
{
	switch (obj) {
	case CANOPEN_NMT:	return "NMT";
	case CANOPEN_SYNC:	return "SYNC";
	case CANOPEN_EMCY:	return "EMCY";
	case CANOPEN_TIMESTAMP:	return "TIMESTAMP";
	case CANOPEN_TPDO1 ... CANOPEN_RPDO4:
				return "PDO";
	case CANOPEN_TSDO ... CANOPEN_RSDO:
				return "SDO";
	case CANOPEN_HEARTBEAT:	return "HEARTBEAT";
	default:		return "UNKNOWN";
	}
}

const char* canopen_object_type_to_string_exact(enum canopen_object obj)
{
	switch (obj) {
	case CANOPEN_NMT:	return "NMT";
	case CANOPEN_SYNC:	return "SYNC";
	case CANOPEN_EMCY:	return "EMCY";
	case CANOPEN_TIMESTAMP:	return "TIMESTAMP";
	case CANOPEN_TPDO1:	return "TPDO1";
	case CANOPEN_RPDO1:	return "RPDO1";
	case CANOPEN_TPDO2:	return "TPDO2";
	case CANOPEN_RPDO2:	return "RPDO2";
	case CANOPEN_TPDO3:	return "TPDO3";
	case CANOPEN_RPDO3:	return "RPDO3";
	case CANOPEN_TPDO4:	return "TPDO4";
	case CANOPEN_RPDO4:	return "RPDO4";
	case CANOPEN_TSDO:	return "TSDO";
	case CANOPEN_RSDO:	return "RSDO";
	case CANOPEN_HEARTBEAT:	return "HEARTBEAT";
	default:		return "UNKNOWN";
	}
}

