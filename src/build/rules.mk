# build/rules.mk
#
# Global build targets for all make systems
#
# $Id$

$(OBJODIR):
	$(QUIET)$(IFNOTEXIST) mkdir $(OBJODIR)

$(LIBODIR):
	$(QUIET)$(IFNOTEXIST) mkdir $(LIBODIR)

$(EXEODIR):
	$(QUIET)$(IFNOTEXIST) mkdir $(EXEODIR)

clean:
	@echo Deleting $(OBJODIR)$(DIRSEP)
	$(QUIET)$(DELETE) $(OBJODIR)$(DIRSEP)*
	@echo Deleting $(LIBODIR)$(DIRSEP)
	$(QUIET)$(DELETE) $(LIBODIR)$(DIRSEP)*
	@echo Deleting $(EXEODIR)$(DIRSEP)
	$(QUIET)$(DELETE) $(EXEODIR)$(DIRSEP)*
