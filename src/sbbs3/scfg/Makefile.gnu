# Makefile.gnu

#########################################################################
# Makefile for SCFG			 											#
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
USE_DIALOG =	1		# Comment out for stdio (uifcx) version
CC		=	gcc
SLASH	=	/
OFILE	=	o

ifeq ($(os),win32)	# Windows

LD		=	dllwrap
LIBFILE	=	.dll
EXEFILE	=	.exe
LIBODIR	:=	gcc.win32.dll
EXEODIR	:=	gcc.win32.exe
SBBSLIBODIR	:=	gcc.win32.sbbsdll
UIFCLIBODIR	:=	gcc.win32.uifcdll
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
SBBSLIBODIR	:=	gcc.freebsd.sbbslib
UIFCLIBODIR	:=	gcc.freebsd.uifclib
MAKE	:=	gmake
else                    # Linux
LIBODIR	:=	gcc.linux.lib
EXEODIR	:=	gcc.linux.exe
SBBSLIBODIR	:=	gcc.linux.sbbslib
UIFCLIBODIR	:=	gcc.linux.uifclib
MAKE	:=	make
endif

LIBDIR	:=	/usr/lib
LFLAGS  :=	
DELETE	=	rm -f -v
OUTLIB	=	-o

CFLAGS	:=	-I../../uifc -I/usr/local/include -I../  -D_THREAD_SAFE

LIBS	:=	-L/usr/local/lib

ifdef USE_DIALOG
LIBS	:=	$(LIBS) -L../../libdialog -ldialog -lcurses
CFLAGS	:=	$(CFLAGS) -I../../libdialog -DUSE_DIALOG
endif

endif   # Unix (end)

# Math library needed
LIBS	:=	$(LIBS) -lm

ifdef DEBUG
CFLAGS	:=	$(CFLAGS) -g -O0 -D_DEBUG 
LIBODIR	:=	$(LIBODIR).debug
EXEODIR	:=	$(EXEODIR).debug
SBBSLIBODIR	:=	$(SBBSLIBODIR).debug
UIFCLIBODIR	:=	$(UIFCLIBODIR).debug
else # RELEASE
LFLAGS	:=	$(LFLAGS) -S
LIBODIR	:=	$(LIBODIR).release
EXEODIR	:=	$(EXEODIR).release
SBBSLIBODIR	:=	$(SBBSLIBODIR).release
UIFCLIBODIR	:=	$(UIFCLIBODIR).release
endif

include targets.mak		# defines all targets
include objects.mak		# defines $(OBJS)
include headers.mak		# defines $(HEADERS)

ifdef USE_DIALOG		# moved here from objects.mak
OBJS := $(OBJS)			$(UIFCLIBODIR)$(SLASH)uifcd.$(OFILE)
endif

SBBSLIB	=	$(LIBODIR)/sbbs.a
SBBSDEFs =	
	

# Implicit C Compile Rule for SCFG
$(LIBODIR)/%.o : %.c
	$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Implicit C++ Compile Rule for SCFG
$(LIBODIR)/%.o : %.cpp
	$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Implicit C Compile Rule for SBBS Objects
$(SBBSLIBODIR)/%.o : ../%.c
	$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Implicit C++ Compile Rule for SBBS Objects
$(SBBSLIBODIR)/%.o : ../%.cpp
	$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# uifc Rules
$(UIFCLIBODIR)/%.o : ../../uifc/%.c
	$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Create output directories
$(LIBODIR):
	mkdir $(LIBODIR)

$(EXEODIR):
	mkdir $(EXEODIR)

$(SBBSLIBODIR):
	mkdir $(SBBSLIBODIR)

$(UIFCLIBODIR):
	mkdir $(UIFCLIBODIR)

makehelp: makehelp.c
	$(CC) makehelp.c -o makehelp

$(SCFGHELP): $(OBJS) makehelp
	./makehelp $(EXEODIR)

# Monolithic Synchronet executable Build Rule
$(SCFG): $(OBJS)
ifdef USE_DIALOG
	$(MAKE) -C ../../libdialog
endif
	$(CC) -o $(SCFG) $(OBJS) $(LIBS)

#indlude depends.mak

