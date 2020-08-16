/* xpprintf.c */
// vi: tabstop=4

/* Deuce's vs[n]printf() replacement */

/* $Id: xpprintf.c,v 1.62 2018/04/07 07:21:46 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#if defined(__linux__) && !defined(_GNU_SOURCE)
	#define _GNU_SOURCE	// asprintf() on Linux
#endif
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "genwrap.h"    /* alloca() */

#include "xpprintf.h"
#include "gen_defs.h"

#if defined(_MSC_VER) || defined(__MSVCRT__)
int DLLCALL vasprintf(char **strptr, const char *format, va_list va)
{
	va_list	va2;
	int		ret;

	if (strptr == NULL)
		return -1;
	va_copy(va2, va);
	ret = _vscprintf(format, va);
	*strptr = (char *)malloc(ret+1);
	if (*strptr == NULL) {
		va_end(va2);
		return -1;
	}
	ret = vsprintf(*strptr, format, va2);
	va_end(va2);
	return ret;
}

int DLLCALL asprintf(char **strptr, const char *format, ...)
{
	va_list	va;
	int		ret;

	if (strptr == NULL)
		return -1;
	va_start(va, format);
	ret=vasprintf(strptr, format, va);
	va_end(va);
	return ret;
}
#endif

/* Maximum length of a format specifier including the % */
#define MAX_FORMAT_LEN	256

void DLLCALL xp_asprintf_free(char *format)
{
	free(format);
}

int DLLCALL xp_printf_get_type(const char *format)
{
	const char	*p;
	int		modifier=0;
	int		j;
	int		correct_type=0;

	if(!*(size_t *)format)
		return(0);
	p=format+*(size_t *)format;
	if(*p!='%')
		return(0);
	p++;

	/*
	 * Skip flags (zero or more)
	 */
	j=1;
	while(j) {
		switch(*p) {
			case '#':
				p++;
				break;
			case '-':
				p++;
				break;
			case '+':
				p++;
				break;
			case ' ':
				p++;
				break;
			case '0':
				p++;
				break;
			case '\'':
				p++;
				break;
			default:
				j=0;
				break;
		}
	}
	if(*p=='*')
		return(XP_PRINTF_TYPE_INT);
	while(*p>= '0' && *p <= '9')
		p++;
	if(*p=='.') {
		p++;
		if(*p=='*')
			return(XP_PRINTF_TYPE_INT);
	}
	while(*p>= '0' && *p <= '9')
		p++;
	switch(*p) {
		case 'h':
			modifier='h';
			p++;
			if(*p=='h') {
				p++;
				modifier+='h'<<8;
			}
			break;
		case 'l':
			modifier='l';
			p++;
			if(*p=='l') {
				p++;
				modifier+='l'<<8;
			}
			break;
		case 'j':
			modifier='j';
			p++;
			break;
		case 't':
			modifier='t';
			p++;
			break;
		case 'z':
			modifier='z';
			p++;
			break;
		case 'L':
			modifier='L';
			p++;
			break;
	}
	/*
	 * The next char is now the type... if type is auto, 
	 * set type to what it SHOULD be
	 */
	switch(*p) {
		/* INT types */
		case 'd':
		case 'i':
			switch(modifier) {
				case 'h'|'h'<<8:
					correct_type=XP_PRINTF_TYPE_SCHAR;
					break;
				case 'h':
					correct_type=XP_PRINTF_TYPE_SHORT;
					break;
				case 'l':
					correct_type=XP_PRINTF_TYPE_LONG;
					break;
#if defined(XP_PRINTF_TYPE_ULONGLONG)
				case 'l'|'l'<<8:
					correct_type=XP_PRINTF_TYPE_LONGLONG;
					break;
#endif
				case 'j':
					correct_type=XP_PRINTF_TYPE_INTMAX;
					break;
				case 't':
					correct_type=XP_PRINTF_TYPE_PTRDIFF;
					break;
				case 'z':
					/*
					 * ToDo this is a signed type of same size
					 * as size_t
					 */
					correct_type=XP_PRINTF_TYPE_LONG;
					break;
				default:
					correct_type=XP_PRINTF_TYPE_INT;
					break;
			}
			break;
		case 'o':
		case 'u':
		case 'x':
		case 'X':
			switch(modifier) {
				case 'h'|'h'<<8:
					correct_type=XP_PRINTF_TYPE_UCHAR;
					break;
				case 'h':
					correct_type=XP_PRINTF_TYPE_USHORT;
					break;
				case 'l':
					correct_type=XP_PRINTF_TYPE_ULONG;
					break;
#if defined(XP_PRINTF_TYPE_ULONGLONG)
				case 'l'|'l'<<8:
					correct_type=XP_PRINTF_TYPE_ULONGLONG;
					break;
#endif
				case 'j':
					correct_type=XP_PRINTF_TYPE_UINTMAX;
					break;
				case 't':
					/*
					 * ToDo this is an unsigned type of same size
					 * as ptrdiff_t
					 */
					correct_type=XP_PRINTF_TYPE_ULONG;
					break;
				case 'z':
					correct_type=XP_PRINTF_TYPE_SIZET;
					break;
				default:
					correct_type=XP_PRINTF_TYPE_UINT;
					break;
			}
			break;
		case 'a':
		case 'A':
		case 'e':
		case 'E':
		case 'f':
		case 'F':
		case 'g':
		case 'G':
			switch(modifier) {
				case 'L':
					correct_type=XP_PRINTF_TYPE_LONGDOUBLE;
					break;
				case 'l':
				default:
					correct_type=XP_PRINTF_TYPE_DOUBLE;
					break;
			}
			break;
		case 'C':
			/* ToDo wide chars... not yet supported */
			correct_type=XP_PRINTF_TYPE_CHAR;
			break;
		case 'c':
			switch(modifier) {
				case 'l':
					/* ToDo wide chars... not yet supported */
				default:
					correct_type=XP_PRINTF_TYPE_CHAR;
			}
			break;
		case 'S':
			/* ToDo wide chars... not yet supported */
			correct_type=XP_PRINTF_TYPE_CHARP;
			break;
		case 's':
			switch(modifier) {
				case 'l':
					/* ToDo wide chars... not yet supported */
				default:
					correct_type=XP_PRINTF_TYPE_CHARP;
			}
			break;
		case 'p':
			correct_type=XP_PRINTF_TYPE_VOIDP;
			break;
	}
	return(correct_type);
}

/*
 * Performs the next replacement in format using the variable
 * specified as the only vararg which is currently the type
 * specified in type (defined in xpprintf.h).
 *
 * Does not currently support the $ argument selector.
 *
 * Currently, the type is not overly useful, but this could be used for
 * automatic type conversions (ie: int to char *).  Right now it just assures
 * that the type passed to sprintf() is the type passed to
 * xp_asprintf_next().
 */
char* DLLCALL xp_asprintf_next(char *format, int type, ...)
{
	va_list vars;
	char			*p;
	char			*newbuf;
	int				i=0,j;
	unsigned int	ui=0;
	long int		l=0;
	unsigned long int	ul=0;
#if defined(XP_PRINTF_TYPE_LONGLONG)
	long long int	ll=0;
	unsigned long long int	ull=0;
#endif
	double			d=0;
	long double		ld=0;
	char*			cp=NULL;
	void*			pntr=NULL;
	size_t			s=0;
	unsigned long	offset=0;
	unsigned long	offset2=0;
	size_t			format_len;
	size_t			this_format_len;
	char			int_buf[MAX_FORMAT_LEN];
	char			*entry;
	char			this_format[MAX_FORMAT_LEN];
	char			*fmt;
	int				modifier=0;
	int				correct_type=0;
	char			num_str[128];		/* More than enough room for a 256-bit int */

	/*
	 * Check if we're already done...
	 */
	if(!*(size_t *) format)
		return(format);

	p=format+*(size_t *)format;
	offset=p-format;
	format_len=*(size_t *)(format+sizeof(size_t))+sizeof(size_t)*2+1;
	this_format[0]=0;
	fmt=this_format;
	*(fmt++)=*(p++);

	/*
	 * Skip flags (zero or more)
	 */
	j=1;
	while(j) {
		switch(*p) {
			case '#':
				*(fmt++)=*(p++);
				break;
			case '-':
				*(fmt++)=*(p++);
				break;
			case '+':
				*(fmt++)=*(p++);
				break;
			case ' ':
				*(fmt++)=*(p++);
				break;
			case '0':
				*(fmt++)=*(p++);
				break;
			case '\'':
				*(fmt++)=*(p++);
				break;
			default:
				j=0;
				break;
		}
	}

	/*
	 * If width is '*' then the argument is an int
	 * which specifies the width.
	 */
	if(*p=='*') {		/* The argument is this width */
		va_start(vars, type);
		i=sprintf(int_buf,"%d", va_arg(vars, int));
		va_end(vars);
		if(i > 1) {
			/*
			 * We must calculate this before we go mucking about
			 * with format and p
			 */
			offset2=p-format;
			newbuf=(char *)realloc(format, format_len+i-1 /* -1 for the '*' that's already there */);
			if(newbuf==NULL)
				return(NULL);
			format=newbuf;
			p=format+offset2;
			/*
			 * Move trailing end to make space... leaving the * where it
			 * is so it can be overwritten
			 */
			memmove(p+i, p+1, format-p+format_len-1);
			memcpy(p, int_buf, i);
			*(size_t *)(format+sizeof(size_t))+=i-1;
		}
		else
			*p=int_buf[0];
		p=format+offset;
		*(size_t *)format=p-format;
		return(format);
	}
	/* Skip width */
	while(*p >= '0' && *p <= '9')
		*(fmt++)=*(p++);
	/* Check for precision */
	if(*p=='.') {
		*(fmt++)=*(p++);
		/*
		 * If the precision is '*' then the argument is an int which
		 * specifies the precision.
		 */
		if(*p=='*') {
			va_start(vars, type);
			i=sprintf(int_buf,"%d", va_arg(vars, int));
			va_end(vars);
			if(i > 1) {
				/*
				 * We must calculate this before we go mucking about
				 * with format and p
				 */
				offset2=p-format;
				newbuf=(char *)realloc(format, format_len+i-1 /* -1 for the '*' that's already there */);
				if(newbuf==NULL)
					return(NULL);
				format=newbuf;
				p=format+offset2;
				/*
				 * Move trailing end to make space... leaving the * where it
				 * is so it can be overwritten
				 */
				memmove(p+i, p+1, format-p+format_len-1);
				memcpy(p, int_buf, i);
				*(size_t *)(format+sizeof(size_t))+=i-1;
			}
			else
				*p=int_buf[0];
			p=format+offset;
			*(size_t *)format=p-format;
			return(format);
		}
		/* Skip precision */
		while(*p >= '0' && *p <= '9')
			*(fmt++)=*(p++);
	}

	/* Skip/Translate length modifiers */
	/*
	 * ToDo: This could (should?) convert the standard ll modifier
	 * to the MSVC equivilant (I64 or something?)
	 * if you do this, the calculations using this_format_len will need
	 * rewriting.
	 */
	switch(*p) {
		case 'h':
			modifier='h';
			*(fmt++)=*(p++);
			if(*p=='h') {
				*(fmt++)=*(p++);
				modifier+='h'<<8;
			}
			break;
		case 'l':
			modifier='l';
			*(fmt++)=*(p++);
			if(*p=='l') {
				*(fmt++)=*(p++);
				modifier+='l'<<8;
			}
			break;
		case 'j':
			modifier='j';
			*(fmt++)=*(p++);
			break;
		case 't':
			modifier='t';
			*(fmt++)=*(p++);
			break;
		case 'z':
			modifier='z';
			*(fmt++)=*(p++);
			break;
		case 'L':
			modifier='L';
			*(fmt++)=*(p++);
			break;
	}

	/*
	 * The next char is now the type... if type is auto, 
	 * set type to what it SHOULD be
	 */
	if(type==XP_PRINTF_TYPE_AUTO || type & XP_PRINTF_CONVERT) {
		switch(*p) {
			/* INT types */
			case 'd':
			case 'i':
				switch(modifier) {
					case 'h'|'h'<<8:
						correct_type=XP_PRINTF_TYPE_SCHAR;
						break;
					case 'h':
						correct_type=XP_PRINTF_TYPE_SHORT;
						break;
					case 'l':
						correct_type=XP_PRINTF_TYPE_LONG;
						break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
					case 'l'|'l'<<8:
						correct_type=XP_PRINTF_TYPE_LONGLONG;
						break;
#endif
					case 'j':
						correct_type=XP_PRINTF_TYPE_INTMAX;
						break;
					case 't':
						correct_type=XP_PRINTF_TYPE_PTRDIFF;
						break;
					case 'z':
						/*
						 * ToDo this is a signed type of same size
						 * as size_t
						 */
						correct_type=XP_PRINTF_TYPE_LONG;
						break;
					default:
						correct_type=XP_PRINTF_TYPE_INT;
						break;
				}
				break;
			case 'o':
			case 'u':
			case 'x':
			case 'X':
				switch(modifier) {
					case 'h'|'h'<<8:
						correct_type=XP_PRINTF_TYPE_UCHAR;
						break;
					case 'h':
						correct_type=XP_PRINTF_TYPE_USHORT;
						break;
					case 'l':
						correct_type=XP_PRINTF_TYPE_ULONG;
						break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
					case 'l'|'l'<<8:
						correct_type=XP_PRINTF_TYPE_ULONGLONG;
						break;
#endif
					case 'j':
						correct_type=XP_PRINTF_TYPE_UINTMAX;
						break;
					case 't':
						/*
						 * ToDo this is an unsigned type of same size
						 * as ptrdiff_t
						 */
						correct_type=XP_PRINTF_TYPE_ULONG;
						break;
					case 'z':
						correct_type=XP_PRINTF_TYPE_SIZET;
						break;
					default:
						correct_type=XP_PRINTF_TYPE_UINT;
						break;
				}
				break;
			case 'a':
			case 'A':
			case 'e':
			case 'E':
			case 'f':
			case 'F':
			case 'g':
			case 'G':
				switch(modifier) {
					case 'L':
						correct_type=XP_PRINTF_TYPE_LONGDOUBLE;
						break;
					case 'l':
					default:
						correct_type=XP_PRINTF_TYPE_DOUBLE;
						break;
				}
				break;
			case 'C':
				/* ToDo wide chars... not yet supported */
				correct_type=XP_PRINTF_TYPE_CHAR;
				break;
			case 'c':
				switch(modifier) {
					case 'l':
						/* ToDo wide chars... not yet supported */
					default:
						correct_type=XP_PRINTF_TYPE_CHAR;
				}
				break;
			case 'S':
				/* ToDo wide chars... not yet supported */
				correct_type=XP_PRINTF_TYPE_CHARP;
				break;
			case 's':
				switch(modifier) {
					case 'l':
						/* ToDo wide chars... not yet supported */
					default:
						correct_type=XP_PRINTF_TYPE_CHARP;
				}
				break;
			case 'p':
				correct_type=XP_PRINTF_TYPE_VOIDP;
				break;
		}
	}
	if(type==XP_PRINTF_TYPE_AUTO)
		type=correct_type;

	/*
	 * Copy the arg to the passed type.
	 */
	va_start(vars, type);
	switch(type & ~XP_PRINTF_CONVERT) {
		case XP_PRINTF_TYPE_CHAR:
		case XP_PRINTF_TYPE_INT:	/* Also includes char and short */
			i=va_arg(vars, int);
			break;
		case XP_PRINTF_TYPE_UINT:	/* Also includes char and short */
			/*
			 * ToDo: If it's a %c, and the value is 0, should it output [null]
			 * or should it terminate the string?
			 */
			ui=va_arg(vars, unsigned int);
			break;
		case XP_PRINTF_TYPE_LONG:
			l=va_arg(vars, long);
			break;
		case XP_PRINTF_TYPE_ULONG:
			ul=va_arg(vars, unsigned long int);
			break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
		case XP_PRINTF_TYPE_LONGLONG:
			ll=va_arg(vars, long long int);
			break;
		case XP_PRINTF_TYPE_ULONGLONG:
			ull=va_arg(vars, unsigned long long int);
			break;
#endif
		case XP_PRINTF_TYPE_CHARP:
			cp=va_arg(vars, char*);
			break;
		case XP_PRINTF_TYPE_DOUBLE:
			d=va_arg(vars, double);
			break;
		case XP_PRINTF_TYPE_LONGDOUBLE:
			ld=va_arg(vars, long double);
			break;
		case XP_PRINTF_TYPE_VOIDP:
			pntr=va_arg(vars, void*);
			break;
		case XP_PRINTF_TYPE_SIZET:
			s=va_arg(vars, size_t);
			break;
	}
	va_end(vars);

	if(type & XP_PRINTF_CONVERT) {
		type=type & ~XP_PRINTF_CONVERT;
		if(type != correct_type) {
			switch(correct_type) {
				case XP_PRINTF_TYPE_CHAR:
					switch(type) {
						case XP_PRINTF_TYPE_CHAR:
						case XP_PRINTF_TYPE_INT:
							break;
						case XP_PRINTF_TYPE_UINT:
							i=ui;
							break;
						case XP_PRINTF_TYPE_LONG:
							i=l;
							break;
						case XP_PRINTF_TYPE_ULONG:
							i=ul;
							break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
						case XP_PRINTF_TYPE_LONGLONG:
							i=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							i=ull;
							break;
#endif
						case XP_PRINTF_TYPE_CHARP:
							if(cp)
								i=*cp;
							else
								i=0;
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							i=(int)d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							i=(int)ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							i=(long)pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							i=s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_INT:
					switch(type) {
						case XP_PRINTF_TYPE_CHAR:
						case XP_PRINTF_TYPE_INT:
							break;
						case XP_PRINTF_TYPE_UINT:
							i=ui;
							break;
						case XP_PRINTF_TYPE_LONG:
							i=l;
							break;
						case XP_PRINTF_TYPE_ULONG:
							i=ul;
							break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
						case XP_PRINTF_TYPE_LONGLONG:
							i=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							i=ull;
							break;
#endif
						case XP_PRINTF_TYPE_CHARP:
							i=strtol(cp, NULL, 0);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							i=(int)d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							i=(int)ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							i=(long)pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							i=s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_UINT:
					switch(type) {
						case XP_PRINTF_TYPE_CHAR:
						case XP_PRINTF_TYPE_INT:
							ui=i;
							break;
						case XP_PRINTF_TYPE_UINT:
							break;
						case XP_PRINTF_TYPE_LONG:
							ui=l;
							break;
						case XP_PRINTF_TYPE_ULONG:
							ui=ul;
							break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
						case XP_PRINTF_TYPE_LONGLONG:
							ui=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							ui=ull;
							break;
#endif
						case XP_PRINTF_TYPE_CHARP:
							ui=strtoul(cp, NULL, 0);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							ui=(unsigned)d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							ui=(unsigned)ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							ui=(unsigned long)pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							ui=s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_LONG:
					switch(type) {
						case XP_PRINTF_TYPE_CHAR:
						case XP_PRINTF_TYPE_INT:
							l=i;
							break;
						case XP_PRINTF_TYPE_UINT:
							l=ui;
							break;
						case XP_PRINTF_TYPE_LONG:
							break;
						case XP_PRINTF_TYPE_ULONG:
							l=ul;
							break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
						case XP_PRINTF_TYPE_LONGLONG:
							l=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							l=ull;
							break;
#endif
						case XP_PRINTF_TYPE_CHARP:
							l=strtol(cp, NULL, 0);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							l=(long)d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							l=(long)ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							l=(long)pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							l=s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_ULONG:
					switch(type) {
						case XP_PRINTF_TYPE_CHAR:
						case XP_PRINTF_TYPE_INT:
							ul=i;
							break;
						case XP_PRINTF_TYPE_UINT:
							ul=ui;
							break;
						case XP_PRINTF_TYPE_LONG:
							ul=l;
							break;
						case XP_PRINTF_TYPE_ULONG:
							break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
						case XP_PRINTF_TYPE_LONGLONG:
							ul=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							ul=ull;
							break;
#endif
						case XP_PRINTF_TYPE_CHARP:
							ul=strtoul(cp, NULL, 0);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							ul=(unsigned long)d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							ul=(unsigned long)ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							ul=(unsigned long)pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							ul=s;
							break;
					}
					break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
				case XP_PRINTF_TYPE_LONGLONG:
					switch(type) {
						case XP_PRINTF_TYPE_CHAR:
						case XP_PRINTF_TYPE_INT:
							ll=i;
							break;
						case XP_PRINTF_TYPE_UINT:
							ll=ui;
							break;
						case XP_PRINTF_TYPE_LONG:
							ll=l;
							break;
						case XP_PRINTF_TYPE_ULONG:
							ll=ul;
							break;
						case XP_PRINTF_TYPE_LONGLONG:
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							ll=ull;
							break;
						case XP_PRINTF_TYPE_CHARP:
							ll=strtoll(cp, NULL, 0);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							ll=d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							ll=ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							ll=(long long)((intptr_t)pntr);
							break;
						case XP_PRINTF_TYPE_SIZET:
							ll=s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_ULONGLONG:
					switch(type) {
						case XP_PRINTF_TYPE_CHAR:
						case XP_PRINTF_TYPE_INT:
							ull=i;
							break;
						case XP_PRINTF_TYPE_UINT:
							ull=ui;
							break;
						case XP_PRINTF_TYPE_LONG:
							ull=l;
							break;
						case XP_PRINTF_TYPE_ULONG:
							ull=ul;
							break;
						case XP_PRINTF_TYPE_LONGLONG:
							ull=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							break;
						case XP_PRINTF_TYPE_CHARP:
							ull=strtoull(cp, NULL, 0);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							ull=d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							ull=ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							ull=(unsigned long long int)((uintptr_t)pntr);
							break;
						case XP_PRINTF_TYPE_SIZET:
							ull=s;
							break;
					}
					break;
#endif
				case XP_PRINTF_TYPE_CHARP:
					num_str[0]=0;
					switch(type) {
						case XP_PRINTF_TYPE_CHAR:
						case XP_PRINTF_TYPE_INT:
							sprintf(num_str, "%d", i);
							cp=num_str;
							break;
						case XP_PRINTF_TYPE_UINT:
							sprintf(num_str, "%u", i);
							cp=num_str;
							break;
						case XP_PRINTF_TYPE_LONG:
							sprintf(num_str, "%ld", l);
							cp=num_str;
							break;
						case XP_PRINTF_TYPE_ULONG:
							sprintf(num_str, "%lu", ul);
							cp=num_str;
							break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
						case XP_PRINTF_TYPE_LONGLONG:
							/* ToDo MSVC doesn't like this */
							sprintf(num_str, "%lld", ll);
							cp=num_str;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							/* ToDo MSVC doesn't like this */
							sprintf(num_str, "%llu", ull);
							cp=num_str;
							break;
#endif
						case XP_PRINTF_TYPE_CHARP:
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							sprintf(num_str, "%f", d);
							cp=num_str;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							sprintf(num_str, "%Lf", ld);
							cp=num_str;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							/* ToDo: Or should this pretend it's a char *? */
							sprintf(num_str, "%p", pntr);
							cp=num_str;
							break;
						case XP_PRINTF_TYPE_SIZET:
							sprintf(num_str, "%zu", s);
							cp=num_str;
							break;
					}
					break;
				case XP_PRINTF_TYPE_DOUBLE:
					switch(type) {
						case XP_PRINTF_TYPE_CHAR:
						case XP_PRINTF_TYPE_INT:
							d=i;
							break;
						case XP_PRINTF_TYPE_UINT:
							d=ui;
							break;
						case XP_PRINTF_TYPE_LONG:
							d=l;
							break;
						case XP_PRINTF_TYPE_ULONG:
							d=ul;
							break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
						case XP_PRINTF_TYPE_LONGLONG:
							d=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							d=ull;
							break;
#endif
						case XP_PRINTF_TYPE_CHARP:
							d=strtod(cp, NULL);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							d=ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							d=(double)((long int)pntr);
							break;
						case XP_PRINTF_TYPE_SIZET:
							d=s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_LONGDOUBLE:
					switch(type) {
						case XP_PRINTF_TYPE_CHAR:
						case XP_PRINTF_TYPE_INT:
							ld=i;
							break;
						case XP_PRINTF_TYPE_UINT:
							ld=ui;
							break;
						case XP_PRINTF_TYPE_LONG:
							ld=l;
							break;
						case XP_PRINTF_TYPE_ULONG:
							ld=ul;
							break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
						case XP_PRINTF_TYPE_LONGLONG:
							ld=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							ld=ull;
							break;
#endif
						case XP_PRINTF_TYPE_CHARP:
							/* strtold() isn't ubiquitous yet */
							ld=strtod(cp, NULL);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							ld=d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							break;
						case XP_PRINTF_TYPE_VOIDP:
							ld=(long double)((long int)pntr);
							break;
						case XP_PRINTF_TYPE_SIZET:
							ld=s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_VOIDP:
					/* ToDo: this is nasty... */
					switch(type) {
						case XP_PRINTF_TYPE_CHAR:
						case XP_PRINTF_TYPE_INT:
							pntr=(void *)((intptr_t)i);
							break;
						case XP_PRINTF_TYPE_UINT:
							pntr=(void *)((uintptr_t)ui);
							break;
						case XP_PRINTF_TYPE_LONG:
							pntr=(void *)((intptr_t)l);
							break;
						case XP_PRINTF_TYPE_ULONG:
							pntr=(void *)((uintptr_t)ul);
							break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
						case XP_PRINTF_TYPE_LONGLONG:
							pntr=(void *)((intptr_t)ll);
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							pntr=(void *)((uintptr_t)ull);
							break;
#endif
						case XP_PRINTF_TYPE_CHARP:
							pntr=(void *)(cp);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							pntr=(void *)((intptr_t)d);
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							pntr=(void *)((intptr_t)ld);
							break;
						case XP_PRINTF_TYPE_VOIDP:
							break;
						case XP_PRINTF_TYPE_SIZET:
							pntr=(void *)((intptr_t)s);
							break;
					}
					break;
				case XP_PRINTF_TYPE_SIZET:
					switch(type) {
						case XP_PRINTF_TYPE_CHAR:
						case XP_PRINTF_TYPE_INT:
							s=i;
							break;
						case XP_PRINTF_TYPE_UINT:
							s=ui;
							break;
						case XP_PRINTF_TYPE_LONG:
							s=l;
							break;
						case XP_PRINTF_TYPE_ULONG:
							s=ul;
							break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
						case XP_PRINTF_TYPE_LONGLONG:
							s=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							s=ull;
							break;
#endif
						case XP_PRINTF_TYPE_CHARP:
							s=strtol(cp, NULL, 0);	/* was strtoll */
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							s=(size_t)d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							s=(size_t)ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							s=(size_t)pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							break;
					}
					break;
			}
			type=correct_type;
		}
	}

	/* The next char is now the type... check the length required to spore the printf()ed string */
	*(fmt++)=*p;
	*fmt=0;
	entry=NULL;
	switch(type) {
		case XP_PRINTF_TYPE_CHAR:	/* Also includes char and short */
		case XP_PRINTF_TYPE_INT:	/* Also includes char and short */
			j=asprintf(&entry, this_format, i);
			break;
		case XP_PRINTF_TYPE_UINT:	/* Also includes char and short */
			j=asprintf(&entry, this_format, ui);
			break;
		case XP_PRINTF_TYPE_LONG:
			j=asprintf(&entry, this_format, l);
			break;
		case XP_PRINTF_TYPE_ULONG:
			j=asprintf(&entry, this_format, ul);
			break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
		case XP_PRINTF_TYPE_LONGLONG:
			j=asprintf(&entry, this_format, ll);
			break;
		case XP_PRINTF_TYPE_ULONGLONG:
			j=asprintf(&entry, this_format, ull);
			break;
#endif
		case XP_PRINTF_TYPE_CHARP:
			if(cp==NULL)
				j=asprintf(&entry, this_format, "<null>");
			else
				j=asprintf(&entry, this_format, cp);
			break;
		case XP_PRINTF_TYPE_DOUBLE:
			j=asprintf(&entry, this_format, d);
			break;
		case XP_PRINTF_TYPE_LONGDOUBLE:
			j=asprintf(&entry, this_format, ld);
			break;
		case XP_PRINTF_TYPE_VOIDP:
			j=asprintf(&entry, this_format, pntr);
			break;
		case XP_PRINTF_TYPE_SIZET:
			j=asprintf(&entry, this_format, s);
			break;
		default:
			j = -1;
			entry = NULL;
			break;
	}

	if (j<0) {
		FREE_AND_NULL(entry);
		entry = strdup("<error>");
		j=strlen(entry);
	}

	this_format_len=strlen(this_format);
	if(j>=0) {
		/*
		 * This isn't necessary if it's already the right size,
		 * or it's too large... this realloc() should only need to grow
		 * the string.
		 */
		if(format_len < (format_len-this_format_len+j)) {
			newbuf=(char *)realloc(format, format_len-this_format_len+j);
			if(newbuf==NULL) {
				FREE_AND_NULL(entry);
				return(NULL);
			}
			format=newbuf;
		}
		/* Move trailing end to make space */
		memmove(format+offset+j, format+offset+this_format_len, format_len-offset-this_format_len);
		memcpy(format+offset, entry, j);
		p=format+offset+j;
	}
	else
		p=format+offset+this_format_len;
	FREE_AND_NULL(entry);

	*(size_t *)(format+sizeof(size_t))=format_len-this_format_len+j-sizeof(size_t)*2-1;

	/*
	 * Search for next non-%% separateor and set offset
	 * to zero if none found for wrappers to know when
	 * they're done.
	 */
	for(; *p; p++) {
		if(*p=='%') {
			if(*(p+1) == '%')
				p++;
			else
				break;
		}
	}
	if(!*p)
		*(size_t *)format=0;
	else
		*(size_t *)format=p-format;
	return(format);
}

char* DLLCALL xp_asprintf_start(const char *format)
{
	char	*ret;
	char	*p;

	ret=(char *)malloc(strlen(format)+1+((sizeof(size_t)*2)));
	if(ret==NULL)
		return(NULL);
	/* Place current offset at the start of the buffer */
	strcpy(ret+sizeof(size_t)*2,format);
	/* Place the current length after the offset */
	*(size_t *)(ret+sizeof(size_t))=strlen(format);

	/*
	 * Find the next non %% format, leaving %% as it is
	 */
	for(p=ret+sizeof(size_t)*2; *p; p++) {
		if(*p=='%') {
			if(*(p+1) == '%')
				p++;
			else
				break;
		}
	}
	if(!*p)
		*(size_t *)ret=0;
	else
		*(size_t *)ret=p-ret;
	return(ret);
}

char* DLLCALL xp_asprintf_end(char *format, size_t *lenret)
{
	char	*p;
	size_t	len;
	size_t	end_len;

	len=*(size_t *)(format+sizeof(size_t));
	end_len=len;
	for(p=format+sizeof(size_t)*2; len; p++, len--) {
		if(*p=='%' && *(p+1)=='%') {
			memmove(p, p+1, len--);
			end_len--;
		}
	}
	memmove(format, format+sizeof(size_t)*2, end_len+1);
	if(lenret)
		*lenret=end_len;
	return(format);
}

char* DLLCALL xp_vasprintf(const char *format, va_list va)
{
	char	*working;
	char	*next;
	int		type;

	next=xp_asprintf_start(format);
	if(next==NULL)
		return(NULL);
	working=next;
	while(*(size_t *)working) {
		type=xp_printf_get_type(working);
		switch(type) {
			case 0:
				free(working);
				return(NULL);
			case XP_PRINTF_TYPE_CHAR:
			case XP_PRINTF_TYPE_INT:	/* Also includes char and short */
				next=xp_asprintf_next(working, type, va_arg(va, int));
				break;
			case XP_PRINTF_TYPE_UINT:	/* Also includes char and short */
				next=xp_asprintf_next(working, type, va_arg(va, unsigned int));
				break;
			case XP_PRINTF_TYPE_LONG:
				next=xp_asprintf_next(working, type, va_arg(va, long));
				break;
			case XP_PRINTF_TYPE_ULONG:
				next=xp_asprintf_next(working, type, va_arg(va, unsigned long));
				break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
			case XP_PRINTF_TYPE_LONGLONG:
				next=xp_asprintf_next(working, type, va_arg(va, long long));
				break;
			case XP_PRINTF_TYPE_ULONGLONG:
				next=xp_asprintf_next(working, type, va_arg(va, unsigned long long));
				break;
#endif
			case XP_PRINTF_TYPE_CHARP:
				next=xp_asprintf_next(working, type, va_arg(va, char *));
				break;
			case XP_PRINTF_TYPE_DOUBLE:
				next=xp_asprintf_next(working, type, va_arg(va, double));
				break;
			case XP_PRINTF_TYPE_LONGDOUBLE:
				next=xp_asprintf_next(working, type, va_arg(va, long double));
				break;
			case XP_PRINTF_TYPE_VOIDP:
				next=xp_asprintf_next(working, type, va_arg(va, void *));
				break;
			case XP_PRINTF_TYPE_SIZET:
				next=xp_asprintf_next(working, type, va_arg(va, size_t));
				break;
		}
		if(next==NULL) {
			free(working);
			return(NULL);
		}
		working=next;
	}
	next=xp_asprintf_end(working, NULL);
	if(next==NULL) {
		free(working);
		return(NULL);
	}
	return(next);
}

char* DLLCALL xp_asprintf(const char *format, ...)
{
	char	*ret;
	va_list	va;

	va_start(va, format);
	ret=xp_vasprintf(format, va);
	va_end(va);
	return(ret);
}

#if defined(XP_PRINTF_TEST)

int main(int argc, char *argv[])
{
	char	*format;
	char	*p;
	int	i,j;
#if defined(XP_PRINTF_TYPE_LONGLONG)
	long long L;
#endif
	long l;
	char *cp;
	double d;
	float f;
	long double D;

	p=xp_asprintf("%%%%%*.*f %% %%%ss %cs %*.*lu",3,3,123.123456789,"%llutesting%",32,3,3,123);
	printf("%s\n",p);
	free(p);
	if(argc < 2)
		return(1);

	format=argv[1];
	format=xp_asprintf_start(format);
	for(j=2; j<argc; j++) {
		switch(argv[j][0]) {
			case 'f':
				f=(float)atof(argv[j]+1);
				p=xp_asprintf_next(format,XP_PRINTF_CONVERT|XP_PRINTF_TYPE_FLOAT,f);
				break;
			case 'd':
				d=atof(argv[j]+1);
				p=xp_asprintf_next(format,XP_PRINTF_CONVERT|XP_PRINTF_TYPE_DOUBLE,d);
				break;
			case 'D':
				/* Don't know of a thing that converts a string to a long double */
				D=atof(argv[j]+1);
				p=xp_asprintf_next(format,XP_PRINTF_CONVERT|XP_PRINTF_TYPE_LONGDOUBLE,D);
				break;
			case 'i':
				i=atoi(argv[j]+1);
				p=xp_asprintf_next(format,XP_PRINTF_CONVERT|XP_PRINTF_TYPE_INT,i);
				break;
			case 'l':
				l=atol(argv[j]+1);
				p=xp_asprintf_next(format,XP_PRINTF_CONVERT|XP_PRINTF_TYPE_LONG,l);
				break;
#if defined(XP_PRINTF_TYPE_LONGLONG)
			case 'L':
				L=strtoll(argv[j]+1, NULL, 10);
				p=xp_asprintf_next(format,XP_PRINTF_CONVERT|XP_PRINTF_TYPE_LONGLONG,L);
				break;
#endif
			case 's':
				cp=argv[j]+1;
				p=xp_asprintf_next(format,XP_PRINTF_CONVERT|XP_PRINTF_TYPE_CHARP,cp);
				break;
		}
		if(p==NULL) {
			printf("Failed converting on item after %s\n",format);
			return(1);
		}
		format=p;
	}
	p=xp_asprintf_end(format, NULL);
	printf("At end, value is: '%s'\n",p);
	free(p);
}

#endif
