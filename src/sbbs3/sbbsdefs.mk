# sbbsdefs.mk

# C/C++ compiler preprocessor definitions for building SBBS.DLL

# $Id: sbbsdefs.mk,v 1.11 2010/03/12 01:32:02 deuce Exp $

SBBSDEFS=	-DSBBS -DSBBS_EXPORTS -DSMB_EXPORTS -DMD5_EXPORTS -DRINGBUF_SEM -DRINGBUF_EVENT -DRINGBUF_MUTEX -DUSE_CRYPTLIB
