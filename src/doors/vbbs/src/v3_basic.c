
#include <stdarg.h>

#include "vbbs.h"
#include "v3_defs.h"
#include "v3_basic.h"
#include "v3_io.h"
#include "v3_mci.h"

#if !defined(_MSC_VER) && !defined(__BORLANDC__)
void strupr(char *text)
{
	unsigned int i;
	for(i=0;i<strlen(text);i++)
		if(text[i]>='a' && text[i]<='z')
			text[i]-=32;
}

void strlwr(char *text)
{
	unsigned int i;
	for(i=0;i<strlen(text);i++)
		if(text[i]>='A' && text[i]<='Z')
			text[i]+=32;
}
#endif /* !_MSC_VER && !__BORLANDC__ */

void nl()
{
	mci("\r\n");
}

void paws()
{
	text("0080");
	od_get_key(TRUE);
	mci("\b\b\b\b\b\b\b`0F");
}

int yesno()
{
	char ch;
	int x;

	text("0081"); /* [Y] */
	ch = od_get_answer("YN\n\r");
	if ((ch == 'Y') || (ch == '\n') || (ch=='\r'))
	{
		x = TRUE;
		text("0083"); /* Yes */
	}
	else
	{
		x = FALSE;
		text("0084"); /* No */
	}

	return x;
}

int noyes()
{
	char ch;
	int x;

	text("0082"); /* [N] */
	ch = od_get_answer("YN\n\r");
	if ((ch == 'N') || (ch == '\n') || (ch == '\r'))
	{
		x = TRUE;
		text("0084"); /* No */
	}
	else
	{
		x = FALSE;
		text("0083"); /* Yes */
	}

	return x;
}

int printfile(char *fname)
{
	FILE *fptr;
	int done=FALSE,cnt=0,x=FALSE;
	char ttxt[161];

	fptr = fopen(fname,"rt");
	if (fptr!=NULL)
 	{
		while(fgets(ttxt,161,fptr)!=NULL && !done)
		{
			mci(ttxt);
			cnt++;
			if(cnt>=20)
			{
				mci("~SMMore? ");
				if (yesno())
					cnt=0;
				else
					done=TRUE;
			}
		}
		x = TRUE;
	}

	fclose(fptr);
	return(x);
}

void center(char *text)
{
	int i;


	for(i=0;i<(40-(mcistrlen(text)/2));i++)
		printf(" ");
	mci(text);
}

char moreyn()
{
	char ch;
	int i, s = 13;

	mci("~SMMore? [Y]: ");
	ch = od_get_answer("YN\r\n");

	for(i=0;i<s;i++)
	{
		printf("\b \b");
	}

	if ((ch == '\n') || (ch == '\r'))
	{
		ch = 'Y';
	}

	return ch;
}

int mcistrset(char *mcstr, int chop)
{
	int i,x;

	i=strlen(mcstr);
	if(mcistrlen(mcstr)>chop){
		for(x=0;x<chop;x++){
			if(mcstr[x]=='`'){
				chop+=3;
				}
			}
		if(x<i)	mcstr[x]='\0';
		}
	else{
		for(x=mcistrlen(mcstr);x<chop;x++){
			strcat(mcstr," ");
			}
		}
	return 0;
}

int mcistrrset(char *mcstr, int chop)
{
	int i,x;
	char t1[191];

	i=strlen(mcstr);
	if(mcistrlen(mcstr)>chop){
		for(x=0;x<chop;x++){
			if(mcstr[x]=='`'){
				chop+=3;
				}
			}
		if(x<i)	mcstr[x]='\0';
		}
	else{
		*t1='\0';
		for(x=mcistrlen(mcstr);x<chop;x++){
			strcat(t1," ");
			}
		strcat(t1,mcstr);
		strcpy(mcstr,t1);
		}
	return 0;
}

int mcistrcset(char *mcstr, int chop)
{
	int i,x,num,num1;
	char t1[191];

	i=strlen(mcstr);
	if(mcistrlen(mcstr)>chop){
		for(x=0;x<chop;x++){
			if(mcstr[x]=='`'){
				chop+=3;
				}
			}
		if(x<i)	mcstr[x]='\0';
		}
	else{
		num=chop-mcistrlen(mcstr);  /*  How many total blanks */
		num1=(num/2);  /* How many blanks in front */
		*t1='\0';
		for(x=0;x<num1;x++){
			strcat(t1," ");
			}
		strcat(t1,mcstr);
		for(x=num1;x<num;x++){   /* How many blanks in back */
			strcat(t1," ");
			}
		strcpy(mcstr,t1);
		}
	return 0;
}

void new_user()
{
	memset(&plyr, 0, sizeof(tUserRecord));
	plyr.status = USED;
	plyr.plyrnum = plyrcnt;
	strcpy(plyr.realname,od_control.user_name);
	strcpy(plyr.bbsname,od_control.user_name);
	strcat(plyr.bbsname,"s BBS");
	plyr.funds = 100;
	plyr.pacbell_installed = 1;
	plyr.mspeed.low = 1;
	plyr.actions = CONFIG_ACTIONS_PER_DAY;
 	time(&plyr.laston);
}

void act_change(int idx, int val, int opflag)
{
	switch(idx)
	{
		case ACT_NOTHING_HAPPENS:
            break;
		case ACT_ADD_FREE_USERS:
			inum = (s16)(rand_num((plyr.skill_lev+1)*6));
			s16_2A(1, inum);
			if (inum>0)
			{
				plyr.users_f += inum;
				if(opflag)
					text("0850");
			}
			upskore(inum);
			break;
		case ACT_ADD_PAY_USERS:
			nl();
			inum = (s16)(rand_num((s16)((plyr.users_f+1)/5)));
			s16_2A(1,inum);
			if (inum>0)
			{
				plyr.users_f -= inum;
				/* users_f is unsigned
				if (plyr.users_f<0) plyr.users_f=0; */
				plyr.users_p += inum;
				if (opflag) text("0851");
			}
			upskore(inum);
			break;
		case ACT_SUB_FREE_USERS:
			nl();
			inum = (s16)(rand_num((s16)((plyr.users_f+1)/10)));
			s16_2A(1,inum);
			if (inum>0)
			{
				plyr.users_f -= inum;
				/* users_f is unsigned
				if (plyr.users_f<0)	plyr.users_f=0; */
				if (opflag) text("0852");
			}
			downskore(inum);
			break;
		case ACT_SUB_PAY_USERS:
			inum = (s16)(rand_num((s16)((plyr.users_p+1)/10)));
			s16_2A(1,inum);
			if (inum>0)
			{
				plyr.users_p -= inum;
				/* users_p is unsigned
				if (plyr.users_p<0)	plyr.users_p=0; */
				if (opflag) text("0853");
			}
            downskore(2);
			break;
		case ACT_ADD_FUNDS:
			/* add the funds to the account */
			plyr.funds += val;
			/* display gains */
			if (opflag)
			{
				s32_2C(1, (s32)val);
				s32_2C(2, (s32)plyr.funds);
				text("0854");
			}
			/* recalc score */
			upskore(1);
			break;
		case ACT_SUB_FUNDS:
			plyr.funds -= val;
			if (plyr.funds < 0)
				plyr.funds = 0;
            downskore(1);
			break;
		case ACT_UPGRADE_CPU:
			if(plyr.cpu<MAX_CPU){
				plyr.cpu++;
				str_2A(1,computer[plyr.cpu].cpuname);
				if(opflag)
					text("0855");
				upskore(plyr.cpu);
				}
			else
				if(opflag)
					text("0863");
			break;
		case ACT_ADD_ACTIONS:
			plyr.actions+=val;
            upskore(val/2);
			break;
		case ACT_FREE_2_PAY:
			inum = (s16)(rand_num((s16)((plyr.users_f+1)/5)));
			s16_2A(1,inum);
			if (inum>0)
			{
				plyr.users_f -= inum;
				/* users_f is unsigned
				if(plyr.users_f<0)	plyr.users_f=0; */
				plyr.users_p += inum;
				if (opflag) text("0856");
			}
			upskore(inum*(plyr.skill_lev+1));
			break;
		case ACT_UPGRADE_HD:
			if(plyr.hd_size<MAX_HD){
				plyr.hd_size++;
				str_2A(1,harddrive[plyr.hd_size].hdsize);
				if(opflag)	text("0857");
				upskore(plyr.hd_size);
				}
			else
				if(opflag)	text("0863");
			break;
		case ACT_DOWNGRADE_CPU:
			downskore(plyr.cpu);
			plyr.cpu--;
			str_2A(1,computer[plyr.cpu].cpuname);
			if(plyr.cpu<0)	plyr.cpu=0;
			if(plyr.cpu<1)	text("0858");
			else			text("0859");
			break;
		case ACT_DOWNGRADE_HD:
			downskore(plyr.hd_size);
			plyr.hd_size--;
			str_2A(1,harddrive[plyr.hd_size].hdsize);
			if(plyr.hd_size<0)	plyr.hd_size=0;
			if(plyr.hd_size<1)	text("0860");
			else				text("0861");
			break;
		case ACT_DOWNGRADE_MODEM:
			if(plyr.mspeed.low)	plyr.mspeed.low--;
			else	if(plyr.mspeed.high)	plyr.mspeed.high--;
					else	if(plyr.mspeed.digital)	plyr.mspeed.digital--;

			downskore(2);
			break;
		case ACT_PHONELINE_DAMAGE:
			if (plyr.pacbell_installed > 0)
			{
				plyr.pacbell_installed--;
				plyr.pacbell_broke++;
			}
			break;
		}
}

void downskore(int sk)
{
	if (plyr.skore_now > (uint)sk)
		plyr.skore_now -= sk;
	else
		plyr.skore_now = 0;
}

void upskore(int sk)
{
	plyr.skore_now += (s32)sk;
}

void fillvars()
{
	strcpy(harddrive[0].hdsize,"30meg 60ms");
	strcpy(harddrive[1].hdsize,"100meg 60ms");
	strcpy(harddrive[2].hdsize,"300meg 16ms");
	strcpy(harddrive[3].hdsize,"1.2 Gigabytes 11ms");
	strcpy(harddrive[4].hdsize,"6 GB 3ms");
	strcpy(harddrive[5].hdsize,"24 GB 3ms");
	strcpy(harddrive[6].hdsize,"40 GB Autoraid");
	strcpy(harddrive[7].hdsize,"60 GB Autoraid");
	strcpy(harddrive[8].hdsize,"120 GB Autoraid");
	strcpy(harddrive[9].hdsize,"300 GB Autoraid");
	strcpy(harddrive[10].hdsize,"500 GB Emc2");
	strcpy(harddrive[11].hdsize,"750 GB Emc2");
	strcpy(harddrive[12].hdsize,"1.0 Terrabytes  Emc2");
	strcpy(harddrive[13].hdsize,"5.0 TB  Emc2");
	strcpy(harddrive[14].hdsize,"25.0 TB  Emc2");
	strcpy(harddrive[15].hdsize,"75.0 TB  Emc2");

	harddrive[0].cost  = 500L;
	harddrive[1].cost  = 1000L;
	harddrive[2].cost  = 2000L;
	harddrive[3].cost  = 2000L;
	harddrive[4].cost  = 6000L;
	harddrive[5].cost  = 8000L;
	harddrive[6].cost  = 16000L;
	harddrive[7].cost  = 32000L;
	harddrive[8].cost  = 64000L;
	harddrive[9].cost  = 128000L;
	harddrive[10].cost = 512000L;
	harddrive[11].cost = 1024000L;
	harddrive[12].cost = 2048000L;
	harddrive[13].cost = 4096000L;
	harddrive[14].cost = 8192000L;
	harddrive[15].cost = 16000000L;

	harddrive[0].req=0;
	harddrive[1].req=0;
	harddrive[2].req=1;
	harddrive[3].req=1;
	harddrive[4].req=2;
	harddrive[5].req=3;
	harddrive[6].req=3;
	harddrive[7].req=4;
	harddrive[8].req=5;
	harddrive[9].req=6;
	harddrive[10].req=7;
	harddrive[11].req=8;
	harddrive[12].req=9;
	harddrive[13].req=10;
	harddrive[14].req=11;
	harddrive[15].req=12;

	strcpy(computer[0].cpuname,"8MHz XT clone with 512k RAM");
	strcpy(computer[1].cpuname,"12MHz 286 with 1Meg ");
	strcpy(computer[2].cpuname,"25MHz 386 with 4Meg ");
	strcpy(computer[3].cpuname,"50MHz 486 with 8Meg ");
	strcpy(computer[4].cpuname,"99MHz 586 with 16Meg");
	strcpy(computer[5].cpuname,"166MHz Pentium with 32Meg");
	strcpy(computer[6].cpuname,"320MHz PII with 64Meg");
	strcpy(computer[7].cpuname,"512MHz PIII with 128Meg");
	strcpy(computer[8].cpuname,"Mini v1");
	strcpy(computer[9].cpuname,"Mini v11");
	strcpy(computer[10].cpuname,"Midrange 9000");
	strcpy(computer[11].cpuname,"2 Node Cluster (Midrange) ");
	strcpy(computer[12].cpuname,"8 Node Cluster (Midrange) ");
	strcpy(computer[13].cpuname,"Enterprise 10000");
	strcpy(computer[14].cpuname,"2 Node Cluster (Enterprise)");
	strcpy(computer[15].cpuname,"8 Node Cluster (Enterprise)");

	computer[0].cost  = 0L;
	computer[1].cost  = 1000L;
	computer[2].cost  = 2000L;
	computer[3].cost  = 4000L;
	computer[4].cost  = 8000L;
	computer[5].cost  = 16000L;
	computer[6].cost  = 32000L;
	computer[7].cost  = 64000L;
	computer[8].cost  = 128000L;
	computer[9].cost  = 256000L;
	computer[10].cost = 512000L;
	computer[11].cost = 1024000L;
	computer[12].cost = 2048000L;
	computer[13].cost = 4096000L;
	computer[14].cost = 8192000L;
	computer[15].cost = 16284000L;

	strcpy(mtype[0].modemname,"Cheap 2400");
	strcpy(mtype[1].modemname,"Good 2400");
	strcpy(mtype[2].modemname,"2400 v.42bis");
	strcpy(mtype[3].modemname,"9600");
	strcpy(mtype[4].modemname,"9600 v.32");
	strcpy(mtype[5].modemname,"9600ds");
	strcpy(mtype[6].modemname,"14.4kbps v.32bis");
	strcpy(mtype[7].modemname,"19.2kbps v.90");
	strcpy(mtype[8].modemname,"28.0kbps v.90");
	strcpy(mtype[9].modemname,"56kbps");

	mtype[0].cost = 80L;
	mtype[1].cost = 160L;
	mtype[2].cost = 320L;
	mtype[3].cost = 640L;
	mtype[4].cost = 1280L;
	mtype[5].cost = 5120L;
	mtype[6].cost = 10240L;
	mtype[7].cost = 20480L;
	mtype[8].cost = 40960L;
	mtype[9].cost = 81960L;

	mtype[0].req=0;
	mtype[1].req=0;
	mtype[2].req=0;
	mtype[3].req=1;
	mtype[4].req=3;
	mtype[5].req=5;
	mtype[6].req=7;
	mtype[7].req=9;
	mtype[8].req=11;
	mtype[9].req=13;

	strcpy(bbssw[0].bbsname,"Renegard");
	strcpy(bbssw[1].bbsname,"WWIII");
	strcpy(bbssw[2].bbsname,"RemoteEntry");
	strcpy(bbssw[3].bbsname,"WildeKat!");
	strcpy(bbssw[4].bbsname,"Excalibur");
	strcpy(bbssw[5].bbsname,"Majorr BBS");
	strcpy(bbssw[6].bbsname,"TSX-Online");
	strcpy(bbssw[7].bbsname,"Mindwire");
	strcpy(bbssw[8].bbsname,"Compu$erver");
	strcpy(bbssw[9].bbsname,"Another On-Line");

	bbssw[0].cost = 50L;
	bbssw[1].cost = 300L;
	bbssw[2].cost = 1800L;
	bbssw[3].cost = 6000L;
	bbssw[4].cost = 36000L;
	bbssw[5].cost = 180000L;
	bbssw[6].cost = 600000L;
	bbssw[7].cost = 3600000L;
	bbssw[8].cost = 18000000L;
	bbssw[9].cost = 60000000L;

	bbssw[0].req=0;
	bbssw[1].req=0;
	bbssw[2].req=1;
	bbssw[3].req=2;
	bbssw[4].req=5;
	bbssw[5].req=7;
	bbssw[6].req=9;
	bbssw[7].req=11;
	bbssw[8].req=13;
	bbssw[9].req=15;

	bbssw[0].numlines = 1L;
	bbssw[1].numlines = 2L;
	bbssw[2].numlines = 16L;
	bbssw[3].numlines = 32L;
	bbssw[4].numlines = 64L;
	bbssw[5].numlines = 256L;
	bbssw[6].numlines = 1024L;
	bbssw[7].numlines = 8192L;
	bbssw[8].numlines = 51200L;
	bbssw[9].numlines = 100000L;

	strcpy(skill_txt[0],"Absolute Beginner");
	strcpy(skill_txt[1],"Beginner");
	strcpy(skill_txt[2],"Novice");
	strcpy(skill_txt[3],"Average");
	strcpy(skill_txt[4],"Above Average");
	strcpy(skill_txt[5],"Expert");
	strcpy(skill_txt[6],"Expert Master");
	strcpy(skill_txt[7],"Guru");
	strcpy(skill_txt[8],"Fountain of Modem Knowledge");
	strcpy(skill_txt[9],"Sysop");

	scant[0]=30;
	scant[1]=60;
	scant[2]=90;
	scant[3]=120;
	scant[4]=240;
	scant[5]=300;

	CONFIG_ACTIONS_PER_DAY=100;
}
