# Makefile.bor

#########################################################################
# Makefile for Synchronet BBS Configuration Utility (SCFG)				#
# For use with Borland C++ Builder 5+ or Borland C++ 5.5 for Win32      #
# @format.tab-size 4													#
#																		#
# usage: make -f makefile.bor											#
#########################################################################

# $Id$

# Macros
DEBUG	=	1				# Comment out for release (non-debug) version
CC		=	bcc32
LD		=	ilink32
SLASH	=	\\
OFILE	=	obj
EXEFILE	=	.exe
ODIR	=	bcc.win32	# Executable output directory
XPDEV	=   ..\..\xpdev
UIFC	=	..\..\uifc
CFLAGS	=	-M -I..;$(UIFC);$(XPDEV) -g1
DELETE	=	echo y | del 

.path.c = .;..;$(UIFC);$(XPDEV)

# Enable auto-dependency checking
.autodepend
.cacheautodepend	

# Optional compile flags (disable banner, warnings and such)
CFLAGS	=	$(CFLAGS) -q -d -H -X- -w-csu -w-pch -w-ccc -w-rch -w-par -w-pro 
#-w-aus

# Debug or release build?
!ifdef DEBUG
CFLAGS	=	$(CFLAGS) -v -Od -D_DEBUG 
LFLAGS	=	$(LFLAGS) -v
ODIR	=	$(ODIR).debug
!else
ODIR	=	$(ODIR).release
!endif

# Cross platform/compiler definitions
!include targets.mk	# defines all targets
!include objects.mk	# defines $(OBJS)
!include headers.mk	# defines $(HEADERS)

OBJS = $(OBJS) $(ODIR)\uifc.$(OFILE)

# Implicit C Compile Rule for SCFG
.c.$(OFILE):
	@$(CC) $(CFLAGS) -n$(ODIR) -c $<

# Create output directories if they don't exist
$(ODIR):
	@if not exist $(ODIR) mkdir $(ODIR)

makehelp.exe: makehelp.c
	$(CC) $?

$(SCFGHELP): $(OBJS) makehelp.exe
	makehelp $(ODIR)

# Monolithic SCFG executable Build Rule
$(SCFG): $(OBJS) 
	@echo Linking $@
	@$(CC) $(CFLAGS) -e$@ $**
