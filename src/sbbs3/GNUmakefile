# Makefile.gnu

#########################################################################
# Makefile for Synchronet BBS 											#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: make -f Makefile.gnu											#
# FreeBSD: make -f Makefile.gnu os=freebsd								#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id$

# Macros
DEBUG	=	1		# Comment out for release (non-debug) version
CC		=	gcc
SLASH	=	/
OFILE	=	o

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

DELETE	=	rm -fv

CFLAGS	=	-DJAVASCRIPT -I../mozilla/js/src

ifeq ($(os),freebsd)	# FreeBSD
CFLAGS	+= -pthread -D_THREAD_SAFE
else			# Linux / Other UNIX
endif

# Math and pthread libraries needed
LFLAGS	:=	-lm -lpthread

ifdef DEBUG
CFLAGS	+=	-g -O0 -D_DEBUG 
LIBODIR	+=	.debug
EXEODIR	+=	.debug
ifeq ($(os),freebsd)	# FreeBSD
LIBS	+=	../mozilla/js/src/FreeBSD4.3-RELEASE_DBG.OBJ/libjs.a
else			# Linux
LIBS	+=	../mozilla/js/src/Linux_All_DBG.OBJ/libjs.a
endif
else # RELEASE
LIBODIR	+=	.release
EXEODIR	+=	.release
ifeq ($(os),freebsd)	# FreeBSD
LIBS	+=	../mozilla/js/src/FreeBSD4.3-RELEASE_OPT.OBJ/libjs.a
else
LIBS	+=	../mozilla/js/src/Linux_All_OPT.OBJ/libjs.a
endif
endif

include targets.mak		# defines all targets
include objects.mak		# defines $(OBJS)
include headers.mak		# defines $(HEADERS)
include sbbsdefs.mak	# defines $(SBBSDEFS)

SBBSLIB	=	$(LIBODIR)/sbbs.a
	

# Implicit C Compile Rule for SBBS
$(LIBODIR)/%.o : %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Implicit C++ Compile Rule for SBBS
$(LIBODIR)/%.o : %.cpp
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $(SBBSDEFS) $< -o $@

# Create output directories
$(LIBODIR):
	mkdir $(LIBODIR)

$(EXEODIR):
	mkdir $(EXEODIR)

CON_OBJS	= $(EXEODIR)/sbbscon.o $(EXEODIR)/conwrap.o
FTP_OBJS	= $(LIBODIR)/ftpsrvr.o
MAIL_OBJS	= $(LIBODIR)/mailsrvr.o $(LIBODIR)/mxlookup.o $(LIBODIR)/mime.o 
SERVICE_OBJS= $(LIBODIR)/services.o

MONO_OBJS	= $(CON_OJBS) $(FTP_OBJS) $(MAIL_OBJS) $(SERVICE_OBJS)

# Monolithic Synchronet executable Build Rule
$(SBBSMONO): $(EXEODIR) $(LIBODIR) $(MONO_OBJS) $(OBJS) $(LIBS) $(LIBODIR)/ver.o 
	@echo Linking $@
	@$(CC) $(LFLAGS) $^ -o $@

# Synchronet BBS library Link Rule
$(SBBS): $(LIBODIR) $(OBJS) $(LIBS) $(LIBODIR)/ver.o
	$(LD) $(LFLAGS) -S -o $(SBBS) $^ $(LIBS) -o $@

# FTP Server Link Rule
$(FTPSRVR): $(LIBODIR) $(LIBODIR)/ftpsrvr.o $(SBBSLIB)
	$(LD) $(LFLAGS) -S $^ $(LIBS) -o $@ 

# Mail Server Link Rule
$(MAILSRVR): $(LIBODIR) $(MAIL_OBJS) $(SBBSLIB)
	$(LD) $(LFLAGS) -S $^ $(LIBS) -o $@

# Synchronet Console Build Rule
$(SBBSCON): $(EXEODIR) $(CON_OBJS) $(SBBSLIB)
	$(CC) $(CFLAGS) -o $@ $^

# Specifc Compile Rules
$(LIBODIR)/ftpsrvr.o: ftpsrvr.c ftpsrvr.h
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c -DFTPSRVR_EXPORTS $< -o $@

$(LIBODIR)/mailsrvr.o: mailsrvr.c mailsrvr.h
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c -DMAILSRVR_EXPORTS $< -o $@

$(LIBODIR)/mxlookup.o: mxlookup.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c -DMAILSRVR_EXPORTS $< -o $@		

$(LIBODIR)/mime.o: mime.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c -DMAILSRVR_EXPORTS $< -o $@		

$(LIBODIR)/services.o: services.c services.h
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c -DSERVICES_EXPORTS $< -o $@

# Baja Utility
$(BAJA): $(EXEODIR)/baja.o $(EXEODIR)/ars.o $(EXEODIR)/smbwrap.o $(EXEODIR)/crc32.o
	@echo Linking $@
	@$(CC) $^ -o $@

# Node Utility
$(NODE): $(EXEODIR)/node.o $(EXEODIR)/smbwrap.o
	@echo Linking $@
	@$(CC) $^ -o $@ 

# FIXSMB Utility
$(FIXSMB): $(EXEODIR)/fixsmb.o $(EXEODIR)/smblib.o $(EXEODIR)/smbwrap.o
	@echo Linking $@
	@$(CC) $^ -o $@

# CHKSMB Utility
$(CHKSMB): $(EXEODIR)/chksmb.o $(EXEODIR)/smblib.o $(EXEODIR)/smbwrap.o $(EXEODIR)/conwrap.o
	@echo Linking $@
	@$(CC) $^ -o $@

# SMB Utility
$(SMBUTIL): $(EXEODIR)/smbutil.o $(EXEODIR)/smblib.o $(EXEODIR)/smbwrap.o $(EXEODIR)/conwrap.o $(EXEODIR)/smbtxt.o $(EXEODIR)/crc32.o $(EXEODIR)/lzh.o 
	@echo Linking $@
	@$(CC) $^ -o $@

include depends.mak
