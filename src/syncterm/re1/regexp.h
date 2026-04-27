// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#define nil ((void*)0)
#define nelem(x) (sizeof(x)/sizeof((x)[0]))

typedef struct Regexp Regexp;
typedef struct Prog Prog;
typedef struct Inst Inst;

struct Regexp
{
	int type;
	int n;
	int ch;
	Regexp *left;
	Regexp *right;
};

enum	/* Regexp.type */
{
	Alt = 1,
	Cat,
	Lit,
	Dot,
	Paren,
	Quest,
	Star,
	Plus,
};

Regexp *parse(char*);
/* Like parse() but without the auto-prepended `.*?` match-anywhere
 * prefix.  Use when the caller anchors at the start of input by
 * other means (e.g., SyncTERM's streaming Pike VM trims its buffer
 * on IMPOSSIBLE, achieving match-anywhere without needing the
 * prefix). */
Regexp *parse_unanchored(char*);
Regexp *reg(int type, Regexp *left, Regexp *right);
void printre(Regexp*);
void fatal(char*, ...);
void *mal(int);

/* Optional host-side trap for fatal().  If set, called with the
 * formatted message; the host is expected to longjmp/abort and NOT
 * return.  Default NULL — fatal() then prints to stderr and exit(2)s
 * as before. */
extern void (*re1_fatal_handler)(const char *msg);

struct Prog
{
	Inst *start;
	int len;
};

struct Inst
{
	int opcode;
	int c;
	int n;
	Inst *x;
	Inst *y;
	int gen;	// global state, oooh!
};

enum	/* Inst.opcode */
{
	Char = 1,
	Match,
	Jmp,
	Split,
	Any,
	Save,
};

Prog *compile(Regexp*);
void printprog(Prog*);

extern int gen;

enum {
	MAXSUB = 20
};

typedef struct Sub Sub;

struct Sub
{
	int ref;
	int nsub;
	char *sub[MAXSUB];
};

Sub *newsub(int n);
Sub *incref(Sub*);
Sub *copy(Sub*);
Sub *update(Sub*, int, char*);
void decref(Sub*);

int backtrack(Prog*, char*, char**, int);
int pikevm(Prog*, char*, char**, int);
int recursiveloopprog(Prog*, char*, char**, int);
int recursiveprog(Prog*, char*, char**, int);
int thompsonvm(Prog*, char*, char**, int);

/* Streaming Pike VM — feed bytes one at a time.  See pike.c. */
typedef struct PikeVM PikeVM;

PikeVM *pikevm_new(Prog *prog, int nsubp);
void    pikevm_free(PikeVM *vm);
void    pikevm_start(PikeVM *vm, char *sp);
int     pikevm_step(PikeVM *vm, char *sp);   /* 1=match, 0=impossible, -1=pending */
void    pikevm_match(PikeVM *vm, char **subp);
