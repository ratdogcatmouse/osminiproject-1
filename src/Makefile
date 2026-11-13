CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu11 -O2
LDFLAGS = -pthread -lrt

TARGET  = campuscompute
SRCS    = main.c jobs.c sched.c logger.c ipc.c banker.c worker.c
HDRS    = jobs.h sched.h logger.h ipc.h banker.h worker.h
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) campus.log

.PHONY: all clean
