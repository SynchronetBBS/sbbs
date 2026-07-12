// door32.c -- see door32.h.

#include "door32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dirwrap.h"   // xpdev: mkpath()

// basename(arg) == "door32.sys", case-insensitively. Spelled out rather than
// using stricmp/strcasecmp so this file needs no platform header: the doors that
// include it build under MSVC as well as GCC/Clang.
static int tg_ieq(const char *a, const char *b)
{
	for (; *a != '\0' && *b != '\0'; a++, b++) {
		int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
		int cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;

		if (ca != cb)
			return 0;
	}
	return *a == '\0' && *b == '\0';
}

int termgfx_door32_is_path(const char *arg)
{
	const char *b;

	if (arg == NULL || *arg == '\0')
		return 0;
	b = arg + strlen(arg);
	while (b > arg && b[-1] != '/' && b[-1] != '\\')
		b--;
	return tg_ieq(b, "door32.sys");
}

/* atoi() is the wrong tool here and it took a probe to see why: atoi("") and
 * atoi("garbage") both return 0 -- and 0 is a MEANINGFUL comm type ("local", i.e.
 * a stdio door). So a truncated or corrupt drop file would make a door believe
 * the BBS had redirected its stdio, and ignore the perfectly good socket on the
 * next line. A missing number must read as UNKNOWN, never as local.
 *
 * Returns 1 and sets *out on success; 0 if the line holds no number at all. */
static int tg_parse_int(const char *line, int *out)
{
	const char *p = line;
	int         sign = 1, digits = 0, v = 0;

	while (*p == ' ' || *p == '\t')
		p++;
	if (*p == '-' || *p == '+') {
		sign = (*p == '-') ? -1 : 1;
		p++;
	}
	for (; *p >= '0' && *p <= '9'; p++) {
		v = v * 10 + (*p - '0');
		digits++;
	}
	if (digits == 0)
		return 0;
	*out = v * sign;
	return 1;
}

int termgfx_door32_read(const char *path, termgfx_door32_t *d)
{
	FILE *f;
	char  line[256];
	int   ln = 0;
	int   commtype = -1, handle = -1, tmin = -1, node = 0;

	if (d == NULL)
		return -1;
	memset(d, 0, sizeof(*d));
	d->socket   = -1;
	d->commtype = -1;

	if (path == NULL || (f = fopen(path, "r")) == NULL)
		return -1;

	// The line numbers below are 0-BASED (an fgets counter); the spec numbers its
	// lines from 1, so "line 1/2/7/9/11" of the file is ln 0/1/6/8/10 here. Every
	// door that copied this got the off-by-one right; the comment is what kept
	// them honest, so it moves here with the code.
	while (fgets(line, sizeof(line), f) != NULL) {
		switch (ln) {
			case 0: (void)tg_parse_int(line, &commtype); break;   // 0=local 1=serial 2=telnet
			case 1: (void)tg_parse_int(line, &handle);   break;   // comm/socket descriptor
			case 6: {                               // user alias / handle
				// memcpy, not snprintf("%s"): the truncation here is DELIBERATE (a
				// 256-byte line into a 64-byte alias), and snprintf earns a
				// -Wformat-truncation warning for it -- which -Werror turns into a
				// build failure on some of this project's targets. The doors learned
				// this one already; it moves here with the code.
				size_t n;

				line[strcspn(line, "\r\n")] = '\0';
				n = strlen(line);
				if (n >= sizeof(d->alias))
					n = sizeof(d->alias) - 1;
				memcpy(d->alias, line, n);
				d->alias[n] = '\0';
				break;
			}
			case 8:  (void)tg_parse_int(line, &tmin); break;      // time left, MINUTES
			case 10: (void)tg_parse_int(line, &node); break;      // node number
		}
		if (++ln > 10)
			break;
	}
	fclose(f);

	d->commtype = commtype;

	// A socket only when the BBS says it gave us one AND the handle is usable. A
	// comm type of 2 with a bogus handle is not a socket -- it is a broken drop
	// file, and pretending otherwise gets us a door that draws to a dead fd.
	if (commtype == 2 && handle >= 0)
		d->socket = handle;
	else if (commtype == 0)
		// LOCAL: there is no socket because the BBS redirected our stdio. See
		// door32.h -- this is the case four of the five doors never knew about.
		d->stdio = 1;

	if (tmin > 0)
		d->time_limit_ms = (uint32_t)tmin * 60000u;
	if (node > 0)
		d->node = node;

	return 0;
}

const char *termgfx_door32_why_unusable(const termgfx_door32_t *d)
{
	if (d == NULL)
		return "no drop file";
	if (d->socket >= 0 || d->stdio)
		return NULL;                    // usable: a socket, or the BBS's stdio
	switch (d->commtype) {
		case -1: return "no comm type on line 1 (the drop file is truncated or corrupt)";
		case 1:  return "comm type 1 = SERIAL, which these doors do not speak";
		case 2:  return "comm type 2 = telnet, but line 2 is not a usable socket handle";
		default: return "an unknown comm type on line 1";
	}
}

int termgfx_stderr_capture(const char *doorname, int node)
{
	const char *data = getenv("SBBSDATA");
	char        path[512];

	if (doorname == NULL || doorname[0] == '\0')
		return -1;

	if (data != NULL && data[0] != '\0') {
		size_t      n   = strlen(data);
		const char *sep = (n > 0 && (data[n - 1] == '/' || data[n - 1] == '\\')) ? "" : "/";
		char        dir[512];

		snprintf(dir, sizeof dir, "%s%s%s", data, sep, doorname);
		mkpath(dir);                       // generated runtime data belongs in data/
		if (node > 0)
			snprintf(path, sizeof path, "%s/%s_n%d.log", dir, doorname, node);
		else
			snprintf(path, sizeof path, "%s/%s.log", dir, doorname);
	} else {
		snprintf(path, sizeof path, "%s.log", doorname);
	}

	if (freopen(path, "w", stderr) == NULL)
		return -1;                         // leave stderr alone rather than lose it
	// Unbuffered: a door that dies by _exit() (the hangup path) must not lose the
	// buffered tail that says why.
	setvbuf(stderr, NULL, _IONBF, 0);
	return 0;
}
