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

ifndef VERBOSE
 QUIET	=	@
endif

#USE_DIALOG =   1       # Dialog vesrion of UIFC
#USE_FLTK =     1       # Use Windowed version
#USE_CURSES =	1	# Use *nix curses version
ifndef NO_CURSES
 USE_UIFC32      =       1       # Curses version of UIFC
endif

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

UIFC	=	../uifc/
XPDEV	=	../xpdev/
LIBPREFIX =	lib

ifndef os
 os		:=	$(shell uname)
endif
ifeq ($(shell uname -m),ppc)
 os		:=	$(os)-ppc
endif
# this line wont work with solaris unless awk in path is actually gawk 
os      :=	$(shell echo $(os) | tr "[ A-Z]" "[\-a-z]")
# remove '/' from "os/2"
os      :=  $(shell echo $(os) | tr -d "/")

ifeq ($(os),openbsd)
LIBFILE =	.so.0.0
else
LIBFILE	=	.so
endif

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

CFLAGS  +=  -I$(XPDEV) -I$(UIFC) -DJAVASCRIPT
ifdef JSINCLUDE
 CFLAGS += -I$(JSINCLUDE)
else
 CFLAGS += -I../../include/mozilla/js
endif

ifdef BSD	# BSD
 # Math libraries needed and uses pthread
 LFLAGS	:=	-lm -lutil
 CFLAGS +=	-pthread 
else			# Linux / Other UNIX
 # Math and pthread libraries needed
 ifdef bcc
  LFLAGS	:=	libpthread.so
 else
  LFLAGS	:=	-lm -lpthread -lutil
 endif
endif

ifeq ($(os),linux)    # Linux
 ifndef THREADS_ACTUALLY_WORK
  CFLAGS	+= -D_THREAD_SUID_BROKEN
 endif
endif

ifeq ($(os),sunos)    # Solaris
 CFLAGS	:= -D_REENTRANT -D__solaris__ -DNEEDS_DAEMON -D_POSIX_PTHREAD_SEMANTICS -DNEEDS_FORKPTY
 LFLAGS := -lm -lpthread -lsocket -lnsl -lrt
endif

ifeq ($(os),netbsd)
 CFLAGS += -D_REENTRANT -D__unix__ -I/usr/pkg/include -DNEEDS_FORKPTY
 LFLAGS := -lm -lpthread -L/usr/pkg/lib -L/usr/pkg/pthreads/lib
 UTIL_LFLAGS	+=	-lpth -L/usr/pkg/lib
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

ifdef PREFIX
 CFLAGS += -DPREFIX=$(PREFIX)
endif

ifdef USE_DOSEMU
 CFLAGS += -DUSE_DOSEMU
endif

ifndef JSLIBDIR
 JSLIBDIR := ../../lib/mozilla/js/$(os).$(BUILD)
endif
ifndef JSLIB
 JSLIB	:=	js
endif
ifndef NSPRDIR
 NSPRDIR := ../../lib/mozilla/nspr/$(os).$(BUILD)
endif

ifdef DONT_BLAME_SYNCHRONET
 LFLAGS += -DDONT_BLAME_SYNCHRONET
endif

LFLAGS += -L$(JSLIBDIR) -l$(JSLIB)

# The following are needed for echocfg (uses UIFC)
UIFC_OBJS =	$(LIBODIR)/uifcx.o
ifdef USE_FLTK
 CFLAGS +=	-DUSE_FLTK -I../../include/fltk
 UIFC_LFLAGS += -L../../lib/fltk/$(os) -L/usr/X11R6/lib -lm -lfltk -lX11
 UIFC_OBJS+=	$(LIBODIR)/uifcfltk.o
endif
ifdef USE_CURSES
 CFLAGS +=	-DUSE_CURSES
 ifeq ($(os),qnx)
  UIFC_LFLAGS += -lncurses
 else
  UIFC_LFLAGS += -lcurses
 endif
 UIFC_OBJS +=	$(LIBODIR)/uifcc.o
endif

ifdef USE_UIFC32
 CFLAGS +=	-DUSE_UIFC32
 ifeq ($(os),qnx)
  UIFC_LFLAGS += -lncurses
 else
  UIFC_LFLAGS += -lcurses
 endif
 UIFC_OBJS +=	$(LIBODIR)/uifc32.o
 UIFC_OBJS +=	$(LIBODIR)/ciowrap.o
endif

#The following is needed for nspr support on Linux
ifeq ($(os),linux)
 LFLAGS	+=	-ldl
endif

include targets.mk		# defines all targets
include objects.mk		# defines $(OBJS)
include sbbsdefs.mk		# defines $(SBBSDEFS)

ifeq ($(USE_XP_SEMAPHORES),1)
 OBJS	+=	$(LIBODIR)$(SLASH)xpsem.$(OFILE)
endif

#SBBSLIB	=	$(LIBODIR)$(SLASH)libsbbs.so
SBBSLIB	=	-lsbbs

#dummy rule
$(SBBSLIB) : $(SBBS)

vpath %.c $(XPDEV) $(UIFC)
vpath %.cpp $(UIFC)

LFLAGS		+=	-L./$(LIBODIR) -L$(NSPRDIR)
SBBSLDFLAGS	:=	$(LFLAGS) -rpath-link ./$(LIBODIR) -rpath ./ 
#LFLAGS		+=	-Wl,-rpath-link,./$(LIBODIR),-rpath,./
LFLAGS		+=	-Xlinker -rpath
LFLAGS		+=	-Xlinker .
ifneq ($(os),openbsd)
LFLAGS		+=	-Xlinker -rpath-link
LFLAGS		+=	-Xlinker ./$(LIBODIR)
LFLAGS		+=	-Xlinker -rpath-link
LFLAGS		+=	-Xlinker $(JSLIBDIR)
LFLAGS		+=	-Xlinker -rpath-link
LFLAGS		+=	-Xlinker $(NSPRDIR)
else
LFLAGS		+=	-l$(JSLIB) -lnspr4
endif
ifeq ($(os),freebsd)
LFLAGS		+=	-pthread
endif
ifeq ($(os),openbsd)
LFLAGS		+=	-pthread
endif

# Implicit C Compile Rule for SBBS
$(LIBODIR)/%.o : %.c $(BUILD_DEPENDS)
   ifndef bcc
	@echo $(COMPILE_MSG) $<
   endif
	$(QUIET)$(CC) $(CFLAGS) $(SBBSDEFS) -o $@ -c $<

# Implicit C++ Compile Rule for SBBS
$(LIBODIR)/%.o : %.cpp $(BUILD_DEPENDS)
   ifndef bcc
	@echo $(COMPILE_MSG) $<
   endif
	$(QUIET)$(CCPP) $(CFLAGS) $(SBBSDEFS) -o $@ -c $<

$(LIBODIR):
	mkdir $(LIBODIR)

$(EXEODIR):
	mkdir $(EXEODIR)

CON_OBJS	= $(LIBODIR)/sbbscon.o $(LIBODIR)/conwrap.o \
		  $(LIBODIR)/sbbs_ini.o
CON_LDFLAGS	= -lftpsrvr -lwebsrvr -lmailsrvr -lservices
FTP_OBJS	= $(LIBODIR)/ftpsrvr.o
MAIL_OBJS	= $(LIBODIR)/mailsrvr.o $(LIBODIR)/mxlookup.o \
 		  $(LIBODIR)/mime.o $(LIBODIR)/base64.o
WEB_OBJS	= $(LIBODIR)/websrvr.o $(LIBODIR)/sockwrap.o $(LIBODIR)/base64.o
SERVICE_OBJS	= $(LIBODIR)/services.o $(LIBODIR)/ini_file.o

MONO_OBJS	= $(CON_OBJS) $(FTP_OBJS) $(WEB_OBJS) \
			$(MAIL_OBJS) $(SERVICE_OBJS)
SMBLIB_OBJS = \
	$(LIBODIR)/smblib.o \
	$(LIBODIR)/filewrap.o \
	$(LIBODIR)/crc16.o

SHLIBOPTS	:=	-shared

# Monolithic Synchronet executable Build Rule
FORCE$(SBBSMONO): $(MONO_OBJS) $(OBJS) $(LIBS)

$(SBBSMONO): $(MONO_OBJS) $(OBJS) $(LIBS)
	@echo Linking $@
	$(QUIET)$(CCPP) -o $@ $(LFLAGS) $^

# Synchronet BBS library Link Rule
FORCE$(SBBS): $(OBJS) $(LIBS)

$(SBBS): $(OBJS) $(LIBS)
	@echo Linking $@
	$(QUIET)$(CCPP) $(LFLAGS) -o $@ $^ $(SHLIBOPTS)

# FTP Server Link Rule
FORCE$(FTPSRVR): $(LIBODIR)/ftpsrvr.o $(SBBSLIB)

$(FTPSRVR): $(LIBODIR)/ftpsrvr.o $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(CC) $(LFLAGS) $^ $(SHLIBOPTS) -o $@ 

# Mail Server Link Rule
FORCE$(MAILSRVR): $(MAIL_OBJS) $(LIBODIR)$(SLASH)$(SBBSLIB)

$(MAILSRVR): $(MAIL_OBJS) $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(CC) $(LFLAGS) $^ $(SHLIBOPTS) -o $@

# Mail Server Link Rule
FORCE$(WEBSRVR): $(WEB_OBJS) $(SBBSLIB)

$(WEBSRVR): $(WEB_OBJS) $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(CC) $(LFLAGS) $^ $(SHLIBOPTS) -o $@

# Services Link Rule
FORCE$(SERVICES): $(WEB_OBJS) $(SBBSLIB)

$(SERVICES): $(SERVICE_OBJS) $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(CC) $(LFLAGS) $^ $(SHLIBOPTS) -o $@

# Synchronet Console Build Rule
FORCE$(SBBSCON): $(CON_OBJS) $(SBBSLIB) $(FTP_OBJS) $(MAIL_OBJS) $(WEB_OBJS) $(SERVICE_OBJS)

$(SBBSCON): $(CON_OBJS) $(SBBSLIB) $(FTPSRVR) $(WEBSRVR) $(MAILSRVR) $(SERVICES)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) $(LFLAGS) $(CON_LDFLAGS) -o $@ $(CON_OBJS) $(SBBSLIB)

# Specifc Compile Rules
$(LIBODIR)/ftpsrvr.o: ftpsrvr.c ftpsrvr.h $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) -DFTPSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/mailsrvr.o: mailsrvr.c mailsrvr.h $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) -DMAILSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/mxlookup.o: mxlookup.c $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) -DMAILSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/mime.o: mime.c $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) -DMAILSRVR_EXPORTS -o $@ -c $<		

$(LIBODIR)/websrvr.o: websrvr.c websrvr.h $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) -DWEBSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/base64.o: base64.c base64.h $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) -DWEBSRVR_EXPORTS -o $@ -c $<

$(LIBODIR)/services.o: services.c services.h $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) -DSERVICES_EXPORTS -o $@ -c $<

# Baja Utility
BAJA_OBJS = \
	$(LIBODIR)/baja.o \
	$(LIBODIR)/ars.o \
	$(LIBODIR)/crc32.o \
	$(LIBODIR)/genwrap.o \
	$(LIBODIR)/filewrap.o
FORCE$(BAJA): $(BAJA_OBJS)

$(BAJA): $(BAJA_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LFLAGS) -o $@ $^

# Node Utility
NODE_OBJS = \
	$(LIBODIR)/node.o \
	$(LIBODIR)/genwrap.o \
	$(LIBODIR)/filewrap.o
FORCE$(NODE): $(NODE_OBJS)

$(NODE): $(NODE_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LFLAGS) -o $@ $^ 

# FIXSMB Utility
FIXSMB_OBJS = \
	$(LIBODIR)/fixsmb.o \
	$(SMBLIB_OBJS) \
	$(LIBODIR)/genwrap.o \
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/str_util.o
FORCE$(FIXSMB): $(FIXSMB_OBJS)
	
$(FIXSMB): $(FIXSMB_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LFLAGS) -o $@ $^

# CHKSMB Utility
CHKSMB_OBJS = \
	$(LIBODIR)/chksmb.o \
	$(SMBLIB_OBJS) \
	$(LIBODIR)/conwrap.o \
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/genwrap.o
FORCE$(CHKSMB): $(CHKSMB_OBJS)

$(CHKSMB): $(CHKSMB_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LFLAGS) -o $@ $^

# SMB Utility
SMBUTIL_OBJS = \
	$(LIBODIR)/smbutil.o \
	$(SMBLIB_OBJS) \
	$(LIBODIR)/conwrap.o \
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/genwrap.o \
	$(LIBODIR)/smbtxt.o \
	$(LIBODIR)/crc32.o \
	$(LIBODIR)/lzh.o \
	$(LIBODIR)/date_str.o \
	$(LIBODIR)/str_util.o
FORCE$(SMBUTIL): $(SMBUTIL_OBJS)
	
$(SMBUTIL): $(SMBUTIL_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LFLAGS) -o $@ $^

# SBBSecho (FidoNet Packet Tosser)
SBBSECHO_OBJS = \
	$(LIBODIR)/sbbsecho.o \
	$(LIBODIR)/ars.o \
	$(LIBODIR)/crc32.o \
	$(LIBODIR)/date_str.o \
	$(LIBODIR)/load_cfg.o \
	$(LIBODIR)/scfglib1.o \
	$(LIBODIR)/scfglib2.o \
	$(LIBODIR)/nopen.o \
	$(LIBODIR)/str_util.o \
	$(LIBODIR)/dat_rec.o \
	$(LIBODIR)/userdat.o \
	$(LIBODIR)/rechocfg.o \
	$(LIBODIR)/conwrap.o \
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/genwrap.o \
	$(SMBLIB_OBJS) \
	$(LIBODIR)/smbtxt.o \
	$(LIBODIR)/lzh.o

FORCE$(SBBSECHO): $(SBBSECHO_OBJS)

$(SBBSECHO): $(SBBSECHO_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LFLAGS) -o $@ $^

# SBBSecho Configuration Program
ECHOCFG_OBJS = \
	$(LIBODIR)/echocfg.o \
	$(LIBODIR)/rechocfg.o \
	$(UIFC_OBJS) \
	$(LIBODIR)/nopen.o \
	$(LIBODIR)/crc16.o \
	$(LIBODIR)/str_util.o \
	$(LIBODIR)/filewrap.o \
	$(LIBODIR)/genwrap.o \
	$(LIBODIR)/dirwrap.o

FORCE$(ECHOCFG): $(ECHOCFG_OBJS)

$(ECHOCFG): $(ECHOCFG_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LFLAGS) -o $@ $^ $(UIFC_LFLAGS)

# ADDFILES
ADDFILES_OBJS = \
	$(LIBODIR)/addfiles.o \
	$(LIBODIR)/ars.o \
	$(LIBODIR)/date_str.o \
	$(LIBODIR)/load_cfg.o \
	$(LIBODIR)/scfglib1.o \
	$(LIBODIR)/scfglib2.o \
	$(LIBODIR)/nopen.o \
	$(LIBODIR)/str_util.o \
	$(LIBODIR)/dat_rec.o \
	$(LIBODIR)/userdat.o \
	$(LIBODIR)/filedat.o \
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/genwrap.o \
	$(SMBLIB_OBJS)

FORCE$(ADDFILES): $(ADDFILES_OBJS)

$(ADDFILES): $(ADDFILES_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LFLAGS) -o $@ $^

# FILELIST
FILELIST_OBJS = \
	$(LIBODIR)/filelist.o \
	$(LIBODIR)/ars.o \
	$(LIBODIR)/date_str.o \
	$(LIBODIR)/load_cfg.o \
	$(LIBODIR)/scfglib1.o \
	$(LIBODIR)/scfglib2.o \
	$(LIBODIR)/nopen.o \
	$(LIBODIR)/str_util.o \
	$(LIBODIR)/dat_rec.o \
	$(LIBODIR)/filedat.o \
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/genwrap.o \
	$(SMBLIB_OBJS)

FORCE$(FILELIST): $(FILELIST_OBJS)

$(FILELIST): $(FILELIST_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LFLAGS) -o $@ $^

# MAKEUSER
MAKEUSER_OBJS = \
	$(LIBODIR)/makeuser.o \
	$(LIBODIR)/ars.o \
	$(LIBODIR)/date_str.o \
	$(LIBODIR)/load_cfg.o \
	$(LIBODIR)/scfglib1.o \
	$(LIBODIR)/scfglib2.o \
	$(LIBODIR)/nopen.o \
	$(LIBODIR)/str_util.o \
	$(LIBODIR)/dat_rec.o \
	$(LIBODIR)/userdat.o \
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/genwrap.o \
	$(SMBLIB_OBJS)

FORCE$(MAKEUSER): $(MAKEUSER_OBJS)

$(MAKEUSER): $(MAKEUSER_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LFLAGS) -o $@ $^

# JSEXEC
JSEXEC_OBJS = \
	$(LIBODIR)/jsexec.o \
	$(SBBSLIB)

FORCE$(JSEXEC): $(JSEXEC_OBJS)

$(JSEXEC): $(JSEXEC_OBJS)
	@echo Linking $@
	$(QUIET)$(CCPP) $(UTIL_LFLAGS) -o $@ $^ $(LFLAGS)
	
# ANS2MSG
$(ANS2MSG): $(LIBODIR)/ans2msg.o
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LFLAGS) -o $@ $^

# MSG2ANS
$(MSG2ANS): $(LIBODIR)/msg2ans.o
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LFLAGS) -o $@ $^

depend:
	$(QUIET)$(DELETE) $(LIBODIR)/.depend
	$(QUIET)$(DELETE) $(EXEODIR)/.depend
	$(MAKE) BUILD_DEPENDS=FORCE

FORCE:

-include $(LIBODIR)/.depend
-include $(EXEODIR)/.depend
-include $(LIBODIR)/*.d
-include $(EXEODIR)/*.d
