
CC = gcc
CFLAGS = -O3 -Wall
LD = gcc
LDFLAGS =
LIBS = -lxmp -lm -lSDL2

ifeq (Darwin,$(shell uname -s))
	CFLAGS += -I/usr/local/include
	LDFLAGS += -framework Cocoa -L/usr/local/lib
endif

.c.o:
	$(CC) -c -o $*.o $(CFLAGS) $<

all: xmdp

xmdp: mdp.o graphics.o parse.o font1.o font2.o
	$(LD) -o $@ $(LDFLAGS) $+ $(LIBS)

clean:
	rm -f core *.o *~

graphics.o parse.o mdp.o: mdp.h
