UMONITOR	=	$(EXEODIR)$(DIRSEP)umonitor$(EXEFILE)

all: ciolib-mt uifc-mt xpdev-mt smblib $(MTOBJODIR) $(EXEODIR) $(UMONITOR)

$(UMONITOR):	$(CIOLIB-MT) $(UIFCLIB-MT) $(XPDEV-MT_LIB) $(SMBLIB)
