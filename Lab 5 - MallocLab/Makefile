#
# Students' Makefile for the Malloc Lab
#
VERSION = 1
HANDINDIR = /users/groups/cs224ta/malloclab

CC = gcc
#CFLAGS = -Wall -O2 -m32
CFLAGS = -Wall -m32 -g -pg 

OBJS = mdriver.o mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o

mdriver: $(OBJS)
	$(CC) $(CFLAGS) -o mdriver $(OBJS)

mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h
memlib.o: memlib.c memlib.h
mm.o: mm.c mm.h memlib.h
fsecs.o: fsecs.c fsecs.h config.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h

handin:
	install -m660 mm.c $(HANDINDIR)/$(USER)-$(VERSION)-mm.c

clean:
	rm -f *~ *.o mdriver


