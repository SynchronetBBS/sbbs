UEDIT	=	$(EXEODIR)$(DIRSEP)uedit$(EXEFILE)

all: smblib uifc-mt ciolib-mt xpdev-mt $(MTOBJODIR) $(EXEODIR) $(UEDIT)

$(UEDIT):	$(SMBLIB) $(UIFCLIB-MT) $(CIOLIB-MT) $(XPDEV-MT_LIB)
