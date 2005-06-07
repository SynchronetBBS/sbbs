UIFCTEST	=	$(EXEODIR)$(DIRSEP)uifctest$(EXEFILE)
UIFCLIB_BUILD	=	$(LIBODIR)$(DIRSEP)$(LIBPREFIX)uifc$(LIBFILE)
UIFCLIB-MT_BUILD	=	$(LIBODIR)$(DIRSEP)$(LIBPREFIX)uifc_mt$(LIBFILE)

all: lib mtlib test

test: xpdev-mt ciolib-mt mtlib $(EXEODIR) $(UIFCTEST)

lib: $(OBJODIR) $(LIBODIR) $(UIFCLIB_BUILD)
mtlib: $(MTOBJODIR) $(LIBODIR) $(UIFCLIB-MT_BUILD)

# Library dependencies.
$(UIFCTEST):	$(CIOLIB-MT) $(XPDEV-MT_LIB) $(UIFCLIB-MT) mtlib

