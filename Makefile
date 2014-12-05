CC := gcc
CFLAGS := -g -Wall -std=c99 -D_GNU_SOURCE -O0 -Iinc/
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

tst/sdo: src/sdo.o tst/sdo.o
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY:
test: tst/sdo
	run-parts tst

install:
	install $(LIBCANOPEN) $(LIBDIR)
	install libcanopen.so $(LIBDIR)

# vi: noet sw=8 ts=8 tw=80

