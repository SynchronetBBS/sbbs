# GNUmakefile

#########################################################################
# Makefile for Synchronet monitor for Unix								#
# For use with GNU make and GNU C Compiler or Borland Kylix C++			#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# gcc: gmake															#
# Borland (still in testing/debuging stage): gmake bcc=1				#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id: GNUmakefile,v 1.9 2020/04/30 21:26:42 deuce Exp $

SRC_ROOT	=	../..
include $(SRC_ROOT)/build/Common.gmake
include $(SRC_ROOT)/sbbs3/sbbsdefs.mk

ifeq ($(os),sunos)
 EXTRA_LIBS	+=	-lnsl
endif

vpath %.c ..

CFLAGS	+=	$(SBBSDEFS) -I.. -I../../comio $(SMBLIB_CFLAGS) $(HASH_CFLAGS) $(XPDEV-MT_CFLAGS) $(CRYPT_CFLAGS) `pkg-config gtk+-3.0 --cflags`
# Hopefully, -Wl,-E exports all symbols dynamically everywhere.
LDFLAGS	+=	-Wl,-E -L../$(LIBODIR) $(SMBLIB_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(MT_LDFLAGS)
EXTRA_LIBS += `pkg-config gtk+-3.0 --libs`

# Because pkg-config for libglade uses --rpath, we need to also, LD_RUN_PATH
# won't work.  :-(
ifdef SBBSDIR
 LDFLAGS	+=	-Wl,--rpath -Wl,$(SBBSDIR)/exec
endif
PARENT_DIR = $(shell dirname `pwd`)
LDFLAGS	+=	-Wl,--rpath -Wl,$(PARENT_DIR)/$(LIBODIR)
ifeq ($(os),sunos)
 LDFLAGS	+=	-Wl,--rpath -Wl,/opt/sfw/gcc-3/lib
endif

$(GTKUSERLIST): $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(LDFLAGS) $(OBJS) -o $@ $(EXTRA_LIBS) -lsbbs $(SMBLIB_LIBS) $(XPDEV-MT_LIBS)
	$(QUIET)cd $(EXEODIR) && ln -fs ../gtkuserlist.glade .
