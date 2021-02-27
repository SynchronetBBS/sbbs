# targets.mk

# Make 'include file' defining targets for Synchronet project

# $Id: targets.mk,v 1.54 2020/05/14 20:32:04 rswindell Exp $

# LIBODIR, EXEODIR, DIRSEP, LIBFILE, EXEFILE, and DELETE must be pre-defined

SBBS		= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)sbbs$(SOFILE)
FTPSRVR		= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)ftpsrvr$(SOFILE)
WEBSRVR		= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)websrvr$(SOFILE)
MAILSRVR	= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)mailsrvr$(SOFILE)
SERVICES	= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)services$(SOFILE)
SBBSCON		= $(EXEODIR)$(DIRSEP)sbbs$(EXEFILE)
SBBSMONO	= $(EXEODIR)$(DIRSEP)sbbsmono$(EXEFILE)
JSEXEC		= $(EXEODIR)$(DIRSEP)jsexec$(EXEFILE)
JSDOOR		= $(EXEODIR)$(DIRSEP)jsdoor$(EXEFILE)
NODE		= $(EXEODIR)$(DIRSEP)node$(EXEFILE)
BAJA		= $(EXEODIR)$(DIRSEP)baja$(EXEFILE)
UNBAJA		= $(EXEODIR)$(DIRSEP)unbaja$(EXEFILE)
FIXSMB		= $(EXEODIR)$(DIRSEP)fixsmb$(EXEFILE)
CHKSMB		= $(EXEODIR)$(DIRSEP)chksmb$(EXEFILE)
SMBUTIL		= $(EXEODIR)$(DIRSEP)smbutil$(EXEFILE)
SBBSECHO	= $(EXEODIR)$(DIRSEP)sbbsecho$(EXEFILE)
ECHOCFG		= $(EXEODIR)$(DIRSEP)echocfg$(EXEFILE)
ADDFILES	= $(EXEODIR)$(DIRSEP)addfiles$(EXEFILE)
FILELIST	= $(EXEODIR)$(DIRSEP)filelist$(EXEFILE)
MAKEUSER	= $(EXEODIR)$(DIRSEP)makeuser$(EXEFILE)
ANS2ASC		= $(EXEODIR)$(DIRSEP)ans2asc$(EXEFILE)
ASC2ANS		= $(EXEODIR)$(DIRSEP)asc2ans$(EXEFILE)
SEXYZ		= $(EXEODIR)$(DIRSEP)sexyz$(EXEFILE)
QWKNODES	= $(EXEODIR)$(DIRSEP)qwknodes$(EXEFILE)
SLOG		= $(EXEODIR)$(DIRSEP)slog$(EXEFILE)
ALLUSERS	= $(EXEODIR)$(DIRSEP)allusers$(EXEFILE)
DELFILES	= $(EXEODIR)$(DIRSEP)delfiles$(EXEFILE)
DUPEFIND	= $(EXEODIR)$(DIRSEP)dupefind$(EXEFILE)
SMBACTIV	= $(EXEODIR)$(DIRSEP)smbactiv$(EXEFILE)
DSTSEDIT	= $(EXEODIR)$(DIRSEP)dstsedit$(EXEFILE)
READSAUCE	= $(EXEODIR)$(DIRSEP)readsauce$(EXEFILE)
SHOWSTAT	= $(EXEODIR)$(DIRSEP)showstat$(EXEFILE)
PKTDUMP		= $(EXEODIR)$(DIRSEP)pktdump$(EXEFILE)
FMSGDUMP	= $(EXEODIR)$(DIRSEP)fmsgdump$(EXEFILE)

UTILS		= $(FIXSMB) $(CHKSMB) \
			  $(SMBUTIL) $(BAJA) $(NODE) \
			  $(SBBSECHO) $(ECHOCFG) \
			  $(ADDFILES) $(FILELIST) $(MAKEUSER) \
			  $(ANS2ASC) $(ASC2ANS)  $(UNBAJA) \
			  $(QWKNODES) $(SLOG) $(ALLUSERS) \
			  $(DELFILES) $(DUPEFIND) $(SMBACTIV) \
			  $(SEXYZ) $(DSTSEDIT) $(READSAUCE) $(SHOWSTAT) \
			  $(PKTDUMP) $(FMSGDUMP)

GIT_INFO	= git_hash.h git_branch.h

all:	$(GIT_INFO) dlls utils console scfg uedit umonitor

console:	$(JS_DEPS) xpdev-mt smblib \
		$(MTOBJODIR) $(LIBODIR) $(EXEODIR) \
		dlls \
		$(SBBSCON) $(JSEXEC)

utils:	smblib xpdev-mt xpdev ciolib-mt uifc-mt \
		$(LIBODIR) $(OBJODIR) $(MTOBJODIR) $(EXEODIR) \
		$(UTILS)

gtkutils: gtkmonitor gtkchat gtkuseredit gtkuserlist

dlls:	$(JS_DEPS) smblib xpdev-mt \
		$(MTOBJODIR) $(LIBODIR) \
		$(SBBS) $(FTPSRVR) $(MAILSRVR) $(SERVICES)

mono:	xpdev-mt smblib \
		$(MTOBJODIR) $(EXEODIR) \
		$(SBBSMONO)

.PHONY: scfg
scfg:
	$(MAKE) -C scfg $(MAKEFLAGS)

.PHONY: uedit
uedit: uifc-mt
	$(MAKE) -C uedit $(MAKEFLAGS)

.PHONY: umonitor
umonitor: uifc-mt
	$(MAKE) -C umonitor $(MAKEFLAGS)

.PHONY: gtkmonitor
gtkmonitor:
	$(MAKE) -C gtkmonitor $(MAKEFLAGS)

.PHONY: gtkchat
gtkchat:
	$(MAKE) -C gtkchat $(MAKEFLAGS)

.PHONY: gtkuseredit
gtkuseredit:
	$(MAKE) -C gtkuseredit $(MAKEFLAGS)

.PHONY: gtkuserlist
gtkuserlist:
	$(MAKE) -C gtkuserlist $(MAKEFLAGS)

ifdef SBBSEXEC
.PHONY: install
install: all
	install $(EXEODIR)/* $(SBBSEXEC)
	install $(LIBODIR)/* $(SBBSEXEC)
	install */$(EXEODIR)/* $(SBBSEXEC)

.PHONY: symlinks
symlinks: all
	ln -sfr $(EXEODIR)/* $(SBBSEXEC)
	ln -sfr $(LIBODIR)/* $(SBBSEXEC)
	ln -sfr */$(EXEODIR)/* $(SBBSEXEC)
endif

.PHONY: FORCE
FORCE:

ifneq ($(GIT), NO)
git_hash.h: FORCE ../../.git
	$(QUIET)echo '#define GIT_HASH "'`git log -1 HEAD --format=%h`\" > $@.tmp
	$(QUIET)diff $@.tmp $@ || cp $@.tmp $@
	$(QUIET)rm -f $@.tmp

git_branch.h: FORCE ../../.git
	$(QUIET)echo '#define GIT_BRANCH "'`git rev-parse --abbrev-ref HEAD`\" > $@.tmp
	$(QUIET)diff $@.tmp $@ || cp $@.tmp $@
	$(QUIET)rm -f $@.tmp
endif

ifeq ($(os),linux)
.PHONY: setcap
setcap: all
	sudo setcap 'cap_net_bind_service=+ep' $(EXEODIR)/sbbs
endif

.PHONY: sexyz
sexyz:	$(SEXYZ)

.PHONY: jsdoor
jsdoor: $(GIT_INFO) $(JS_DEPS) $(CRYPT_DEPS) $(XPDEV-MT_LIB) $(SMBLIB) $(UIFCLIB-MT) $(CIOLIB-MT) $(JSDOOR)

# Library dependencies
$(SBBS):
$(FTPSRVR): 
$(WEBSRVR):
$(MAILSRVR):
$(SERVICES): 
$(SBBSCON): $(XPDEV-MT_LIB) $(SMBLIB)
$(SBBSMONO): $(XPDEV-MT_LIB) $(SMBLIB)
$(JSEXEC): $(XPDEV-MT_LIB) $(SMBLIB)
$(JSDOOR): $(XPDEV-MT_LIB)
$(NODE): $(XPDEV_LIB)
$(BAJA): $(XPDEV_LIB) $(SMBLIB)
$(UNBAJA): $(XPDEV_LIB)
$(FIXSMB): $(XPDEV_LIB) $(SMBLIB)
$(CHKSMB): $(XPDEV_LIB) $(SMBLIB)
$(SMBUTIL): $(XPDEV_LIB) $(SMBLIB)
$(SBBSECHO): $(XPDEV_LIB) $(SMBLIB)
$(ECHOCFG): $(XPDEV-MT_LIB) $(SMBLIB) $(UIFCLIB-MT) $(CIOLIB-MT)
$(ADDFILES): $(XPDEV_LIB)
$(FILELIST): $(XPDEV_LIB)
$(MAKEUSER): $(XPDEV_LIB)
$(ANS2ASC):
$(ASC2ANS):
$(SEXYZ): $(XPDEV-MT_LIB) $(SMBLIB)
$(QWKNODES): $(XPDEV_LIB)
$(SLOG): $(XPDEV_LIB)
$(ALLUSERS): $(XPDEV_LIB)
$(DELFILES): $(XPDEV_LIB)
$(DUPEFIND): $(XPDEV_LIB) $(SMBLIB)
$(SMBACTIV): $(XPDEV_LIB) $(SMBLIB)
$(DSTSEDIT): $(XPDEV_LIB)
$(READSAUCE): $(XPDEV_LIB)
$(SHOWSTAT): $(XPDEV_LIB)
