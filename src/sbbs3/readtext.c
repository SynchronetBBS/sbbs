#include "sbbs.h"
#include "text_defaults.h"	/* text_defaults */

/****************************************************************************/
/* Reads special TEXT.DAT printf style text lines, splicing multiple lines, */
/* replacing escaped characters, and allocating the memory					*/
/****************************************************************************/
char *readtext(long *line,FILE *stream,long dflt)
{
	char buf[MAX_TEXTDAT_ITEM_LEN+256],str[MAX_TEXTDAT_ITEM_LEN+1],tmp[256],*p,*p2;
	int i,j,k;

	if(!fgets(buf,256,stream)) {
		/* Hide the EOF */
		if(feof(stream))
			clearerr(stream);
		goto use_default;
	}
	if(line)
		(*line)++;
	if(buf[0]=='#')
		goto use_default;
	p=strrchr(buf,'"');
	if(!p) {
		if(line)
			lprintf(LOG_WARNING,"No quotation marks in line %ld of text.dat",*line);
		goto use_default;
	}
	if(*(p+1)=='\\')	/* merge multiple lines */
		while(strlen(buf)<MAX_TEXTDAT_ITEM_LEN) {
			if(!fgets(str,255,stream)) {
				/* Hide the EOF */
				if(feof(stream))
					clearerr(stream);
				goto use_default;
			}
			if(line)
				(*line)++;
			p2=strchr(str,'"');
			if(!p2)
				continue;
			strcpy(p,p2+1);
			p=strrchr(p,'"');
			if(p && *(p+1)=='\\')
				continue;
			break; 
		}
	*(p)=0;
	k=strlen(buf);
	for(i=1,j=0;i<k && j<sizeof(str)-1;j++) {
		if(buf[i]=='\\')	{ /* escape */
			i++;
			if(isdigit(buf[i])) {
				str[j]=atoi(buf+i); 	/* decimal, NOT octal */
				if(isdigit(buf[++i]))	/* skip up to 3 digits */
					if(isdigit(buf[++i]))
						i++;
				continue; 
			}
			switch(buf[i++]) {
				case '\\':
					str[j]='\\';
					break;
				case '?':
					str[j]='?';
					break;
				case 'x':
					tmp[0]=buf[i++];        /* skip next character */
					tmp[1]=0;
					if(isxdigit(buf[i])) {  /* if another hex digit, skip too */
						tmp[1]=buf[i++];
						tmp[2]=0; 
					}
					str[j]=(char)ahtoul(tmp);
					break;
				case '\'':
					str[j]='\'';
					break;
				case '"':
					str[j]='"';
					break;
				case 'r':
					str[j]=CR;
					break;
				case 'n':
					str[j]=LF;
					break;
				case 't':
					str[j]=TAB;
					break;
				case 'b':
					str[j]=BS;
					break;
				case 'a':
					str[j]=BEL;
					break;
				case 'f':
					str[j]=FF;
					break;
				case 'v':
					str[j]=11;	/* VT */
					break;
				default:
					str[j]=buf[i];
					break; 
			}
			continue; 
		}
		str[j]=buf[i++]; 
	}
	str[j]=0;
	if((p=(char *)calloc(1,j+2))==NULL) { /* +1 for terminator, +1 for YNQX line */
		lprintf(LOG_CRIT,"Error allocating %u bytes of memory from text.dat",j);
		goto use_default;
	}
	strcpy(p,str);
	return(p);
use_default:
	p=strdup(text_defaults[dflt]);
	if(p==NULL)
		lprintf(LOG_CRIT,"Error duplicating %s text defaults",text_defaults[dflt]);
	return(p);
}

