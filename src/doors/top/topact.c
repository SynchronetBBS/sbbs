
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

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <filewrap.h>
#include <genwrap.h>

#ifdef __OS2__
#undef random
#define random(xxx) od_random(xxx)
#endif

#if defined(__OS2__) || defined(__WIN32__) || defined(__unix__)
#define XFAR
#define XINT short int
#else
#define XFAR far
#define XINT int
#endif

#define crtest(vvvv) if (vvvv[strlen(vvvv) - 1] != 10)\
                         res = 1;
#define stripcr(vvvv) if (vvvv[strlen(vvvv) - 1] == 10)\
                          vvvv[strlen(vvvv) - 1] = 0

typedef struct
    {
    unsigned long datstart; /* Location of first action's data. */
    unsigned long txtstart; /* Location of first action's text. */
    unsigned char name[31];
    unsigned XINT minsec;
    unsigned XINT maxsec;
    unsigned long minchannel;
    unsigned long maxchannel;
    unsigned long numactions;
    } action_file_typ;

typedef struct
    {
    char          type;   /* -1 = deleted */
    unsigned char verb[11];
    unsigned long textofs; /* From txtstart */
    unsigned XINT responselen; /* 0 = N/A */
    unsigned XINT singularlen; /* 0 = N/A */
    unsigned XINT plurallen;  /* 0 = N/A */
    } action_data_typ;

typedef struct
    {
    unsigned char *responsetext;
    unsigned char *singulartext;
    unsigned char *pluraltext;
    } action_ptr_typ;

int main(XINT argc, char *argv[]);
void actfile_compile(unsigned char *ifname, unsigned char *ofname);

unsigned char loadbuf[513];
unsigned char iname[256], oname[256];

int main(XINT argc, char *argv[])
    {

    printf("\nTOPACT - Action Compiler for TOP 2.00.\n\n");

    if (argc < 2 || argc > 3)
        {
        printf("Command line error!  Syntax:\n\n");
        printf("TOPACT <infile> [<outfile>]\n\n");
        printf("<infile> is the file to compile.  If an extension is not "
               "specified, .ACT will\n         be used.\n");
        printf("<outfile> is the file to output the compiled information "
               "to.  If an extension\n          is not specified, .TAC ");
        printf("be used.  If <outfile> is not specified, the\n          "
               "base name of <infile> will be used with the extension of "
               ".TAC.\n");
        return(0);
        }

    strcpy(iname, argv[1]);
    if (!strchr(iname, '.'))
        {
        strcat(iname, ".act");
        }
/*    strupr(iname); */

    if (argc == 2)
        {
        strcpy(oname, iname);
        if (strchr(oname, '.'))
            {
            strcpy(strchr(oname, '.'), ".tac");
            }
        }
    else
        {
        strcpy(oname, argv[2]);
        if (!strchr(oname, '.'))
            {
            strcat(oname, ".tac");
            }
        }
/*    strupr(oname); */

    actfile_compile(iname, oname);

    return(0);

    }

void actfile_compile(unsigned char *ifname, unsigned char *ofname)
    {
    FILE *ifil = NULL;
    XINT ofil;
    action_file_typ afile;
    action_data_typ tact;
    unsigned long count = 0, dloc = 0, tloc = 0, d;
    XINT res;

    printf("--- BEGIN COMPILE ---\n");
    printf("Opening %s...", ifname);

    ifil = fopen(ifname, "r+t");
    if (!ifil)
        {
        printf("ERROR!\nCan't open %s!\n", ifname);
        return;
        }

    printf("Done!\nOpening %s...", ofname);

    if (!access(ofname, 0))
        {
        printf("WARNING!\nFile already exists!  Making backup...");
        strcpy(loadbuf, ofname);
        if (strchr(loadbuf, '.'))
            {
            strcpy(strchr(loadbuf, '.'), ".bak");
            }
        else
            {
            strcat(loadbuf, ".bak");
            }
        /* strupr(loadbuf); */
        unlink(loadbuf);
        rename(ofname, loadbuf);
        printf("Done!\nBacked up to %s.\nOpening %s...", loadbuf,
               ofname);
        }

    ofil = sopen(ofname, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                 S_IREAD | S_IWRITE);
    if (ofil == -1)
        {
        printf("ERROR!\nCan't open %s!\n", ofname);
        fclose(ifil);
        return;
        }

    printf("Done!\n--- PASS 1 ---\n");
    printf("Getting configuration information...");

    res = 0;
    res += (fgets(loadbuf, 512, ifil) == NULL);
    crtest(loadbuf);
    stripcr(loadbuf);
    if (strlen(loadbuf) > 30)
        {
        printf("WARNING!\nList Name too long - truncated to 30 "
               "characters.\n");
        printf("Continuing to get configuration information...");
        }
    strncpy(afile.name, loadbuf, 30);
    afile.name[31] = '\0';
    res += (fgets(loadbuf, 512, ifil) == NULL);
    crtest(loadbuf);
    afile.minsec = strtoul(loadbuf, NULL, 10);
    if (!strnicmp(loadbuf, "MINSEC", 6))
        {
        afile.minsec = 0;
        }
    res += (fgets(loadbuf, 512, ifil) == NULL);
    crtest(loadbuf);
    afile.maxsec = strtoul(loadbuf, NULL, 10);
    if (!strnicmp(loadbuf, "MAXSEC", 6))
        {
        afile.maxsec = 65535;
        }
    res += (fgets(loadbuf, 512, ifil) == NULL);
    crtest(loadbuf);
    afile.minchannel = strtoul(loadbuf, NULL, 10);
    if (!strnicmp(loadbuf, "MINCHA", 6))
        {
        afile.minchannel = 1UL;
        }
    if (!strnicmp(loadbuf, "MINPUB", 6))
        {
        afile.minchannel = 1UL;
        }
    if (!strnicmp(loadbuf, "MINPER", 6))
        {
        afile.minchannel = 4000000000UL;
        }
    if (!strnicmp(loadbuf, "MINCON", 6))
        {
        afile.minchannel = 4001000000UL;
        }
    res += (fgets(loadbuf, 512, ifil) == NULL);
    crtest(loadbuf);
    afile.maxchannel = strtoul(loadbuf, NULL, 10);
    if (!strnicmp(loadbuf, "MAXPUB", 6))
        {
        afile.maxchannel = 3999999999UL;
        }
    if (!strnicmp(loadbuf, "MAXPER", 6))
        {
        afile.maxchannel = 4000999999UL;
        }
    if (!strnicmp(loadbuf, "MAXCON", 6))
        {
        afile.maxchannel = 0xFFFFFFFEUL;
        }
    if (!strnicmp(loadbuf, "MAXCHA", 6))
        {
        afile.maxchannel = 0xFFFFFFFEUL;
        }
    if (res)
        {
        printf("ERROR!\nCan't read configuration information!\n");
        fclose(ifil);
        close(ofil);
        return;
        }

    printf("Done!\nName: \"%s\"  MinSec: %u  MaxSec = %u\n", afile.name,
           afile.minsec, afile.maxsec);
    printf("MinChan: %lu  MaxChan: %lu\n", afile.minchannel,
           afile.maxchannel);

    afile.datstart = (long) sizeof(action_file_typ);

    tloc = (long) sizeof(action_file_typ);

    printf("Counting actions...%5i", count);

    while(!feof(ifil))
        {
        res = 0;
        res += (fgets(loadbuf, 512, ifil) == NULL);
        crtest(loadbuf);
        res += (fgets(loadbuf, 512, ifil) == NULL);
        crtest(loadbuf);
        res += (fgets(loadbuf, 512, ifil) == NULL);
        crtest(loadbuf);
        res += (fgets(loadbuf, 512, ifil) == NULL);
        crtest(loadbuf);
        res += (fgets(loadbuf, 512, ifil) == NULL);
        crtest(loadbuf);
        if (!res)
            {
            tloc += (long) sizeof(action_data_typ);
            printf("\b\b\b\b\b%5i", ++count);
            }
        }

    afile.txtstart = tloc;
    afile.numactions = count;

    printf("...Done!\nWriting configuration information...");

    lseek(ofil, 0, SEEK_SET);
    write(ofil, &afile, sizeof(action_file_typ));

    printf("Done!\n--- PASS 2 ---\n");
    printf("Skipping configuration information (not needed on second "
           "pass)...");

    dloc = 0;
    tloc = 0;
    fseek(ifil, 0, SEEK_SET);

    res = 0;
    res += (fgets(loadbuf, 512, ifil) == NULL);
    crtest(loadbuf);
    res += (fgets(loadbuf, 512, ifil) == NULL);
    crtest(loadbuf);
    res += (fgets(loadbuf, 512, ifil) == NULL);
    crtest(loadbuf);
    res += (fgets(loadbuf, 512, ifil) == NULL);
    crtest(loadbuf);
    res += (fgets(loadbuf, 512, ifil) == NULL);
    crtest(loadbuf);
    if (res)
        {
        printf("ERROR!\nCan't skip configuration information!\n");
        fclose(ifil);
        close(ofil);
        return;
        }

    d = 0;
    printf("Done!\nReading actions...%5i", d);

    for (; d < afile.numactions; d++)
        {
        res = 0;
        res = (fgets(loadbuf, 512, ifil) == NULL);
        crtest(loadbuf);
        if (res)
            {
            printf("...ERROR!\nCan't read type for action #%i!\n",
                   d + 1);
            printf("Continuing to compile actions...%5i", d + 1);
            continue;
            }
        tact.type = -1;
        if (!strnicmp(loadbuf, "NOR", 3))
            {
            tact.type = 0;
            }
        if (!strnicmp(loadbuf, "TAL", 3))
            {
            tact.type = 1;
            }
        res = (fgets(loadbuf, 512, ifil) == NULL);
        crtest(loadbuf);
        if (res)
            {
            printf("...ERROR!\nCan't read verb for action #%i!\n",
                   d + 1);
            printf("Continuing to compile actions...%5i", d + 1);
            continue;
            }
        stripcr(loadbuf);
        if (strlen(loadbuf) > 10)
            {
            printf("...WARNING!\nVerb \"%s\" is too long.\n", loadbuf);
            printf("Truncated to 10 characters.\n");
            printf("Continuing to compile actions...%5i", d);
            }
        strncpy(tact.verb, loadbuf, 10);
        tact.verb[10] = '\0';
        if (tact.type == -1)
            {
            printf("...WARNING!\nInvalid action type for \"%s\" action!\n",
                   tact.verb);
            printf("Continuing to compile actions...%5i", d);
            }
        tact.textofs = tloc;
        res = (fgets(loadbuf, 512, ifil) == NULL);
        crtest(loadbuf);
        if (res)
            {
            printf("...ERROR!\nCan't read response text for action #%i!\n",
                   d + 1);
            printf("Continuing to compile actions...%5i", d + 1);
            continue;
            }
        stripcr(loadbuf);
        if (!stricmp(loadbuf, "N/A"))
            {
            loadbuf[0] = '\0';
            }
        tact.responselen = strlen(loadbuf);
        lseek(ofil, afile.txtstart + tloc, SEEK_SET);
        res = write(ofil, loadbuf, tact.responselen);
        if (res == -1)
            {
            printf("ERROR!\nCan't write response text for action #%i!\n",
                   d + 1);
            printf("Continuing to compile actions...%5i", d + 1);
            continue;
            }
        tloc += (long) tact.responselen;

        res = (fgets(loadbuf, 512, ifil) == NULL);
        crtest(loadbuf);
        if (res)
            {
            printf("...ERROR!\nCan't read singular text for action #%i!\n",
                   d + 1);
            printf("Continuing to compile actions...%5i", d + 1);
            continue;
            }
        stripcr(loadbuf);
        if (!stricmp(loadbuf, "N/A"))
            {
            loadbuf[0] = '\0';
            }
        tact.singularlen = strlen(loadbuf);
        lseek(ofil, afile.txtstart + tloc, SEEK_SET);
        res = write(ofil, loadbuf, tact.singularlen);
        if (res == -1)
            {
            printf("ERROR!\nCan't write singular text for action #%i!\n",
                   d + 1);
            printf("Continuing to compile actions...%5i", d + 1);
            continue;
            }
        tloc += (long) tact.singularlen;

        res = (fgets(loadbuf, 512, ifil) == NULL);
        crtest(loadbuf);
        if (res)
            {
            printf("...ERROR!\nCan't read plural text for action #%i!\n",
                   d + 1);
            printf("Continuing to compile actions...%5i", d + 1);
            continue;
            }
        stripcr(loadbuf);
        if (!stricmp(loadbuf, "N/A"))
            {
            loadbuf[0] = '\0';
            }
        tact.plurallen = strlen(loadbuf);
        lseek(ofil, afile.txtstart + tloc, SEEK_SET);
        res = write(ofil, loadbuf, tact.plurallen);
        tloc += (long) tact.plurallen;
        if (res == -1)
            {
            printf("ERROR!\nCan't write plural text for action #%i!\n",
                   d + 1);
            printf("Continuing to compile actions...%5i", d + 1);
            continue;
            }

        lseek(ofil, afile.datstart + dloc, SEEK_SET);
        res = write(ofil, &tact, sizeof(action_data_typ));
        if (res == -1)
            {
            printf("ERROR!\nCan't write action data for action #%i!\n",
                   d + 1);
            printf("Continuing to compile actions...%5i", d + 1);
            continue;
            }
        dloc += sizeof(action_data_typ);
        printf("\b\b\b\b\b%5i", d + 1);
        }

    fclose(ifil);
    close(ofil);

    printf("...Done!\n--- COMPILE COMPLETED ---\n");

    }
