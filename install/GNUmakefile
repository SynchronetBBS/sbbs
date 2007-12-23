# Global GNU makefile for Synchronet
#
# Usage:
# ------
# [g]make install [variable=value]...
#
# variables:
# ----------
# DEBUG = Set to force a debug build
# RELEASE = Set to force a release build
# INSTALL = Set to CLASSIC (All in one sbbs dir) or UNIX (use /etc, /sbin...)
# SYMLINK = Don't copy binaries, rather create symlinks in $(SBBSDIR)/exec
# SBBSDIR = Directory to do CLASSIC install to
# PREFIX = Set to the UNIX base directory to install to
# bcc = Set to use Borland compiler
# os = Set to the OS name (Not required)
# SBBSUSER = Owner for the installed files
# SBBSGROUP = Group for the installed files
# NOCVS	= do not do CVS update
# JSLIB = Full path and filename to JavaScript library.
# CVSTAG = CVS tag to pull
# NO_X = Don't include build conio library (ciolib) for X
# NO_GLADE = Don't build Glade-based sysop tools
# X_PATH = /path/to/X (if not /usr/X11R6)

ifndef DEBUG
 ifndef RELEASE
  RELEASE	:=	1
 endif
endif

ifdef SYMLINK
 INSBIN	:=	ln -sf
else
 INSBIN	:=	cp
endif

ifdef bcc
 CCPRE	:=	bcc
 MKFLAGS	+=	bcc=1
else
 CCPRE	:=	gcc
endif

INSTALL	?=	CLASSIC		# Can be CLASSIC or UNIX
CVSTAG	?=	HEAD		# CVS tag to pull... HEAD means current.

SBBSUSER	?= $(USER)
SBBSGROUP	?= $(GROUP)
SBBSCHOWN	:= $(SBBSUSER):$(SBBSGROUP)

ifeq ($(INSTALL),UNIX)
 PREFIX	?=	/usr/local
 MKFLAGS	+=	PREFIX=$(PREFIX)
else	# Classic Install
 SBBSDIR	?=	$(shell pwd)
 export SBBSDIR
endif

# Get OS
ifndef os
 os             =       $(shell uname)
endif
os      :=      $(shell echo $(os) | tr '[A-Z]' '[a-z]' | tr ' ' '_')

machine         :=      $(shell if uname -m | egrep -v "(i[3456789]|x)86" > /dev/null; then uname -m | tr "[A-Z]" "[a-z]" | tr " " "_" ; fi)
machine		:=	$(shell if uname -m | egrep "64" > /dev/null; then uname -m | tr "[A-Z]" "[a-z]" | tr " " "_" ; else echo $(machine) ; fi)
ifeq ($(machine),)
 machine        :=      $(os)
else
 machine        :=      $(os).$(machine)
endif

MKFLAGS	+=	os=$(os)

ifdef DEBUG
 BUILD  :=  debug
 MKFLAGS	+=	DEBUG=1
else
 BUILD  :=  release
 MKFLAGS	+=	RELEASE=1
endif
BUILDPATH	?=	$(BUILD)

ifdef JSLIB
 MKFLAGS	+=	JSLIB=$(JSLIB)
endif

ifdef NO_X
 MKFLAGS	+=	NO_X=$(NO_X)
endif

ifdef X_PATH
 MKFLAGS	+=	X_PATH=$(X_PATH)
endif

# Check for GLADE
ifndef NO_GTK
 ifeq ($(shell pkg-config libglade-2.0 --exists && echo YES),YES)
  ifeq ($(shell pkg-config gtk+-2.0 --atleast-version=2.6 && echo YES),YES)
    USE_GLADE	:=	YES
  endif
 endif
endif

all: binaries baja externals

binaries:	sbbs3 scfg umonitor uedit gtkuseredit gtkchat gtkmonitor gtkuserlist syncview

externals:	sbj sbl dpoker tbd

sbbs3:	src $(SBBSDIR)/lib/mozilla/js/$(machine).$(BUILD) $(SBBSDIR)/lib/mozilla/nspr/$(machine).$(BUILD) $(SBBSDIR)/lib/cryptlib/$(machine).release
	$(MAKE) -C $(SBBSDIR)/src/sbbs3 $(MKFLAGS)

scfg:	src
	$(MAKE) -C $(SBBSDIR)/src/sbbs3/scfg $(MKFLAGS)

umonitor:	src
	$(MAKE) -C $(SBBSDIR)/src/sbbs3/umonitor $(MKFLAGS)

uedit:	src
	$(MAKE) -C $(SBBSDIR)/src/sbbs3/uedit $(MKFLAGS)

baja:	run sbbs3
	$(MAKE) -C $(SBBSDIR)/exec $(MKFLAGS) BAJAPATH=$(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/baja

sbj:	run
	$(MAKE) -C $(SBBSDIR)/xtrn/sbj $(MKFLAGS)

sbl:	run
	$(MAKE) -C $(SBBSDIR)/xtrn/sbl $(MKFLAGS) SBBS_SRC=$(SBBSDIR)/src/sbbs3/ XPDEV=$(SBBSDIR)/src/xpdev/

dpoker:	run
	$(MAKE) -C $(SBBSDIR)/xtrn/dpoker $(MKFLAGS) SBBS_SRC=$(SBBSDIR)/src/sbbs3/ XPDEV=$(SBBSDIR)/src/xpdev/

tbd:	run
	$(MAKE) -C $(SBBSDIR)/xtrn/tbd $(MKFLAGS) SBBS_SRC=$(SBBSDIR)/src/sbbs3/ XPDEV=$(SBBSDIR)/src/xpdev/

gtkuseredit:	src
ifdef USE_GLADE
	$(MAKE) -C $(SBBSDIR)/src/sbbs3/gtkuseredit $(MKFLAGS) SBBS_SRC=$(SBBSDIR)/src/sbbs3/ XPDEV=$(SBBSDIR)/src/xpdev/
endif

gtkchat:	src
ifdef USE_GLADE
	$(MAKE) -C $(SBBSDIR)/src/sbbs3/gtkchat $(MKFLAGS) SBBS_SRC=$(SBBSDIR)/src/sbbs3/ XPDEV=$(SBBSDIR)/src/xpdev/
endif

gtkmonitor:	src
ifdef USE_GLADE
	$(MAKE) -C $(SBBSDIR)/src/sbbs3/gtkmonitor $(MKFLAGS) SBBS_SRC=$(SBBSDIR)/src/sbbs3/ XPDEV=$(SBBSDIR)/src/xpdev/
endif

gtkuserlist:	src
ifdef USE_GLADE
	$(MAKE) -C $(SBBSDIR)/src/sbbs3/gtkuserlist $(MKFLAGS) SBBS_SRC=$(SBBSDIR)/src/sbbs3/ XPDEV=$(SBBSDIR)/src/xpdev/
endif

syncview:
	$(MAKE) -C $(SBBSDIR)/src/sbbs3/syncview $(MKFLAGS) SBBS_SRC=$(SBBSDIR)/src/sbbs3/ XPDEV=$(SBBSDIR)/src/xpdev/

install: all
ifeq ($(INSTALL),UNIX)
	@echo ERROR: UNIX Install type not yet supported.
	fail
else
	@echo Installing to $(SBBSDIR)
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/smbactiv $(SBBSDIR)/exec/smbactiv
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/dupefind $(SBBSDIR)/exec/dupefind
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/delfiles $(SBBSDIR)/exec/delfiles
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/allusers $(SBBSDIR)/exec/allusers
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/qwknodes $(SBBSDIR)/exec/qwknodes
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/asc2ans $(SBBSDIR)/exec/asc2ans
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/ans2asc $(SBBSDIR)/exec/ans2asc
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/jsexec $(SBBSDIR)/exec/jsexec
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/baja $(SBBSDIR)/exec/baja
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/unbaja $(SBBSDIR)/exec/unbaja
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/slog $(SBBSDIR)/exec/slog
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/node $(SBBSDIR)/exec/node
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/chksmb $(SBBSDIR)/exec/chksmb
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/fixsmb $(SBBSDIR)/exec/fixsmb
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/addfiles $(SBBSDIR)/exec/addfiles
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/makeuser $(SBBSDIR)/exec/makeuser
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/smbutil $(SBBSDIR)/exec/smbutil
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/sbbs $(SBBSDIR)/exec/sbbs
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/sbbsmono $(SBBSDIR)/exec/sbbsmono
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/sbbsecho $(SBBSDIR)/exec/sbbsecho
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/echocfg $(SBBSDIR)/exec/echocfg
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/filelist $(SBBSDIR)/exec/filelist
	$(INSBIN) $(SBBSDIR)/src/sbbs3/scfg/$(CCPRE).$(machine).exe.$(BUILDPATH)/scfg $(SBBSDIR)/exec/scfg
	$(INSBIN) $(SBBSDIR)/src/sbbs3/scfg/$(CCPRE).$(machine).exe.$(BUILDPATH)/scfghelp.ixb $(SBBSDIR)/exec/scfghelp.ixb
	$(INSBIN) $(SBBSDIR)/src/sbbs3/scfg/$(CCPRE).$(machine).exe.$(BUILDPATH)/scfghelp.dat $(SBBSDIR)/exec/scfghelp.dat
	$(INSBIN) $(SBBSDIR)/src/sbbs3/umonitor/$(CCPRE).$(machine).exe.$(BUILDPATH)/umonitor $(SBBSDIR)/exec/umonitor
	$(INSBIN) $(SBBSDIR)/src/sbbs3/uedit/$(CCPRE).$(machine).exe.$(BUILDPATH)/uedit $(SBBSDIR)/exec/uedit
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libsbbs.so $(SBBSDIR)/exec/libsbbs.so
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libftpsrvr.so $(SBBSDIR)/exec/libftpsrvr.so
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libmailsrvr.so $(SBBSDIR)/exec/libmailsrvr.so
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libservices.so $(SBBSDIR)/exec/libservices.so
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libwebsrvr.so $(SBBSDIR)/exec/libwebsrvr.so
ifdef USE_GLADE
	$(INSBIN) $(SBBSDIR)/src/sbbs3/gtkuseredit/$(CCPRE).$(machine).exe.$(BUILDPATH)/gtkuseredit $(SBBSDIR)/exec/gtkuseredit
	$(INSBIN) $(SBBSDIR)/src/sbbs3/gtkuseredit/gtkuseredit.glade $(SBBSDIR)/exec/gtkuseredit.glade
	$(INSBIN) $(SBBSDIR)/src/sbbs3/gtkmonitor/$(CCPRE).$(machine).exe.$(BUILDPATH)/gtkmonitor $(SBBSDIR)/exec/gtkmonitor
	$(INSBIN) $(SBBSDIR)/src/sbbs3/gtkmonitor/gtkmonitor.glade $(SBBSDIR)/exec/gtkmonitor.glade
	$(INSBIN) $(SBBSDIR)/src/sbbs3/gtkmonitor/pixmaps/stock_help-chat.png $(SBBSDIR)/exec/stock_help-chat.png
	$(INSBIN) $(SBBSDIR)/src/sbbs3/gtkmonitor/pixmaps/stock_mail-send.png $(SBBSDIR)/exec/stock_mail-send.png
	$(INSBIN) $(SBBSDIR)/src/sbbs3/gtkchat/$(CCPRE).$(machine).exe.$(BUILDPATH)/gtkchat $(SBBSDIR)/exec/gtkchat
	$(INSBIN) $(SBBSDIR)/src/sbbs3/gtkuserlist/$(CCPRE).$(machine).exe.$(BUILDPATH)/gtkuserlist $(SBBSDIR)/exec/gtkuserlist
	$(INSBIN) $(SBBSDIR)/src/sbbs3/gtkuserlist/gtkuserlist.glade $(SBBSDIR)/exec/gtkuserlist.glade
endif
	$(INSBIN) $(SBBSDIR)/src/sbbs3/syncview/$(CCPRE).$(machine).exe.$(BUILDPATH)/syncview $(SBBSDIR)/exec/syncview
# kludge... must fix this to allow moz JS libs and such.  ToDo
	$(INSBIN) $(SBBSDIR)/lib/mozilla/*/$(machine).$(BUILD)/*.so $(SBBSDIR)/exec/
	-chown -R $(SBBSCHOWN) $(SBBSDIR)
	-chown -h $(SBBSCHOWN) $(SBBSDIR)/exec/*
endif

# CVS checkout command-line
CVS_CO = @cd $(SBBSDIR); cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs co -r $(CVSTAG)

src: 
ifndef NOCVS
	$(CVS_CO) src-sbbs3
endif

run: 
ifndef NOCVS
	$(CVS_CO) run-sbbs3
endif

$(SBBSDIR)/lib/mozilla/js/$(machine).$(BUILD): 
ifndef NOCVS
	$(CVS_CO) lib/mozilla/js/$(machine).$(BUILD)
endif

$(SBBSDIR)/lib/mozilla/nspr/$(machine).$(BUILD): 
ifndef NOCVS
	$(CVS_CO) lib/mozilla/nspr/$(machine).$(BUILD)
endif

$(SBBSDIR)/lib/cryptlib/$(machine).release: 
ifndef NOCVS
	$(CVS_CO) lib/cryptlib/$(machine).release
endif

cvslogin: $(SBBSDIR)
ifndef NOCVS
	@echo Press \<ENTER\> when prompted for password
	@cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login
endif

$(SBBSDIR):
	@[ ! -e $(SBBSDIR) ] && mkdir $(SBBSDIR);
