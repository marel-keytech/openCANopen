PRJ_NAME := canopen2
PRJ_TYPE := SOLIB

ADD_CFLAGS := -std=gnu99 -std=gnu++0x -D_GNU_SOURCE -Wextra -fexceptions \
	      -fvisibility=hidden -pthread

ADD_LIBS := mloop appbase dl sharedmalloc plog
ADD_LFLAGS := -pthread -Wl,-rpath=/usr/lib/mloop

ifeq ($(shell marel_getcompilerprefix powerpc),powerpc-marel-linux-gnu)
	ADD_CFLAGS += -flto
	ADD_LFLAGS += -flto
endif

MAIN_SRC := \
	canopen-master.c \
	canbridge.c \
	canopen-dump.c \
	canopen-vnode.c

SRC := \
	master.c \
	sdo_common.c \
	sdo_req.c \
	byteorder.c \
	network.c \
	canopen.c \
	sdo_async.c \
	socketcan.c \
	legacy-driver.cpp \
	DriverManager.cpp \
	Driver.cpp \
	rest.c \
	http.c \
	eds.c \
	ini_parser.c \
	types.c \
	sdo-rest.c \
	conversions.c \
	strlcpy.c \
	canopen_info.c \
	profiling.c \
	sdo_sync.c \
	sdo_srv.c \
	driver.c \
	net-util.c \
	sock.c \
	stream.c \
	dump.c \
	vnode.c \
	sdo-dict.c \
	hexdump.c \
	string-utils.c \
	can-tcp.c

TEST_SRC := \
	unit_arc.c \
	unit_conversions.c \
	unit_http.c \
	unit_init_parser.c \
	unit_network.c \
	unit_sdo_req.c \
	unit_string-utils.c \
	unit_vector.c \
	unit_sdo_async.c \
	unit_rest.c \
	unit_types.c \
	unit_sdo-dict.c \
	sdo_async_fuzz_test.c

include $(MDEV)/make/make.main

# vi: noet sw=8 ts=8 tw=80

