# Makefile

#########################################################################
# Makefile for cross-platform development "wrappers" test				#
# For use with Borland or Microsoft C++ Compilers for Win32				#
# @format.tab-size 4													#
#																		#
# usage: make [msc=1]													#
#########################################################################

# $Id$

# Macros
DEBUG	=	1				# Comment out for release (non-debug) version

# OS-specific
SLASH	=	\	# This comment is necessary
OFILE	=	obj
LIBFILE	=	.dll
EXEFILE	=	.exe
DELETE	=	echo y | del 

# Compiler-specific
!ifdef msc	# Microsoft Visual C++
CC		=	cl
LD		=	link
ODIR	=	msvc.win32
OUTPUT	=	-Fo
LOUTPUT	=	-Fe
CFLAGS  =	-nologo -MTd
LFLAGS	=	$(CFLAGS)
!ifdef DEBUG
CFLAGS	=	$(CFLAGS) -Yd
!endif

!else		# Borland C++

CC		=	bcc32
LD		=	ilink32
ODIR	=	bcc.win32		# Output directory
OUTPUT	=	-o
LOUTPUT	=	$(OUTPUT)
CFLAGS	=	-n$(ODIR) -WM -q 
LFLAGS	=	$(CFLAGS)
CLFAGS  =	$(CFLAGS) -M -WD -WM -X-
!ifdef DEBUG
CFLAGS	=	$(CFLAGS) -v
!endif
!endif

# Common compiler flags
!ifdef DEBUG
CFLAGS	=	$(CFLAGS) -Od -D_DEBUG 
!endif

# Debug or release build?
!ifdef DEBUG
ODIR	=	$(ODIR).debug
!else
ODIR	=	$(ODIR).release
!endif

!include objects.mk		# defines $(OBJS)
!include targets.mk

# Implicit C Compile Rule
{.}.c.$(OFILE):
	@$(CC) $(CFLAGS) -c $< $(OUTPUT)$@

# Create output directories if they don't exist
$(ODIR):
	@if not exist $(ODIR) mkdir $(ODIR)

# Executable Build Rule
$(WRAPTEST): $(ODIR)\wraptest.obj $(OBJS)
	@echo Linking $@
	@$(CC) $(LFLAGS) $** $(LOUTPUT)$@
