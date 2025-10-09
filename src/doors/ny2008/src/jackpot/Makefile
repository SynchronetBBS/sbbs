CC	= gcc
CXX	= g++
UIFC_SRC	=	../../../../uifc
XPDEV_SRC	=	../../../../xpdev
ODOOR_SRC	=	../../../../odoors
CIOLIB_SRC	=	../../../../conio
CFLAGS += -O2
CFLAGS += -g
CFLAGS += -DNO_OS_VERSION
CFLAGS += -I${ODOOR_SRC} -I${XPDEV_SRC} -I${UIFC_SRC} -I${CIOLIB_SRC}
CFLAGS += -DHAS_RANDOM_FUNC
# Needed for struct alignment!
CFLAGS += -fshort-enums
LDFLAGS += -L${ODOOR_SRC}/libs-`uname`

all : jackpot

jackpot : jackpot.cpp jackpot.h dirwrap.o filewrap.o genwrap.o
	$(CXX) $(CFLAGS) ${LDFLAGS} jackpot.cpp dirwrap.o filewrap.o genwrap.o -o ../../jackpot -lODoors

dirwrap.o:
	$(CC) $(CFLAGS) -c ${XPDEV_SRC}/dirwrap.c

filewrap.o:
	$(CC) $(CFLAGS) -c ${XPDEV_SRC}/filewrap.c

genwrap.o:
	$(CC) $(CFLAGS) -c ${XPDEV_SRC}/genwrap.c

clean:
	rm -f ../../jackpot
	rm -f *.o
