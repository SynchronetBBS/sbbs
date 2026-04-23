ifdef win
SYNCTERM	=	$(EXEODIR)$(DIRSEP)syncterm$(SOFILE)
else
SYNCTERM	=	$(EXEODIR)$(DIRSEP)syncterm$(EXEFILE)
endif

# SFTP-MT and DeuceSSH are pulled in only when SSH support is compiled.
ifndef WITHOUT_DEUCESSH
 SYNCTERM_EXTRA_PREREQS := sftp-mt deucessh
 SYNCTERM_EXTRA_LIBS    := $(SFTPLIB-MT) $(DEUCESSH_LIB)
endif

all: botan xpdev-mt ciolib-mt uifc-mt $(SYNCTERM_EXTRA_PREREQS) $(MTOBJODIR) $(EXEODIR) $(SYNCTERM)

$(SYNCTERM):	$(XPDEV-MT_LIB) $(CIOLIB-MT) $(UIFCLIB-MT) $(ENCODE_LIB) $(HASH_LIB) $(SYNCTERM_EXTRA_LIBS)
