CC := clang
CFLAGS := -Wall -g -std=c99 -D_GNU_SOURCE -O0 -Iinc/ -I/usr/include/lua5.1
LDFLAGS := -lrt

MAJOR = 0
MINOR = 0
PATCH = 1

LIBCANOPEN = libcanopen.so.$(MAJOR).$(MINOR).$(PATCH)

PREFIX ?= /usr/local

LIBDIR = $(DESTDIR)$(PREFIX)/lib
BINDIR = $(DESTDIR)$(PREFIX)/bin

all: libcanopen.so

$(LIBCANOPEN): src/canopen.o
	$(CC) -shared $(LDFLAGS) $^ -o $@

libcanopen.so: $(LIBCANOPEN)
	ln -fs $(LIBCANOPEN) libcanopen.so

.PHONY: .c.o
.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f *.so*
	rm -f src/*.o tst/*.o
	rm -f tst/test_*

tst/test_sdo_srv: src/sdo_common.o src/sdo_srv.o src/byteorder.o tst/sdo_srv.o
	$(CC) $^ $(LDFLAGS) -o $@

tst/test_sdo_client: src/sdo_common.o src/sdo_client.o src/byteorder.o \
		     tst/sdo_client.o
	$(CC) $^ $(LDFLAGS) -o $@

tst/test_prioq: src/prioq.o tst/prioq_test.o
	$(CC) $^ -o $@

tst/test_worker: src/prioq.o src/worker.o tst/worker_test.o
	$(CC) $^ -pthread -o $@

tst/test_network: src/canopen.o src/byteorder.o src/network.o tst/network_test.o
	$(CC) $^ -o $@

fakenode: src/fakenode.o src/canopen.o src/socketcan.o src/sdo_common.o \
	  src/sdo_srv.o src/byteorder.o
	$(CC) $^ $(LDFLAGS) -llua5.1 -o $@

dlsdo: src/dlsdo.o src/canopen.o src/socketcan.o src/sdo_common.o \
	src/sdo_client.o src/byteorder.o
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY:
test: tst/test_sdo_srv tst/test_sdo_client tst/test_prioq tst/test_worker \
	tst/test_network
	run-parts tst

install:
	install $(LIBCANOPEN) $(LIBDIR)
	install libcanopen.so $(LIBDIR)

# vi: noet sw=8 ts=8 tw=80

