SFTPLIB-MT  =       $(SFTP_SRC)$(DIRSEP)$(LIBODIR)$(DIRSEP)$(LIBPREFIX)sftp_mt$(LIBFILE)

SFTP-MT_CFLAGS   =       -I$(SFTP_SRC)
SFTP-MT_LDFLAGS  =       -L$(SFTP_SRC)$(DIRSEP)$(LIBODIR)
SFTP-MT_LIBS	=	$(UL_PRE)sftp_mt$(UL_SUF)
