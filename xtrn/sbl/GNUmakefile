# Makefile.gnu

#########################################################################
# Makefile for Synchronet BBS List										#
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
ifndef XPDEV
 XPDEV		=	../../src/xpdev
endif
ifndef SBBS_SRC
 SBBS_SRC	=	../../src/sbbs3
endif

vpath %.c $(XPDEV) $(XSDK)

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
 CFLAGS	+= -D_THREAD_SAFE
 LFLAGS	:=	-pthread
 BSD	=	1
else
 ifeq ($(os),openbsd)	# OpenBSD
  CFLAGS	+= -D_THREAD_SAFE
  LFLAGS	:=	-pthread
  BSD		=	1
 else
  ifeq ($(os),netbsd)	# NetBSD
  CFLAGS	+= -D_THREAD_SAFE -D__unix__ -D_NEED_SEM -I/usr/pkg/include
  LFLAGS	:=	-lpth -lpthread -L/usr/pkg/lib
  else
   ifeq ($(os),qnx)	# QNX
    CFLAGS	+= -D_THREAD_SAFE
    LFLAGS	:= 
   else			# Linux / Other UNIX
    ifdef bcc
     LFLAGS	:=	libpthread.a
    else
     LFLAGS	:=	-lpthread
    endif
   endif
  endif
 endif
endif


CFLAGS	+=	-I$(XSDK) -I$(XPDEV) -I$(SBBS_SRC)

all: sbl sbl2smb smb2sbl sblpack

sbl: sbl.o xsdk.o xsdkvars.o xsdkwrap.o
	$(CC) $(CFLAGS) $(LFLAGS) $^ -o $@ $(LIBS)

SMBIO	=	$(XPDEV)/genwrap.o $(XPDEV)/filewrap.o $(XPDEV)/dirwrap.o \
		$(SBBS_SRC)/smblib.o $(SBBS_SRC)/lzh.o

sbl2smb: sbl2smb.o $(SMBIO)

smb2sbl: smb2sbl.o $(SMBIO)

sblpack: sblpack.o $(XPDEV)/filewrap.o

clean:
	rm -f *.o
