# Makefile for CampusCompute (Ubuntu / WSL Ubuntu, gcc + make)
CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu11 -g
LDFLAGS = -pthread -lrt
TARGET  = campuscompute
SRCS    = main.c jobs.c scheduler.c logger.c ipc.c banker.c worker.c limits.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c campuscompute.h
	$(CC) $(CFLAGS) -c $< -o $@

# run the integrated demonstration
test: $(TARGET)
	./$(TARGET) demo

# remove only the shared memory / semaphores created by this program
cleanipc: $(TARGET)
	./$(TARGET) cleanup

# remove build output and the generated log file
clean:
	rm -f $(OBJS) $(TARGET) campuscompute.log

.PHONY: all test cleanipc clean
