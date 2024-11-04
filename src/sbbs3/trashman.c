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
#include "findstr.h"
#include "nopen.h"
#include "git_branch.h"
#include "git_hash.h"

bool test = false;
int verbosity = 0;
int max_age = 0;
const char* prot = NULL;

int maint(const char* fname)
{
	int removed = 0;
	str_list_t list = findstr_list(fname);
	if(list == NULL) {
		perror(fname);
		return -1;
	}
	FILE* fp;
	if(test)
		fp = fopen(_PATH_DEVNULL, "w");
	else
		fp = fnopen(NULL, fname, O_WRONLY|O_TRUNC);
	if(fp == NULL) {
		perror(test ? _PATH_DEVNULL : fname);
		return -2;
	}
	time_t now = time(NULL);
	for(int i = 0; list[i] != NULL; ++i) {
		struct trash trash;
		char item[256];
		if(!trash_parse_details(list[i], &trash, item, sizeof item)
			|| (prot != NULL && stricmp(trash.prot, prot) != 0)) {
			fputs(list[i], fp);
			continue;
		}
		if(verbosity > 1) {
			char details[256];
			printf("%s: %s %s\n", fname, item, trash_details(&trash, details, sizeof details));
		}
		if(trash.expires && trash.expires < now) {
			if(verbosity > 0)
				printf("%s: %s expired %s", fname, item, ctime(&trash.expires));
			++removed;
			continue;
		}
		if(max_age != 0 && trash.added != 0) {
			int age = (int)(now - trash.added);
			if(age > 0 && (age/=(24*60*60)) > max_age) {
				if(verbosity > 0)
					printf("%s: %s is %d days old\n", fname, item, age);
				++removed;
				continue;
			}
		}
		fputs(list[i], fp);
	}
	fclose(fp);
	strListFree(&list);
	if(removed || verbosity > 0)
		printf("%s: %d items %sremoved\n"
			,fname, removed, test ? "would have been " : "");
	return removed;
}

int usage(const char* prog)
{
	printf("\nusage: %s [-opt][...] /path/to/file1.can [/path/to/file2.can][...]\n"
		,getfname(prog));
	printf("\noptions:\n");
	printf("     -a<days> specify maximum age of filter items, in days\n");
	printf("     -p<prot> only manage filters for specified protocol\n");
	printf("     -t       run in test (read-only) mode\n");
	printf("     -v       increase verbosity of output\n");
	return EXIT_SUCCESS;
}

int main(int argc, const char** argv)
{
	printf("\nSynchronet Trash Can (Filter File) Manager  v1.0  %s/%s\n"
		,GIT_BRANCH, GIT_HASH);

	if(argc < 2)
		return usage(argv[0]);
	for(int i = 1; i < argc; ++i) {
		const char* arg = argv[i];
		if(*arg != '-')
			continue;
		++arg;
		switch(*arg) {
			case 't':
				test = true;
				break;
			case 'v':
				do {
					++verbosity;
				} while(*(++arg) == 'v');
				break;
			case 'a':
				max_age = atoi(++arg);
				break;
			case 'p':
				prot = ++arg;
				break;
			default:
				return usage(argv[0]);
		}
	}
	int total = 0;
	int files = 0;
	for(int i = 1; i < argc; ++i) {
		const char* arg = argv[i];
		if(*arg == '-')
			continue;
		int  removed = maint(arg);
		if(removed < 0)
			return EXIT_FAILURE;
		total += removed;
		++files;
	}
	if(files > 1)
		printf("%d total items %sremoved from %d files\n", total, test ? "would have been " : "", files);
	return EXIT_SUCCESS;
}
