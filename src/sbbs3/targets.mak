# targets.mak

# Make 'include file' defining targets for Synchronet project

# $Id$

# LIBODIR, EXEODIR, SLASH, LIBFILE, EXEFILE, and DELETE must be pre-defined

SBBS	=	$(LIBODIR)$(SLASH)sbbs$(LIBFILE) 
FTPSRVR	=	$(LIBODIR)$(SLASH)ftpsrvr$(LIBFILE)
MAILSRVR=	$(LIBODIR)$(SLASH)mailsrvr$(LIBFILE)
BAJA	=	$(EXEODIR)$(SLASH)baja$(EXEFILE)
FIXSMB	=	$(EXEODIR)$(SLASH)fixsmb$(EXEFILE)
CHKSMB	=	$(EXEODIR)$(SLASH)chksmb$(EXEFILE)
SMBUTIL	=	$(EXEODIR)$(SLASH)smbutil$(EXEFILE)

all: $(LIBODIR) $(EXEODIR) \
	 $(SBBS) $(FTPSRVR) $(MAILSRVR) \
	 $(FIXSMB) $(CHKSMB) $(SMBUTIL) $(BAJA)

clean:
	$(DELETE) $(LIBODIR)
	$(DELETE) $(EXEODIR)