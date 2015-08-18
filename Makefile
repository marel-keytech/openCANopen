CC := gcc
CXX := g++
COMMON_FLAGS := -Wall -ggdb -D_GNU_SOURCE -O0 -Iinc/
CFLAGS := -std=gnu99 $(COMMON_FLAGS) -fexceptions
CXXFLAGS := -std=gnu++0x $(COMMON_FLAGS)
LDFLAGS := -lrt

PREFIX ?= /usr/local

LIBDIR = $(DESTDIR)$(PREFIX)/lib
BINDIR = $(DESTDIR)$(PREFIX)/bin

all: bin/canopen-master

bin:
	mkdir bin

bin/canopen-master: src/master.o src/sdo_common.o src/sdo_req.o \
		    src/byteorder.o src/network.o src/canopen.o \
		    src/sdo_async.o src/socketcan.o src/legacy-driver.o \
		    src/DriverManager.o src/Driver.o src/rest.o src/http.o \
		    src/eds.o src/ini_parser.o src/types.o
	$(CXX) $^ $(LDFLAGS) -pthread -lappbase -lmloop -ldl -lplog -o $@

bin/canopen-dump: src/canopen-dump.o src/sdo_common.o src/byteorder.o \
		  src/network.o src/canopen.o src/socketcan.o
	$(CC) $^ $(LDFLAGS) -o $@

bin/canbridge: src/canopen.o src/socketcan.o src/network.o src/canbridge.o
	$(CC) $^ $(LDFLAGS) -o $@

bin/fakenode: src/fakenode.o src/canopen.o src/socketcan.o \
	      src/sdo_common.o src/sdo_srv.o src/byteorder.o src/network.o
	$(CC) $^ $(LDFLAGS) -llua5.1 -o $@

.PHONY: .c.o
.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: .cpp.o
.cpp.o:
	$(CXX) -c $(CXXFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f bin/*
	rm -f src/*.o tst/*.o
	rm -f tst/test_* tst/fuzz_test_*

tst/test_sdo_srv: src/sdo_common.o src/sdo_srv.o src/byteorder.o tst/sdo_srv.o
	$(CC) $^ $(LDFLAGS) -o $@

tst/test_network: src/canopen.o src/byteorder.o src/network.o tst/network_test.o
	$(CC) $^ -o $@

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

tst/test_ini_parser: tst/ini_parser_test.o src/ini_parser.o
	$(CC) $^ -o $@

.PHONY:
test: tst/test_sdo_srv tst/test_network tst/test_vector tst/test_sdo_async \
      tst/fuzz_test_sdo_async tst/test_sdo_req tst/test_http \
      tst/test_ini_parser
	run-parts tst

# vi: noet sw=8 ts=8 tw=80

