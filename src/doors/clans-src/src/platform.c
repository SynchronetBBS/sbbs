/*
 * General platform functions that do not rely on anything else.
 */

#include <ctype.h>
#include <sys/stat.h>
#include "platform.h"

#ifdef _WIN32
# include "win_wrappers.c"
#else
# include "unix_wrappers.c"
#endif

size_t strlcpy(char *dst, const char *src, size_t dsize)
{
	const char *osrc = src;
	size_t      nleft = dsize;

	/* Copy as many bytes as will fit. */
	if (nleft != 0) {
		while (--nleft != 0) {
			if ((*dst++ = *src++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src. */
	if (nleft == 0) {
		if (dsize != 0)
			*dst = '\0'; /* NUL-terminate dst */
		while (*src++)
			;
	}

	return (size_t)(src - osrc - 1); /* count does not include NUL */
}

size_t
strlcat(char *dst, const char *src, size_t dsize)
{
	const char *odst = dst;
	const char *osrc = src;
	size_t      n = dsize;
	size_t      dlen;

	/* Find the end of dst and adjust bytes left but don't go past end. */
	while (n-- != 0 && *dst != '\0')
		dst++;
	dlen = (size_t)(dst - odst);
	n = dsize - dlen;

	if (n-- == 0)
		return dlen + strlen(src);
	while (*src != '\0') {
		if (n != 0) {
			*dst++ = *src;
			n--;
		}
		src++;
	}
	*dst = '\0';

	return dlen + (size_t)(src - osrc);    /* count does not include NUL */
}

int32_t CRCValue(const void *Data, ptrdiff_t DataSize)
{
	const char *p;
	int32_t CRC = 0;

	p = (char *)Data;

	while (p < ((char *)Data + DataSize)) {
		CRC += (*p);
		p++;
	}

	return CRC;
}

bool isRelative(const char *fname)
{
	if (fname[0] == '/')
		return false;
#ifdef _WIN32
	if (fname[0] == '\\')
		return false;
	if (fname[0] && fname[1] == ':' && (fname[2] == '/' || fname[2] == '\\'))
		return false;
#endif
	return true;
}

bool SameFile(const char *f1, const char *f2)
{
	if (strcasecmp(f1, f2) == 0)
		return true;
	if (isRelative(f1) || isRelative(f2)) {
		const char *n1 = FileName(f1);
		const char *n2 = FileName(f2);
		if (strcasecmp(n1, n2) == 0)
			return true;
	}
	return false;
}

bool DirExists(const char *pszDirName)
{
	if (pszDirName == NULL)
		return false;
	char *szDirFileName = strdup(pszDirName);
#if !defined(_WIN32) & !defined(__unix__)
	struct ffblk DirEntry;
#else
	struct stat file_stats;
#endif

	/* Remove any trailing backslash from directory name */
	if (szDirFileName[strlen(szDirFileName) - 1] == '/' || szDirFileName[strlen(szDirFileName) - 1] == '\\') {
		szDirFileName[strlen(szDirFileName) - 1] = '\0';
	}

	/* Return true if file exists and it is a directory */
#if !defined(_WIN32) & !defined(__unix__)
	return(findfirst(szDirFileName, &DirEntry, FA_ARCH|FA_DIREC) == 0 &&
		   (DirEntry.ff_attrib & FA_DIREC));
#else
	if (stat(szDirFileName, &file_stats) == 0)
		if (S_ISDIR(file_stats.st_mode))
			return true;

	return false;
#endif
}

void Strip(char *szString)
/*
 * This function will strip any leading and trailing spaces from the
 * string.
 */
{
	char *start = szString;
	char *end;
	size_t len;

	// Set start to first non-space character
	while(isspace(*start))
		start++;

	// Set end to the string terminator
	end = strchr(start, 0);

	/*
	 * Set end to last non-space character
	 * if there are no non-space characters, end ends up as start-1
	 */
	end--;
	while (end >= start && isspace(*end))
		end--;

	// Early return if it's all whitespace
	if (end < start) {
		*szString = 0;
		return;
	}

	// Set len to the number of non-whitespace characters
	len = (size_t)(end - start) + 1;
	memmove(szString, start, len);
	szString[len] = 0;
}

bool iscodechar(char c)
/*
 * Returns true if the character is a digit or within 'A' and 'F'
 * (Used mainly to see if it is a valid char for `xx codes.
 *
 */
{
	if ((c <= 'F' && c >= 'A')  || (isdigit(c) && (c >= 32 && c <= 126)))
		return true;
	else
		return false;
}


void RemovePipes(char *pszSrc, char *pszDest)
/*
 * This function removes any colour codes from the string and outputs the
 * result to the destination.
 *
 */
{
	while (*pszSrc) {
		if (*pszSrc == '|' && isdigit(*(pszSrc+1)) && isalnum(*(pszSrc+2)))
			pszSrc += 3;
		else if (*pszSrc == '`' && iscodechar(*(pszSrc+1)) && iscodechar(*(pszSrc+2)))
			pszSrc += 3;
		else {
			/* else is normal */
			*pszDest = *pszSrc;
			++pszSrc;
			++pszDest;
		}
	}
	*pszDest = 0;
}

#define TOTAL_MONTHS    12

static bool IsLeapYear(int year)
{
	if ((year % 4) == 0) {
		if ((year % 100) == 0)
			return (year % 400) == 0;
		return true;
	}
	return false;
}

static int32_t DaysSinceJan1(char szTheDate[])
{
	int CurMonth, ThisMonth, ThisYear, ThisDay;
	const char NumDaysPerMonth[2][TOTAL_MONTHS] = {
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
	};
	int32_t Days = 0;

	ThisMonth = atoi(szTheDate);
	ThisDay = atoi(&szTheDate[3]);
	ThisYear = atoi(&szTheDate[6]);

	for (CurMonth = 1; CurMonth < ThisMonth; CurMonth++) {
		bool LeapYear = IsLeapYear(ThisYear);

		Days += (int32_t) NumDaysPerMonth[LeapYear + 0][CurMonth - 1];
	}

	Days += ThisDay;

	return (Days);

}

int32_t DaysSince1970(char szTheDate[])
{
	int ThisYear, CurYear;
	int32_t Days = 0;

	ThisYear = atoi(&szTheDate[6]);

	for (CurYear = 1970; CurYear < ThisYear; CurYear++) {
		bool LeapYear = IsLeapYear(CurYear);

		if (LeapYear)
			Days += 366;
		else
			Days += 365;
	}

	Days += DaysSinceJan1(szTheDate);

	return (Days);
}

int32_t DaysBetween(char szFirstDate[], char szLastDate[])
/*
 * This function returns the number of days between the first date and
 * last date.
 *
 *
 * DaysBetween = LastDate - FirstDate
 *
 *
 * LastDate is MOST recent date.
 */
{
	int32_t Days;

	Days = DaysSince1970(szLastDate) - DaysSince1970(szFirstDate);

	return (Days);
}
