CC	= gcc
CXX	= g++
UIFC_SRC	=	../../../uifc
XPDEV_SRC	=	../../../xpdev
ODOOR_SRC	=	../../../odoors
CCFLAGS += -O2
CCFLAGS += -g
CCFLAGS += -I${ODOOR_SRC} -L${ODOOR_SRC} -I${XPDEV_SRC} -I${UIFC_SRC}
# Needed for struct alignment!
CCFLAGS += -fshort-enums

all : ny2008 aedit nyibbs nyedit medit

filewrap.o:
	$(CC) $(CCFLAGS) -c ${XPDEV_SRC}/filewrap.c

dirwrap.o:
	$(CC) $(CCFLAGS) -c ${XPDEV_SRC}/dirwrap.c

genwrap.o:
	$(CC) $(CCFLAGS) -c ${XPDEV_SRC}/genwrap.c

ciowrap.o:
	$(CC) $(CCFLAGS) -c ${UIFC_SRC}/ciowrap.c

fights.o : fights.cpp fights.h ny2008.h
	$(CXX) $(CCFLAGS) -c fights.cpp

ibbsny.o : ibbsny.cpp ibbsny.h ny2008.h
	$(CXX) $(CCFLAGS) -c ibbsny.cpp

igmsup.o : igmsup.cpp ny2008.h
	$(CXX) $(CCFLAGS) -c igmsup.cpp

second.o : second.cpp ny2008.h
	$(CXX) $(CCFLAGS) -c second.cpp

chatadd.o : chatadd.cpp ny2008.h
	$(CXX) $(CCFLAGS) -c chatadd.cpp

ny2008.o : ny2008.cpp
	$(CXX) $(CCFLAGS) -c ny2008.cpp

ny2008 : ny2008.o ibbsny.o second.o igmsup.o ibbsny.o fights.o dirwrap.o filewrap.o genwrap.o ciowrap.o chatadd.o
	$(CXX) $(CCFLAGS) ciowrap.o ny2008.o chatadd.o dirwrap.o filewrap.o genwrap.o fights.o ibbsny.o igmsup.o second.o -lcurses -lODoors -lm -o ../ny2008

aedit : aedit.cpp
	$(CXX) $(CCFLAGS) aedit.cpp -o ../aedit

medit : medit.cpp
	$(CXX) $(CCFLAGS) medit.cpp -o ../medit

nyibbs : nyibbs.cpp ibbsny.o filewrap.o genwrap.o dirwrap.o
	$(CXX) $(CCFLAGS) nyibbs.cpp ibbsny.o filewrap.o genwrap.o dirwrap.o -o ../nyibbs

nyedit : nyedit.cpp nyedit.h filewrap.o genwrap.o dirwrap.o ciowrap.o
	$(CXX) $(CCFLAGS) nyedit.cpp filewrap.o genwrap.o dirwrap.o ciowrap.o -lcurses -lODoors -lm -o ../nyedit

clean :
	rm -f *.o
	rm -f ../ny2008
	rm -f ../medit
	rm -f ../aedit
	rm -f ../nyibbs
	rm -f ../nyedit
