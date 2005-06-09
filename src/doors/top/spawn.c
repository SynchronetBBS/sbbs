#include "top.h"

/*  Copyright 1993 - 2000 Paul J. Sidorsky

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2, as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

char load_spawn_progs(void)
    {
    FILE *spfil;
    XINT d;
    unsigned char loadstr[513];

    spfil = fopen("spawn.cfg", "rt");
    if (spfil == NULL)
        {
        return 0;
        }

    rewind(spfil);
    while(!feof(spfil))
        {
        if (fgets(loadstr, 512, spfil) != NULL)
            {
            if (fgets(loadstr, 512, spfil) != NULL)
                {
                if (fgets(loadstr, 512, spfil) != NULL)
                    {
                    numspawnprog++;
                    }
                }
            }
        }

    spawnprog = malloc(numspawnprog * sizeof(spawn_prog_typ));
    if (spawnprog == NULL)
        {
        fclose(spfil);
        return 0;
        }

    rewind(spfil);

    for (d = 0; d < numspawnprog; d++)
        {
        memset(&spawnprog[d], 0, sizeof(spawn_prog_typ));
        fgets(loadstr, 512, spfil);
        stripcr(loadstr);
        strncpy(spawnprog[d].name, loadstr, 40);
        fgets(loadstr, 512, spfil);
        stripcr(loadstr);
        strncpy(spawnprog[d].cmds, loadstr, 80);
        fgets(loadstr, 512, spfil);
        stripcr(loadstr);
        strncpy(spawnprog[d].cmdline, loadstr, 128);
        }

    fclose(spfil);

    return 1;
    }

char check_spawn_cmds(XINT cmdwds)
    {
    XINT d, c, t;
    unsigned char cmdstr[256];

    if (cmdwds > 1)
        {
        return 0;
        }

    for (d = 0; d < numspawnprog; d++)
        {
        if (checkcmdmatch(get_word(0), spawnprog[d].cmds))
            {
            top_output(OUT_SCREEN, getlang("SpawnPrefix"));
            memset(cmdstr, 0, 256);
            for (c = 0, t = 0; c < strlen(spawnprog[c].cmdline); c++)
                {
                if (spawnprog[d].cmdline[c] == '*')
                    {
                    c++;
                    if (spawnprog[d].cmdline[c] == 'A');
                        {
                        strcat(cmdstr, od_control.user_handle);
                        t = strlen(cmdstr);
                        }
                    if (spawnprog[d].cmdline[c] == 'B');
                        {
                        ultoa(od_control.od_baud, outnum[0], 10);
                        strcat(cmdstr, outnum[0]);
                        t = strlen(cmdstr);
                        }
                    if (spawnprog[d].cmdline[c] == 'F');
                        {
                        strcat(cmdstr, );
                        t = strlen(cmdstr);
                        }
                    if (spawnprog[d].cmdline[c] == 'G');
                        {
                        itoa(od_control.user_ansi, outnum[0], 10);
                        strcat(cmdstr, outnum[0]);
                        t = strlen(cmdstr);
                        }
                    if (spawnprog[d].cmdline[c] == '');
                        {
                        strcat(cmdstr, );
                        t = strlen(cmdstr);
                        }
                    if (spawnprog[d].cmdline[c] == '');
                        {
                        strcat(cmdstr, );
                        t = strlen(cmdstr);
                        }
                    if (spawnprog[d].cmdline[c] == '');
                        {
                        strcat(cmdstr, );
                        t = strlen(cmdstr);
                        }
                    if (spawnprog[d].cmdline[c] == '');
                        {
                        strcat(cmdstr, );
                        t = strlen(cmdstr);
                        }
                    if (spawnprog[d].cmdline[c] == '');
                        {
                        strcat(cmdstr, );
                        t = strlen(cmdstr);
                        }
                    if (spawnprog[d].cmdline[c] == '');
                        {
                        strcat(cmdstr, );
                        t = strlen(cmdstr);
                        }
                    }
                }

            od_spawn(spawnprog[d].cmdline);
            top_output(OUT_SCREEN, getlang("SpawnSuffix"));
            return 1;
            }
        }

    return 0;
    }

