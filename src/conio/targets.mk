# conio/targets.mk
CIOLIB-MT	=	$(LIBODIR)$(DIRSEP)$(LIBPREFIX)ciolib-mt$(LIBFILE)
default: mtlib

mtlib:	$(LIBODIR) $(CIOLIB-MT)
