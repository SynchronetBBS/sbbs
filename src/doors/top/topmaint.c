#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <conwrap.h>
#include "top.h"

#define CLI_FULL        0
#define CLI_DELETE      1
#define CLI_PURGE       2
#define CLI_PACK        3
#define CLI_SETSEC      4
#define CLI_RENAME      5
#define CLI_DEFIX       6

user_data_typ ubuf;

int main(XINT argc, char *argv[]);
void showcmdline(XINT cmd);
void deleteuser(char *nam);
void packusers(void);
void purgeusers(unsigned XINT sec, unsigned XINT days);
XINT calc_days_ago(XDATE *lastdate);
void setusersec(unsigned XINT sec, char *nam);

int main(XINT argc, char *argv[])
{
    XINT d;

    printf("TOPMaint v2.00a - User maintenance utility for TOP v2.00\n");
    printf("By Paul Sidorsky, ISMWare\n\n");

    for (d = 1; d < argc; d++)
    {
        if (!stricmp(argv[d], "DELETE") && d != argc - 1)
        {
            if (!stricmp(argv[d + 1], "?"))
            {
                showcmdline(CLI_DELETE);
                exit(0);
            }
            deleteuser(argv[d + 1]);
            exit(0);
        }
        if (!stricmp(argv[d], "PURGE") && d != argc - 1)
        {
            unsigned XINT sec = 0, days = 0;

            if (!stricmp(argv[d + 1], "?"))
            {
                showcmdline(CLI_PURGE);
                exit(0);
            }
            while (++d < argc)
            {
                if (!strnicmp(argv[d], "/S", 2))
                {
                    sec = strtol(&argv[d][2], NULL, 10);
                }
                if (!strnicmp(argv[d], "/D", 2))
                {
                    days = strtol(&argv[d][2], NULL, 10);
                }
            }

            if (sec != 0 || days != 0)
            {
                purgeusers(sec, days);
                exit(0);
            }
        }
        if (!stricmp(argv[d], "PACK"))
        {
            if (d != argc - 1 && !stricmp(argv[d + 1], "?"))
            {
                showcmdline(CLI_PACK);
                exit(0);
            }
            packusers();
            exit(0);
        }
        if (!stricmp(argv[d], "SETSEC") && d != argc - 1)
        {
            if (!stricmp(argv[d + 1], "?"))
            {
                showcmdline(CLI_SETSEC);
                exit(0);
            }
            if (d < argc - 2)
            {
                setusersec(strtol(argv[d + 1], NULL, 10), argv[d + 2]);
                exit(0);
            }
        }
    }

    showcmdline(CLI_FULL);

}

void showcmdline(XINT cmd)
{

    if (cmd == CLI_FULL)
    {
        printf("TOPMaint Command Line:\n\n");
        printf("TOPMAINT <command> [<options>]\n\n");

        printf("    <command> is one of the following:\n\n");

        printf("*   DELETE          Removes a user from the user file.\n");
        printf("    PURGE           Purges users who no longer call the system.\n");
        printf("    PACK            Removes empty (deleted) user records from the user file.\n");
        printf("*   SETSEC          Sets a user's security level.\n");
//        printf("    RENAME          Changes a user's handle.\n");
//        printf("    DEFIX           Undoes the automatic name "fixing" (capitalization).\n");

        printf("\n    NOTE:  Commands marked with a * require user interaction and should not\n");
        printf("be run inside a batch file.\n\n");

        printf("    For help on any command, enter:\n\n");

        printf("TOPMAINT <command> ?\n");
    }
    if (cmd == CLI_DELETE)
    {
        printf("DELETE Command Syntax:\n\n");
        printf("TOPMAINT DELETE \"<user>\"\n\n");

        printf("    <user> is the name or partial name of the user you wish to delete.\n");
        printf("           If you wish to specify a first and last name, <user> must be\n");
        printf("           enclosed in quotation marks.\n\n");

        printf("    TOPMaint will scan users.top for users whos real name or handle match\n");
        printf("<user> and prompt for confirmation before deleting.  If you are specifying a\n");
        printf("partial name, pressing N will cause TOP to continue scanning the user file.\n\n");

        printf("Examples:\n\n");

        printf("TOPMAINT DELETE Simpson\n");
        printf("TOPMAINT DELETE \"Homer Simpson\"\n");
        printf("TOPMAINT DELETE \"Simp\"\n");
        printf("TOPMAINT DELETE \"Paul Sidorsky\"\n");
    }
    if (cmd == CLI_PURGE)
    {
        printf("PURGE Command Syntax:\n\n");
        printf("TOPMAINT PURGE [/S<sec>] [/D<days>]\n\n");

        printf("    <sec> is the minimum security a user must have to _NOT_ be purged.  Users\n");
        printf("          who have a security equal to or above the specified security will\n");
        printf("          not be deleted under any circumstances.  If not specified, 0 is\n");
        printf("          assumed (i.e. users will be deleted regardless of security).\n");
        printf("    <days> is the maximum number of days permitted since the user last entered\n");
        printf("           TOP.  Users who have not entered TOP in at least the specified\n");
        printf("           number of days will be deleted.  If not specified or set to 0, all\n");
        printf("           who's security is low enough will be deleted!\n\n");

        printf("Examples:\n\n");

        printf("TOPMAINT PURGE /S100\n");
        printf("TOPMAINT PURGE /D30\n");
        printf("TOPMAINT PURGE /S500 /D60\n");
        printf("TOPMAINT PURGE /D45 /S10\n");
    }
    if (cmd == CLI_PACK)
    {
        printf("PACK Command Syntax:\n\n");
        printf("TOPMAINT PACK\n\n");

        printf("    TOPMaint will remove all blank (deleted) user records from users.top.\n");
        printf("This operation should be run after a DELETE or PURGE command to compress the\n");
        printf("size of the user file and speed user searching in TOP.\n\n");

        printf("Example:\n\n");

        printf("TOPMAINT PACK\n");
    }
    if (cmd == CLI_SETSEC)
    {
        printf("SETSEC Command Syntax:\n\n");
        printf("TOPMAINT SETSEC <sec> \"<user>\"\n\n");

        printf("    <sec> is the new security you wish to assign, from 0 to 65535.\n");
        printf("    <user> is the name or partial name of the user you wish to delete.\n");
        printf("           If you wish to specify a first and last name, <user> must be\n");
        printf("           enclosed in quotation marks.\n\n");

        printf("    TOPMaint will scan users.top for users whos real name or handle match\n");
        printf("<user> and prompt for confirmation before changing the user's security.  If\n");
        printf("you are specifying a partial name, pressing N will cause TOP to continue\n");
        printf("scanning the user file.\n\n");

        printf("Examples:\n\n");

        printf("TOPMAINT SETSEC 20 Simpson\n");
        printf("TOPMAINT SETSEC 100 \"Homer Simpson\"\n");
        printf("TOPMAINT SETSEC 0 \"Simp\"\n");
        printf("TOPMAINT SETSEC 65535 \"Paul Sidorsky\"\n");
    }

}

void deleteuser(char *nam)
{
    XINT d;
    XINT nu;
    FILE *fil;
    char trn[51], tha[51];
    XINT key;

    printf("DELETE - Deleting \"%s\"...\n\n", nam);

    strupr(nam);

    fil = fopen("users.top", "r+b");
    if (fil == NULL)
    {
        printf("Can't open users.top!  Aborting...\n");
        return;
    }

    printf("Scanning users.top...     ");

    nu = filelength(fileno(fil)) / sizeof(user_data_typ);

    for (d = 0; d < nu; d++)
    {
        fseek(fil, (long) d * sizeof(user_data_typ), SEEK_SET);
        fread(&ubuf, sizeof(user_data_typ), 1, fil);
        strcpy(trn, ubuf.realname);
        strcpy(tha, ubuf.handle);
        strupr(trn);
        strupr(tha);
        if (strstr(trn, nam) || strstr(tha, nam))
        {
            printf("\b\b\b\b\b%5i", d);
            printf("\nDo you mean %s (%s)? ", ubuf.realname, ubuf.handle);
            key = toupper(getch());
            if (key == 'Y')
            {
                printf("Yes\n");
                strcpy(trn, ubuf.realname);
                memset(&ubuf, 0, sizeof(user_data_typ));
                fseek(fil, (long) d * sizeof(user_data_typ), SEEK_SET);
                fwrite(&ubuf, sizeof(user_data_typ), 1, fil);
                fclose(fil);
                printf("\"%s\" has been deleted!\n", trn);
                return;
            }
            printf("No\nScanning users.top...     ");
        }
        printf("\b\b\b\b\b%5i", d);
    }

    fclose(fil);

    printf("\nScanning completed.\n");

    printf("\nDeletion completed.\n");

}

void packusers(void)
{
    XINT d, e = 0, n = 0;
    XINT nu;
    FILE *ifil, *ofil;

    printf("PACK - Removing blank user records from users.top...\n\n");

    ifil = fopen("users.top", "rb");
    if (ifil == NULL)
    {
        printf("Can't open users.top!  Aborting...\n");
        return;
    }

    ofil = fopen("users.new", "wb");
    if (ofil == NULL)
    {
        printf("Can't open users.new!  Aborting...\n");
        return;
    }

    printf("Scanning users.top...\n");

    nu = filelength(fileno(ifil)) / sizeof(user_data_typ);

    for (d = 0; d < nu; d++)
    {
        printf("%5i", d);
        fseek(ifil, (long) d * sizeof(user_data_typ), SEEK_SET);
        fread(&ubuf, sizeof(user_data_typ), 1, ifil);
        if (ubuf.realname[0])
        {
            fseek(ofil, (long) e * sizeof(user_data_typ), SEEK_SET);
            fwrite(&ubuf, sizeof(user_data_typ), 1, ofil);
            e++;
            printf("\r");
        }
        else
        {
            n++;
            printf("\n");
        }
    }

    fclose(ifil);
    fclose(ofil);

    printf("Scanning completed.\n");
    printf("%i empty user records were removed.\n\n", n);
    printf("Deleting users.top...\n");

    unlink("users.top");

    printf("Renaming users.new to users.top...\n");

    rename("users.new", "users.top");

    printf("\nPack completed.\n");

}

void purgeusers(unsigned XINT sec, unsigned XINT days)
{
    XINT d, n = 0;
    XINT nu;
    FILE *fil;

    if (sec == 0)
    {
        sec = 65535;
    }

    printf("PURGE - Deleting users with security < %u who are idle at least %u days...\n\n",
           sec, days);

    fil = fopen("users.top", "r+b");
    if (fil == NULL)
    {
        printf("Can't open users.top!  Aborting...\n");
        return;
    }

    printf("Scanning users.top...\n");

    nu = filelength(fileno(fil)) / sizeof(user_data_typ);

    for (d = 0; d < nu; d++)
    {
        printf("\r%5i", d);
        fseek(fil, (long) d * sizeof(user_data_typ), SEEK_SET);
        fread(&ubuf, sizeof(user_data_typ), 1, fil);
        if (!ubuf.realname[0])
            {
            continue;
            }
        if (ubuf.security < sec && calc_days_ago(&ubuf.last_use) >= days)
        {
            printf(" - Deleting %s (%s)...\n%5i", ubuf.realname,
                   ubuf.handle, d);
            memset(&ubuf, 0, sizeof(user_data_typ));
            fseek(fil, (long) d * sizeof(user_data_typ), SEEK_SET);
            fwrite(&ubuf, sizeof(user_data_typ), 1, fil);
            n++;
        }
    }

    fclose(fil);

    printf("\nScanning completed.\n");
    printf("%i users were deleted.\n\n", n);

    printf("Purge completed.\n");

}

void setusersec(unsigned XINT sec, char *nam)
{
    XINT d;
    XINT nu;
    FILE *fil;
    char trn[51], tha[51];
    XINT key;

    printf("SETSEC - Setting security of \"%s\" to %u...\n\n", nam, sec);

    strupr(nam);

    fil = fopen("users.top", "r+b");
    if (fil == NULL)
    {
        printf("Can't open users.top!  Aborting...\n");
        return;
    }

    printf("Scanning users.top...     ");

    nu = filelength(fileno(fil)) / sizeof(user_data_typ);

    for (d = 0; d < nu; d++)
    {
        fseek(fil, (long) d * sizeof(user_data_typ), SEEK_SET);
        fread(&ubuf, sizeof(user_data_typ), 1, fil);
        strcpy(trn, ubuf.realname);
        strcpy(tha, ubuf.handle);
        strupr(trn);
        strupr(tha);
        if (strstr(trn, nam) || strstr(tha, nam))
        {
            printf("\b\b\b\b\b%5i", d);
            printf("\nDo you mean %s (%s)? ", ubuf.realname, ubuf.handle);
            key = toupper(getch());
            if (key == 'Y')
            {
                printf("Yes\n");
                ubuf.security = sec;
                fseek(fil, (long) d * sizeof(user_data_typ), SEEK_SET);
                fwrite(&ubuf, sizeof(user_data_typ), 1, fil);
                fclose(fil);
                printf("\"%s\" now has a security level of %u.\n",
                       ubuf.realname, sec);
                return;
            }
            printf("No\nScanning users.top...     ");
        }
        printf("\b\b\b\b\b%5i", d);
    }

    fclose(fil);

    printf("\nScanning completed.\n");

    printf("\nSetSec completed.\n");

}

XINT calc_days_ago(XDATE *lastdate)
{
	struct date thisdate;
	long d1 = 0, d2 = 0;
	long daysago = 0;
	XINT d;

	getdate(&thisdate);

	for (d = 1; d < lastdate->da_year; d++)
	{
		d1 += 365;
		if (d % 4 == 0)
		{
			d1++;
		}
    }
	for (d = 1; d < lastdate->da_mon; d++)
	{
	    switch(d)
    	{
        	case 1 : d1 += 31; break;
    	    case 2 :
				d1 += 28;
        	    if (lastdate->da_year % 4 == 0)
            	{
    	            d1++;
                }
				break;
        	case 3 : d1 += 31; break;
    	    case 4 : d1 += 30; break;
	        case 5 : d1 += 31; break;
        	case 6 : d1 += 30; break;
    	    case 7 : d1 += 31; break;
	        case 8 : d1 += 31; break;
        	case 9 : d1 += 30; break;
    	    case 10: d1 += 31; break;
	        case 11: d1 += 30; break;
		}
    }
	for (d = 1; d < lastdate->da_day; d++)
	{
	    d1++;
    }

	for (d = 1; d < thisdate.da_year; d++)
	{
		d2 += 365;
		if (d % 4 == 0)
		{
			d2++;
		}
	}
	for (d = 1; d < thisdate.da_mon; d++)
	{
	    switch(d)
    	{
	        case 1 : d2 += 31; break;
    	    case 2 :
				d2 += 28;
	            if (thisdate.da_year % 4 == 0)
            	{
        	        d2++;
    	        }
				break;
        	case 3 : d2 += 31; break;
    	    case 4 : d2 += 30; break;
	        case 5 : d2 += 31; break;
        	case 6 : d2 += 30; break;
    	    case 7 : d2 += 31; break;
	        case 8 : d2 += 31; break;
        	case 9 : d2 += 30; break;
    	    case 10: d2 += 31; break;
	        case 11: d2 += 30; break;
		}
    }
	for (d = 1; d < thisdate.da_day; d++)
	{
    	d2++;
    }

	daysago = d2 - d1;

	return daysago;
}
