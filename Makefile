CC := gcc
CXX := g++
CFLAGS := -Wall -ggdb -std=c99 -D_GNU_SOURCE -O0 -fexceptions -Iinc/ -I/usr/include/lua5.1/
CXXFLAGS := -Wall -ggdb -std=c++0x -D_GNU_SOURCE -O0 -Iinc/
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

canopen-master: src/master.o src/sdo_common.o src/sdo_req.o src/byteorder.o \
		src/network.o src/canopen.o src/sdo_async.o \
		src/socketcan.o src/legacy-driver.o \
		src/DriverManager.o src/Driver.o
	$(CXX) $^ $(LDFLAGS) -pthread -lappbase -lmloop -ldl -lplog -o $@

canopen-dump: src/canopen-dump.o src/sdo_common.o src/byteorder.o \
	      src/network.o src/canopen.o src/socketcan.o
	$(CC) $^ $(LDFLAGS) -o $@

canbridge: src/canopen.o src/socketcan.o src/network.o src/canbridge.o
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY: .c.o
.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: .cpp.o
.cpp.o:
	$(CXX) -c $(CXXFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f *.so*
	rm -f src/*.o tst/*.o
	rm -f tst/test_*

tst/test_sdo_srv: src/sdo_common.o src/sdo_srv.o src/byteorder.o tst/sdo_srv.o
	$(CC) $^ $(LDFLAGS) -o $@

tst/test_sdo_client: src/sdo_common.o src/sdo_client.c src/byteorder.o \
		     tst/sdo_client.c src/ptr_fifo.o
	$(CC) $^ -pthread $(CFLAGS) $(LDFLAGS) -o $@

tst/test_prioq: src/prioq.o tst/prioq_test.o
	$(CC) $^ $(LDFLAGS) -o $@

tst/test_worker: src/prioq.o src/worker.o tst/worker_test.o
	$(CC) $^ $(LDFLAGS) -pthread -o $@

tst/test_network: src/canopen.o src/byteorder.o src/network.o tst/network_test.o
	$(CC) $^ -o $@

tst/test_sdo_fifo: tst/sdo_fifo_test.o
	$(CC) $^ $(LDFLAGS) -o $@

tst/test_frame_fifo: src/frame_fifo.c tst/frame_fifo_test.o
	$(CC) $(CFLAGS) -DFRAME_FIFO_LENGTH=2 $^ $(LDFLAGS) -pthread -o $@

tst/test_vector: tst/vector_test.o
	$(CC) $^ -o $@

tst/test_sdo_async: tst/sdo_async_test.o src/sdo_srv.o src/byteorder.o \
		    src/sdo_common.o src/sdo_async.o src/network.o \
		    src/canopen.o
	$(CC) $^ -o $@

tst/fuzz_test_sdo_async: tst/sdo_async_fuzz_test.o src/sdo_srv.o \
			 src/byteorder.o src/sdo_common.o src/sdo_async.o \
			 src/canopen.o
	$(CC) $^ -o $@

tst/test_sdo_req: tst/sdo_req_test.o src/sdo_req.o
	$(CC) $^ -o $@

tst/test_http: tst/http_test.o src/http.o
	$(CC) $^ -o $@

fakenode: src/fakenode.o src/canopen.o src/socketcan.o src/sdo_common.o \
	  src/sdo_srv.o src/byteorder.o src/network.o
	$(CC) $^ $(LDFLAGS) -llua5.1 -o $@

dlsdo: src/dlsdo.o src/canopen.o src/socketcan.o src/sdo_common.o \
	src/sdo_client.o src/byteorder.o
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY:
test: tst/test_sdo_srv tst/test_sdo_client tst/test_prioq tst/test_worker \
	tst/test_network tst/test_sdo_fifo tst/test_frame_fifo tst/test_vector
	run-parts tst

install:
	install $(LIBCANOPEN) $(LIBDIR)
	install libcanopen.so $(LIBDIR)

# vi: noet sw=8 ts=8 tw=80

