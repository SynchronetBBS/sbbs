$(LIBODIR):
	$(QUIET)$(IFNOTEXIST) mkdir $(LIBODIR)

$(EXEODIR):
	$(QUIET)$(IFNOTEXIST) mkdir $(EXEODIR)

clean:
	@echo Deleting $(LIBODIR)$(SLASH)
	$(QUIET)$(DELETE) $(LIBODIR)$(SLASH)*
	@echo Deleting $(EXEODIR)$(SLASH)
	$(QUIET)$(DELETE) $(EXEODIR)$(SLASH)*

