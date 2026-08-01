/* syncretro_title.c -- a display title from a ROM filename.
 *
 * See syncretro_title.h for what this deliberately does NOT do, and why the
 * lobby's syncretro_parse_title() remains the authoritative parser.
 *
 * Pure: no core, no terminal, no BBS. test_title.c is the gate on it.
 */
#include "syncretro_title.h"

#include <stdio.h>
#include <string.h>

/* Length of s[0..len) with trailing blanks discounted. Returns a length rather
 * than terminating the string: the parse below has to look PAST a group it may
 * decide to keep, and a trim that wrote the NUL would have already destroyed it
 * by the time that decision is made. */
static size_t sr_rtrim_len(const char *s, size_t len)
{
	while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t'))
		len--;
	return len;
}

/* Is s[0..n) a 4-digit year, optionally a range -- "1982", "1982-83"? */
static int sr_is_year(const char *s, size_t n)
{
	size_t i;

	if (n != 4 && n != 7 && n != 9)
		return 0;
	for (i = 0; i < 4; i++)
		if (s[i] < '0' || s[i] > '9')
			return 0;
	if (n == 4)
		return 1;
	if (s[4] != '-')
		return 0;
	for (i = 5; i < n; i++)
		if (s[i] < '0' || s[i] > '9')
			return 0;
	return 1;
}

/* Index of the '<open>' opening a bracket group that ENDS s[0..len), or -1. The
 * body must hold no nested group of the same kind, which is what stops a title's
 * own parenthetical from being mistaken for a trailing tag. */
static int sr_trailing_group(const char *s, size_t len, char open, char close)
{
	size_t i;

	if (len < 2 || s[len - 1] != close)
		return -1;
	for (i = len - 1; i > 0; ) {
		i--;
		if (s[i] == close)
			return -1;
		if (s[i] == open)
			return (int)i;
	}
	return -1;
}

void sr_title_from_rom(char *out, size_t cap, const char *path)
{
	/* Parsed in a buffer of OUR OWN, never the caller's. Truncating the filename
	 * to `cap` first and parsing that leaves a half-eaten "(Joseph" for the
	 * trailing-group scan to misread -- and the title being recovered is exactly
	 * the part that would have fit. */
	char        work[512];
	const char *base = path;
	const char *p, *dot;
	size_t      n, len;
	int         open;

	if (cap == 0)
		return;
	if (path == NULL || *path == '\0') {
		snprintf(out, cap, "%s", "a cartridge");
		return;
	}
	for (p = path; *p != '\0'; p++)
		if (*p == '/' || *p == '\\')
			base = p + 1;
	if (*base == '\0') {            /* a path ending in a separator */
		snprintf(out, cap, "%s", "a cartridge");
		return;
	}

	dot = strrchr(base, '.');
	n   = (dot != NULL && dot != base) ? (size_t)(dot - base) : strlen(base);
	if (n >= sizeof work)
		n = sizeof work - 1;
	memcpy(work, base, n);
	work[n] = '\0';
	len     = n;

	/* Nothing below writes to `work`: every step narrows `len` instead. That is
	 * what lets the publisher branch look back past a group it may keep. */

	/* One or more stacked dump markers: "[!]", "[a1][!]". */
	for (;;) {
		len = sr_rtrim_len(work, len);
		if ((open = sr_trailing_group(work, len, '[', ']')) < 0)
			break;
		len = (size_t)open;
	}
	len = sr_rtrim_len(work, len);

	/* "<title> (<year>) (<publisher>)", or "<title> (<year>)". The publisher is
	 * only taken WITH a year in front of it -- on its own, a trailing
	 * parenthetical is far more likely to be part of the title than a tag. */
	if ((open = sr_trailing_group(work, len, '(', ')')) >= 0) {
		if (sr_is_year(work + open + 1, len - (size_t)open - 2)) {
			len = sr_rtrim_len(work, (size_t)open);
		} else {
			size_t before = sr_rtrim_len(work, (size_t)open);
			int    year   = sr_trailing_group(work, before, '(', ')');

			if (year >= 0 && sr_is_year(work + year + 1, before - (size_t)year - 2))
				len = sr_rtrim_len(work, (size_t)year);
		}
	}

	/* Everything stripped -- a filename that is nothing but tags. Better the raw
	 * basename than an empty status line. */
	if (len == 0)
		len = n;
	if (len >= cap)
		len = cap - 1;
	memcpy(out, work, len);
	out[len] = '\0';
}

