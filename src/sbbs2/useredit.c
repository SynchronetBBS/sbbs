#line 1 "USEREDIT.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/*******************************************************************/
/* The function useredit(), and functions that are closely related */
/*******************************************************************/

#include "sbbs.h"

#define SEARCH_TXT 0
#define SEARCH_ARS 1

int searchup(char *search,int usernum);
int searchdn(char *search,int usernum);

/****************************************************************************/
/* Edits user data. Can only edit users with a Main Security Level less 	*/
/* than or equal to the current user's Main Security Level					*/
/* Called from functions waitforcall, main_sec, xfer_sec and inkey			*/
/****************************************************************************/
void useredit(int usernumber, int local)
{
	uchar str[256],tmp2[256],tmp3[256],c,stype=SEARCH_TXT;
	uchar search[256]={""},artxt[128]={""},*ar=NULL;
	int i,j,k;
	long l;
	user_t user;

if(online==ON_REMOTE && console&(CON_R_ECHO|CON_R_INPUT) && !chksyspass(local))
	return;
if(online==ON_LOCAL) {
	if(!(sys_misc&SM_L_SYSOP))
		return;
	if(node_misc&NM_SYSPW && !chksyspass(local))
		return; }
if(usernumber)
	user.number=usernumber;
else
	user.number=useron.number;
action=NODE_SYSP;
if(sys_status&SS_INUEDIT)
	return;
sys_status|=SS_INUEDIT;
while(online) {
	CLS;
	attr(LIGHTGRAY);
	getuserdat(&user);
	if(!user.number) {
		user.number=1;
		getuserdat(&user);
		if(!user.number) {
			bputs(text[NoUserData]);
			getkey(0);
			sys_status&=~SS_INUEDIT;
			return; } }
	unixtodstr(time(NULL),str);
	unixtodstr(user.laston,tmp);
	if(strcmp(str,tmp) && user.ltoday) {
		user.ltoday=user.ttoday=user.ptoday=user.etoday=user.textra=0;
		user.freecdt=level_freecdtperday[user.level];
		putuserdat(user); }	/* Leave alone */
	if(user.misc&DELETED)
		bputs(text[Deleted]);
	else if(user.misc&INACTIVE)
		bputs(text[Inactive]);
	bprintf(text[UeditAliasPassword]
		,user.alias, (user.level>useron.level && console&CON_R_ECHO)
		|| !(sys_misc&SM_ECHO_PW) ? "XXXXXXXX" : user.pass
		, unixtodstr(user.pwmod,tmp));
	bprintf(text[UeditRealNamePhone]
		,user.level>useron.level && console&CON_R_ECHO
		? "XXXXXXXX" : user.name
		,user.level>useron.level && console&CON_R_ECHO
		? "XXX-XXX-XXXX" : user.phone);
	bprintf(text[UeditAddressBirthday]
		,user.address,getage(user.birth),user.sex,user.birth);
	bprintf(text[UeditLocationZipcode],user.location,user.zipcode);
	bprintf(text[UeditNoteHandle],user.note,user.handle);
	bprintf(text[UeditComputerModem],user.comp,user.modem);
	sprintf(str,"%sUSER\\%4.4u.MSG",data_dir,user.number);
	i=fexist(str);
	if(user.comment[0] || i)
		bprintf(text[UeditCommentLine],i ? '+' : SP
			,user.comment);
	else
		CRLF;
	unixtodos(user.laston,&date,&curtime);
	bprintf(text[UserDates]
		,unixtodstr(user.firston,str),unixtodstr(user.expire,tmp)
		,unixtodstr(user.laston,tmp2),curtime.ti_hour,curtime.ti_min);
	bprintf(text[UserTimes]
		,user.timeon,user.ttoday,level_timeperday[user.level]
		,user.tlast,level_timepercall[user.level],user.textra);
	if(user.posts)
		i=user.logons/user.posts;
	else
		i=0;
	bprintf(text[UserLogons]
		,user.logons,user.ltoday,level_callsperday[user.level],user.posts
		,i ? 100/i : user.posts>user.logons ? 100 : 0
		,user.ptoday);
	bprintf(text[UserEmails]
		,user.emails,user.fbacks,getmail(user.number,0),user.etoday);
	if(user.misc&NETMAIL)
		bprintf(text[UserNetMail],user.netmail);
	else
		CRLF;
	bprintf(text[UserUploads],ultoac(user.ulb,tmp),user.uls);
	if(user.leech)
		sprintf(str,text[UserLeech],user.leech);
	else
		str[0]=0;
	bprintf(text[UserDownloads],ultoac(user.dlb,tmp),user.dls,str);
	bprintf(text[UserCredits],ultoac(user.cdt,tmp)
		,ultoac(user.freecdt,tmp2),ultoac(level_freecdtperday[user.level],str));
	bprintf(text[UserMinutes],ultoac(user.min,tmp));
	bprintf(text[UeditSecLevel],user.level);
	bprintf(text[UeditFlags],ltoaf(user.flags1,tmp),ltoaf(user.flags3,tmp2)
		,ltoaf(user.flags2,tmp3),ltoaf(user.flags4,str));
	bprintf(text[UeditExempts],ltoaf(user.exempt,tmp),ltoaf(user.rest,tmp2));
	l=lastuser();
	ASYNC;
	if(lncntr>=rows-2)
        lncntr=0;
	bprintf(text[UeditPrompt],user.number,l);
	if(user.level>useron.level && console&CON_R_INPUT)
		strcpy(str,"QG[]?/{},");
	else
		strcpy(str,"ABCDEFGHIJKLMNOPQRSTUVWXYZ+[]?/{}~*$#");
	l=getkeys(str,l);
	if(l&0x80000000L) {
		user.number=l&~0x80000000L;
		continue; }
	switch(l) {
		case 'A':
			bputs(text[EnterYourAlias]);
			getstr(user.alias,LEN_ALIAS,K_LINE|K_EDIT|K_AUTODEL);
			putuserrec(user.number,U_ALIAS,LEN_ALIAS,user.alias);
			if(!(user.misc&DELETED))
				putusername(user.number,user.alias);
			bputs(text[EnterYourHandle]);
			getstr(user.handle,LEN_HANDLE,K_LINE|K_EDIT|K_AUTODEL);
			putuserrec(user.number,U_HANDLE,LEN_HANDLE,user.handle);
			break;
		case 'B':
			bputs(text[EnterYourBirthday]);
			gettmplt(user.birth,"nn/nn/nn",K_LINE|K_EDIT|K_AUTODEL);
			putuserrec(user.number,U_BIRTH,LEN_BIRTH,user.birth);
			break;
		case 'C':
			bputs(text[EnterYourComputer]);
			getstr(user.comp,LEN_COMP,K_LINE|K_EDIT|K_AUTODEL);
			putuserrec(user.number,U_COMP,LEN_COMP,user.comp);
			break;
		case 'D':
			if(user.misc&DELETED) {
				if(!noyes(text[UeditRestoreQ])) {
					putuserrec(user.number,U_MISC,8
						,ultoa(user.misc&~DELETED,str,16));
					putusername(user.number,user.alias); }
				break; }
			if(user.misc&INACTIVE) {
				if(!noyes(text[UeditActivateQ]))
					putuserrec(user.number,U_MISC,8
						,ultoa(user.misc&~INACTIVE,str,16));
				break; }
			if(!noyes(text[UeditDeleteQ])) {
				getsmsg(user.number);
				if(getmail(user.number,0)) {
					if(yesno(text[UeditReadUserMailWQ]))
						readmail(user.number,MAIL_YOUR); }
				if(getmail(user.number,1)) {
					if(yesno(text[UeditReadUserMailSQ]))
						readmail(user.number,MAIL_SENT); }
				putuserrec(user.number,U_MISC,8
					,ultoa(user.misc|DELETED,str,16));
				putusername(user.number,nulstr);
				break; }
			if(!noyes(text[UeditDeactivateUserQ])) {
				if(getmail(user.number,0)) {
					if(yesno(text[UeditReadUserMailWQ]))
						readmail(user.number,MAIL_YOUR); }
				if(getmail(user.number,1)) {
					if(yesno(text[UeditReadUserMailSQ]))
						readmail(user.number,MAIL_SENT); }
				putuserrec(user.number,U_MISC,8
					,ultoa(user.misc|INACTIVE,str,16));
				break; }
			break;
		case 'E':
			if(!yesno(text[ChangeExemptionQ]))
				break;
			while(online) {
				bprintf(text[FlagEditing],ltoaf(user.exempt,tmp));
				c=getkeys("ABCDEFGHIJKLMNOPQRSTUVWXYZ?\r",0);
                if(sys_status&SS_ABORT)
					break;
				if(c==CR) break;
				if(c=='?') {
					menu("EXEMPT");
					continue; }
				if(!(useron.exempt&FLAG(c)) && console&CON_R_INPUT)
					continue;
                user.exempt^=FLAG(c);
				putuserrec(user.number,U_EXEMPT,8,ultoa(user.exempt,tmp,16)); }
			break;
		case 'F':
			i=1;
			while(online) {
				bprintf("\r\nFlag Set #%d\r\n",i);
				switch(i) {
					case 1:
						bprintf(text[FlagEditing],ltoaf(user.flags1,tmp));
						break;
					case 2:
						bprintf(text[FlagEditing],ltoaf(user.flags2,tmp));
                        break;
					case 3:
						bprintf(text[FlagEditing],ltoaf(user.flags3,tmp));
                        break;
					case 4:
						bprintf(text[FlagEditing],ltoaf(user.flags4,tmp));
						break; }
				c=getkeys("ABCDEFGHIJKLMNOPQRSTUVWXYZ?1234\r",0);
				if(sys_status&SS_ABORT)
					break;
				if(c==CR) break;
				if(c=='?') {
					sprintf(str,"FLAGS%d",i);
					menu(str);
					continue; }
				if(isdigit(c)) {
					i=c&0xf;
					continue; }
				if(console & CON_R_INPUT)
					switch(i) {
						case 1:
							if(!(useron.flags1&FLAG(c)))
								continue;
							break;
						case 2:
							if(!(useron.flags2&FLAG(c)))
								continue;
                            break;
						case 3:
							if(!(useron.flags3&FLAG(c)))
								continue;
                            break;
						case 4:
							if(!(useron.flags4&FLAG(c)))
								continue;
							break; }
				switch(i) {
					case 1:
						user.flags1^=FLAG(c);
						putuserrec(user.number,U_FLAGS1,8
							,ultoa(user.flags1,tmp,16));
						break;
					case 2:
						user.flags2^=FLAG(c);
						putuserrec(user.number,U_FLAGS2,8
							,ultoa(user.flags2,tmp,16));
						break;
					case 3:
						user.flags3^=FLAG(c);
						putuserrec(user.number,U_FLAGS3,8
							,ultoa(user.flags3,tmp,16));
                        break;
					case 4:
						user.flags4^=FLAG(c);
						putuserrec(user.number,U_FLAGS4,8
							,ultoa(user.flags4,tmp,16));
						break; } }
			break;
		case 'G':
			bputs(text[GoToUser]);
			if(getstr(str,LEN_ALIAS,K_UPPER|K_LINE)) {
				if(isdigit(str[0])) {
					i=atoi(str);
					if(i>lastuser())
						break;
					if(i) user.number=i; }
				else {
					i=finduser(str);
					if(i) user.number=i; } }
			break;
		case 'H': /* edit user's information file */
			attr(LIGHTGRAY);
            sprintf(str,"%sUSER\\%4.4u.MSG",data_dir,user.number);
            editfile(str);
            break;
		case 'I':
			maindflts(user);
			break;
		case 'J':   /* Edit Minutes */
			bputs(text[UeditMinutes]);
			ultoa(user.min,str,10);
            if(getstr(str,10,K_NUMBER|K_LINE))
				putuserrec(user.number,U_MIN,10,str);
            break;
		case 'K':	/* date changes */
			bputs(text[UeditLastOn]);
			unixtodstr(user.laston,str);
			gettmplt(str,"nn/nn/nn",K_LINE|K_EDIT);
			if(sys_status&SS_ABORT)
				break;
			user.laston=dstrtounix(str);
			putuserrec(user.number,U_LASTON,8,ultoa(user.laston,tmp,16));
			bputs(text[UeditFirstOn]);
			unixtodstr(user.firston,str);
			gettmplt(str,"nn/nn/nn",K_LINE|K_EDIT);
			if(sys_status&SS_ABORT)
				break;
			user.firston=dstrtounix(str);
			putuserrec(user.number,U_FIRSTON,8,ultoa(user.firston,tmp,16));
			bputs(text[UeditExpire]);
			unixtodstr(user.expire,str);
			gettmplt(str,"nn/nn/nn",K_LINE|K_EDIT);
			if(sys_status&SS_ABORT)
				break;
			user.expire=dstrtounix(str);
			putuserrec(user.number,U_EXPIRE,8,ultoa(user.expire,tmp,16));
			bputs(text[UeditPwModDate]);
			unixtodstr(user.pwmod,str);
			gettmplt(str,"nn/nn/nn",K_LINE|K_EDIT);
			if(sys_status&SS_ABORT)
				break;
			user.pwmod=dstrtounix(str);
			putuserrec(user.number,U_PWMOD,8,ultoa(user.pwmod,tmp,16));
			break;
		case 'L':
			bputs(text[EnterYourAddress]);
			getstr(user.address,LEN_ADDRESS,K_LINE|K_EDIT|K_AUTODEL);
			if(sys_status&SS_ABORT)
				break;
			putuserrec(user.number,U_ADDRESS,LEN_ADDRESS,user.address);
			bputs(text[EnterYourCityState]);
			getstr(user.location,LEN_LOCATION,K_LINE|K_EDIT|K_AUTODEL);
			if(sys_status&SS_ABORT)
				break;
			putuserrec(user.number,U_LOCATION,LEN_LOCATION,user.location);
			bputs(text[EnterYourZipCode]);
			getstr(user.zipcode,LEN_ZIPCODE,K_LINE|K_EDIT|K_AUTODEL|K_UPPER);
			if(sys_status&SS_ABORT)
				break;
			putuserrec(user.number,U_ZIPCODE,LEN_ZIPCODE,user.zipcode);
			break;
		case 'M':
			bputs(text[UeditML]);
			itoa(user.level,str,10);
			if(getstr(str,2,K_NUMBER|K_LINE))
				if(!(atoi(str)>useron.level && console&CON_R_INPUT))
					putuserrec(user.number,U_LEVEL,2,str);
			break;
		case 'N':
			bputs(text[UeditNote]);
			getstr(user.note,LEN_NOTE,K_LINE|K_EDIT|K_AUTODEL);
			putuserrec(user.number,U_NOTE,LEN_NOTE,user.note);
			break;
		case 'O':
			bputs(text[UeditComment]);
			getstr(user.comment,60,K_LINE|K_EDIT|K_AUTODEL);
			putuserrec(user.number,U_COMMENT,60,user.comment);
			break;
		case 'P':
			bputs(text[EnterYourPhoneNumber]);
			getstr(user.phone,LEN_PHONE,K_UPPER|K_LINE|K_EDIT|K_AUTODEL);
			putuserrec(user.number,U_PHONE,LEN_PHONE,user.phone);
			break;
		case 'Q':
			CLS;
			sys_status&=~SS_INUEDIT;
			if(ar)
				FREE(ar);
			return;
		case 'R':
			bputs(text[EnterYourRealName]);
			getstr(user.name,LEN_NAME,K_LINE|K_EDIT|K_AUTODEL);
			putuserrec(user.number,U_NAME,LEN_NAME,user.name);
			break;
		case 'S':
			bputs(text[EnterYourSex]);
			if(getstr(str,1,K_UPPER|K_LINE))
				putuserrec(user.number,U_SEX,1,str);
			break;
		case 'T':   /* Text Search */
			bputs(text[SearchStringPrompt]);
			if(getstr(search,30,K_UPPER|K_LINE))
				stype=SEARCH_TXT;
			break;
		case 'U':
			bputs(text[UeditUlBytes]);
			ultoa(user.ulb,str,10);
			if(getstr(str,10,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL))
				putuserrec(user.number,U_ULB,10,str);
			if(sys_status&SS_ABORT)
				break;
			bputs(text[UeditUploads]);
			sprintf(str,"%u",user.uls);
			if(getstr(str,5,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL))
				putuserrec(user.number,U_ULS,5,str);
            if(sys_status&SS_ABORT)
				break;
			bputs(text[UeditDlBytes]);
			ultoa(user.dlb,str,10);
			if(getstr(str,10,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL))
				putuserrec(user.number,U_DLB,10,str);
            if(sys_status&SS_ABORT)
				break;
			bputs(text[UeditDownloads]);
			sprintf(str,"%u",user.dls);
			if(getstr(str,5,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL))
				putuserrec(user.number,U_DLS,5,str);
			break;
		case 'V':
			CLS;
			attr(LIGHTGRAY);
			for(i=0;i<10;i++) {
				bprintf(text[QuickValidateFmt]
					,i,val_level[i],ltoaf(val_flags1[i],str)
					,ltoaf(val_exempt[i],tmp)
					,ltoaf(val_rest[i],tmp3)); }
			ASYNC;
			bputs(text[QuickValidatePrompt]);
			c=getkey(0);
			if(!isdigit(c))
				break;
			i=c&0xf;
			user.level=val_level[i];
			user.flags1=val_flags1[i];
			user.flags2=val_flags2[i];
			user.flags3=val_flags3[i];
			user.flags4=val_flags4[i];
			user.exempt=val_exempt[i];
			user.rest=val_rest[i];
			user.cdt+=val_cdt[i];
			now=time(NULL);
			if(val_expire[i]) {
				if(user.expire<now)
					user.expire=now+((long)val_expire[i]*24L*60L*60L);
				else
					user.expire+=((long)val_expire[i]*24L*60L*60L); }
			putuserdat(user);
			break;
		case 'W':
			bputs(text[UeditPassword]);
			getstr(user.pass,LEN_PASS,K_UPPER|K_LINE|K_EDIT|K_AUTODEL);
			putuserrec(user.number,U_PASS,LEN_PASS,user.pass);
			break;
		case 'X':
			attr(LIGHTGRAY);
            sprintf(str,"%sUSER\\%4.4u.MSG",data_dir,user.number);
			printfile(str,0);
			pause();
			break;
        case 'Y':
			if(!noyes(text[UeditCopyUserQ])) {
				bputs(text[UeditCopyUserToSlot]);
				i=getnum(lastuser());
				if(i>0) {
					user.number=i;
					putusername(user.number,user.alias);
					putuserdat(user); } }
			break;
		case 'Z':
			if(!yesno(text[ChangeRestrictsQ]))
				break;
			while(online) {
				bprintf(text[FlagEditing],ltoaf(user.rest,tmp));
				c=getkeys("ABCDEFGHIJKLMNOPQRSTUVWXYZ?\r",0);
                if(sys_status&SS_ABORT)
					break;
				if(c==CR) break;
				if(c=='?') {
					menu("RESTRICT");
					continue; }
				user.rest^=FLAG(c);
				putuserrec(user.number,U_REST,8,ultoa(user.rest,tmp,16)); }
			break;
		case '?':
			CLS;
			menu("UEDIT");  /* Sysop Uedit Edit Menu */
			pause();
			break;
		case '~':
			bputs(text[UeditLeech]);
			if(getstr(str,2,K_NUMBER|K_LINE))
				putuserrec(user.number,U_LEECH,2,itoa(atoi(str),tmp,16));
            break;
		case '+':
			bputs(text[ModifyCredits]);
			getstr(str,10,K_UPPER|K_LINE);
			l=atol(str);
			if(strstr(str,"M"))
				l*=0x100000L;
			else if(strstr(str,"K"))
				l*=1024;
			else if(strstr(str,"$"))
				l*=cdt_per_dollar;
			if(l<0L && l*-1 > user.cdt)
				user.cdt=0L;
			else
				user.cdt+=l;
			putuserrec(user.number,U_CDT,10,ultoa(user.cdt,tmp,10));
			break;
		case '*':
			bputs(text[ModifyMinutes]);
			getstr(str,10,K_UPPER|K_LINE);
			l=atol(str);
			if(strstr(str,"H"))
				l*=60L;
			if(l<0L && l*-1 > user.min)
				user.min=0L;
			else
				user.min+=l;
			putuserrec(user.number,U_MIN,10,ultoa(user.min,tmp,10));
			break;
		case '#': /* read new user questionaire */
			sprintf(str,"%sUSER\\%4.4u.DAT",data_dir,user.number);
			if(!new_sof[0] || !fexist(str))
				break;
			read_sif_dat(new_sof,str);
			if(!noyes(text[DeleteQuestionaireQ]))
				remove(str);
			break;
		case '$':
			bputs(text[UeditCredits]);
            ultoa(user.cdt,str,10);
            if(getstr(str,10,K_NUMBER|K_LINE))
                putuserrec(user.number,U_CDT,10,str);
            break;
		case '/':
			bputs(text[SearchStringPrompt]);
			if(getstr(artxt,40,K_UPPER|K_LINE))
				stype=SEARCH_ARS;
			if(ar && ar[0])
				FREE(ar);
			ar=arstr(0,artxt);
            break;
		case '{':
			if(stype==SEARCH_TXT)
				user.number=searchdn(search,user.number);
			else {
				if(!ar)
					break;
				k=user.number;
				for(i=k-1;i;i--) {
					user.number=i;
					getuserdat(&user);
					if(chk_ar(ar,user)) {
						outchar(7);
						break; } }
				if(!i)
					user.number=k; }
			break;
		case '}':
			if(stype==SEARCH_TXT)
				user.number=searchup(search,user.number);
			else {
				if(!ar)
					break;
				j=lastuser();
				k=user.number;
				for(i=k+1;i<=j;i++) {
					user.number=i;
					getuserdat(&user);
					if(chk_ar(ar,user)) {
						outchar(7);
						break; } }
				if(i>j)
                    user.number=k; }
			break;
		case ']':
			if(user.number==lastuser())
				user.number=1;
			else user.number++;
			break;
		case '[':
			if(user.number==1)
				user.number=lastuser();
			else user.number--;
			break; } }
sys_status&=~SS_INUEDIT;
}

/****************************************************************************/
/* Seaches forward through the USER.DAT file for the ocurrance of 'search'  */
/* starting at the offset for usernum+1 and returning the usernumber of the */
/* record where the string was found or the original usernumber if the 		*/
/* string wasn't found														*/
/* Called from the function useredit										*/
/****************************************************************************/
int searchup(char *search,int usernum)
{
	char userdat[U_LEN+1];
	int file,count;
	uint i=usernum+1;

if(!search[0])
	return(usernum);
sprintf(userdat,"%sUSER\\USER.DAT",data_dir);
if((file=nopen(userdat,O_RDONLY|O_DENYNONE))==-1)
	return(usernum);
lseek(file,(long)((long)usernum*U_LEN),0);

while(!eof(file)) {
	count=0;
	while(count<LOOP_NODEDAB
		&& lock(file,(long)((long)(i-1)*U_LEN),U_LEN)==-1) {
		if(count>10)
			mswait(55);
		count++; }

	if(count>=LOOP_NODEDAB) {
		close(file);
		errormsg(WHERE,ERR_LOCK,"USER.DAT",i);
		return(usernum); }

	if(read(file,userdat,U_LEN)!=U_LEN) {
		unlock(file,(long)((long)(i-1)*U_LEN),U_LEN);
		close(file);
		errormsg(WHERE,ERR_READ,"USER.DAT",U_LEN);
		return(usernum); }

	unlock(file,(long)((long)(i-1)*U_LEN),U_LEN);
	userdat[U_LEN]=0;
	strupr(userdat);
	if(strstr(userdat,search)) {
		outchar(7);
		close(file);
		return(i); }
	i++; }
close(file);
return(usernum);
}

/****************************************************************************/
/* Seaches backward through the USER.DAT file for the ocurrance of 'search' */
/* starting at the offset for usernum-1 and returning the usernumber of the */
/* record where the string was found or the original usernumber if the 		*/
/* string wasn't found														*/
/* Called from the function useredit										*/
/****************************************************************************/
int searchdn(char *search,int usernum)
{
	char userdat[U_LEN+1];
	int file,count;
	uint i=usernum-1;

if(!search[0])
	return(usernum);
sprintf(userdat,"%sUSER\\USER.DAT",data_dir);
if((file=nopen(userdat,O_RDONLY))==-1)
	return(usernum);
while(i) {
	lseek(file,(long)((long)(i-1)*U_LEN),0);
	count=0;
	while(count<LOOP_NODEDAB
		&& lock(file,(long)((long)(i-1)*U_LEN),U_LEN)==-1) {
		if(count>10)
			mswait(55);
		count++; }

	if(count>=LOOP_NODEDAB) {
		close(file);
		errormsg(WHERE,ERR_LOCK,"USER.DAT",i);
        return(usernum); }

	if(read(file,userdat,U_LEN)==-1) {
		unlock(file,(long)((long)(i-1)*U_LEN),U_LEN);
		close(file);
		errormsg(WHERE,ERR_READ,"USER.DAT",U_LEN);
		return(usernum); }
	unlock(file,(long)((long)(i-1)*U_LEN),U_LEN);
	userdat[U_LEN]=0;
	strupr(userdat);
	if(strstr(userdat,search)) {
		outchar(7);
		close(file);
		return(i); }
	i--; }
close(file);
return(usernum);
}

/****************************************************************************/
/* This function view/edits the users main default settings.				*/
/****************************************************************************/
void maindflts(user_t user)
{
	char str[256],ch;
	int i;

action=NODE_DFLT;
while(online) {
	CLS;
/*
	if(user.number==useron.number && useron.rest&FLAG('G')) /* Guest */
		user=useron;
	else
*/
		getuserdat(&user);
    if(user.rows)
		rows=user.rows;
	bprintf(text[UserDefaultsHdr],user.alias,user.number);
	sprintf(str,"%s%s%s%s%s"
						,user.misc&AUTOTERM ? "Auto Detect ":nulstr
						,user.misc&ANSI ? "ANSI ":"TTY "
						,user.misc&COLOR ? "(Color) ":"(Mono) "
						,user.misc&WIP	? "WIP" : user.misc&RIP ? "RIP "
							:nulstr
						,user.misc&NO_EXASCII ? "ASCII Only":nulstr);
	bprintf(text[UserDefaultsTerminal],str);
	if(total_xedits)
		bprintf(text[UserDefaultsXeditor]
			,user.xedit ? xedit[user.xedit-1]->name : "None");
	if(user.rows)
		itoa(user.rows,tmp,10);
	else
		sprintf(tmp,"Auto Detect (%d)",rows);
	bprintf(text[UserDefaultsRows],tmp);
	if(total_shells>1)
		bprintf(text[UserDefaultsCommandSet]
			,shell[user.shell]->name);
	bprintf(text[UserDefaultsArcType]
		,user.tmpext);
	bprintf(text[UserDefaultsMenuMode]
		,user.misc&EXPERT ? text[On] : text[Off]);
	bprintf(text[UserDefaultsPause]
		,user.misc&UPAUSE ? text[On] : text[Off]);
	bprintf(text[UserDefaultsHotKey]
		,user.misc&COLDKEYS ? text[Off] : text[On]);
	bprintf(text[UserDefaultsCursor]
		,user.misc&SPIN ? text[On] : text[Off]);
	bprintf(text[UserDefaultsCLS]
		,user.misc&CLRSCRN ? text[On] : text[Off]);
	bprintf(text[UserDefaultsAskNScan]
		,user.misc&ASK_NSCAN ? text[On] : text[Off]);
	bprintf(text[UserDefaultsAskSScan]
        ,user.misc&ASK_SSCAN ? text[On] : text[Off]);
	bprintf(text[UserDefaultsANFS]
		,user.misc&ANFSCAN ? text[On] : text[Off]);
	bprintf(text[UserDefaultsRemember]
		,user.misc&CURSUB ? text[On] : text[Off]);
	bprintf(text[UserDefaultsBatFlag]
		,user.misc&BATCHFLAG ? text[On] : text[Off]);
	if(sys_misc&SM_FWDTONET)
		bprintf(text[UserDefaultsNetMail]
			,user.misc&NETMAIL ? text[On] : text[Off]);
	if(useron.exempt&FLAG('Q') || user.misc&QUIET)
		bprintf(text[UserDefaultsQuiet]
			,user.misc&QUIET ? text[On] : text[Off]);
	if(user.prot!=SP)
		sprintf(str,"%c",user.prot);
	else
		strcpy(str,"None");
	bprintf(text[UserDefaultsProtocol],str
        ,user.misc&AUTOHANG ? "(Hang-up After Xfer)":nulstr);
	if(sys_misc&SM_PWEDIT && !(user.rest&FLAG('G')))
		bputs(text[UserDefaultsPassword]);

	ASYNC;
	bputs(text[UserDefaultsWhich]);
	strcpy(str,"HTBALPRSYFNCQXZ\r");
	if(sys_misc&SM_PWEDIT && !(user.rest&FLAG('G')))
		strcat(str,"W");
	if(useron.exempt&FLAG('Q') || user.misc&QUIET)
		strcat(str,"D");
	if(total_xedits)
		strcat(str,"E");
	if(sys_misc&SM_FWDTONET)
		strcat(str,"M");
	if(total_shells>1)
		strcat(str,"K");
	ch=getkeys(str,0);
	switch(ch) {
		case 'T':
			if(yesno(text[AutoTerminalQ])) {
				user.misc|=AUTOTERM;
				user.misc&=~(ANSI|RIP|WIP);
				user.misc|=autoterm; }
			else
				user.misc&=~AUTOTERM;
			if(!(user.misc&AUTOTERM)) {
				if(yesno(text[AnsiTerminalQ]))
					user.misc|=ANSI;
				else
					user.misc&=~(ANSI|COLOR); }
			if(user.misc&ANSI) {
				if(yesno(text[ColorTerminalQ]))
					user.misc|=COLOR;
				else
					user.misc&=~COLOR; }
			if(!yesno(text[ExAsciiTerminalQ]))
				user.misc|=NO_EXASCII;
			else
				user.misc&=~NO_EXASCII;
			if(!(user.misc&AUTOTERM)) {
				if(!noyes(text[RipTerminalQ]))
					user.misc|=RIP;
				else
					user.misc&=~RIP; }
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
			break;
		case 'B':
			user.misc^=BATCHFLAG;
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
			break;
		case 'E':
			if(noyes("Use an external editor")) {
				putuserrec(user.number,U_XEDIT,8,nulstr);
				break; }
			if(user.xedit)
				user.xedit--;
			for(i=0;i<total_xedits;i++)
				uselect(1,i,"External Editor",xedit[i]->name,xedit[i]->ar);
			if((i=uselect(0,user.xedit,0,0,0))>=0)
				putuserrec(user.number,U_XEDIT,8,xedit[i]->code);
			break;
		case 'K':   /* Command shell */
			for(i=0;i<total_shells;i++)
				uselect(1,i,"Command Shell",shell[i]->name,shell[i]->ar);
			if((i=uselect(0,user.shell,0,0,0))>=0)
				putuserrec(user.number,U_SHELL,8,shell[i]->code);
			break;
		case 'A':
			for(i=0;i<total_fcomps;i++)
				uselect(1,i,"Archive Type",fcomp[i]->ext,fcomp[i]->ar);
			if((i=uselect(0,0,0,0,0))>=0)
				putuserrec(user.number,U_TMPEXT,3,fcomp[i]->ext);
			break;
		case 'L':
			bputs(text[HowManyRows]);
			if((ch=getnum(99))!=-1)
				putuserrec(user.number,U_ROWS,2,itoa(ch,tmp,10));
			break;
		case 'P':
			user.misc^=UPAUSE;
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
			break;
		case 'H':
			user.misc^=COLDKEYS;
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
            break;
		case 'S':
			user.misc^=SPIN;
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
			break;
		case 'F':
			user.misc^=ANFSCAN;
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
			break;
		case 'X':
			user.misc^=EXPERT;
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
			break;
		case 'R':   /* Remember current sub/dir */
			user.misc^=CURSUB;
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
			break;
		case 'Y':   /* Prompt for scanning message to you */
			user.misc^=ASK_SSCAN;
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
            break;
		case 'N':   /* Prompt for new message/files scanning */
			user.misc^=ASK_NSCAN;
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
			break;
		case 'M':   /* NetMail address */
			if(noyes(text[ForwardMailQ]))
				user.misc&=~NETMAIL;
			else {
				user.misc|=NETMAIL;
				bputs(text[EnterNetMailAddress]);
				if(!getstr(user.netmail,LEN_NETMAIL,K_EDIT|K_AUTODEL|K_LINE))
					break;
				putuserrec(user.number,U_NETMAIL,LEN_NETMAIL,user.netmail); }
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
			break;
		case 'C':
			user.misc^=CLRSCRN;
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
			break;
		case 'D':
			user.misc^=QUIET;
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
			break;
		case 'W':
			if(noyes(text[NewPasswordQ]))
				break;
			bputs(text[CurrentPassword]);
			console|=CON_R_ECHOX;
			if(!(sys_misc&SM_ECHO_PW))
				console|=CON_L_ECHOX;
			ch=getstr(str,LEN_PASS,K_UPPER);
			console&=~(CON_R_ECHOX|CON_L_ECHOX);
			if(strcmp(str,user.pass)) {
				bputs(text[WrongPassword]);
				pause();
                break; }
			bputs(text[NewPassword]);
			if(!getstr(str,LEN_PASS,K_UPPER|K_LINE))
				break;
			truncsp(str);
			if(!chkpass(str,user)) {
				CRLF;
				pause();
				break; }
			bputs(text[VerifyPassword]);
			console|=CON_R_ECHOX;
			if(!(sys_misc&SM_ECHO_PW))
				console|=CON_L_ECHOX;
			getstr(tmp,LEN_PASS,K_UPPER);
			console&=~(CON_R_ECHOX|CON_L_ECHOX);
			if(strcmp(str,tmp)) {
				bputs(text[WrongPassword]);
				pause();
				break; }
			if(!online)
				break;
			putuserrec(user.number,U_PASS,LEN_PASS,str);
			now=time(NULL);
			putuserrec(user.number,U_PWMOD,8,ultoa(now,tmp,16));
			bputs(text[PasswordChanged]);
			logline(nulstr,"Changed password");
			pause();
			break;
		case 'Z':
			menu("DLPROT");
            SYNC;
            mnemonics(text[ProtocolOrQuit]);
			strcpy(str,"Q");
            for(i=0;i<total_prots;i++)
				if(prot[i]->dlcmd[0] && chk_ar(prot[i]->ar,useron)) {
                    sprintf(tmp,"%c",prot[i]->mnemonic);
					strcat(str,tmp); }
			ch=getkeys(str,0);
			if(ch=='Q' || sys_status&SS_ABORT) {
				ch=SP;
				putuserrec(user.number,U_PROT,1,&ch); }
			else
				putuserrec(user.number,U_PROT,1,&ch);
			if(yesno(text[HangUpAfterXferQ]))
				user.misc|=AUTOHANG;
			else
				user.misc&=~AUTOHANG;
			putuserrec(user.number,U_MISC,8,ultoa(user.misc,str,16));
			break;
		default:
			return; } }
}

void purgeuser(int usernumber)
{
	uchar str[128];
	user_t user;

user.number=usernumber;
getuserdat(&user);
sprintf(str,"Purged %s #%u",user.alias,usernumber);
logentry("!*",str);
delallmail(usernumber);
putusername(usernumber,nulstr);
putuserrec(usernumber,U_MISC,8,ultoa(user.misc|DELETED,str,16));
}
