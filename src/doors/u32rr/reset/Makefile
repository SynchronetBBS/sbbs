CFLAGS += -g -std=c99 -Wall -pedantic
CFLAGS += -I..

vpath %.c ..

all: edabody.o edarmor.o edarms.o edbody.o eddrink.o edface.o edfeets.o edfood.o edhands.o edhead.o edlegs.o edmonst.o edmonst2.o edneck.o edrings.o edshield.o edwaist.o edweap.o edweap01.o edweap2.o edweap3.o reset.o eweapon.o

reset: edabody.o edarmor.o edarms.o edbody.o eddrink.o edface.o edfeets.o edfood.o edhands.o edhead.o edlegs.o edneck.o edrings.o edshield.o edwaist.o edweap.o edweap01.o edweap2.o edweap3.o edmonst.o edmonst2.o eweapon.o reset.o files.o

clean:
	rm -rf *.o
