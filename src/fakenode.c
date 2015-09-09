#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <sys/socket.h> // for definition of sa_family_t
#include <linux/can.h>
#include "canopen.h"
#include "socketcan.h"
#include "canopen/sdo_srv.h"
#include "canopen/byteorder.h"
#include "canopen/network.h"
#include "canopen/nmt.h"

static struct sdo_srv sdo_srv_;
static lua_State* lua_state_;
static int nodeid_;
static void* current_sdo_ = NULL;
static size_t current_sdo_size_ = 0;

static void usage()
{
	fprintf(stderr, "Usage: fakenode <interface> <node id> <config path>\n");
}

static lua_State* load_config(const char* path)
{
	lua_State* L = luaL_newstate();
	if (!L)
		return NULL;

	luaL_openlibs(L);

	if (luaL_dofile(L, path) != 0)
	{
		fprintf(stderr, "Config error: %s\n", lua_tostring(L, -1));
		lua_close(L);
		return NULL;
	}
	
	lua_gc(L, LUA_GCCOLLECT, 0);

	return L;
}

static inline void close_config(lua_State* L)
{
	lua_close(L);
}

static int my_sdo_get_obj(struct sdo_obj* obj, int index, int subindex)
{
	const char* data;
	size_t size;
	lua_State* L = lua_state_;
	
	uint32_t integer32;
	uint16_t integer16;

	lua_getglobal(L, "obj_dict");
	/* { table or nil } */

	if (lua_isnil(L, -1))
		goto failure;

	lua_pushnumber(L, index);
	/* { table, number } */

	lua_gettable(L, -2);
	/* { table, table or nil} */

	if (lua_isnil(L, -1))
		goto failure;

	lua_pushnumber(L, subindex);
	/* { table, table, number } */

	lua_gettable(L, -2);
	/* { table, table, any } */

	int type = lua_type(L, -1);
	if (type == LUA_TSTRING) {
		data = lua_tolstring(L, -1, &size);
		++size;
		memcpy(current_sdo_, data, size);
	} else if (type == LUA_TTABLE) {
		lua_pushnumber(L, 1);
		lua_gettable(L, -2);
		assert(!lua_isnil(L, -1));
		
		size = lua_tointeger(L, -1);
		lua_pop(L, 1);

		lua_pushnumber(L, 2);
		lua_gettable(L, -2);
		assert(!lua_isnil(L, -1));

		switch (size) {
		case 1: *((char*)current_sdo_) = lua_tointeger(L, -1);
			break;
		case 2: integer16 = lua_tointeger(L, -1);
			byteorder(current_sdo_, &integer16, 2);
			break;
		case 4: integer32 = lua_tointeger(L, -1);
			byteorder(current_sdo_, &integer32, 4);
			break;
		}
	} else {
		abort();
	}

	/*
	if (current_sdo_) {
		if (current_sdo_size_ < size)
		{
			current_sdo_size_ = size;
			current_sdo_ = realloc(current_sdo_, current_sdo_size_);
			assert(current_sdo_);
		}
	} else {
		current_sdo_size_ = size;
		current_sdo_ = malloc(current_sdo_size_);
	}
	*/


	obj->flags = SDO_OBJ_R | SDO_OBJ_LE;
	obj->addr = current_sdo_;
	obj->size = size;

	lua_settop(L, 0);
	return 0;

failure:
	lua_settop(L, 0);
	return -1;
}

static int process_rsdo(int fd, struct canopen_msg* msg,
			struct can_frame* frame)
{
	if (sdo_srv_feed(&sdo_srv_, frame)) {
		sdo_srv_.out_frame.can_id = TSDO_LOW + nodeid_;
		sdo_srv_.out_frame.can_dlc = 8;
		assert(write(fd, &sdo_srv_.out_frame, sizeof(struct can_frame))
		       == sizeof(struct can_frame));
	}

	return 0;
}

static void send_bootup(int fd)
{
	struct can_frame cf;
	//cf.can_id = R_EMCY + nodeid_;
	//cf.can_dlc = 0;
	cf.can_id = R_HEARTBEAT + nodeid_;
	cf.can_dlc = 1;
	cf.data[0] = 0;

	while (write(fd, &cf, sizeof(cf)) != sizeof(cf))
		usleep(1000);
}

static void reset_communication(int fd)
{
	sdo_srv_init(&sdo_srv_);
	send_bootup(fd);
}

static int process_nmt(int fd, struct canopen_msg* msg,
		       struct can_frame* frame)
{
	int command = frame->data[0];
	int nodeid = frame->data[1];

	if (nodeid != 0 && nodeid != nodeid_)
		return 0;

//	if (command == 0x81 || command == 0x82)
//		reset_communication(fd);

	return 0;
}

static int process_node_guarding(int fd)
{
	struct can_frame cf = { 0 };
	cf.can_id = R_HEARTBEAT + nodeid_;
	cf.can_dlc = 1;
	cf.data[0] = NMT_STATE_OPERATIONAL;

	while (write(fd, &cf, sizeof(cf)) != sizeof(cf))
		usleep(1000);

	return 0;
}

static int process_single_message(int fd, struct canopen_msg* msg,
				  struct can_frame* frame)
{
	switch (msg->object)
	{
	case CANOPEN_HEARTBEAT: return process_node_guarding(fd);
	case CANOPEN_NMT:  return process_nmt(fd, msg, frame);
	case CANOPEN_RSDO: return process_rsdo(fd, msg, frame);
	default: break;
	}

	return 0;
}

static void process_can_messages(int fd)
{
	struct can_frame cf;
	struct canopen_msg msg;

	while (1) {
		assert(read(fd, &cf, sizeof(cf)) == sizeof(cf));
		assert(canopen_get_object_type(&msg, &cf) == 0);
		assert(process_single_message(fd, &msg, &cf) == 0);
	}
}

int main(int argc, char* argv[])
{
	if (argc < 4) {
		usage();
		return 1;
	}

	const char* interface = argv[1];
	const char* nodeid = argv[2];
	const char* config_path = argv[3];

	lua_State* L = load_config(config_path);
	if (!L)
		return 1;

	current_sdo_size_ = 42;
	current_sdo_ = malloc(current_sdo_size_);

	sdo_srv_init(&sdo_srv_);
	sdo_get_obj = my_sdo_get_obj;
	lua_state_ = L;
	nodeid_ = atoi(nodeid);

	assert(nodeid_ < 128);

	int fd = socketcan_open_slave(interface, nodeid_);
	if (fd < 0) {
		perror("Failed to open can interface");
		goto failure;
	}

	net_fix_sndbuf(fd);

	send_bootup(fd);

	process_can_messages(fd);

	close(fd);
	close_config(L);
	return 0;

failure:
	close_config(L);
	return 1;
}

