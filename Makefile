VPATH = shared
PROG_CFLAGS = -O3 -g -std=c99 -D_XOPEN_SOURCE=700 \
	      -Wall -Wextra -Wcast-qual -Wshadow -Wconversion -Wsign-compare \
	      -Wformat -Wformat-security -Wmissing-prototypes -Wstrict-prototypes \
	      -Ishared

ifeq ($(OS),Windows_NT)
	UNAME_S := win32
else
	UNAME_S := $(shell uname -s)
endif

ifeq ($(UNAME_S),Linux)
	PROG_CFLAGS += `pkg-config --cflags libevdev`
	PROG_LDFLAGS += `pkg-config --libs libevdev`
	VPATH += :linux
endif

ifeq ($(UNAME_S),win32)
	PROG_LDFLAGS += -ldinput8
	VPATH += :win32
endif

CC = gcc
LD = $(CC)

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
