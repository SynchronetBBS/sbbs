/* SMMCFG.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include <uifc.h>
#include <sys\stat.h>
#include "gen_defs.h"
#include "smmdefs.h"
#include "smmvars.c"

char **opt;

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

void main()
{
	char str[256];
	int i,j,k,file,dflt,sysop_level;
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
sprintf(str,"Synchronet Match Maker v%s",SMM_VER);
uscrn(str);

if((file=open("SMM.CFG",O_RDONLY|O_BINARY|O_DENYNONE))==-1) {
	textattr(LIGHTGRAY);
	clrscr();
	lprintf("Error opening SMM.CFG\r\n");
	bail(1); }
if((stream=fdopen(file,"rb"))==NULL) {
	textattr(LIGHTGRAY);
	clrscr();
	lprintf("Error fdopen SMM.CFG\r\n");
	bail(1); }

str[0]=0;
fgets(str,128,stream);
purity_age=atoi(str);
str[0]=0;
fgets(str,128,stream);
min_age=atoi(str);
str[0]=0;
fgets(str,128,stream);
min_level=atoi(str);

req_flags1[0]=0;
fgets(req_flags1,128,stream);
req_flags1[27]=0;
truncsp(req_flags1);

req_flags2[0]=0;
fgets(req_flags2,128,stream);
req_flags2[27]=0;
truncsp(req_flags2);

req_flags3[0]=0;
fgets(req_flags3,128,stream);
req_flags3[27]=0;
truncsp(req_flags3);

req_flags4[0]=0;
fgets(req_flags4,128,stream);
req_flags4[27]=0;
truncsp(req_flags4);

str[0]=0;
fgets(str,128,stream);
profile_cdt=atol(str);
str[0]=0;
fgets(str,128,stream);
telegram_cdt=atol(str);
str[0]=0;
fgets(str,128,stream);
auto_update=atoi(str);
str[0]=0;
fgets(str,128,stream);
notify_user=atoi(str);

str[0]=0;
fgets(str,128,stream);	// regnum

str[0]=0;
fgets(str,128,stream);
telegram_level=atoi(str);

str[0]=0;
fgets(str,128,stream);
que_level=atoi(str);

str[0]=0;
fgets(str,128,stream);
wall_level=atoi(str);

str[0]=0;
fgets(str,128,stream);
wall_cdt=atol(str);

str[0]=0;
fgets(str,128,stream);
que_cdt=atol(str);

zmodem_send[0]=0;
fgets(zmodem_send,128,stream);
if(!zmodem_send[0])
	strcpy(zmodem_send,DEFAULT_ZMODEM_SEND);
truncsp(zmodem_send);

str[0]=0;
fgets(str,128,stream);
smm_misc=atol(str);

str[0]=0;
fgets(str,128,stream);
sprintf(system_name,"%.25s",str);
truncsp(system_name);

local_view[0]=0;
fgets(local_view,128,stream);
truncsp(local_view);

str[0]=0;
fgets(str,128,stream);
sysop_level=atoi(str);
if(!sysop_level)
	sysop_level=90;

str[0]=0;
fgets(str,128,stream);
wall_age=atoi(str);

str[0]=0;
fgets(str,128,stream);
age_split=atoi(str);

fclose(stream);


dflt=0;
while(1) {
	helpbuf=
" Synchronet Match Maker Configuration \r\n\r\n"
"Move through the various options using the arrow keys.  Select the\r\n"
"highlighted options by pressing ENTER.\r\n\r\n";
	j=0;
	sprintf(opt[j++],"%-40.40s %s","System Name",system_name);
	sprintf(opt[j++],"Wall Security...");
	sprintf(opt[j++],"Profile Database Security...");
	sprintf(str,"Credit %s for Adding Profile"
		,profile_cdt>0 ? "Bonus":"Cost");
	sprintf(opt[j++],"%-40.40s %ldk",str
		,profile_cdt>0 ? profile_cdt/1024L : (-profile_cdt)/1024L);
	sprintf(str,"Credit %s for Sending Telegram"
		,telegram_cdt>0 ? "Bonus":"Cost");
	sprintf(opt[j++],"%-40.40s %ldk",str
		,telegram_cdt>0 ? telegram_cdt/1024L : (-telegram_cdt)/1024L);
	sprintf(str,"Credit %s for Writing on the Wall"
		,wall_cdt>0 ? "Bonus":"Cost");
	sprintf(opt[j++],"%-40.40s %ldk",str
		,wall_cdt>0 ? wall_cdt/1024L : (-wall_cdt)/1024L);
	sprintf(str,"Credit %s for Reading Questionnaire"
		,que_cdt>0 ? "Bonus":"Cost");
	sprintf(opt[j++],"%-40.40s %ldk",str
		,que_cdt>0 ? que_cdt/1024L : (-que_cdt)/1024L);
	sprintf(opt[j++],"%-40.40s %u","Minimum Level to Send Telegrams"
		,telegram_level);
	sprintf(opt[j++],"%-40.40s %u","Minimum Level to Read Questionnaires"
		,que_level);
	sprintf(opt[j++],"%-40.40s %s","Minor Segregation (Protection) Age"
		,age_split ? itoa(age_split,str,10):"Disabled");
	sprintf(opt[j++],"%-40.40s %u","Sysop Level",sysop_level);
	sprintf(opt[j++],"%-40.40s %s","Auto-Update Profiles"
		,auto_update ? itoa(auto_update,str,10):"Disabled");
	sprintf(opt[j++],"%-40.40s %s","Notify User of Activity"
		,notify_user ? itoa(notify_user,str,10):"Disabled");
	sprintf(opt[j++],"%-40.40s %s","Use Metric System"
		,smm_misc&SMM_METRIC ? "Yes":"No");
	sprintf(opt[j++],"%-40.40s %.25s","Zmodem Send Command"
		,zmodem_send);
	sprintf(opt[j++],"%-40.40s %.25s","Local Photo Viewer"
		,local_view);
	opt[j][0]=NULL;
	switch(ulist(WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC,0,0,60,&dflt,0
	,"Synchronet Match Maker Configuration",opt)) {
		case 0:
helpbuf=
" System Name \r\n\r\n"
"This is your BBS name. Once you have configured your BBS name here, you\r\n"
"will not be able to change it, without losing all of your local users'\r\n"
"profiles in your database.\r\n"
"\r\n"
"It is highly recommended that you do not change your BBS name here,\r\n"
"even if you decide to change your actual BBS name in the future.\r\n"
"\r\n"
"All BBS names in a match maker network must be unique.";
			uinput(WIN_MID,0,0,"System Name"
				,system_name,40,K_EDIT|K_UPPER);
            break;
		case 1:
			j=0;
			while(1) {
				k=0;
				sprintf(opt[k++],"%-40.40s  %u"
					,"Minimum User Age to Access Wall"
					,wall_age);
				sprintf(opt[k++],"%-40.40s  %u"
					,"Minimum Security Level to Write on Wall"
					,wall_level);
				opt[k][0]=NULL;
helpbuf=
" Wall Security \r\n\r\n"
"This menu allows you to specify which users can access the wall.\r\n";

				j=ulist(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,0,0,0,&j,0
					,"Wall Security",opt);
				if(j==-1)
					break;
				switch(j) {
					case 0:
helpbuf=
" Minimum User Age \r\n\r\n"
"This is the minimum user age that will be allowed to access (read or\r\n"
"write) the wall.";
						sprintf(str,"%u",wall_age);
						uinput(WIN_L2R|WIN_SAV,0,0,
							"Minimum Age to Access Wall"
							,str,2,K_EDIT|K_NUMBER);
						wall_age=atoi(str);
                        break;
					case 1:
helpbuf=
" Minimum User Level \r\n\r\n"
"This is the minimum user level required to write on the wall.\r\n";
						sprintf(str,"%u",wall_level);
						uinput(WIN_L2R|WIN_SAV,0,0,
							"Minimum Level to Write on Wall"
							,str,2,K_EDIT|K_NUMBER);
						wall_level=atoi(str);
						break; } }
			break;
		case 2:
			j=0;
			while(1) {
				k=0;
				sprintf(opt[k++],"%-40.40s  %u"
					,"Minimum User Age to Add Profile"
					,min_age);
				sprintf(opt[k++],"%-40.40s  %u"
					,"Minimum User Age to Take Purity Test"
					,purity_age);
				sprintf(opt[k++],"%-40.40s  %u"
					,"Minimum Security Level to Add Profile"
					,min_level);
				sprintf(opt[k++],"%-40.40s  %s"
					,"Required Flags (Set 1) to Add Profile"
					,req_flags1);
				sprintf(opt[k++],"%-40.40s  %s"
					,"Required Flags (Set 2) to Add Profile"
					,req_flags2);
				sprintf(opt[k++],"%-40.40s  %s"
					,"Required Flags (Set 3) to Add Profile"
					,req_flags3);
				sprintf(opt[k++],"%-40.40s  %s"
					,"Required Flags (Set 4) to Add Profile"
					,req_flags4);
				opt[k][0]=NULL;
helpbuf=
" Profile Security \r\n\r\n"
"This menu allows you to specify which users can create profiles\r\n"
"and take the purity test.";

				j=ulist(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,0,0,0,&j,0
					,"Profile Database Security",opt);
				if(j==-1)
					break;
				switch(j) {
					case 0:
helpbuf=
" Minimum User Age \r\n\r\n"
"This is the minimum user age that will be allowed to add a profile to\r\n"
"the database.";
						sprintf(str,"%u",min_age);
						uinput(WIN_L2R|WIN_SAV,0,0,
							"Minimum Age",str,2,K_EDIT|K_NUMBER);
						min_age=atoi(str);
						break;
					case 1:
helpbuf=
" Minimum User Age for Purity Test\r\n\r\n"
"This is the minimum user age that will be allowed to take the purity\r\n"
"test.";
						sprintf(str,"%u",purity_age);
						uinput(WIN_L2R|WIN_SAV,0,0,
							"Minimum Age for Purity Test"
							,str,2,K_EDIT|K_NUMBER);
						purity_age=atoi(str);
						break;
					case 2:
helpbuf=
" Minimum Security Level \r\n\r\n"
"This is the minimum security level that is required to add a profile\r\n"
"to the database.";
						sprintf(str,"%u",min_level);
						uinput(WIN_L2R|WIN_SAV,0,0,
							"Security Level",str,3,K_EDIT|K_NUMBER);
						min_level=atoi(str);
                        break;
					case 3:
helpbuf=
" Required Flags from Flag Set 1 \r\n\r\n"
"These are the flags that a required for the user to add a profile\r\n"
"to the database.";
						uinput(WIN_L2R|WIN_SAV,0,0,"Flag Set 1"
							,req_flags1
							,26,K_EDIT|K_UPPER|K_ALPHA);
                        break;
					case 4:
helpbuf=
" Required Flags from Flag Set 2 \r\n\r\n"
"These are the flags that a required for the user to add a profile\r\n"
"to the database.";
						uinput(WIN_L2R|WIN_SAV,0,0,"Flag Set 2"
							,req_flags2
							,26,K_EDIT|K_UPPER|K_ALPHA);
                        break;
					case 5:
helpbuf=
" Required Flags from Flag Set 3 \r\n\r\n"
"These are the flags that a required for the user to add a profile\r\n"
"to the database.";
						uinput(WIN_L2R|WIN_SAV,0,0,"Flag Set 3"
							,req_flags3
							,26,K_EDIT|K_UPPER|K_ALPHA);
                        break;
					case 6:
helpbuf=
" Required Flags from Flag Set 4 \r\n\r\n"
"These are the flags that a required for the user to add a profile\r\n"
"to the database.";
						uinput(WIN_L2R|WIN_SAV,0,0,"Flag Set 4"
							,req_flags4
							,26,K_EDIT|K_UPPER|K_ALPHA);
						break; } }
            break;
		case 3:
helpbuf=
" Credit Adjustment for Adding Profile \r\n\r\n"
"You can have Synchronet Match Maker either give credits to or take\r\n"
"credits from the user for adding a profile to the database.";
			strcpy(opt[0],"Add Credits");
			strcpy(opt[1],"Remove Credits");
			opt[2][0]=0;
			i=1;
			i=ulist(WIN_L2R|WIN_BOT|WIN_ACT,0,0,0,&i,0
				,"Credit Adjustment for Adding Profile",opt);
			if(i==-1)
				break;
			sprintf(str,"%ld",profile_cdt<0 ? -profile_cdt:profile_cdt);
			uinput(WIN_MID,0,0,"Credits (K=1024)"
				,str,10,K_EDIT|K_UPPER);
			if(strchr(str,'K'))
				profile_cdt=atol(str)*1024L;
			else
				profile_cdt=atol(str);
			if(i==1)
				profile_cdt=-profile_cdt;
			break;
		case 4:
helpbuf=
" Credit Adjustment for Sending Telegram \r\n\r\n"
"You can have Synchronet Match Maker either give credits to or take\r\n"
"credits from the user for sending a telegram to another user in SMM.";
			strcpy(opt[0],"Add Credits");
			strcpy(opt[1],"Remove Credits");
			opt[2][0]=0;
			i=1;
			i=ulist(WIN_L2R|WIN_BOT|WIN_ACT,0,0,0,&i,0
				,"Credit Adjustment for Sending Telegram",opt);
			if(i==-1)
				break;
			sprintf(str,"%ld",telegram_cdt<0 ? -telegram_cdt:telegram_cdt);
			uinput(WIN_MID,0,0,"Credits (K=1024)"
				,str,10,K_EDIT|K_UPPER);
			if(strchr(str,'K'))
				telegram_cdt=atol(str)*1024L;
			else
				telegram_cdt=atol(str);
			if(i==1)
				telegram_cdt=-telegram_cdt;
            break;
		case 5:
helpbuf=
" Credit Adjustment for Writing on the Wall \r\n\r\n"
"You can have Synchronet Match Maker either give credits to or take\r\n"
"credits from the user for writing on the wall.";
			strcpy(opt[0],"Add Credits");
			strcpy(opt[1],"Remove Credits");
			opt[2][0]=0;
			i=1;
			i=ulist(WIN_L2R|WIN_BOT|WIN_ACT,0,0,0,&i,0
				,"Credit Adjustment for Writing on the Wall",opt);
			if(i==-1)
				break;
			sprintf(str,"%ld",wall_cdt<0 ? -wall_cdt:wall_cdt);
			uinput(WIN_MID,0,0,"Credits (K=1024)"
				,str,10,K_EDIT|K_UPPER);
			if(strchr(str,'K'))
				wall_cdt=atol(str)*1024L;
			else
				wall_cdt=atol(str);
			if(i==1)
				wall_cdt=-wall_cdt;
            break;
		case 6:
helpbuf=
" Credit Adjustment for Reading Questionnaire \r\n\r\n"
"You can have Synchronet Match Maker either give credits to or take\r\n"
"credits from the user when reading another user's questionnaire";
			strcpy(opt[0],"Add Credits");
			strcpy(opt[1],"Remove Credits");
			opt[2][0]=0;
			i=1;
			i=ulist(WIN_L2R|WIN_BOT|WIN_ACT,0,0,0,&i,0
				,"Credit Adjustment for Reading Questionnaire",opt);
			if(i==-1)
				break;
			sprintf(str,"%ld",que_cdt<0 ? -que_cdt:que_cdt);
			uinput(WIN_MID,0,0,"Credits (K=1024)"
				,str,10,K_EDIT|K_UPPER);
			if(strchr(str,'K'))
				que_cdt=atol(str)*1024L;
			else
				que_cdt=atol(str);
			if(i==1)
				que_cdt=-que_cdt;
            break;
		case 7:
helpbuf=
" Minimum Level to Send Telegrams \r\n\r\n"
"Use this option to restrict the sending of telegrams to users of a\r\n"
"specific security level or higher.";
			sprintf(str,"%u",telegram_level);
			uinput(WIN_MID,0,0,"Minimum Level to Send Telegrams"
				,str,3,K_EDIT|K_NUMBER);
			telegram_level=atoi(str);
            break;
		case 8:
helpbuf=
" Minimum Level to Read Questionnaires \r\n\r\n"
"Users will only be allowed to read other users' questionnaires if\r\n"
"their security level is this value or higher.";
			sprintf(str,"%u",que_level);
			uinput(WIN_MID,0,0,"Minimum Level to Read Questionnaires"
				,str,3,K_EDIT|K_NUMBER);
			que_level=atoi(str);
            break;
		case 9:
helpbuf=
" Minor Segregation (Protection) Age \r\n\r\n"
"This option (if enabled) separates all users into two groups:\r\n"
"\r\n"
"Minors: Those users below the specified age (normally 18)\r\n"
"Adults: Those users at or above the specified age\r\n"
"\r\n"
"If enabled, adults cannot see minors' profiles or send telegrams to\r\n"
"minors and vice versa.\r\n"
"\r\n"
"If disabled, all users can see eachother's profiles regardless of age.\r\n";
			sprintf(str,"%u",age_split);
			uinput(WIN_MID,0,0,"Minor Segregation Age (0=Disabled)"
				,str,2,K_EDIT|K_NUMBER);
			age_split=atoi(str);
			break;
		case 10:
helpbuf=
" Sysop Level \r\n\r\n"
"Every user of this level or higher will be given access to sysop\r\n"
"commands in the match maker.";
			sprintf(str,"%u",sysop_level);
			uinput(WIN_MID,0,0,"Sysop Level",str,3,K_EDIT|K_NUMBER);
			sysop_level=atoi(str);
            break;

		case 11:
helpbuf=
" Auto-Update Profiles \r\n\r\n"
"If you would like to have Synchronet Match Maker automatically send\r\n"
"a network update message for a user that is active in SMM, but hasn't\r\n"
"actually made any changes to his or her profile, set this option to the\r\n"
"number of days between Auto-Updates (e.g. 30 days is a good value).\r\n"
"\r\n"
"Setting this option to 0 disables this feature.";
			sprintf(str,"%u",auto_update);
			uinput(WIN_MID,0,0,"Auto-Update Profiles (in Days)"
				,str,3,K_EDIT|K_NUMBER);
			auto_update=atoi(str);
			break;
		case 12:
helpbuf=
" Notify User of Activity \r\n\r\n"
"If you would like to have Synchronet Match Maker automatically send\r\n"
"a message to a specific user (most commonly the sysop) whenever a user\r\n"
"adds a profile to the database or sends a telegram from SMM, set this\r\n"
"option to the number of that user (e.g. 1 would indicate user #1).\r\n"
"\r\n"
"Setting this option to 0 disables this feature.";
			sprintf(str,"%u",notify_user);
			uinput(WIN_MID,0,0,"Notify User Number (0=disabled)"
				,str,5,K_EDIT|K_NUMBER);
			notify_user=atoi(str);
            break;
		case 13:
helpbuf=
" Use Metric System \r\n\r\n"
"If you wish to use centimeters and kilograms instead of inches and\r\n"
"pounds for height and weight measurements, set this option to Yes.";
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			i=1;
			i=ulist(WIN_MID|WIN_ACT,0,0,0,&i,0
				,"Use Metric Measurement System",opt);
			if(i==-1)
                break;
			if(i==1)
				smm_misc&=~SMM_METRIC;
			else
				smm_misc|=SMM_METRIC;
			break;

		case 14:
helpbuf=
" Zmodem Send Command \r\n\r\n"
"This is the command line to execute to send files to remote user.\r\n";
			uinput(WIN_MID,0,0,"Zmodem Send"
				,zmodem_send,50,K_EDIT);
            break;
		case 15:
helpbuf=
" Local Photo Viewer \r\n\r\n"
"This is the command line to execute to view photos when logged on\r\n"
"locally.\r\n";
			uinput(WIN_MID,0,0,"Local Viewer"
				,local_view,50,K_EDIT);
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
			if((file=open("SMM.CFG"
				,O_WRONLY|O_BINARY|O_CREAT|O_DENYALL|O_TRUNC,S_IWRITE))==-1) {
				textattr(LIGHTGRAY);
				clrscr();
				lprintf("Error opening SMM.CFG\r\n");
				bail(1); }
			if((stream=fdopen(file,"wb"))==NULL) {
				textattr(LIGHTGRAY);
				clrscr();
				lprintf("Error fdopen SMM.CFG\r\n");
				bail(1); }
			fprintf(stream,"%u\r\n%u\r\n%u\r\n%s\r\n%s\r\n%s\r\n%s\r\n"
				,purity_age,min_age,min_level
				,req_flags1,req_flags2,req_flags3,req_flags4);
			fprintf(stream,"%ld\r\n%ld\r\n%u\r\n%u\r\n%s\r\n%u\r\n%u\r\n"
				,profile_cdt,telegram_cdt,auto_update,notify_user,""
				,telegram_level,que_level);
			fprintf(stream,"%u\r\n%ld\r\n%ld\r\n%s\r\n%ld\r\n%s\r\n"
				,wall_level,wall_cdt,que_cdt,zmodem_send,smm_misc,system_name);
			fprintf(stream,"%s\r\n%u\r\n%u\r\n%u\r\n"
				,local_view,sysop_level,wall_age,age_split);
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
