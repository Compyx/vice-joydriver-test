#CC = gcc
#LD = $(CC)
VPATH = src/shared

# -Wenum-int-mismatch appears to be a GCC 13 addition
PROG_CFLAGS = -O0 -g -std=c99 \
	      -Wall -Wextra -Wcast-qual -Wshadow -Wconversion -Wsign-compare \
	      -Wsign-conversion \
	      -Wformat -Wformat-security -Wmissing-prototypes -Wstrict-prototypes \
	      -Isrc -Isrc/shared

ifeq ($(OS),Windows_NT)
	UNAME_S := win32
else
	UNAME_S := $(shell uname -s)
endif

ifeq ($(UNAME_S),Linux)
	CC = gcc
	LD = $(CC)
	PROG_CFLAGS += `pkg-config --cflags libevdev` -D_XOPEN_SOURCE=700 -DUNIX_COMPILE -DLINUX_COMPILE -Ilinux
	PROG_LDFLAGS += `pkg-config --libs libevdev`
	VPATH += :src/linux
endif

ifeq ($(UNAME_S),NetBSD)
	CC = gcc
	LD = $(CC)
	PROG_CFLAGS += -D_NETBSD_SOURCE -DUNIX_COMPILE -DNETBSD_COMPILE
	PROG_LDFLAGS += -lusbhid
	VPATH += :src/bsd
endif

ifeq ($(UNAME_S),FreeBSD)
	CC = clang
	LD = $(CC)
	PROG_CFLAGS += -D_XOPEN_SOURCE=700 -DUNIX_COMPILE -DFREEBSD_COMPILE
	PROG_LDFLAGS += -lusb -lusbhid
	VPATH += :src/bsd
endif

ifeq ($(UNAME_S),win32)
	CC = gcc
	LD = $(CC)
	PROG_CFLAGS += -Wenum-int-mismatch -DWINDOWS_COMPILE
	PROG_LDFLAGS += -ldinput8
	VPATH += :src/win32
endif

PROG = vice-joydriver-test
OBJS = main.o cmdline.o lib.o joy.o joyapi.o joymap.o uiactions.o

all: $(PROG)

cmdline.o: lib.o cmdline.h
lib.o: lib.h
joy.o: lib.o joyapi.o joyapi-types.h
joyapi.o: lib.o joymap.o uiactions.o joyapi.h joyapi-types.h
joymap.o: lib.o joymap.h uiactions.o joyapi-types.h
main.o: cmdline.o joy.o joyapi.o lib.o
uiactions.o: uiactions.h machine.h

$(PROG): $(OBJS)
	$(LD) -o $@ $^ $(PROG_LDFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) $(PROG_CFLAGS) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -rfd $(PROG) $(OBJS)
