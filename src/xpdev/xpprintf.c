#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xpprintf.h"

/* MSVC Sucks - can't tell the required len of a *printf() */
/*
 * ToDo: don't cripple everyone because of this... see
 * conio/ciolib.c cprintf() for details
 */
#define MAX_ARG_LEN		10240

/* Maximum length of a format specifier including the % */
#define MAX_FORMAT_LEN	256

/*
 * Performs the next replacement in format using the variable
 * specified as the only vararg which is currently the type
 * specified in type (defined in xpprintf.h).
 *
 * Does not currently support the $ argument selector.
 *
 * Currently, the type is not overly usefull, but this could be used for
 * automatic type conversions (ie: int to char *).  Right now it just assures
 * that the type passed to sprintf() is the type passed to
 * xp_asprintf_next().
 */
char *xp_asprintf_next(char *format, int type, ...)
{
	va_list vars;
	char			*p;
	char			*newbuf;
	int				i,j;
	unsigned int	ui;
	long int		l;
	unsigned long int	ul;
	long long int	ll;
	unsigned long long int	ull;
	double			d;
	long double		ld;
	char*			cp;
	void*			pntr;
	size_t			s;
	unsigned long	offset=0;
	size_t			format_len;
	size_t			this_format_len;
	size_t			tail_len;
	char			entry_buf[MAX_ARG_LEN];
	char			this_format[MAX_FORMAT_LEN];
	char			*fmt;
	int				modifier=0;
	char			*fmt_start;
	int				correct_type=0;
	char			num_str[128];		/* More than enough room for a 256-bit int */

	/*
	 * Find the next non %% format, leaving %% as it is
	 */
	for(p=format+*(size_t *)format; *p; p++) {
		if(*p=='%') {
			if(*(p+1) == '%')
				p++;
			else
				break;
		}
	}
	if(!*p)
		return(format);
	offset=p-format;
	format_len=strlen(format+sizeof(size_t))+sizeof(size_t);
	this_format[0]=0;
	fmt=this_format;
	fmt_start=p;
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
	 * If width is '*' then the argument is an unsigned int
	 * which specifies the width.
	 */
	if(*p=='*') {		/* The argument is this width */
		va_start(vars, type);
		i=sprintf(entry_buf,"%u", va_arg(vars, int));
		va_end(vars);
		if(i > 1) {
			/*
			 * We must calculate this before we go mucking about
			 * with format and p
			 */
			offset=p-format;
			newbuf=(char *)realloc(format, format_len+i /* -1 for the '*' that's already there, +1 for the terminator */);
			if(newbuf==NULL)
				return(NULL);
			format=newbuf;
			p=format+offset;
			/*
			 * Move trailing end to make space... leaving the * where it
			 * is so it can be overwritten
			 */
			memmove(p+i, p+1, format-p+format_len);
			memcpy(p, entry_buf, i);
		}
		else
			*p=entry_buf[0];
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
		 * If the precision is '*' then the argument is an unsigned int which
		 * specifies the precision.
		 */
		if(*p=='*') {
			va_start(vars, type);
			i=sprintf(entry_buf,"%u", va_arg(vars, int));
			va_end(vars);
			if(i > 1) {
				/*
				 * We must calculate this before we go mucking about
				 * with format and p
				 */
				offset=p-format;
				newbuf=(char *)realloc(format, format_len+i /* -1 for the '*' that's already there, +1 for the terminator */);
				if(newbuf==NULL)
					return(NULL);
				format=newbuf;
				p=format+offset;
				/*
				 * Move trailing end to make space... leaving the * where it
				 * is so it can be overwritten
				 */
				memmove(p+i, p+1, format-p+format_len);
				memcpy(p, entry_buf, i);
			}
			else
				*p=entry_buf[0];
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
			modifier='h';
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
					case 'l'|'l'<<8:
						correct_type=XP_PRINTF_TYPE_LONGLONG;
						break;
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
					case 'l'|'l'<<8:
						correct_type=XP_PRINTF_TYPE_ULONGLONG;
						break;
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
		case XP_PRINTF_TYPE_INT:	/* Also includes char and short */
			i=va_arg(vars, int);
			break;
		case XP_PRINTF_TYPE_UINT:	/* Also includes char and short */
			ui=va_arg(vars, unsigned int);
			break;
		case XP_PRINTF_TYPE_LONG:
			l=va_arg(vars, long);
			break;
		case XP_PRINTF_TYPE_ULONG:
			ul=va_arg(vars, unsigned long int);
			break;
		case XP_PRINTF_TYPE_LONGLONG:
			ll=va_arg(vars, long long int);
			break;
		case XP_PRINTF_TYPE_ULONGLONG:
			ull=va_arg(vars, unsigned long long int);
			break;
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
				case XP_PRINTF_TYPE_INT:
					switch(type) {
						case XP_PRINTF_TYPE_INT:
							i=i;
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
						case XP_PRINTF_TYPE_LONGLONG:
							i=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							i=ull;
							break;
						case XP_PRINTF_TYPE_CHARP:
							i=strtol(cp, NULL, 0);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							i=d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							i=ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							i=(int)pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							i=s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_UINT:
					switch(type) {
						case XP_PRINTF_TYPE_INT:
							ui=i;
							break;
						case XP_PRINTF_TYPE_UINT:
							ui=ui;
							break;
						case XP_PRINTF_TYPE_LONG:
							ui=l;
							break;
						case XP_PRINTF_TYPE_ULONG:
							ui=ul;
							break;
						case XP_PRINTF_TYPE_LONGLONG:
							ui=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							ui=ull;
							break;
						case XP_PRINTF_TYPE_CHARP:
							ui=strtoul(cp, NULL, 0);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							ui=d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							ui=ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							ui=(unsigned int)pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							ui=s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_LONG:
					switch(type) {
						case XP_PRINTF_TYPE_INT:
							l=i;
							break;
						case XP_PRINTF_TYPE_UINT:
							l=ui;
							break;
						case XP_PRINTF_TYPE_LONG:
							l=l;
							break;
						case XP_PRINTF_TYPE_ULONG:
							l=ul;
							break;
						case XP_PRINTF_TYPE_LONGLONG:
							l=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							l=ull;
							break;
						case XP_PRINTF_TYPE_CHARP:
							l=strtol(cp, NULL, 0);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							l=d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							l=ld;
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
							ul=ul;
							break;
						case XP_PRINTF_TYPE_LONGLONG:
							ul=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							ul=ull;
							break;
						case XP_PRINTF_TYPE_CHARP:
							ul=strtoul(cp, NULL, 0);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							ul=d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							ul=ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							ul=(unsigned long)pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							ul=s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_LONGLONG:
					switch(type) {
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
							ll=ll;
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
							ll=(long long)pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							ll=s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_ULONGLONG:
					switch(type) {
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
							ull=ull;
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
							ull=(unsigned long long int)pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							ull=s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_CHARP:
					num_str[0]=0;
					switch(type) {
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
						case XP_PRINTF_TYPE_CHARP:
							cp=cp;
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							sprintf(num_str, "%f", d);
							cp=num_str;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							sprintf(num_str, "%Lf", d);
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
						case XP_PRINTF_TYPE_LONGLONG:
							d=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							d=ull;
							break;
						case XP_PRINTF_TYPE_CHARP:
							d=strtod(cp, NULL);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							d=d;
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
						case XP_PRINTF_TYPE_LONGLONG:
							ld=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							ld=ull;
							break;
						case XP_PRINTF_TYPE_CHARP:
							/* strtold() isn't ubiquitous yet */
							ld=strtod(cp, NULL);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							ld=d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							ld=ld;
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
						case XP_PRINTF_TYPE_INT:
							pntr=(void *)i;
							break;
						case XP_PRINTF_TYPE_UINT:
							pntr=(void *)ui;
							break;
						case XP_PRINTF_TYPE_LONG:
							pntr=(void *)l;
							break;
						case XP_PRINTF_TYPE_ULONG:
							pntr=(void *)ul;
							break;
						case XP_PRINTF_TYPE_LONGLONG:
							pntr=(void *)ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							pntr=(void *)ull;
							break;
						case XP_PRINTF_TYPE_CHARP:
							pntr=(void *)cp;
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							pntr=(void *)(long int)d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							pntr=(void *)(long int)ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							pntr=pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							pntr=(void *)s;
							break;
					}
					break;
				case XP_PRINTF_TYPE_SIZET:
					switch(type) {
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
						case XP_PRINTF_TYPE_LONGLONG:
							s=ll;
							break;
						case XP_PRINTF_TYPE_ULONGLONG:
							s=ull;
							break;
						case XP_PRINTF_TYPE_CHARP:
							s=strtoll(cp, NULL, 0);
							break;
						case XP_PRINTF_TYPE_DOUBLE:
							s=d;
							break;
						case XP_PRINTF_TYPE_LONGDOUBLE:
							s=ld;
							break;
						case XP_PRINTF_TYPE_VOIDP:
							s=(size_t)pntr;
							break;
						case XP_PRINTF_TYPE_SIZET:
							s=s;
							break;
					}
					break;
			}
		}
	}

	/* The next char is now the type... perform native sprintf() using it */
	*(fmt++)=*p;
	*fmt=0;
	switch(type) {
		case XP_PRINTF_TYPE_INT:	/* Also includes char and short */
			j=sprintf(entry_buf, this_format, i);
			break;
		case XP_PRINTF_TYPE_UINT:	/* Also includes char and short */
			j=sprintf(entry_buf, this_format, ui);
			break;
		case XP_PRINTF_TYPE_LONG:
			j=sprintf(entry_buf, this_format, l);
			break;
		case XP_PRINTF_TYPE_ULONG:
			j=sprintf(entry_buf, this_format, ul);
			break;
		case XP_PRINTF_TYPE_LONGLONG:
			j=sprintf(entry_buf, this_format, ll);
			break;
		case XP_PRINTF_TYPE_ULONGLONG:
			j=sprintf(entry_buf, this_format, ull);
			break;
		case XP_PRINTF_TYPE_CHARP:
			if(cp==NULL)
				j=sprintf(entry_buf, this_format, "<null>");
			else
				j=sprintf(entry_buf, this_format, cp);
			break;
		case XP_PRINTF_TYPE_DOUBLE:
			j=sprintf(entry_buf, this_format, d);
			break;
		case XP_PRINTF_TYPE_LONGDOUBLE:
			j=sprintf(entry_buf, this_format, ld);
			break;
		case XP_PRINTF_TYPE_VOIDP:
			j=sprintf(entry_buf, this_format, pntr);
			break;
		case XP_PRINTF_TYPE_SIZET:
			j=sprintf(entry_buf, this_format, s);
			break;
	}

	this_format_len=strlen(this_format);
	/*
	 * This isn't necessary if it's already the right size,
	 * or it's too large... this realloc() should only need to grow
	 * the string.
	 */
	newbuf=(char *)realloc(format, format_len-this_format_len+j+1);
	if(newbuf==NULL)
		return(NULL);
	format=newbuf;
	/* Move trailing end to make space */
	memmove(format+offset+j, format+offset+this_format_len, offset+format_len-this_format_len+1);
	memcpy(format+offset, entry_buf, j);
	p=format+offset+j;
	*(size_t *)format=p-format;
	return(format);
}

char *xp_asprintf_start(char *format)
{
	char	*p;

	p=(char *)malloc(strlen(format)+1+(sizeof(size_t)));
	if(p==NULL)
		return(NULL);
	/* Place current offset at the start of the buffer */
	*(int *)p=sizeof(size_t);
	strcpy(p+sizeof(size_t),format);
	return(p);
}

char *xp_asprintf_end(char *format)
{
	char	*p;
	size_t	len;

	len=strlen(format+sizeof(size_t));
	for(p=format+sizeof(size_t); *p; p++, len--) {
		if(*p=='%' && *(p+1)=='%')
			memmove(p, p+1, len--);
	}
	memmove(format, format+sizeof(size_t), strlen(format+sizeof(size_t))+1);
	return(format);
}

int main(int argc, char *argv[])
{
	char	*format;
	char	*p;
	int	i,j;
	long long L;
	long l;
	char *cp;
	double d;
	float f;
	long double D;

	if(argc < 2)
		return(1);

	format=argv[1];
	format=xp_asprintf_start(format);
	for(j=2; j<argc; j++) {
		switch(argv[j][0]) {
			case 'f':
				f=atof(argv[j]+1);
				p=xp_asprintf_next(format,XP_PRINTF_TYPE_FLOAT,f);
				break;
			case 'd':
				d=atof(argv[j]+1);
				p=xp_asprintf_next(format,XP_PRINTF_TYPE_DOUBLE,d);
				break;
			case 'D':
				/* Don't know of a thing that converts a string to a long double */
				D=atof(argv[j]+1);
				p=xp_asprintf_next(format,XP_PRINTF_TYPE_LONGDOUBLE,D);
				break;
			case 'i':
				i=atoi(argv[j]+1);
				p=xp_asprintf_next(format,XP_PRINTF_TYPE_INT,i);
				break;
			case 'l':
				l=atol(argv[j]+1);
				p=xp_asprintf_next(format,XP_PRINTF_TYPE_LONG,l);
				break;
			case 'L':
				L=strtoll(argv[j]+1, NULL, 10);
				p=xp_asprintf_next(format,XP_PRINTF_TYPE_LONGLONG,L);
				break;
			case 's':
				cp=argv[j]+1;
				p=xp_asprintf_next(format,XP_PRINTF_TYPE_CHARP,cp);
				break;
		}
		if(p==NULL) {
			printf("Failed converting on item after %s\n",format);
			return(1);
		}
		format=p;
	}
	p=xp_asprintf_end(format);
	printf("At end, value is: '%s'\n",p);
}
