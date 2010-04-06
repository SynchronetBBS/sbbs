/* DSTSEDIT.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

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
	               ,*nv = "\nNew value: ", *p;
	int		file;
	stats_t		stats;
	time32_t		t;
	scfg_t		cfg;

	memset(&cfg, 0, sizeof(cfg));

	if (argc > 1)
		strcpy(path, argv[1]);
	else {
		p=getenv("SBBSCTRL");
		if(p)
			SAFECOPY(path, p);
		else
			getcwd(path, sizeof(path));
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
		printf("Error reading %u bytes from %s\r\n", sizeof(stats_t), str);
		exit(1);
	}
	close(file);
	while (1) {
		printf("Synchronet Daily Statistics Editor v1.01\r\n\r\n");
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
			if (str[0])
				t = dstrtounix(&cfg, str);
			break;
		case 'L':
			printf(nv);
			fgets(str, sizeof(str), stdin);
			if (str[0])
				stats.logons = atol(str);
			break;
		case 'O':
			printf(nv);
			fgets(str, sizeof(str), stdin);
			if (str[0])
				stats.ltoday = atol(str);
			break;
		case 'T':
			printf(nv);
			fgets(str, sizeof(str), stdin);
			if (str[0])
				stats.timeon = atol(str);
			break;
		case 'I':
			printf(nv);
			fgets(str, sizeof(str), stdin);
			if (str[0])
				stats.ttoday = atol(str);
			break;
		case 'U':
			printf(nv);
			fgets(str, sizeof(str), stdin);
			if (str[0])
				stats.uls = atol(str);
			break;
		case 'B':
			printf(nv);
			fgets(str, sizeof(str), stdin);
			if (str[0])
				stats.ulb = atol(str);
			break;
		case 'D':
			printf(nv);
			fgets(str, sizeof(str), stdin);
			if (str[0])
				stats.dls = atol(str);
			break;
		case 'W':
			printf(nv);
			fgets(str, sizeof(str), stdin);
			if (str[0])
				stats.dlb = atol(str);
			break;
		case 'P':
			printf(nv);
			fgets(str, sizeof(str), stdin);
			if (str[0])
				stats.ptoday = atol(str);
			break;
		case 'E':
			printf(nv);
			fgets(str, sizeof(str), stdin);
			if (str[0])
				stats.etoday = atol(str);
			break;
		case 'F':
			printf(nv);
			fgets(str, sizeof(str), stdin);
			if (str[0])
				stats.ftoday = atol(str);
			break;
		case 'N':
			printf(nv);
			fgets(str, sizeof(str), stdin);
			if (str[0])
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
