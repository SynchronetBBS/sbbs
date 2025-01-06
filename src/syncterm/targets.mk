ifdef win
SYNCTERM	=	$(EXEODIR)$(DIRSEP)syncterm$(SOFILE)
else
SYNCTERM	=	$(EXEODIR)$(DIRSEP)syncterm$(EXEFILE)
endif

all: xpdev-mt ciolib-mt uifc-mt sftp-mt $(MTOBJODIR) $(EXEODIR) $(SYNCTERM)

$(SYNCTERM):	$(XPDEV-MT_LIB) $(SFTPLIB-MT) $(CIOLIB-MT) $(UIFCLIB-MT) $(ENCODE_LIB) $(HASH_LIB)
