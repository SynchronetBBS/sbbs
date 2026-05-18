/* Synchronet boolean search expression compiler/evaluator */

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

#ifndef _BOOLSRCH_H_
#define _BOOLSRCH_H_

#include <stdbool.h>
#include <stddef.h>

#include "dllexport.h"

/* Boolean search query syntax (compatible with PCBoard and Wildcat):
 *
 *     expr   := orExpr
 *     orExpr := andExpr ( ('|' | "OR")  andExpr )*
 *     andExpr:= notExpr ( ('&' | "AND") notExpr )*
 *     notExpr:= ('!' | "NOT") notExpr | atom
 *     atom   := '(' expr ')' | TERM
 *     TERM   := '"' quotedString '"' | barePhrase
 *
 * Whitespace between bare words is part of one phrase term (PCBoard rule),
 * not implicit-AND. Operator keywords AND/OR/NOT are case-insensitive and
 * only recognized when whole-word; "BANDIT" does not contain an AND operator.
 * Matching is case-insensitive substring; a quoted term applies a word-
 * boundary check at each side that does not contain whitespace inside the
 * quotes (so `"WORD"` is whole-word, while `" WORD "` is pure substring).
 *
 * Operator precedence: NOT (tightest) > AND > OR.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bool_expr bool_expr_t;

/* Compile `query` into an expression tree. Returns NULL on syntax error.
 * On failure, if `errmsg` is non-NULL, *errmsg is set to a malloc'd
 * description of the error and the caller is responsible for free()ing it.
 * An empty or whitespace-only query is reported as a "empty query" error. */
DLLEXPORT bool_expr_t* bool_expr_compile(const char* query, char** errmsg);

/* Evaluate against a single haystack. A term matches if it appears as a
 * case-insensitive substring of `haystack`. NULL haystack is treated as the
 * empty string (no terms match). */
DLLEXPORT bool         bool_expr_match(const bool_expr_t* expr,
                                       const char* haystack);

/* Evaluate against multiple fields: a term matches the document if it
 * appears in ANY of the supplied fields. NULL field pointers are tolerated
 * (treated as empty strings). nfields == 0 returns the empty-haystack
 * evaluation. */
DLLEXPORT bool         bool_expr_match_fields(const bool_expr_t* expr,
                                              const char* const* fields,
                                              size_t nfields);

DLLEXPORT void         bool_expr_free(bool_expr_t* expr);

#ifdef __cplusplus
}
#endif

#endif /* _BOOLSRCH_H_ */
