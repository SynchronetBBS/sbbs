# GNUmakefile

#########################################################################
# Makefile for Synchronet BBS 											#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: gmake [bcc=1]													#
# FreeBSD: gmake os=FreeBSD												#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id$

# Macros
DEBUG	=	1		# Comment out for release (non-debug) version
ifdef bcc
CC		=	bc++ -q
CCPP	=	bc++ -q
LD		=	ilink -q
CFLAGS 	=	-D__unix__ -w-csu -w-pch -w-ccc -w-rch -w-par -w-aus
else
CC		=	gcc
CCPP	=	g++
LD		=	ld
CFLAGS	=	-Wall
endif
SLASH	=	/
OFILE	=	o

LIBFILE	=	.a
UIFC	=	../uifc/
XPDEV	=	../xpdev/

ifndef $(os)
os		=	$(shell uname)
$(warning OS not specified on command line, setting to '$(os)'.)
endif

ifeq ($(os),FreeBSD)	# FreeBSD
LIBODIR	:=	gcc.freebsd.lib
EXEODIR	:=	gcc.freebsd.exe
else                    # Linux
ifdef bcc
LIBODIR	:=	bcc.linux.lib
EXEODIR	:=	bcc.linux.exe
else
# -O doesn't work on FreeBSD (possible conflict with -g)
# CFLAGS	+=	-O
LIBODIR	:=	gcc.linux.lib
EXEODIR	:=	gcc.linux.exe
endif
endif

DELETE	=	rm -fv

CFLAGS	+=	-DJAVASCRIPT -I../mozilla/js/src -I$(XPDEV) -I$(UIFC)

ifeq ($(os),FreeBSD)	# FreeBSD
CFLAGS	+= -D_THREAD_SAFE
# Math libraries needed and uses pthread
LFLAGS	:=	-lm -pthread -lutil
else			# Linux / Other UNIX
# Math and pthread libraries needed
ifdef bcc
LFLAGS	:=	libpthread.so
else
LFLAGS	:=	-lm -lpthread -lutil
endif
endif

ifeq ($(os),Linux)    # Linux
CFLAGS	+= -D_THREAD_SUID_BROKEN
endif

ifdef DEBUG
ifdef bcc
CFLAGS	+=	-y -v -Od
else
CFLAGS	+=	-g
endif
CFLAGS  +=	-D_DEBUG
LIBODIR	:=	$(LIBODIR).debug
EXEODIR	:=	$(EXEODIR).debug
ifeq ($(os),FreeBSD)	# FreeBSD
LIBS	+=	../mozilla/js/src/FreeBSD4.3-RELEASE_DBG.OBJ/libjs.a
else			# Linux
LIBS	+=	../mozilla/js/src/Linux_All_DBG.OBJ/libjs.a
endif
else # RELEASE
LIBODIR	:=	$(LIBODIR).release
EXEODIR	:=	$(EXEODIR).release
ifeq ($(os),FreeBSD)	# FreeBSD
LIBS	+=	../mozilla/js/src/FreeBSD4.3-RELEASE_OPT.OBJ/libjs.a
else
LIBS	+=	../mozilla/js/src/Linux_All_OPT.OBJ/libjs.a
endif
endif

include targets.mk		# defines all targets
include objects.mk		# defines $(OBJS)
include headers.mk		# defines $(HEADERS)
include sbbsdefs.mk		# defines $(SBBSDEFS)

SBBSLIB	=	$(LIBODIR)/sbbs.a

vpath %.c $(XPDEV) $(UIFC)

# Implicit C Compile Rule for utils
$(EXEODIR)/%.o : %.c
ifndef bcc
	@echo Compiling $<
endif
	@$(CC) $(CFLAGS) -o $@ -c $<

# Implicit C Compile Rule for SBBS
$(LIBODIR)/%.o : %.c
ifndef bcc
	@echo Compiling $<
endif
	@$(CC) $(CFLAGS) $(SBBSDEFS) -o $@ -c $<

# Implicit C++ Compile Rule for SBBS
$(LIBODIR)/%.o : %.cpp
ifndef bcc
	@echo Compiling $<
endif
	@$(CCPP) $(CFLAGS) $(SBBSDEFS) -o $@ -c $<

# Create output directories
$(LIBODIR):
	mkdir $(LIBODIR)

$(EXEODIR):
	mkdir $(EXEODIR)

CON_OBJS	= $(EXEODIR)/sbbscon.o $(EXEODIR)/conwrap.o \
		  $(EXEODIR)/ini_file.o $(EXEODIR)/sbbs_ini.o
FTP_OBJS	= $(LIBODIR)/ftpsrvr.o
MAIL_OBJS	= $(LIBODIR)/mailsrvr.o $(LIBODIR)/mxlookup.o \
 		  $(LIBODIR)/mime.o
WEB_OBJS	= $(LIBODIR)/websrvr.o $(LIBODIR)/sockwrap.o
SERVICE_OBJS= $(LIBODIR)/services.o

MONO_OBJS	= $(CON_OBJS) $(FTP_OBJS) $(WEB_OBJS) \
			$(MAIL_OBJS) $(SERVICE_OBJS)

# Monolithic Synchronet executable Build Rule
$(SBBSMONO): $(MONO_OBJS) $(OBJS) $(LIBS) $(LIBODIR)/ver.o 
	@echo Linking $@
	@$(CCPP) -o $@ $(LFLAGS) $^

# Synchronet BBS library Link Rule
$(SBBS): $(OBJS) $(LIBS) $(LIBODIR)/ver.o
	$(LD) $(LFLAGS) -S -o $(SBBS) $^ $(LIBS) -o $@

# FTP Server Link Rule
$(FTPSRVR): $(LIBODIR)/ftpsrvr.o $(SBBSLIB)
	$(LD) $(LFLAGS) -S $^ $(LIBS) -o $@ 

# Mail Server Link Rule
$(MAILSRVR): $(MAIL_OBJS) $(SBBSLIB)
	$(LD) $(LFLAGS) -S $^ $(LIBS) -o $@

# Synchronet Console Build Rule
$(SBBSCON): $(CON_OBJS) $(SBBSLIB)
	$(CC) $(CFLAGS) -o $@ $^

# Specifc Compile Rules
$(LIBODIR)/ftpsrvr.o: ftpsrvr.c ftpsrvr.h
	@echo Compiling $<
	@$(CC) $(CFLAGS) -DFTPSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/mailsrvr.o: mailsrvr.c mailsrvr.h
	@echo Compiling $<
	@$(CC) $(CFLAGS) -DMAILSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/mxlookup.o: mxlookup.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -DMAILSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/mime.o: mime.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -DMAILSRVR_EXPORTS -o $@ -c $<		

$(LIBODIR)/websrvr.o: websrvr.c websrvr.h
	@echo Compiling $<
	@$(CC) $(CFLAGS) -DWEBSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/services.o: services.c services.h
	@echo Compiling $<
	@$(CC) $(CFLAGS) -DSERVICES_EXPORTS -o $@ -c $<

# Baja Utility
$(BAJA): $(EXEODIR)/baja.o $(EXEODIR)/ars.o $(EXEODIR)/crc32.o \
	$(EXEODIR)/genwrap.o $(EXEODIR)/filewrap.o
	@echo Linking $@
	@$(CC) -o $@ $^

# Node Utility
$(NODE): $(EXEODIR)/node.o $(EXEODIR)/genwrap.o $(EXEODIR)/filewrap.o
	@echo Linking $@
	@$(CC) -o $@ $^ 

SMBLIB = $(EXEODIR)/smblib.o $(EXEODIR)/filewrap.o

# FIXSMB Utility
$(FIXSMB): $(EXEODIR)/fixsmb.o $(SMBLIB) $(EXEODIR)/genwrap.o
	@echo Linking $@
	@$(CC) -o $@ $^

# CHKSMB Utility
$(CHKSMB): $(EXEODIR)/chksmb.o $(SMBLIB) $(EXEODIR)/conwrap.o $(EXEODIR)/dirwrap.o $(EXEODIR)/genwrap.o
	@echo Linking $@
	@$(CC) -o $@ $^

# SMB Utility
$(SMBUTIL): $(EXEODIR)/smbutil.o $(SMBLIB) $(EXEODIR)/conwrap.o $(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o $(EXEODIR)/smbtxt.o $(EXEODIR)/crc32.o $(EXEODIR)/lzh.o \
	$(EXEODIR)/date_str.o $(EXEODIR)/str_util.o
	@echo Linking $@
	@$(CC) -o $@ $^

# SBBSecho (FidoNet Packet Tosser)
$(SBBSECHO): \
	$(EXEODIR)/sbbsecho.o \
	$(EXEODIR)/ars.o \
	$(EXEODIR)/crc32.o \
	$(EXEODIR)/date_str.o \
	$(EXEODIR)/load_cfg.o \
	$(EXEODIR)/scfglib1.o \
	$(EXEODIR)/scfglib2.o \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/dat_rec.o \
	$(EXEODIR)/userdat.o \
	$(EXEODIR)/rechocfg.o \
	$(EXEODIR)/conwrap.o \
	$(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o \
	$(SMBLIB) \
	$(EXEODIR)/smbtxt.o \
	$(EXEODIR)/lzh.o
	@echo Linking $@
	@$(CC) -o $@ $^

# SBBSecho Configuration Program
$(ECHOCFG): \
	$(EXEODIR)/echocfg.o \
	$(EXEODIR)/rechocfg.o \
	$(EXEODIR)/uifcx.o \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/filewrap.o \
	$(EXEODIR)/genwrap.o
	@echo Linking $@
	@$(CC) -o $@ $^

# ADDFILES
$(ADDFILES): \
	$(EXEODIR)/addfiles.o \
	$(EXEODIR)/ars.o \
	$(EXEODIR)/date_str.o \
	$(EXEODIR)/load_cfg.o \
	$(EXEODIR)/scfglib1.o \
	$(EXEODIR)/scfglib2.o \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/dat_rec.o \
	$(EXEODIR)/filedat.o \
	$(EXEODIR)/filewrap.o \
	$(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o
	@echo Linking $@
	@$(CC) -o $@ $^

# FILELIST
$(FILELIST): \
	$(EXEODIR)/filelist.o \
	$(EXEODIR)/ars.o \
	$(EXEODIR)/date_str.o \
	$(EXEODIR)/load_cfg.o \
	$(EXEODIR)/scfglib1.o \
	$(EXEODIR)/scfglib2.o \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/dat_rec.o \
	$(EXEODIR)/filedat.o \
	$(EXEODIR)/filewrap.o \
	$(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o
	@echo Linking $@
	@$(CC) -o $@ $^

include depends.mk
