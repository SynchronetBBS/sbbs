# GNUmakefile

#########################################################################
# Makefile for Synchronet BBS for Unix									#
# For use with GNU make and GNU C Compiler or Borland Kylix C++			#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# gcc: gmake															#
# Borland (still in testing/debuging stage): gmake bcc=1				#
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

ifdef DEBUG
 BUILD	=	"debug"
else
 BUILD	=	"release"
endif

ifdef bcc
 CC		=	bc++ -q
 CCPRE	:=	bcc
 CCPP	=	bc++ -q
 LD		=	ilink -q
 CFLAGS +=	-mm -md -D__unix__ -w-csu -w-pch -w-ccc -w-rch -w-par -w-aus
else
 CC		=	gcc
 CCPRE	:=	gcc
 CCPP	=	g++
 LD		=	ld
 CFLAGS	+=	-MMD -Wall
endif
SLASH	=	/
OFILE	=	o

LIBFILE	=	.a
UIFC	=	../uifc/
XPDEV	=	../xpdev/

ifndef os
 os		:=	$(shell uname)
 $(warning OS not specified on command line, setting to '$(os)'.)
endif
# this line wont work with solaris unless awk in path is actually gawk 
os      :=	$(shell echo $(os) | tr "[A-Z]" "[a-z]")
#os      :=	$(shell echo $(os) | awk '/.*/ { print tolower($$1)}')

ifeq ($(os),freebsd)
 BSD	=	1
else
 ifeq ($(os),openbsd)
  BSD	=	1
 endif
endif

LIBODIR :=	$(CCPRE).$(os).lib.$(BUILD)
EXEODIR :=	$(CCPRE).$(os).exe.$(BUILD)

DELETE	=	rm -f

CFLAGS	+=	-DJAVASCRIPT -I../mozilla/js/src -I$(XPDEV) -I$(UIFC)

ifdef BSD	# BSD
 CFLAGS	+=	-D_THREAD_SAFE
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

ifeq ($(os),linux)    # Linux
 CFLAGS	+= -D_THREAD_SUID_BROKEN
endif

ifeq ($(os),sunos)    # Solaris
 CFLAGS	+= -D_REENTRANT -D__solaris__ -DNEEDS_DAEMON -D_POSIX_PTHREAD_SEMANTICS -DNEEDS_FORKPTY
 LFLAGS := -lm -lpthread -lsocket -lnsl -lrt
endif

ifdef DEBUG
 ifdef bcc
  CFLAGS	+=	-y -v -Od
 else
  CFLAGS	+=	-ggdb
 endif
 CFLAGS  +=	-D_DEBUG
endif

ifdef JSLIB
 LIBS	+=	$(JSLIB)
else
 LIBS	+=	../../lib/mozilla/js/$(os).$(BUILD)/libjs.a
endif

include targets.mk		# defines all targets
include objects.mk		# defines $(OBJS)
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
	@$(CC) $(CFLAGS) -o $@ $^

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

# MAKEUSER
$(MAKEUSER): \
	$(EXEODIR)/makeuser.o \
	$(EXEODIR)/ars.o \
	$(EXEODIR)/date_str.o \
	$(EXEODIR)/load_cfg.o \
	$(EXEODIR)/scfglib1.o \
	$(EXEODIR)/scfglib2.o \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/dat_rec.o \
	$(EXEODIR)/userdat.o \
	$(EXEODIR)/filewrap.o \
	$(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o
	@echo Linking $@
	@$(CC) -o $@ $^


# Auto-dependency files
-include $(LIBODIR)/*.d
-include $(EXEODIR)/*.d
