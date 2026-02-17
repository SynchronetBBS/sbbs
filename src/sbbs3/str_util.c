/* Synchronet string utility routines */

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

#include "genwrap.h"
#include "dirwrap.h"
#include "str_util.h"
#include "utf8.h"
#include "unicode.h"
#include "cp437defs.h"

/****************************************************************************/
/* For all the functions that take a 'dest' argument, pass NULL to have the	*/
/* function malloc the buffer for you and return it.						*/
/****************************************************************************/

/****************************************************************************/
/* Removes ctrl-a codes from the string 'str'								*/
/****************************************************************************/
char* remove_ctrl_a(const char *str, char *dest)
{
	int i, j;

	if (dest == NULL && (dest = strdup(str)) == NULL)
		return NULL;
	for (i = j = 0; str[i]; i++) {
		if (str[i] == CTRL_A) {
			i++;
			if (str[i] == 0 || str[i] == 'Z')    /* EOF */
				break;
			/* convert non-destructive backspace to a destructive backspace */
			if (str[i] == '<' && j)
				j--;
			else if (str[i] == '/') { // Conditional new-line
				dest[j++] = '\r';
				dest[j++] = '\n';
			}
		}
		else
			dest[j++] = str[i];
	}
	dest[j] = 0;
	return dest;
}

char* strip_ctrl(const char *str, char* dest)
{
	int i, j;

	if (dest == NULL && (dest = strdup(str)) == NULL)
		return NULL;
	for (i = j = 0; str[i]; i++) {
		if (str[i] == CTRL_A) {
			i++;
			if (str[i] == 0 || str[i] == 'Z')    /* EOF */
				break;
			/* convert non-destructive backspace to a destructive backspace */
			if (str[i] == '<' && j)
				j--;
		}
		else if ((uchar)str[i] >= ' ' && str[i] != DEL)
			dest[j++] = str[i];
	}
	dest[j] = 0;
	return dest;
}

char* strip_ansi(char* str)
{
	char* s = str;
	char* d = str;
	while (*s != '\0') {
		if (*s == ESC && *(s + 1) == '[') {
			s += 2;
			while (*s != '\0' && (*s < '@' || *s > '~'))
				s++;
			if (*s != '\0') // Skip "final byte""
				s++;
		} else {
			*(d++) = *(s++);
		}
	}
	*d = '\0';
	return str;
}

// Sort of a stripped down version of ANS2ASC
char* convert_ansi(const char* src, char* dest, size_t len, int width, bool ice_color)
{
	const char* s = src;
	char*       d = dest;
	char*       p;
	ulong       n[10];
	size_t      nc;
	int         column = 0;
	while (*s != '\0' && d < dest + len) {
		if (*s == ESC && *(s + 1) == '[') {
			s += 2;
			nc = 0;
			do {
				n[nc] = strtoul(s, &p, 10);
				if (p == s || p == NULL)
					break;
				nc++;
				s = p;
				if (*s != ';')
					break;
				s++;
			} while (nc < sizeof(n) / sizeof(n[0]));
			while (*s != '\0' && (*s < '@' || *s > '~'))
				s++;
			if (*s == 'C') { // Cursor right
				if (n[0] < 1)
					n[0] = 1;
				while (n[0] >= 1 && d < dest + len) {
					*(d++) = ' ';
					n[0]--;
					column++;
				}
			} else if (*s == 'm') { // Color / Attributes
				for (size_t i = 0; i < nc && d < dest + len; i++) {
					*(d++) = CTRL_A;
					switch (n[i]) {
						case 0:
						case 2:
							*(d++) = 'N';
							break;
						case 1:
							*(d++) = 'H';
							break;
						case 3:
						case 4:
						case 5:                 /* blink */
						case 6:
						case 7:
							*(d++) = ice_color ? 'E': 'I';
							break;
						case 30:
							*(d++) = 'K';
							break;
						case 31:
							*(d++) = 'R';
							break;
						case 32:
							*(d++) = 'G';
							break;
						case 33:
							*(d++) = 'Y';
							break;
						case 34:
							*(d++) = 'B';
							break;
						case 35:
							*(d++) = 'M';
							break;
						case 36:
							*(d++) = 'C';
							break;
						case 37:
							*(d++) = 'W';
							break;
						case 40:
						case 41:
						case 42:
						case 43:
						case 44:
						case 45:
						case 46:
						case 47:
							*(d++) = '0' + ((int)n[i] - 40);
							break;
					}
				}
			}
			if (*s != '\0') // Skip "final byte"
				s++;
		} else {
			if (*s == '\r' || *s == '\n') {
				*(d++) = *(s++);
				column = 0;
			} else {
				if (width && column >= width) {
					d += sprintf(d, "\1+\1N\1/\1-");    // Save, normal, cond-newline, restore
					column = 0;
				}
				*(d++) = *(s++);
				column++;
			}
		}
	}
	*d = '\0';
	return dest;
}

char* strip_exascii(const char *str, char* dest)
{
	int i, j;

	if (dest == NULL && (dest = strdup(str)) == NULL)
		return NULL;
	for (i = j = 0; str[i]; i++)
		if (!(str[i] & 0x80))
			dest[j++] = str[i];
	dest[j] = 0;
	return dest;
}

char* strip_cp437_graphics(const char *str, char* dest)
{
	int i, j;

	if (dest == NULL && (dest = strdup(str)) == NULL)
		return NULL;
	for (i = j = 0; str[i]; i++)
		if ((uchar)str[i] <= (uchar)CP437_INVERTED_EXCLAMATION_MARK
		    || (uchar)str[i] >= (uchar)CP437_GREEK_SMALL_LETTER_ALPHA)
			dest[j++] = str[i];
	dest[j] = 0;
	return dest;
}

char* strip_space(const char *str, char* dest)
{
	int i, j;

	if (dest == NULL && (dest = strdup(str)) == NULL)
		return NULL;
	for (i = j = 0; str[i]; i++)
		if (!IS_WHITESPACE(str[i]))
			dest[j++] = str[i];
	dest[j] = 0;
	return dest;
}

char* strip_char(const char* str, char* dest, char ch)
{
	const char* src;

	if (dest == NULL && (dest = strdup(str)) == NULL)
		return NULL;
	char*       retval = dest;
	for (src = str; *src != '\0'; src++) {
		if (*src != ch)
			*(dest++) = *src;
	}
	*dest = '\0';
	return retval;
}

/****************************************************************************/
/* Returns in 'string' a character representation of the number in l with   */
/* thousands separators (e.g. commas).										*/
/****************************************************************************/
char* u32toac(uint32_t l, char *string, char sep)
{
	char str[256];
	int  i, j, k;

	ultoa(l, str, 10);
	i = strlen(str) - 1;
	j = i / 3 + 1 + i;
	string[j--] = 0;
	for (k = 1; i > -1; k++) {
		string[j--] = str[i--];
		if (j > 0 && !(k % 3))
			string[j--] = sep;
	}
	return string;
}

char* u64toac(uint64_t l, char *string, char sep)
{
	char str[256];
	int  i, j, k;

	_ui64toa(l, str, 10);
	i = strlen(str) - 1;
	j = i / 3 + 1 + i;
	string[j--] = 0;
	for (k = 1; i > -1; k++) {
		string[j--] = str[i--];
		if (j > 0 && !(k % 3))
			string[j--] = sep;
	}
	return string;
}

/****************************************************************************/
/* Truncate string at first occurrence of char in specified character set	*/
/* Returns a pointer to the terminating NUL if the string was truncated,	*/
/* NULL otherwise.															*/
/****************************************************************************/
char* truncstr(char* str, const char* set)
{
	char* p;

	p = strpbrk(str, set);
	if (p != NULL && *p != 0)
		*p = 0;

	return p;
}

/****************************************************************************/
/* Truncate string at first occurrence of char in specified character set	*/
/* Returns a pointer to the start of the string.							*/
/****************************************************************************/
char* truncated_str(char* str, const char* set)
{
	truncstr(str, set);
	return str;
}

/****************************************************************************/
/* Truncates the specified substring from the end of the string if it		*/
/* exists.																	*/
/****************************************************************************/
void remove_end_substr(char* str, const char* substr)
{
	size_t str_len = strlen(str);
	size_t sub_len = strlen(substr);
	if (sub_len == 0 || str_len < sub_len)
		return;
	if (strcmp(str + (str_len - sub_len), substr) == 0)
		*(str + (str_len - sub_len)) = '\0';
}

/****************************************************************************/
/* rot13 encoder/decoder - courtesy of Mike Acar							*/
/****************************************************************************/
char* rot13(char* str)
{
	char  ch, cap;
	char* p;

	p = str;
	while ((ch = *p) != 0) {
		cap = ch & 32;
		ch &= ~cap;
		ch = ((ch >= 'A') && (ch <= 'Z') ? ((ch - 'A' + 13) % 26 + 'A') : ch) | cap;
		*(p++) = ch;
	}

	return str;
}

/****************************************************************************/
/* Puts a backslash on path strings if not just a drive letter and colon	*/
/****************************************************************************/
char* backslashcolon(char *str)
{
	int i;

	i = strlen(str);
	if (i && !IS_PATH_DELIM(str[i - 1]) && str[i - 1] != ':') {
		str[i] = PATH_DELIM;
		str[i + 1] = 0;
	}

	return str;
}

/****************************************************************************/
/* Compares pointers to pointers to char. Used in conjunction with qsort()  */
/****************************************************************************/
int pstrcmp(const char **str1, const char **str2)
{
	return strcmp(*str1, *str2);
}

/****************************************************************************/
/* Returns the number of characters that are the same between str1 and str2 */
/****************************************************************************/
int strsame(const char *str1, const char *str2)
{
	int i, j = 0;

	for (i = 0; str1[i]; i++)
		if (str1[i] == str2[i])
			j++;
	return j;
}

/****************************************************************************/
/* Converts an ASCII Hex string into an ulong                               */
/* by Steve Deppe (Ille Homine Albe)										*/
/****************************************************************************/
ulong ahtoul(const char *str)
{
	ulong l, val = 0;

	while ((l = (*str++) | 0x20) != 0x20)
		val = (l & 0xf) + (l >> 6 & 1) * 9 + val * 16;
	return val;
}

/****************************************************************************/
/* Converts an ASCII Hex string into an uint32_t                            */
/* by Steve Deppe (Ille Homine Albe)										*/
/****************************************************************************/
uint32_t ahtou32(const char *str)
{
	uint32_t l, val = 0;

	while ((l = (*str++) | 0x20) != 0x20)
		val = (l & 0xf) + (l >> 6 & 1) * 9 + val * 16;
	return val;
}

/****************************************************************************/
/* Returns true if a is a valid ctrl-a "attribute" code, false if it isn't. */
/****************************************************************************/
bool valid_ctrl_a_attr(char a)
{
	switch (toupper(a)) {
		case '+':   /* push attr	*/
		case '-':   /* pop attr		*/
		case '_':   /* clear        */
		case 'B':   /* blue     fg  */
		case 'C':   /* cyan     fg  */
		case 'G':   /* green    fg  */
		case 'H':   /* high     fg  */
		case 'E':   /* high		bg	*/
		case 'I':   /* blink        */
		case 'K':   /* black    fg  */
		case 'M':   /* magenta  fg  */
		case 'N':   /* normal       */
		case 'R':   /* red      fg  */
		case 'W':   /* white    fg  */
		case 'U':   /* user theme */
		case 'V':   /* mnemonics */
		/* "Rainbow" attribute is not valid for messages (no ANSI equivalent)
				case 'X':	// rainbow
		*/
		case 'Y':   /* yellow   fg  */
		case '0':   /* black    bg  */
		case '1':   /* red      bg  */
		case '2':   /* green    bg  */
		case '3':   /* brown    bg  */
		case '4':   /* blue     bg  */
		case '5':   /* magenta  bg  */
		case '6':   /* cyan     bg  */
		case '7':   /* white    bg  */
			return true;
	}
	return false;
}

/****************************************************************************/
/* Returns true if a is a valid QWKnet compatible Ctrl-A code, else false	*/
/****************************************************************************/
bool valid_ctrl_a_code(char a)
{
	switch (toupper(a)) {
		case 'P':       /* Pause */
		case 'L':       /* CLS */
		case ',':       /* 100ms delay */
			return true;
	}
	return valid_ctrl_a_attr(a);
}

/****************************************************************************/
/****************************************************************************/
char ctrl_a_to_ascii_char(char a)
{
	switch (toupper(a)) {
		case 'L':   /* cls          */
			return FF;
		case '<':   /* backspace	*/
			return '\b';
		case '[':   /* CR			*/
			return '\r';
		case ']':   /* LF			*/
			return '\n';
	}
	return 0;
}

/****************************************************************************/
/* Strips invalid Ctrl-Ax "attribute" sequences from str                    */
/* Returns number of ^A's in line                                           */
/****************************************************************************/
size_t strip_invalid_attr(char *str)
{
	char*  dest;
	size_t a, c, d;

	dest = str;
	for (a = c = d = 0; str[c]; c++) {
		if (str[c] == CTRL_A) {
			a++;
			if (str[c + 1] == 0)
				break;
			if (!valid_ctrl_a_attr(str[c + 1])) {
				/* convert non-destructive backspace to a destructive backspace */
				if (str[c + 1] == '<' && d)
					d--;
				c++;
				continue;
			}
		}
		dest[d++] = str[c];
	}
	dest[d] = 0;
	return a;
}

/****************************************************************************/
/* Detects valid Ctrl-Ax "attribute" sequences in str                    	*/
/****************************************************************************/
bool contains_ctrl_a_attr(const char *str)
{

	while (*str != '\0') {
		if (*str == CTRL_A) {
			++str;
			if (valid_ctrl_a_attr(*str))
				return true;
		}
		++str;
	}
	return false;
}

/****************************************************************************/
/****************************************************************************/
char exascii_to_ascii_char(uchar ch)
{
	/* Seven bit table for EXASCII to ASCII conversion */
	const char *sbtbl = "CUeaaaaceeeiiiAAEaAooouuyOUcLYRfaiounNao?--24!<>"
	                    "###||||++||++++++--|-+||++--|-+----++++++++##[]#"
	                    "abrpEout*ono%0ENE+><rj%=o*.+n2* ";

	if (ch & 0x80)
		return sbtbl[ch ^ 0x80];
	return ch;
}

bool str_is_ascii(const char* str)
{
	for (const char* p = str; *p != 0; p++) {
		if (*p < 0)
			return false;
	}
	return true;
}

bool str_has_ctrl(const char* str)
{
	for (const char* p = str; *p != 0; p++) {
		if ((uchar) * p < ' ')
			return true;
	}
	return false;
}

/****************************************************************************/
/* Convert string from IBM extended ASCII to just ASCII						*/
/****************************************************************************/
char* ascii_str(uchar* str)
{
	uchar* p = str;

	while (*p) {
		if ((*p) & 0x80)
			*p = exascii_to_ascii_char(*p);
		p++;
	}
	return (char*)str;
}

/****************************************************************************/
// Replaces named variables in the source string with their values from the
// provided lists of named variables. The replaced string is returned in the
// provided buffer.
// strlist_list is a NULL-terminated list of named_string_t* lists, which are
// searched in order for variable matches. string_list and int_list termniated
// by a named item with a NULL name. If a variable is found in more than one list,
// the first match is used (search order is strlist_list, then string_list, then
// int_list).
// If an escape sequence is provided, it is used to skip variable replacement
// If case_sensitive is false, variable names are matched case-insensitively.
/****************************************************************************/
char* replace_named_values(const char* src
                           , char* buf
                           , size_t buflen /* includes '\0' terminator */
                           , const char* escape_seq
                           , named_string_t** strlist_list
                           , named_string_t* string_list
                           , named_long_t* int_list
                           , bool case_sensitive)
{
	char   val[32];
	size_t i;
	size_t esc_len = 0;
	size_t name_len;
	size_t value_len;
	char*  p = buf;
	int    (*cmp)(const char*, const char*, size_t);

	if (case_sensitive)
		cmp = strncmp;
	else
		cmp = strnicmp;

	if (escape_seq != NULL)
		esc_len = strlen(escape_seq);

	while (*src && (size_t)(p - buf) < buflen - 1) {
		if (esc_len) {
			if (cmp(src, escape_seq, esc_len) != 0) {
				*p++ = *src++;
				continue;
			}
			src += esc_len;  /* skip the escape seq */
		}
		if (strlist_list) {
			for (i = 0; strlist_list[i] != NULL && strlist_list[i]->name != NULL /* terminator */; i++) {
				name_len = strlen(strlist_list[i]->name);
				if (cmp(src, strlist_list[i]->name, name_len) == 0) {
					if (strlist_list[i]->value != NULL) {
						value_len = strlen(strlist_list[i]->value);
						if ((p - buf) + value_len > buflen - 1)  /* buffer overflow? */
							value_len = (buflen - 1) - (p - buf); /* truncate value */
						memcpy(p, strlist_list[i]->value, value_len);
						p += value_len;
					}
					src += name_len;
					break;
				}
			}
			if (strlist_list[i] != NULL) /* variable match */
				continue;
		}
		if (string_list) {
			for (i = 0; string_list[i].name != NULL /* terminator */; i++) {
				name_len = strlen(string_list[i].name);
				if (cmp(src, string_list[i].name, name_len) == 0) {
					if (string_list[i].value != NULL) {
						value_len = strlen(string_list[i].value);
						if ((p - buf) + value_len > buflen - 1)  /* buffer overflow? */
							value_len = (buflen - 1) - (p - buf); /* truncate value */
						memcpy(p, string_list[i].value, value_len);
						p += value_len;
					}
					src += name_len;
					break;
				}
			}
			if (string_list[i].name != NULL) /* variable match */
				continue;
		}
		if (int_list) {
			for (i = 0; int_list[i].name != NULL /* terminator */; i++) {
				name_len = strlen(int_list[i].name);
				if (cmp(src, int_list[i].name, name_len) == 0) {
					SAFEPRINTF(val, "%ld", int_list[i].value);
					value_len = strlen(val);
					if ((p - buf) + value_len > buflen - 1)  /* buffer overflow? */
						value_len = (buflen - 1) - (p - buf); /* truncate value */
					memcpy(p, val, value_len);
					p += value_len;
					src += name_len;
					break;
				}
			}
			if (int_list[i].name != NULL) /* variable match */
				continue;
		}

		*p++ = *src++;
	}
	*p = 0;  /* terminate string in destination buffer */

	return buf;
}

/****************************************************************************/
/****************************************************************************/
char* replace_chars(char *str, char c1, char c2)
{
	char* p;

	REPLACE_CHARS(str, c1, c2, p);
	return str;
}

/****************************************************************************/
/* Condense consecutive white-space chars in a string to single spaces		*/
/****************************************************************************/
char* condense_whitespace(char* str)
{
	char* s = str;
	char* d = str;
	while (*s != '\0') {
		if (IS_WHITESPACE(*s)) {
			*(d++) = ' ';
			SKIP_WHITESPACE(s);
		} else {
			*(d++) = *(s++);
		}
	}
	*d = '\0';
	return str;
}

uint32_t str_to_bits(uint32_t val, const char *str)
{
	/* op can be 0 for replace, + for add, or - for remove */
	int         op = 0;
	const char *s;
	char        ctrl;

	for (s = str; *s; s++) {
		if (*s == '+')
			op = 1;
		else if (*s == '-')
			op = 2;
		else {
			if (!op) {
				val = 0;
				op = 1;
			}
			ctrl = toupper(*s);
			ctrl &= 0x1f;         /* Ensure it fits */
			switch (op) {
				case 1:     /* Add to the set */
					val |= 1 << ctrl;
					break;
				case 2:     /* Remove from the set */
					val &= ~(1 << ctrl);
					break;
			}
		}
	}
	return val;
}

/* Convert a UTF-8 encoded string to a CP437-encoded string */
char* utf8_to_cp437_inplace(char* str)
{
	utf8_normalize_str(str);
	return utf8_replace_chars(str, unicode_to_cp437
	                          , /* unsupported char: */ CP437_INVERTED_QUESTION_MARK
	                          , /* unsupported zero-width ch: */ 0
	                          , /* decode error char: */ CP437_INVERTED_EXCLAMATION_MARK);
}

char* separate_thousands(const char* src, char *dest, size_t maxlen, char sep)
{
	if (strlen(src) * 1.3 > maxlen)
		return (char*)src;
	const char* tail = src;
	while (*tail && IS_DIGIT(*tail))
		tail++;
	if (tail == src)
		return (char*)src;
	size_t digits = tail - src;
	char*  d = dest;
	for (size_t i = 0; i < digits; d++, i++) {
		*d = src[i];
		if (i + 3 < digits && (digits - (i + 1)) % 3 == 0)
			*(++d) = sep;
	}
	*d = 0;
	strcpy(d, tail);
	return dest;
}

// Update 'str' to conform with RFC 5536 requirements of a newsgroup name
char* make_newsgroup_name(char* str)
{
	/*
	 * From RFC5536:
	 * newsgroup-name  =  component *( "." component )
	 * component       =  1*component-char
	 * component-char  =  ALPHA / DIGIT / "+" / "-" / "_"
	 */
	if (str[0] == '.')
		str[0] = '_';
	size_t c;
	for (c = 0; str[c] != 0; c++) {
		/* Legal characters */
		if ((str[c] >= 'A' && str[c] <= 'Z')
		    || (str[c] >= 'a' && str[c] <= 'z')
		    || (str[c] >= '0' && str[c] <= '9')
		    || str[c] == '+'
		    || str[c] == '-'
		    || str[c] == '_'
		    || str[c] == '.')
			continue;
		str[c] = '_';
	}
	c--;
	if (str[c] == '.')
		str[c] = '_';
	return str;
}

size_t widest_line(const char* str)
{
	size_t widest = 0;
	size_t width = 0;

	for (size_t i = 0; str[i] != '\0'; ++i) {
		if (str[i] == '\n') {
			if (width > widest)
				widest = width;
			width = 0;
		} else if(str[i] == CTRL_A) {
			++i;
			if (str[i] == '\0' || str[i] == 'Z')
				break;
		} else if(str[i] != '\r')
			++width;
	}
	if (width > widest)
		return width;
	return widest;
}
