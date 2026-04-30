/* WOM (Wren Object Model) deserialiser.  Hand-written recursive-descent
 * parser over the same literal subset that the Wren-side WOM.serialize
 * produces.  Crucially this is NOT eval — the input never reaches the
 * Wren compiler — so it is safe to feed untrusted text.
 *
 * Public API is fn_WOM_deserialize, declared in wren_bind_wom.h and
 * registered as `WOM.deserialize(_)` in the BINDINGS table in
 * wren_bind.c. */

#include "wren_bind_internal.h"
#include "wren_bind_wom.h"

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
#define WOM_MAX_DEPTH 256

struct wom_parser {
	const char *src;   /* start of buffer (for error offset reports) */
	const char *end;   /* one-past-end */
	const char *pos;   /* current cursor */
	int depth;
};

/* Format an error and abort the calling fiber.  Includes the cursor
 * offset so the Wren-side caller can locate the bad token. */
static void
wom_throw(WrenVM *vm, struct wom_parser *p, const char *fmt, ...)
{
	char inner[160];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(inner, sizeof inner, fmt, ap);
	va_end(ap);
	char full[256];
	snprintf(full, sizeof full,
	    "WOM.deserialize: %s at offset %ld",
	    inner, (long)(p->pos - p->src));
	wren_throw(vm, full);
}

static void
skip_ws(struct wom_parser *p)
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

static bool parse_value(WrenVM *vm, struct wom_parser *p, int slot);

/* Match a literal keyword exactly, advancing the cursor on success. */
static bool
match_keyword(struct wom_parser *p, const char *kw, int kwlen)
{
	if (p->end - p->pos < kwlen) return false;
	if (memcmp(p->pos, kw, kwlen) != 0) return false;
	p->pos += kwlen;
	return true;
}

static bool
parse_number(WrenVM *vm, struct wom_parser *p, int slot)
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
			wom_throw(vm, p, "expected hex digits after '0x'");
			return false;
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
		wom_throw(vm, p, "expected digit");
		return false;
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
			wom_throw(vm, p, "expected digit in exponent");
			return false;
		}
	}

	/* strtod needs a NUL-terminated buffer; the source span is
	 * not, so copy.  64 bytes covers any double literal with
	 * comfortable headroom. */
	size_t n = (size_t)(p->pos - start);
	if (n >= 64) {
		wom_throw(vm, p, "number literal too long");
		return false;
	}
	char buf[64];
	memcpy(buf, start, n);
	buf[n] = '\0';
	char *endp;
	double d = strtod(buf, &endp);
	if (endp != buf + n) {
		wom_throw(vm, p, "malformed number");
		return false;
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
read_hex(struct wom_parser *p, int n_digits, uint32_t *out)
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
parse_string(WrenVM *vm, struct wom_parser *p, int slot)
{
	if (p->pos >= p->end || *p->pos != '"') {
		wom_throw(vm, p, "expected string");
		return false;
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
			wom_throw(vm, p, "unterminated escape");
			return false;
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
				wom_throw(vm, p, "malformed \\x escape");
				return false;
			}
			if (!sb_putc(&sb, (char)cp)) goto oom;
			continue;
		case 'u':
			if (!read_hex(p, 4, &cp)) {
				free(sb.data);
				wom_throw(vm, p, "malformed \\u escape");
				return false;
			}
			if (!sb_put_codepoint(&sb, cp)) {
				free(sb.data);
				wom_throw(vm, p, "invalid codepoint in \\u escape");
				return false;
			}
			continue;
		case 'U':
			if (!read_hex(p, 8, &cp)) {
				free(sb.data);
				wom_throw(vm, p, "malformed \\U escape");
				return false;
			}
			if (!sb_put_codepoint(&sb, cp)) {
				free(sb.data);
				wom_throw(vm, p, "invalid codepoint in \\U escape");
				return false;
			}
			continue;
		default:
			free(sb.data);
			wom_throw(vm, p, "invalid escape character '\\%c'", e);
			return false;
		}
		if (!sb_putc(&sb, out)) goto oom;
	}
	if (p->pos >= p->end) {
		free(sb.data);
		wom_throw(vm, p, "unterminated string");
		return false;
	}
	p->pos++; /* closing quote */
	wrenSetSlotBytes(vm, slot, sb.data ? sb.data : "", sb.len);
	free(sb.data);
	return true;

oom:
	free(sb.data);
	wren_throw(vm, "WOM.deserialize: out of memory");
	return false;
}

static bool
parse_list(WrenVM *vm, struct wom_parser *p, int slot)
{
	if (p->pos >= p->end || *p->pos != '[') {
		wom_throw(vm, p, "expected '['");
		return false;
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
		wom_throw(vm, p, "expected ',' or ']' in List");
		return false;
	}
}

static bool
parse_map(WrenVM *vm, struct wom_parser *p, int slot)
{
	if (p->pos >= p->end || *p->pos != '{') {
		wom_throw(vm, p, "expected '{'");
		return false;
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
			wom_throw(vm, p, "List/Map cannot be a Map key");
			return false;
		}
		skip_ws(p);
		if (p->pos >= p->end || *p->pos != ':') {
			wom_throw(vm, p, "expected ':' after Map key");
			return false;
		}
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
		wom_throw(vm, p, "expected ',' or '}' in Map");
		return false;
	}
}

static bool
parse_value(WrenVM *vm, struct wom_parser *p, int slot)
{
	if (p->depth >= WOM_MAX_DEPTH) {
		wom_throw(vm, p, "nested too deeply");
		return false;
	}
	p->depth++;
	skip_ws(p);
	if (p->pos >= p->end) {
		wom_throw(vm, p, "unexpected end of input");
		p->depth--;
		return false;
	}
	bool ok;
	char c = *p->pos;
	if (c == 'n') {
		ok = match_keyword(p, "null", 4);
		if (ok) wrenSetSlotNull(vm, slot);
		else wom_throw(vm, p, "expected 'null'");
	} else if (c == 't') {
		ok = match_keyword(p, "true", 4);
		if (ok) wrenSetSlotBool(vm, slot, true);
		else wom_throw(vm, p, "expected 'true'");
	} else if (c == 'f') {
		ok = match_keyword(p, "false", 5);
		if (ok) wrenSetSlotBool(vm, slot, false);
		else wom_throw(vm, p, "expected 'false'");
	} else if (c == '"') {
		ok = parse_string(vm, p, slot);
	} else if (c == '[') {
		ok = parse_list(vm, p, slot);
	} else if (c == '{') {
		ok = parse_map(vm, p, slot);
	} else if (c == '-' || (c >= '0' && c <= '9')) {
		ok = parse_number(vm, p, slot);
	} else {
		wom_throw(vm, p, "unexpected character '%c'", c);
		ok = false;
	}
	p->depth--;
	return ok;
}

void
fn_WOM_deserialize(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_STRING) {
		wren_throw(vm, "WOM.deserialize: argument must be a String");
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
		wren_throw(vm, "WOM.deserialize: out of memory");
		return;
	}
	if (len > 0) memcpy(text, src, (size_t)len);
	text[len] = '\0';

	struct wom_parser p = {
		.src   = text,
		.end   = text + len,
		.pos   = text,
		.depth = 0,
	};

	wrenEnsureSlots(vm, 3);
	if (parse_value(vm, &p, 0)) {
		skip_ws(&p);
		if (p.pos < p.end)
			wom_throw(vm, &p, "trailing data after value");
	}
	free(text);
	/* On success, slot 0 holds the parsed value (the return).  On
	 * failure, the fiber is already aborted with the error message in
	 * slot 0. */
}
