/* Statistics Log Viewer */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"
#include "dat_file.h"

/****************************************************************************/
/* Lists system statistics for everyday the bbs has been running.           */
/* Either for the current node (node=1) or the system (node=0)              */
/****************************************************************************/
int main(int argc, char **argv)
{
	char       str[256], dir[256] = {""}, *p;
	FILE*      fp;
	int        i, lncntr = 0, rows = 20;
	uint       days = 0;
	uint       count = 0;
	bool       pause = false;
	time_t     timestamp;
	stats_t    stats = {0};
	totals_t   total = {0};
	bool       totals = false;
	time_t     yesterday;
	struct tm* tm;

	ZERO_VAR(total);
	printf("\nSynchronet System/Node Statistics Log Viewer v2.0\n\n");

	for (i = 1; i < argc; i++) {
		if (stricmp(argv[i], "/P") == 0 || strncmp(argv[i], "-p", 2) == 0) {
			pause = true;
			if (IS_DIGIT(argv[i][2]))
				rows = atoi(argv[i] + 2);
		}
		else if (strcmp(argv[i], "-t") == 0)
			totals = true;
		else if (strncmp(argv[i], "-d", 2) == 0)
			days = atoi(argv[i] + 2);
		else if (isdir(argv[i]))
			SAFECOPY(dir, argv[i]);
		else {
			fprintf(stderr, "usage: %s [-opt [...]] [path]\n"
			        "\n"
			        " opts:\n"
			        "\t-p[rows]  enable screen pause prompt\n"
			        "\t-d<days>  specify maximum number of days\n"
			        "\t-t        display totals\n"
			        , argv[0]
			        );
			return EXIT_SUCCESS;
		}
	}
	if (!dir[0]) {
		p = getenv("SBBSCTRL");
		if (p == NULL)
			p = SBBSCTRL_DEFAULT;
		SAFECOPY(dir, p);
	}

	backslash(dir);

	SAFEPRINTF(str, "%scsts.tab", dir);
	if (!fexistcase(str)) {
		fprintf(stderr, "%s does not exist\n", str);
		return EXIT_FAILURE;
	}
	if ((fp = fopen(str, "rb")) == NULL) {
		fprintf(stderr, "Error %d opening %s\n", errno, str);
		return EXIT_FAILURE;
	}

	str_list_t  columns = NULL;
	str_list_t* records = tabReadFile(fp, &columns);
	fclose(fp);

	long        l;
	COUNT_LIST_ITEMS(records, l);
	for (--l; l >= 0; --l) {
		parse_cstats(records[l], &stats);
		timestamp = isoDateTime_to_time(strtoul(records[l][CSTATS_DATE], NULL, 10), 0);
		yesterday = timestamp - (24 * 60 * 60);   /* 1 day less than stamp */
		tm = localtime(&yesterday);
		printf("%02d/%02d/%02d", tm->tm_year % 100, tm->tm_mon + 1, tm->tm_mday);
		printf(" T:%4u  L:%3u  P:%3u  E:%3u  F:%3u"
		       , stats.ttoday, stats.ltoday, stats.ptoday, stats.etoday, stats.ftoday);
		printf("  U:%5s %5u"
		       , byte_estimate_to_str(stats.ulb, str, sizeof(str), 1, 0), stats.uls);
		printf("  D:%5s %5u"
		       , byte_estimate_to_str(stats.dlb, str, sizeof(str), 1, 0), stats.dls);
		printf("  N:%2u\n", stats.nusers);
		total.timeon += stats.ttoday;
		total.logons += stats.ltoday;
		total.posts += stats.ptoday;
		total.email += stats.etoday;
		total.fbacks += stats.ftoday;
		total.uls += stats.uls;
		total.ulb += stats.ulb;
		total.dls += stats.dls;
		total.dlb += stats.dlb;
		total.nusers += stats.nusers;
		lncntr++;
		count++;
		if (days && count >= days)
			break;
		if (pause && lncntr >= rows) {
			printf("More (Y/n) ? ");
			fflush(stdout);
			int ch = getchar();
			if (ch == CTRL_C || toupper(ch) == 'N')
				break;
			printf("\r");
			lncntr = 0;
		}
	}

	if (totals) {
		printf("\n");
		printf("%10s\n", "===TOTAL===");
		char sep = ',';
		printf("%10s : %13s\n", columns[CSTATS_TIMEON], minutes_to_str(total.timeon, str, sizeof str, /* estimate: */false, /* verbose: */false));
		printf("%10s : %13s\n", columns[CSTATS_LOGONS], u32toac(total.logons, str, sep));
		printf("%10s : %13s\n", columns[CSTATS_POSTS], u32toac(total.posts, str, sep));
		printf("%10s : %13s\n", "Email Sent", u32toac(total.email, str, sep));
		printf("%10s : %13s\n", "Feedbacks", u32toac(total.fbacks, str, sep));
		printf("%10s : %13s\n", "New Users", u32toac(total.nusers, str, sep));
		printf("%10s : %7s bytes", "Uploads", byte_estimate_to_str(total.ulb, str, sizeof(str), 1, 1));
		printf(" in %s files\n", u32toac(total.uls, str, sep));
		printf("%10s : %7s bytes", "Downloads", byte_estimate_to_str(total.dlb, str, sizeof(str), 1, 1));
		printf(" in %s files\n", u32toac(total.dls, str, sep));
	}
	return EXIT_SUCCESS;
}
