CC	= gcc
CXX	= g++
UIFC_SRC	=	../../../../uifc
XPDEV_SRC	=	../../../../xpdev
ODOOR_SRC	=	../../../../odoors
CFLAGS += -O2
CFLAGS += -g
CFLAGS += -I${ODOOR_SRC} -I${XPDEV_SRC} -I${UIFC_SRC}
# Needed for struct alignment!
CFLAGS += -fshort-enums
LDFLAGS += -L${ODOOR_SRC}

all : jackpot

jackpot : jackpot.cpp jackpot.h dirwrap.o genwrap.o
	$(CXX) $(CFLAGS) ${LDFLAGS} jackpot.cpp dirwrap.o genwrap.o -o ../../jackpot -lODoors

dirwrap.o:
	$(CC) $(CFLAGS) -c ${XPDEV_SRC}/dirwrap.c

genwrap.o:
	$(CC) $(CFLAGS) -c ${XPDEV_SRC}/genwrap.c

clean:
	rm -f ../../jackpot
	rm -f *.o
