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
USE_CURSES	=	1	# Curses version of UIFC
CC		=	gcc
SLASH	=	/
OFILE	=	o
UIFC	=	../../uifc
XPDEV	=	../../xpdev

ifndef os
 os		=	$(shell uname)
 $(warning OS not specified on command line, setting to '$(os)'.)
endif
os      :=	$(shell echo $(os) | awk '/.*/ { print tolower($$1)}')

ODIR	:=	gcc.$(os)

LIBDIR	:=	/usr/lib
DELETE	=	rm -f -v
OUTLIB	=	-o

CFLAGS	+=	-Wall -I$(UIFC) -I$(XPDEV) -I/usr/local/include -I../  -D_THREAD_SAFE

LFLAGS	:=	-L/usr/local/lib

ifdef USE_DIALOG
 LFLAGS	:=	$(LFLAGS) -L../../libdialog -ldialog -lcurses
 CFLAGS	+=	-I../../libdialog -DUSE_DIALOG
endif

ifdef USE_CURSES
 LFLAGS	:=	$(LFLAGS) -lcurses
 CFLAGS	+=	-DUSE_CURSES
endif

# Math library needed
LFLAGS	:=	$(LFLAGS) -lm

ifdef DEBUG
 CFLAGS	+=	-ggdb -O0 -D_DEBUG 
 ODIR	:=	$(ODIR).debug
else # RELEASE
 ODIR	:=	$(ODIR).release
endif

include targets.mk		# defines all targets
include objects.mk		# defines $(OBJS)
include headers.mk		# defines $(HEADERS)

ifdef USE_DIALOG		
 OBJS := $(OBJS)			$(ODIR)$(SLASH)uifcd.$(OFILE)
endif

ifdef USE_CURSES		
 OBJS := $(OBJS)			$(ODIR)$(SLASH)uifcc.$(OFILE)
endif

vpath %.c ..
vpath %.c $(UIFC)
vpath %.c $(XPDEV)

# Implicit C Compile Rule for SCFG
$(ODIR)/%.o : %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Create output directories
$(ODIR):
	mkdir $(ODIR)

makehelp: makehelp.c
	$(CC) makehelp.c -o makehelp

$(SCFGHELP): $(OBJS) makehelp
	./makehelp $(ODIR)

# Monolithic Synchronet executable Build Rule
$(SCFG): $(OBJS)
   ifdef USE_DIALOG
	@$(MAKE) --no-print-directory -C ../../libdialog
   endif
	@echo Linking $@
	@$(CC) -o $@ $(OBJS) $(LFLAGS)

include depends.mk	# defines dependencies

