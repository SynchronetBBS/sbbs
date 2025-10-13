GAC_WH	=	$(EXEODIR)$(DIRSEP)gac_wh$(EXE_SUFFIX)$(EXEFILE)
GAC_WH_DST =	..$(DIRSEP)gac_wh.$(machine)$(EXEFILE)

all: xpdev-mt ciolib-mt $(MTOBJODIR) $(EXEODIR) $(GAC_WH)

$(GAC_WH):	$(XPDEV-MT_LIB)

