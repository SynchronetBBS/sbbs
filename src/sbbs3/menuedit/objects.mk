# objects.mk

# Make 'include file' listing object files for Synchronet MenuEdit

# $Id$

# LIBODIR, SLASH, and OFILE must be pre-defined


OBJS	=	$(MTOBJODIR)$(DIRSEP)menuedit$(OFILE)\
	        $(MTOBJODIR)$(DIRSEP)ini_file$(OFILE)\
        	$(MTOBJODIR)$(DIRSEP)str_list$(OFILE)\
		$(MTOBJODIR)$(DIRSEP)genwrap$(OFILE)\
		$(MTOBJODIR)$(DIRSEP)dirwrap$(OFILE)\
		$(MTOBJODIR)$(DIRSEP)uifcx$(OFILE)
