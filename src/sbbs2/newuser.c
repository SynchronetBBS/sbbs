#line 1 "NEWUSER.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "cmdshell.h"

extern char qwklogon;

/****************************************************************************/
/* This function is invoked when a user enters "NEW" at the NN: prompt		*/
/* Prompts user for personal information and then sends feedback to sysop.  */
/* Called from function waitforcall											*/
/****************************************************************************/
void newuser()
{
	char	c,str[512],usa;
	int 	file;
	uint	i,j;
	long	misc;
	FILE	*stream;

if(cur_rate<node_minbps) {
	bprintf(text[MinimumModemSpeed],node_minbps);
	sprintf(str,"%sTOOSLOW.MSG",text_dir);
	if(fexist(str))
		printfile(str,0);
	sprintf(str,"New user modem speed: %lu<%u"
		,cur_rate,node_minbps);
	logline("N!",str);
	hangup();
	return; }

getnodedat(node_num,&thisnode,0);
if(thisnode.misc&NODE_LOCK) {
	bputs(text[NodeLocked]);
	logline("N!","New user locked node logon attempt");
	hangup();
	return; }

if(sys_misc&SM_CLOSED) {
	bputs(text[NoNewUsers]);
	hangup();
	return; }
getnodedat(node_num,&thisnode,1);
thisnode.status=NODE_NEWUSER;
if(online==ON_LOCAL)
	thisnode.connection=0;
else
	thisnode.connection=cur_rate;
putnodedat(node_num,thisnode);
memset(&useron,0,sizeof(user_t));	  /* Initialize user info to null */
if(new_pass[0] && online==ON_REMOTE) {
	c=0;
	while(++c<4) {
		bputs(text[NewUserPasswordPrompt]);
		getstr(str,40,K_UPPER);
		if(!strcmp(str,new_pass))
			break;
		sprintf(tmp,"NUP Attempted: '%s'",str);
		logline("N!",tmp); }
	if(c==4) {
		sprintf(str,"%sNUPGUESS.MSG",text_dir);
		if(fexist(str))
			printfile(str,P_NOABORT);
		hangup();
		return; } }

if(autoterm || yesno(text[AutoTerminalQ])) {
	useron.misc|=AUTOTERM;
	useron.misc|=autoterm; }

if(!(useron.misc&AUTOTERM)) {
	if(yesno(text[AnsiTerminalQ]))
		useron.misc|=ANSI; }

if(useron.misc&ANSI) {
	useron.rows=0;	/* Auto-rows */
	if(useron.misc&(RIP|WIP) || yesno(text[ColorTerminalQ]))
		useron.misc|=COLOR; }
else
	useron.rows=24;
if(!yesno(text[ExAsciiTerminalQ]))
	useron.misc|=NO_EXASCII;

/* Sets defaults per sysop config */
useron.misc|=(new_misc&~(DELETED|INACTIVE|QUIET|NETMAIL));
useron.qwk=(QWK_FILES|QWK_ATTACH|QWK_EMAIL|QWK_DELMAIL);
strcpy(useron.modem,connection);
useron.firston=useron.laston=useron.pwmod=time(NULL);
if(new_expire) {
	now=time(NULL);
	useron.expire=now+((long)new_expire*24L*60L*60L); }
else
	useron.expire=0;
useron.sex=SP;
useron.prot=new_prot;
strcpy(useron.note,cid);		/* Caller ID if supported, NULL otherwise */
strcpy(useron.alias,"New");     /* just for status line */
strcpy(useron.modem,connection);
if(!lastuser()) {	/* Automatic sysop access for first user */
	useron.level=99;
	useron.exempt=useron.flags1=useron.flags2=0xffffffffUL;
	useron.flags3=useron.flags4=0xffffffffUL;
	useron.rest=0L; }
else {
	useron.level=new_level;
	useron.flags1=new_flags1;
	useron.flags2=new_flags2;
	useron.flags3=new_flags3;
	useron.flags4=new_flags4;
	useron.rest=new_rest;
	useron.exempt=new_exempt; }

useron.cdt=new_cdt;
useron.min=new_min;
useron.freecdt=level_freecdtperday[useron.level];

if(total_fcomps)
	strcpy(useron.tmpext,fcomp[0]->ext);
else
	strcpy(useron.tmpext,"ZIP");
for(i=0;i<total_xedits;i++)
	if(!stricmp(xedit[i]->code,new_xedit) && chk_ar(xedit[i]->ar,useron))
		break;
if(i<total_xedits)
	useron.xedit=i+1;


useron.shell=new_shell;

statline=sys_def_stat;
statusline();
useron.alias[0]=0;
while(online) {
	while(online) {
		if(uq&UQ_ALIASES)
			bputs(text[EnterYourAlias]);
		else
			bputs(text[EnterYourRealName]);
		getstr(useron.alias,LEN_ALIAS
			,K_UPRLWR|(uq&UQ_NOEXASC)|K_EDIT|K_AUTODEL);
		truncsp(useron.alias);
		if(useron.alias[0]<=SP || !isalpha(useron.alias[0])
			|| strchr(useron.alias,0xff)
			|| matchuser(useron.alias) || trashcan(useron.alias,"NAME")
			|| (!(uq&UQ_ALIASES) && !strchr(useron.alias,SP))) {
			bputs(text[YouCantUseThatName]);
			continue; }
		break; }
	statusline();
	if(!online) return;
	if(uq&UQ_ALIASES && uq&UQ_REALNAME) {
		while(online) {
			bputs(text[EnterYourRealName]);
			if(!getstr(useron.name,LEN_NAME
				,K_UPRLWR|(uq&UQ_NOEXASC)|K_EDIT|K_AUTODEL)
				|| trashcan(useron.name,"NAME")
				|| strchr(useron.name,0xff)
				|| !strchr(useron.name,SP)
				|| (uq&UQ_DUPREAL
					&& userdatdupe(useron.number,U_NAME,LEN_NAME
						,useron.name,0)))
				bputs(text[YouCantUseThatName]);
			else
				break; } }
	else if(uq&UQ_COMPANY) {
			bputs(text[EnterYourCompany]);
			getstr(useron.name,LEN_NAME,(uq&UQ_NOEXASC)|K_EDIT|K_AUTODEL); }
	if(!useron.name[0])
        strcpy(useron.name,useron.alias);
    if(!online) return;
	if(!useron.handle[0])
		sprintf(useron.handle,"%.*s",LEN_HANDLE,useron.alias);
	while(uq&UQ_HANDLE && online) {
		bputs(text[EnterYourHandle]);
		if(!getstr(useron.handle,LEN_HANDLE
			,K_LINE|K_EDIT|K_AUTODEL|(uq&UQ_NOEXASC))
			|| strchr(useron.handle,0xff)
			|| (uq&UQ_DUPHAND
				&& userdatdupe(0,U_HANDLE,LEN_HANDLE,useron.handle,0))
			|| trashcan(useron.handle,"NAME"))
			bputs(text[YouCantUseThatName]);
		else
			break; }
	if(!online) return;
	if(uq&UQ_ADDRESS)
		while(online) { 	   /* Get address and zip code */
			bputs(text[EnterYourAddress]);
			if(getstr(useron.address,LEN_ADDRESS
				,K_UPRLWR|(uq&UQ_NOEXASC)|K_EDIT|K_AUTODEL))
				break; }
	if(!online) return;
	while(uq&UQ_LOCATION && online) {
		bputs(text[EnterYourCityState]);
		if(getstr(useron.location,LEN_LOCATION
			,K_UPRLWR|(uq&UQ_NOEXASC)|K_EDIT|K_AUTODEL)
			&& (uq&UQ_NOCOMMAS || strchr(useron.location,',')))
			break;
		bputs("\r\nYou must include a comma between the city and state.\r\n");
		useron.location[0]=0; }
	if(uq&UQ_ADDRESS)
		while(online) {
			bputs(text[EnterYourZipCode]);
			if(getstr(useron.zipcode,LEN_ZIPCODE
				,K_UPPER|(uq&UQ_NOEXASC)|K_EDIT|K_AUTODEL))
				break; }
	if(!online) return;
	if(uq&UQ_PHONE) {
		usa=yesno(text[CallingFromNorthAmericaQ]);
		while(online) {
			bputs(text[EnterYourPhoneNumber]);
			if(!usa) {
				if(getstr(useron.phone,LEN_PHONE
					,K_UPPER|K_LINE|(uq&UQ_NOEXASC)|K_EDIT|K_AUTODEL)<5)
					continue; }
			else {
				if(gettmplt(useron.phone,sys_phonefmt
					,K_LINE|(uq&UQ_NOEXASC)|K_EDIT)<strlen(sys_phonefmt))
					continue; }
			if(!trashcan(useron.phone,"PHONE"))
				break; } }
	if(!online) return;
	if(uq&UQ_SEX) {
		bputs(text[EnterYourSex]);
		useron.sex=getkeys("MF",0); }
	while(uq&UQ_BIRTH && online) {
		bputs(text[EnterYourBirthday]);
		if(gettmplt(useron.birth,"nn/nn/nn",K_EDIT)==8 && getage(useron.birth))
			break; }
	if(yesno(text[UserInfoCorrectQ]))
		break; }
sprintf(str,"New user: %s",useron.alias);
logline("N",str);
if(!online) return;
if(uq&UQ_COMP)
	getcomputer(useron.comp);
CLS;
sprintf(str,"%sSBBS.MSG",text_dir);
printfile(str,P_NOABORT);
if(lncntr)
	pause();
CLS;
sprintf(str,"%sSYSTEM.MSG",text_dir);
printfile(str,P_NOABORT);
if(lncntr)
	pause();
CLS;
sprintf(str,"%sNEWUSER.MSG",text_dir);
printfile(str,P_NOABORT);
if(lncntr)
	pause();
CLS;
answertime=time(NULL);		/* could take 10 minutes to get this far */

if(total_xedits && uq&UQ_XEDIT && !noyes("Use an external editor")) {
	if(useron.xedit) useron.xedit--;
    for(i=0;i<total_xedits;i++)
		uselect(1,i,"External Editor",xedit[i]->name,xedit[i]->ar);
	if((int)(i=uselect(0,useron.xedit,0,0,0))>=0)
		useron.xedit=i+1; }

if(total_shells>1 && uq&UQ_CMDSHELL) {
    for(i=0;i<total_shells;i++)
		uselect(1,i,"Command Shell",shell[i]->name,shell[i]->ar);
	if((int)(i=uselect(0,useron.shell,0,0,0))>=0)
		useron.shell=i; }

c=0;
while(c<LEN_PASS) { 				/* Create random password */
	useron.pass[c]=random(43)+48;
	if(isalnum(useron.pass[c]))
		c++; }
useron.pass[c]=0;
bprintf(text[YourPasswordIs],useron.pass);

if(sys_misc&SM_PWEDIT && yesno(text[NewPasswordQ]))
	while(online) {
		bputs(text[NewPassword]);
		getstr(str,LEN_PASS,K_UPPER|K_LINE);
		truncsp(str);
		if(chkpass(str,useron)) {
			strcpy(useron.pass,str);
			CRLF;
			bprintf(text[YourPasswordIs],useron.pass);
			break; }
		CRLF; }

c=0;
while(online) {
	bprintf(text[NewUserPasswordVerify]);
	console|=CON_R_ECHOX;
	if(!(sys_misc&SM_ECHO_PW))
		console|=CON_L_ECHOX;
	str[0]=0;
	getstr(str,LEN_PASS,K_UPPER);
	console&=~(CON_R_ECHOX|CON_L_ECHOX);
	if(!strcmp(str,useron.pass)) break;
	sprintf(tmp,"Failed PW verification: '%s' instead of '%s'",str
		,useron.pass);
	logline(nulstr,tmp);
	if(++c==4) {
		logline("N!","Couldn't figure out password.");
		hangup(); }
	bputs(text[IncorrectPassword]);
	bprintf(text[YourPasswordIs],useron.pass); }
if(!online) return;
if(new_magic[0]) {
	bputs(text[MagicWordPrompt]);
	str[0]=0;
	getstr(str,50,K_UPPER);
	if(strcmp(str,new_magic)) {
		bputs(text[FailedMagicWord]);
		sprintf(tmp,"Failed magic word: '%s'",str);
		logline("N!",tmp);
		hangup(); }
	if(!online) return; }

i=1;
bputs(text[CheckingSlots]);
sprintf(str,"%s\\USER\\NAME.DAT",data_dir);
if(fexist(str)) {
	if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
		hangup();
		return; }
	j=filelength(file)/(LEN_ALIAS+2);	   /* total users */
	while(i<=j) {
		fread(str,LEN_ALIAS+2,1,stream);
		for(c=0;c<LEN_ALIAS;c++)
			if(str[c]==ETX) break;
		str[c]=0;
		if(!c) {	 /* should be a deleted user */
			getuserrec(i,U_MISC,8,str);
			misc=ahtoul(str);
			if(misc&DELETED) {	 /* deleted bit set too */
				getuserrec(i,U_LASTON,8,str);
				now=ahtoul(str);				/* delete long enough ? */
				if((time(NULL)-now)/86400>=sys_deldays) break; } }
		i++; }
	fclose(stream); }

j=lastuser();		/* Check against data file */
if(i<=j) {			/* Overwriting existing user */
	getuserrec(i,U_MISC,8,str);
	misc=ahtoul(str);
	if(!(misc&DELETED)) /* Not deleted? Set usernumber to end+1 */
		i=j+1; }

useron.number=i;
putuserdat(useron);
putusername(useron.number,useron.alias);
logline(nulstr,"Wrote user data");
if(new_sif[0]) {
	sprintf(str,"%sUSER\\%4.4u.DAT",data_dir,useron.number);
	create_sif_dat(new_sif,str); }
if(!(uq&UQ_NODEF))
	maindflts(useron);

delallmail(useron.number);

if(useron.number!=1 && node_valuser) {
	sprintf(str,"%sFEEDBACK.MSG",text_dir);
	CLS;
	printfile(str,P_NOABORT);
	sprintf(str,text[NewUserFeedbackHdr]
		,nulstr,getage(useron.birth),useron.sex,useron.birth
		,useron.name,useron.phone,useron.comp,useron.modem);
	email(node_valuser,str,"New User Validation",WM_EMAIL);
	if(!useron.fbacks && !useron.emails) {
		if(online) {						/* didn't hang up */
			bprintf(text[NoFeedbackWarning],username(node_valuser,tmp));
			email(node_valuser,str,"New User Validation",WM_EMAIL);
			} /* give 'em a 2nd try */
		if(!useron.fbacks && !useron.emails) {
        	bprintf(text[NoFeedbackWarning],username(node_valuser,tmp));
			logline("N!","Aborted feedback");
			hangup();
			putuserrec(useron.number,U_COMMENT,60,"Didn't leave feedback");
			putuserrec(useron.number,U_MISC,8
				,ultoa(useron.misc|DELETED,tmp,16));
			putusername(useron.number,nulstr);
			return; } } }

sprintf(str,"%sFILE\\%04u.IN",data_dir,useron.number);  /* delete any files */
delfiles(str,"*.*");                                    /* waiting for user */
rmdir(str);
sprintf(tmp,"%04u.*",useron.number);
sprintf(str,"%sFILE",data_dir);
delfiles(str,tmp);

answertime=starttime=time(NULL);	  /* set answertime to now */
sprintf(str,"%sUSER\\PTRS\\%04u.IXB",data_dir,useron.number); /* msg ptrs */
remove(str);
sprintf(str,"%sMSGS\\%04u.MSG",data_dir,useron.number); /* delete short msg */
remove(str);
sprintf(str,"%sUSER\\%04u.MSG",data_dir,useron.number); /* delete ex-comment */
remove(str);
if(newuser_mod[0])
	exec_bin(newuser_mod,&main_csi);
user_event(EVENT_NEWUSER);
logline("N+","Successful new user logon");
sys_status|=SS_NEWUSER;
}
