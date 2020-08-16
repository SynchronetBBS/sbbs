/* Synchronet string utility routines */

/* $Id: str_util.c,v 1.67 2020/01/03 20:34:56 rswindell Exp $ */

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

#include "sbbs.h"
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
		else if((uchar)str[i]>=' ')
			dest[j++]=str[i];
	}
	dest[j]=0;
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

char* strip_space(const char *str, char* dest)
{
	int	i,j;

	if(dest==NULL && (dest=strdup(str))==NULL)
		return NULL;
	for(i=j=0;str[i];i++)
		if(!isspace((unsigned char)str[i]))
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

char* prep_file_desc(const char *str, char* dest)
{
	int	i,j;

	if(dest==NULL && (dest=strdup(str))==NULL)
		return NULL;
	for(i=j=0;str[i];i++)
		if(str[i]==CTRL_A && str[i+1]!=0) {
			i++;
			if(str[i]==0 || str[i]=='Z')	/* EOF */
				break;
			/* convert non-destructive backspace to a destructive backspace */
			if(str[i]=='<' && j)	
				j--;
		}
		else if(j && str[i]<=' ' && dest[j-1]==' ')
			continue;
		else if(i && !isalnum((unsigned char)str[i]) && str[i]==str[i-1])
			continue;
		else if((uchar)str[i]>=' ')
			dest[j++]=str[i];
		else if(str[i]==TAB || (str[i]==CR && str[i+1]==LF))
			dest[j++]=' ';
	dest[j]=0;
	return dest;
}

/****************************************************************************/
/* Pattern matching string search of 'insearchof' in 'string'.				*/
/****************************************************************************/
BOOL findstr_in_string(const char* insearchof, char* string)
{
	char*	p;
	char	str[256];
	char	search[81];
	int		c;
	int		i;
	BOOL	found=FALSE;

	if(string==NULL || insearchof==NULL)
		return(FALSE);

	SAFECOPY(search,insearchof);
	strupr(search);
	SAFECOPY(str,string);

	p=str;	
//	SKIP_WHITESPACE(p);

	if(*p==';')		/* comment */
		return(FALSE);

	if(*p=='!')	{	/* !match */
		found=TRUE;
		p++;
	}

	truncsp(p);
	c=strlen(p);
	if(c) {
		c--;
		strupr(p);
		if(p[c]=='~') {
			p[c]=0;
			if(strstr(search,p))
				found=!found; 
		}

		else if(p[c]=='^' || p[c]=='*') {
			p[c]=0;
			if(!strncmp(p,search,c))
				found=!found; 
		}

		else if(p[0]=='*') {
			i=strlen(search);
			if(i<c)
				return(found);
			if(!strncmp(p+1,search+(i-c),c))
				found=!found; 
		}

		else if(!strcmp(p,search))
			found=!found; 
	} 
	return(found);
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
	strcpy(tmp,str);
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
char* utf8_to_cp437_str(char* str)
{
	utf8_normalize_str(str);
	return utf8_replace_chars(str, unicode_to_cp437
		,/* unsupported char: */CP437_INVERTED_QUESTION_MARK
		,/* unsupported zero-width ch: */0
		,/* decode error char: */CP437_INVERTED_EXCLAMATION_MARK);
}

char* subnewsgroupname(scfg_t* cfg, sub_t* sub, char* str, size_t size)
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

char* get_ctrl_dir(void)
{
	char* p = getenv("SBBSCTRL");
	if(p == NULL || *p == '\0') {
		fprintf(stderr, "!SBBSCTRL environment variable not set, using default value: " SBBSCTRL_DEFAULT "\n\n");
		p = SBBSCTRL_DEFAULT;
	}
	return p;
}

