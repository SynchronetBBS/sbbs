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

#USE_DIALOG =   1       # Dialog vesrion of UIFC
#USE_FLTK =     1       # Use Windowed version
#USE_CURSES =	1	# Use *nix curses version
ifndef NO_CURSES
 USE_UIFC32      =       1       # Curses version of UIFC
endif

UIFC_SRC =	../uifc/
XPDEV	 =	../xpdev/
SBBS_SRC =	./

NEED_JAVASCRIPT	:= 1
NEED_THREADS	:= 1

include $(XPDEV)/Common.gmake
include $(SBBS_SRC)/Common.gmake
include $(UIFC_SRC)/Common.gmake

ifeq ($(os),freebsd)
 BSD	=	1
else
 ifeq ($(os),openbsd)
  BSD	=	1
 endif
endif

CFLAGS  +=  -I$(XPDEV)
CFLAGS	+=  $(UIFC_CFLAGS)

ifndef bcc
 LDFLAGS	+=	-lm
 ifneq ($(os),sunos)
  LDFLAGS	+=	-lutil
 endif
endif

ifeq ($(os),sunos)    # Solaris
 LDFLAGS += -lsocket -lnsl -lrt
endif

ifeq ($(os),netbsd)
 LDFLAGS += -L/usr/pkg/lib
 UTIL_LDFLAGS	+=	-lpth -L/usr/pkg/lib
endif

# So far, only QNX has sem_timedwait()
ifeq ($(os),qnx)
 LDFLAGS += -lsocket
endif

ifdef PREFIX
 CFLAGS += -DPREFIX=$(PREFIX)
endif

ifdef USE_DOSEMU
 CFLAGS += -DUSE_DOSEMU
endif

ifdef DONT_BLAME_SYNCHRONET
 CFLAGS += -DDONT_BLAME_SYNCHRONET
endif

#SBBSLIB	=	$(LIBODIR)$(SLASH)libsbbs.so
SBBSLIB	=	-lsbbs

#dummy rule
$(SBBSLIB) : $(SBBS)
	$(QUIET)touch -- '$(SBBSLIB)'

ifneq ($(os),darwin)
ifneq ($(os),sunos)
SBBSLDFLAGS	:=	$(LDFLAGS) -rpath-link ./$(LIBODIR) -rpath ./ 
#LDFLAGS		+=	-Wl,-rpath-link,./$(LIBODIR),-rpath,./
LDFLAGS		+=	-Xlinker -rpath
LDFLAGS		+=	-Xlinker .
ifneq ($(os),openbsd)
LDFLAGS		+=	-Xlinker -rpath-link
LDFLAGS		+=	-Xlinker ./$(LIBODIR)
LDFLAGS		+=	-Xlinker -rpath-link
LDFLAGS		+=	-Xlinker $(JSLIBDIR)
LDFLAGS		+=	-Xlinker -rpath-link
LDFLAGS		+=	-Xlinker $(NSPRDIR)
endif
endif
endif

CON_OBJS	= $(LIBODIR)/sbbscon.o $(LIBODIR)/conwrap.o \
		  $(LIBODIR)/sbbs_ini.o
CON_LDFLAGS	= -lftpsrvr -lwebsrvr -lmailsrvr -lservices
FTP_OBJS	= $(LIBODIR)/ftpsrvr.o
MAIL_OBJS	= $(LIBODIR)/mailsrvr.o $(LIBODIR)/mxlookup.o \
 		  $(LIBODIR)/mime.o $(LIBODIR)/base64.o $(LIBODIR)/ini_file.o \
		  $(LIBODIR)/str_list.o
WEB_OBJS	= $(LIBODIR)/websrvr.o $(LIBODIR)/sockwrap.o $(LIBODIR)/base64.o
SERVICE_OBJS	= $(LIBODIR)/services.o $(LIBODIR)/ini_file.o $(LIBODIR)/str_list.o

MONO_OBJS	= $(CON_OBJS) $(FTP_OBJS) $(WEB_OBJS) \
			$(MAIL_OBJS) $(SERVICE_OBJS)
SMBLIB_OBJS = \
	$(LIBODIR)/smblib.o \
	$(LIBODIR)/smbtxt.o \
	$(LIBODIR)/smbdump.o \
	$(LIBODIR)/crc16.o \
	$(LIBODIR)/crc32.o \
	$(LIBODIR)/md5.o \
	$(LIBODIR)/lzh.o \
	$(LIBODIR)/filewrap.o

SHLIBOPTS	:=	-shared
ifeq ($(os),darwin)
 MKSHLIB		:=	libtool -dynamic -framework System -lcc_dynamic
 MKSHPPLIB		:=	libtool -dynamic -framework System -lcc_dynamic -lstdc++
 SHLIBOPTS	:=	
else
 ifeq ($(os),sunos)
  MKSHLIB		:=	/usr/ccs/bin/ld -G
  MKSHPPLIB		:=	/usr/ccs/bin/ld -G -L/usr/local/lib -lstdc++
  SHLIBOPTS	:=	
 else
  MKSHLIB		:=	$(CC)
  MKSHPPLIB		:=	$(CXX)
 endif
endif

# Monolithic Synchronet executable Build Rule
FORCE$(SBBSMONO): $(MONO_OBJS) $(OBJS) $(LIBS)

$(SBBSMONO): $(MONO_OBJS) $(OBJS) $(LIBS)
	@echo Linking $@
	$(QUIET)$(CXX) -o $@ $(LDFLAGS) $^

# Synchronet BBS library Link Rule
FORCE$(SBBS): $(OBJS) $(LIBS)

$(SBBS): $(OBJS) $(LIBS)
	@echo Linking $@
	$(QUIET)$(MKSHPPLIB) $(LDFLAGS) -o $@ $^ $(SHLIBOPTS)

# FTP Server Link Rule
FORCE$(FTPSRVR): $(LIBODIR)/ftpsrvr.o $(SBBSLIB)

$(FTPSRVR): $(LIBODIR)/ftpsrvr.o $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $^ $(SHLIBOPTS) -o $@ 

# Mail Server Link Rule
FORCE$(MAILSRVR): $(MAIL_OBJS) $(LIBODIR)$(SLASH)$(SBBSLIB)

$(MAILSRVR): $(MAIL_OBJS) $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $^ $(SHLIBOPTS) -o $@

# Mail Server Link Rule
FORCE$(WEBSRVR): $(WEB_OBJS) $(SBBSLIB)

$(WEBSRVR): $(WEB_OBJS) $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $^ $(SHLIBOPTS) -o $@

# Services Link Rule
FORCE$(SERVICES): $(WEB_OBJS) $(SBBSLIB)

$(SERVICES): $(SERVICE_OBJS) $(SBBSLIB)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $^ $(SHLIBOPTS) -o $@

# Synchronet Console Build Rule
FORCE$(SBBSCON): $(CON_OBJS) $(SBBSLIB) $(FTP_OBJS) $(MAIL_OBJS) $(WEB_OBJS) $(SERVICE_OBJS)

$(SBBSCON): $(CON_OBJS) $(SBBSLIB) $(FTPSRVR) $(WEBSRVR) $(MAILSRVR) $(SERVICES)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) $(LDFLAGS) $(CON_LDFLAGS) -o $@ $(CON_OBJS) $(SBBSLIB)

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
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/filewrap.o
FORCE$(BAJA): $(BAJA_OBJS)

$(BAJA): $(BAJA_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^

# Node Utility
NODE_OBJS = \
	$(LIBODIR)/node.o \
	$(LIBODIR)/genwrap.o \
	$(LIBODIR)/filewrap.o
FORCE$(NODE): $(NODE_OBJS)

$(NODE): $(NODE_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^ 

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
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^

# CHKSMB Utility
CHKSMB_OBJS = \
	$(LIBODIR)/chksmb.o \
	$(SMBLIB_OBJS) \
	$(LIBODIR)/smbdump.o \
	$(LIBODIR)/conwrap.o \
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/genwrap.o
FORCE$(CHKSMB): $(CHKSMB_OBJS)

$(CHKSMB): $(CHKSMB_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^

# SMB Utility
SMBUTIL_OBJS = \
	$(LIBODIR)/smbutil.o \
	$(SMBLIB_OBJS) \
	$(LIBODIR)/conwrap.o \
	$(LIBODIR)/dirwrap.o \
	$(LIBODIR)/genwrap.o \
	$(LIBODIR)/date_str.o \
	$(LIBODIR)/str_util.o
FORCE$(SMBUTIL): $(SMBUTIL_OBJS)
	
$(SMBUTIL): $(SMBUTIL_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^

# SBBSecho (FidoNet Packet Tosser)
SBBSECHO_OBJS = \
	$(LIBODIR)/sbbsecho.o \
	$(LIBODIR)/ars.o \
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
	$(SMBLIB_OBJS)

FORCE$(SBBSECHO): $(SBBSECHO_OBJS)

$(SBBSECHO): $(SBBSECHO_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^

# SBBSecho Configuration Program
ECHOCFG_OBJS = \
	$(LIBODIR)/echocfg.o \
	$(LIBODIR)/rechocfg.o \
	$(UIFC_OBJS) \
	$(LIBODIR)/uifcx.o \
	$(LIBODIR)/nopen.o \
	$(LIBODIR)/crc16.o \
	$(LIBODIR)/str_util.o \
	$(LIBODIR)/filewrap.o \
	$(LIBODIR)/genwrap.o \
	$(LIBODIR)/dirwrap.o

FORCE$(ECHOCFG): $(ECHOCFG_OBJS)

$(ECHOCFG): $(ECHOCFG_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^ $(UIFC_LDFLAGS)

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
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^

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
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^

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
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^

# JSEXEC
JSEXEC_OBJS = \
	$(LIBODIR)/jsexec.o \
	$(SBBSLIB)

FORCE$(JSEXEC): $(JSEXEC_OBJS)

$(JSEXEC): $(JSEXEC_OBJS)
	@echo Linking $@
	$(QUIET)$(CXX) $(UTIL_LDFLAGS) -o $@ $^ $(LDFLAGS)
	
# ANS2ASC
FORCE$(ANS2ASC): $(LIBODIR)/ans2asc.o

$(ANS2ASC): $(LIBODIR)/ans2asc.o
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^

# ASC2ANS
FORCE$(ASC2ANS): $(LIBODIR)/asc2ans.o

$(ASC2ANS): $(LIBODIR)/asc2ans.o
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^

FORCE:
