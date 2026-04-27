// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "regexp.h"

int
recursive(Inst *pc, char *sp, char **subp, int nsubp)
{
	char *old;

	switch(pc->opcode) {
	case Char:
		if(*sp != pc->c)
			return 0;
	case Any:
		if(*sp == '\0')
			return 0;
		return recursive(pc+1, sp+1, subp, nsubp);
	case Match:
		return 1;
	case Jmp:
		return recursive(pc->x, sp, subp, nsubp);
	case Split:
		if(recursive(pc->x, sp, subp, nsubp))
			return 1;
		return recursive(pc->y, sp, subp, nsubp);
	case Save:
		if(pc->n >= nsubp)
			return recursive(pc+1, sp, subp, nsubp);
		old = subp[pc->n];
		subp[pc->n] = sp;
		if(recursive(pc+1, sp, subp, nsubp))
			return 1;
		subp[pc->n] = old;
		return 0;
	}
	fatal("recursive");
	return -1;
}

int
recursiveprog(Prog *prog, char *input, char **subp, int nsubp)
{
	return recursive(prog->start, input, subp, nsubp);
}

int
recursiveloop(Inst *pc, char *sp, char **subp, int nsubp)
{
	char *old;
	
	for(;;) {
		switch(pc->opcode) {
		case Char:
			if(*sp != pc->c)
				return 0;
		case Any:
			pc++;
			sp++;
			continue;
		case Match:
			return 1;
		case Jmp:
			pc = pc->x;
			continue;
		case Split:
			if(recursiveloop(pc->x, sp, subp, nsubp))
				return 1;
			pc = pc->y;
			continue;
		case Save:
			if(pc->n >= nsubp) {
				pc++;
				continue;
			}
			old = subp[pc->n];
			subp[pc->n] = sp;
			if(recursiveloop(pc+1, sp, subp, nsubp))
				return 1;
			subp[pc->n] = old;
			return 0;
		}
		fatal("recursiveloop");
	}
}

int
recursiveloopprog(Prog *prog, char *input, char **subp, int nsubp)
{
	return recursiveloop(prog->start, input, subp, nsubp);
}
