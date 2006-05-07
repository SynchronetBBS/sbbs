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

# $Id$

SRC_ROOT	=	../..
include $(SRC_ROOT)/build/Common.gmake
include $(SRC_ROOT)/sbbs3/sbbsdefs.mk

ifeq ($(os),sunos)
 LDFLAGS	+=	-lnsl
endif

vpath %.c ..

# Set up LD_RUN_PATH for run-time locating of the .so files
PWD     :=      $(shell dirname `pwd`)
ifdef SBBSDIR
 ifeq ($(os),sunos)
  LD_RUN_PATH   :=      $(SBBSDIR)/exec:$(PWD)/$(LIBODIR):/opt/sfw/gcc-3/lib
 else
  LD_RUN_PATH   :=      $(SBBSDIR)/exec:$(PWD)/$(LIBODIR)
 endif
else
 ifeq ($(os),sunos) 
  LD_RUN_PATH   :=      $(PWD)/$(LIBODIR):/opt/sfw/gcc-3/lib
 else
  LD_RUN_PATH   :=      $(PWD)/$(LIBODIR)
 endif
endif
export LD_RUN_PATH

CFLAGS	+=	$(SBBSDEFS) -I.. $(SMBLIB_CFLAGS) $(XPDEV-MT_CFLAGS) `pkg-config libglade-2.0 --cflags`
# Hopefully, -Wl,-E exports all symbols dynamically everywhere.
LDFLAGS	+=	-Wl,-E -L../$(LIBODIR) $(SMBLIB_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(MT_LDFLAGS) `pkg-config libglade-2.0 --libs`

$(GTKUSEREDIT): $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(LDFLAGS) $(OBJS) -o $@ -lsbbs $(SMBLIB_LIBS) $(XPDEV-MT_LIBS)
