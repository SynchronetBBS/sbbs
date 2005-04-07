GAC_FC	=	$(EXEODIR)$(DIRSEP)gac_fc$(EXE_SUFFIX)$(EXEFILE)

all: xpdev-mt ciolib-mt $(MTOBJODIR) $(EXEODIR) $(GAC_FC)

$(GAC_FC):	$(XPDEV-MT_LIB) $(CIOLIB-MT)

