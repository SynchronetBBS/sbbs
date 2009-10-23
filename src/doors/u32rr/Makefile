CFLAGS += -g -std=c99 -Wall -pedantic
CFLAGS += -I../../conio
LDFLAGS += -lcurses -lm -pthread

all: Alchemisty.o ArmorShop.o ShadyShops.o IO.o Config.o files.o various.o test

testmain: Alchemisty.o ShadyShops.o IO.o Config.o files.o testmain.o various.o ../../conio/gcc.freebsd.lib.ace.hurd.local/libciolib_mt.a ../../xpdev/gcc.freebsd.lib.ace.hurd.local/libxpdev_mt.a

clean:
	rm -rf *.o
