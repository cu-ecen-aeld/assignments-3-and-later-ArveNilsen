CROSS-COMPILE ?=
CC ?= $(CROSS_COMPILE)gcc
# native: make (or make CC=gcc)
# cross: make CROSS_COMPILE=aarch64-none-linux-gnu-

CPPFLAGS ?=
CPPFLAGS += -MMD -MP
-include $(OBJS:.o=.d)

CFLAGS ?= -Wall -Wextra -O2 -g
LDFLAGS ?=
LDLIBS ?=

.DEFAULT_GOAL := aesdsocket
OBJS := aesdsocket.o sb.o handleconn.o

all: aesdsocket

aesdsocket: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean
clean:
	$(RM) %.o aesdsocket
