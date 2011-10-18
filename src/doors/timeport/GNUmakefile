VER	:=	1.2
CFLAGS	+=	-Wall
STRIP	:=

ifdef DEBUG
 CFLAGS +=	-g -Wshadow -Wcast-align -Wconversion -Wredundant-decls
else
 CFLAGS +=	-O2
 STRIP	+=	strip
endif

ifdef SEPOS
 OS	:=	$(shell uname | tr "[ A-Z]" "[\-a-z]")
 LIBPREFIX	:=	./libs.$(OS)/
 BINSUFFIX	:=	.$(OS)
else
 LIBPREFIX	:= ./
endif

BINARIES	:= timeport$(BINSUFFIX) tp-event$(BINSUFFIX) makemod$(BINSUFFIX) outside$(BINSUFFIX) \
		   badtimes$(BINSUFFIX) stoneage$(BINSUFFIX) twenties$(BINSUFFIX) install$(BINSUFFIX)

ifdef STATIC
 REQLIBS :=	$(LIBPREFIX)tplib.o $(LIBPREFIX)doors.o
 LIBS	:=	$(LIBPREFIX)tplib.o $(LIBPREFIX)doors.o
else
 REQLIBS :=	$(LIBPREFIX)libtp.so
 LIBS	:=	-ltp
 CFLAGS +=	-L$(LIBPREFIX)
 BINARIES	+=	$(LIBPREFIX)libtp.so
 LFLAGS	+=      -Xlinker -rpath
 LFLAGS	+=      -Xlinker $(LIBPREFIX)
endif

ifeq ($(OS),netbsd)
 CURSLIB	:=	-lncurses
 CFLAGS 	+=	-I/usr/pkg/include -L/usr/pkg/lib
else
 CURSLIB	:=	-lcurses
endif

all: ${BINARIES} ${STRIP}

install$(BINSUFFIX): install.c
	gcc ${CFLAGS} -o install$(BINSUFFIX) install.c $(CURSLIB)

twenties$(BINSUFFIX): ${REQLIBS} twenties.c
	gcc ${CFLAGS} ${LFLAGS} $(LIBS) -o twenties$(BINSUFFIX) twenties.c

makemod$(BINSUFFIX): ${REQLIBS} makemod.c
	gcc ${CFLAGS} ${LFLAGS} $(LIBS) -o makemod$(BINSUFFIX) makemod.c

stoneage$(BINSUFFIX): ${REQLIBS} stoneage.c
	gcc ${CFLAGS} ${LFLAGS} $(LIBS) -o stoneage$(BINSUFFIX) stoneage.c

timeport$(BINSUFFIX): ${REQLIBS} timeport.c timeport.h
	gcc ${CFLAGS} ${LFLAGS} $(LIBS) -o timeport$(BINSUFFIX) timeport.c

tp-event$(BINSUFFIX): ${REQLIBS} tp-event.c
	gcc ${CFLAGS} ${LFLAGS} $(LIBS) -o tp-event$(BINSUFFIX) tp-event.c

outside$(BINSUFFIX): ${REQLIBS} outside.c
	gcc ${CFLAGS} ${LFLAGS} $(LIBS) -o outside$(BINSUFFIX) outside.c

badtimes$(BINSUFFIX): ${REQLIBS} badtimes.c
	gcc ${CFLAGS} ${LFLAGS} $(LIBS) -o badtimes$(BINSUFFIX) badtimes.c

$(LIBPREFIX)tplib.o: tplib.c tplib.h doors.h $(LIBPREFIX)
	gcc -c ${CFLAGS} -o $(LIBPREFIX)tplib.o tplib.c

$(LIBPREFIX)doors.o: doors.c doors.h tplib.h $(LIBPREFIX)
	gcc ${CFLAGS} -o $(LIBPREFIX)doors.o -c doors.c

$(LIBPREFIX)libtp.so: $(LIBPREFIX)tplib.o $(LIBPREFIX)doors.o $(LIBPREFIX)
	gcc $(CFLAGS) -shared -o $(LIBPREFIX)libtp.so.$(VER) $(LIBPREFIX)tplib.o $(LIBPREFIX)doors.o
	@cd $(LIBPREFIX) && ln -fs libtp.so.$(VER) libtp.so

$(LIBPREFIX):
	mkdir $(LIBPREFIX)

clean:
	-rm \
	$(LIBPREFIX)doors.o \
	$(LIBPREFIX)tplib.o \
	timeport$(BINSUFFIX) \
	tp-event$(BINSUFFIX) \
	makemod$(BINSUFFIX) \
	outside$(BINSUFFIX) \
	badtimes$(BINSUFFIX) \
	stoneage$(BINSUFFIX) \
	twenties$(BINSUFFIX) \
	install$(BINSUFFIX) \
	$(LIBPREFIX)libtp.so \
	$(LIBPREFIX)libtp.so.$(VER)

strip:
	strip ${BINARIES}
