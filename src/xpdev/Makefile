# Makefile

#########################################################################
# Makefile for cross-platform development "wrappers" test				#
# For use with Borland C++ Builder 5+ or Borland C++ 5.5 for Win32      #
# @format.tab-size 4													#
#																		#
# usage: make															#
#########################################################################

# $Id$

# Macros
DEBUG	=	1				# Comment out for release (non-debug) version
CC		=	bcc32
LD		=	ilink32
SLASH	=	\\
OFILE	=	obj
LIBFILE	=	.dll
EXEFILE	=	.exe
ODIR	=	bcc.win32		# Output directory
CFLAGS	=	-M -g1 
LFLAGS  =	-m -s -c -Tpd -Gi -I$(LIBODIR)
DELETE	=	echo y | del 

# Optional compile flags (disable banner, warnings and such)
CFLAGS	=	$(CFLAGS) -q -d -H -X- -w-csu -w-pch -w-ccc -w-rch -w-par

# Debug or release build?
!ifdef DEBUG
CFLAGS	=	$(CFLAGS) -v -Od -D_DEBUG 
LFLAGS	=	$(LFLAGS) -v
ODIR	=	$(ODIR).debug
!else
ODIR	=	$(ODIR).release
!endif

!include objects.mk		# defines $(OBJS)

all: $(ODIR) $(ODIR)/wraptest.exe

# Implicit C Compile Rule
{.}.c.$(OFILE):
	$(CC) $(CFLAGS) -WD -WM -n$(ODIR) -c $<

# Implicit C++ Compile Rule
{.}.cpp.$(OFILE):
	@$(CC) $(CFLAGS) -WD -WM -n$(ODIR) -c  $<

# Create output directories if they don't exist
$(ODIR):
	@if not exist $(ODIR) mkdir $(ODIR)

# Executable Build Rule
$(ODIR)/wraptest.exe: $(ODIR)\wraptest.obj $(OBJS)
	@$(CC) $(CFLAGS) -WM -n$(ODIR) $**

#!include depends.mak	# defines dependencies