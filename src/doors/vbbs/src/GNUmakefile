#
#  Virtual BBS makefile
#
##################################

O	     = ./obj

VPATH	 = .:linux

# define tools
CC = gcc
LD = gcc
RC = rc
RM = rm -f
MV = mv -f
CP = cp

BUILD ?= release

TARGET = ./bin/vbbs

#		  ${O}/mx_file.o \

OBJS	=  ${O}/v3_basic.o ${O}/v3_hack.o ${O}/v3_io.o ${O}/v3_learn.o ${O}/v3_mail.o ${O}/v3_maint.o ${O}/v3_mci.o ${O}/v3_store.o ${O}/vbbsutil.o ${O}/vbbs.o ${O}/vbbs_db.o ${O}/filewrap.o

LIBS	= ../lib/libODoors.a

ifeq (${BUILD},debug)
    CFLAGS    = -g -Wall -O -I. -I../include
    CPPFLAGS  = -g -Wall -O -I. -I../include
    LDFLAGS   = -g 
	QUIET	:=
else
    CFLAGS    = -O2 -Wall -fno-common -fomit-frame-pointer -I. -I../include
    CPPFLAGS  = -O2 -Wall -fno-common -fomit-frame-pointer -fno-exceptions -fno-rtti -I. -I../include
    LDFLAGS   = 
	QUIET	:=	@
endif

.PHONY : all run ${TARGET}
.SUFFIXES: .c

all: ${TARGET}

bin:
	mkdir bin

obj:
	mkdir obj

${TARGET}: bin obj ${OBJS} ${LIBS}
	@echo linking $@
	${QUIET}${LD} ${LDFLAGS} ${OBJS} ${LIBS} -o ${TARGET}
	${QUIET}${CP} ${TARGET} ../build

clean:
	@echo Cleaning up...
	${QUIET}${RM} ${O}/*.o ${TARGET}

${O}/%.o : %.c
	@echo Compiling $<
	${QUIET}${CC} -c ${CFLAGS} $< -o $@

${O}/%.o : %.cpp
	@echo $<
	${QUIET}${CC} -c ${CPPFLAGS} $< -o $@
