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
	int	i,j;

	if(dest==NULL && (dest=strdup(str))==NULL)
		return NULL;
	for(i=j=0;str[i];i++) {
		if(str[i]==CTRL_A) {
			i++;
			if(str[i]==0 || str[i]=='Z')	/* EOF */
				break;
			/* convert non-destructive backspace to a destructive backspace */
			if(str[i]=='<' && j)	
				j--;
		}
		else dest[j++]=str[i]; 
	}
	dest[j]=0;
	return dest;
}

char* strip_ctrl(const char *str, char* dest)
{
	int	i,j;

	if(dest==NULL && (dest=strdup(str))==NULL)
		return NULL;
	for(i=j=0;str[i];i++) {
		if(str[i]==CTRL_A) {
			i++;
			if(str[i]==0 || str[i]=='Z')	/* EOF */
				break;
			/* convert non-destructive backspace to a destructive backspace */
			if(str[i]=='<' && j)	
				j--;
		}
		else if((uchar)str[i]>=' ' && str[i] != DEL)
			dest[j++]=str[i];
	}
	dest[j]=0;
	return dest;
}

char* strip_ansi(char* str)
{
	char* s = str;
	char* d = str;
	while(*s != '\0') {
		if(*s == ESC && *(s + 1) == '[') {
			s += 2;
			while(*s != '\0' && (*s < '@' || *s > '~'))
				s++;
			if(*s != '\0') // Skip "final byte""
				s++;
		} else {
			*(d++) = *(s++);
		}
	}
	*d = '\0';
	return str;
}

// Sort of a stripped down version of ANS2ASC
char* convert_ansi(const char* src, char* dest, size_t len, int width, BOOL ice_color)
{
	const char* s = src;
	char* d = dest;
	char* p;
	ulong n[10];
	size_t nc;
	int column = 0;
	while(*s != '\0' && d < dest + len) {
		if(*s == ESC && *(s + 1) == '[') {
			s += 2;
			nc = 0;
			do {
				n[nc] = strtoul(s, &p, 10);
				if(p == s || p == NULL)
					break;
				nc++;
				s = p;
				if(*s != ';')
					break;
				s++;
			} while(nc < sizeof(n) / sizeof(n[0]));
			while(*s != '\0' && (*s < '@' || *s > '~'))
				s++;
			if(*s == 'C') {	// Cursor right
				if(n[0] < 1)
					n[0] = 1;
				while(n[0] >= 1 && d < dest + len) {
					*(d++) = ' ';
					n[0]--;
					column++;
				}
			} else if(*s == 'm') { // Color / Attributes
				for(size_t i = 0; i < nc && d < dest + len; i++) {
					*(d++) = CTRL_A;
					switch(n[i]) {
						case 0:
						case 2:
							*(d++) = 'N';
							break;
						case 1:
							*(d++) = 'H';
							break;
						case 3:
						case 4:
						case 5: 				/* blink */
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
			if(*s != '\0') // Skip "final byte"
				s++;
		} else {
			if(*s == '\r' || *s == '\n') {
				*(d++) = *(s++);
				column = 0;
			} else {
				if(width && column >= width) {
					d += sprintf(d, "\1+\1N\1/\1-");	// Save, normal, cond-newline, restore
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
	int	i,j;

	if(dest==NULL && (dest=strdup(str))==NULL)
		return NULL;
	for(i=j=0;str[i];i++)
		if(!(str[i]&0x80))
			dest[j++]=str[i];
	dest[j]=0;
	return dest;
}

char* strip_cp437_graphics(const char *str, char* dest)
{
	int	i,j;

	if(dest==NULL && (dest=strdup(str))==NULL)
		return NULL;
	for(i=j=0;str[i];i++)
		if((uchar)str[i] <= (uchar)CP437_INVERTED_EXCLAMATION_MARK
			|| (uchar)str[i] >= (uchar)CP437_GREEK_SMALL_LETTER_ALPHA)
			dest[j++]=str[i];
	dest[j]=0;
	return dest;
}

char* strip_space(const char *str, char* dest)
{
	int	i,j;

	if(dest==NULL && (dest=strdup(str))==NULL)
		return NULL;
	for(i=j=0;str[i];i++)
		if(!IS_WHITESPACE(str[i]))
			dest[j++]=str[i];
	dest[j]=0;
	return dest;
}

char* strip_char(const char* str, char* dest, char ch)
{
	const char* src;

	if(dest == NULL && (dest = strdup(str)) == NULL)
		return NULL;
	char* retval = dest;
	for(src = str; *src != '\0'; src++) {
		if(*src != ch)
			*(dest++) = *src;
	}
	*dest = '\0';
	return retval;
}

/****************************************************************************/
/* Pattern matching string search of 'insearchof' in 'pattern'.				*/
/* pattern matching is case-insensitive										*/
/* patterns beginning with ';' are comments (never match)					*/
/* patterns beginning with '!' are reverse-matched (returns FALSE if match)	*/
/* patterns ending in '~' will match string anywhere (sub-string search)	*/
/* patterns ending in '^' will match left string fragment only				*/
/* patterns including '*' must match both left and right string fragments	*/
/* all other patterns are exact-match checking								*/
/****************************************************************************/
BOOL findstr_in_string(const char* search, const char* pattern)
{
	char	buf[256];
	char*	p;
	char*	last;
	const char*	splat;
	size_t	len;
	BOOL	found = FALSE;

	if(pattern == NULL || search == NULL)
		return FALSE;

	SAFECOPY(buf, pattern);
	p = buf;

	if(*p == ';')		/* comment */
		return FALSE;

	if(*p == '!')	{	/* reverse-match */
		found = TRUE;
		p++;
	}

	truncsp(p);
	len = strlen(p);
	if(len > 0) {
		last = p + len - 1;
		if(*last == '~') {
			*last = '\0';
			if(strcasestr(search, p) != NULL)
				found = !found; 
		}

		else if(*last == '^') {
			if(strnicmp(p, search, len - 1) == 0)
				found = !found; 
		}

		else if((splat = strchr(p, '*')) != NULL) {
			int left = splat - p;
			int right = len - (left + 1);
			int slen = strlen(search);
			if(slen < left + right)
				return found;
			if(strnicmp(search, p, left) == 0
				&& strnicmp(p + left + 1, search + (slen - right), right) == 0)
				found = !found;
		}

		else if(stricmp(p, search) == 0)
			found = !found; 
	} 
	return found;
}

static uint32_t encode_ipv4_address(unsigned int byte[])
{
	if(byte[0] > 0xff || byte[1] > 0xff || byte[2] > 0xff || byte[3] > 0xff)
		return 0;
	return (byte[0]<<24) | (byte[1]<<16) | (byte[2]<<8) | byte[3];
}

static uint32_t parse_ipv4_address(const char* str)
{
	unsigned int byte[4];

	if(sscanf(str, "%u.%u.%u.%u", &byte[0], &byte[1], &byte[2], &byte[3]) != 4)
		return 0;
	return encode_ipv4_address(byte);
}

static uint32_t parse_cidr(const char* p, unsigned* subnet)
{
	unsigned int byte[4];

	if(*p == '!')
		p++;

	*subnet = 0;
	if(sscanf(p, "%u.%u.%u.%u/%u", &byte[0], &byte[1], &byte[2], &byte[3], subnet) != 5 || *subnet > 32)
		return 0;
	return encode_ipv4_address(byte);
}

static BOOL is_cidr_match(const char *p, uint32_t ip_addr, uint32_t cidr, unsigned subnet)
{
	BOOL	match = FALSE;

	if(*p == '!')
		match = TRUE;

	if(((ip_addr ^ cidr) >> (32-subnet)) == 0)
		match = !match;

	return match;
}

/****************************************************************************/
/* Pattern matching string search of 'insearchof' in 'list'.				*/
/****************************************************************************/
BOOL findstr_in_list(const char* insearchof, str_list_t list)
{
	size_t	index;
	BOOL	found=FALSE;
	char*	p;
	uint32_t ip_addr, cidr;
	unsigned subnet;

	if(list==NULL || insearchof==NULL)
		return FALSE;
	ip_addr = parse_ipv4_address(insearchof);
	for(index=0; list[index]!=NULL; index++) {
		p=list[index];
//		SKIP_WHITESPACE(p);
		if(ip_addr != 0 && (cidr = parse_cidr(p, &subnet)) != 0)
			found = is_cidr_match(p, ip_addr, cidr, subnet);
		else
			found = findstr_in_string(insearchof,p);
		if(found != (*p=='!'))
			break;
	}
	return found;
}

/****************************************************************************/
/* Pattern matching string search of 'insearchof' in 'fname'.				*/
/****************************************************************************/
BOOL findstr(const char* insearchof, const char* fname)
{
	char		str[256];
	BOOL		found=FALSE;
	FILE*		fp;
	uint32_t	ip_addr, cidr;
	unsigned	subnet;

	if(insearchof==NULL || fname==NULL)
		return FALSE;

	if((fp=fopen(fname,"r"))==NULL)
		return FALSE; 

	ip_addr = parse_ipv4_address(insearchof);
	while(!feof(fp) && !ferror(fp) && !found) {
		if(!fgets(str,sizeof(str),fp))
			break;
		char* p = str;
		SKIP_WHITESPACE(p);
		c_unescape_str(p);
		if(ip_addr !=0 && (cidr = parse_cidr(p, &subnet)) != 0)
			found = is_cidr_match(p, ip_addr, cidr, subnet);
		else
			found = findstr_in_string(insearchof, p);
	}

	fclose(fp);
	return found;
}

/****************************************************************************/
/* Searches the file <name>.can in the TEXT directory for matches			*/
/* Returns TRUE if found in list, FALSE if not.								*/
/****************************************************************************/
BOOL trashcan(scfg_t* cfg, const char* insearchof, const char* name)
{
	char fname[MAX_PATH+1];

	return(findstr(insearchof,trashcan_fname(cfg,name,fname,sizeof(fname))));
}

/****************************************************************************/
char* trashcan_fname(scfg_t* cfg, const char* name, char* fname, size_t maxlen)
{
	safe_snprintf(fname,maxlen,"%s%s.can",cfg->text_dir,name);
	return fname;
}

static char* process_findstr_item(size_t index, char *str, void* cbdata)
{
	SKIP_WHITESPACE(str);
	return c_unescape_str(str);
}

/****************************************************************************/
str_list_t findstr_list(const char* fname)
{
	FILE*	fp;
	str_list_t	list;

	if((fp=fopen(fname,"r"))==NULL)
		return NULL;

	list=strListReadFile(fp, NULL, /* Max line length: */255);
	strListModifyEach(list, process_findstr_item, /* cbdata: */NULL);

	fclose(fp);

	return list;
}

/****************************************************************************/
str_list_t trashcan_list(scfg_t* cfg, const char* name)
{
	char	fname[MAX_PATH+1];

	return findstr_list(trashcan_fname(cfg, name, fname, sizeof(fname)));
}

/****************************************************************************/
/* Returns in 'string' a character representation of the number in l with   */
/* commas.																	*/
/****************************************************************************/
char* ultoac(ulong l, char *string)
{
	char str[256];
	int i,j,k;

	ultoa(l,str,10);
	i=strlen(str)-1;
	j=i/3+1+i;
	string[j--]=0;
	for(k=1;i>-1;k++) {
		string[j--]=str[i--];
		if(j>0 && !(k%3))
			string[j--]=','; 
	}
	return(string);
}

/****************************************************************************/
/* Truncate string at first occurrence of char in specified character set	*/
/* Returns a pointer to the terminating NUL if the string was truncated,	*/
/* NULL otherwise.															*/
/****************************************************************************/
char* truncstr(char* str, const char* set)
{
	char* p;

	p=strpbrk(str,set);
	if(p!=NULL && *p!=0)
		*p=0;

	return(p);
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
/* rot13 encoder/decoder - courtesy of Mike Acar							*/
/****************************************************************************/
char* rot13(char* str)
{
	char ch, cap;
	char* p;
  
	p=str;
	while((ch=*p)!=0) {
		cap = ch & 32;
		ch &= ~cap;
		ch = ((ch >= 'A') && (ch <= 'Z') ? ((ch - 'A' + 13) % 26 + 'A') : ch) | cap;
		*(p++)=ch;
    }

	return(str);
}

/****************************************************************************/
/* Puts a backslash on path strings if not just a drive letter and colon	*/
/****************************************************************************/
char* backslashcolon(char *str)
{
    int i;

	i=strlen(str);
	if(i && !IS_PATH_DELIM(str[i-1]) && str[i-1]!=':') {
		str[i]=PATH_DELIM; 
		str[i+1]=0; 
	}

	return str;
}

/****************************************************************************/
/* Compares pointers to pointers to char. Used in conjunction with qsort()  */
/****************************************************************************/
int pstrcmp(const char **str1, const char **str2)
{
	return(strcmp(*str1,*str2));
}

/****************************************************************************/
/* Returns the number of characters that are the same between str1 and str2 */
/****************************************************************************/
int strsame(const char *str1, const char *str2)
{
	int i,j=0;

	for(i=0;str1[i];i++)
		if(str1[i]==str2[i]) j++;
	return(j);
}


/****************************************************************************/
/* Returns string for 2 digit hex+ numbers up to 575						*/
/****************************************************************************/
char *hexplus(uint num, char *str)
{
	sprintf(str,"%03x",num);
	str[0]=num/0x100 ? 'f'+(num/0x10)-0xf : str[1];
	str[1]=str[2];
	str[2]=0;
	return(str);
}

/****************************************************************************/
/* Converts an ASCII Hex string into an ulong                               */
/* by Steve Deppe (Ille Homine Albe)										*/
/****************************************************************************/
ulong ahtoul(const char *str)
{
    ulong l,val=0;

	while((l=(*str++)|0x20)!=0x20)
		val=(l&0xf)+(l>>6&1)*9+val*16;
	return(val);
}

/****************************************************************************/
/* Converts hex-plus string to integer										*/
/****************************************************************************/
uint hptoi(const char *str)
{
	char tmp[128];
	uint i;

	if(!str[1] || toupper(str[0])<='F')
		return(ahtoul(str));
	SAFECOPY(tmp,str);
	tmp[0]='F';
	i=ahtoul(tmp)+((toupper(str[0])-'F')*0x10);
	return(i);
}

/****************************************************************************/
/* Returns TRUE if a is a valid ctrl-a "attribute" code, FALSE if it isn't. */
/****************************************************************************/
BOOL valid_ctrl_a_attr(char a)
{
	switch(toupper(a)) {
		case '+':	/* push attr	*/
		case '-':   /* pop attr		*/
		case '_':   /* clear        */
		case 'B':   /* blue     fg  */
		case 'C':   /* cyan     fg  */
		case 'G':   /* green    fg  */
		case 'H':   /* high     fg  */
		case 'E':	/* high		bg	*/
		case 'I':   /* blink        */
		case 'K':   /* black    fg  */
		case 'M':   /* magenta  fg  */
		case 'N':   /* normal       */
		case 'R':   /* red      fg  */
		case 'W':   /* white    fg  */
		case 'Y':   /* yellow   fg  */
		case '0':   /* black    bg  */
		case '1':   /* red      bg  */
		case '2':   /* green    bg  */
		case '3':   /* brown    bg  */
		case '4':   /* blue     bg  */
		case '5':   /* magenta  bg  */
		case '6':   /* cyan     bg  */
		case '7':   /* white    bg  */
			return(TRUE); 
	}
	return(FALSE);
}

/****************************************************************************/
/* Returns TRUE if a is a valid QWKnet compatible Ctrl-A code, else FALSE	*/
/****************************************************************************/
BOOL valid_ctrl_a_code(char a)
{
	switch(toupper(a)) {
		case 'P':		/* Pause */
		case 'L':		/* CLS */
		case ',':		/* 100ms delay */
			return TRUE;
	}
	return valid_ctrl_a_attr(a);
}

/****************************************************************************/
/****************************************************************************/
char ctrl_a_to_ascii_char(char a)
{
	switch(toupper(a)) {
		case 'L':   /* cls          */
			return FF;
		case '<':	/* backspace	*/
			return '\b';
		case '[':	/* CR			*/
			return '\r';
		case ']':	/* LF			*/
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
    char*	dest;
    size_t	a,c,d;

	dest=str;
	for(a=c=d=0;str[c];c++) {
		if(str[c]==CTRL_A) {
			a++;
			if(str[c+1]==0)
				break;
			if(!valid_ctrl_a_attr(str[c+1])) {
				/* convert non-destructive backspace to a destructive backspace */
				if(str[c+1]=='<' && d)	
					d--;
				c++;
				continue; 
			}
		}
		dest[d++]=str[c]; 
	}
	dest[d]=0;
	return(a);
}

/****************************************************************************/
/****************************************************************************/
char exascii_to_ascii_char(uchar ch)
{
	/* Seven bit table for EXASCII to ASCII conversion */
	const char *sbtbl="CUeaaaaceeeiiiAAEaAooouuyOUcLYRfaiounNao?--24!<>"
			"###||||++||++++++--|-+||++--|-+----++++++++##[]#"
			"abrpEout*ono%0ENE+><rj%=o*.+n2* ";

	if(ch&0x80)
		return sbtbl[ch^0x80];
	return ch;
}

BOOL str_is_ascii(const char* str)
{
	for(const char* p = str; *p != 0; p++) {
		if(*p < 0)
			return FALSE;
	}
	return TRUE;
}

BOOL str_has_ctrl(const char* str)
{
	for(const char* p = str; *p != 0; p++) {
		if((uchar)*p < ' ')
			return TRUE;
	}
	return FALSE;
}

/****************************************************************************/
/* Convert string from IBM extended ASCII to just ASCII						*/
/****************************************************************************/
char* ascii_str(uchar* str)
{
	uchar*	p=str;

	while(*p) {
		if((*p)&0x80)
			*p=exascii_to_ascii_char(*p);	
		p++;
	}
	return((char*)str);
}

char* replace_named_values(const char* src	 
    ,char* buf	 
    ,size_t buflen       /* includes '\0' terminator */	 
    ,const char* escape_seq	 
    ,named_string_t* string_list	 
    ,named_int_t* int_list	 
    ,BOOL case_sensitive)	 
 {	 
     char    val[32];	 
     size_t  i;	 
     size_t  esc_len=0;	 
     size_t  name_len;	 
     size_t  value_len;	 
     char*   p = buf;	 
     int (*cmp)(const char*, const char*, size_t);	 
 
     if(case_sensitive)	 
         cmp=strncmp;	 
     else	 
         cmp=strnicmp;	 
 
     if(escape_seq!=NULL)	 
         esc_len = strlen(escape_seq);	 
 
     while(*src && (size_t)(p-buf) < buflen-1) {	 
         if(esc_len) {	 
             if(cmp(src, escape_seq, esc_len)!=0) {	 
                 *p++ = *src++;	 
                 continue;	 
             }	 
             src += esc_len; /* skip the escape seq */	 
         }	 
         if(string_list) {	 
             for(i=0; string_list[i].name!=NULL /* terminator */; i++) {	 
                 name_len = strlen(string_list[i].name);	 
                 if(cmp(src, string_list[i].name, name_len)==0) {	 
                     value_len = strlen(string_list[i].value);	 
                     if((p-buf)+value_len > buflen-1)        /* buffer overflow? */	 
                         value_len = (buflen-1)-(p-buf); /* truncate value */	 
                     memcpy(p, string_list[i].value, value_len);	 
                     p += value_len;	 
                     src += name_len;	 
                     break;	 
                 }	 
             }	 
             if(string_list[i].name!=NULL) /* variable match */	 
                 continue;	 
         }	 
         if(int_list) {	 
             for(i=0; int_list[i].name!=NULL /* terminator */; i++) {	 
                 name_len = strlen(int_list[i].name);	 
                 if(cmp(src, int_list[i].name, name_len)==0) {	 
                     SAFEPRINTF(val,"%d",int_list[i].value);	 
                     value_len = strlen(val);	 
                     if((p-buf)+value_len > buflen-1)        /* buffer overflow? */	 
                         value_len = (buflen-1)-(p-buf); /* truncate value */	 
                     memcpy(p, val, value_len);	 
                     p += value_len;	 
                     src += name_len;	 
                     break;	 
                 }	 
             }	 
             if(int_list[i].name!=NULL) /* variable match */	 
                 continue;	 
         }	 

         *p++ = *src++;	 
     }	 
     *p=0;   /* terminate string in destination buffer */	 
 
     return(buf);	 
 }	 
 
/****************************************************************************/
/* Condense consecutive white-space chars in a string to single spaces		*/
/****************************************************************************/
char* condense_whitespace(char* str)
{
	char* s = str;
	char* d = str;
	while(*s != '\0') {
		if(IS_WHITESPACE(*s)) {
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
	int op=0;
	const char *s;
	char ctrl;

	for(s=str; *s; s++) {
		if(*s=='+')
			op=1;
		else if(*s=='-')
			op=2;
		else {
			if(!op) {
				val=0;
				op=1;
			}
			ctrl=toupper(*s);
			ctrl&=0x1f;			/* Ensure it fits */
			switch(op) {
				case 1:		/* Add to the set */
					val |= 1<<ctrl;
					break;
				case 2:		/* Remove from the set */
					val &= ~(1<<ctrl);
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
		,/* unsupported char: */CP437_INVERTED_QUESTION_MARK
		,/* unsupported zero-width ch: */0
		,/* decode error char: */CP437_INVERTED_EXCLAMATION_MARK);
}

char* sub_newsgroup_name(scfg_t* cfg, sub_t* sub, char* str, size_t size)
{
	memset(str, 0, size);
	if(sub->newsgroup[0])
		strncpy(str, sub->newsgroup, size - 1);
	else {
		snprintf(str, size - 1, "%s.%s", cfg->grp[sub->grp]->sname, sub->sname);
		/*
		 * From RFC5536:
		 * newsgroup-name  =  component *( "." component )
		 * component       =  1*component-char
		 * component-char  =  ALPHA / DIGIT / "+" / "-" / "_"
		 */
		if (str[0] == '.')
			str[0] = '_';
		size_t c;
		for(c = 0; str[c] != 0; c++) {
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
	}
	return str;
}

char* sub_area_tag(scfg_t* cfg, sub_t* sub, char* str, size_t size)
{
	char* p;

	memset(str, 0, size);
	if(sub->area_tag[0])
		strncpy(str, sub->area_tag, size - 1);
	else if(sub->newsgroup[0])
		strncpy(str, sub->newsgroup, size - 1);
	else {
		strncpy(str, sub->sname, size - 1);
		REPLACE_CHARS(str, ' ', '_', p);
	}
	strupr(str);
	return str;
}

char* dir_area_tag(scfg_t* cfg, dir_t* dir, char* str, size_t size)
{
	char* p;

	memset(str, 0, size);
	if(dir->area_tag[0])
		strncpy(str, dir->area_tag, size - 1);
	else {
		strncpy(str, dir->sname, size - 1);
		REPLACE_CHARS(str, ' ', '_', p);
	}
	strupr(str);
	return str;
}

char* get_ctrl_dir(BOOL warn)
{
	char* p = getenv("SBBSCTRL");
	if(p == NULL || *p == '\0') {
		if(warn)
			fprintf(stderr, "!SBBSCTRL environment variable not set, using default value: " SBBSCTRL_DEFAULT "\n\n");
		p = SBBSCTRL_DEFAULT;
	}
	return p;
}

