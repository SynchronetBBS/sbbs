# GNU makefile for Synchronet installation
#
# Usage:
# -----
# [g]make -f path/to/this/file [variable=value]... [target]
#
# variables (set to any other value to enable, e.g. DEBUG=1):
# ---------
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

# targets:
# -------
# install (the default)
# build

# the magic bit:
MKFLAGS += MAKEFLAGS=
ifndef DEBUG
 ifndef RELEASE
  RELEASE	:=	1
 endif
endif

ifdef SYMLINK
 INSBIN	:=	ln -sf
 INSTALL :=	symlinks
else
 INSBIN	:=	cp -r
 INSTALL :=	install
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

SBBSDIR	?=	$(shell /bin/pwd)
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
ifeq ($(os),linux)
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

default: install

build: localdefs binaries baja externals

localdefs: src
	echo $(MKFLAGS) | tr ' ' '\n' > $(REPODIR)/src/build/localdefs.mk

binaries:	sbbs3 gtkutils syncview sexpots

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

gtkutils:	src
ifdef USE_GLADE
	$(MAKE) -C $(REPODIR)/src/sbbs3 $(MKFLAGS) gtkutils
endif

syncview:
	$(MAKE) -C $(REPODIR)/src/sbbs3/syncview $(MKFLAGS) SBBS_SRC=$(REPODIR)/src/sbbs3/ XPDEV=$(REPODIR)/src/xpdev/

install: build
	@echo Installing to $(SBBSDIR)
	SBBSEXEC=$(SBBSDIR)/exec $(MAKE) -C $(REPODIR)/src/sbbs3 $(MKFLAGS) $(INSTALL)
	$(INSBIN) $(REPODIR)/src/sexpots/$(CCPRE).$(machine).exe.$(BUILDPATH)/sexpots $(SBBSDIR)/exec/sexpots
ifndef SETCAP
	-chown -R $(SBBSCHOWN) $(SBBSDIR)
	-chown -h $(SBBSCHOWN) $(SBBSDIR)/exec/*
endif

$(REPODIR):
	git clone https://gitlab.com/SynchronetBBS/sbbs.git $(REPODIR) \
		|| git clone https://github.com/SynchronetBBS/sbbs.git $(REPODIR) \
		|| git clone https://gitlab.synchro.net/main/sbbs.git $(REPODIR)
	cd $(REPODIR); git remote set-url origin https://gitlab.synchro.net/main/sbbs.git
ifdef TAG
	cd $(REPODIR); git checkout tags/$(TAG)
endif

src: $(REPODIR)

run: $(SBBSDIR)/ctrl $(SBBSDIR)/docs $(SBBSDIR)/exec $(SBBSDIR)/text $(SBBSDIR)/web $(SBBSDIR)/webv4 $(SBBSDIR)/xtrn \
	$(SBBSDIR)/node1 $(SBBSDIR)/node2 $(SBBSDIR)/node3 $(SBBSDIR)/node4


$(SBBSDIR)/ctrl: $(SBBSDIR) $(REPODIR)
	cp -r $(REPODIR)/ctrl $(SBBSDIR)

$(SBBSDIR)/docs: $(SBBSDIR) $(REPODIR)
	$(INSBIN) $(REPODIR)/docs $(SBBSDIR)

$(SBBSDIR)/exec: $(SBBSDIR) $(REPODIR)
	$(INSBIN) $(REPODIR)/exec $(SBBSDIR)

$(SBBSDIR)/node1: $(SBBSDIR) $(REPODIR)
	cp -r $(REPODIR)/node1 $@

$(SBBSDIR)/node2: $(SBBSDIR) $(REPODIR)
	cp -r $(REPODIR)/node1 $@

$(SBBSDIR)/node3: $(SBBSDIR) $(REPODIR)
	cp -r $(REPODIR)/node1 $@

$(SBBSDIR)/node4: $(SBBSDIR) $(REPODIR)
	cp -r $(REPODIR)/node1 $@

$(SBBSDIR)/text: $(SBBSDIR) $(REPODIR)
	$(INSBIN) $(REPODIR)/text $(SBBSDIR)

$(SBBSDIR)/web: $(SBBSDIR) $(REPODIR)
	$(INSBIN) $(REPODIR)/web $(SBBSDIR)

$(SBBSDIR)/webv4: $(SBBSDIR) $(REPODIR)
	$(INSBIN) $(REPODIR)/webv4 $(SBBSDIR)

$(SBBSDIR)/xtrn: $(SBBSDIR) $(REPODIR)
	$(INSBIN) $(REPODIR)/xtrn $(SBBSDIR)

$(SBBSDIR):
	@[ ! -e $(SBBSDIR) ] && mkdir $(SBBSDIR);
