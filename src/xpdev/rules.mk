# Implicit C Compile Rule
$(LIBODIR)/%.o : %.c $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) -o $@ -c $<

# Implicit C++ Compile Rule
$(LIBODIR)/%.o : %.cpp $(BUILD_DEPENDS)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CXX) $(CFLAGS) -o $@ -c $<

$(LIBODIR):
	@echo Creating $(LIBODIR)
	$(QUIET)mkdir $(LIBODIR)

$(EXEODIR):
	@echo Creating $(LIBODIR)
	$(QUIET)mkdir $(EXEODIR)

clean:
	@echo Deleting $(LIBODIR)$(SLASH)
	$(QUIET)$(DELETE) $(LIBODIR)$(SLASH)*
	@echo Deleting $(EXEODIR)$(SLASH)
	$(QUIET)$(DELETE) $(EXEODIR)$(SLASH)*

