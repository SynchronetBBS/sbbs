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

ifndef $(os)
os              =       $(shell uname)
$(warning OS not specified on command line, setting to '$(os)'.)
endif

ifeq ($(os),win32)	# Windows

EXEFILE	=	.exe
LIBDIR	:=	/gcc/i386-mingw32/lib
CFLAGS	:=	-mno-cygwin
LFLAGS  :=	--target=i386-mingw32 -mno-cygwin
DELETE	=	echo y | del 
LIBS	=	$(LIBDIR)/libwsock32.a

else	# Linux

EXEFILE	=	
LIBODIR	:=	gcc.linux.lib
EXEODIR	:=	gcc.linux.exe
LIBDIR	:=	/usr/lib
CFLAGS	:=	
LFLAGS  :=	
DELETE	=	rm -f -v
ifeq ($(os),FreeBSD)	# FreeBSD
LIBS	=	-pthread
else
LIBS	=	$(LIBDIR)/libpthread.a
endif	#! FreeBSD

endif

CFLAGS	:=	$(CFLAGS) -I../sdk

SBJ: sbj$(EXEFILE)

sbj$(EXEFILE) : sbj.c ../sdk/xsdk.c ../sdk/xsdkvars.c ../sdk/xsdkwrap.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
