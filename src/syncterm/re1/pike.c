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
		addthread(l, thread(t.pc+1, re1_update(t.sub, t.pc->n, sp)), sp);
		break;
	}
}

/* ----- Streaming Pike VM (additive — leaves pikevm() untouched) -----
 *
 * Feed bytes one at a time; tri-state return tells the caller whether
 * a match completed, whether continuation is impossible, or whether
 * threads are still alive and need more bytes.  Used by SyncTERM's
 * Wren input regex hooks; see wren_host.c for the buffer-management
 * algorithm built on top of this.
 *
 * The PikeVM owns its threadlists for the life of the object so step
 * is allocation-free in the steady state (only newsub on start and
 * incref/decref of Sub during stepping).  `gen` is global; multiple
 * VMs operating on distinct Progs don't interfere because gen counts
 * are compared per-Inst, and each Prog has its own Insts.
 */

struct PikeVM {
	Prog        *prog;
	ThreadList  *clist;
	ThreadList  *nlist;
	Sub         *matched;
	int          nsubp;
};

PikeVM*
pikevm_new(Prog *prog, int nsubp)
{
	PikeVM *vm = mal(sizeof *vm);
	vm->prog    = prog;
	vm->clist   = threadlist(prog->len);
	vm->nlist   = threadlist(prog->len);
	vm->matched = nil;
	vm->nsubp   = nsubp;
	return vm;
}

void
pikevm_free(PikeVM *vm)
{
	if(vm == nil)
		return;
	if(vm->matched)
		decref(vm->matched);
	free(vm->clist);
	free(vm->nlist);
	free(vm);
}

/* Reset to initial state.  Call before the first step, and again
 * after pulling captures from a MATCH. */
void
pikevm_start(PikeVM *vm, char *sp)
{
	int i;
	Sub *sub;

	if(vm->matched) {
		decref(vm->matched);
		vm->matched = nil;
	}
	/* Drain any threads left from a previous attempt. */
	for(i = 0; i < vm->clist->n; i++)
		decref(vm->clist->t[i].sub);
	vm->clist->n = 0;
	for(i = 0; i < vm->nlist->n; i++)
		decref(vm->nlist->t[i].sub);
	vm->nlist->n = 0;

	sub = newsub(vm->nsubp);
	for(i = 0; i < vm->nsubp; i++)
		sub->sub[i] = nil;

	gen++;
	addthread(vm->clist, thread(vm->prog->start, sub), sp);
}

/* Consume the byte at *sp and advance the NFA.  Caller advances sp
 * by one between calls.  Note: unlike the buffer-shaped pikevm, this
 * does NOT treat a NUL byte as end-of-input — `.` matches NUL too. */
int
pikevm_step(PikeVM *vm, char *sp)
{
	int i, j;
	Inst *pc;
	Sub *sub;
	ThreadList *tmp;

	if(vm->matched) {
		/* caller didn't reset after the previous match — drop it */
		decref(vm->matched);
		vm->matched = nil;
	}
	if(vm->clist->n == 0)
		return 0;	/* IMPOSSIBLE */

	gen++;
	for(i = 0; i < vm->clist->n; i++) {
		pc = vm->clist->t[i].pc;
		sub = vm->clist->t[i].sub;
		switch(pc->opcode) {
		case Char:
			if(*sp != pc->c) {
				decref(sub);
				break;
			}
			/* fallthrough — both Char (matched) and Any consume the byte */
		case Any:
			addthread(vm->nlist, thread(pc+1, sub), sp+1);
			break;
		case Match:
			/* a Match thread sat in clist from a prior step's
			 * tail; commit it and drop the rest */
			if(vm->matched)
				decref(vm->matched);
			vm->matched = sub;
			for(i++; i < vm->clist->n; i++)
				decref(vm->clist->t[i].sub);
			goto swap;
		}
	}
swap:
	tmp = vm->clist;
	vm->clist = vm->nlist;
	vm->nlist = tmp;
	vm->nlist->n = 0;

	if(vm->matched)
		return 1;	/* MATCH */

	/* Same-step Match detection: a Char that just advanced may have
	 * landed a thread on a Match opcode now sitting in clist.  Scan
	 * before yielding so the caller sees MATCH on the byte that
	 * completed it, not on a phantom next byte. */
	for(i = 0; i < vm->clist->n; i++) {
		if(vm->clist->t[i].pc->opcode == Match) {
			vm->matched = vm->clist->t[i].sub;
			for(j = 0; j < vm->clist->n; j++) {
				if(j != i)
					decref(vm->clist->t[j].sub);
			}
			vm->clist->n = 0;
			return 1;	/* MATCH */
		}
	}

	if(vm->clist->n == 0)
		return 0;	/* IMPOSSIBLE */
	return -1;		/* PENDING */
}

/* Copy capture pointers from the most recent MATCH.  Pointers are
 * into the caller's input buffer; valid only until the buffer is
 * mutated. */
void
pikevm_match(PikeVM *vm, char **subp)
{
	int i;
	if(vm->matched == nil) {
		for(i = 0; i < vm->nsubp; i++)
			subp[i] = nil;
		return;
	}
	for(i = 0; i < vm->nsubp; i++)
		subp[i] = vm->matched->sub[i];
}

/* ----- Buffer-shaped pikevm (original, untouched) ----------------- */

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
