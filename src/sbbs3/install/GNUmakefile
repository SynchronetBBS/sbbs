# GNUmakefile

#########################################################################
# Makefile for SBBSINST		 											#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: gmake															#
# FreeBSD: gmake os=FreeBSD												#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id$

#USE_DIALOG =	1	# Dialog vesrion of UIFC
USE_CURSES	=	1	# Curses version of UIFC
#USE_FLTK	=	1	# Windowed version of UIFC
CC		=	gcc
SLASH	=	/
OFILE	=	o
UIFC	=	../../uifc
XPDEV	=	../../xpdev

ifdef STATIC
 LDFLAGS	+=	-static
endif

ifndef os
 os		=	$(shell uname)
endif
os      :=	$(shell echo $(os) | tr "[A-Z]" "[a-z]")
# remove '/' from "os/2"
os      :=  $(shell echo $(os) | tr -d "/")

ifeq ($(os),netbsd)
 CFLAGS	+=	-D__unix__ -I/usr/pkg/include -DUSE_XP_SEMAPHORES
 LDFLAGS +=	-L/usr/pkg/lib
endif

ODIR	:=	gcc.$(os)

LIBDIR	:=	/usr/lib
DELETE	=	rm -f -v
OUTLIB	=	-o

#CFLAGS	+=	-g
CFLAGS	+=	-O2
CFLAGS	+=	-MMD -Wall -I$(UIFC) -I$(XPDEV) -I..

LDFLAGS	+=	-L/usr/local/lib

ODIR	:=	$(ODIR).release

include targets.mk		# defines all targets
include objects.mk		# defines $(OBJS)

ifdef USE_DIALOG		
 LDFLAGS	+=	-L../../libdialog -ldialog -lncurses
 CFLAGS	+=	-I../../libdialog -DUSE_DIALOG
 OBJS += $(ODIR)$(SLASH)uifcd.$(OFILE)
endif

ifdef USE_CURSES
 LDFLAGS	+=	-lncurses
 CFLAGS	+=	-DUSE_CURSES
 OBJS += $(ODIR)$(SLASH)uifc32.$(OFILE)
endif

ifdef USE_FLTK
 LDFLAGS +=	-L/usr/X11R6/lib -lfltk -lX11 -lm
 CFLAGS +=	-I/usr/local/include -I/usr/X11R6/include -DUSE_FLTK
 OBJS += $(ODIR)$(SLASH)uifcfltk.$(OFILE)
endif

vpath %.c ..
vpath %.c $(UIFC)
vpath %.c $(XPDEV)

# Implicit C Compile Rule for SBBSINST
$(ODIR)/%.o : %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Explicit C++ Compile Rule for SBBSINST
$(ODIR)/uifcfltk.o : $(UIFC)/uifcfltk.cpp
	@echo Compiling uifcfltk.cpp
	$(CC) $(CFLAGS) -c $(SBBSDEFS) $(UIFC)/uifcfltk.cpp -o $(ODIR)/uifcfltk.o

# Create output directories
$(ODIR):
	mkdir $(ODIR)

# Monolithic Synchronet executable Build Rule
$(SBBSINST): $(OBJS)
   ifdef USE_DIALOG
	@$(MAKE) --no-print-directory -C ../../libdialog
   endif
	@echo Linking $@
	@$(CC) -o $@ $(OBJS) $(LDFLAGS)
#	@strip $@

# Auto-dependency files (should go in output dir, but gcc v2.9.5 puts in cwd)
-include ./*.d
-include $(ODIR)/*.d
