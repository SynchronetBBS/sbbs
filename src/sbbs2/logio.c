#line 1 "LOGIO.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/********************************/
/* Log file reading and writing */
/********************************/

#include "sbbs.h"

void logentry(char *code, char *entry)
{
	char str[512];

now=time(NULL);
sprintf(str,"Node %2d  %s\r\n   %s",node_num,timestr(&now),entry);
logline(code,str);
}

/****************************************************************************/
/* Writes 'str' verbatim into node.log 										*/
/****************************************************************************/
void log(char *str)
{
if(!(sys_status&SS_LOGOPEN) || online==ON_LOCAL) return;
if(logcol>=78 || (78-logcol)<strlen(str)) {
	write(logfile,crlf,2);
	logcol=1; }
if(logcol==1) {
	write(logfile,"   ",3);
	logcol=4; }
write(logfile,str,strlen(str));
if(str[strlen(str)-1]==LF)
	logcol=1;
else
	logcol+=strlen(str);
}

/****************************************************************************/
/* Writes 'str' on it's own line in node.log								*/
/****************************************************************************/
void logline(char *code, char *str)
{
	char line[1024];

if(!(sys_status&SS_LOGOPEN) || (online==ON_LOCAL && strcmp(code,"!!"))) return;
if(logcol!=1)
	write(logfile,crlf,2);
sprintf(line,"%-2.2s %s\r\n",code,str);
write(logfile,line,strlen(line));
logcol=1;
}

/****************************************************************************/
/* Writes a comma then 'ch' to log, tracking column.						*/
/****************************************************************************/
void logch(char ch, char comma)
{
	char slash='/';

if(!(sys_status&SS_LOGOPEN) || (online==ON_LOCAL)) return;
if((uchar)ch<SP)	/* Don't log control chars */
	return;
if(logcol==1) {
	logcol=4;
	write(logfile,"   ",3); }
else if(logcol>=78) {
	logcol=4;
	write(logfile,"\r\n   ",5); }
if(comma && logcol!=4) {
	write(logfile,",",1);
	logcol++; }
if(ch&0x80) {
	ch&=0x7f;
	if(ch<SP)
		return;
	write(logfile,&slash,1); }
write(logfile,&ch,1);
logcol++;
}

