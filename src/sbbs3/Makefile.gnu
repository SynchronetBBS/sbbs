# Makefile.gnu

#########################################################################
# Makefile for Synchronet BBS 											#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: make -f Makefile.gnu											#
# Win32: make -f Makefile.gnu os=win32									#
#########################################################################

# $Id$

# Macros
DEBUG	=	1		# Comment out for release (non-debug) version
CC		=	gcc
SLASH	=	/
OFILE	=	o

ifeq ($(os),win32)	# Windows

LD		=	dllwrap
LFILE	=	dll
LIBODIR	:=	gcc.win32.dll
EXEODIR	:=	gcc.win32.exe
LIBDIR	:=	/gcc/i386-mingw32/lib
CFLAGS	:=	-mno-cygwin
LFLAGS  :=	--target=i386-mingw32 -mno-cygwin

else	# Linux

LD		=	ld
LFILE	=	a
LIBODIR	:=	gcc.linux.lib
EXEODIR	:=	gcc.linux.exe
LIBDIR	:=	/usr/lib
CFLAGS	:=	
LFLAGS  :=	

endif

ifdef DEBUG
CFLAGS	:=	$(CFLAGS) -g -O0 -D_DEBUG 
LIBODIR	:=	$(LIBODIR).debug
EXEODIR	:=	$(EXEODIR).debug
else
LFLAGS	:=	$(LFLAGS) -S
LIBODIR	:=	$(LIBODIR).release
EXEODIR	:=	$(EXEODIR).release
endif

SBBS	=	$(LIBODIR)/sbbs.$(LFILE)

FTPSRVR	=	$(LIBODIR)/ftpsrvr.$(LFILE)

MAILSRVR=	$(LIBODIR)/mailsrvr.$(LFILE)

SBBSLIB	=	$(LIBODIR)/sbbs.a

ALL: $(LIBODIR) $(SBBS) $(FTPSRVR) $(MAILSRVR)

include objects.mak		# defines $(OBJS)
include headers.mak		# defines $(HEADERS)
include sbbsdefs.mak	# defines $(SBBSDEFS)
	
LIBS	=	$(LIBDIR)/libwsock32.a $(LIBDIR)/libwinmm.a

# Implicit C Compile Rule for SBBS
$(LIBODIR)/%.o : %.c
	$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Implicit C++ Compile Rule for SBBS
$(LIBODIR)/%.o : %.cpp
	$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Create output directory
$(LIBODIR):
	mkdir $(LIBODIR)

# SBBS Link Rule
$(SBBS): $(OBJS) $(LIBODIR)/ver.o
	$(LD) $(LFLAGS) -o $@ $^ $(LIBS) --output-lib $(SBBSLIB)

# FTP Server Link Rule
$(FTPSRVR): $(LIBODIR)/ftpsrvr.o $(SBBSLIB)
	$(LD) $(LFLAGS) -o $@ $^ $(LIBS) --output-lib $(LIBODIR)/ftpsrvr.a

# Mail Server Link Rule
$(MAILSRVR): $(LIBODIR)/mailsrvr.o $(LIBODIR)/mxlookup.o $(SBBSLIB)
	$(LD) $(LFLAGS) -o $@ $^ $(LIBS) --output-lib $(LIBODIR)/mailsrvr.a

# Specifc Compile Rules
$(LIBODIR)/ftpsrvr.o: ftpsrvr.c ftpsrvr.h
	$(CC) $(CFLAGS) -c -DFTPSRVR_EXPORTS $< -o $@

$(LIBODIR)/mailsrvr.o: mailsrvr.c mailsrvr.h
	$(CC) $(CFLAGS) -c -DMAILSRVR_EXPORTS $< -o $@

$(LIBODIR)/mxlookup.o: mxlookup.c
	$(CC) $(CFLAGS) -c -DMAILSRVR_EXPORTS $< -o $@		

include depends.mak