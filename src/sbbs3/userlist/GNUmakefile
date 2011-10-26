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
include ../Common.make
include ../Common.gmake

CFLAGS	+=	$(SBBS_CFLAGS) $(SMBLIB_CFLAGS) $(XPDEV-MT_CFLAGS) `wx-config --unicode --cflags`
CXXFLAGS	+= $(CFLAGS)
LDFLAGS	+=	`wx-config --unicode --libs` $(SMBLIB_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(MT_LDFLAGS) $(SBBS_LDFLAGS) 
ifdef SBBSDIR
LD_RUN_PATH	:=	$(SBBSDIR)/exec:$(SBBS_SRC)/$(LIBODIR)
else
LD_RUN_PATH	:=	$(SBBS_SRC)/$(LIBODIR)
endif
export LD_RUN_PATH

$(USERLIST): $(SBBS) $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(LDFLAGS) $(OBJS) -o $@ -lsbbs $(SMBLIB_LIBS) $(XPDEV-MT_LIBS) $(SBBS_LIBS)
