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

XSDK		=	../sdk
XPDEV	?=	../../src/xpdev/
vpath %.c $(XSDK)
NEED_THREADS	:=	1
include $(XPDEV)Common.gmake
CFLAGS	+=	-I$(XSDK)

$(EXEODIR)$(SLASH)sbj: $(LIBODIR)$(SLASH)sbj.o $(OBJS)
   ifndef bcc
	@echo Linking $@
   endif
	$(QUIET)$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)

$(EXEODIR)$(SLASH)sbjclean: $(LIBODIR)$(SLASH)sbjclean.o $(OBJS)
   ifndef bcc
	@echo Linking $@
   endif
	$(QUIET)$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)
