# GNUmakefile

#########################################################################
# Makefile for SCFG			 											#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: gmake															#
# FreeBSD: gmake os=freebsd												#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id$

# Macros
DEBUG	=	1		# Comment out for release (non-debug) version
USE_DIALOG =	1	# Comment out for stdio (uifcx) version
CC		=	gcc
SLASH	=	/
OFILE	=	o
EXEFILE	=	

ifeq ($(os),freebsd)	# FreeBSD
ODIR	:=	gcc.freebsd
else                    # Linux
ODIR	:=	gcc.linux
endif

LIBDIR	:=	/usr/lib
DELETE	=	rm -f -v
OUTLIB	=	-o

CFLAGS	:=	-I../../uifc -I../../xpdev -I/usr/local/include -I../  -D_THREAD_SAFE

LFLAGS	:=	-L/usr/local/lib

ifdef USE_DIALOG
LFLAGS	:=	$(LFLAGS) -L../../libdialog -ldialog -lcurses
CFLAGS	:=	$(CFLAGS) -I../../libdialog -DUSE_DIALOG
endif

# Math library needed
LFLAGS	:=	$(LFLAGS) -lm

ifdef DEBUG
CFLAGS	:=	$(CFLAGS) -g -O0 -D_DEBUG 
ODIR	:=	$(ODIR).debug
else # RELEASE
LFLAGS	:=	$(LFLAGS)
ODIR	:=	$(ODIR).release
endif

include targets.mk		# defines all targets
include objects.mk		# defines $(OBJS)
include headers.mk		# defines $(HEADERS)

ifdef USE_DIALOG		# moved here from objects.mak
OBJS := $(OBJS)			$(ODIR)$(SLASH)uifcd.$(OFILE)
endif

vpath %.c ..
vpath %.c ../../uifc
vpath %.c ../../xpdev

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
	@$(MAKE) -C ../../libdialog
endif
	@echo Linking $@
	@$(CC) -o $@ $(OBJS) $(LFLAGS)

include depends.mk	# defines dependencies

