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

ifndef os
 os              =       $(shell uname)
 $(warning OS not specified on command line, setting to '$(os)'.)
endif
os      :=	$(shell echo $(os) | awk '/.*/ { print tolower($$1)}')

LIBDIR	:=	/usr/lib
DELETE	:=	rm -f -v
ifeq ($(os),freebsd)	# FreeBSD
 LIBS	+=	-pthread
else
 ifeq ($(os),openbsd)	# OpenBSD
  LIBS	+=	-pthread
 else					# Linux / Generic UNIX
  LIBS	+=	-lpthread
 endif	#! OpenBSD
endif	#! FreeBSD

CFLAGS	+=	-I$(XSDK)

all: sbj sbjclean

sbj: sbj.c $(XSDK)/xsdk.c $(XSDK)/xsdkvars.c $(XSDK)/xsdkwrap.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

sbjclean: sbjclean.c $(XSDK)/xsdk.c $(XSDK)/xsdkvars.c $(XSDK)/xsdkwrap.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
