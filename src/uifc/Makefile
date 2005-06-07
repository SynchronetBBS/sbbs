SRC_ROOT = ..
# Cross platform/compiler definitions
!include $(SRC_ROOT)\build\Common.bmake	# defines clean and output directory rules

CFLAGS = -w-par -w-aus -w-csu $(CFLAGS) $(XPDEV-MT_CFLAGS) $(CIOLIB-MT_CFLAGS)

$(UIFCLIB_BUILD): $(OBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
        &$(QUIET)tlib $@ +$**

$(UIFCLIB-MT_BUILD): $(MT_OBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
        &$(QUIET)tlib $@ +$**

$(UIFCTEST): $(MTOBJODIR)$(DIRSEP)uifctest$(OFILE) $(MTOBJODIR)$(DIRSEP)filepick$(OFILE)
	@echo Creating $@ ...
	$(QUIET)$(CC) $(MT_LDFLAGS) $(UIFC-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(LDFLAGS) -e$@ $(MTOBJODIR)$(DIRSEP)filepick$(OFILE) $(MTOBJODIR)$(DIRSEP)uifctest$(OFILE) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)


