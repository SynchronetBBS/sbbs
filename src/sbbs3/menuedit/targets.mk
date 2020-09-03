# targets.mk

# Make 'include file' defining targets for Synchronet MenuEdit project

# $Id: targets.mk,v 1.2 2011/05/27 06:03:31 deuce Exp $

# EXEODIR, LIBODIR, DIRSEP, and EXEFILE must be pre-defined

MENUEDIT=	$(EXEODIR)$(DIRSEP)menuedit$(EXEFILE) 

all:		$(MTOBJODIR) \
		$(EXEODIR) \
		$(LIBODIR) \
		xpdev-mt uifc-mt ciolib-mt \
		$(MENUEDIT)
