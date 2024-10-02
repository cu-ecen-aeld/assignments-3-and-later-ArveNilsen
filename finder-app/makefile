# Defines the variable if it is not set at the command line.
# Help with error handling
CROSS-COMPILE?=native

ifeq ($(CROSS-COMPILE), native)
	CC:=gcc
else ifeq ($(CROSS-COMPILE), aarch64-none-linux-gnu-)
	CC:=aarch64-none-linux-gnu-gcc
else
	echo "Invalid CROSS-COMPILE option"
	exit 1
endif

writer: writer.o
	${CC} ${CFLAGS} writer.o -o writer

writer.o: writer.c
	${CC} ${CFLAGS} -c writer.c -o writer.o

.PHONY: clean
clean:
	rm -f writer.o writer
