# Global GNU makefile for Synchronet
#
# Usage:
# ------
# gmake install [variable=value]...
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
# SBBSOWNER = Owner for the installed files
# SBBSGROUP = Group for the installed files
# NOCVS	= do not do CVS update
# JSLIB = Full path and filename to JavaScript library.
# CVSTAG = CVS tag to pull

ifndef DEBUG
 ifndef RELEASE
  DEBUG	:=	1
 endif
endif

ifdef SYMLINK
 INSBIN	:=	-ln -s
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

ifndef SBBSOWNER
 SBBSCHOWN	:= $(USER)
endif

ifdef SBBSGROUP
 SBBSCHOWN	:= $(SBBSCHOWN):$(SBBSGROUP)
endif

ifeq ($(INSTALL),UNIX)
 PREFIX	?=	/usr/local
 MKFLAGS	+=	PREFIX=$(PREFIX)
else	# Classic Install
 SBBSDIR	?=	$(shell pwd)
endif

os      ?=   $(shell uname)
os      :=	$(shell echo $(os) | awk '/.*/ { print tolower($$1)}')

MKFLAGS	+=	os=$(os)

ifdef DEBUG
 SUFFIX  :=  debug
 MKFLAGS	+=	DEBUG=1
else
 SUFFIX  :=  release
 MKFLAGS	+=	RELEASE=1
endif

ifdef JSLIB
 MKFLAGS	+=	JSLIB=$(JSLIB)
endif

all: binaries baja externals

binaries:	sbbs3 scfg

externals:	sbj sbl


sbbs3:	$(SBBSDIR)/src/sbbs3 $(SBBSDIR)/src/uifc $(SBBSDIR)/src/xpdev \
    $(SBBSDIR)/src/mozilla $(SBBSDIR)/lib/mozilla/js/$(os).$(SUFFIX)
	gmake -C $(SBBSDIR)/src/sbbs3 $(MKFLAGS)
      MKFLAGS	+=	BAJAPATH=../src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/baja

scfg:	$(SBBSDIR)/src/sbbs3 $(SBBSDIR)/src/uifc $(SBBSDIR)/src/xpdev
	gmake -C $(SBBSDIR)/src/sbbs3/scfg $(MKFLAGS)

baja:	$(SBBSDIR)/exec binaries
	gmake -C $(SBBSDIR)/exec $(MKFLAGS)

sbj:	$(SBBSDIR)/xtrn
	gmake -C $(SBBSDIR)/xtrn/sbj $(MKFLAGS)

sbl:	$(SBBSDIR)/xtrn
	gmake -C $(SBBSDIR)/xtrn/sbl $(MKFLAGS) SBBS_SRC=$(SBBSDIR)/src/sbbs3 XPDEV=$(SBBSDIR)/src/xpdev

install: all $(SBBSDIR)/ctrl $(SBBSDIR)/text $(SBBSDIR)/node1 $(SBBSDIR)/node2 $(SBBSDIR)/node3 $(SBBSDIR)/node4
ifeq ($(INSTALL),UNIX)
	@echo ERROR: UNIX Install type not yet supported.
	fail
else
	echo Installing to $(SBBSDIR)
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/baja $(SBBSDIR)/exec/baja
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/node $(SBBSDIR)/exec/node
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/chksmb $(SBBSDIR)/exec/chksmb
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/fixsmb $(SBBSDIR)/exec/fixsmb
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/addfiles $(SBBSDIR)/exec/addfiles
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/smbutil $(SBBSDIR)/exec/smbutil
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/sbbs $(SBBSDIR)/exec/sbbs
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/sbbsecho $(SBBSDIR)/exec/sbbsecho
	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/sbbsecho $(SBBSDIR)/exec/filelist
	$(INSBIN) $(SBBSDIR)/src/sbbs3/scfg/$(CCPRE).$(os).$(SUFFIX)/scfg $(SBBSDIR)/exec/scfg
	$(INSBIN) $(SBBSDIR)/src/sbbs3/scfg/$(CCPRE).$(os).$(SUFFIX)/scfghelp.ixb $(SBBSDIR)/exec/scfghelp.ixb
	$(INSBIN) $(SBBSDIR)/src/sbbs3/scfg/$(CCPRE).$(os).$(SUFFIX)/scfghelp.dat $(SBBSDIR)/exec/scfghelp.dat
	chown -R $(SBBSCHOWN) $(SBBSDIR)
endif

# CVS checkout command-line
CVS_CO = @cd $(SBBSDIR); cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs co

$(SBBSDIR)/ctrl: cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG) ctrl
endif

$(SBBSDIR)/text: cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG) text
endif

$(SBBSDIR)/exec: cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG) exec
endif

$(SBBSDIR)/node1: cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG) node1
endif

$(SBBSDIR)/node2: cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG) node2
endif

$(SBBSDIR)/node3: cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG) node3
endif

$(SBBSDIR)/node4: cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG) node4
endif

$(SBBSDIR)/xtrn: cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG) -N xtrn
endif

$(SBBSDIR)/src/sbbs3: cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG)  src/sbbs3
endif

$(SBBSDIR)/src/uifc: cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG)  src/uifc
endif

$(SBBSDIR)/src/xpdev: cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG)  src/xpdev
endif

$(SBBSDIR)/src/mozilla: cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG)  src/mozilla
endif

$(SBBSDIR)/lib/mozilla/js/$(os).$(SUFFIX): cvslogin
ifndef NOCVS
	$(CVS_CO) -r $(CVSTAG) lib/mozilla/js/$(os).$(SUFFIX)
endif

cvslogin: $(SBBSDIR)
ifndef NOCVS
	@echo Press \<ENTER\> when prompted for password
	@cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login
endif

$(SBBSDIR):
	@[ ! -e $(SBBSDIR) ] && mkdir $(SBBSDIR);

update:	$(SBBSDIR)/src/mozilla $(SBBSDIR)/src/xpdev $(SBBSDIR)/src/uifc $(SBBSDIR)/src/sbbs3 $(SBBSDIR)/xtrn \
     $(SBBSDIR)/node1 $(SBBSDIR)/exec $(SBBSDIR)/text $(SBBSDIR)/ctrl

update-src: $(SBBSDIR)/src/xpdev $(SBBSDIR)/src/uifc $(SBBSDIR)/src/sbbs3 $(SBBSDIR)/xtrn

