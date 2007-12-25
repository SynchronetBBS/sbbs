#ifndef _USERFILE_H_
#define _USERFILE_H_

#include "OpenDoor.h"	/* BOOL */
#include "dgnlnce.h"	/* struct playertype */

void writenextuser(struct playertype * plr, FILE * outfile);
void saveuser(struct playertype * plr);
BOOL loadnextuser(struct playertype * plr, FILE *infile);
BOOL loaduser(char *name, BOOL bbsname, struct playertype * plr);

#endif
