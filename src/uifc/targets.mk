all: lib mtlib

lib: $(LIBODIR) $(UIFCLIB)
mtlib: $(LIBODIR) $(UIFCLIB-MT)
