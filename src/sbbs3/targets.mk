# targets.mk

# Make 'include file' defining targets for Synchronet project

# LIBODIR, EXEODIR, LIBFILE, EXEFILE, and DELETE must be pre-defined

SBBS		= $(LIBODIR)/$(LIBPREFIX)sbbs$(SOFILE)
FTPSRVR		= $(LIBODIR)/$(LIBPREFIX)ftpsrvr$(SOFILE)
WEBSRVR		= $(LIBODIR)/$(LIBPREFIX)websrvr$(SOFILE)
MAILSRVR	= $(LIBODIR)/$(LIBPREFIX)mailsrvr$(SOFILE)
SERVICES	= $(LIBODIR)/$(LIBPREFIX)services$(SOFILE)
SBBSCON		= $(EXEODIR)/sbbs$(EXEFILE)
SBBSMONO	= $(EXEODIR)/sbbsmono$(EXEFILE)
JSEXEC		= $(EXEODIR)/jsexec$(EXEFILE)
JSDOOR		= $(EXEODIR)/jsdoor$(EXEFILE)
NODE		= $(EXEODIR)/node$(EXEFILE)
BAJA		= $(EXEODIR)/baja$(EXEFILE)
UNBAJA		= $(EXEODIR)/unbaja$(EXEFILE)
FIXSMB		= $(EXEODIR)/fixsmb$(EXEFILE)
CHKSMB		= $(EXEODIR)/chksmb$(EXEFILE)
SMBUTIL		= $(EXEODIR)/smbutil$(EXEFILE)
SBBSECHO	= $(EXEODIR)/sbbsecho$(EXEFILE)
ECHOCFG		= $(EXEODIR)/echocfg$(EXEFILE)
ADDFILES	= $(EXEODIR)/addfiles$(EXEFILE)
FILELIST	= $(EXEODIR)/filelist$(EXEFILE)
MAKEUSER	= $(EXEODIR)/makeuser$(EXEFILE)
ANS2ASC		= $(EXEODIR)/ans2asc$(EXEFILE)
ASC2ANS		= $(EXEODIR)/asc2ans$(EXEFILE)
SEXYZ		= $(EXEODIR)/sexyz$(EXEFILE)
QWKNODES	= $(EXEODIR)/qwknodes$(EXEFILE)
SLOG		= $(EXEODIR)/slog$(EXEFILE)
DELFILES	= $(EXEODIR)/delfiles$(EXEFILE)
DUPEFIND	= $(EXEODIR)/dupefind$(EXEFILE)
READSAUCE	= $(EXEODIR)/readsauce$(EXEFILE)
PKTDUMP		= $(EXEODIR)/pktdump$(EXEFILE)
FMSGDUMP	= $(EXEODIR)/fmsgdump$(EXEFILE)
UPGRADE_TO_V319 = $(EXEODIR)/upgrade_to_v319$(EXEFILE)
UPGRADE_TO_V320 = $(EXEODIR)/upgrade_to_v320$(EXEFILE)

UTILS		= $(FIXSMB) $(CHKSMB) \
			  $(SMBUTIL) $(BAJA) $(NODE) \
			  $(SBBSECHO) $(ECHOCFG) \
			  $(ADDFILES) $(FILELIST) $(MAKEUSER) \
			  $(ANS2ASC) $(ASC2ANS)  $(UNBAJA) \
			  $(QWKNODES) $(SLOG) \
			  $(DELFILES) $(DUPEFIND) \
			  $(SEXYZ) $(READSAUCE) \
			  $(PKTDUMP) $(FMSGDUMP) $(UPGRADE_TO_V319) \
			  $(UPGRADE_TO_V320)

GIT_INFO	= git_hash.h git_branch.h

all:	$(GIT_INFO) dlls utils console scfg uedit umonitor

gitinfo:	$(GIT_INFO)

console:	$(JS_DEPS) xpdev-mt smblib \
		$(MTOBJODIR) $(LIBODIR) $(EXEODIR) \
		dlls \
		$(SBBSCON) $(JSEXEC)

utils:	smblib xpdev-mt xpdev ciolib-mt uifc-mt \
		$(LIBODIR) $(OBJODIR) $(MTOBJODIR) $(EXEODIR) \
		$(UTILS)

gtkutils: gtkmonitor gtkchat gtkuseredit gtkuserlist

.PHONY: standalone-utils
standalone-utils: $(FIXSMB) $(CHKSMB) \
			  $(SMBUTIL) $(BAJA) $(NODE) \
			  $(SBBSECHO) $(ECHOCFG) \
			  $(ADDFILES) $(FILELIST) $(MAKEUSER) \
			  $(ANS2ASC) $(ASC2ANS)  $(UNBAJA) \
			  $(QWKNODES) \
			  $(DELFILES) $(DUPEFIND) \
			  $(SEXYZ) $(READSAUCE) \
			  $(PKTDUMP) $(FMSGDUMP)

.PHONY: libdeps
libdeps: $(JS_DEPS) gitinfo smblib xpdev-mt $(MTOBJODIR) $(LIBODIR)

.PHONY: libsbbs
libsbbs: libdeps $(SBBS)

.PHONY: libftpsrvr
libftpsrvr: libdeps $(FTPSRVR)

.PHONY: libmailsrvr
libmailsrvr: libdeps $(MAILSRVR)

.PHONY: libservices
libservices: libdeps $(SERVICES)

.PHONY: libwebsrvr
libwebsrvr: libdeps $(WEBSRVR)

dlls:	libsbbs libftpsrvr libmailsrvr libservices libwebsrvr

mono:	xpdev-mt smblib \
		$(MTOBJODIR) $(EXEODIR) \
		$(SBBSMONO)

.PHONY: scfg
scfg:
	$(MAKE) -C scfg

.PHONY: uedit
uedit: uifc-mt
	$(MAKE) -C uedit

.PHONY: umonitor
umonitor: uifc-mt
	$(MAKE) -C umonitor

.PHONY: gtkmonitor
gtkmonitor:
	$(MAKE) -C gtkmonitor

.PHONY: gtkchat
gtkchat:
	$(MAKE) -C gtkchat

.PHONY: gtkuseredit
gtkuseredit:
	$(MAKE) -C gtkuseredit

.PHONY: gtkuserlist
gtkuserlist:
	$(MAKE) -C gtkuserlist

ifdef SBBSEXEC
.PHONY: install
install: all
	install $(EXEODIR)/* $(SBBSEXEC)
	install $(LIBODIR)/* $(SBBSEXEC)
	install */$(EXEODIR)/* $(SBBSEXEC)

.PHONY: symlinks
symlinks: all
	ln -sf `realpath $(EXEODIR)/*` $(SBBSEXEC)
	ln -sf `realpath $(LIBODIR)/*` $(SBBSEXEC)
	ln -sf `realpath */$(EXEODIR)/*` $(SBBSEXEC)
endif

.PHONY: FORCE
FORCE:

ifneq ($(GIT), NO)
git_hash.h: FORCE ../../.git
	$(QUIET)git log -1 HEAD --format="#define GIT_HASH \"%h\"" > $@.tmp
	$(QUIET)git log -1 HEAD --format="#define GIT_DATE \"%cd\"" '--date=format-local:%b %d %Y %H:%M' >> $@.tmp
	$(QUIET)git log -1 HEAD --format="#define GIT_TIME %cd" --date=unix >> $@.tmp
	$(QUIET)test -e $@ && diff $@.tmp $@ || cp $@.tmp $@
	$(QUIET)rm -f $@.tmp

git_branch.h: FORCE ../../.git
	$(QUIET)echo '#define GIT_BRANCH "'`git rev-parse --abbrev-ref HEAD`\" > $@.tmp
	$(QUIET)test -e $@ && diff $@.tmp $@ || cp $@.tmp $@
	$(QUIET)rm -f $@.tmp
endif

ifeq ($(os),linux)
.PHONY: setcap
setcap: all
	sudo `whereis -b setcap | cut -d" " -f2` 'cap_net_bind_service=+ep' $(EXEODIR)/sbbs
endif

.PHONY: sexyz
sexyz:	$(SEXYZ)

.PHONY: jsdoor
jsdoor: $(GIT_INFO) $(JS_DEPS) $(CRYPT_DEPS) $(XPDEV-MT_LIB) $(SMBLIB) $(UIFCLIB-MT) $(CIOLIB-MT) $(JSDOOR)

# Library dependencies
$(SBBS):
$(FTPSRVR): $(SMBLIB) 
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
$(ADDFILES): $(XPDEV_LIB) $(SMBLIB)
$(FILELIST): $(XPDEV_LIB) $(SMBLIB)
$(MAKEUSER): $(XPDEV_LIB)
$(ANS2ASC):
$(ASC2ANS):
$(SEXYZ): $(XPDEV-MT_LIB) $(SMBLIB)
$(QWKNODES): $(XPDEV_LIB)
$(SLOG): $(XPDEV_LIB)
$(DELFILES): $(XPDEV_LIB) $(SMBLIB)
$(DUPEFIND): $(XPDEV_LIB) $(SMBLIB)
$(READSAUCE): $(XPDEV_LIB)
$(UPGRADE_TO_V319): $(XPDEV_LIB) $(SMBLIB)
$(UPGRADE_TO_V320): $(XPDEV_LIB)

