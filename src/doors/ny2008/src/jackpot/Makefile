CC	= gcc
CXX	= g++
UIFC_SRC	=	../../../../uifc
XPDEV_SRC	=	../../../../xpdev
ODOOR_SRC	=	../../../../odoors
CCFLAGS += -O2
CCFLAGS += -g
CCFLAGS += -I${ODOOR_SRC} -L${ODOOR_SRC} -I${XPDEV_SRC} -I${UIFC_SRC}
# Needed for struct alignment!
CCFLAGS += -fshort-enums

all : jackpot

jackpot : jackpot.cpp jackpot.h dirwrap.o genwrap.o
	$(CXX) $(CCFLAGS) jackpot.cpp dirwrap.o genwrap.o -o ../../jackpot -lODoors

dirwrap.o:
	$(CC) $(CCFLAGS) -c ${XPDEV_SRC}/dirwrap.c

genwrap.o:
	$(CC) $(CCFLAGS) -c ${XPDEV_SRC}/genwrap.c

clean:
	rm -f ../../jackpot
	rm -f *.o
