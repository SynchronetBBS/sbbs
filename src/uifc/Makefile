SRC_ROOT = ..
# Cross platform/compiler definitions
!include $(SRC_ROOT)\build\Common.bmake	# defines clean and output directory rules

CFLAGS = -DWRAPPER_IMPORTS $(CFLAGS) $(XPDEV-MT_CFLAGS) $(CIOLIB-MT_CFLAGS)

$(UIFCLIB): $(OBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
        &$(QUIET)tlib $@ +$**

$(UIFCLIB-MT): $(MT_OBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
        &$(QUIET)tlib $@ +$**
