# targets.mk

# Make 'include file' defining targets for xpdel wrappers

# $Id: targets.mk,v 1.13 2008/02/23 10:59:48 rswindell Exp $

# ODIR, DIRSEP, LIBFILE, EXEFILE, and DELETE must be pre-defined

WRAPTEST		= $(EXEODIR)$(DIRSEP)wraptest$(EXEFILE)
XPTIME			= $(EXEODIR)$(DIRSEP)xptime$(EXEFILE)
XPDEV_LIB_BUILD	= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)xpdev$(LIBFILE)
XPDEV-MT_LIB_BUILD	= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)xpdev_mt$(LIBFILE)
XPDEV_SHLIB_BUILD	= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)xpdev$(SOFILE)
XPDEV-MT_SHLIB_BUILD	= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)xpdev_mt$(SOFILE)

all: lib mtlib

tests: $(MTOBJODIR) $(LIBODIR) $(EXEODIR) $(WRAPTEST)

dl: dl-lib dl-mtlib

lib:	$(OBJODIR) $(LIBODIR) $(XPDEV_LIB_BUILD)

dl-lib: $(OBJODIR) $(LIBODIR) $(XPDEV_SHLIB_BUILD)

mtlib:	$(MTOBJODIR) $(LIBODIR) $(XPDEV-MT_LIB_BUILD)

dl-mtlib: $(MTOBJODIR) $(LIBODIR) $(XPDEV-MT_SHLIB_BUILD)

xptime: $(OBJODIR) $(LIBODIR) $(EXEODIR) $(XPTIME)

$(WRAPTEST): $(XPDEV-MT_LIB_BUILD)
