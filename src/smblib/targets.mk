# smblib/targets.mk
SMBLIB	=	$(LIBODIR)$(DIRSEP)$(LIBPREFIX)smb$(LIBFILE)
default: lib mtlib

lib:	$(LIBODIR) $(SMBLIB)
mtlib:	$(LIBODIR) $(SMBLIB-MT)
