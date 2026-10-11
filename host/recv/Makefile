# SPDX-License-Identifier: MIT
# sdr-recv: a C11 compiler and POSIX threads.
CC ?= cc
CFLAGS ?= -O2
WARNINGS ?= -Wall -Wextra -Werror
PREFIX ?= /usr/local
override CFLAGS += -std=c11 -pthread $(WARNINGS)
LDLIBS += -pthread

.PHONY: all clean install
all: build/sdr-recv

build/sdr-recv: sdr-recv.c | build
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@

build:
	mkdir -p $@

install: build/sdr-recv
	install -D -m 755 $< $(DESTDIR)$(PREFIX)/bin/sdr-recv

clean:
	rm -rf build
