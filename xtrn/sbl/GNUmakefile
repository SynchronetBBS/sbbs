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
XPDEV		=	/src/xpdev
SBBS_SRC	=	/src/sbbs3

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
  LIBS	=	$(LIBDIR)/libpthread.a
 endif
endif


CFLAGS	+=	-I$(XSDK) -I$(XPDEV) -I$(SBBS_SRC)

all: sbl sbl2smb smb2sbl

sbl: sbl.c $(XSDK)/xsdk.c $(XSDK)/xsdkvars.c $(XSDK)/xsdkwrap.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

SMBIO	=	$(XPDEV)/genwrap.c $(XPDEV)/filewrap.c $(XPDEV)/dirwrap.c \
		$(SBBS_SRC)/smblib.c $(SBBS_SRC)/lzh.c

sbl2smb: sbl2smb.c $(SMBIO)

smb2sbl: smb2sbl.c $(SMBIO)