FILESYSTEM	:= filesystem.c

SRCS    := ${FILESYSTEM} misc.c
SRCS2	:= mkfs.c
OBJS    := ${SRCS:.c=.o} 
OBJS2    := ${SRCS2:.c=.o} 

CFLAGS = -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse -pthread -g -Wall
LDFLAGS = -L/lib -lfuse -ldl
TARGET = filesystem
TARGET2 = mkfs

.PHONY: all clean

all: ${TARGET} ${TARGET2}

${OBJS} ${OBJS2}: %.o: %.c
	gcc ${CFLAGS} -o $@ -c $<


${TARGET}: ${OBJS}
	gcc -o $@ $^ ${LDFLAGS}

${TARGET2}: ${OBJS2}
	gcc -o $@ $^

clean::
	rm -f *.o *~ ${TARGET} ${TARGET2}


