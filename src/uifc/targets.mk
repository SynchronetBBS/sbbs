all: lib mtlib

lib: $(OBJODIR) $(LIBODIR) $(UIFCLIB)
mtlib: $(MTOBJODIR) $(LIBODIR) $(UIFCLIB-MT)
