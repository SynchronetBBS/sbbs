# targets.mk

# Make 'include file' defining targets for Synchronet MenuEdit project

# $Id$

# EXEODIR, LIBODIR, SLASH, and EXEFILE must be pre-defined

MENUEDIT=	$(EXEODIR)$(SLASH)menuedit$(EXEFILE) 

all:		$(EXEODIR) \
		$(LIBODIR) \
		$(MENUEDIT)
