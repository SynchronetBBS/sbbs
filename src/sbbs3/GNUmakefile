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

#USE_DIALOG =   1       # Dialog vesrion of UIFC
#USE_FLTK =     1       # Use Windowed version
USE_CURSES      =       1       # Curses version of UIFC

ifdef DEBUG
 BUILD	=	debug
else
 BUILD	=	release
endif

ifdef bcc
 CC		=	bc++ -q
 CCPRE	:=	bcc
 CCPP	=	bc++ -q
 LD		=	ilink -q
 CFLAGS +=	-mm -md -D__unix__ -w-csu -w-pch -w-ccc -w-rch -w-par -w-aus
else
 CFLAGS	+=	-MMD -Wall
 CCPRE	:=	gcc
 ifdef BUILD_DEPENDS
  CC		=	../build/mkdep -a
  CCPP	=	../build/mkdep -a
  LD		=	echo
  COMPILE_MSG	:= Depending
 else
  CC		=	gcc
  CCPP	=	g++
  LD		=	ld
  COMPILE_MSG	:= Compiling
 endif
endif
SLASH	=	/
OFILE	=	o

LIBFILE	=	.a
UIFC	=	../uifc/
XPDEV	=	../xpdev/

ifndef os
 os		:=	$(shell uname)
endif
# this line wont work with solaris unless awk in path is actually gawk 
os      :=	$(shell echo $(os) | tr "[A-Z]" "[a-z]")
#os      :=	$(shell echo $(os) | awk '/.*/ { print tolower($$1)}')
# remove '/' from "os/2"
os      :=  $(shell echo $(os) | tr -d "/")

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

CFLAGS	+=	-DJAVASCRIPT -I../../include/mozilla/js -I$(XPDEV) -I$(UIFC)

ifdef BSD	# BSD
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

ifeq ($(os),netbsd)
 CFLAGS += -D_REENTRANT -D__unix__ -I/usr/pkg/include -DNEEDS_FORKPTY
 LFLAGS := -lm -lpthread -L/usr/pkg/lib
endif

# So far, only QNX has sem_timedwait()
ifeq ($(os),qnx)
 LFLAGS := -lm -lsocket
else
 CFLAGS	+=	-DUSE_XP_SEMAPHORES
 USE_XP_SEMAPHORES	:=	1
endif

ifdef DEBUG
 ifdef bcc
  CFLAGS	+=	-y -v -Od
 else
  CFLAGS	+=	-ggdb
 endif
 CFLAGS  +=	-D_DEBUG
else
 CFLAGS	+= -O3
endif

ifdef JSLIB
 LIBS	+=	$(JSLIB)
else
 LIBS	+=	../../lib/mozilla/js/$(os).$(BUILD)/libjs.a
endif

# The following are needed for echocfg (uses UIFC)
UIFC_OBJS =	$(EXEODIR)/uifcx.o
ifdef USE_FLTK
 CFLAGS +=	-DUSE_FLTK -I../../include/fltk
 UIFC_LFLAGS += -L../../lib/fltk/$(os) -L/usr/X11R6/lib -lm -lfltk -lX11
 UIFC_OBJS+=	$(EXEODIR)/uifcfltk.o
endif
ifdef USE_CURSES
 CFLAGS +=	-DUSE_CURSES
 ifeq ($(os),qnx)
  UIFC_LFLAGS += -lncurses
 else
  UIFC_LFLAGS += -lcurses
 endif
 UIFC_OBJS +=	$(EXEODIR)/uifcc.o
endif

include targets.mk		# defines all targets
include objects.mk		# defines $(OBJS)
include sbbsdefs.mk		# defines $(SBBSDEFS)

ifeq ($(USE_XP_SEMAPHORES),1)
 OBJS	+=	$(LIBODIR)$(SLASH)xpsem.$(OFILE)
endif

SBBSLIB	=	$(LIBODIR)/sbbs.a

vpath %.c $(XPDEV) $(UIFC)
vpath %.cpp $(UIFC)

# Implicit C Compile Rule for utils
$(EXEODIR)/%.o : %.c $(BUILD_DEPENDS)
   ifndef bcc
	@echo $(COMPILE_MSG) $<
   endif
	@$(CC) $(CFLAGS) -o $@ -c $<

# Implicit C++ Compile Rule for utils
$(EXEODIR)/%.o : %.cpp $(BUILD_DEPENDS)
   ifndef bcc
	@echo $(COMPILE_MSG) $<
   endif
	@$(CCPP) $(CFLAGS) -o $@ -c $<

# Implicit C Compile Rule for SBBS
$(LIBODIR)/%.o : %.c $(BUILD_DEPENDS)
   ifndef bcc
	@echo $(COMPILE_MSG) $<
   endif
	@$(CC) $(CFLAGS) $(SBBSDEFS) -o $@ -c $<

# Implicit C++ Compile Rule for SBBS
$(LIBODIR)/%.o : %.cpp $(BUILD_DEPENDS)
   ifndef bcc
	@echo $(COMPILE_MSG) $<
   endif
	@$(CCPP) $(CFLAGS) $(SBBSDEFS) -o $@ -c $<

$(LIBODIR):
	mkdir $(LIBODIR)

$(EXEODIR):
	mkdir $(EXEODIR)

CON_OBJS	= $(EXEODIR)/sbbscon.o $(EXEODIR)/conwrap.o \
		  $(EXEODIR)/sbbs_ini.o
FTP_OBJS	= $(LIBODIR)/ftpsrvr.o
MAIL_OBJS	= $(LIBODIR)/mailsrvr.o $(LIBODIR)/mxlookup.o \
 		  $(LIBODIR)/mime.o $(LIBODIR)/base64.o
WEB_OBJS	= $(LIBODIR)/websrvr.o $(LIBODIR)/sockwrap.o $(LIBODIR)/base64.o
SERVICE_OBJS= $(LIBODIR)/services.o

MONO_OBJS	= $(CON_OBJS) $(FTP_OBJS) $(WEB_OBJS) \
			$(MAIL_OBJS) $(SERVICE_OBJS)

# Monolithic Synchronet executable Build Rule
FORCE$(SBBSMONO): \
	$(MONO_OBJS) $(OBJS) $(LIBS) $(LIBODIR)/ver.o

$(SBBSMONO): $(MONO_OBJS) $(OBJS) $(LIBS) $(LIBODIR)/ver.o
	@echo Linking $@
	@$(CCPP) -o $@ $(LFLAGS) $^

# Synchronet BBS library Link Rule
FORCE$(SBBS): \
	$(OBJS) $(LIBS) $(LIBODIR)/ver.o

$(SBBS): $(OBJS) $(LIBS) $(LIBODIR)/ver.o
	$(LD) $(LFLAGS) -S -o $(SBBS) $^ $(LIBS) -o $@

# FTP Server Link Rule
FORCE$(FTPSRVR): \
	$(LIBODIR)/ftpsrvr.o $(SBBSLIB)

$(FTPSRVR): $(LIBODIR)/ftpsrvr.o $(SBBSLIB)
	$(LD) $(LFLAGS) -S $^ $(LIBS) -o $@ 

# Mail Server Link Rule
FORCE$(MAILSRVR): $(MAIL_OBJS) $(SBBSLIB)

$(MAILSRVR): $(MAIL_OBJS) $(SBBSLIB)
	$(LD) $(LFLAGS) -S $^ $(LIBS) -o $@

# Synchronet Console Build Rule
FORCE$(SBBSCON): \
	$(CON_OBJS) $(SBBSLIB)

$(SBBSCON): $(CON_OBJS) $(SBBSLIB)
	@$(CC) $(CFLAGS) -o $@ $^

# Specifc Compile Rules
$(LIBODIR)/ftpsrvr.o: ftpsrvr.c ftpsrvr.h $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	@$(CC) $(CFLAGS) -DFTPSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/mailsrvr.o: mailsrvr.c mailsrvr.h $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	@$(CC) $(CFLAGS) -DMAILSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/mxlookup.o: mxlookup.c $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	@$(CC) $(CFLAGS) -DMAILSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/mime.o: mime.c $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	@$(CC) $(CFLAGS) -DMAILSRVR_EXPORTS -o $@ -c $<		

$(LIBODIR)/websrvr.o: websrvr.c websrvr.h $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	@$(CC) $(CFLAGS) -DWEBSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/base64.o: base64.c base64.h $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	@$(CC) $(CFLAGS) -DWEBSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/services.o: services.c services.h $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	@$(CC) $(CFLAGS) -DSERVICES_EXPORTS -o $@ -c $<

# Baja Utility
FORCE$(BAJA): \
	$(EXEODIR)/baja.o $(EXEODIR)/ars.o $(EXEODIR)/crc32.o \
	$(EXEODIR)/genwrap.o $(EXEODIR)/filewrap.o

$(BAJA): $(EXEODIR)/baja.o $(EXEODIR)/ars.o $(EXEODIR)/crc32.o \
	$(EXEODIR)/genwrap.o $(EXEODIR)/filewrap.o
	@echo Linking $@
	@$(CC) -o $@ $^

# Node Utility
FORCE$(NODE): \
	$(EXEODIR)/node.o $(EXEODIR)/genwrap.o $(EXEODIR)/filewrap.o

$(NODE): $(EXEODIR)/node.o $(EXEODIR)/genwrap.o $(EXEODIR)/filewrap.o
	@echo Linking $@
	@$(CC) -o $@ $^ 

SMBLIB = $(EXEODIR)/smblib.o $(EXEODIR)/filewrap.o $(EXEODIR)/crc16.o

# FIXSMB Utility
FORCE$(FIXSMB): \
	$(EXEODIR)/fixsmb.o $(SMBLIB) $(EXEODIR)/genwrap.o $(EXEODIR)/str_util.o

$(FIXSMB): $(EXEODIR)/fixsmb.o $(SMBLIB) $(EXEODIR)/genwrap.o $(EXEODIR)/str_util.o
	@echo Linking $@
	@$(CC) -o $@ $^

# CHKSMB Utility
FORCE$(CHKSMB): \
	$(EXEODIR)/chksmb.o $(SMBLIB) $(EXEODIR)/conwrap.o $(EXEODIR)/dirwrap.o $(EXEODIR)/genwrap.o

$(CHKSMB): $(EXEODIR)/chksmb.o $(SMBLIB) $(EXEODIR)/conwrap.o $(EXEODIR)/dirwrap.o $(EXEODIR)/genwrap.o
	@echo Linking $@
	@$(CC) -o $@ $^

# SMB Utility
FORCE$(SMBUTIL): \
	$(EXEODIR)/smbutil.o $(SMBLIB) $(EXEODIR)/conwrap.o $(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o $(EXEODIR)/smbtxt.o $(EXEODIR)/crc32.o $(EXEODIR)/lzh.o \
	$(EXEODIR)/date_str.o $(EXEODIR)/str_util.o

$(SMBUTIL): $(EXEODIR)/smbutil.o $(SMBLIB) $(EXEODIR)/conwrap.o $(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o $(EXEODIR)/smbtxt.o $(EXEODIR)/crc32.o $(EXEODIR)/lzh.o \
	$(EXEODIR)/date_str.o $(EXEODIR)/str_util.o
	@echo Linking $@
	@$(CC) -o $@ $^

# SBBSecho (FidoNet Packet Tosser)
FORCE$(SBBSECHO): \
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
FORCE$(ECHOCFG): \
	$(EXEODIR)/echocfg.o \
	$(EXEODIR)/rechocfg.o \
	$(UIFC_OBJS) \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/crc16.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/filewrap.o \
	$(EXEODIR)/genwrap.o \
	$(EXEODIR)/dirwrap.o

$(ECHOCFG): \
	$(EXEODIR)/echocfg.o \
	$(EXEODIR)/rechocfg.o \
	$(UIFC_OBJS) \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/crc16.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/filewrap.o \
	$(EXEODIR)/genwrap.o \
	$(EXEODIR)/dirwrap.o
	@echo Linking $@
	@$(CC) -o $@ $^ $(UIFC_LFLAGS)

# ADDFILES
FORCE$(ADDFILES): \
	$(EXEODIR)/addfiles.o \
	$(EXEODIR)/ars.o \
	$(EXEODIR)/date_str.o \
	$(EXEODIR)/load_cfg.o \
	$(EXEODIR)/scfglib1.o \
	$(EXEODIR)/scfglib2.o \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/crc16.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/dat_rec.o \
	$(EXEODIR)/filedat.o \
	$(EXEODIR)/filewrap.o \
	$(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o

$(ADDFILES): \
	$(EXEODIR)/addfiles.o \
	$(EXEODIR)/ars.o \
	$(EXEODIR)/date_str.o \
	$(EXEODIR)/load_cfg.o \
	$(EXEODIR)/scfglib1.o \
	$(EXEODIR)/scfglib2.o \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/crc16.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/dat_rec.o \
	$(EXEODIR)/filedat.o \
	$(EXEODIR)/filewrap.o \
	$(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o
	@echo Linking $@
	@$(CC) -o $@ $^

# FILELIST
FORCE$(FILELIST): \
	$(EXEODIR)/filelist.o \
	$(EXEODIR)/ars.o \
	$(EXEODIR)/date_str.o \
	$(EXEODIR)/load_cfg.o \
	$(EXEODIR)/scfglib1.o \
	$(EXEODIR)/scfglib2.o \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/crc16.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/dat_rec.o \
	$(EXEODIR)/filedat.o \
	$(EXEODIR)/filewrap.o \
	$(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o

$(FILELIST): \
	$(EXEODIR)/filelist.o \
	$(EXEODIR)/ars.o \
	$(EXEODIR)/date_str.o \
	$(EXEODIR)/load_cfg.o \
	$(EXEODIR)/scfglib1.o \
	$(EXEODIR)/scfglib2.o \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/crc16.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/dat_rec.o \
	$(EXEODIR)/filedat.o \
	$(EXEODIR)/filewrap.o \
	$(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o
	@echo Linking $@
	@$(CC) -o $@ $^

# MAKEUSER
FORCE$(MAKEUSER): \
	$(EXEODIR)/makeuser.o \
	$(EXEODIR)/ars.o \
	$(EXEODIR)/date_str.o \
	$(EXEODIR)/load_cfg.o \
	$(EXEODIR)/scfglib1.o \
	$(EXEODIR)/scfglib2.o \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/crc16.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/dat_rec.o \
	$(EXEODIR)/userdat.o \
	$(EXEODIR)/filewrap.o \
	$(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o

$(MAKEUSER): \
	$(EXEODIR)/makeuser.o \
	$(EXEODIR)/ars.o \
	$(EXEODIR)/date_str.o \
	$(EXEODIR)/load_cfg.o \
	$(EXEODIR)/scfglib1.o \
	$(EXEODIR)/scfglib2.o \
	$(EXEODIR)/nopen.o \
	$(EXEODIR)/crc16.o \
	$(EXEODIR)/str_util.o \
	$(EXEODIR)/dat_rec.o \
	$(EXEODIR)/userdat.o \
	$(EXEODIR)/filewrap.o \
	$(EXEODIR)/dirwrap.o \
	$(EXEODIR)/genwrap.o
	@echo Linking $@
	@$(CC) -o $@ $^

FORCE$(ANS2MSG): $(ANS2MSG).o

FORCE$(MSG2ANS): $(MSG2ANS).o

depend:
	@$(DELETE) $(LIBODIR)/.depend
	@$(DELETE) $(EXEODIR)/.depend
	$(MAKE) BUILD_DEPENDS=FORCE

FORCE:

-include $(LIBODIR)/.depend
-include $(EXEODIR)/.depend
-include $(LIBODIR)/*.d
-include $(EXEODIR)/*.d
