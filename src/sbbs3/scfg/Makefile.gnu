# Makefile.gnu

#########################################################################
# Makefile for Synchronet BBS 											#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: make -f Makefile.gnu											#
# Win32: make -f Makefile.gnu os=win32									#
# FreeBSD: make -f Makefile.gnu os=freebsd								#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id$

# Macros
# DEBUG	=	1		# Comment out for release (non-debug) version
CC		=	gcc
SLASH	=	/
OFILE	=	o

ifeq ($(os),win32)	# Windows

LD		=	dllwrap
LIBFILE	=	.dll
EXEFILE	=	.exe
LIBODIR	:=	gcc.win32.dll
EXEODIR	:=	gcc.win32.exe
LIBDIR	:=	/gcc/i386-mingw32/lib
CFLAGS	:=	-mno-cygwin
LFLAGS  :=	--target=i386-mingw32 -mno-cygwin
DELETE	=	echo y | del 
OUTLIB	=	--output-lib
LIBS	=	$(LIBDIR)/libwinmm.a

else	# Unix (begin)

LD		=	ld
LIBFILE	=	.a
EXEFILE	=	

ifeq ($(os),freebsd)	# FreeBSD
LIBODIR	:=	gcc.freebsd.lib
EXEODIR	:=	gcc.freebsd.exe
else                    # Linux
LIBODIR	:=	gcc.linux.lib
EXEODIR	:=	gcc.linux.exe
endif

LIBDIR	:=	/usr/lib
LFLAGS  :=	
DELETE	=	rm -f -v
OUTLIB	=	-o

CFLAGS	:=	-I../../uifc -I/usr/local/include -I../

ifeq ($(os),freebsd)	# FreeBSD
CFLAGS	:=	$(CFLAGS) -DUSE_DIALOG
LIBS	:=	-L/usr/local/lib -ldialog
USE_DIALOG =	YES
else			# Linux / Other UNIX
LIBS	:=	-L/usr/local/lib
endif

endif   # Unix (end)

# Math library needed
LIBS	:=	$(LIBS) -lm

ifdef DEBUG
CFLAGS	:=	$(CFLAGS) -g -O0 -D_DEBUG 
LIBODIR	:=	$(LIBODIR).debug
EXEODIR	:=	$(EXEODIR).debug
else # RELEASE
LFLAGS	:=	$(LFLAGS) -S
LIBODIR	:=	$(LIBODIR).release
EXEODIR	:=	$(EXEODIR).release
endif

include targets.mak		# defines all targets
include objects.mak		# defines $(OBJS)
include headers.mak		# defines $(HEADERS)

SBBSLIB	=	$(LIBODIR)/sbbs.a
SBBSDEFS =	
	

# Implicit C Compile Rule for SBBS
$(LIBODIR)/%.o : %.c
	$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Implicit C++ Compile Rule for SBBS
$(LIBODIR)/%.o : %.cpp
	$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# uifc Rules
../../uifc/uifcx.$(OFILE):
	$(CC) $(CFLAGS) -c $(SBBSDEFS) ../../uifc/uifcx.c -o ../../uifc/uifcx.$(OFILE)

../../uifc/uifcd.c:

../../uifc/uifcd.$(OFILE):
	$(CC) $(CFLAGS) -c $(SBBSDEFS) ../../uifc/uifcd.c -o ../../uifc/uifcd.$(OFILE)

# Create output directories
$(LIBODIR):
	mkdir $(LIBODIR)

$(EXEODIR):
	mkdir $(EXEODIR)

# Monolithic Synchronet executable Build Rule
$(SCFG): $(OBJS)
	$(CC) -o $(SCFG) $^ $(LIBS)

#indlude depends.mak
