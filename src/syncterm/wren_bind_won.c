/* WON (Wren Object Notation) deserialiser.  Hand-written recursive-descent
 * parser over the same literal subset that the Wren-side WON.serialize
 * produces.  Crucially this is NOT eval — the input never reaches the
 * Wren compiler — so it is safe to feed untrusted text.
 *
 * Public API is fn_WON_deserialize, declared in wren_bind_won.h and
 * registered as `WON.deserialize(_)` in the BINDINGS table in
 * wren_bind.c. */

#include "wren_bind_internal.h"
#include "wren_bind_won.h"
#include "wren_host_internal.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Recursion limit — every nested List/Map level recurses into
 * parse_value.  Matches the recursion budget the Wren compiler uses
 * itself (see wren_compiler.c MAX_RECURSION_DEPTH). */
#define WON_MAX_DEPTH 256

/* WONError code values — granular enough to demux ("the input was
 * truncated, ask the user for more" vs "the syntax is wrong, give
 * up") without overfitting one code per message.  Mirrors the
 * SFTPError / FileError pattern. */
enum won_err_code {
	WON_ERR_OK = 0,
	WON_ERR_SYNTAX,         /* expected X, unexpected character, malformed token */
	WON_ERR_TRUNCATED,      /* unterminated string / premature EOF */
	WON_ERR_INVALID_ESCAPE, /* malformed \x / \u / \U */
	WON_ERR_INVALID_KEY,    /* List/Map used as Map key */
	WON_ERR_TOO_DEEP,       /* nested past WON_MAX_DEPTH */
	WON_ERR_TRAILING_DATA,  /* extra bytes after a complete value */
};

struct wren_won_error {
	enum syncterm_wren_foreign type;
	uint32_t code;     /* enum won_err_code */
	int      offset;   /* byte offset where the error was detected */
	char    *message;  /* malloc'd; may be NULL */
};

struct won_parser {
	const char *src;   /* start of buffer (for error offset reports) */
	const char *end;   /* one-past-end */
	const char *pos;   /* current cursor */
	int depth;
};

void
wren_won_error_allocate(WrenVM *vm)
{
	struct wren_won_error *e =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type = SWF_WON_ERROR;
}

void
wren_won_error_finalize(void *data)
{
	struct wren_won_error *e = data;
	free(e->message);
}

void
fn_WONError_code(WrenVM *vm)
{
	struct wren_won_error *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->code);
}

void
fn_WONError_offset(WrenVM *vm)
{
	struct wren_won_error *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->offset);
}

void
fn_WONError_message(WrenVM *vm)
{
	struct wren_won_error *e = wrenGetSlotForeign(vm, 0);
	if (e->message != NULL)
		wrenSetSlotString(vm, 0, e->message);
	else
		wrenSetSlotNull(vm, 0);
}

void
fn_WONError_toString(WrenVM *vm)
{
	struct wren_won_error *e = wrenGetSlotForeign(vm, 0);
	const char *name;
	switch ((enum won_err_code)e->code) {
	case WON_ERR_OK:             name = "OK";              break;
	case WON_ERR_SYNTAX:         name = "SYNTAX";          break;
	case WON_ERR_TRUNCATED:      name = "TRUNCATED";       break;
	case WON_ERR_INVALID_ESCAPE: name = "INVALID_ESCAPE";  break;
	case WON_ERR_INVALID_KEY:    name = "INVALID_KEY";     break;
	case WON_ERR_TOO_DEEP:       name = "TOO_DEEP";        break;
	case WON_ERR_TRAILING_DATA:  name = "TRAILING_DATA";   break;
	default:                     name = "UNKNOWN";         break;
	}
	char buf[320];
	if (e->message != NULL)
		snprintf(buf, sizeof(buf),
		    "WONError: %s: %s at offset %d",
		    name, e->message, e->offset);
	else
		snprintf(buf, sizeof(buf),
		    "WONError: %s at offset %d", name, e->offset);
	wrenSetSlotString(vm, 0, buf);
}

/* Build a WONError into slot 0 (the deserializer's return slot).
 * Always writes to slot 0 regardless of which slot the failing parse
 * function was working on — a parse failure invalidates whatever
 * partial value was being built; the caller (top-level
 * fn_WON_deserialize) reads slot 0 unconditionally.  Returns false
 * so callers can `return won_set_error(...)` from their own
 * `bool`-returning bodies. */
static bool
won_set_error(WrenVM *vm, struct won_parser *p, enum won_err_code code,
              const char *fmt, ...)
{
	char msg[200];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);

	struct wren_host_state *st = wren_host_state();
	wrenEnsureSlots(vm, 2);
	load_class_into_slot(vm, &st->won_error_class, "WONError", 1);
	struct wren_won_error *e =
	    wrenSetSlotNewForeign(vm, 0, 1, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type    = SWF_WON_ERROR;
	e->code    = (uint32_t)code;
	e->offset  = (int)(p->pos - p->src);
	e->message = strdup(msg);
	return false;
}

static void
skip_ws(struct won_parser *p)
{
	while (p->pos < p->end) {
		char c = *p->pos;
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
		    c == '\v' || c == '\f') {
			p->pos++;
			continue;
		}
		break;
	}
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool
is_hex(char c)
{
	return is_digit(c) || (c >= 'a' && c <= 'f') ||
	       (c >= 'A' && c <= 'F');
}

static int
hex_val(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static bool parse_value(WrenVM *vm, struct won_parser *p, int slot);

/* Match a literal keyword exactly, advancing the cursor on success. */
static bool
match_keyword(struct won_parser *p, const char *kw, int kwlen)
{
	if (p->end - p->pos < kwlen) return false;
	if (memcmp(p->pos, kw, kwlen) != 0) return false;
	p->pos += kwlen;
	return true;
}

static bool
parse_number(WrenVM *vm, struct won_parser *p, int slot)
{
	const char *start = p->pos;
	bool neg = false;
	if (p->pos < p->end && *p->pos == '-') {
		neg = true;
		p->pos++;
	}

	/* Hex literal: 0x... / 0X... — Wren accepts both.  Read the digits
	 * directly into a uint64 since strtod doesn't grok 0x in C99 mode. */
	if (p->end - p->pos >= 2 && p->pos[0] == '0' &&
	    (p->pos[1] == 'x' || p->pos[1] == 'X')) {
		p->pos += 2;
		const char *hex_start = p->pos;
		uint64_t v = 0;
		while (p->pos < p->end && is_hex(*p->pos)) {
			v = (v << 4) | (uint64_t)hex_val(*p->pos);
			p->pos++;
		}
		if (p->pos == hex_start) {
			return won_set_error(vm, p, WON_ERR_SYNTAX, "expected hex digits after '0x'");
		}
		double d = (double)v;
		if (neg) d = -d;
		wrenSetSlotDouble(vm, slot, d);
		return true;
	}

	/* Decimal: digits[.digits][(e|E)[+-]digits] */
	const char *dig_start = p->pos;
	while (p->pos < p->end && is_digit(*p->pos)) p->pos++;
	if (p->pos == dig_start) {
		return won_set_error(vm, p, WON_ERR_SYNTAX, "expected digit");
	}
	if (p->pos < p->end && *p->pos == '.') {
		p->pos++;
		while (p->pos < p->end && is_digit(*p->pos)) p->pos++;
	}
	if (p->pos < p->end && (*p->pos == 'e' || *p->pos == 'E')) {
		p->pos++;
		if (p->pos < p->end && (*p->pos == '+' || *p->pos == '-'))
			p->pos++;
		const char *exp_start = p->pos;
		while (p->pos < p->end && is_digit(*p->pos)) p->pos++;
		if (p->pos == exp_start) {
			return won_set_error(vm, p, WON_ERR_SYNTAX, "expected digit in exponent");
		}
	}

	/* strtod needs a NUL-terminated buffer; the source span is
	 * not, so copy.  64 bytes covers any double literal with
	 * comfortable headroom. */
	size_t n = (size_t)(p->pos - start);
	if (n >= 64) {
		return won_set_error(vm, p, WON_ERR_SYNTAX, "number literal too long");
	}
	char buf[64];
	memcpy(buf, start, n);
	buf[n] = '\0';
	char *endp;
	double d = strtod(buf, &endp);
	if (endp != buf + n) {
		return won_set_error(vm, p, WON_ERR_SYNTAX, "malformed number");
	}
	wrenSetSlotDouble(vm, slot, d);
	return true;
}

/* Growable byte buffer for string-literal accumulation.  Lives across
 * parse_string()'s lifetime; a single realloc-as-grow strategy is fine
 * for typical input sizes. */
struct strbuf {
	char  *data;
	size_t len;
	size_t cap;
};

static bool
sb_putc(struct strbuf *sb, char c)
{
	if (sb->len == sb->cap) {
		size_t ncap = sb->cap ? sb->cap * 2 : 64;
		char *nbuf = realloc(sb->data, ncap);
		if (nbuf == NULL) return false;
		sb->data = nbuf;
		sb->cap = ncap;
	}
	sb->data[sb->len++] = c;
	return true;
}

/* Encode codepoint cp as UTF-8.  Returns false on invalid codepoint or
 * allocation failure. */
static bool
sb_put_codepoint(struct strbuf *sb, uint32_t cp)
{
	if (cp < 0x80) {
		return sb_putc(sb, (char)cp);
	} else if (cp < 0x800) {
		return sb_putc(sb, (char)(0xC0 | (cp >> 6))) &&
		       sb_putc(sb, (char)(0x80 | (cp & 0x3F)));
	} else if (cp < 0x10000) {
		return sb_putc(sb, (char)(0xE0 | (cp >> 12))) &&
		       sb_putc(sb, (char)(0x80 | ((cp >> 6) & 0x3F))) &&
		       sb_putc(sb, (char)(0x80 | (cp & 0x3F)));
	} else if (cp < 0x110000) {
		return sb_putc(sb, (char)(0xF0 | (cp >> 18))) &&
		       sb_putc(sb, (char)(0x80 | ((cp >> 12) & 0x3F))) &&
		       sb_putc(sb, (char)(0x80 | ((cp >> 6) & 0x3F))) &&
		       sb_putc(sb, (char)(0x80 | (cp & 0x3F)));
	}
	return false;
}

/* Read N hex digits at the cursor into *out, advancing on success. */
static bool
read_hex(struct won_parser *p, int n_digits, uint32_t *out)
{
	if (p->end - p->pos < n_digits) return false;
	uint32_t v = 0;
	for (int i = 0; i < n_digits; i++) {
		int h = hex_val(p->pos[i]);
		if (h < 0) return false;
		v = (v << 4) | (uint32_t)h;
	}
	p->pos += n_digits;
	*out = v;
	return true;
}

/* Parse a "..."-quoted string literal.  Escape set matches Wren's own
 * compiler exactly (see wren_compiler.c readString); accepting any
 * escape Wren itself rejects would break the "output is valid Wren
 * source" property. */
static bool
parse_string(WrenVM *vm, struct won_parser *p, int slot)
{
	if (p->pos >= p->end || *p->pos != '"') {
		return won_set_error(vm, p, WON_ERR_SYNTAX, "expected string");
	}
	p->pos++;
	struct strbuf sb = { NULL, 0, 0 };
	while (p->pos < p->end && *p->pos != '"') {
		char c = *p->pos++;
		if (c != '\\') {
			if (!sb_putc(&sb, c)) goto oom;
			continue;
		}
		if (p->pos >= p->end) {
			free(sb.data);
			return won_set_error(vm, p, WON_ERR_TRUNCATED, "unterminated escape");
		}
		char e = *p->pos++;
		uint32_t cp;
		char out;
		switch (e) {
		case '"':  out = '"';  break;
		case '\\': out = '\\'; break;
		case '%':  out = '%';  break;
		case '0':  out = 0;    break;
		case 'a':  out = '\a'; break;
		case 'b':  out = '\b'; break;
		case 'e':  out = 0x1B; break;
		case 'f':  out = '\f'; break;
		case 'n':  out = '\n'; break;
		case 'r':  out = '\r'; break;
		case 't':  out = '\t'; break;
		case 'v':  out = '\v'; break;
		case 'x':
			if (!read_hex(p, 2, &cp)) {
				free(sb.data);
				return won_set_error(vm, p, WON_ERR_INVALID_ESCAPE, "malformed \\x escape");
			}
			if (!sb_putc(&sb, (char)cp)) goto oom;
			continue;
		case 'u':
			if (!read_hex(p, 4, &cp)) {
				free(sb.data);
				return won_set_error(vm, p, WON_ERR_INVALID_ESCAPE, "malformed \\u escape");
			}
			if (!sb_put_codepoint(&sb, cp)) {
				free(sb.data);
				return won_set_error(vm, p, WON_ERR_INVALID_ESCAPE, "invalid codepoint in \\u escape");
			}
			continue;
		case 'U':
			if (!read_hex(p, 8, &cp)) {
				free(sb.data);
				return won_set_error(vm, p, WON_ERR_INVALID_ESCAPE, "malformed \\U escape");
			}
			if (!sb_put_codepoint(&sb, cp)) {
				free(sb.data);
				return won_set_error(vm, p, WON_ERR_INVALID_ESCAPE, "invalid codepoint in \\U escape");
			}
			continue;
		default:
			free(sb.data);
			return won_set_error(vm, p, WON_ERR_INVALID_ESCAPE, "invalid escape character '\\%c'", e);
		}
		if (!sb_putc(&sb, out)) goto oom;
	}
	if (p->pos >= p->end) {
		free(sb.data);
		return won_set_error(vm, p, WON_ERR_TRUNCATED, "unterminated string");
	}
	p->pos++; /* closing quote */
	wrenSetSlotBytes(vm, slot, sb.data ? sb.data : "", sb.len);
	free(sb.data);
	return true;

oom:
	free(sb.data);
	wren_throw(vm, "WON.deserialize: out of memory");
	return false;
}

static bool
parse_list(WrenVM *vm, struct won_parser *p, int slot)
{
	if (p->pos >= p->end || *p->pos != '[') {
		return won_set_error(vm, p, WON_ERR_SYNTAX, "expected '['");
	}
	p->pos++;
	wrenEnsureSlots(vm, slot + 2);
	wrenSetSlotNewList(vm, slot);
	for (;;) {
		skip_ws(p);
		if (p->pos < p->end && *p->pos == ']') { p->pos++; return true; }
		if (!parse_value(vm, p, slot + 1)) return false;
		/* parse_value may have grown the slot stack — re-establish. */
		wrenEnsureSlots(vm, slot + 2);
		wrenInsertInList(vm, slot, -1, slot + 1);
		skip_ws(p);
		if (p->pos < p->end && *p->pos == ',') {
			p->pos++;
			continue;
		}
		if (p->pos < p->end && *p->pos == ']') {
			p->pos++;
			return true;
		}
		/* EOF mid-list is truncation; bad character is syntax. */
		if (p->pos >= p->end)
			return won_set_error(vm, p, WON_ERR_TRUNCATED,
			    "unterminated List (expected ',' or ']')");
		return won_set_error(vm, p, WON_ERR_SYNTAX,
		    "expected ',' or ']' in List");
	}
}

static bool
parse_map(WrenVM *vm, struct won_parser *p, int slot)
{
	if (p->pos >= p->end || *p->pos != '{') {
		return won_set_error(vm, p, WON_ERR_SYNTAX, "expected '{'");
	}
	p->pos++;
	wrenEnsureSlots(vm, slot + 3);
	wrenSetSlotNewMap(vm, slot);
	for (;;) {
		skip_ws(p);
		if (p->pos < p->end && *p->pos == '}') { p->pos++; return true; }
		/* key */
		if (!parse_value(vm, p, slot + 1)) return false;
		wrenEnsureSlots(vm, slot + 3);
		WrenType kt = wrenGetSlotType(vm, slot + 1);
		if (kt == WREN_TYPE_LIST || kt == WREN_TYPE_MAP) {
			return won_set_error(vm, p, WON_ERR_INVALID_KEY, "List/Map cannot be a Map key");
		}
		skip_ws(p);
		if (p->pos >= p->end)
			return won_set_error(vm, p, WON_ERR_TRUNCATED,
			    "unterminated Map (expected ':' after key)");
		if (*p->pos != ':')
			return won_set_error(vm, p, WON_ERR_SYNTAX, "expected ':' after Map key");
		p->pos++;
		skip_ws(p);
		/* value */
		if (!parse_value(vm, p, slot + 2)) return false;
		wrenEnsureSlots(vm, slot + 3);
		wrenSetMapValue(vm, slot, slot + 1, slot + 2);
		skip_ws(p);
		if (p->pos < p->end && *p->pos == ',') {
			p->pos++;
			continue;
		}
		if (p->pos < p->end && *p->pos == '}') {
			p->pos++;
			return true;
		}
		/* EOF mid-map is truncation; bad character is syntax. */
		if (p->pos >= p->end)
			return won_set_error(vm, p, WON_ERR_TRUNCATED,
			    "unterminated Map (expected ',' or '}')");
		return won_set_error(vm, p, WON_ERR_SYNTAX, "expected ',' or '}' in Map");
	}
}

static bool
parse_value(WrenVM *vm, struct won_parser *p, int slot)
{
	if (p->depth >= WON_MAX_DEPTH) {
		return won_set_error(vm, p, WON_ERR_TOO_DEEP, "nested too deeply");
	}
	p->depth++;
	skip_ws(p);
	if (p->pos >= p->end) {
		p->depth--;
		return won_set_error(vm, p, WON_ERR_TRUNCATED,
		    "unexpected end of input");
	}
	bool ok;
	char c = *p->pos;
	if (c == 'n') {
		ok = match_keyword(p, "null", 4);
		if (ok) wrenSetSlotNull(vm, slot);
		else { p->depth--;
		       return won_set_error(vm, p, WON_ERR_SYNTAX,
		           "expected 'null'"); }
	} else if (c == 't') {
		ok = match_keyword(p, "true", 4);
		if (ok) wrenSetSlotBool(vm, slot, true);
		else { p->depth--;
		       return won_set_error(vm, p, WON_ERR_SYNTAX,
		           "expected 'true'"); }
	} else if (c == 'f') {
		ok = match_keyword(p, "false", 5);
		if (ok) wrenSetSlotBool(vm, slot, false);
		else { p->depth--;
		       return won_set_error(vm, p, WON_ERR_SYNTAX,
		           "expected 'false'"); }
	} else if (c == '"') {
		ok = parse_string(vm, p, slot);
	} else if (c == '[') {
		ok = parse_list(vm, p, slot);
	} else if (c == '{') {
		ok = parse_map(vm, p, slot);
	} else if (c == '-' || (c >= '0' && c <= '9')) {
		ok = parse_number(vm, p, slot);
	} else {
		p->depth--;
		return won_set_error(vm, p, WON_ERR_SYNTAX,
		    "unexpected character '%c'", c);
	}
	p->depth--;
	return ok;
}

void
fn_WON_deserialize(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_STRING) {
		wren_throw(vm, "WON.deserialize: argument must be a String");
		return;
	}
	int len;
	const char *src = wrenGetSlotBytes(vm, 1, &len);
	if (len < 0) len = 0;

	/* Copy the input into a malloc'd buffer.  We're going to overwrite
	 * slot 0 with the parse result and Wren may GC during that, which
	 * could free the ObjString backing `src`.  The copy isolates us. */
	char *text = (char *)malloc((size_t)len + 1);
	if (text == NULL) {
		wren_throw(vm, "WON.deserialize: out of memory");
		return;
	}
	if (len > 0) memcpy(text, src, (size_t)len);
	text[len] = '\0';

	struct won_parser p = {
		.src   = text,
		.end   = text + len,
		.pos   = text,
		.depth = 0,
	};

	wrenEnsureSlots(vm, 3);
	if (parse_value(vm, &p, 0)) {
		skip_ws(&p);
		if (p.pos < p.end)
			won_set_error(vm, &p, WON_ERR_TRAILING_DATA,
			    "trailing data after value");
	}
	free(text);
	/* slot 0 holds either the parsed value (success) or a WONError
	 * (failure — recoverable; the script can `is WONError` check). */
}
