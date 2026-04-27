// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "regexp.h"

typedef struct Thread Thread;
struct Thread
{
	Inst *pc;
	Sub *sub;
};

typedef struct ThreadList ThreadList;
struct ThreadList
{
	int n;
	Thread t[1];
};

static Thread
thread(Inst *pc, Sub *sub)
{
	Thread t = {pc, sub};
	return t;
}

static ThreadList*
threadlist(int n)
{
	return mal(sizeof(ThreadList)+n*sizeof(Thread));
}

static void
addthread(ThreadList *l, Thread t, char *sp)
{
	if(t.pc->gen == gen) {
		decref(t.sub);
		return;	// already on list
	}
	t.pc->gen = gen;
	
	switch(t.pc->opcode) {
	default:
		l->t[l->n] = t;
		l->n++;
		break;
	case Jmp:
		addthread(l, thread(t.pc->x, t.sub), sp);
		break;
	case Split:
		addthread(l, thread(t.pc->x, incref(t.sub)), sp);
		addthread(l, thread(t.pc->y, t.sub), sp);
		break;
	case Save:
		addthread(l, thread(t.pc+1, update(t.sub, t.pc->n, sp)), sp);
		break;
	}
}

int
pikevm(Prog *prog, char *input, char **subp, int nsubp)
{
	int i, len;
	ThreadList *clist, *nlist, *tmp;
	Inst *pc;
	char *sp;
	Sub *sub, *matched;
	
	matched = nil;	
	for(i=0; i<nsubp; i++)
		subp[i] = nil;
	sub = newsub(nsubp);
	for(i=0; i<nsubp; i++)
		sub->sub[i] = nil;

	len = prog->len;
	clist = threadlist(len);
	nlist = threadlist(len);
	
	gen++;
	addthread(clist, thread(prog->start, sub), input);
	matched = 0;
	for(sp=input;; sp++) {
		if(clist->n == 0)
			break;
		// printf("%d(%02x).", (int)(sp - input), *sp & 0xFF);
		gen++;
		for(i=0; i<clist->n; i++) {
			pc = clist->t[i].pc;
			sub = clist->t[i].sub;
			// printf(" %d", (int)(pc - prog->start));
			switch(pc->opcode) {
			case Char:
				if(*sp != pc->c) {
					decref(sub);
					break;
				}
			case Any:
				if(*sp == 0) {
					decref(sub);
					break;
				}
				addthread(nlist, thread(pc+1, sub), sp+1);
				break;
			case Match:
				if(matched)
					decref(matched);
				matched = sub;
				for(i++; i < clist->n; i++)
					decref(clist->t[i].sub);
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
	if(matched) {
		for(i=0; i<nsubp; i++)
			subp[i] = matched->sub[i];
		decref(matched);
		return 1;
	}
	return 0;
}
