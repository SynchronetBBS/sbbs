# GNUmakefile

#########################################################################
# Makefile for SCFG			 											#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: gmake															#
# FreeBSD: gmake os=FreeBSD												#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id$

# Macros
ifndef DEBUG
 ifndef RELEASE
  DEBUG	:=	1
 endif
endif

#USE_DIALOG =	1	# Dialog vesrion of UIFC
#USE_FLTK =	1	# Use Windowed version
USE_CURSES	=	1	# Curses version of UIFC
CC		=	gcc
CCPP		=	gcc
SLASH	=	/
OFILE	=	o
UIFC	=	../../uifc
XPDEV	=	../../xpdev

ifndef os
 os		=	$(shell uname)
 $(warning OS not specified on command line, setting to '$(os)'.)
endif
os      :=	$(shell echo $(os) | tr "[A-Z]" "[a-z]")

ODIR	:=	gcc.$(os)

LIBDIR	:=	/usr/lib
DELETE	=	rm -f
OUTLIB	=	-o

CFLAGS	+=	-MMD
CFLAGS	+=	-Wall -I$(UIFC) -I$(XPDEV) -I../  -D_THREAD_SAFE

LFLAGS	:=	-L/usr/local/lib

ifdef USE_DIALOG
 LFLAGS	+=	-L../../libdialog -ldialog -lcurses
 CFLAGS	+=	-I../../libdialog -DUSE_DIALOG
endif

ifdef USE_CURSES
 LFLAGS	+=	-lcurses
 CFLAGS	+=	-DUSE_CURSES
endif

ifdef USE_FLTK
 LFLAGS +=	-L../../../lib/fltk/$(os) -L/usr/X11R6/lib -lfltk -lX11
 CFLAGS +=	-I../../../include/fltk -I/usr/X11R6/include -DUSE_FLTK
endif

ifdef DEBUG
 CFLAGS	+=	-ggdb -O0 -D_DEBUG 
 ODIR	:=	$(ODIR).debug
else # RELEASE
 ODIR	:=	$(ODIR).release
endif

include targets.mk		# defines all targets
include objects.mk		# defines $(OBJS)

ifdef USE_DIALOG		
 OBJS += $(ODIR)$(SLASH)uifcd.$(OFILE)
endif

ifdef USE_CURSES		
 OBJS += $(ODIR)$(SLASH)uifcc.$(OFILE)
endif

ifdef USE_FLTK
 OBJS += $(ODIR)$(SLASH)uifcfltk.$(OFILE)
endif

vpath %.c ..
vpath %.c $(UIFC)
vpath %.cpp $(UIFC)
vpath %.c $(XPDEV)

# Implicit C Compile Rule for SCFG
$(ODIR)/%.o : %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Implicit C++ Compile Rule for SCFG
$(ODIR)/%.o : %.cpp
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Create output directories
$(ODIR):
	mkdir $(ODIR)

$(MAKEHELP): makehelp.c
	@$(CC) makehelp.c -o $(MAKEHELP)

$(SCFGHELP): $(OBJS) $(MAKEHELP)
	@$(MAKEHELP) $(ODIR)

# Monolithic Synchronet executable Build Rule
$(SCFG): $(OBJS)
   ifdef USE_DIALOG
	@$(MAKE) --no-print-directory -C ../../libdialog
   endif
	@echo Linking $@
	@$(CC) -o $@ $(OBJS) $(LFLAGS)

# Auto-dependency files (should go in output dir, but gcc v2.9.5 puts in cwd)
-include ./*.d
-include $(ODIR)/*.d
