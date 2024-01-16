#CC = gcc
#LD = $(CC)
VPATH = shared
PROG_CFLAGS = -O3 -g -std=c99 \
	      -Wall -Wextra -Wcast-qual -Wshadow -Wconversion -Wsign-compare \
	      -Wsign-conversion \
	      -Wformat -Wformat-security -Wmissing-prototypes -Wstrict-prototypes \
	      -Ishared

ifeq ($(OS),Windows_NT)
	UNAME_S := win32
else
	UNAME_S := $(shell uname -s)
endif

ifeq ($(UNAME_S),Linux)
	CC = gcc
	LD = $(CC)
	PROG_CFLAGS += `pkg-config --cflags libevdev` -D_XOPEN_SOURCE=700 -DUNIX_COMPILE -DLINUX_COMPILE
	PROG_LDFLAGS += `pkg-config --libs libevdev`
	VPATH += :linux
endif

ifeq ($(UNAME_S),NetBSD)
	CC = gcc
	LD = $(CC)
	PROG_CFLAGS += -D_NETBSD_SOURCE -DUNIX_COMPILE -DNETBSD_COMPILE
	PROG_LDFLAGS += -lusbhid
	VPATH += :bsd
endif

ifeq ($(UNAME_S),FreeBSD)
	CC = clang
	LD = $(CC)
	PROG_CFLAGS += -D_XOPEN_SOURCE=700 -DUNIX_COMPILE -DFREEBSD_COMPILE
	PROG_LDFLAGS += -lusb -lusbhid
	VPATH += :bsd
endif

ifeq ($(UNAME_S),win32)
	PROG_CFLAGS += -DWINDOWS_COMPILE
	PROG_LDFLAGS += -ldinput8
	VPATH += :win32
endif

PROG = vice-joydriver-test
OBJS = main.o cmdline.o lib.o joy.o joyapi.o

all: $(PROG)

cmdline.o: lib.o
joy.o: lib.o joyapi.o
main.o: cmdline.o joy.o joyapi.o lib.o

$(PROG): $(OBJS)
	$(LD) -o $@ $^ $(PROG_LDFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) $(PROG_CFLAGS) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -rfd $(PROG) $(OBJS)
