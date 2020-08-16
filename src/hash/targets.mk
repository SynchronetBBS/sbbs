# hash/targets.mk
# $Id: targets.mk,v 1.1 2019/06/29 01:14:55 rswindell Exp $

HASH_BUILD	=	$(LIBODIR)$(DIRSEP)$(LIBPREFIX)hash$(LIBFILE)

lib: $(OBJODIR) $(LIBODIR) $(HASH_BUILD)
