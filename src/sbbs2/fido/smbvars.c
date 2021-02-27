/* SMBVARS.C */

/*************************************************************************/
/* Global variables for SMBLIB. Same file used for both header and code. */
/*************************************************************************/

#ifndef GLOBAL
#define GLOBAL
#endif

#include "smbdefs.h"
#include <stdio.h>

GLOBAL char 	smb_file[128];	/* path and filename for SMB file (no ext) */
GLOBAL char 	shd_buf[SHD_BLOCK_LEN];
GLOBAL FILE 	*sdt_fp;
GLOBAL FILE 	*shd_fp;
GLOBAL FILE 	*sid_fp;
GLOBAL FILE 	*sda_fp;
GLOBAL FILE 	*sha_fp;
GLOBAL FILE 	*sch_fp;

