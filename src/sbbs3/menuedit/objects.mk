# objects.mk

# Make 'include file' listing object files for Synchronet MenuEdit

# $Id$

# LIBODIR, SLASH, and OFILE must be pre-defined


OBJS	=	$(LIBODIR)$(SLASH)menuedit.$(OFILE)\
	        $(LIBODIR)$(SLASH)ini_file.$(OFILE)\
        	$(LIBODIR)$(SLASH)str_list.$(OFILE)\
		$(LIBODIR)$(SLASH)genwrap.$(OFILE)\
		$(LIBODIR)$(SLASH)dirwrap.$(OFILE)\
		$(LIBODIR)$(SLASH)uifcx.$(OFILE)