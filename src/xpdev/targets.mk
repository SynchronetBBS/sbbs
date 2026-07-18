# targets.mk

# Make 'include file' defining targets for xpdel wrappers

# $Id: targets.mk,v 1.13 2008/02/23 10:59:48 rswindell Exp $

# ODIR, DIRSEP, LIBFILE, EXEFILE, and DELETE must be pre-defined

WRAPTEST		= $(EXEODIR)$(DIRSEP)wraptest$(EXEFILE)
MIXERTEST		= $(EXEODIR)$(DIRSEP)mixertest$(EXEFILE)
SOPENFILE		= $(EXEODIR)$(DIRSEP)sopenfile$(EXEFILE)
LOCKFILE		= $(EXEODIR)$(DIRSEP)lockfile$(EXEFILE)
SHOWLOCKS		= $(EXEODIR)$(DIRSEP)showlocks$(EXEFILE)
XPTIME			= $(EXEODIR)$(DIRSEP)xptime$(EXEFILE)
XPDEV_LIB_BUILD	= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)xpdev$(LIBFILE)
XPDEV-MT_LIB_BUILD	= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)xpdev_mt$(LIBFILE)
XPDEV_SHLIB_BUILD	= $(LIBODIR)$(DIRSEP)$(SHLIBPREFIX)xpdev$(SOFILE)
XPDEV-MT_SHLIB_BUILD	= $(LIBODIR)$(DIRSEP)$(SHLIBPREFIX)xpdev_mt$(SOFILE)

all: lib mtlib

tests: $(MTOBJODIR) $(LIBODIR) $(EXEODIR) $(WRAPTEST) $(MIXERTEST) $(SOPENFILE) $(LOCKFILE) $(SHOWLOCKS)

# Build and RUN the automated regression tests (nonzero exit == failure), for
# CI. mixertest is headless and self-checking; wraptest is interactive, so it
# is built by "tests" but deliberately not run here. PHONY because the target
# shares its name with the unrelated (and unbuilt) test.c in this directory.
.PHONY: test
# VALGRIND is empty by default (plain run); CI sets it to a valgrind command
# line so the regression runs under the leak checker where valgrind exists.
test: $(MTOBJODIR) $(LIBODIR) $(EXEODIR) $(MIXERTEST)
	$(VALGRIND) $(MIXERTEST)

dl: dl-lib dl-mtlib

lib:	$(OBJODIR) $(LIBODIR) $(XPDEV_LIB_BUILD)

dl-lib: $(OBJODIR) $(LIBODIR) $(XPDEV_SHLIB_BUILD)

mtlib:	$(MTOBJODIR) $(LIBODIR) $(XPDEV-MT_LIB_BUILD)

dl-mtlib: $(MTOBJODIR) $(LIBODIR) $(XPDEV-MT_SHLIB_BUILD)

xptime: $(OBJODIR) $(LIBODIR) $(EXEODIR) $(XPTIME)

$(WRAPTEST): $(XPDEV-MT_LIB_BUILD)
$(MIXERTEST): $(XPDEV-MT_LIB_BUILD)
