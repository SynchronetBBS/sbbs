/*
 * mocks_system.h
 *
 * Shared mocks for system.h functionality.  Provides System_Error, System_Close,
 * CheckMem, and related system function stubs used by 15+ test files.
 *
 * These mocks use longjmp to signal fatal errors for testing via ASSERT_FATAL.
 * Each test file #includes this header before #include "../the_file.c".
 */

#ifndef MOCKS_SYSTEM_H
#define MOCKS_SYSTEM_H

#include <setjmp.h>
#include <stdnoreturn.h>
#include <string.h>

/* =========================================================================
 * jmp_buf and fatal error mechanism
 * ========================================================================= */

static jmp_buf g_fatal_jmp;

noreturn void System_Error(char *szErrorMsg)
{
	(void)szErrorMsg;
	longjmp(g_fatal_jmp, 1);
}

noreturn void System_Close(void)
{
	longjmp(g_fatal_jmp, 1);
}

void System_Close_AtExit(void)
{
}

void System_Init(void)
{
}

void System_Maint(void)
{
}

char *DupeStr(const char *s)
{
	(void)s;
	return NULL;
}

void CheckMem(void *ptr)
{
	if (!ptr) longjmp(g_fatal_jmp, 1);
}

/* =========================================================================
 * External variable definitions required by system.h and defines.h
 * ========================================================================= */

struct system System;
bool Verbose = false;
int _argc = 0;
static char *_argv_buf[] = {NULL};
char **_argv = _argv_buf;

#endif /* MOCKS_SYSTEM_H */
