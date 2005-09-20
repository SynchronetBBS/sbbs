/******************************************************************************
  DPCLEAN.EXE: Clean up program for Domain Poker (DPOKER.EXE) online
  multi-player poker BBS door game for Synchronet (and other) BBS software.
  Copyright (C) 1992-2005 Allen Christiansen DBA Domain Entertainment.

  This program is free software; you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation; either version 2 of the License, or (at your option) any later
  version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 Temple
  Place - Suite 330, Boston, MA  02111-1307, USA.

  E-Mail      : door.games@domainentertainment.com
  Snail Mail  : Domain Entertainment
                PO BOX 54452
                Irvine, CA 92619-4452
******************************************************************************/

#define DPCLEAN

#include "dpoker.c"

/***********************************************************
 Wow! Look at all this code to make a cleanup program <grin>
************************************************************/
int main(int argc, char **argv)
{
    char str[128];
    int x;

    sprintf(node_dir,"%s",getenv("SBBSNODE"));
    if(node_dir[strlen(node_dir)-1]!='\\')
        strcat(node_dir,"\\");
    if(!stricmp(argv[1],"/?")) {
        printf("\r\nDomain Poker Clean-Up  Copyright 2005 Domain "
                "Entertainment");
        printf("\r\n\r\nUsage: DPCLEAN [/options]\r\n");
        printf("\r\nOptions: DAILY = Perform daily clean-up");
        return(0);
    }
    for (x=1; x<argc; x++) {
        if (!strchr(argv[x],'/')) {
            strcpy(node_dir,argv[x]);
            if (node_dir[strlen(node_dir)-1]!='\\')
                strcat(node_dir,"\\"); } }

    initdata();
    sprintf(str,"player.%d",node_num);
    if (!fexist(str)) return;
    else {
        get_player(node_num);
        newpts=temppts;
        quit_out();
    }
    return(0);
}
