# targets.mk

# Make 'include file' defining targets for Synchronet MenuEdit project

# $Id$

# EXEODIR, LIBODIR, DIRSEP, and EXEFILE must be pre-defined

MENUEDIT=	$(EXEODIR)$(DIRSEP)menuedit$(EXEFILE) 

all:		$(MTOBJODIR) \
		$(EXEODIR) \
		$(LIBODIR) \
		xpdev-mt uifc-mt ciolib-mt \
		$(MENUEDIT)
