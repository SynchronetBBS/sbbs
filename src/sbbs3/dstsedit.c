/* $Id: dstsedit.c,v 1.8 2020/01/03 20:59:58 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#include "sbbs.h"
#include "dirwrap.h"
#include "nopen.h"
#include "sbbsdefs.h"
#include "conwrap.h"

int 
main(int argc, char **argv)
{
	char		ch, str[MAX_PATH+1], path[MAX_PATH + 1]
	               ,*lst = "%c) %-25s: %13lu\n"
	               ,*nv = "\nNew value: ";
	int		file;
	stats_t		stats;
	time32_t		t;
	scfg_t		cfg;

	memset(&cfg, 0, sizeof(cfg));

	if (argc > 1) {
		SAFECOPY(path, argv[1]);
	} else {
		SAFECOPY(path, get_ctrl_dir());
	}
	backslash(path);

	sprintf(str, "%sdsts.dab", path);
	if ((file = nopen(str, O_RDONLY)) == -1) {
		printf("Can't open %s\r\n", str);
		exit(1);
	}
	read(file, &t, 4L);
	if (read(file, &stats, sizeof(stats_t)) != sizeof(stats_t)) {
		close(file);
		printf("Error reading %" XP_PRIsize_t "u bytes from %s\r\n", sizeof(stats_t), str);
		exit(1);
	}
	close(file);
	while (1) {
		printf("Synchronet Daily Statistics Editor v1.02\r\n\r\n");
		printf("S) %-25s: %13s\n", "Date Stamp (MM/DD/YY)", unixtodstr(&cfg, t, str));
		printf(lst, 'L', "Total Logons", stats.logons);
		printf(lst, 'O', "Logons Today", stats.ltoday);
		printf(lst, 'T', "Total Time on", stats.timeon);
		printf(lst, 'I', "Time on Today", stats.ttoday);
		printf(lst, 'U', "Uploaded Files Today", stats.uls);
		printf(lst, 'B', "Uploaded Bytes Today", stats.ulb);
		printf(lst, 'D', "Downloaded Files Today", stats.dls);
		printf(lst, 'W', "Downloaded Bytes Today", stats.dlb);
		printf(lst, 'P', "Posts Today", stats.ptoday);
		printf(lst, 'E', "E-Mails Today", stats.etoday);
		printf(lst, 'F', "Feedback Today", stats.ftoday);
		printf("%c) %-25s: %13u\r\n", 'N', "New Users Today", stats.nusers);

		printf("Q) Quit and save changes\r\n");
		printf("X) Quit and don't save changes\r\n");

		printf("\r\nWhich: ");

		ch = toupper(getch());
		printf("%c\r\n", ch);

		switch (ch) {
		case 'S':
			printf("Date stamp (MM/DD/YY): ");
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				t = dstrtounix(&cfg, str);
			break;
		case 'L':
			fputs(nv,stdout);
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				stats.logons = atol(str);
			break;
		case 'O':
			fputs(nv,stdout);
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				stats.ltoday = atol(str);
			break;
		case 'T':
			fputs(nv,stdout);
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				stats.timeon = atol(str);
			break;
		case 'I':
			fputs(nv,stdout);
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				stats.ttoday = atol(str);
			break;
		case 'U':
			fputs(nv,stdout);
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				stats.uls = atol(str);
			break;
		case 'B':
			fputs(nv,stdout);
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				stats.ulb = atol(str);
			break;
		case 'D':
			fputs(nv,stdout);
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				stats.dls = atol(str);
			break;
		case 'W':
			fputs(nv,stdout);
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				stats.dlb = atol(str);
			break;
		case 'P':
			fputs(nv,stdout);
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				stats.ptoday = atol(str);
			break;
		case 'E':
			fputs(nv,stdout);
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				stats.etoday = atol(str);
			break;
		case 'F':
			fputs(nv,stdout);
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				stats.ftoday = atol(str);
			break;
		case 'N':
			fputs(nv,stdout);
			fgets(str, sizeof(str), stdin);
			if (isdigit(str[0]))
				stats.nusers = atoi(str);
			break;
		case 'Q':
			sprintf(str, "%sdsts.dab", path);
			if ((file = nopen(str, O_WRONLY)) == -1) {
				printf("Error opening %s\r\n", str);
				exit(1);
			}
			write(file, &t, 4L);
			write(file, &stats, sizeof(stats_t));
			close(file);
		case 'X':
			exit(0);
		default:
			putchar(7);
			break;
		}
	}
}
