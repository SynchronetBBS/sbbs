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
os      :=	$(shell echo $(os) | awk '/.*/ { print tolower($$1)}')

LIBDIR	:=	/usr/lib
DELETE	:=	rm -f -v
ifeq ($(os),freebsd)	# FreeBSD
 LIBS	=	-pthread
else
 ifeq ($(os),openbsd)	# OpenBSD
  LIBS	=	-pthread
 else					# Linux / Other UNIX
  LIBS	=	-lpthread
 endif
endif


CFLAGS	+=	-I$(XSDK) -I$(XPDEV) -I$(SBBS_SRC)

all: sbl sbl2smb smb2sbl sblpack

sbl: sbl.o xsdk.o xsdkvars.o xsdkwrap.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

SMBIO	=	$(XPDEV)/genwrap.o $(XPDEV)/filewrap.o $(XPDEV)/dirwrap.o \
		$(SBBS_SRC)/smblib.o $(SBBS_SRC)/lzh.o

sbl2smb: sbl2smb.o $(SMBIO)

smb2sbl: smb2sbl.o $(SMBIO)

sblpack: sblpack.o $(XPDEV)/filewrap.o

clean: rm -f *.o