#line 1 "INITDATA.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/**************************************************************************/
/* This file contains the function initdata() and its exclusive functions */
/**************************************************************************/

#include "sbbs.h"

/****************************************************************/
/* Prototypes of functions that are called only by initdata()	*/
/****************************************************************/
void md(char *path);
char *readtext(long *line, FILE *stream);

/****************************************************************************/
/* Initializes system and node configuration information and data variables */
/****************************************************************************/
void initdata()
{
	char	str[256],str2[LEN_ALIAS+1],fname[13];
	int 	file;
	uint 	i,j;
	long	l,line=0L;
	FILE 	*instream;
	read_cfg_text_t txt;

txt.openerr="\7\r\nError opening %s for read.\r\n";
txt.reading="\r\nReading %s...";
txt.readit="\rRead %s       ";
txt.allocerr="\7\r\nError allocating %u bytes of memory\r\n";
txt.error="\7\r\nERROR: Offset %lu in %s\r\n\r\n";

read_node_cfg(txt);
read_main_cfg(txt);
read_msgs_cfg(txt);
read_file_cfg(txt);
read_xtrn_cfg(txt);
read_chat_cfg(txt);
read_attr_cfg(txt);

strcpy(fname,"TEXT.DAT");
sprintf(str,"%s%s",ctrl_dir,fname);
if((instream=fnopen(&file,str,O_RDONLY))==NULL) {
	lprintf(txt.openerr,str);
	bail(1); }
lprintf(txt.reading,fname);
for(i=0;i<TOTAL_TEXT && !feof(instream) && !ferror(instream);i++)
	if((text[i]=text_sav[i]=readtext(&line,instream))==NULL)
		i--;
if(i<TOTAL_TEXT) {
	lprintf(txt.error,line,fname);
	lprintf("Less than TOTAL_TEXT (%u) strings defined in %s\r\n"
		,TOTAL_TEXT,fname);
	bail(1); }
/****************************/
/* Read in static text data */
/****************************/
fclose(instream);
lprintf(txt.readit,fname);

strcpy(str,temp_dir);
if(strcmp(str+1,":\\"))     /* not root directory */
    str[strlen(str)-1]=0;   /* chop off '\' */
md(str);

for(i=0;i<=sys_nodes;i++) {
	sprintf(str,"%sDSTS.DAB",i ? node_path[i-1] : ctrl_dir);
	if(flength(str)<DSTSDABLEN) {
		if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
			lprintf("\7\r\nError creating %s\r\n",str);
			bail(1); }
		while(filelength(file)<DSTSDABLEN)
			if(write(file,"\0",1)!=1)
				break;				/* Create NULL system dsts.dab */
		close(file); } }

j=0;
sprintf(str,"%sUSER\\USER.DAT",data_dir);
if((l=flength(str))>0L) {
	if(l%U_LEN) {
		lprintf("\7\r\n%s is not evenly divisable by U_LEN (%d)\r\n"
			,str,U_LEN);
		bail(1); }
	j=lastuser(); }
if(node_valuser>j)	/* j still equals last user */
	node_valuser=1;
sys_status|=SS_INITIAL;
#ifdef __MSDOS__
freedosmem=farcoreleft();
#endif
}

/****************************************************************************/
/* If the directory 'path' doesn't exist, create it.                      	*/
/****************************************************************************/
void md(char *path)
{
	struct ffblk ff;

if(findfirst(path,&ff,FA_DIREC)) {
	lprintf("\r\nCreating Directory %s... ",path);
	if(mkdir(path)) {
		lprintf("\7failed!\r\nFix configuration or make directory by "
			"hand.\r\n");
		bail(1); }
	lputs(crlf); }
}

/****************************************************************************/
/* Reads special TEXT.DAT printf style text lines, splicing multiple lines, */
/* replacing escaped characters, and allocating the memory					*/
/****************************************************************************/
char *readtext(long *line,FILE *stream)
{
	char buf[2048],str[2048],*p,*p2;
	int i,j,k;

if(!fgets(buf,256,stream))
	return(NULL);
if(line)
	(*line)++;
if(buf[0]=='#')
	return(NULL);
p=strrchr(buf,'"');
if(!p) {
	if(line) {
		lprintf("\7\r\nNo quotation marks in line %d of TEXT.DAT\r\n",*line);
		bail(1); }
	return(NULL); }
if(*(p+1)=='\\')	/* merge multiple lines */
	while(strlen(buf)<2000) {
		if(!fgets(str,255,stream))
			return(NULL);
		if(line)
			(*line)++;
		p2=strchr(str,'"');
		if(!p2)
			continue;
		strcpy(p,p2+1);
		p=strrchr(p,'"');
		if(p && *(p+1)=='\\')
			continue;
		break; }
*(p)=0;
k=strlen(buf);
for(i=1,j=0;i<k;j++) {
	if(buf[i]=='\\')	{ /* escape */
		i++;
		if(isdigit(buf[i])) {
			str[j]=atoi(buf+i); 	/* decimal, NOT octal */
			if(isdigit(buf[++i]))	/* skip up to 3 digits */
				if(isdigit(buf[++i]))
					i++;
			continue; }
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
                    tmp[2]=0; }
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
				str[j]=7;	/* BEL */
				break;
			case 'f':
				str[j]=FF;
				break;
			case 'v':
				str[j]=11;	/* VT */
				break;
			default:
				str[j]=buf[i];
				break; }
		continue; }
	str[j]=buf[i++]; }
str[j]=0;
if((p=(char *)MALLOC(j+1))==NULL) {
	lprintf("\7\r\nError allocating %u bytes of memory from TEXT.DAT\r\n",j);
	bail(1); }
strcpy(p,str);
return(p);
}

/****************************************************************************/
/* Reads in ATTR.CFG and initializes the associated variables               */
/****************************************************************************/
void read_attr_cfg(read_cfg_text_t txt)
{
    char    str[256],fname[13];
    int     file,i;
	long	offset=0;
    FILE    *instream;

strcpy(fname,"ATTR.CFG");
sprintf(str,"%s%s",ctrl_dir,fname);
if((instream=fnopen(&file,str,O_RDONLY))==NULL) {
    lprintf(txt.openerr,str);
    bail(1); }
lprintf(txt.reading,fname);
for(i=0;i<TOTAL_COLORS && !feof(instream) && !ferror(instream);i++) {
	readline(&offset,str,4,instream);
    color[i]=attrstr(str); }
if(i<TOTAL_COLORS) {
	lprintf(txt.error,offset,fname);
    lprintf("Less than TOTAL_COLORS (%u) defined in %s\r\n"
        ,TOTAL_COLORS,fname);
    bail(1); }
fclose(instream);
lprintf(txt.readit,fname);
}

