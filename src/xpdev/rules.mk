$(LIBODIR):
	@echo Creating $(LIBODIR)
	$(QUIET)mkdir $(LIBODIR)

$(EXEODIR):
	@echo Creating $(EXEODIR)
	$(QUIET)mkdir $(EXEODIR)

clean:
	@echo Deleting $(LIBODIR)$(SLASH)
	$(QUIET)$(DELETE) $(LIBODIR)$(SLASH)*
	@echo Deleting $(EXEODIR)$(SLASH)
	$(QUIET)$(DELETE) $(EXEODIR)$(SLASH)*

