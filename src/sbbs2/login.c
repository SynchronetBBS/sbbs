#line 1 "LOGIN.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "cmdshell.h"

extern char qwklogon;

int login(char *str, char *pw)
{
	long useron_misc=useron.misc;

useron.number=0;
if(node_dollars_per_call && noyes(text[AreYouSureQ]))
	return(LOGIC_FALSE);

if(str[0]=='*') {
	memmove(str,str+1,strlen(str));
	qwklogon=1; }
else
	qwklogon=0;

if(!(node_misc&NM_NO_NUM) && isdigit(str[0])) {
	useron.number=atoi(str);
	getuserdat(&useron);
	if(useron.number && useron.misc&(DELETED|INACTIVE))
		useron.number=0; }

if(!useron.number) {
	useron.number=matchuser(str);
	if(!useron.number && (uchar)str[0]<0x7f && str[1]
		&& isalpha(str[0]) && strchr(str,SP) && node_misc&NM_LOGON_R)
		useron.number=userdatdupe(0,U_NAME,LEN_NAME,str,0);
	if(useron.number) {
		getuserdat(&useron);
		if(useron.number && useron.misc&(DELETED|INACTIVE))
			useron.number=0; } }

if(!useron.number) {
	if(node_misc&NM_LOGON_P) {
        strcpy(useron.alias,str);
		bputs(pw);
		console|=CON_R_ECHOX;
		if(!(sys_misc&SM_ECHO_PW))
			console|=CON_L_ECHOX;
		getstr(str,LEN_PASS,K_UPPER|K_LOWPRIO);
		console&=~(CON_R_ECHOX|CON_L_ECHOX);
		bputs(text[InvalidLogon]);
		sprintf(tmp,"(%04u)  %-25s  Password: '%s'"
			,0,useron.alias,str);
		logline("+!",tmp); }
	else {
		bputs(text[UnknownUser]);
		sprintf(tmp,"Unknown User '%s'",str);
		logline("+!",tmp); }
	useron.misc=useron_misc;
	return(LOGIC_FALSE); }

if(!online) {
	useron.number=0;
	return(LOGIC_FALSE); }
statline=0;
statusline();
if((online==ON_REMOTE || sys_misc&SM_REQ_PW || node_misc&NM_SYSPW)
	&& (useron.pass[0] || REALSYSOP)
	&& (!node_dollars_per_call || sys_misc&SM_REQ_PW)) {
	bputs(pw);
	console|=CON_R_ECHOX;
	if(!(sys_misc&SM_ECHO_PW))
		console|=CON_L_ECHOX;
	getstr(str,LEN_PASS,K_UPPER|K_LOWPRIO);
	console&=~(CON_R_ECHOX|CON_L_ECHOX);
	if(!online) {
		useron.number=0;
		return(LOGIC_FALSE); }
	if(stricmp(useron.pass,str)) {
		bputs(text[InvalidLogon]);
		sprintf(tmp,"(%04u)  %-25s  Password: '%s' Attempt: '%s'"
			,useron.number,useron.alias,useron.pass,str);
		logline("+!",tmp);
		useron.number=0;
		useron.misc=useron_misc;
		return(LOGIC_FALSE); }
	if(REALSYSOP && !chksyspass(0)) {
		bputs(text[InvalidLogon]);
		useron.number=0;
		useron.misc=useron_misc;
		return(LOGIC_FALSE); } }

return(LOGIC_TRUE);
}
