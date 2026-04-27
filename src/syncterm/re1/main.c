// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "regexp.h"

struct {
	char *name;
	int (*fn)(Prog*, char*, char**, int);
} tab[] = {
	"recursive", recursiveprog,
	"recursiveloop", recursiveloopprog,
	"backtrack", backtrack,
	"thompson", thompsonvm,
	"pike", pikevm,
};

void
usage(void)
{
	fprintf(stderr, "usage: re regexp string...\n");
	exit(2);
}

int
main(int argc, char **argv)
{
	int i, j, k, l;
	Regexp *re;
	Prog *prog;
	char *sub[MAXSUB];

	if(argc < 2)
		usage();
	
	re = parse(argv[1]);
	printre(re);
	printf("\n");

	prog = compile(re);
	printprog(prog);

	for(i=2; i<argc; i++) {
		printf("#%d %s\n", i, argv[i]);
		for(j=0; j<nelem(tab); j++) {
			printf("%s ", tab[j].name);
			memset(sub, 0, sizeof sub);
			if(!tab[j].fn(prog, argv[i], sub, nelem(sub))) {
				printf("-no match-\n");
				continue;
			}
			printf("match");
			for(k=MAXSUB; k>0; k--)
				if(sub[k-1])
					break;
			for(l=0; l<k; l+=2) {
				printf(" (");
				if(sub[l] == nil)
					printf("?");
				else
					printf("%d", (int)(sub[l] - argv[i]));
				printf(",");
				if(sub[l+1] == nil)
					printf("?");
				else
					printf("%d", (int)(sub[l+1] - argv[i]));
				printf(")");
			}
			printf("\n");
		}
	}
	return 0;
}
