GAC_BJ	=	$(EXEODIR)$(DIRSEP)gac_bj$(EXEFILE)

all: xpdev-mt ciolib-mt $(MTOBJODIR) $(EXEODIR) $(GAC_BJ)

$(GAC_BJ):	$(XPDEV-MT_LIB) $(CIOLIB-MT)

