# smblib/targets.mk
SMBLIB	=	$(LIBODIR)$(DIRSEP)$(LIBPREFIX)smb$(LIBFILE)
default: $(OBJODIR) $(LIBODIR) $(SMBLIB)
