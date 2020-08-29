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
# WORKDIR = Directory where source files are cloned
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
# USE_DOSEMU = Set to 1 to enable Linux-DOSEMU support

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
 INSBIN	:=	cp
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
WORKDIR		= $(SBBSDIR)/repo

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

ifdef USE_DOSEMU
 MKFLAGS	+=	USE_DOSEMU=$(USE_DOSEMU)
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
	echo $(MKFLAGS) | tr ' ' '\n' > $(WORKDIR)/src/build/localdefs.mk

binaries:	sbbs3 gtkuseredit gtkchat gtkmonitor gtkuserlist syncview sexpots

externals:	sbj dpoker tbd

sbbs3:	src
ifeq ($(os),linux)
	$(MAKE) -C $(WORKDIR)/src/sbbs3 $(MKFLAGS) setcap
else
	$(MAKE) -C $(WORKDIR)/src/sbbs3 $(MKFLAGS)
endif

sexpots:	src
	$(MAKE) -C $(WORKDIR)/src/sexpots $(MKFLAGS)

baja:	run sbbs3
	$(MAKE) -C $(SBBSDIR)/exec $(MKFLAGS) BAJAPATH=$(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/baja

sbj:	run
	$(MAKE) -C $(WORKDIR)/xtrn/sbj $(MKFLAGS)

dpoker:	run
	$(MAKE) -C $(WORKDIR)/xtrn/dpoker $(MKFLAGS) SBBS_SRC=$(WORKDIR)/src/sbbs3/ XPDEV=$(WORKDIR)/src/xpdev/

tbd:	run
	$(MAKE) -C $(WORKDIR)/xtrn/tbd $(MKFLAGS) SBBS_SRC=$(WORKDIR)/src/sbbs3/ XPDEV=$(WORKDIR)/src/xpdev/

gtkuseredit:	src
ifdef USE_GLADE
	$(MAKE) -C $(WORKDIR)/src/sbbs3/gtkuseredit $(MKFLAGS) SBBS_SRC=$(WORKDIR)/src/sbbs3/ XPDEV=$(WORKDIR)/src/xpdev/
endif

gtkchat:	src
ifdef USE_GLADE
	$(MAKE) -C $(WORKDIR)/src/sbbs3/gtkchat $(MKFLAGS) SBBS_SRC=$(WORKDIR)/src/sbbs3/ XPDEV=$(WORKDIR)/src/xpdev/
endif

gtkmonitor:	src
ifdef USE_GLADE
	$(MAKE) -C $(WORKDIR)/src/sbbs3/gtkmonitor $(MKFLAGS) SBBS_SRC=$(WORKDIR)/src/sbbs3/ XPDEV=$(WORKDIR)/src/xpdev/
endif

gtkuserlist:	src
ifdef USE_GLADE
	$(MAKE) -C $(WORKDIR)/src/sbbs3/gtkuserlist $(MKFLAGS) SBBS_SRC=$(WORKDIR)/src/sbbs3/ XPDEV=$(WORKDIR)/src/xpdev/
endif

syncview:
	$(MAKE) -C $(WORKDIR)/src/sbbs3/syncview $(MKFLAGS) SBBS_SRC=$(WORKDIR)/src/sbbs3/ XPDEV=$(WORKDIR)/src/xpdev/

install: all
	@echo Installing to $(SBBSDIR)
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/showstat $(SBBSDIR)/exec/showstat
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/readsauce $(SBBSDIR)/exec/readsauce
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/dstsedit $(SBBSDIR)/exec/dstsedit
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/smbactiv $(SBBSDIR)/exec/smbactiv
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/dupefind $(SBBSDIR)/exec/dupefind
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/delfiles $(SBBSDIR)/exec/delfiles
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/allusers $(SBBSDIR)/exec/allusers
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/qwknodes $(SBBSDIR)/exec/qwknodes
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/asc2ans $(SBBSDIR)/exec/asc2ans
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/ans2asc $(SBBSDIR)/exec/ans2asc
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/jsexec $(SBBSDIR)/exec/jsexec
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/baja $(SBBSDIR)/exec/baja
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/unbaja $(SBBSDIR)/exec/unbaja
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/slog $(SBBSDIR)/exec/slog
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/node $(SBBSDIR)/exec/node
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/chksmb $(SBBSDIR)/exec/chksmb
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/fixsmb $(SBBSDIR)/exec/fixsmb
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/addfiles $(SBBSDIR)/exec/addfiles
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/makeuser $(SBBSDIR)/exec/makeuser
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/smbutil $(SBBSDIR)/exec/smbutil
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/sbbs $(SBBSDIR)/exec/sbbs
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/sbbsecho $(SBBSDIR)/exec/sbbsecho
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/sexyz $(SBBSDIR)/exec/sexyz
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/echocfg $(SBBSDIR)/exec/echocfg
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).exe.$(BUILDPATH)/filelist $(SBBSDIR)/exec/filelist
	$(INSBIN) $(WORKDIR)/src/sbbs3/scfg/$(CCPRE).$(machine).exe.$(BUILDPATH)/scfg $(SBBSDIR)/exec/scfg
	$(INSBIN) $(WORKDIR)/src/sbbs3/umonitor/$(CCPRE).$(machine).exe.$(BUILDPATH)/umonitor $(SBBSDIR)/exec/umonitor
	$(INSBIN) $(WORKDIR)/src/sbbs3/uedit/$(CCPRE).$(machine).exe.$(BUILDPATH)/uedit $(SBBSDIR)/exec/uedit
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libsbbs.so $(SBBSDIR)/exec/libsbbs.so
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libftpsrvr.so $(SBBSDIR)/exec/libftpsrvr.so
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libmailsrvr.so $(SBBSDIR)/exec/libmailsrvr.so
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libservices.so $(SBBSDIR)/exec/libservices.so
	$(INSBIN) $(WORKDIR)/src/sbbs3/$(CCPRE).$(machine).lib.$(BUILDPATH)/libwebsrvr.so $(SBBSDIR)/exec/libwebsrvr.so
ifdef USE_GLADE
	$(INSBIN) $(WORKDIR)/src/sbbs3/gtkuseredit/$(CCPRE).$(machine).exe.$(BUILDPATH)/gtkuseredit $(SBBSDIR)/exec/gtkuseredit
	$(INSBIN) $(WORKDIR)/src/sbbs3/gtkuseredit/gtkuseredit.glade $(SBBSDIR)/exec/gtkuseredit.glade
	$(INSBIN) $(WORKDIR)/src/sbbs3/gtkmonitor/$(CCPRE).$(machine).exe.$(BUILDPATH)/gtkmonitor $(SBBSDIR)/exec/gtkmonitor
	$(INSBIN) $(WORKDIR)/src/sbbs3/gtkmonitor/gtkmonitor.glade $(SBBSDIR)/exec/gtkmonitor.glade
	$(INSBIN) $(WORKDIR)/src/sbbs3/gtkmonitor/pixmaps/stock_help-chat.png $(SBBSDIR)/exec/stock_help-chat.png
	$(INSBIN) $(WORKDIR)/src/sbbs3/gtkmonitor/pixmaps/stock_mail-send.png $(SBBSDIR)/exec/stock_mail-send.png
	$(INSBIN) $(WORKDIR)/src/sbbs3/gtkchat/$(CCPRE).$(machine).exe.$(BUILDPATH)/gtkchat $(SBBSDIR)/exec/gtkchat
	$(INSBIN) $(WORKDIR)/src/sbbs3/gtkuserlist/$(CCPRE).$(machine).exe.$(BUILDPATH)/gtkuserlist $(SBBSDIR)/exec/gtkuserlist
	$(INSBIN) $(WORKDIR)/src/sbbs3/gtkuserlist/gtkuserlist.glade $(SBBSDIR)/exec/gtkuserlist.glade
endif
	$(INSBIN) $(WORKDIR)/src/sbbs3/syncview/$(CCPRE).$(machine).exe.$(BUILDPATH)/syncview $(SBBSDIR)/exec/syncview
	$(INSBIN) $(WORKDIR)/src/sexpots/$(CCPRE).$(machine).exe.$(BUILDPATH)/sexpots $(SBBSDIR)/exec/sexpots
	-chown -R $(SBBSCHOWN) $(SBBSDIR)
	-chown -h $(SBBSCHOWN) $(SBBSDIR)/exec/*

$(WORKDIR):
	git clone https://gitlab.com/synchronet/sbbs.git $(WORKDIR) \
		|| git clone https://github.com/rswindell/sbbs.git $(WORKDIR) \
		|| git clone https://gitlab.synchro.net/sbbs/sbbs.git $(WORKDIR)
	git remote set-url origin https://gitlab.synchro.net/sbbs/sbbs.git
ifdef TAG
	git -C $(WORKDIR) checkout tags/$(TAG)
endif

src: $(WORKDIR)

run: $(WORKDIR)
	cp -r $(WORKDIR)/ctrl $(SBBSDIR)
	cp -r $(WORKDIR)/docs $(SBBSDIR)
	cp -r $(WORKDIR)/exec $(SBBSDIR)
	cp -r $(WORKDIR)/node1 $(SBBSDIR)
	cp -r $(WORKDIR)/text $(SBBSDIR)
	cp -r $(WORKDIR)/web $(SBBSDIR)
	cp -r $(WORKDIR)/xtrn $(SBBSDIR)

$(SBBSDIR):
	@[ ! -e $(SBBSDIR) ] && mkdir $(SBBSDIR);
