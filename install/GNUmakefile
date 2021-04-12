# $Id: GNUmakefile,v 1.79 2020/05/07 22:17:44 rswindell Exp $
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
# SYMLINK = Don't copy binaries, rather create symlinks in $(SBBSDIR)/exec
# SBBSDIR = Directory to do install to
# REPODIR = Directory where source files are cloned
# NOCAP = Set to defeat sbbs3 bind-capabilities (setcap) build target on Linux
# bcc = Set to use Borland compiler
# os = Set to the OS name (Not required)
# SBBSUSER = Owner for the installed files
# SBBSGROUP = Group for the installed files
# JSLIB = Library name of JavaScript library.
# JSLIBDIR = Full path to JavaScript library
# CRYPTLIBINCLUDE = Path to cryptlib header file(s)
# CRYPTLIBDIR = Path to libcl.*
# NSPRDIR = Path to nspr4 library
# NSPRINCLUDE = Path to NSPR header files
# SDL_CONFIG = Path to sdl-config program
# TAG = Git tag to checkout
# NO_X = Don't include build conio library (ciolib) for X
# NO_GTK = Don't build GTK-based sysop tools
# X_PATH = /path/to/X (if not /usr/X11R6)

# the magic bit:
MKFLAGS += MAKEFLAGS=
ifndef DEBUG
 ifndef RELEASE
  RELEASE	:=	1
 endif
endif

ifdef SYMLINK
 INSBIN	:=	ln -sf
else
 INSBIN	:=	cp -r
endif

ifdef bcc
 CCPRE	:=	bcc
 MKFLAGS	+=	bcc=1
else
 CC		?=	gcc
 CCPRE	?= ${shell if [ `echo __clang__ | $(CC) -E - | grep -v '^\#'` != __clang__ ] ; then echo clang ; elif [ `echo __INTEL_COMPILER | $(CC) -E - | grep -v '^\#'` != __INTEL_COMPILER ] ; then echo icc ; else echo gcc ; fi}
 CCPRE := $(lastword $(subst /, ,$(CCPRE)))
endif

SBBSUSER	?= $(USER)
SBBSGROUP	?= $(GROUP)
SBBSCHOWN	:= $(SBBSUSER):$(SBBSGROUP)

SBBSDIR	?=	$(shell pwd)
export SBBSDIR
REPODIR		= $(SBBSDIR)/repo

# Get OS
ifndef os
 os             =       $(shell uname)
endif
os      		:=      $(shell echo $(os) | tr '[A-Z]' '[a-z]' | tr ' ' '_')

machine         :=      $(shell if uname -m | egrep -v "(i[3456789]|x)86" > /dev/null; then uname -m | tr "[A-Z]" "[a-z]" | tr " " "_" ; fi)
machine			:=	$(shell if uname -m | egrep "64" > /dev/null; then uname -m | tr "[A-Z]" "[a-z]" | tr " " "_" ; else echo $(machine) ; fi)
ifeq ($(machine),x86_64)
 machine        := 	x64
endif
ifeq ($(machine),)
 machine        :=      $(os)
else
 machine        :=      $(os).$(machine)
endif

MKFLAGS	+=	os=$(os)

ifndef NOCAP
ifeq (($os),linux)
	SETCAP 	:=	setcap
endif
endif

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

ifdef JSLIBDIR
 MKFLAGS	+=	JSLIBDIR=$(JSLIBDIR)
endif

ifdef CRYPTLIBINCLUDE
 MKFLAGS	+=	CRYPTLIBINCLUDE=$(CRYPTLIBINCLUDE)
endif

ifdef CRYPTLIBDIR
 MKFLAGS	+=	CRYPTLIBDIR=$(CRYPTLIBDIR)
endif

ifdef NSPRDIR
 MKFLAGS	+=	NSPRDIR=$(NSPRDIR)
endif

ifdef NSPRINCLUDE
 MKFLAGS	+=	NSPRINCLUDE=$(NSPRINCLUDE)
endif

ifdef SDL_CONFIG
 MKFLAGS	+=	SDL_CONFIG=$(SDL_CONFIG)
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

all: localdefs binaries baja externals

localdefs: src
	echo $(MKFLAGS) | tr ' ' '\n' > $(REPODIR)/src/build/localdefs.mk

binaries:	sbbs3 gtkuseredit gtkchat gtkmonitor gtkuserlist syncview sexpots

externals:	sbj dpoker tbd

sbbs3:	src
	$(MAKE) -C $(REPODIR)/src/sbbs3 $(MKFLAGS) $(SETCAP)

sexpots:	src
	$(MAKE) -C $(REPODIR)/src/sexpots $(MKFLAGS)

baja:	run sbbs3
	$(MAKE) -C $(SBBSDIR)/exec $(MKFLAGS) BAJAPATH=$(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/baja

sbj:	run
	$(MAKE) -C $(SBBSDIR)/xtrn/sbj $(MKFLAGS) SRC_ROOT=$(REPODIR)/src XPDEV=$(REPODIR)/src/xpdev/

dpoker:	run
	$(MAKE) -C $(SBBSDIR)/xtrn/dpoker $(MKFLAGS) SRC_ROOT=$(REPODIR)/src XPDEV=$(REPODIR)/src/xpdev/

tbd:	run
	$(MAKE) -C $(SBBSDIR)/xtrn/tbd $(MKFLAGS) SRC_ROOT=$(REPODIR)/src XPDEV=$(REPODIR)/src/xpdev/

gtkuseredit:	src
ifdef USE_GLADE
	$(MAKE) -C $(REPODIR)/src/sbbs3/gtkuseredit $(MKFLAGS) SBBS_SRC=$(REPODIR)/src/sbbs3/ XPDEV=$(REPODIR)/src/xpdev/
endif

gtkchat:	src
ifdef USE_GLADE
	$(MAKE) -C $(REPODIR)/src/sbbs3/gtkchat $(MKFLAGS) SBBS_SRC=$(REPODIR)/src/sbbs3/ XPDEV=$(REPODIR)/src/xpdev/
endif

gtkmonitor:	src
ifdef USE_GLADE
	$(MAKE) -C $(REPODIR)/src/sbbs3/gtkmonitor $(MKFLAGS) SBBS_SRC=$(REPODIR)/src/sbbs3/ XPDEV=$(REPODIR)/src/xpdev/
endif

gtkuserlist:	src
ifdef USE_GLADE
	$(MAKE) -C $(REPODIR)/src/sbbs3/gtkuserlist $(MKFLAGS) SBBS_SRC=$(REPODIR)/src/sbbs3/ XPDEV=$(REPODIR)/src/xpdev/
endif

syncview:
	$(MAKE) -C $(REPODIR)/src/sbbs3/syncview $(MKFLAGS) SBBS_SRC=$(REPODIR)/src/sbbs3/ XPDEV=$(REPODIR)/src/xpdev/

install: all
	@echo Installing to $(SBBSDIR)
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/showstat $(SBBSDIR)/exec/showstat
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/readsauce $(SBBSDIR)/exec/readsauce
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/dstsedit $(SBBSDIR)/exec/dstsedit
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/smbactiv $(SBBSDIR)/exec/smbactiv
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/dupefind $(SBBSDIR)/exec/dupefind
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/delfiles $(SBBSDIR)/exec/delfiles
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/allusers $(SBBSDIR)/exec/allusers
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/qwknodes $(SBBSDIR)/exec/qwknodes
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/asc2ans $(SBBSDIR)/exec/asc2ans
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/ans2asc $(SBBSDIR)/exec/ans2asc
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/jsexec $(SBBSDIR)/exec/jsexec
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/baja $(SBBSDIR)/exec/baja
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/unbaja $(SBBSDIR)/exec/unbaja
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/slog $(SBBSDIR)/exec/slog
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/node $(SBBSDIR)/exec/node
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/chksmb $(SBBSDIR)/exec/chksmb
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/fixsmb $(SBBSDIR)/exec/fixsmb
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/addfiles $(SBBSDIR)/exec/addfiles
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/makeuser $(SBBSDIR)/exec/makeuser
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/smbutil $(SBBSDIR)/exec/smbutil
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/sbbs $(SBBSDIR)/exec/sbbs
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/sbbsecho $(SBBSDIR)/exec/sbbsecho
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/sexyz $(SBBSDIR)/exec/sexyz
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/echocfg $(SBBSDIR)/exec/echocfg
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/filelist $(SBBSDIR)/exec/filelist
	$(INSBIN) $(REPODIR)/src/sbbs3/scfg/$(CCPRE).$(machine).exe.$(BUILDPATH)/scfg $(SBBSDIR)/exec/scfg
	$(INSBIN) $(REPODIR)/src/sbbs3/umonitor/$(CCPRE).$(machine).exe.$(BUILDPATH)/umonitor $(SBBSDIR)/exec/umonitor
	$(INSBIN) $(REPODIR)/src/sbbs3/uedit/$(CCPRE).$(machine).exe.$(BUILDPATH)/uedit $(SBBSDIR)/exec/uedit
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/upgrade_to_v319 $(SBBSDIR)/exec/upgrade_to_v319
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libsbbs.so $(SBBSDIR)/exec/libsbbs.so
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libftpsrvr.so $(SBBSDIR)/exec/libftpsrvr.so
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libmailsrvr.so $(SBBSDIR)/exec/libmailsrvr.so
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libservices.so $(SBBSDIR)/exec/libservices.so
	$(INSBIN) $(REPODIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libwebsrvr.so $(SBBSDIR)/exec/libwebsrvr.so
ifdef USE_GLADE
	$(INSBIN) $(REPODIR)/src/sbbs3/gtkuseredit/$(CCPRE).$(machine).exe.$(BUILDPATH)/gtkuseredit $(SBBSDIR)/exec/gtkuseredit
	$(INSBIN) $(REPODIR)/src/sbbs3/gtkuseredit/gtkuseredit.glade $(SBBSDIR)/exec/gtkuseredit.glade
	$(INSBIN) $(REPODIR)/src/sbbs3/gtkmonitor/$(CCPRE).$(machine).exe.$(BUILDPATH)/gtkmonitor $(SBBSDIR)/exec/gtkmonitor
	$(INSBIN) $(REPODIR)/src/sbbs3/gtkmonitor/gtkmonitor.glade $(SBBSDIR)/exec/gtkmonitor.glade
	$(INSBIN) $(REPODIR)/src/sbbs3/gtkmonitor/pixmaps/stock_help-chat.png $(SBBSDIR)/exec/stock_help-chat.png
	$(INSBIN) $(REPODIR)/src/sbbs3/gtkmonitor/pixmaps/stock_mail-send.png $(SBBSDIR)/exec/stock_mail-send.png
	$(INSBIN) $(REPODIR)/src/sbbs3/gtkchat/$(CCPRE).$(machine).exe.$(BUILDPATH)/gtkchat $(SBBSDIR)/exec/gtkchat
	$(INSBIN) $(REPODIR)/src/sbbs3/gtkuserlist/$(CCPRE).$(machine).exe.$(BUILDPATH)/gtkuserlist $(SBBSDIR)/exec/gtkuserlist
	$(INSBIN) $(REPODIR)/src/sbbs3/gtkuserlist/gtkuserlist.glade $(SBBSDIR)/exec/gtkuserlist.glade
endif
	$(INSBIN) $(REPODIR)/src/sbbs3/syncview/$(CCPRE).$(machine).exe.$(BUILDPATH)/syncview $(SBBSDIR)/exec/syncview
	$(INSBIN) $(REPODIR)/src/sexpots/$(CCPRE).$(machine).exe.$(BUILDPATH)/sexpots $(SBBSDIR)/exec/sexpots
	-chown -R $(SBBSCHOWN) $(SBBSDIR)
	-chown -h $(SBBSCHOWN) $(SBBSDIR)/exec/*

$(REPODIR):
	git clone https://gitlab.com/SynchronetBBS/sbbs.git $(REPODIR) \
		|| git clone https://github.com/SynchronetBBS/sbbs.git $(REPODIR) \
		|| git clone https://gitlab.synchro.net/main/sbbs.git $(REPODIR)
	git -C $(REPODIR) remote set-url origin https://gitlab.synchro.net/main/sbbs.git
ifdef TAG
	git -C $(REPODIR) checkout tags/$(TAG)
endif

src: $(REPODIR)

run: $(SBBSDIR)/ctrl $(SBBSDIR)/docs $(SBBSDIR)/exec $(SBBSDIR)/text $(SBBSDIR)/web $(SBBSDIR)/webv4 $(SBBSDIR)/xtrn \
	$(SBBSDIR)/node1 $(SBBSDIR)/node2 $(SBBSDIR)/node3 $(SBBSDIR)/node4

$(SBBSDIR)/ctrl: $(REPODIR)
	cp -r $(REPODIR)/ctrl $(SBBSDIR)

$(SBBSDIR)/docs: $(REPODIR)
	$(INSBIN) $(REPODIR)/docs $(SBBSDIR)

$(SBBSDIR)/exec: $(REPODIR)
	$(INSBIN) $(REPODIR)/exec $(SBBSDIR)

$(SBBSDIR)/node1: $(REPODIR)
	cp -r $(REPODIR)/node1 $@

$(SBBSDIR)/node2: $(REPODIR)
	cp -r $(REPODIR)/node1 $@

$(SBBSDIR)/node3: $(REPODIR)
	cp -r $(REPODIR)/node1 $@

$(SBBSDIR)/node4: $(REPODIR)
	cp -r $(REPODIR)/node1 $@

$(SBBSDIR)/text: $(REPODIR)
	$(INSBIN) $(REPODIR)/text $(SBBSDIR)

$(SBBSDIR)/web: $(REPODIR)
	$(INSBIN) $(REPODIR)/web $(SBBSDIR)

$(SBBSDIR)/webv4: $(REPODIR)
	$(INSBIN) $(REPODIR)/webv4 $(SBBSDIR)

$(SBBSDIR)/xtrn: $(REPODIR)
	$(INSBIN) $(REPODIR)/xtrn $(SBBSDIR)

$(SBBSDIR):
	@[ ! -e $(SBBSDIR) ] && mkdir $(SBBSDIR);
