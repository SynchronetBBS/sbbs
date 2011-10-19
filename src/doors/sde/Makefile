ODDIR	:=	../../odoors
ODLIBS	:=	../../odoors/libs-`uname`

CC      ?=	gcc
CFLAGS  +=	-Wall -ansi -DUNIX -g
SRCDIR	:=	./src/
CFLAGS	+=	-I${ODDIR}
LDFLAGS +=	-L${ODLIBS} -lODoors

SRCS = mci.c dynasty.c commafmt.c sde_input.c
HDRS = mci.h dynasty.h commafmt.h sde_input.h
OBJS = mci.o dynasty.o commafmt.o sde_input.o

%.o: $(SRCDIR)%.c
	$(CC) -c $(CFLAGS) $(SRCDIR)$*.c

all: dynasty
dynasty: $(OBJS)
	 $(CC) ${CFLAGS} -o dynasty $(OBJS) ${LDFLAGS}
	# strip dynasty 

clean:
	-rm $(OBJS) dynasty
