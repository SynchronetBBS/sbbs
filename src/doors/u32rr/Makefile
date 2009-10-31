CFLAGS += -g -std=c99 -Wall -pedantic
CFLAGS += -I../../conio
LDFLAGS += -lcurses -lm -pthread

testmain: Alchemisty.o BobsBeer.o Status.o ShadyShops.o IO.o Config.o Bash.o Races.o Classes.o files.o str.o testmain.o various.o ../../conio/gcc.freebsd.lib.ace.hurd.local/libciolib_mt.a ../../xpdev/gcc.freebsd.lib.ace.hurd.local/libxpdev_mt.a

clean:
	rm -rf *.o
