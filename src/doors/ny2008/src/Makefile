CC	= gcc
CXX	= g++
UIFC_SRC	=	../../../uifc
XPDEV_SRC	=	../../../xpdev
ODOOR_SRC	=	../../../odoors
CFLAGS += -O2
CFLAGS += -g
CFLAGS += -I${ODOOR_SRC} -I${XPDEV_SRC} -I${UIFC_SRC}
# Needed for struct alignment!
CFLAGS += -fshort-enums
LDFLAGS += -L${ODOOR_SRC}

all : ny2008 aedit nyibbs nyedit medit

filewrap.o:
	$(CC) $(CFLAGS) -c ${XPDEV_SRC}/filewrap.c

dirwrap.o:
	$(CC) $(CFLAGS) -c ${XPDEV_SRC}/dirwrap.c

genwrap.o:
	$(CC) $(CFLAGS) -c ${XPDEV_SRC}/genwrap.c

ciowrap.o:
	$(CC) $(CFLAGS) -c ${UIFC_SRC}/ciowrap.c

fights.o : fights.cpp fights.h ny2008.h
	$(CXX) $(CFLAGS) -c fights.cpp

ibbsny.o : ibbsny.cpp ibbsny.h ny2008.h
	$(CXX) $(CFLAGS) -c ibbsny.cpp

igmsup.o : igmsup.cpp ny2008.h
	$(CXX) $(CFLAGS) -c igmsup.cpp

second.o : second.cpp ny2008.h
	$(CXX) $(CFLAGS) -c second.cpp

chatadd.o : chatadd.cpp ny2008.h
	$(CXX) $(CFLAGS) -c chatadd.cpp

ny2008.o : ny2008.cpp
	$(CXX) $(CFLAGS) -c ny2008.cpp

ny2008 : ny2008.o ibbsny.o second.o igmsup.o ibbsny.o fights.o dirwrap.o filewrap.o genwrap.o ciowrap.o chatadd.o
	$(CXX) $(CFLAGS) ${LDFLAGS} ciowrap.o ny2008.o chatadd.o dirwrap.o filewrap.o genwrap.o fights.o ibbsny.o igmsup.o second.o -lcurses -lODoors -lm -o ../ny2008

aedit : aedit.cpp
	$(CXX) $(CFLAGS) ${LDFLAGS} aedit.cpp -o ../aedit

medit : medit.cpp
	$(CXX) $(CFLAGS) ${LDFLAGS} medit.cpp -o ../medit

nyibbs : nyibbs.cpp ibbsny.o filewrap.o genwrap.o dirwrap.o
	$(CXX) $(CFLAGS) ${LDFLAGS} nyibbs.cpp ibbsny.o filewrap.o genwrap.o dirwrap.o -o ../nyibbs

nyedit : nyedit.cpp nyedit.h filewrap.o genwrap.o dirwrap.o ciowrap.o
	$(CXX) $(CFLAGS) ${LDFLAGS} nyedit.cpp filewrap.o genwrap.o dirwrap.o ciowrap.o -lcurses -lODoors -lm -o ../nyedit

clean :
	rm -f *.o
	rm -f ../ny2008
	rm -f ../medit
	rm -f ../aedit
	rm -f ../nyibbs
	rm -f ../nyedit
