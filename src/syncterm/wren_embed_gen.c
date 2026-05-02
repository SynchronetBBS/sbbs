/* wren_embed_gen — read .wren script files from argv, write a freestanding
 * C source file to stdout that defines:
 *
 *     const struct embedded_script EMBEDDED_SCRIPTS[] = {
 *         { "name", "...source...", "event-or-null" },
 *         ...
 *         { NULL, NULL, NULL }
 *     };
 *
 * `struct embedded_script` is declared in wren_host_internal.h, which the
 * generated file includes.  The output is a real .c — compiled to its own
 * object and linked alongside wren_host.o; the SyncTERM build system runs
 * this tool with the *host* CC (BUILD_CC) so cross-builds work.
 *
 * Plain C99, no platform deps.  Module name = basename of the input path
 * with the trailing ".wren" extension stripped.  Event field is the
 * directory name immediately under `.../scripts/auto/` if present in the
 * path, otherwise NULL — distinguishing auto-load entry scripts (per
 * event) from pure library modules. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *
basename_no_ext(const char *path, char *out, size_t outsz)
{
	const char *base = strrchr(path, '/');
#ifdef _WIN32
	const char *bs = strrchr(path, '\\');
	if (bs != NULL && (base == NULL || bs > base))
		base = bs;
#endif
	base = (base == NULL) ? path : base + 1;

	size_t i = 0;
	while (i + 1 < outsz && base[i] != '\0')
		out[i] = base[i], i++;
	out[i] = '\0';

	char *dot = strrchr(out, '.');
	if (dot != NULL)
		*dot = '\0';
	return out;
}

/* Extract the event-name segment from a path of the form
 * `.../scripts/auto/<event>/<file>.wren`.  Returns out (filled with the
 * event name) or NULL if the path doesn't contain `/auto/<event>/`. */
static const char *
infer_event(const char *path, char *out, size_t outsz)
{
	const char *p = strstr(path, "/auto/");
#ifdef _WIN32
	if (p == NULL)
		p = strstr(path, "\\auto\\");
#endif
	if (p == NULL)
		return NULL;
	p += 6;
	const char *end = p;
	while (*end != '\0' && *end != '/' && *end != '\\')
		end++;
	if (end == p)
		return NULL;
	size_t len = (size_t)(end - p);
	if (len + 1 >= outsz)
		return NULL;
	memcpy(out, p, len);
	out[len] = '\0';
	return out;
}

/* True if `c` is a hex digit (0-9, a-f, A-F).  Used to decide whether
 * a `\xNN` escape needs an empty-string concat after it to keep a
 * following hex character from being absorbed into the same escape. */
static int
is_hex_digit(unsigned char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
	       (c >= 'A' && c <= 'F');
}

/* In-place minify a Wren source buffer.  Strips line and block
 * comments (block comments may nest in Wren) and leading/trailing
 * whitespace per line, but preserves every input newline so that
 * compile error line numbers in the embedded module match the
 * source.  String literals (including their interpolations) pass
 * through verbatim, tracked via a small state stack alternating
 * CODE / STRING; if the stack overflows the tail gets copied
 * without further minification.  Returns the new length. */
static size_t
minify_wren(unsigned char *buf, size_t n)
{
	enum { S_CODE, S_STR };
	int    state[16];
	int    paren[16];   /* only meaningful in S_CODE at sp > 0 */
	size_t cap = sizeof(state) / sizeof(state[0]);
	int    sp = 0;
	state[0] = S_CODE;
	paren[0] = 0;

	size_t r = 0, w = 0;
	int    at_line_start  = 1;

	while (r < n) {
		unsigned char c = buf[r];

		if (state[sp] == S_STR) {
			/* Copy string contents verbatim; watch for `\\`
			 * escape, `"` close, and `%(` interpolation start. */
			buf[w++] = c;
			r++;
			if (c == '\\' && r < n) {
				buf[w++] = buf[r++];
				continue;
			}
			if (c == '"') {
				if (sp > 0)
					sp--;
				at_line_start = 0;
				continue;
			}
			if (c == '%' && r < n && buf[r] == '(') {
				buf[w++] = buf[r++];
				if ((size_t)sp + 1 < cap) {
					sp++;
					state[sp] = S_CODE;
					paren[sp] = 1;
				}
			}
			continue;
		}

		/* S_CODE — top level minifies; nested (sp > 0) just
		 * passes bytes through while tracking parens / strings. */
		if (sp > 0) {
			buf[w++] = c;
			r++;
			if (c == '(') {
				paren[sp]++;
			} else if (c == ')') {
				paren[sp]--;
				if (paren[sp] == 0)
					sp--;
			} else if (c == '"') {
				if ((size_t)sp + 1 < cap) {
					sp++;
					state[sp] = S_STR;
				}
			}
			continue;
		}

		if (at_line_start && (c == ' ' || c == '\t')) {
			r++;
			continue;
		}
		if (c == '\n' || c == '\r') {
			while (w > 0 && (buf[w-1] == ' ' || buf[w-1] == '\t'))
				w--;
			r++;
			if (r < n && (buf[r] == '\n' || buf[r] == '\r') &&
			    buf[r] != c)
				r++;
			buf[w++] = '\n';
			at_line_start = 1;
			continue;
		}
		if (c == '/' && r + 1 < n && buf[r+1] == '/') {
			while (r < n && buf[r] != '\n' && buf[r] != '\r')
				r++;
			continue;
		}
		if (c == '/' && r + 1 < n && buf[r+1] == '*') {
			r += 2;
			int depth = 1;
			while (r < n && depth > 0) {
				if (r + 1 < n && buf[r] == '/' &&
				    buf[r+1] == '*') {
					depth++;
					r += 2;
				} else if (r + 1 < n && buf[r] == '*' &&
				           buf[r+1] == '/') {
					depth--;
					r += 2;
				} else if (buf[r] == '\n') {
					/* Preserve newlines from inside block
					 * comments so following lines retain
					 * their source line numbers. */
					buf[w++] = '\n';
					r++;
				} else if (buf[r] == '\r') {
					r++;
					if (r < n && buf[r] == '\n')
						r++;
					buf[w++] = '\n';
				} else {
					r++;
				}
			}
			continue;
		}
		if (c == '"') {
			buf[w++] = c;
			r++;
			if ((size_t)sp + 1 < cap) {
				sp++;
				state[sp] = S_STR;
			}
			at_line_start = 0;
			continue;
		}
		buf[w++] = c;
		r++;
		at_line_start = 0;
	}
	return w;
}

/* Emit a C string literal for `bytes` (length n) to stdout, broken into
 * one literal per source line so the generated file stays readable. */
static void
emit_c_string(const unsigned char *bytes, size_t n)
{
	fputs("\"", stdout);
	for (size_t i = 0; i < n; i++) {
		unsigned char c = bytes[i];
		switch (c) {
			case '\\': fputs("\\\\", stdout); break;
			case '\"': fputs("\\\"", stdout); break;
			case '\n': fputs("\\n\"\n        \"", stdout); break;
			case '\r': fputs("\\r", stdout); break;
			case '\t': fputs("\\t", stdout); break;
			default:
				if (c < 0x20 || c >= 0x7f) {
					fprintf(stdout, "\\x%02x", c);
					/* C parses \x as "any number of hex
					 * digits"; if the next byte is a hex
					 * digit it gets absorbed into the
					 * current escape and overflows.  Break
					 * the escape with an empty-string
					 * concat. */
					if (i + 1 < n && is_hex_digit(bytes[i + 1]))
						fputs("\"\"", stdout);
				} else {
					putchar(c);
				}
				break;
		}
	}
	fputs("\"", stdout);
}

static int
emit_one(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		fprintf(stderr, "wren_embed_gen: cannot open %s\n", path);
		return -1;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}
	long sz = ftell(f);
	if (sz < 0 || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return -1;
	}
	unsigned char *buf = malloc((size_t)sz);
	if (buf == NULL && sz > 0) {
		fclose(f);
		return -1;
	}
	size_t got = sz > 0 ? fread(buf, 1, (size_t)sz, f) : 0;
	fclose(f);

	got = minify_wren(buf, got);

	char modname[256];
	basename_no_ext(path, modname, sizeof(modname));

	char event[64];
	const char *ev = infer_event(path, event, sizeof(event));

	fprintf(stdout, "    { \"%s\",\n        ", modname);
	emit_c_string(buf, got);
	if (ev != NULL)
		fprintf(stdout, ",\n        \"%s\" },\n", ev);
	else
		fputs(",\n        NULL },\n", stdout);

	free(buf);
	return 0;
}

int
main(int argc, char **argv)
{
	fputs("/* Generated by wren_embed_gen. DO NOT EDIT. */\n", stdout);
	fputs("#include \"wren_host_internal.h\"\n\n", stdout);
	fputs("const struct embedded_script EMBEDDED_SCRIPTS[] = {\n",
	    stdout);

	for (int i = 1; i < argc; i++) {
		if (emit_one(argv[i]) != 0)
			return EXIT_FAILURE;
	}

	fputs("    { NULL, NULL, NULL }\n};\n", stdout);
	return EXIT_SUCCESS;
}
