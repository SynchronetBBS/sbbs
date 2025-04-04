/* Synchronet JavaScript "[s]printf" implementation */

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
#include "xpprintf.h"   /* Hurrah for Deuce! */

char*
js_sprintf(JSContext *cx, uint argn, uintN argc, jsval *argv)
{
	char*  op;
	char*  p;
	char * p2 = NULL;
	size_t p2_sz = 0;

	JSVALUE_TO_MSTRING(cx, argv[argn], op, NULL);
	argn++;
	if (JS_IsExceptionPending(cx))
		JS_ClearPendingException(cx);
	if (op == NULL)
		return NULL;

	p = op;
	p = xp_asprintf_start(p);
	uintN cur = 0;
	for (;;) {
		int next = xp_printf_get_next(p);
		if (next < 0)
			break;
		if (next > 0 && next != (cur + 1))
			cur = next;
		else
			cur++;
		if (cur >= argc)
			break;
		if (JSVAL_IS_DOUBLE(argv[cur]))
			p = xp_asprintf_next(p, XP_PRINTF_CONVERT | XP_PRINTF_TYPE_DOUBLE, JSVAL_TO_DOUBLE(argv[cur]));
		else if (JSVAL_IS_INT(argv[cur]))
			p = xp_asprintf_next(p, XP_PRINTF_CONVERT | XP_PRINTF_TYPE_INT, JSVAL_TO_INT(argv[cur]));
		else if (JSVAL_IS_BOOLEAN(argv[cur]) && xp_printf_get_type(p) != XP_PRINTF_TYPE_CHARP)
			p = xp_asprintf_next(p, XP_PRINTF_CONVERT | XP_PRINTF_TYPE_INT, JSVAL_TO_BOOLEAN(argv[cur]));
		else {
			JSVALUE_TO_RASTRING(cx, argv[cur], p2, &p2_sz, NULL);
			if (JS_IsExceptionPending(cx))
				JS_ClearPendingException(cx);
			if (p2 == NULL) {
				free(p);
				free(op);
				return NULL;
			}
			p = xp_asprintf_next(p, XP_PRINTF_CONVERT | XP_PRINTF_TYPE_CHARP, p2);
		}
	}

	if (p2)
		free(p2);
	p2 = xp_asprintf_end(p, NULL);
	free(op);
	return p2;
}

void
js_sprintf_free(char* p)
{
	xp_asprintf_free(p);
}
