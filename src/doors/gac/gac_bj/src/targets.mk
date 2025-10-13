GAC_BJ	=	$(EXEODIR)$(DIRSEP)gac_bj$(EXE_SUFFIX)$(EXEFILE)
GAC_BJ_DST =	..$(DIRSEP)gac_bj.$(machine)$(EXEFILE)

all: xpdev-mt ciolib-mt $(MTOBJODIR) $(EXEODIR) $(GAC_BJ)

$(GAC_BJ):	$(XPDEV-MT_LIB)
