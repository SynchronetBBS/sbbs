# Makefile.gnu

#########################################################################
# Makefile for Synchronet Blackjack										#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: make -f Makefile.gnu											#
# Win32: make -f Makefile.gnu os=win32									#
# FreeBSD: make -f Makefile.gnu os=freebsd								#
#########################################################################

# $Id$

# Macros
CC		=	gcc
LD		=	ld
XSDK		=	../sdk

vpath %.c $(XSDK)

ifndef os
 os              =       $(shell uname)
 $(warning OS not specified on command line, setting to '$(os)'.)
endif
os      :=	$(shell echo $(os) | tr "[A-Z]" "[a-z]")
# remove '/' from "os/2"
os      :=  $(shell echo $(os) | tr -d "/")


LIBDIR	:=	/usr/lib
DELETE	:=	rm -f -v
ifeq ($(os),freebsd)	# FreeBSD
 LIBS	+=	-pthread
else
 ifeq ($(os),openbsd)	# OpenBSD
  LIBS	+=	-pthread
 else
  ifeq ($(os),netbsd)	#NetBSD
   CFLAGS	+=	-D__unix__ -I/usr/pkg/include -I../../src/xpdev
   LIBS	+=	-L/usr/pkg/lib -lpthread
  else
   ifeq ($(os),qnx)	# QNX
    LIBS	+=	-lsocket
   else					# Linux / Generic UNIX
    LIBS	+=	-lpthread
   endif
  endif #! QNX
 endif	#! OpenBSD
endif	#! FreeBSD

CFLAGS	+=	-I$(XSDK)

OBJS		= xsdk.o xsdkvars.o xsdkwrap.o
ifeq ($(os),netbsd)
 OBJS	+=	../../src/xpdev/sem.o
endif

all: sbj sbjclean

sbj: sbj.o $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

sbjclean: sbjclean.o $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f *.o
