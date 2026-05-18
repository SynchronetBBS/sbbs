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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "genwrap.h"        /* strcasestr, snprintf */
#include "boolsrch.h"

enum bool_node_op {
	BOOL_NODE_TERM,
	BOOL_NODE_NOT,
	BOOL_NODE_AND,
	BOOL_NODE_OR
};

struct bool_expr {
	enum bool_node_op op;
	char*             term;   /* TERM only */
	bool              quoted; /* TERM only: came from "..." -> word-bounded */
	struct bool_expr* a;      /* NOT: child; AND/OR: left */
	struct bool_expr* b;      /* AND/OR: right */
};

enum op_kw { KW_NONE = 0, KW_AND, KW_OR, KW_NOT };

typedef struct {
	const char* src;
	const char* p;
	char*       errmsg;
} parser_t;

/* ------------------------------------------------------------------------ */
/* Node helpers                                                             */
/* ------------------------------------------------------------------------ */

static bool_expr_t* new_node(enum bool_node_op op)
{
	bool_expr_t* n = (bool_expr_t*)calloc(1, sizeof *n);
	if (n != NULL)
		n->op = op;
	return n;
}

static bool_expr_t* new_term(const char* text, size_t len)
{
	bool_expr_t* n = new_node(BOOL_NODE_TERM);
	if (n == NULL)
		return NULL;
	n->term = (char*)malloc(len + 1);
	if (n->term == NULL) {
		free(n);
		return NULL;
	}
	memcpy(n->term, text, len);
	n->term[len] = '\0';
	return n;
}

static void free_node(bool_expr_t* n)
{
	if (n == NULL)
		return;
	free_node(n->a);
	free_node(n->b);
	free(n->term);
	free(n);
}

/* ------------------------------------------------------------------------ */
/* Lexer helpers                                                            */
/* ------------------------------------------------------------------------ */

static int is_word_char(int c)
{
	return (c >= 'a' && c <= 'z')
	    || (c >= 'A' && c <= 'Z')
	    || (c >= '0' && c <= '9')
	    || c == '_';
}

static int is_special_char(int c)
{
	return c == '&' || c == '|' || c == '!' || c == '(' || c == ')' || c == '"';
}

static void skip_ws(parser_t* ps)
{
	while (*ps->p != '\0' && isspace((unsigned char)*ps->p))
		ps->p++;
}

static void set_error(parser_t* ps, const char* msg)
{
	if (ps->errmsg == NULL && msg != NULL)
		ps->errmsg = strdup(msg);
}

/* Returns KW_AND/KW_OR/KW_NOT if `p` is at the start of a whole-word
 * operator keyword (case-insensitive), 0 otherwise. The caller must ensure
 * the byte immediately before `p` is a word boundary (start of input,
 * whitespace, paren, or operator char). The length of the matched keyword
 * (2 or 3) is stored in *len_out when non-NULL. */
static enum op_kw match_op_keyword(const char* p, int* len_out)
{
	enum op_kw kw  = KW_NONE;
	int        len = 0;

	if (p[0] == '\0')
		return KW_NONE;

	if ((p[0] == 'A' || p[0] == 'a')
	    && (p[1] == 'N' || p[1] == 'n')
	    && (p[2] == 'D' || p[2] == 'd')) {
		kw = KW_AND;
		len = 3;
	} else if ((p[0] == 'O' || p[0] == 'o')
	    && (p[1] == 'R' || p[1] == 'r')) {
		kw = KW_OR;
		len = 2;
	} else if ((p[0] == 'N' || p[0] == 'n')
	    && (p[1] == 'O' || p[1] == 'o')
	    && (p[2] == 'T' || p[2] == 't')) {
		kw = KW_NOT;
		len = 3;
	}

	if (kw == KW_NONE)
		return KW_NONE;

	/* Must be followed by word boundary (non-word char or EOS). */
	if (p[len] != '\0' && is_word_char((unsigned char)p[len]))
		return KW_NONE;

	if (len_out != NULL)
		*len_out = len;
	return kw;
}

/* ------------------------------------------------------------------------ */
/* Parser (recursive descent, precedence: NOT > AND > OR)                   */
/* ------------------------------------------------------------------------ */

static bool_expr_t* parse_or(parser_t* ps);

static bool_expr_t* parse_term(parser_t* ps)
{
	skip_ws(ps);
	if (*ps->p == '\0') {
		set_error(ps, "expected search term");
		return NULL;
	}

	if (*ps->p == '"') {
		ps->p++;
		const char* start = ps->p;
		while (*ps->p != '\0' && *ps->p != '"')
			ps->p++;
		if (*ps->p != '"') {
			set_error(ps, "unterminated quoted string");
			return NULL;
		}
		size_t len = (size_t)(ps->p - start);
		ps->p++; /* consume closing quote */
		if (len == 0) {
			set_error(ps, "empty quoted string");
			return NULL;
		}
		bool_expr_t* n = new_term(start, len);
		if (n != NULL)
			n->quoted = true;
		return n;
	}

	/* Bare phrase: scan accepting non-special chars and internal whitespace,
	 * stopping at any operator character or at whitespace followed by an
	 * operator keyword. Trailing whitespace is trimmed. */
	const char* start = ps->p;
	while (*ps->p != '\0') {
		unsigned char c = (unsigned char)*ps->p;
		if (is_special_char(c))
			break;
		if (isspace(c)) {
			const char* q = ps->p;
			while (*q != '\0' && isspace((unsigned char)*q))
				q++;
			if (*q == '\0')
				break;
			if (is_special_char((unsigned char)*q))
				break;
			if (match_op_keyword(q, NULL) != KW_NONE)
				break;
		}
		ps->p++;
	}

	const char* end = ps->p;
	while (end > start && isspace((unsigned char)*(end - 1)))
		end--;

	if (end == start) {
		set_error(ps, "expected search term");
		return NULL;
	}
	return new_term(start, (size_t)(end - start));
}

static bool_expr_t* parse_atom(parser_t* ps)
{
	skip_ws(ps);
	if (*ps->p == '(') {
		ps->p++;
		bool_expr_t* inner = parse_or(ps);
		if (inner == NULL)
			return NULL;
		skip_ws(ps);
		if (*ps->p != ')') {
			free_node(inner);
			set_error(ps, "expected ')'");
			return NULL;
		}
		ps->p++;
		return inner;
	}
	return parse_term(ps);
}

static bool_expr_t* parse_not(parser_t* ps)
{
	bool negate = false;
	for (;;) {
		skip_ws(ps);
		if (*ps->p == '!') {
			negate = !negate;
			ps->p++;
			continue;
		}
		int len;
		if (match_op_keyword(ps->p, &len) == KW_NOT) {
			negate = !negate;
			ps->p += len;
			continue;
		}
		break;
	}

	bool_expr_t* child = parse_atom(ps);
	if (child == NULL)
		return NULL;
	if (!negate)
		return child;

	bool_expr_t* n = new_node(BOOL_NODE_NOT);
	if (n == NULL) {
		free_node(child);
		set_error(ps, "out of memory");
		return NULL;
	}
	n->a = child;
	return n;
}

static bool_expr_t* parse_and(parser_t* ps)
{
	bool_expr_t* left = parse_not(ps);
	if (left == NULL)
		return NULL;
	for (;;) {
		skip_ws(ps);
		bool is_and = false;
		if (*ps->p == '&') {
			is_and = true;
			ps->p++;
		} else {
			int len;
			if (match_op_keyword(ps->p, &len) == KW_AND) {
				is_and = true;
				ps->p += len;
			} else if (*ps->p == '!'
			    || match_op_keyword(ps->p, NULL) == KW_NOT) {
				/* Implicit AND before a unary NOT, per Wildcat's example
				 * `(windows | DOS) & (modem | comm) !OS/2`. Don't consume
				 * the '!' or "NOT" — parse_not will. */
				is_and = true;
			}
		}
		if (!is_and)
			break;

		bool_expr_t* right = parse_not(ps);
		if (right == NULL) {
			free_node(left);
			return NULL;
		}
		bool_expr_t* n = new_node(BOOL_NODE_AND);
		if (n == NULL) {
			free_node(left);
			free_node(right);
			set_error(ps, "out of memory");
			return NULL;
		}
		n->a = left;
		n->b = right;
		left = n;
	}
	return left;
}

static bool_expr_t* parse_or(parser_t* ps)
{
	bool_expr_t* left = parse_and(ps);
	if (left == NULL)
		return NULL;
	for (;;) {
		skip_ws(ps);
		bool is_or = false;
		if (*ps->p == '|') {
			is_or = true;
			ps->p++;
		} else {
			int len;
			if (match_op_keyword(ps->p, &len) == KW_OR) {
				is_or = true;
				ps->p += len;
			}
		}
		if (!is_or)
			break;

		bool_expr_t* right = parse_and(ps);
		if (right == NULL) {
			free_node(left);
			return NULL;
		}
		bool_expr_t* n = new_node(BOOL_NODE_OR);
		if (n == NULL) {
			free_node(left);
			free_node(right);
			set_error(ps, "out of memory");
			return NULL;
		}
		n->a = left;
		n->b = right;
		left = n;
	}
	return left;
}

/* ------------------------------------------------------------------------ */
/* Public API                                                               */
/* ------------------------------------------------------------------------ */

bool_expr_t* bool_expr_compile(const char* query, char** errmsg)
{
	if (errmsg != NULL)
		*errmsg = NULL;
	if (query == NULL) {
		if (errmsg != NULL)
			*errmsg = strdup("NULL query");
		return NULL;
	}

	const char* start = query;
	while (*start != '\0' && isspace((unsigned char)*start))
		start++;
	if (*start == '\0') {
		if (errmsg != NULL)
			*errmsg = strdup("empty query");
		return NULL;
	}

	parser_t ps;
	ps.src = query;
	ps.p = query;
	ps.errmsg = NULL;

	bool_expr_t* expr = parse_or(&ps);
	if (expr == NULL) {
		if (errmsg != NULL)
			*errmsg = ps.errmsg != NULL ? ps.errmsg : strdup("parse error");
		else
			free(ps.errmsg);
		return NULL;
	}
	skip_ws(&ps);
	if (*ps.p != '\0') {
		char buf[128];
		snprintf(buf, sizeof buf, "unexpected '%c' at offset %ld",
		    *ps.p, (long)(ps.p - query));
		free_node(expr);
		free(ps.errmsg);
		if (errmsg != NULL)
			*errmsg = strdup(buf);
		return NULL;
	}
	free(ps.errmsg); /* unused on success path */
	return expr;
}

void bool_expr_free(bool_expr_t* expr)
{
	free_node(expr);
}

/* ------------------------------------------------------------------------ */
/* Evaluation                                                               */
/* ------------------------------------------------------------------------ */

/* Scan `haystack` for `needle` (case-insensitive substring). If check_lead
 * is true, the match must be preceded by a non-word character (or be at the
 * start of `haystack`). If check_trail is true, the match must be followed
 * by a non-word character (or be at the end of `haystack`). */
static bool find_word_bounded(const char* haystack, const char* needle,
                              bool check_lead, bool check_trail)
{
	size_t      nlen = strlen(needle);
	const char* h    = haystack;
	if (nlen == 0)
		return true;
	while ((h = strcasestr(h, needle)) != NULL) {
		size_t pos = (size_t)(h - haystack);
		bool   ok  = true;
		if (check_lead && pos > 0
		    && is_word_char((unsigned char)haystack[pos - 1]))
			ok = false;
		if (ok && check_trail) {
			char after = haystack[pos + nlen];
			if (after != '\0' && is_word_char((unsigned char)after))
				ok = false;
		}
		if (ok)
			return true;
		h++; /* advance past this start position and keep scanning */
	}
	return false;
}

static bool eval_term(const bool_expr_t* n, const char* const* fields, size_t nfields)
{
	if (n->term == NULL || *n->term == '\0')
		return true;
	bool check_lead  = false;
	bool check_trail = false;
	if (n->quoted) {
		size_t tlen = strlen(n->term);
		if (tlen > 0 && !isspace((unsigned char)n->term[0]))
			check_lead = true;
		if (tlen > 0 && !isspace((unsigned char)n->term[tlen - 1]))
			check_trail = true;
	}
	for (size_t i = 0; i < nfields; i++) {
		const char* f = fields[i];
		if (f == NULL)
			continue;
		if (n->quoted) {
			if (find_word_bounded(f, n->term, check_lead, check_trail))
				return true;
		} else {
			if (strcasestr(f, n->term) != NULL)
				return true;
		}
	}
	return false;
}

static bool eval_expr(const bool_expr_t* n, const char* const* fields, size_t nfields)
{
	if (n == NULL)
		return false;
	switch (n->op) {
		case BOOL_NODE_TERM:
			return eval_term(n, fields, nfields);
		case BOOL_NODE_NOT:
			return !eval_expr(n->a, fields, nfields);
		case BOOL_NODE_AND:
			return eval_expr(n->a, fields, nfields)
			    && eval_expr(n->b, fields, nfields);
		case BOOL_NODE_OR:
			return eval_expr(n->a, fields, nfields)
			    || eval_expr(n->b, fields, nfields);
	}
	return false;
}

bool bool_expr_match(const bool_expr_t* expr, const char* haystack)
{
	const char* fields[1];
	fields[0] = haystack;
	return eval_expr(expr, fields, 1);
}

bool bool_expr_match_fields(const bool_expr_t* expr, const char* const* fields, size_t nfields)
{
	return eval_expr(expr, fields, nfields);
}

/* ------------------------------------------------------------------------ */
/* Diagnostic helpers (file-local; reachable from unit tests that include   */
/* this translation unit directly).                                         */
/* ------------------------------------------------------------------------ */

static bool bool_expr_is_simple(const bool_expr_t* expr)
{
	return expr != NULL && expr->op == BOOL_NODE_TERM && !expr->quoted;
}

static const char* bool_expr_simple_text(const bool_expr_t* expr)
{
	if (expr == NULL || expr->op != BOOL_NODE_TERM || expr->quoted)
		return NULL;
	return expr->term;
}

static size_t append_str(char* out, size_t outsz, size_t pos, const char* s)
{
	if (outsz == 0 || pos + 1 >= outsz)
		return pos;
	size_t slen = strlen(s);
	size_t avail = outsz - pos - 1; /* leave room for NUL */
	size_t cpy = slen < avail ? slen : avail;
	memcpy(out + pos, s, cpy);
	return pos + cpy;
}

static size_t describe_rec(const bool_expr_t* n, char* out, size_t outsz, size_t pos)
{
	if (n == NULL)
		return append_str(out, outsz, pos, "<null>");
	switch (n->op) {
		case BOOL_NODE_TERM:
			if (n->quoted)
				pos = append_str(out, outsz, pos, "Q\"");
			else
				pos = append_str(out, outsz, pos, "\"");
			pos = append_str(out, outsz, pos, n->term != NULL ? n->term : "");
			pos = append_str(out, outsz, pos, "\"");
			return pos;
		case BOOL_NODE_NOT:
			pos = append_str(out, outsz, pos, "(NOT ");
			pos = describe_rec(n->a, out, outsz, pos);
			pos = append_str(out, outsz, pos, ")");
			return pos;
		case BOOL_NODE_AND:
			pos = append_str(out, outsz, pos, "(AND ");
			pos = describe_rec(n->a, out, outsz, pos);
			pos = append_str(out, outsz, pos, " ");
			pos = describe_rec(n->b, out, outsz, pos);
			pos = append_str(out, outsz, pos, ")");
			return pos;
		case BOOL_NODE_OR:
			pos = append_str(out, outsz, pos, "(OR ");
			pos = describe_rec(n->a, out, outsz, pos);
			pos = append_str(out, outsz, pos, " ");
			pos = describe_rec(n->b, out, outsz, pos);
			pos = append_str(out, outsz, pos, ")");
			return pos;
	}
	return pos;
}

static size_t bool_expr_describe(const bool_expr_t* expr, char* out, size_t outsz)
{
	if (out == NULL || outsz == 0)
		return 0;
	size_t pos = describe_rec(expr, out, outsz, 0);
	if (pos >= outsz)
		pos = outsz - 1;
	out[pos] = '\0';
	return pos;
}
