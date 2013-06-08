
CC = gcc
CFLAGS = -O3 -Wall
LD = gcc
LDFLAGS =
LIBS = -L../../lib -lxmp -lm

all: xmdp

xmdp: mdp.o font1.o font2.o
	$(LD) -o $@ $(LDFLAGS) $+ -lSDL $(LIBS)

clean:
	rm -f core *.o *~
