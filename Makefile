ALL_C_FILE:=$(wildcard *.c)
CC = gcc
CFLAGS = -O3 -Wall -ggdb3 
LDFLAGS = 
TARGETS = client server
OBJS = $(patsubst %.c,%.o,$(ALL_C_FILE))

all: filetransfer 

filetransfer: $(TARGETS)

$(TARGETS): %: %.o common.o addr_helper.o  
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean: 
	rm -f $(OBJS) $(TARGETS) dependencies 

dependencies: $(OBJS:.o=.c)
	$(CC) -MM $^ $(CFLAGS) > $@
	
include dependencies
