GAC_BJ	=	$(EXEODIR)$(DIRSEP)gac_bj$(EXE_SUFFIX)$(EXEFILE)
ENCRYPT =	$(EXEODIR)$(DIRSEP)encrypt$(EXE_SUFFIX)$(EXEFILE)
GAC_BJ_DST =	..$(DIRSEP)gac_bj.$(machine)$(EXEFILE)
ENCRYPT_DST =	..$(DIRSEP)encrypt.$(machine)$(EXEFILE)

all: xpdev-mt ciolib-mt $(MTOBJODIR) $(EXEODIR) $(GAC_BJ) release

$(GAC_BJ):	$(XPDEV-MT_LIB)

release: $(ENCRYPT)
