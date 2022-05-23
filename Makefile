CC = gcc
CFLAGS = -c -pthread -Wall -Wextra
LDFLAGS = -pthread 

all: CUT

CUT: CUT.o
	$(CC) $(LDFLAGS) CUT.o -o CUT

CUT.o: CUT.c
	$(CC) $(CFLAGS) CUT.c 

