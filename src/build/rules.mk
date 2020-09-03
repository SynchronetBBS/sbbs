# build/rules.mk
#
# Global build targets for all make systems
#
# $Id: rules.mk,v 1.6 2011/10/21 21:44:27 deuce Exp $

$(OBJODIR):
	$(QUIET)$(IFNOTEXIST) mkdir $(OBJODIR)

$(MTOBJODIR):
	$(QUIET)$(IFNOTEXIST) mkdir $(MTOBJODIR)

$(LIBODIR):
	$(QUIET)$(IFNOTEXIST) mkdir $(LIBODIR)

$(EXEODIR):
	$(QUIET)$(IFNOTEXIST) mkdir $(EXEODIR)

clean:
clean:
	@echo Deleting $(OBJODIR)$(DIRSEP)
	-$(QUIET)$(DELETE) $(OBJODIR)$(DIRSEP)*
	@echo Deleting $(MTOBJODIR)$(DIRSEP)
	-$(QUIET)$(DELETE) $(MTOBJODIR)$(DIRSEP)*
	@echo Deleting $(LIBODIR)$(DIRSEP)
	-$(QUIET)$(DELETE) $(LIBODIR)$(DIRSEP)*
	@echo Deleting $(EXEODIR)$(DIRSEP)
	-$(QUIET)$(DELETE) $(EXEODIR)$(DIRSEP)*
