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

LIBFILE	=	.so
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
 LFLAGS	:=	-lm -lutil -lc_r
 CFLAGS +=	-pthread 
else			# Linux / Other UNIX
 # Math and pthread libraries needed
 ifdef bcc
  LFLAGS	:=	libpthread.so -lc
 else
  LFLAGS	:=	-lm -lpthread -lutil -lc
 endif
endif

ifeq ($(os),linux)    # Linux
 CFLAGS	+= -D_THREAD_SUID_BROKEN
endif

ifeq ($(os),sunos)    # Solaris
 CFLAGS	:= -D_REENTRANT -D__solaris__ -DNEEDS_DAEMON -D_POSIX_PTHREAD_SEMANTICS -DNEEDS_FORKPTY
 LFLAGS := -lm -lpthread -lsocket -lnsl -lrt
endif

ifeq ($(os),netbsd)
 CFLAGS += -D_REENTRANT -D__unix__ -I/usr/pkg/include -DNEEDS_FORKPTY
 LFLAGS := -lm -lpthread -L/usr/pkg/lib -L/usr/pkg/pthreads/lib
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
 LIBS	+=	../../lib/mozilla/js/$(os).$(BUILD)/libjs.a ../../lib/mozilla/nspr/$(os).$(BUILD)/libnspr4.a
endif

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

LFLAGS		+=	-L./$(LIBODIR)
SBBSLDFLAGS	:=	$(LFLAGS) -rpath-link ./$(LIBODIR) -rpath ./ 
#LFLAGS		+=	-Wl,-rpath-link,./$(LIBODIR),-rpath,./
LFLAGS		+=	-Xlinker -rpath
LFLAGS		+=	-Xlinker .
ifneq ($(os),openbsd)
LFLAGS		+=	-Xlinker -rpath-link
LFLAGS		+=	-Xlinker ./$(LIBODIR)
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
SERVICE_OBJS	= $(LIBODIR)/services.o

MONO_OBJS	= $(CON_OBJS) $(FTP_OBJS) $(WEB_OBJS) \
			$(MAIL_OBJS) $(SERVICE_OBJS)
SMBLIB_OBJS = \
	$(LIBODIR)/smblib.o \
	$(LIBODIR)/filewrap.o \
	$(LIBODIR)/crc16.o


# Monolithic Synchronet executable Build Rule
FORCE$(SBBSMONO): $(MONO_OBJS) $(OBJS) $(LIBS)

$(SBBSMONO): $(MONO_OBJS) $(OBJS) $(LIBS)
	@echo Linking $@
	$(QUIET)$(CCPP) -o $@ $(LFLAGS) $^

# Synchronet BBS library Link Rule
FORCE$(SBBS): $(OBJS) $(LIBS)

$(SBBS): $(OBJS) $(LIBS)
	@echo Linking $@
	$(QUIET)$(CCPP) $(LFLAGS) -o $(SBBS) $^ -shared -o $@

# FTP Server Link Rule
FORCE$(FTPSRVR): $(LIBODIR)/ftpsrvr.o $(SBBSLIB)

$(FTPSRVR): $(LIBODIR)/ftpsrvr.o $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(CC) $(LFLAGS) $^ -shared -o $@ 

# Mail Server Link Rule
FORCE$(MAILSRVR): $(MAIL_OBJS) $(LIBODIR)$(SLASH)$(SBBSLIB)

$(MAILSRVR): $(MAIL_OBJS) $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(CC) $(LFLAGS) $^ -shared -o $@

# Mail Server Link Rule
FORCE$(WEBSRVR): $(WEB_OBJS) $(SBBSLIB)

$(WEBSRVR): $(WEB_OBJS) $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(CC) $(LFLAGS) $^ -shared -o $@

# Services Link Rule
FORCE$(SERVICES): $(WEB_OBJS) $(SBBSLIB)

$(SERVICES): $(SERVICE_OBJS) $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(CC) $(LFLAGS) $^ -shared -o $@

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
	$(QUIET)$(CC) -o $@ $^

# Node Utility
NODE_OBJS = \
	$(LIBODIR)/node.o \
	$(LIBODIR)/genwrap.o \
	$(LIBODIR)/filewrap.o
FORCE$(NODE): $(NODE_OBJS)

$(NODE): $(NODE_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $^ 

# FIXSMB Utility
FIXSMB_OBJS = \
	$(LIBODIR)/fixsmb.o \
	$(SMBLIB_OBJS) \
	$(LIBODIR)/genwrap.o \
	$(LIBODIR)/str_util.o
FORCE$(FIXSMB): $(FIXSMB_OBJS)
	
$(FIXSMB): $(FIXSMB_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $^

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
	$(QUIET)$(CC) -o $@ $^

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
	$(QUIET)$(CC) -o $@ $^

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
	$(QUIET)$(CC) -o $@ $^

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
	$(QUIET)$(CC) -o $@ $^ $(UIFC_LFLAGS)

# ADDFILES
ADDFILES_OBJS = \
	$(LIBODIR)/addfiles.o \
	$(LIBODIR)/ars.o \
	$(LIBODIR)/date_str.o \
	$(LIBODIR)/load_cfg.o \
	$(LIBODIR)/scfglib1.o \
	$(LIBODIR)/scfglib2.o \
	$(LIBODIR)/nopen.o \
	$(LIBODIR)/crc16.o \
	$(LIBODIR)/str_util.o \
	$(LIBODIR)/dat_rec.o \
	$(LIBODIR)/userdat.o \
	$(LIBODIR)/filedat.o \
	$(LIBODIR)/filewrap.o \
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/genwrap.o

FORCE$(ADDFILES): $(ADDFILES_OBJS)

$(ADDFILES): $(ADDFILES_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $^

# FILELIST
FILELIST_OBJS = \
	$(LIBODIR)/filelist.o \
	$(LIBODIR)/ars.o \
	$(LIBODIR)/date_str.o \
	$(LIBODIR)/load_cfg.o \
	$(LIBODIR)/scfglib1.o \
	$(LIBODIR)/scfglib2.o \
	$(LIBODIR)/nopen.o \
	$(LIBODIR)/crc16.o \
	$(LIBODIR)/str_util.o \
	$(LIBODIR)/dat_rec.o \
	$(LIBODIR)/filedat.o \
	$(LIBODIR)/filewrap.o \
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/genwrap.o

FORCE$(FILELIST): $(FILELIST_OBJS)

$(FILELIST): $(FILELIST_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $^

# MAKEUSER
MAKEUSER_OBJS = \
	$(LIBODIR)/makeuser.o \
	$(LIBODIR)/ars.o \
	$(LIBODIR)/date_str.o \
	$(LIBODIR)/load_cfg.o \
	$(LIBODIR)/scfglib1.o \
	$(LIBODIR)/scfglib2.o \
	$(LIBODIR)/nopen.o \
	$(LIBODIR)/crc16.o \
	$(LIBODIR)/str_util.o \
	$(LIBODIR)/dat_rec.o \
	$(LIBODIR)/userdat.o \
	$(LIBODIR)/filewrap.o \
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/genwrap.o

FORCE$(MAKEUSER): $(MAKEUSER_OBJS)

$(MAKEUSER): $(MAKEUSER_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $^

# JSEXEC
JSEXEC_OBJS = \
	$(LIBODIR)/jsexec.o \
	$(SBBSLIB)

FORCE$(JSEXEC): $(JSEXEC_OBJS)

$(JSEXEC): $(JSEXEC_OBJS)
	@echo Linking $@
	$(QUIET)$(CCPP) -o $@ $(LFLAGS) $^
	
# ANS2MSG
$(ANS2MSG): $(LIBODIR)/ans2msg.o
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $^

# MSG2ANS
$(MSG2ANS): $(LIBODIR)/msg2ans.o
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $^

# Single servers
FTPCON_OBJS	= $(LIBODIR)/sbbsftp.o $(LIBODIR)/conwrap.o \
		  $(LIBODIR)/sbbs_ini.o
WEBCON_OBJS	= $(LIBODIR)/sbbsweb.o $(LIBODIR)/conwrap.o \
		  $(LIBODIR)/sbbs_ini.o
MAILCON_OBJS	= $(LIBODIR)/sbbsmail.o $(LIBODIR)/conwrap.o \
		  $(LIBODIR)/sbbs_ini.o
SRVCCON_OBJS	= $(LIBODIR)/sbbssrvc.o $(LIBODIR)/conwrap.o \
		  $(LIBODIR)/sbbs_ini.o
BBSCON_OBJS	= $(LIBODIR)/sbbs_bbs.o $(LIBODIR)/conwrap.o \
		  $(LIBODIR)/sbbs_ini.o

$(LIBODIR)/sbbsweb.o : sbbscon.c
   ifndef bcc
	@echo $(COMPILE_MSG) $<
   endif
	$(QUIET)$(CC) $(CFLAGS) -o $@ -c $< -DNO_TELNET_SERVER -DNO_FTP_SERVER -DNO_MAIL_SERVER -DNO_SERVICES

$(LIBODIR)/sbbsftp.o : sbbscon.c
   ifndef bcc
	@echo $(COMPILE_MSG) $<
   endif
	$(QUIET)$(CC) $(CFLAGS) -o $@ -c $< -DNO_TELNET_SERVER -DNO_MAIL_SERVER -DNO_SERVICES -DNO_WEB_SERVER

$(LIBODIR)/sbbsmail.o : sbbscon.c
   ifndef bcc
	@echo $(COMPILE_MSG) $<
   endif
	$(QUIET)$(CC) $(CFLAGS) -o $@ -c $< -DNO_TELNET_SERVER -DNO_FTP_SERVER -DNO_SERVICES -DNO_WEB_SERVER

$(LIBODIR)/sbbssrvc.o : sbbscon.c
   ifndef bcc
	@echo $(COMPILE_MSG) $<
   endif
	$(QUIET)$(CC) $(CFLAGS) -o $@ -c $< -DNO_TELNET_SERVER -DNO_FTP_SERVER -DNO_MAIL_SERVER -DNO_WEB_SERVER

$(LIBODIR)/sbbs_bbs.o : sbbscon.c
   ifndef bcc
	@echo $(COMPILE_MSG) $<
   endif
	$(QUIET)$(CC) $(CFLAGS) -o $@ -c $< -DNO_FTP_SERVER -DNO_MAIL_SERVER -DNO_SERVICES -DNO_WEB_SERVER

$(SBBSWEB): $(WEBCON_OBJS) $(SBBSLIB) $(WEBSRVR)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) $(LFLAGS) -lwebsrvr -o $@ $(WEBCON_OBJS) $(SBBSLIB)

$(SBBSFTP): $(FTPCON_OBJS) $(SBBSLIB) $(FTPSRVR)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) $(LFLAGS) -lftpsrvr -o $@ $(FTPCON_OBJS) $(SBBSLIB)

$(SBBSMAIL): $(MAILCON_OBJS) $(SBBSLIB) $(MAILSRVR)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) $(LFLAGS) -lmailsrvr -o $@ $(MAILCON_OBJS) $(SBBSLIB)

$(SBBSSRVC): $(SRVCCON_OBJS) $(SBBSLIB) $(SERVICES)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) $(LFLAGS) -lservices -o $@ $(SRVCCON_OBJS) $(SBBSLIB)

$(SBBS_BBS): $(BBSCON_OBJS) $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) $(LFLAGS) -o $@ $(BBSCON_OBJS) $(SBBSLIB)


depend:
	$(QUIET)$(DELETE) $(LIBODIR)/.depend
	$(QUIET)$(DELETE) $(EXEODIR)/.depend
	$(MAKE) BUILD_DEPENDS=FORCE

FORCE:

-include $(LIBODIR)/.depend
-include $(EXEODIR)/.depend
-include $(LIBODIR)/*.d
-include $(EXEODIR)/*.d
