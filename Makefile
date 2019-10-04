# -*- coding: utf-8 -*-

PROJECT := discord-c

INCLUDES := /usr/local/include/cjson

ifneq ($(OS),Windows_NT)
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
INCLUDES += /usr/local/opt/openssl/include # homebrew
else
INCLUDES += /usr/local/include # *nix
endif # uname -s
endif # not WinNT
CFLAGS := -Wall -std=c99

ifeq ($(NDEBUG),1)
CFLAGS += -O2 -DNDEBUG=1
else
CFLAGS += -O0 -g -UNDEBUG
endif

CFILES := lib/discord-c.c lib/websocket_internal.c lib/websocket.c
HFILES := lib/discord-c.h lib/discord.h lib/websocket_internal.h \
	lib/websocket.h
OFILES := $(CFILES:.c=.c.o)

INCLUDE := $(patsubst %,-I%,$(INCLUDES))

.PHONY: all clean format

all: lib$(PROJECT).a
test: $(PROJECT)-test

%.c.o: %.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

lib$(PROJECT).a: $(OFILES)
	$(AR) -rcs $@ $^

main.o: main.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(PROJECT)-test: lib$(PROJECT).a main.o
	$(CC) -o $@ $^

clean:
	$(RM) lib$(PROJECT).a
	$(RM) $(OFILES)

format: $(CFILES) $(HFILES)
	for _file in $^; do \
		clang-format -i -style=file $$_file; \
	done
