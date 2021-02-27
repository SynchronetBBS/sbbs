# targets.mk

# Make 'include file' defining targets for Synchronet SBBSINST project

# $Id: targets.mk,v 1.4 2004/09/16 19:02:02 deuce Exp $

# ODIR, SLASH, LIBFILE, EXEFILE, and DELETE must be pre-defined

SBBSINST	=	$(EXEODIR)$(DIRSEP)sbbsinst$(EXEFILE) 

all:	xpdev-mt ciolib-mt uifc-mt $(EXEODIR) \
		$(MTOBJODIR) \
		$(SBBSINST)

$(SBBSINST):	$(XPDEV-MT_LIB) $(CIOLIB-MT) $(UIFCLIB-MT)
