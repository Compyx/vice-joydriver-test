PROG_CFLAGS = -O3 -g -std=c99 -D_XOPEN_SOURCE=700 \
	      -Wall -Wextra -Wcast-qual -Wshadow -Wconversion -Wsign-compare \
	      -Wformat -Wformat-security -Wmissing-prototypes -Wstrict-prototypes \
	      -Ishared \
	      `pkg-config --cflags libevdev` \
	      `pkg-config --cflags libudev`

PROG_LDFLAGS = `pkg-config --libs libevdev` \
	       `pkg-config --libs libudev`


CC = gcc
LD = $(CC)
VPATH = linux:shared

PROG = vice-joydriver-test
OBJS = main.o cmdline.o lib.o joy.o joyapi.o

$(PROG): $(OBJS)
	$(LD) -o $@ $^ $(PROG_LDFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) $(PROG_CFLAGS) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -rfd $(PROG) $(OBJS)
