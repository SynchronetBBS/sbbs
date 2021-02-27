/* SCBCFG.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include <uifc.h>
#include <sys\stat.h>
#include "scb.h"

#define MAX_PRFX	500
#define LEN_PRFX	20

char **opt;
char **prfx;

int add();
void bail(int code);
void main();
char fexist(char *filespec);

unsigned _stklen=16000; 		  /* Set stack size in code, not header */

void bail(int code)
{

if(code)
	getch();
uifcbail();
exit(code);
}

int add()
{
	int i;

i=0;
strcpy(opt[0],"Add these to the user account");
strcpy(opt[1],"Remove these from the user account");
opt[2][0]=0;
/*
Add/Remove Flags/Restricts/Exempts:

Select ADD if you want these flags/restricts/exempts added to the
user upon validation, or select REMOVE to take them away upon
validation.

*/
i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0,"Add or Remove From Account",opt);
return(i);
}

void main()
{
	char str[128],canfile[256],addfile[256],flags1[81]={NULL},
		 flags2[81]={NULL},flags3[81]={NULL},flags4[81]={NULL},
		 exempt[81]={NULL},restrict[81]={NULL},expiration[81]={NULL},
		 credits[81]={NULL},minutes[81]={NULL},level[81]={NULL},
		 callout_attempts[81]={NULL},sysop[81]={NULL},bbs_ac[81],*p;
	int file,i,j,k,options=0,ldstart[7]={NULL},ldend[7]={NULL},min_phone_len=7,
		max_phone_len=11,total_prfxs,dflt,b,hangup_time;
	FILE *stream;

savnum=0;
if((opt=(char **)MALLOC(sizeof(char *)*300))==NULL) {
	cputs("memory allocation error\r\n");
	bail(1); }
for(i=0;i<300;i++)
	if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL) {
		cputs("memory allocation error\r\n");
        bail(1); }
uifcini();
sprintf(str,"Synchronet Callback v%s",VERSION);
uscrn(str);

if(!(fexist("SCB.CFG"))) {
	strcpy(callout_attempts,"5");
	strcpy(canfile,"\\SBBS\\TEXT\\PHONE.CAN");
	strcpy(addfile,canfile);
	options|=MODIFY_USER_NOTE; }
else {
	if((file=open("SCB.CFG",O_RDONLY|O_BINARY|O_DENYNONE))==-1) {
		textattr(LIGHTGRAY);
		clrscr();
		lprintf("Error opening SCB.CFG\r\n");
		bail(1); }
	if((stream=fdopen(file,"rb"))==NULL) {
		textattr(LIGHTGRAY);
		clrscr();
		lprintf("Error fdopen SCB.CFG\r\n");
		bail(1); }
	fgets(callout_attempts,81,stream); truncsp(callout_attempts);
	fgets(str,81,stream); truncsp(str);
	if(str[0]=='Y') options|=ALWAYS_VALIDATE;
	if(str[1]=='Y') options|=MODIFY_USER_NOTE;
	if(str[2]=='Y') options|=START_WITH_0;
	if(str[3]=='Y') options|=START_WITH_1;
	if(str[4]=='Y') options|=STAY_CONNECTED;
	if(str[5]=='Y') options|=SC_LOCAL_ONLY;
	if(str[6]=='Y') options|=US_PHONE_FORMAT;
	if(str[7]=='Y') options|=ALLOWED_ONLY;
	if(str[8]!='N') options|=SAME_AREA_LD;
	fgets(canfile,81,stream); truncsp(canfile);
	fgets(addfile,81,stream); truncsp(addfile);
	fgets(credits,81,stream); truncsp(credits);
	fgets(sysop,81,stream); truncsp(sysop);
	fgets(level,81,stream); truncsp(level);
	fgets(flags1,81,stream); truncsp(flags1);
	fgets(flags2,81,stream); truncsp(flags2);
	fgets(exempt,81,stream); truncsp(exempt);
	fgets(restrict,81,stream); truncsp(restrict);
	fgets(expiration,81,stream); truncsp(expiration);
	fgets(minutes,81,stream); truncsp(minutes);
	fgets(flags3,81,stream); truncsp(flags3);
	fgets(flags4,81,stream); truncsp(flags4);
	for(i=0;i<7;i++) {
		fgets(str,81,stream); ldstart[i]=atoi(str);    /* min since midnight */
		fgets(str,81,stream); ldend[i]=atoi(str); }
	fgets(str,81,stream); min_phone_len=atoi(str);
	fgets(str,81,stream); max_phone_len=atoi(str);
	fgets(bbs_ac,81,stream); truncsp(bbs_ac);
	if(fgets(str,81,stream)) {
		hangup_time=atoi(str);
		if(hangup_time>90)
			hangup_time=90; }
	else
		hangup_time=30;
	str[0]=0;
	fgets(str,81,stream);	// regnum
	fclose(stream); }
dflt=0;
while(1) {
	helpbuf=
" Synchronet Callback Configuration \r\n\r\n"
"Move through the various options using the arrow keys.  Select the\r\n"
"highlighted options by pressing ENTER.\r\n\r\n";
	j=0;
	sprintf(opt[j++],"Toggle Options...");
	sprintf(opt[j++],"Validation Values...");
	sprintf(opt[j++],"Allowed Prefix List...");
	sprintf(opt[j++],"Long Distance Prefix List...");
	sprintf(opt[j++],"Long Distance Calling Times...");
	sprintf(opt[j++],"Phone Number Trash Can   %-30.30s",canfile);
	sprintf(opt[j++],"Validated Phone List     %-30.30s",addfile);
	sprintf(opt[j++],"Callback Attempts        %-2.2s",callout_attempts);
	sprintf(opt[j++],"Minimum Phone Length     %u",min_phone_len);
	sprintf(opt[j++],"Maximum Phone Length     %u",max_phone_len);
	sprintf(opt[j++],"Wait Before Dialing      %u seconds",hangup_time);
	sprintf(opt[j++],"BBS Area Code            %-3.3s",bbs_ac);
	if(sysop[0])
		sprintf(str,"User #%s",sysop);
	else
		sprintf(str,"Disabled");
	sprintf(opt[j++],"Send Message to Sysop    %-30.30s",str);
	opt[j][0]=NULL;
	switch(ulist(WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC,0,0,60,&dflt,0
	,"Synchronet Callback Configuration",opt)) {
		case 0:
			j=0;
			while(1) {
				k=0;
				sprintf(opt[k++],"%-40.40s%-3s","Validate if Unable to Verify"
					,options&ALWAYS_VALIDATE ? "Yes":"No");
				sprintf(opt[k++],"%-40.40s%-3s","Put Result in User Note"
					,options&MODIFY_USER_NOTE ? "Yes":"No");
				sprintf(opt[k++],"%-40.40s%-3s"
					,"Long Distance if not an Allowed Prefix"
					,options&ALLOWED_ONLY ? "Yes":"No");
				sprintf(opt[k++],"%-40.40s%-3s"
					,"Allow Long Distance (Starting with 0)"
					,options&START_WITH_0 ? "Yes":"No");
				sprintf(opt[k++],"%-40.40s%-3s"
					,"Allow Long Distance (Starting with 1)"
					,options&START_WITH_1 ? "Yes":"No");
				sprintf(opt[k++],"%-40.40s%-3s"
					,"Allow Long Distance (Same Area Code)"
					,options&SAME_AREA_LD ? "Yes":"No");
				sprintf(opt[k++],"%-40.40s%-3s"
					,"U.S. Style Phone Format (AAA-PPP-SSSS)"
					,options&US_PHONE_FORMAT ? "Yes":"No");
				sprintf(opt[k++],"%-40.40s%-10s","Stay Connected After Callback"
					,options&STAY_CONNECTED && options&SC_LOCAL_ONLY ?
					"Local Only" : options&STAY_CONNECTED ? "Yes":"No");
				opt[k][0]=NULL;
helpbuf=
" Toggle Options \r\n\r\n"
"Validate if Unable to Call Back\r\n"
"  Setting this to Yes will validate the user even if the board is unable "
"\r\n"
"  to call the user back.\r\n"
"Put Result in User Note\r\n"
"  When set to Yes, this option will place the phone number, and\r\n"
"  information pertaining to the call into the user's note.\r\n"
"Allow Numbers that Start with 0 / Allow Numbers that Start with 1\r\n"
"  Set these options to Yes to allow SCB to call phone numbers which\r\n"
"  start with a 0 or a 1 (respectively).\r\n"
"U.S. Style Phone Format\r\n"
"  Toggle this option to Yes if you live in an area that uses a phone\r\n"
"  number format of (AAA) PPP-SSSS.\r\n"
"Stay Connected After Callback\r\n"
"  Toggle this option to Yes to allow SCB to stay connected after a\r\n"
"  callback, Local Only for local numbers, or No to disconnect.";

				j=ulist(0,0,0,0,&j,0,"Toggle Options",opt);
				if(j==-1)
					break;
				switch(j) {
					case 0:
						options^=ALWAYS_VALIDATE;
						break;
					case 1:
						options^=MODIFY_USER_NOTE;
						break;
					case 2:
						options^=ALLOWED_ONLY;
						break;
					case 3:
						options^=START_WITH_0;
						break;
					case 4:
						options^=START_WITH_1;
						break;
					case 5:
						options^=SAME_AREA_LD;
                        break;
					case 6:
						options^=US_PHONE_FORMAT;
						break;
					case 7:
						if(options&STAY_CONNECTED && options&SC_LOCAL_ONLY) {
							options^=STAY_CONNECTED;
							options^=SC_LOCAL_ONLY;
						} else if(options&STAY_CONNECTED)
							options^=SC_LOCAL_ONLY;
						else options^=STAY_CONNECTED;
						break; } }
            break;
		case 1:
			j=0;
			while(1) {
				k=0;
				sprintf(opt[k++],"%-30.30s  %-27.27s","Security Level"
					,level);
				sprintf(opt[k++],"%-30.30s  %-27.27s","Flag Set 1"
					,flags1);
				sprintf(opt[k++],"%-30.30s  %-27.27s","Flag Set 2"
					,flags2);
				sprintf(opt[k++],"%-30.30s  %-27.27s","Flag Set 3"
					,flags3);
				sprintf(opt[k++],"%-30.30s  %-27.27s","Flag Set 4"
					,flags4);
				sprintf(opt[k++],"%-30.30s  %-27.27s","Exemptions"
					,exempt);
				sprintf(opt[k++],"%-30.30s  %-27.27s","Restrictions"
					,restrict);
				sprintf(opt[k++],"%-30.30s  %-27.27s"
					,"Days to Extend Expiration"
					,expiration);
				sprintf(opt[k++],"%-30.30s  %-27.27s","Credits to Add"
					,credits);
				sprintf(opt[k++],"%-30.30s  %-27.27s","Minutes to Add"
					,minutes);
				opt[k][0]=NULL;
helpbuf=
" Validation Values \r\n\r\n"
"These are the values that will be placed into user accounts that have\r\n"
"been successfully verified through the Synchronet Callback.";


				j=ulist(WIN_ACT,0,0,0,&j,0,"Validation Values",opt);
				if(j==-1)
					break;
				switch(j) {
					case 0:
helpbuf=
" Security Level \r\n\r\n"
"This is the Security level that will be given to users that are called\r\n"
"back successfully.";
						uinput(WIN_L2R|WIN_MID|WIN_SAV,0,0,
							"Security Level",level,2,K_EDIT|K_NUMBER);
						break;
					case 1:
helpbuf=
" Flag Set 1 \r\n\r\n"
"These are the different flags to be added to (or removed from) the\r\n"
"user account.";
						p=strstr(flags1,"-");
						if(p!=NULL) sprintf(str,"%s",p+1);
						else
							sprintf(str,"%s",flags1);
						if(uinput(WIN_L2R|WIN_MID|WIN_SAV,0,0,"Flag Set 1",str
							,26,K_EDIT|K_UPPER|K_ALPHA)<0)
							break;
						if(str[0] && add()>0) sprintf(flags1,"-%s",str);
						else
							sprintf(flags1,"%s",str);
                        break;
					case 2:
helpbuf=
" Flag Set 2 \r\n\r\n"
"These are the different flags to be added to (or removed from) the\r\n"
"user account.";
						p=strstr(flags2,"-");
						if(p!=NULL) sprintf(str,"%s",p+1);
						else
							sprintf(str,"%s",flags2);
						if(uinput(WIN_L2R|WIN_MID|WIN_SAV,0,0,"Flag Set 2",str
							,26,K_EDIT|K_UPPER|K_ALPHA)<0)
							break;
						if(str[0] && add()>0) sprintf(flags2,"-%s",str);
						else
							sprintf(flags2,"%s",str);
                        break;
					case 3:
helpbuf=
" Flag Set 3 \r\n\r\n"
"These are the different flags to be added to (or removed from) the\r\n"
"user account.";
						p=strstr(flags3,"-");
						if(p!=NULL) sprintf(str,"%s",p+1);
						else
							sprintf(str,"%s",flags3);
						if(uinput(WIN_L2R|WIN_MID|WIN_SAV,0,0,"Flag Set 3",str
							,26,K_EDIT|K_UPPER|K_ALPHA)<0)
							break;
						if(str[0] && add()>0) sprintf(flags3,"-%s",str);
						else
							sprintf(flags3,"%s",str);
                        break;
					case 4:
helpbuf=
" Flag Set 4 \r\n\r\n"
"These are the different flags to be added to (or removed from) the\r\n"
"user account.";
						p=strstr(flags4,"-");
						if(p!=NULL) sprintf(str,"%s",p+1);
						else
							sprintf(str,"%s",flags4);
						if(uinput(WIN_L2R|WIN_MID|WIN_SAV,0,0,"Flag Set 4",str
							,26,K_EDIT|K_UPPER|K_ALPHA)<0)
							break;
						if(str[0] && add()>0) sprintf(flags4,"-%s",str);
						else
							sprintf(flags4,"%s",str);
                        break;
					case 5:
helpbuf=
" Exemptions \r\n\r\n"
"These are the exemption flags that will be added to (or removed from)\r\n"
"the user account.";
						p=strstr(exempt,"-");
						if(p!=NULL) sprintf(str,"%s",p+1);
						else
							sprintf(str,"%s",exempt);
						if(uinput(WIN_MID|WIN_SAV,0,0,"Exemptions",str
							,26,K_EDIT|K_UPPER|K_ALPHA)<0)
							break;
						if(str[0] && add()>0) sprintf(exempt,"-%s",str);
						else
							sprintf(exempt,"%s",str);
                        break;
					case 6:
helpbuf=
" Restrictions \r\n\r\n"
"These are the restriction flags that will be added to (or removed\r\n"
"from) the user account.";
						p=strstr(restrict,"-");
						if(p!=NULL) sprintf(str,"%s",p+1);
						else
							sprintf(str,"%s",restrict);
						if(uinput(WIN_MID|WIN_SAV,0,0,"Restrictions",str
							,26,K_EDIT|K_UPPER|K_ALPHA)<0)
							break;
						if(str[0] && add()>0) sprintf(restrict,"-%s",str);
						else
							sprintf(restrict,"%s",str);
                        break;
					case 7:
helpbuf=
" Days to Extend Expiration \r\n\r\n"
"This is the number of days to extend the expiration date of a user\r\n"
"account.";
						uinput(WIN_MID|WIN_SAV,0,0,"Expiration"
							,expiration,8,K_EDIT|K_UPPER|K_NUMBER);
                        break;
					case 8:
helpbuf=
" Credits to Add \r\n\r\n"
"This is the number of credits to add to a user account.";
						uinput(WIN_MID|WIN_SAV,0,0,"Credits",credits
							,10,K_EDIT|K_UPPER|K_NUMBER);
                        break;
					case 9:
helpbuf=
" Minutes to Add \r\n\r\n"
"This is the number of minutes to add to a user account.";
						uinput(WIN_MID|WIN_SAV,0,0,"Minutes",minutes
							,3,K_EDIT|K_UPPER|K_NUMBER);
						break; } }
            break;
		case 2:
helpbuf=
" Allowed Prefix List \r\n\r\n"
"Any phone numbers entered whose prefix matches a number contained\r\n"
"in this list, will not be affected by long distance restrictions.\r\n"
"For example, if you are in the 714 code, and you wish to SCB to\r\n"
"consider all prefixes in the 310 area code local, you would enter 1310\r\n"
"in the list.";

			total_prfxs=0;
			if((prfx=(char **)MALLOC(sizeof(char *)*MAX_PRFX))==NULL) {
				cputs("memory allocation error\r\n");
				bail(1); }
			for(i=0;i<MAX_PRFX;i++)
				if((prfx[i]=(char *)MALLOC(LEN_PRFX+1))==NULL) {
					cputs("memory allocation error\r\n");
					bail(1); }

			if(fexist("ALLOWED.DAT")) {
				if((file=open("ALLOWED.DAT",
					O_RDONLY|O_BINARY|O_DENYNONE))==-1) {
					textattr(LIGHTGRAY);
					clrscr();
					lprintf("Error opening ALLOWED.DAT\r\n");
					bail(1); }
				if((stream=fdopen(file,"rb"))==NULL) {
					textattr(LIGHTGRAY);
					clrscr();
					lprintf("Error fdopen ALLOWED.DAT\r\n");
					bail(1); }
				while(!feof(stream) && total_prfxs<MAX_PRFX) {
					if(!fgets(str,81,stream))
						break;
					truncsp(str);
					sprintf(prfx[total_prfxs++],"%.*s",LEN_PRFX,str); }
				fclose(stream); }
			i=b=0;
            while(1) {
				for(j=0;j<total_prfxs;j++)
					strcpy(opt[j],prfx[j]);
                opt[j][0]=NULL;
				j=WIN_ACT|WIN_SAV;
				if(total_prfxs)
                    j|=WIN_DEL;
				if(total_prfxs<MAX_PRFX)
					j|=WIN_INS|WIN_INSACT|WIN_XTR;
				i=ulist(j,0,0,30,&i,&b,"Allowed Prefix List",opt);
                if((i&0xf000)==MSK_DEL) {
                    i&=0xfff;
					total_prfxs--;
					for(j=i;j<total_prfxs;j++)
						strcpy(prfx[j],prfx[j+1]);
					strcpy(prfx[j+1],"");
                    continue; }
                if((i&0xf000)==MSK_INS) {
                    i&=0xfff;
					if(uinput(WIN_MID|WIN_SAV,0,0,"Enter Prefix",str,LEN_PRFX
                        ,K_NUMBER)<1)
                        continue;
					++total_prfxs;
					if(total_prfxs)
						for(j=total_prfxs;j>i;j--)
							strcpy(prfx[j],prfx[j-1]);
					strcpy(prfx[i],str);
                    continue; }
                if(i==-1) {
                    if((file=open("ALLOWED.DAT",
						O_WRONLY|O_BINARY|O_CREAT|O_TRUNC|O_DENYALL,
						S_IREAD|S_IWRITE))==-1) {
						textattr(LIGHTGRAY);
						clrscr();
                        lprintf("Error opening ALLOWED.DAT\r\n");
                        bail(1); }
                    if((stream=fdopen(file,"wb"))==NULL) {
						textattr(LIGHTGRAY);
						clrscr();
                        lprintf("Error fdopen ALLOWED.DAT\r\n");
                        bail(1); }
					for(i=0;i<total_prfxs;i++) {
						fprintf(stream,"%s\r\n",prfx[i]); }
                    fclose(stream);
					for(i=0;i<MAX_PRFX;i++)
						FREE(prfx[i]);
					FREE(prfx);
                    break; }
                /* else CR was hit */
                uinput(WIN_MID|WIN_SAV,0,0,"Edit Prefix"
					,prfx[i],LEN_PRFX,K_EDIT|K_NUMBER); }
            break;
		case 3:
helpbuf=
" Long Distance Prefix List \r\n\r\n"
"Any phone numbers entered whose prefixes matches a number contained\r\n"
"in this list, will be considered a long distance number.  For example,\r\n"
"in the same area code, the phone number 234-5678 is long distance to you,\r\n"
"so placing 234 in this list will make SCB view calls to that prefix as\r\n"
"long distance.\r\n"
"\r\n"
"If the Long Distance if not an Allowed Prefix toggle option is set to\r\n"
"Yes, then this list is not used.\r\n";

			total_prfxs=0;
			if((prfx=(char **)MALLOC(sizeof(char *)*MAX_PRFX))==NULL) {
				cputs("memory allocation error\r\n");
				bail(1); }
			for(i=0;i<MAX_PRFX;i++)
				if((prfx[i]=(char *)MALLOC(LEN_PRFX+1))==NULL) {
					cputs("memory allocation error\r\n");
					bail(1); }

			if(fexist("LDPREFIX.DAT")) {
				if((file=open("LDPREFIX.DAT",
					O_RDONLY|O_BINARY|O_DENYNONE))==-1) {
					textattr(LIGHTGRAY);
					clrscr();
					lprintf("Error opening LDPREFIX.DAT\r\n");
					bail(1); }
				if((stream=fdopen(file,"rb"))==NULL) {
					textattr(LIGHTGRAY);
					clrscr();
					lprintf("Error fdopen LDPREFIX.DAT\r\n");
					bail(1); }
				while(!feof(stream) && total_prfxs<MAX_PRFX) {
					if(!fgets(str,81,stream))
						break;
					truncsp(str);
					sprintf(prfx[total_prfxs++],"%.*s",LEN_PRFX,str); }
				fclose(stream); }
			i=b=0;
            while(1) {
                for(j=0;j<total_prfxs;j++)
                    strcpy(opt[j],prfx[j]);
                opt[j][0]=NULL;
				j=WIN_ACT|WIN_SAV;
                if(total_prfxs)
                    j|=WIN_DEL;
                if(total_prfxs<MAX_PRFX)
                    j|=WIN_INS|WIN_INSACT|WIN_XTR;
				i=ulist(j,0,0,30,&i,&b,"Long Distance Prefix List",opt);
                if((i&0xf000)==MSK_DEL) {
                    i&=0xfff;
                    total_prfxs--;
                    for(j=i;j<total_prfxs;j++)
                        strcpy(prfx[j],prfx[j+1]);
                    strcpy(prfx[j+1],"");
                    continue; }
                if((i&0xf000)==MSK_INS) {
                    i&=0xfff;
                    if(uinput(WIN_MID|WIN_SAV,0,0,"Enter Prefix",str,LEN_PRFX
                        ,K_NUMBER)<1)
                        continue;
                    ++total_prfxs;
                    if(total_prfxs)
                        for(j=total_prfxs;j>i;j--)
                            strcpy(prfx[j],prfx[j-1]);
                    strcpy(prfx[i],str);
                    continue; }
                if(i==-1) {
					if((file=open("LDPREFIX.DAT",
						O_WRONLY|O_BINARY|O_CREAT|O_TRUNC|O_DENYALL,
						S_IREAD|S_IWRITE))==-1) {
						textattr(LIGHTGRAY);
						clrscr();
						lprintf("Error opening LDPREFIX.DAT\r\n");
                        bail(1); }
                    if((stream=fdopen(file,"wb"))==NULL) {
						textattr(LIGHTGRAY);
						clrscr();
						lprintf("Error fdopen LDPREFIX.DAT\r\n");
                        bail(1); }
                    for(i=0;i<total_prfxs;i++) {
                        fprintf(stream,"%s\r\n",prfx[i]); }
                    fclose(stream);
                    for(i=0;i<MAX_PRFX;i++)
                        FREE(prfx[i]);
                    FREE(prfx);
                    break; }
                /* else CR was hit */
                uinput(WIN_MID|WIN_SAV,0,0,"Edit Prefix"
                    ,prfx[i],LEN_PRFX,K_EDIT|K_NUMBER); }
            break;
		case 4:
helpbuf=
" Long Distance Calling Times \r\n\r\n"
"You are being prompted to specify the beginning and ending times to\r\n"
"allow long distance verification calls (in military 24-hour format).\r\n"
"If you wish to allow long distance calls continuously, set both times\r\n"
"to 00:00.\r\n"
"\r\n"
"To disable long distance calls entirely, set the Allow Long Distance\r\n"
"toggle options to No.";

            j=0;
            while(1) {
                k=0;
				sprintf(opt[k++],"Sunday     From %02d:%02d to %02d:%02d"
					,ldstart[0]/60,ldstart[0]%60
					,ldend[0]/60,ldend[0]%60);
				sprintf(opt[k++],"Monday     From %02d:%02d to %02d:%02d"
					,ldstart[1]/60,ldstart[1]%60
					,ldend[1]/60,ldend[1]%60);
				sprintf(opt[k++],"Tuesday    From %02d:%02d to %02d:%02d"
					,ldstart[2]/60,ldstart[2]%60
					,ldend[2]/60,ldend[2]%60);
				sprintf(opt[k++],"Wednesday  From %02d:%02d to %02d:%02d"
					,ldstart[3]/60,ldstart[3]%60
					,ldend[3]/60,ldend[3]%60);
				sprintf(opt[k++],"Thursday   From %02d:%02d to %02d:%02d"
					,ldstart[4]/60,ldstart[4]%60
					,ldend[4]/60,ldend[4]%60);
				sprintf(opt[k++],"Friday     From %02d:%02d to %02d:%02d"
					,ldstart[5]/60,ldstart[5]%60
					,ldend[5]/60,ldend[5]%60);
				sprintf(opt[k++],"Saturday   From %02d:%02d to %02d:%02d"
					,ldstart[6]/60,ldstart[6]%60
					,ldend[6]/60,ldend[6]%60);
                opt[k][0]=NULL;

				j=ulist(WIN_ACT|WIN_SAV,0,0,0,&j,0
					,"Long Distance Calling Times",opt);
                if(j==-1)
                    break;
				sprintf(str,"%02d:%02d",ldstart[j]/60,ldstart[j]%60);
				if(uinput(WIN_MID|WIN_SAV,0,0,"Long Distance Start "
					"Time",str,5,K_EDIT|K_UPPER)) {
					ldstart[j]=atoi(str)*60;
					p=strchr(str,':');
					if(p)
						ldstart[j]+=atoi(p+1); }
				sprintf(str,"%02d:%02d",ldend[j]/60,ldend[j]%60);
				if(uinput(WIN_MID|WIN_SAV,0,0,"Long Distance End Time",
					str,5,K_EDIT|K_UPPER)) {
					ldend[j]=atoi(str)*60;
					p=strchr(str,':');
					if(p)
						ldend[j]+=atoi(p+1); } }
			break;
		case 5:
helpbuf=
" Phone Can \r\n\r\n"
"This is the complete path (drive, directory, and filename) where a\r\n"
"trashcan file of phone numbers will be kept. Any numbers inside of this\r\n"
"trashcan will not be accepted for callback verification by SCB.";
			uinput(WIN_MID,0,0,"Phone Can",canfile
				,30,K_EDIT|K_UPPER);
            break;
		case 6:
helpbuf=
" Validated Phone List \r\n\r\n"
"This is the complete path (drive, directory, and filename) where SCB\r\n"
"will write a list of numbers that have been verified.\r\n"
"\r\n"
"Setting this option to the same file as Phone Can will keep users from\r\n"
"being validated with an already validated phone number.";
			uinput(WIN_MID|WIN_BOT,0,0,"Phone List",addfile
				,30,K_EDIT|K_UPPER);
			break;
		case 7:
helpbuf=
" Callback Attempts \r\n\r\n"
"This is the number of times SCB will attempt to dial out to a phone\r\n"
"number.";
			uinput(WIN_MID|WIN_BOT,0,0,"Callback Attempts",callout_attempts
				,2,K_EDIT|K_NUMBER);
            break;
		case 8:
helpbuf=
" Minimum Phone Number Length \r\n\r\n"
"This is the minimum number of digits SCB will allow for a phone number.";
			sprintf(str,"%u",min_phone_len);
			uinput(WIN_MID|WIN_BOT,0,0,"Minimum Phone Length",str
				,2,K_EDIT|K_NUMBER);
			min_phone_len=atoi(str);
            break;
		case 9:
helpbuf=
" Maximum Phone Number Length \r\n\r\n"
"This is the maximum number of digits SCB will allow for a phone number.";
			sprintf(str,"%u",max_phone_len);
			uinput(WIN_MID|WIN_BOT,0,0,"Maximum Phone Length",str
				,2,K_EDIT|K_NUMBER);
			max_phone_len=atoi(str);
            break;
		case 10:
helpbuf=
" Wait Before Dialing \r\n\r\n"
"When the BBS hangs up prior to calling a user, this is the amount of\r\n"
"time (in seconds) that the modem will remain hung up before dialing.\r\n"
"This is used to insure that the user has completely disconnected.\r\n"
"The default setting here is 30 seconds, maximum is 90 seconds.";
			sprintf(str,"%u",hangup_time);
			uinput(WIN_MID|WIN_BOT,0,0,"Wait Before Dialing (in seconds)",str
				,3,K_EDIT|K_NUMBER);
			hangup_time=atoi(str);
			if(hangup_time>90)
				hangup_time=90;
			break;
		case 11:
helpbuf=
" BBS Area Code \r\n\r\n"
"Enter the area code for YOUR BBS here.  A user enters a phone number\r\n"
"with this area code, SCB will strip off the area code before attempting\r\n"
"to call the user back (setting this to 0 disables it).";
            uinput(WIN_MID|WIN_BOT,0,0,"BBS Area Code",bbs_ac
                ,3,K_EDIT|K_NUMBER);
            break;
		case 12:
helpbuf=
" Send Message to Sysop \r\n\r\n"
"If you wish for SCB to send a message to a Sysop when a user has been\r\n"
"verified, set this to the number of the user who is to receive the\r\n"
"messages, or set it to 0 to disable this function.";
			uinput(WIN_MID|WIN_BOT,0,0,"Send Message to Sysop - User #",sysop
				,4,K_EDIT|K_NUMBER);
            break;

		case -1:
helpbuf=
" Save Configuration File \r\n\r\n"
"Select Yes to save the config file, No to quit without saving,\r\n"
"or hit  ESC  to go back to the menu.\r\n\r\n";
			i=0;
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			i=ulist(WIN_MID,0,0,0,&i,0,"Save Config File",opt);
			if(i==-1) break;
			if(i) bail(0);
			if((file=open("SCB.CFG"
				,O_WRONLY|O_BINARY|O_CREAT|O_DENYALL|O_TRUNC))==-1) {
				textattr(LIGHTGRAY);
				clrscr();
				lprintf("Error opening SCB.CFG\r\n");
				bail(1); }
			if((stream=fdopen(file,"wb"))==NULL) {
				textattr(LIGHTGRAY);
				clrscr();
				lprintf("Error fdopen SCB.CFG\r\n");
				bail(1); }
			str[0]=0;
			fprintf(stream,"%s\r\n",callout_attempts);
			if(options&ALWAYS_VALIDATE) strcat(str,"Y"); else strcat(str,"N");
			if(options&MODIFY_USER_NOTE) strcat(str,"Y"); else strcat(str,"N");
			if(options&START_WITH_0) strcat(str,"Y"); else strcat(str,"N");
			if(options&START_WITH_1) strcat(str,"Y"); else strcat(str,"N");
			if(options&STAY_CONNECTED) strcat(str,"Y"); else strcat(str,"N");
			if(options&SC_LOCAL_ONLY) strcat(str,"Y"); else strcat(str,"N");
			if(options&US_PHONE_FORMAT) strcat(str,"Y"); else strcat(str,"N");
			if(options&ALLOWED_ONLY) strcat(str,"Y"); else strcat(str,"N");
			if(options&SAME_AREA_LD) strcat(str,"Y"); else strcat(str,"N");
			fprintf(stream,"%s\r\n",str);
			fprintf(stream,"%s\r\n",canfile);
			fprintf(stream,"%s\r\n",addfile);
			fprintf(stream,"%s\r\n",credits);
			fprintf(stream,"%s\r\n",sysop);
			fprintf(stream,"%s\r\n",level);
			fprintf(stream,"%s\r\n",flags1);
			fprintf(stream,"%s\r\n",flags2);
			fprintf(stream,"%s\r\n",exempt);
			fprintf(stream,"%s\r\n",restrict);
			fprintf(stream,"%s\r\n",expiration);
			fprintf(stream,"%s\r\n",minutes);
			fprintf(stream,"%s\r\n",flags3);
			fprintf(stream,"%s\r\n",flags4);
			for(i=0;i<7;i++) {
				fprintf(stream,"%u\r\n",ldstart[i]);
				fprintf(stream,"%u\r\n",ldend[i]); }
			fprintf(stream,"%u\r\n",min_phone_len);
			fprintf(stream,"%u\r\n",max_phone_len);
			fprintf(stream,"%s\r\n",bbs_ac);
			fprintf(stream,"%u\r\n",hangup_time);
			fprintf(stream,"%s\r\n","");    // regnum
			fclose(stream);
			bail(0);
	}
}
}
/****************************************************************************/
/* Checks the disk drive for the existance of a file. Returns 1 if it 		*/
/* exists, 0 if it doesn't.													*/
/* Called from upload														*/
/****************************************************************************/
char fexist(char *filespec)
{
	struct ffblk f;

if(findfirst(filespec,&f,0)==NULL)
	return(1);
return(0);
}
