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

ifndef os
 os              =       $(shell uname)
 $(warning OS not specified on command line, setting to '$(os)'.)
endif
os      :=	$(shell echo $(os) | awk '/.*/ { print tolower($$1)}')

ifeq ($(os),win32)	# Windows

 EXEFILE	:=	.exe
 LIBDIR	:=	/gcc/i386-mingw32/lib
 CFLAGS	+=	-mno-cygwin
 LFLAGS  +=	--target=i386-mingw32 -mno-cygwin
 DELETE	:=	echo y | del 
 LIBS	+=	$(LIBDIR)/libwsock32.a

else	# Linux

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

endif

CFLAGS	+=	-I../sdk

SBL: sbl$(EXEFILE)

sbl$(EXEFILE) : sbl.c ../sdk/xsdk.c ../sdk/xsdkvars.c ../sdk/xsdkwrap.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
