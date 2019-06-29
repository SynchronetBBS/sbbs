# hash/targets.mk
# $Id$

HASH_BUILD	=	$(LIBODIR)$(DIRSEP)$(LIBPREFIX)hash$(LIBFILE)

lib: $(OBJODIR) $(LIBODIR) $(HASH_BUILD)
