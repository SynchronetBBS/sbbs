// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "regexp.h"

typedef struct Thread Thread;
struct Thread
{
	Inst *pc;
};

typedef struct ThreadList ThreadList;
struct ThreadList
{
	int n;
	Thread t[1];
};

static Thread
thread(Inst *pc)
{
	Thread t = {pc};
	return t;
}

static ThreadList*
threadlist(int n)
{
	return mal(sizeof(ThreadList)+n*sizeof(Thread));
}

static void
addthread(ThreadList *l, Thread t)
{
	if(t.pc->gen == gen)
		return;	// already on list

	t.pc->gen = gen;
	l->t[l->n] = t;
	l->n++;
	
	switch(t.pc->opcode) {
	case Jmp:
		addthread(l, thread(t.pc->x));
		break;
	case Split:
		addthread(l, thread(t.pc->x));
		addthread(l, thread(t.pc->y));
		break;
	case Save:
		addthread(l, thread(t.pc+1));
		break;
	}
}

int
thompsonvm(Prog *prog, char *input, char **subp, int nsubp)
{
	int i, len, matched;
	ThreadList *clist, *nlist, *tmp;
	Inst *pc;
	char *sp;
	
	for(i=0; i<nsubp; i++)
		subp[i] = nil;

	len = prog->len;
	clist = threadlist(len);
	nlist = threadlist(len);
	
	if(nsubp >= 1)
		subp[0] = input;
	gen++;
	addthread(clist, thread(prog->start));
	matched = 0;
	for(sp=input;; sp++) {
		if(clist->n == 0)
			break;
		// printf("%d(%02x).", (int)(sp - input), *sp & 0xFF);
		gen++;
		for(i=0; i<clist->n; i++) {
			pc = clist->t[i].pc;
			// printf(" %d", (int)(pc - prog->start));
			switch(pc->opcode) {
			case Char:
				if(*sp != pc->c)
					break;
			case Any:
				if(*sp == 0)
					break;
				addthread(nlist, thread(pc+1));
				break;
			case Match:
				if(nsubp >= 2)
					subp[1] = sp;
				matched = 1;
				goto BreakFor;
			// Jmp, Split, Save handled in addthread, so that
			// machine execution matches what a backtracker would do.
			// This is discussed (but not shown as code) in
			// Regular Expression Matching: the Virtual Machine Approach.
			}
		}
	BreakFor:
		// printf("\n");
		tmp = clist;
		clist = nlist;
		nlist = tmp;
		nlist->n = 0;
		if(*sp == '\0')
			break;
	}
	return matched;
}
