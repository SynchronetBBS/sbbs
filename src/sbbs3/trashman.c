/* Synchronet client/content-filtering (trashcan/twit) maintenance */

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

#include "trash.h"
//#include "datewrap.h"
//#include "xpdatetime.h"
//#include "ini_file.h"
//#include "scfglib.h"
#include "findstr.h"
#include "nopen.h"

int verbosity = 0;

int maint(const char* fname)
{
	int removed = 0;
	str_list_t list = findstr_list(fname);
	if(list == NULL) {
		perror(fname);
		return -1;
	}
	FILE* fp = fnopen(NULL, fname, O_WRONLY|O_TRUNC);
	if(fp == NULL) {
		perror(fname);
		return -2;
	}
	time_t now = time(NULL);
	for(int i = 0; list[i] != NULL; ++i) {
		struct trash trash;
		char item[256];
		if(!trash_parse_details(list[i], &trash, item, sizeof item)) {
			fputs(list[i], fp);
			continue;
		}
		if(verbosity > 1) {
			char details[256];
			printf("%s %s\n", item, trash_details(&trash, details, sizeof details));
		}
		if(trash.expires && trash.expires < now) {
			if(verbosity > 0)
				printf("%s expired %s", item, ctime(&trash.expires));
			++removed;
			continue;
		}
		fputs(list[i], fp);
	}
	fclose(fp);
	strListFree(&list);
	return removed;
}

int usage(const char* prog)
{
	printf("usage: %s [-v][...] /path/to/file1.can [/path/to/file2.can][...]\n"
		,getfname(prog));
	return EXIT_SUCCESS;
}

int main(int argc, const char** argv)
{
	printf("\nSynchronet trash/filter file manager v1.0\n");

	if(argc < 2)
		return usage(argv[0]);
	for(int i = 1; i < argc; ++i) {
		const char* arg = argv[i];
		if(*arg != '-')
			continue;
		switch(arg[1]) {
			case 'v':
				++verbosity;
				break;
			default:
				return usage(argv[0]);
		}
	}
	int total = 0;
	for(int i = 1; i < argc; ++i) {
		const char* arg = argv[i];
		if(*arg == '-')
			continue;
		int  removed = maint(arg);
		if(removed < 0)
			return EXIT_FAILURE;
		total += removed;
	}
	printf("%d total items removed\n", total);
	return EXIT_SUCCESS;
}
