# smblib/targets.mk
default: lib mtlib

lib:	$(LIBODIR) $(SMBLIB)
mtlib:	$(LIBODIR) $(SMBLIB-MT)
