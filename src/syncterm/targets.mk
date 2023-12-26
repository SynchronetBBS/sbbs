SYNCTERM	=	$(EXEODIR)$(DIRSEP)syncterm$(EXEFILE)

all: xpdev-mt ciolib-mt uifc-mt sftp-mt $(MTOBJODIR) $(EXEODIR) $(SYNCTERM)

$(SYNCTERM):	$(XPDEV-MT_LIB) $(SFTP-MT) $(CIOLIB-MT) $(UIFCLIB-MT) $(ENCODE_LIB) $(HASH_LIB)
