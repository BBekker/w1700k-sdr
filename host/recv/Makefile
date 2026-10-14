# SPDX-License-Identifier: MIT
# sdr-recv: a C11 compiler; it reads w1700k-stream's shared memory (../stream/include).
CC ?= cc
include ../common/build.mk
CFLAGS ?= -O2
WARNINGS ?= -Wall -Wextra -Werror
PREFIX ?= /usr/local
override CFLAGS += -std=c11 -I../stream/include $(WARNINGS)
LDLIBS += $(RT_LIBS)

.PHONY: all clean install
all: build/sdr-recv

build/sdr-recv: sdr-recv.c ../stream/include/w1700k_stream.h | build
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@

build:
	mkdir -p $@

install: build/sdr-recv
	install -D -m 755 $< $(DESTDIR)$(PREFIX)/bin/sdr-recv

clean:
	rm -rf build
