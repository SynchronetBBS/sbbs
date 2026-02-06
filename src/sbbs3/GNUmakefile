# GNUmakefile

#########################################################################
# Makefile for Synchronet BBS for Unix									#
# For use with GNU make and GNU C Compiler or Borland Kylix C++			#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# gcc: gmake															#
# Borland (still in testing/debuging stage): gmake bcc=1				#
#																		#
# Optional build targets: dlls, utils, all (default)					#
#########################################################################

PWD	:=	$(shell /bin/pwd)
SRC_ROOT	?=	${PWD}/..
include $(SRC_ROOT)/build/Common.gmake
include extdeps.mk

help:
	@echo "Synchronet build targets (multiple may be specified):"
	@echo " 'all'         Build all libraries, utilities, and sbbs (the default)"
	@echo " 'gitinfo'     Build GIT branch and hash information (header) files only"
	@echo " 'utils'       Build console/TUI sysop utilities"
	@echo " 'gtkutils'    Build graphical (GTK-based) sysop utilities"
ifeq ($(os), linux)
	@echo " 'setcap'      Set port bind capabilities (on Linux only, or use systemd)"
endif
	@echo
	@echo "Additional/sub-build targets (multiple may be specified):"
	@echo " 'sexyz', 'scfg', 'uedit', 'umonitor', 'jsdoor'"
	@echo
	@echo "Synchronet install/update targets (choose one):"
	@echo " 'symlinks'    Update symbolic links in sbbs/exec directory (recommended)"
	@echo " 'install'     Copy output executable/library files to sbbs/exec directory"
	@echo " (these targets require either SBBSCTRL or SBBSEXEC environment variable set)"
	@echo
	@echo "Clean-up targets (beware use with symlinked exec/* files)"
	@echo " 'clean'       Delete output files (see also ../cleanall.sh)"
	@echo
	@echo "Options (may be set persistently in ../build/localdefs.mk):"
	@echo " 'GIT=NO'      Don't require Git repository to (re)build Git info .h files"
	@echo " 'DEBUG=1'     Build output files with debug information"
	@echo " 'RELEASE=1'   Build output files withOUT debug information"
	@echo " 'JSLIB=x'     Specify an alternate path to libmozjs[185].so"
	@echo " 'JSINCLUDE=x' Specify an alternate Mozilla JS library header path"

UTIL_LDFLAGS	:=	$(LDFLAGS)
UTIL_LDFLAGS	+=	$(SMBLIB_LDFLAGS) $(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV_LDFLAGS) $(ENCODE_LDFLAGS)
CONSOLE_LDFLAGS	+=	$(LDFLAGS) $(SMBLIB_LDFLAGS) $(XPDEV_LDFLAGS)
UTIL_LIBS	+=	$(HASH_LIBS)

ifndef win
 ifndef bcc
  ifneq ($(os),sunos)
   LDFLAGS	+=	-lutil
  endif
 endif
endif

ifeq ($(os),sunos)    # Solaris
 LDFLAGS += -lnsl -lrt
 SBBS_LIBS	+=	-lkstat
endif

# So far, only QNX has sem_timedwait()
ifeq ($(os),qnx)
 LDFLAGS += -lsocket
endif

ifdef PREFIX
 CFLAGS += -DPREFIX=$(PREFIX)
endif

ifdef DONT_BLAME_SYNCHRONET
 CFLAGS += -DDONT_BLAME_SYNCHRONET
endif

ifdef BUILD_JSDOCS
 CFLAGS += -DBUILD_JSDOCS
 SBBS_LIBS += ${UIFC-MT_LDFLAGS} ${UIFC-MT_LIBS} $(CIOLIB-MT_LDFLAGS) ${CIOLIB-MT_LIBS}
 OBJS += $(MTOBJODIR)/js_uifc$(OFILE) \
			$(MTOBJODIR)/js_conio$(OFILE)
 EXTRA_SBBS_DEPENDS += $(CIOLIB-MT) $(UIFCLIB-MT)
endif

ifeq ($(os),linux)
 SBBS_LIBS += -lutil
 ifeq ($(shell test -f /usr/include/sys/capability.h && echo "yes"),yes)
  CFLAGS += -DUSE_LINUX_CAPS
  CON_LIBS += -lcap
 endif
 ifeq ($(shell test -f /usr/include/systemd/sd-daemon.h && echo "yes"),yes)
  CFLAGS += -DUSE_SYSTEMD
  CON_LIBS += -lsystemd
 endif
endif
ifndef WITHOUT_MOSQUITTO
 ifneq ($(os), win32)
  ifeq ($(shell pkg-config 'libmosquitto >= 1.6.9' --exists && echo "yes"),yes)
   CFLAGS += -DUSE_MOSQUITTO
   CFLAGS += $(shell pkg-config libmosquitto --cflags)
   CON_LIBS += $(shell pkg-config libmosquitto --libs)
   JS_LIBS += $(shell pkg-config libmosquitto --libs)
  endif
 endif
endif
ifeq ($(shell pkg-config libarchive --exists && echo "yes"),yes)
 CFLAGS += $(shell pkg-config libarchive --cflags)
 FILE_LIBS	= $(shell pkg-config libarchive --libs)
else
 ifeq ($(os), darwin)
  ifeq ($(shell test -d /opt/homebrew/opt/libarchive/include && echo "yes"),yes)
    CFLAGS += -I/opt/homebrew/opt/libarchive/include -I/opt/homebrew/include
    LDFLAGS += -L/opt/homebrew/opt/libarchive/lib -I/opt/homebrew/lib
  else
    ifeq ($(shell test -d /usr/local/opt/libarchive/include && echo "yes"),yes)
      CFLAGS += -I/usr/local/opt/libarchive/include -I/opt/local/include
      LDFLAGS += -L/usr/local/opt/libarchive/lib -I/opt/local/lib
    endif
  endif
 endif
 FILE_LIBS	= -larchive
endif

include sbbsdefs.mk
MT_CFLAGS	+=	$(SBBSDEFS) $(SFTP-MT_CFLAGS)

ifneq ($(os),darwin)
 ifndef NO_ABSOLUTE_RPATH
  ifdef SBBSDIR
   LIB_RPATH += -Wl,-rpath,$(SBBSDIR)/exec
  endif
  LIB_RPATH += -Wl,-rpath,$(PWD)/$(LIBODIR)
 endif
 LIB_RPATH += '-Wl,-rpath,$$ORIGIN,-rpath,$$ORIGIN/../$(LIBODIR),-z,origin,--enable-new-dtags'
endif

CON_LIBS	+= -lsbbs -lftpsrvr -lwebsrvr -lmailsrvr -lservices
ifeq ($(os),sunos)
 MKSHLIB		:=	/usr/ccs/bin/ld -G
 MKSHPPLIB		:=	/usr/ccs/bin/ld -G
 SHLIBOPTS	:=
else
 ifeq ($(os),darwin)
  SHLIBOPTS	=	-shared -install_name @rpath/`VAR=$@; echo $${VAR\#\#*/}`
 else
  SHLIBOPTS	:=	-shared
 endif
 MKSHLIB		:=	$(CC)
 MKSHPPLIB		:=	$(CXX)
endif
ifeq ($(os),darwin)
 LIB_RPATH := -Wl,$(RPATH),@executable_path,$(RPATH),@executable_path/../$(LIBODIR)
endif

CFLAGS	+=	$(JS_CFLAGS) $(CRYPT_CFLAGS) $(UIFC-MT_CFLAGS) $(XPDEV-MT_CFLAGS) $(SMBLIB_CFLAGS) $(CIOLIB-MT_CFLAGS) $(ENCODE_CFLAGS) $(HASH_CFLAGS)
CFLAGS	+=	-I../comio
vpath %.c ../comio
ifeq ($(os), win32)
OBJS	+= $(MTOBJODIR)/comio_win32$(OFILE)
JSDOOR_OBJS	+= $(MTOBJODIR)/comio_win32$(OFILE)
else
OBJS	+= $(MTOBJODIR)/comio_nix$(OFILE)
JSDOOR_OBJS	+= $(MTOBJODIR)/comio_nix$(OFILE)
endif
LDFLAGS +=	$(UIFC-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(SMBLIB_LDFLAGS) $(ENCODE_LDFLAGS) $(HASH_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(JS_LDFLAGS) $(CRYPT_LDFLAGS) $(SFTP-MT_LDFLAGS)
ifneq ($(os),darwin)
 SBBS_EXTRA_LDFLAGS += -Wl,--version-script=sbbs.version
endif

# Synchronet BBS library Link Rule
$(SBBS): $(JS_DEPS) $(CRYPT_DEPS) $(OBJS) $(LIBS) $(EXTRA_SBBS_DEPENDS) $(ENCODE_LIB) $(HASH_LIB) $(SFTPLIB-MT) | $(LIBODIR)
	@echo Linking $@
	$(QUIET)$(MKSHPPLIB) $(LDFLAGS) -o $@ $(OBJS) $(SBBS_LIBS) $(SMBLIB_LIBS) $(LIBS) $(SHLIBOPTS) $(JS_LIBS) $(CRYPT_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(XPDEV-MT_LIBS) $(FILE_LIBS) $(SFTP-MT_LIBS) $(SBBS_EXTRA_LDFLAGS)
ifeq ($(os), netbsd)
	paxctl +m $(SBBS)
endif

# FTP Server Link Rule
$(FTPSRVR): $(MTOBJODIR)/ftpsrvr.o
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(MTOBJODIR)/ftpsrvr.o $(SHLIBOPTS) $(LIB_RPATH) -o $@ -lsbbs

# Mail Server Link Rule
$(MAILSRVR): $(MAIL_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHPPLIB) $(LDFLAGS) $(MAIL_OBJS) $(SHLIBOPTS) $(LIB_RPATH) -o $@ -lsbbs

# Web Server Link Rule
$(WEBSRVR): $(WEB_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHPPLIB) $(LDFLAGS) $(WEB_OBJS) $(SHLIBOPTS) $(LIB_RPATH) -o $@ $(ENCODE_LIBS) -lsbbs $(XPDEV-MT_LIBS)

# Services Link Rule
$(SERVICES): $(SERVICE_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHPPLIB) $(LDFLAGS) $(SERVICE_OBJS) $(SHLIBOPTS) $(LIB_RPATH) -o $@ -lsbbs

# Synchronet Console Build Rule
$(SBBSCON): $(CON_OBJS) $(SBBS) $(FTPSRVR) $(WEBSRVR) $(MAILSRVR) $(SERVICES)
	@echo Linking $@
	$(QUIET)$(CXX) $(LDFLAGS) $(MT_LDFLAGS) $(LIB_RPATH) -o $@ $(CON_OBJS) $(CON_LIBS) $(SMBLIB_LIBS) $(XPDEV-MT_LIBS)

# Baja Utility
$(BAJA): $(BAJA_OBJS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(BAJA_OBJS) $(XPDEV_LIBS) $(HASH_LIBS)

# UnBaja Utility
$(UNBAJA): $(UNBAJA_OBJS) | $(OBJODIR) $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(UNBAJA_OBJS) $(XPDEV_LIBS) $(UTIL_LIBS)

# Node Utility
$(NODE): $(NODE_OBJS) $(ENCODE_LIB) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(NODE_OBJS) $(XPDEV_LIBS) $(ENCODE_LIBS)

# FIXSMB Utility
$(FIXSMB): $(FIXSMB_OBJS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(FIXSMB_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS) $(ENCODE_LIBS) $(HASH_LIBS)

# CHKSMB Utility
$(CHKSMB): $(CHKSMB_OBJS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(CHKSMB_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS) $(ENCODE_LIBS) $(HASH_LIBS)

# SMB Utility
$(SMBUTIL): $(SMBUTIL_OBJS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(SMBUTIL_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS) $(ENCODE_LIBS) $(HASH_LIBS)

# SBBSecho (FidoNet Packet Tosser)
$(SBBSECHO): $(SBBSECHO_OBJS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(SBBSECHO_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(FILE_LIBS)

# SBBSecho Configuration Program
$(ECHOCFG): $(ECHOCFG_OBJS) $(ENCODE_LIB) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) $(MT_LDFLAGS) -o $@ $(ECHOCFG_OBJS) $(UIFC-MT_LDFLAGS) $(SMBLIB_LIBS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS) $(ENCODE_LIBS)

# ADDFILES
$(ADDFILES): $(ADDFILES_OBJS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(ADDFILES_OBJS) $(XPDEV_LIBS) $(SMBLIB_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(FILE_LIBS)

# FILELIST
$(FILELIST): $(FILELIST_OBJS) $(ENCODE_LIB) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(FILELIST_OBJS) $(XPDEV_LIBS) $(SMBLIB_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(FILE_LIBS)

# MAKEUSER
$(MAKEUSER): $(MAKEUSER_OBJS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(MAKEUSER_OBJS) $(XPDEV_LIBS) $(SMBLIB_LIBS) $(ENCODE_LIBS) $(HASH_LIBS)

# JSDOOR
$(JSDOOR): $(JSDOOR_OBJS) $(XPDEV_LIB) $(ENCODE_LIB) $(HASH_LIB) $(CIOLIB-MT) | $(EXEODIR)
	@echo Linking $@
	echo CIOLIB: $(CIOLIB-MT_LDFLAGS)
	echo XPDEV: $(XPDEV-MT_LDFLAGS)
	$(QUIET)$(CXX) $(JS_CFLAGS) $(LDFLAGS) $(MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) -o $@ $(JSDOOR_OBJS) $(JS_LIBS) $(CRYPT_LIBS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(SMBLIB_LIBS) $(ENCODE_LIBS) $(XPDEV-MT_LIBS) $(HASH_LIBS) $(FILE_LIBS)
ifeq ($(os), netbsd)
	paxctl +m $(JSDOOR)
endif

# JSEXEC
$(JSEXEC): $(JSEXEC_OBJS) $(SBBS) $(CIOLIB-MT) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CXX) $(LDFLAGS) $(MT_LDFLAGS) $(LIB_RPATH) -o $@ $(JSEXEC_OBJS) -lsbbs $(JS_LIBS) $(SMBLIB_LIBS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)
ifeq ($(os), netbsd)
	paxctl +m $(JSEXEC)
endif

# ANS2ASC
$(ANS2ASC): $(OBJODIR)/ans2asc.o $(OBJODIR)/sauce.o $(XPDEV_LIB) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $^ -lm $(XPDEV_LIBS)

# ASC2ANS
$(ASC2ANS): $(OBJODIR)/asc2ans.o | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(OBJODIR)/asc2ans.o

# SEXYZ
$(SEXYZ): $(SEXYZ_OBJS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) $(MT_LDFLAGS) -o $@ $(SEXYZ_OBJS) $(HASH_LIBS) $(XPDEV-MT_LIBS)

# QWKNODES
$(QWKNODES): $(QWKNODES_OBJS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(QWKNODES_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS) $(ENCODE_LIBS) $(HASH_LIBS)

# SLOG
$(SLOG): $(SLOG_OBJS) $(SBBS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) $(LIB_RPATH) -o $@ $(SLOG_OBJS) $(XPDEV_LIBS) -lsbbs

# DELFILES
$(DELFILES): $(DELFILES_OBJS) $(ENCODE_LIB) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(DELFILES_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(FILE_LIBS)

# DUPEFIND
$(DUPEFIND): $(DUPEFIND_OBJS) $(ENCODE_LIB) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(DUPEFIND_OBJS) $(SMBLIB_LIBS) $(HASH_LIBS) $(XPDEV_LIBS) $(ENCODE_LIBS)

# READSAUCE
$(READSAUCE): $(READSAUCE_OBJS) | $(EXEODIR) $(OBJODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(READSAUCE_OBJS) $(XPDEV_LIBS)

# PKTDUMP
$(PKTDUMP): $(PKTDUMP_OBJS) | $(OBJODIR) $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(PKTDUMP_OBJS) $(XPDEV_LIBS)

# FMSGDUMP
$(FMSGDUMP): $(FMSGDUMP_OBJS) | $(EXEODIR) $(OBJODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(FMSGDUMP_OBJS)

# TRASHMAN
$(TRASHMAN): $(TRASHMAN_OBJS) | $(EXEODIR) $(OBJODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $^ $(XPDEV_LIBS)

$(UPGRADE_TO_V319): $(UPGRADE_TO_V319_OBJS) $(OBJODIR) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(UPGRADE_TO_V319_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(FILE_LIBS)

$(UPGRADE_TO_V320): $(UPGRADE_TO_V320_OBJS) $(OBJODIR) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CONSOLE_LDFLAGS) -o $@ $(UPGRADE_TO_V320_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS) $(ENCODE_LIBS) $(HASH_LIBS)

ifdef SBBS_OFFICIAL
ifdef SBBSCTRL
$(EXEODIR)/textgen: $(TEXTGEN_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $(TEXTGEN_OBJS) $(XPDEV_LIB) $(ENCODE_LIB) -lm

text.h: $(SBBSCTRL)/text.dat $(EXEODIR)/textgen
	$(EXEODIR)/textgen
endif
endif
