#include "deucegate.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "dirwrap.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#define DG_SEP '\\'
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define DG_SEP '/'
#endif

static const char *level_names[] = {"DEBUG", "INFO", "WARN", "ERROR"};

void
dg_log(dg_log_level_t level, const char *fmt, ...)
{
	time_t now = time(NULL);
	struct tm tmv;
	char stamp[32];
	va_list ap;
#ifdef _WIN32
	localtime_s(&tmv, &now);
#else
	localtime_r(&now, &tmv);
#endif
	strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tmv);
	fprintf(stderr, "%s %-5s ", stamp, level_names[level]);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	fflush(stderr);
}

bool
dg_path_join(char *out, size_t outsz, const char *a, const char *b)
{
	size_t alen;
	if (out == NULL || outsz == 0 || a == NULL || b == NULL)
		return false;
	alen = strlen(a);
	while (*b == '/' || *b == '\\')
		b++;
	int written;
	if (alen > 0 && (a[alen - 1] == '/' || a[alen - 1] == '\\'))
		written = snprintf(out, outsz, "%s%s", a, b);
	else
		written = snprintf(out, outsz, "%s%c%s", a, DG_SEP, b);
	return written >= 0 && (size_t)written < outsz;
}

bool
dg_file_exists(const char *path)
{
	struct stat st;
	return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool
dg_dir_exists(const char *path)
{
	struct stat st;
	return path != NULL && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool
dg_read_file(const char *path, uint8_t **data, size_t *len)
{
	FILE *fp;
	long sz;
	uint8_t *buf;
	if (data == NULL || len == NULL || (fp = fopen(path, "rb")) == NULL)
		return false;
	if (fseek(fp, 0, SEEK_END) != 0 || (sz = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return false;
	}
	buf = malloc((size_t)sz + 1);
	if (buf == NULL || fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
		free(buf);
		fclose(fp);
		return false;
	}
	buf[sz] = 0;
	fclose(fp);
	*data = buf;
	*len = (size_t)sz;
	return true;
}

bool
dg_mkdir_parent(const char *path)
{
	char tmp[DG_PATH_MAX];
	char *slash;
	if (path == NULL || strlen(path) >= sizeof(tmp))
		return false;
	strcpy(tmp, path);
	slash = strrchr(tmp, '/');
#ifdef _WIN32
	{
		char *backslash = strrchr(tmp, '\\');
		if (backslash != NULL && (slash == NULL || backslash > slash))
			slash = backslash;
	}
#endif
	if (slash == NULL)
		return true;
	*slash = 0;
	return *tmp == 0 || dg_dir_exists(tmp) || mkpath(tmp) == 0;
}

bool
dg_write_atomic(const char *path, const void *data, size_t len, unsigned mode)
{
	char tmp[DG_PATH_MAX];
	FILE *fp;
	bool ok;
	if (!dg_mkdir_parent(path) || snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp))
		return false;
	fp = fopen(tmp, "wb");
	if (fp == NULL)
		return false;
	ok = fwrite(data, 1, len, fp) == len && fflush(fp) == 0;
#ifdef _WIN32
	if (ok)
		ok = _commit(_fileno(fp)) == 0;
#else
	if (ok)
		ok = fsync(fileno(fp)) == 0;
#endif
	if (fclose(fp) != 0)
		ok = false;
#ifndef _WIN32
	if (ok && chmod(tmp, (mode_t)mode) != 0)
		ok = false;
#else
	(void)mode;
#endif
	if (ok) {
#ifdef _WIN32
		ok = MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
		ok = rename(tmp, path) == 0;
#endif
	}
	if (!ok)
		remove(tmp);
	return ok;
}

bool
dg_alias_valid(const char *alias)
{
	size_t i, len;
	if (alias == NULL || (len = strlen(alias)) == 0 || len >= DG_ALIAS_MAX)
		return false;
	if (isspace((unsigned char)alias[0]) || isspace((unsigned char)alias[len - 1]))
		return false;
	for (i = 0; i < len; i++) {
		unsigned char ch = (unsigned char)alias[i];
		if (ch < 0x20 || ch == 0x7f || ch == '/' || ch == '\\' || ch == ':' || ch == '[' || ch == ']')
			return false;
	}
	return true;
}

int
dg_stricmp(const char *a, const char *b)
{
	for (;;) {
		int ac = tolower((unsigned char)*a++);
		int bc = tolower((unsigned char)*b++);
		if (ac != bc || ac == 0)
			return ac - bc;
	}
}

char *
dg_trim(char *s)
{
	char *end;
	while (isspace((unsigned char)*s))
		s++;
	end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1]))
		*--end = 0;
	return s;
}

static bool
subtag_chars(const char *subtag, size_t len, int (*check)(int))
{
	for (size_t i = 0; i < len; i++)
		if (!check((unsigned char)subtag[i]))
			return false;
	return true;
}

bool
dg_language_tag_valid(const char *tag)
{
	static const char *grandfathered[] = {
	    "en-GB-oed", "i-ami", "i-bnn", "i-default", "i-enochian", "i-hak",
	    "i-klingon", "i-lux", "i-mingo", "i-navajo", "i-pwn", "i-tao", "i-tay",
	    "i-tsu", "sgn-BE-FR", "sgn-BE-NL", "sgn-CH-DE", "art-lojban", "cel-gaulish",
	    "no-bok", "no-nyn", "zh-guoyu", "zh-hakka", "zh-min", "zh-min-nan", "zh-xiang",
	    NULL
	};
	const char *parts[32];
	size_t lengths[32], count = 0, len, at = 0;
	const char *start;

	if (tag == NULL || (len = strlen(tag)) == 0 || len > DG_LANGUAGE_TAG_MAX)
		return false;
	for (size_t i = 0; grandfathered[i] != NULL; i++)
		if (dg_stricmp(tag, grandfathered[i]) == 0)
			return true;
	start = tag;
	for (const char *p = tag;; p++) {
		if (*p != '-' && *p != 0) {
			if (!isalnum((unsigned char)*p))
				return false;
			continue;
		}
		if (p == start || (size_t)(p - start) > 8 || count >= 32)
			return false;
		parts[count] = start;
		lengths[count++] = (size_t)(p - start);
		if (*p == 0)
			break;
		start = p + 1;
	}
	if (lengths[0] == 1 && tolower((unsigned char)parts[0][0]) == 'x')
		return count > 1;
	if (!subtag_chars(parts[0], lengths[0], isalpha)
	    || lengths[0] < 2 || lengths[0] > 8)
		return false;
	at = 1;
	if (lengths[0] <= 3) {
		for (unsigned extlangs = 0; at < count && extlangs < 3 && lengths[at] == 3
		    && subtag_chars(parts[at], lengths[at], isalpha); extlangs++)
			at++;
	}
	if (at < count && lengths[at] == 4 && subtag_chars(parts[at], lengths[at], isalpha))
		at++;
	if (at < count && ((lengths[at] == 2 && subtag_chars(parts[at], lengths[at], isalpha))
	    || (lengths[at] == 3 && subtag_chars(parts[at], lengths[at], isdigit))))
		at++;
	while (at < count && ((lengths[at] >= 5 && subtag_chars(parts[at], lengths[at], isalnum))
	    || (lengths[at] == 4 && isdigit((unsigned char)parts[at][0])
	    && subtag_chars(parts[at] + 1, 3, isalnum))))
		at++;
	while (at < count && lengths[at] == 1
	    && tolower((unsigned char)parts[at][0]) != 'x') {
		at++;
		if (at == count || lengths[at] < 2)
			return false;
		while (at < count && lengths[at] >= 2)
			at++;
	}
	if (at < count && lengths[at] == 1 && tolower((unsigned char)parts[at][0]) == 'x')
		return at + 1 < count;
	return at == count;
}

bool
dg_language_from_locale(const char *locale, char *tag, size_t tagsz)
{
	size_t used = 0;

	if (locale == NULL || tag == NULL || tagsz == 0 || dg_stricmp(locale, "C") == 0
	    || dg_stricmp(locale, "POSIX") == 0)
		return false;
	for (const char *p = locale; *p != 0 && *p != '.' && *p != '@'; p++) {
		char ch = *p == '_' ? '-' : *p;

		if (used + 1 >= tagsz)
			return false;
		tag[used++] = ch;
	}
	tag[used] = 0;
	if (!dg_language_tag_valid(tag)) {
		tag[0] = 0;
		return false;
	}
	return true;
}
