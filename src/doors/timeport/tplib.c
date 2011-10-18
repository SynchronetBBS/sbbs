#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "doors.h"
#include "tplib.h"

// DIM SHARED ch1$, ch2$, ch3$, ch4$, ch5$, Ch6$, Ch7$, Ch8$, Ch9$, ChRest$
char ch1[256];
char ch2[256];
char ch3[256];
char ch4[256];
char ch5[256];
char Ch6[256];
char Ch7[256];
char Ch8[256];
char Ch9[256];
char ChRest[256];
// DIM SHARED Ch10$, Ch11$, Ch12$, Ch13$, Ch14$, Ch15$, Ch16$, ch17$, Ch18$
char Ch10[256];
char Ch11[256];
char Ch12[256];
char Ch13[256];
char Ch14[256];
char Ch15[256];
char Ch16[256];
char ch17[256];
char Ch18[256];
// DIM SHARED RecLen AS INTEGER, DatPath$, BarBar$, PlayerNum AS INTEGER
short int RecLen=240;
char DatPath[MAXPATHLEN];
char *BarBar="`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n";
short int PlayerNum;
// DIM SHARED Today$, Yesterday$, Regis AS INTEGER, FirstAvail AS INTEGER
char Today[MAXPATHLEN];
char Yesterday[MAXPATHLEN];
short int Regis, FirstAvail;
// DIM SHARED RegisterTo1$, RegisterTo2$, MaxClosFight$, MaxPlayFight$
char RegisterTo1[256];
char RegisterTo2[256];
short MaxClosFight;
short MaxPlayFight;
// DIM SHARED RecordNumber AS INTEGER, OriginNumber AS INTEGER, Version$
short RecordNumber, OriginNumber;
char *Version="1.2a Beta";
// DIM SHARED Quor$, ItemCount AS INTEGER, Def$, User$, Alias$
// char Quor[256]; /* Not used */
char Def[256];
char User[256];
char Alias[256];
short ItemCount;
// DIM SHARED into(10, 2) AS LONG, ItemName$(10) 'stored to read
long int into[11][3];
char ItemName[11][21];
// DIM SHARED Invent$(8), inv(8, 2) AS LONG 'access to inventory
char Invent[9][256];
long int inv[9][3];
// DIM SHARED MaxBadTimes AS INTEGER, KickOut AS INTEGER
short MaxBadTimes=5;
short KickOut;

// Extras I need
char Prompt[128];

// Stuff common to all (May as well be in here eh?)
short Hit;
short Level;
char Bad[256];
char NotAgain[256];
char Weapon;
long Money;
char Defense;
long Stash;
long Exp;
char lastplay[9];
short ClosFight;
short PlayFight;
char LineRest[256];
char ExeLine[256];
char ComLine[256];
short PlayKill;
char sex[16];
char CurCode[256];

char *DefName[13]={"Nothing", "Metal Shield", "Fractal Gloves", "Metalplast Shield",
				"Fractal Boots", "Metalplast Vest", "Energy Helmet", "Laser Shield",
				"Photon Shield", "Heat Absorber", "DiamondMatter Shield",
				"Mind Shielder", "Ion Armor"};

char *WepName[13]={"Fists", "Pistol","Homing Grenade", "Cyber Arm", "Body Blast",
				"Sonic Blaster", "Spacial Wave Rifle", "Neural Phaser", "Photon Ray", "Laser Cannon",
				"Heat Disrupter", "Destruction Wand", "Ion Saber"};

long WepCost[13]={0,50,500,4000,11000,30000,100000,200000,400000,800000,3600000,8000000,20000000};
long PlayerHP[13]={0, 20, 30, 40, 60, 80, 100, 120, 140, 200, 300, 450, 700};
long WeaponAdd[13]={0, 5, 10, 20, 30, 40, 60, 80, 120, 180, 250, 350, 500};
long DefenseAdd[13]={0, 1, 3, 9, 15, 25, 35, 50, 75, 100, 150, 225, 300};
long MonMinHp[13]={0, 10, 20, 60, 160, 260, 400, 600, 850, 1050, 2000, 3000, 4500};
long GoldFromEach[13]={0, 15, 100, 400, 950, 2000, 6000, 12000, 25000, 75000, 200000, 400000, 600000};
long ExpFromEach[13]={0, 10, 75, 200, 425, 1000,3500,6200,11500,20000,43000,85000,120000};
long UpToNext[12]={0, 300, 3000, 10000, 25000, 60000, 180000, 400000, 800000, 1500000, 3000000, 6000000};
long HealAHit[13]={0, 2, 3, 4, 6, 8, 12, 16, 20, 25, 30, 35, 40};
char *Rank[13]={"","New Recruit","Recruit","Partial Adept","Full Adept","Sergeant",
				"Commander","General","Time Sergeant","Time Commander",
				"Time General","Time Master","Time Lord"};

int Playercount;
int MaximumPlayers;
char guardname[256];
char BribeGuard[256];

FILE *SharedOpen(const char *filename, const char *inmode, int lockop)  {
	FILE *file;
	FILE *file2;
	int trunc=0;
	char mode[8];

	strcpy(mode,inmode);
	if(mode[0]=='w')  {
		strcpy(mode,"r+");
		trunc=1;
	}
	file=fopen(filename,mode);
	if(file == NULL)
	{
		if(trunc)
		{
			strcpy(mode,inmode);
			file=fopen(filename,mode);
			trunc=0;
		}
		if(file==NULL)
		{
			fprintf(stderr, "Unable to open %s for read!  %s (%d)\n", filename, strerror(errno), errno);
			return(NULL);
		}
	}
	if(flock(fileno(file),lockop))
	{
		fprintf(stderr, "Unable to lock %s!  %s (%d)\n", filename, strerror(errno), errno);
		fprintf(stderr, "FileNo: %d  LockOp: %d\n",fileno(file), lockop);
		fclose(file);
		return(NULL);
	}
	if(trunc)
	{
		strcpy(mode,inmode);
		file2=fopen(filename,mode);
		flock(fileno(file),lockop);
		fclose(file);
		file=file2;
	}
	return(file);
}

void AddToMail(short x, char *e)
{
	char FilePath[MAXPATHLEN];
	FILE	*file;

	if (x > 999)
		x = 999;
	if (x < 1)
		x = 1;
	sprintf(FilePath,"%smail.%d", DatPath, x);
	file=SharedOpen(FilePath, "a", LOCK_EX);
	if(file != NULL)
	{
		fprintf(file,"%s\r\n",e);
		fclose(file);
	}
}

void DelOldFromNameList(void)
{
	short c=0;
	char NameListFN[MAXPATHLEN];
	char NewListFN[MAXPATHLEN];
	FILE *NameList;
	FILE *NewList;
	char b[256];
	char te[16];
	char *user;
	char *alias;
	char str[256];
	
	sprintf(NameListFN, "%snamelist.dat", DatPath);
	sprintf(NewListFN, "%snewlist.dat", DatPath);
	NameList=SharedOpen(NameListFN,"r",LOCK_EX);
	if(NameList == NULL)
		return;
	NewList=SharedOpen(NewListFN,"w",LOCK_EX);
	if(NewList == NULL)
	{
		fclose(NameList);
		return;
	}
	while(!feof(NameList))
	{
		readline(b, sizeof(b), NameList);
		strcpy(str,b);
	    c++;
	    user = strtok(str, "~");
		if(user!=NULL)
		{
			alias=strtok(NULL,"~");
			if(alias != NULL)
			{
	    		strncpy(te,alias,15);
				RTrim(te);
		    	OpenStats(te);
		    	if(ch1[0])
				{
					fprintf(NewList,"%s\r\n",b);
		    	}
			}
		}
	}
	rename(NewListFN, NameListFN);
	fclose(NameList);
	fclose(NewList);
}

char *FixTheString(char *dest, char *a, char *b, char *c)
{
	char *in;
	char *out;

	in=a;
	out=dest;
	*out=0;
	for(;*in;in++)
	{
		if(*in=='*' && *(in+1)=='1')
		{
			strcat(out,b);
			in++;
		}
		else if(*in=='*' && *(in+1)=='2')
		{
			strcat(out,c);
			in++;
		}
		else
		{
			*out++=*in;
			*out=0;
		}
		out=strchr(dest,0);
	}
	return(dest);
}

char *GetCommand(char *dest)
{
	char pr[256];

	if(strlen(Prompt))
	{
		Def[0] = Prompt[strlen(Prompt)-1];
		Def[1] = 0;
		Prompt[strlen(Prompt)-1]=0;
	}

	Oprint("\r\n");
	sprintf(pr,"`3 Remaining Minutes: `!%ld:%02ld, `3Hits: `!%hd`3 of `!%ld\r\n`%% Awaiting Your Choice: `7"
			,MinsLeft,SecsLeft,Hit,PlayerHP[Level]);
	Oprint(pr);
	sprintf(pr,"`#(`9%s`!%s`5): `$",Prompt,Def);
	strcat(Prompt,Def);
	Oprint(pr);
	ChatEnabled = 1;
	Owait(pr);
	dest[0]=toupper(pr[0]);
	dest[1]=0;
	ChatEnabled = 0;
	Oprint("\r\n");
	if (dest[0]==0 || dest[0]=='\r' || dest[0]=='\n')
	{
		strcpy(dest, Def);
	}
	return(dest);
}

char *GetYesNo(char *dest)
{
	do {
		Owait(dest);
		UCase(dest);
	} while (dest[0] != 'Y' && dest[0] != '\r' && dest[0] != '\n' && dest[0] != 'N');
	if(dest[0] == '\n' || dest[0]=='\r')
		dest[0]='Y';
	Oprint(ret);
	return(dest);
}

void InitPlayer(void)
{
	char e[MAXPATHLEN];
	int i;
	FILE *file;
	time_t currtime;

//	'--- initialize the MAIL.NUM file ---
	sprintf(e,"%smail.%d",DatPath,FirstAvail);
	file=SharedOpen(e,"w",LOCK_EX);
	unlink(e);
	fclose(file);

//	'--- initialize the INVENT.NUM file ---
	sprintf(e,"%sinvent.%d",DatPath,FirstAvail);
	file=SharedOpen(e,"w",LOCK_EX);
	for(i=0;i<8;i++)
	{
		fwrite("------------------- 0        0                                                  ",80,1,file);
	}
	fclose(file);

	strcpy(Bad, "N4");
	strcpy(NotAgain, "                   |");
	Weapon='0';
	Money=150;
	Defense='0';
	Stash=50;
	Hit=PlayerHP[1];
	Level = 1;
	Exp=0;
	currtime=time(NULL);
	strftime(lastplay, 9, "%D", localtime(&currtime));
	NotAgain[5]='1';
	NotAgain[6]='0';
	NotAgain[7]=' ';
	NotAgain[8]=' ';
	NotAgain[10]='0';
	ClosFight=MaxClosFight;
	PlayFight=MaxPlayFight;
	strcpy(LineRest, "                                "
			"                                "
			"                                "
			"                                ");
	LineRest[0]= '2'+random()%3; // 'pole random
	strcpy(ExeLine, " ");
	strcpy(ComLine, " ");
	PlayKill=0;
}

short sip(void)
{
	short c;
	char a[2];

	MakeInputArea(2);
	KbIn(a,1);
	Oprint("`7\r\n");
	c=atoi(a);
	return c;
}

void DisplayInvent(void)
{
	char str[256];
	int x;

	Ocls();
	sprintf(str,"`!T`3ime `!P`3ort `8Version `7%s -- `7Location: `%%Inventory Listing\r\n",Version);
	Oprint(str);
	Oprint(BarBar);
	Oprint("`!                 These are items currently in your inventory.\r\n");
	Oprint(BarBar);
	ItemCount = 8;
	for(x=1;x<=8;x++)
	{
    	strcpy(str, Invent[x]);
	    if(str[0]=='-')
			strcpy(str,"Nothing");
    	strcpy(ItemName[x], str);
	    into[x][1] = inv[x][1];
    	into[x][2] = inv[x][2];
	}

	ListItems(0);
	Oprint(BarBar);
	Oprint("`4      (`#U`4)`@se an item now     ");
	Oprint("`4(`#D`4)`@rop an item     ");
	Oprint("`4(`#R`4)`@eturn to game\r\n");
	strcpy(Prompt, "UDR");
	Oprint(BarBar);
}

void Inventory(void)
{
	long x1, x2, x3, x4, gg1, pp1, b;
	short c,Yilled;
	char str[256];
	char str2[256];

	DisplayInvent();
	while(1)
	{
		GetCommand(str);
		if(str[0]=='R')
			return;
		if(str[0]=='D')
		{
	    	Oprint("\r\n`0  Drop which item? (1..8) ");
	    	c=sip();
	    	if(c >= 1 && c <= 8)
			{
	        	SaveInvItem(c, "-------------------", 0, 0);
				DisplayInvent();
				continue;
	    	}
		}

		if(str[0] == 'U') // 'use the item
		{
	    	Oprint("\r\n`#  USE which item? (1..8) ");
	    	c=sip();
	    	if(c < 1 || c > 8)
			{
	        	Oprint("\r\n`6 You can't use an item other than these listed.\r\n\r\n");
    	    	PressAnyKey();
				DisplayInvent();
				continue;
	    	}
	    	b = inv[c][2];
	    	if(b <= 0)
			{
	        	Oprint("\r\n`6 Nothing is there for you to use.\r\n");
	        	continue;
			}
	    	if(b == 1)
			{
				sprintf(str2,"\r\n`#%ld`5 Morph Credits will transfer to Fred for each healed point.\r\n",HealAHit[Level]);
        		Oprint(str2);
	        	x2 = PlayerHP[Level];  		// 'max hits
        		gg1 = Money;				// 'money
        		pp1 = HealAHit[Level];		// 'cost per
        		x1 = Hit;					// 'current hits
        		x4 = PlayerHP[Level]; 		// 'max hits
        		x3 = x4 - x1;
	        	if(x3 <= 0)
				{
            		Oprint("`5 However, you are at maximum health currently.\r\n\r\n");
            		PressAnyKey();
					DisplayInvent();
					continue;
        		}
        		Yilled = 0;
				x4 = 0;
        		while(gg1 >= pp1 && x1 < x2)
				{
	            	gg1 = gg1 - pp1;
					x4 = x4 + pp1;
    	        	x1 = x1 + 1;
        	    	Yilled = Yilled + 1;
        		}
        		if(Yilled <= 0)
				{
            		Oprint("`5 You couldn't seem to afford a single hit point...\r\n\r\n");
            		PressAnyKey();
					DisplayInvent();
					continue;
        		}
				sprintf(str2,"\r\n\r\n`!%d Hit Points were healed.\r\n\r\n",Yilled);
        		Oprint(str2);
				sprintf(str2,"`3 Fred will receive %ld Morph Credits from your account.\r\n\r\n",x4);
        		Oprint(str2);
        		Money = gg1;
	        	Hit = x1;
        		SaveInvItem(c, "-------------------", 0, 0);
        		PressAnyKey();
				DisplayInvent();
				continue;
    		}
    		Oprint("\r\n");
    		UseItem(c, b); // 'call to a routine MUST be in each module
			Oprint("\r\n");
    		PressAnyKey();
    		if(KickOut == 1)
			{
				KickOut = 0;
				return;
			}
    		KickOut = 0;
			DisplayInvent();
			continue;
		}

		if(str[0] == '?')
		{
			DisplayInvent();
			continue;
		}
		if(str[0] == 0)
			return;
		Oprint("\r\n  `2Choose a valid command, or [`0?`2] to re-list your inventory.\r\n");
		continue;
	}
}

void ListItems(float ee)
{
	int x;
	char str[256];
	
    Oprint("`$                   NUM   Item Name               Buy cost\r\n");
    Oprint(BarBar);
    for(x = 1;x <= ItemCount; x++)
	{
		Oprint("`7                   ");
		sprintf(str,"`7(`!%d`8)`6   ",x);
		Oprint(str);
		Pad(str, ItemName[x], 23);
		Oprint(str);
		sprintf(str,"`6%11ld\r\n",into[x][1]);
		Oprint(str);
	}
	if(ee == 1)
		Oprint("`7                   (`%0`8)`@   None of these (quit)      \r\n");
}

void ListPlayers(void)
{
/*
'Ch1$ = Alias$(15)
'Ch2$ = Money$(8)
'Ch3$ = Stash$(8)
'Ch4$ = Sex$(1)
'Ch5$ = CurCode$(4)
'Ch6$ = NotAgain$(20)
'Ch7$ = Weapon$(1)
'Ch8$ = Defense$(1)
'Ch9$ = Hit$(8)
'Ch10$ = Exp$(8)
'Ch11$ = SkillLevel (2)
'Ch12$ = Date of Last Play (10)
'Ch13$ = Clost Fights Remainint(2)
'Ch14$ = PlayerFights Remaining(1)
'ch15$ = PlayerKills(3)
*/

	// 'first, I need to sort the players based upon their experience
	char AliasName[1000][256];
	long Exper[1000];
	short GoMax;
	char str[256];
	FILE *file;
	char *p;
	short MadeChange;
	short x;
	long l;
	short Pax;
	short Pottr;
	float Bv;
	short xix;

	GoMax = 0;

	sprintf(str,"%snamelist.dat",DatPath);
	file=SharedOpen(str,"r",LOCK_SH);

	while(!feof(file))
	{
		GoMax++;
		readline(str,sizeof(str),file);
		p=strchr(str,'~');
		if(p!=NULL)
		{
			p++;
			for(;*p && isspace(*p);p++);
			strncpy(AliasName[GoMax],p,15);
			AliasName[GoMax][15]=0;
			for(p=AliasName[GoMax]+strlen(AliasName[GoMax])-1;isspace(*p) && p>=AliasName[GoMax];p--)
				*p=0;
			OpenStats(AliasName[GoMax]);
			if(ch1[0])
				sscanf(Ch10,"%ld",&(Exper[GoMax]));
			else
				Exper[GoMax]=0;
		}
	}
	fclose(file);


	if(GoMax > 1)
	{
		puts("");
		do
		{
			MadeChange = 0;
			for(x = GoMax; x>=1; x--)
			{
				if(Exper[x+1] > Exper[x])
				{
					l=Exper[x+1];
					Exper[x+1]=Exper[x];
					Exper[x]=l;
			    	strcpy(str,AliasName[x+1]);
					strcpy(AliasName[x+1],AliasName[x]);
					strcpy(AliasName[x],str);
				    MadeChange = 1;
				}
			}
		} while(MadeChange>0);
	}

	Ocls();
	Pax = 16;
	Pottr = 1;
	Oprint("\r\n");
	Oprint("`2컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴\r\n");
	Oprint("`!                        Time Port: `#List of Recruits\r\n");
	Oprint("`2컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴\r\n");
	Oprint(" Sex  Name            Level   Experience    Alive  Kills  Codes   Wins\r\n");
	Oprint("`2컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴\r\n");

	for(xix = 1; xix<GoMax; xix++)
	{
    	OpenStats(AliasName[xix]);
	    if(ch1[0])
		{
    	    if(Pottr == 0)
			{
        	    Ocls();
				Oprint(" Sex  Name            Level   Experience    Alive  Kills  Codes   Wins\r\n");
				Oprint("`2컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴\r\n");
	        }
    	    // 'show the listing
        	Oprint("`$   ");
	        if(toupper(ch4[0]) == 'F')
				sprintf(str,"%3.3s",ch4);
			else
			{
				str[0]=' ';
				str[1]=' ';
				str[2]=' ';
				str[3]=0;
			}
			Oprint(str);
        	Bv = 19;
			l=strlen(ch1);
	        for(x = 0; x<l; x++)
			{
				if(ch1[x] == '`')
					Bv += 2;
        	}
	        if(Bv < 1)
				Bv = 1;
    	    sprintf(str,"%-*.*s",(int)Bv, (int)Bv, ch1);
			Oprint(str);
			sprintf(str,"`@%-6.6s",Ch11);
			Oprint(str);
			sprintf(str,"%-14.14s",Ch10);
			Oprint(str);
			sprintf(str,"%-10.10s",Ch6[1] == ' '?"`%YES":"`4NO");
			Oprint(str);
			sprintf(str,"`5%-7.7s",Ch15);
			Oprint(str);
			str[0]='`';
			str[1]='2';
			str[2]=Ch6[15]?Ch6[15]:' ';
			str[3]=Ch6[16]?Ch6[16]:' ';
			str[4]=Ch6[17]?Ch6[17]:' ';
			str[5]=' ';
			str[6]=' ';
			str[7]=' ';
			str[8]=' ';
			str[9]=0;
			Oprint(str);
			str[0]='`';
			str[3]=0;
        	if(isdigit(Ch6[10]) && Ch6[10] != '0')
				str[1]='$';
			else
				str[1]= '9';
			str[2]=Ch6[10];
	        Oprint(str);
    	    Pottr++;
			Oprint("\r\n");
	        if(Pottr >= Pax)
			{
				Pottr = 0;
				Pax = 20;
        	    Oprint("\r\n");
            	PressAnyKey();
	        }
		}
	}
	Oprint("\r\n\r\n");
	if(Pottr > 0)
		PressAnyKey();
}

void LoadGame(void)
{
    OpenStats(Alias);
    if(!ch1[0])
		ExitGame();
    strcpy(Bad,"N5");
	OriginNumber = RecordNumber;
	Money=atoi(ch2);		// sscanf(ch2,"%ld",&Money);
	Stash=atoi(ch3);		// sscanf(ch3,"%ld",&Stash);
	sex[0]=toupper(ch4[0]);
	sex[1]=0;
	strcpy(CurCode, ch5);
	strcpy(NotAgain, Ch6);
	Weapon=Ch7[0];		// sscanf(Ch7,"%hd",&Weapon);
	Defense=Ch8[0];		// sscanf(Ch8,"%hd",&Defense);
	Hit=atoi(Ch9);			// sscanf(Ch9,"%hd", &Hit);
	Exp=atoi(Ch10);			// sscanf(Ch10,"%ld", &Exp);
	Level=atoi(Ch11);		// sscanf(Ch11,"%hd",&Level);
	strcpy(lastplay, Ch12);
	ClosFight=atoi(Ch13);	// sscanf(Ch13, "%hd", &ClosFight);
	PlayFight=atoi(Ch14);	// sscanf(Ch14, "%hd", &PlayFight);
	PlayKill=atoi(Ch15);	// sscanf(Ch15, "%hd", &PlayKill);
	strcpy(ExeLine, Ch16);
	strcpy(ComLine, ch17);
	strcpy(LineRest, ChRest);
}

void LoadItems(char *a)
{
	char *x;
	short c=1;
	short b;

	x=a;
	ItemCount = 0;
	while(*x)
	{
	    sscanf(x,"%3hd",&b);
	    while(strlen(x) < 3)
	       strcat(x," ");
	    if(b > 0 && b < 999)
		{
	        ReadItem(b, c);
	        ItemCount = c;
	        c++;
	    }
	    x+=3;
	}
}

void MakeInputArea(short v)
{
	char *buf;

	buf=(char *)malloc(v+1);
	memset(buf,' ',v);
	buf[v]=0;
	SetColor(15, 4, 0);
	Oprint(buf);
	memset(buf,'\b',v);
	Oprint(buf);
}

char *MakeNode(char *dest, char *b)
{
	char *in;
	char *out;
	
	out=dest;
	for(in=b; *in; in++)
	{
		if(*in=='*')
		{
			sprintf(out,"%d",NodeNum);
			out=out+strlen(out)-1;
		}
		else
			*out=*in;
		out++;
	}
	*out=0;
	return(dest);
}

void RTrim(char *str)
{
	char *p;

	if(!*str)
		return;
	for(p=str+strlen(str)-1;(isspace(*p) || !*p) && p>=str; p--)
	{
		*p=0;
	}
}

void OpenInventory(void)
{
	char str[MAXPATHLEN];
	FILE	*file;
	char buf[81];
	int x;
	char midbuf[9];

	sprintf(str,"%sinvent.%d",DatPath,OriginNumber);
	file=SharedOpen(str,"r",LOCK_SH);
	for(x=1;x<=8;x++)
	{
		buf[0]='-';
		fread(buf, 80, 1, file);
		buf[80]=0;
		strncpy(Invent[x],buf,20);
		RTrim(Invent[x]);
		inv[x][1]=atoi(Mid(midbuf,buf,20,8));
		inv[x][2]=atoi(Mid(midbuf,buf,28,3));
	}
	fclose(file);
}

char *Mid(char *dest, char *source, int start, int len)
{
	char *in;
	char *out;
	int x;

	in=source+(start-1);
	out=dest;
	for(x=1;x<=len;x++)
	{
		*(out++)=*(in++);
	}
	*out=0;
	return(dest);
}

void OpenStats(char *a1)
{
/*
'Ch1$ = Alias$(15)
'Ch2$ = Money$(8)
'Ch3$ = Stash$(8)
'Ch4$ = Sex$(1)
'Ch5$ = CurCode$(4)
'Ch6$ = NotAgain$(20)
'Ch7$ = Weapon$(1)
'Ch8$ = Defense$(1)
'Ch9$ = Hit$(8)
'Ch10$ = Exp$(8)
'Ch11$ = SkillLevel(2)
'Ch12$ = Date of Last Play LastPlay$(8)
'Ch13$ = Closet Fights remaining ClosFight$(2)
'Ch14$ = Player fights remaining PlayFight$(1)
'Ch15$ = PlayKill$(3)
'ch16$ = ExeLine$(10)
'ch17$ = ComLine$(8)
'ChRest$ = Reserved end portion of record,
          'used to preserve data from IGM's
*/
	short Found=0;
	short c=0;
	char str[MAXPATHLEN];
	FILE *file;
	char buf[241];
	int x;
	char ach1[16];

	ch1[0]=ch2[0]=ch3[0]=ch4[0]=ch5[0]=Ch6[0]=Ch7[0]=Ch8[0]=Ch9[0]=0;
	Ch10[0]=Ch11[0]=Ch12[0]=Ch13[0]=Ch14[0]=Ch15[0]=Ch16[0]=ch17[0]=ChRest[0]=0;

	sprintf(str,"%splayers.dat",DatPath);
	file=SharedOpen(str,"a",LOCK_EX);
	fclose(file);
	file=SharedOpen(str,"r",LOCK_SH);
	while(!feof(file) && !Found)
	{
	    c++;
		buf[0]=0;
		fread(buf,RecLen,1,file);
		buf[240]=0;
		if(strlen(buf) < RecLen)
		{
			ch1[0]=0;
			FirstAvail = c;
			break;
		}
		strncpy(ach1,buf,15);
		ach1[15]=0;
		RTrim(ach1);
    	if(!strcasecmp(a1,ach1))
		{
			strcpy(ch1,ach1);
        	RTrim(Mid(ch2, buf, 16, 8));
        	RTrim(Mid(ch3, buf, 24, 8));
        	RTrim(Mid(ch4, buf, 32, 1));
        	RTrim(Mid(ch5, buf, 33, 4));
			Mid(Ch6, buf, 37, 20);
			Mid(Ch7, buf, 57, 1);
			Mid(Ch8, buf, 58, 1);
        	RTrim(Mid(Ch9, buf, 59, 8));
        	RTrim(Mid(Ch10, buf, 67, 8));
        	RTrim(Mid(Ch11, buf, 75, 2));
        	RTrim(Mid(Ch12, buf, 77, 10));
        	RTrim(Mid(Ch13, buf, 87, 2));
			Mid(Ch14, buf, 89, 1);
        	RTrim(Mid(Ch15, buf, 90, 3));
        	RTrim(Mid(Ch16, buf, 93, 10));
        	RTrim(Mid(ch17, buf, 103, 8));
        	x = (RecLen - 2) - 110;
        	Mid(ChRest, buf, 111, x);

        	if(atoi(Ch13) <= 0)
			{
				Ch13[0]='0';
				Ch13[1]=0;
			}
			if(atoi(Ch13) >= MaxClosFight)
			{
        		sprintf(Ch13, "%hd", MaxClosFight);
			}
        	if(atoi(Ch14) <= 0)
			{
				Ch14[0]='0';
				Ch14[1]=0;
			}
			if(atoi(Ch14) >= MaxPlayFight)
			{
        		sprintf(Ch14, "%hd", MaxPlayFight);
			}
        	Found = 1;
			RecordNumber = c;
			FirstAvail = c;
		}
		if(*buf<' ' && *buf > -128)
		{
        	ch1[0]=0;
        	Found = 1;
        	FirstAvail = c;
		}
		if(feof(file) && !Found)
		{
			ch1[0]=0;
			FirstAvail = c + 1;
		}
	}
	fclose(file);
}

char *Pad(char *dest, char *a, long l)
{
	char *in;
	char *out;
	long x;
	int pad=0;
	
	in=a;
	out=dest;
	
	for(x=0;x<l;x++)
	{
		if(!*in)
			pad=1;
		if(pad)
			*out++=' ';
		else
			*out++=*in++;
	}
	*out=0;
	return(dest);
}

void ReadConfig(void)
{
	char q[MAXPATHLEN];
	char buf[81];
	FILE *file;
	char code1[128];
	char Code2[128];

	strcpy(Bad, "C1");
	if(NodeNum == 0)
		strcpy(q,"setup.cfg");
	else
		sprintf(q,"setup.%hd", NodeNum);

	file=SharedOpen(q, "r", LOCK_SH);
	buf[0]=0;
	fread(buf, 80, 1, file);
	buf[80]=0;
	strcpy(Today, buf);
	RTrim(Today);
	buf[0]=0;
	fread(buf, 80, 1, file);
	buf[80]=0;
	strcpy(Yesterday, buf);
	RTrim(Yesterday);
	buf[0]=0;
	fread(buf, 80, 1, file);
	buf[80]=0;
	strcpy(q, buf);
	RTrim(q);
	MaxClosFight=atoi(q);
	buf[0]=0;
	fread(buf, 80, 1, file);
	buf[80]=0;
	strcpy(q, buf);
	RTrim(q);
	MaxPlayFight=atoi(q);
	buf[0]=0;
	fread(buf, 80, 1, file);
	buf[80]=0;
	strcpy(DatPath, buf);
	RTrim(DatPath);
	fclose(file);

	sprintf(q,"%stp1.cfg",DatPath);
	file=SharedOpen(q, "r", LOCK_SH);
	readline(code1, sizeof(code1), file);
	RTrim(code1);
	readline(Code2, sizeof(Code2), file);
	RTrim(Code2);
	fclose(file);
	strcpy(RegisterTo1, code1);
	strcpy(RegisterTo2, Code2);
	if(strlen(RegisterTo1)<2 || strlen(RegisterTo2)<2)
	{
		RegisterTo1[0]=0;
		RegisterTo2[0]=0;
		Regis=0;
	}
	else
		Regis=1;

	if(MaxClosFight < 10)
		MaxClosFight = 10;
	if(MaxClosFight > 25)
		MaxClosFight = 25;
	if(MaxPlayFight < 1)
		MaxPlayFight = 1;
	if(MaxPlayFight > 3)
		MaxPlayFight = 3;

	strcpy(Bad,"C2");
}

void ReadItem(int n, int o)
{
	char path[MAXPATHLEN];
	FILE *file;
	char buf[81];
	char midbuf[9];

	sprintf(path,"%sitems.dat",DatPath);
	file=SharedOpen(path, "r", LOCK_SH);
	fseek(file, 80*(n-1), SEEK_SET);
	buf[0]=0;
	fread(buf, 80, 1, file);
	buf[80]=0;
	fclose(file);
	strncpy(ItemName[o],buf,20);
	ItemName[o][20]=0;
	into[o][1]=atoi(Mid(midbuf,buf,20,8));
	into[o][2]=atoi(Mid(midbuf,buf,28,3));
}

char *RealName(char *dest, char *a)
{
	if(a[0]=='-')
		strcpy(dest, "Nothing");
	else
		strcpy(dest, a);
	return(dest);
}

char *RemoveColor(char *dest, char *te)
{
	char *in;
	char *out;
	
	out=dest;
	for(in=te; *in; in++)
	{
		if(*in=='`')
		{
			switch(*in)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				case '!':
				case '@':
				case '#':
				case '$':
				case '%':
					in++;
					break;
				default:
					*out++=*in;
			}
		}
		else
			*out++=*in;
	}
	return(dest);
}

void SaveInvItem(short x, char *c, long b1, long b2)
{
	char path[MAXPATHLEN];
	FILE *file;
	char buf[81];
	
	if(x < 1 || x > 8)
		return;
	sprintf(path,"%sinvent.%d",DatPath,OriginNumber);
	file=SharedOpen(path, "r+", LOCK_EX);
	sprintf(buf, "%-20.20s%-8ld%-3ld%-47.47s  ", c, b1, b2, "");
	fseek(file, 80*(x-1), SEEK_SET);
	fwrite(buf, 80, 1, file);
	fclose(file);
	strcpy(Invent[x],c);
	inv[x][1]=b1;
	inv[x][2]=b2;
}

void SaveStats(char *a0, char *a1, int b1, int c1, char *d1, char *E1, char *f1, char g1, char h1, 
				int i1, int j1, int k1, char *l1, int m1, int n1, int o1, char *p1, char *q1, char *Thr)
/* void SaveStats(char *a0, char *a1, char *b1, char *c1, char *d1, char *E1, char *f1, char *g1, char *h1, 
				char *i1, char *j1, char *k1, char *l1, char *m1, char *n1, char *o1, char *p1, char *q1, char *Thr) */
{
	short Found=0;
	short c=0;
	char path[MAXPATHLEN];
	FILE *file;
	char buf[241];
	char outbuf[241];
	int	Record=0;

	sprintf(path,"%splayers.dat",DatPath);
	file=SharedOpen(path, "r+", LOCK_EX);
    while(!feof(file) && !Found)
	{
		Record++;
	    c++;
		buf[0]=0;
		fread(buf, RecLen, 1, file);
    	if(buf[0] < ' ' && buf[0] > -128)
		{
			// buf[0] = 0;
			strcpy(buf,a1);
			Found = 1;
		}
		buf[15]=0;
		RTrim(buf);
		if(!strcasecmp(buf,a1))
		{
        	Found = 1;
			if(b1>99999999)
				b1=10000000;
			if(b1<-9999999)
				b1=-10000000;
			if(c1>99999999)
				c1=10000000;
			if(c1<-9999999)
				c1=-10000000;
			if(i1>99999999)
				i1=10000000;
			if(i1<-9999999)
				i1=-10000000;
			if(j1>99999999)
				j1=10000000;
			if(j1<-9999999)
				j1=-10000000;
			if(k1>99)
				k1=10;
			if(k1<-9)
				k1=-1;
			if(m1>99)
				m1=10;
			if(m1<-9)
				m1=-1;
			if(n1>9)
				n1=1;
			if(n1<0)
				n1=0;
			if(o1>999)
				o1=100;
			if(o1<-99)
				o1=-10;
			sprintf(buf,"%-15.15s%-8d%-8d%-1.1s%-4.4s%-19.19s|%c%c%-8d%-8d%-2d%-10.10s%-2d%-1d%-3d%-10.10s%-8.8s%-129.129s",
				a0,b1,c1,d1,E1,f1,g1,h1,i1,j1,k1,l1,m1,n1,o1,p1,q1,Thr);
			Pad(outbuf, buf, RecLen);
			fseek(file, (Record-1)*RecLen, SEEK_SET);
			fwrite(outbuf, RecLen, 1, file);
    	}
		if(feof(file))
			buf[0]=0;
	}
	fclose(file);
}

void SaveStrStats(char *a0, char *a1, char *b1, char *c1, char *d1, char *E1, char *f1, char *g1, char *h1, 
				char *i1, char *j1, char *k1, char *l1, char *m1, char *n1, char *o1, char *p1, char *q1, char *Thr)
/* void SaveStats(char *a0, char *a1, char *b1, char *c1, char *d1, char *E1, char *f1, char *g1, char *h1, 
				char *i1, char *j1, char *k1, char *l1, char *m1, char *n1, char *o1, char *p1, char *q1, char *Thr) */
{
	short Found=0;
	short c=0;
	char path[MAXPATHLEN];
	FILE *file;
	char buf[241];
	char outbuf[241];
	int	Record=0;

	sprintf(path,"%splayers.dat",DatPath);
	file=SharedOpen(path, "r+", LOCK_EX);
    while(!feof(file) && !Found)
	{
		Record++;
	    c++;
		buf[0]=0;
		fread(buf, RecLen, 1, file);
    	if(buf[0] < ' ' && buf[0] > -128)
		{
			// buf[0] = 0;
			strcpy(buf,a1);
			Found = 1;
		}
		buf[15]=0;
		RTrim(buf);
		if(!strcasecmp(buf,a1))
		{
        	Found = 1;
			sprintf(buf,"%-15.15s%-8.8s%-8.8s%-1.1s%-4.4s%-19.19s|%-1.1s%-1.1s%-8.8s%-8.8s%-2.2s%-10.10s%-2.2s%-1.1s%-3.3s%-10.10s%-8.8s%-129.129s",
				a0,b1,c1,d1,E1,f1,g1,h1,i1,j1,k1,l1,m1,n1,o1,p1,q1,Thr);
			Pad(outbuf, buf, RecLen);
			fseek(file, (Record-1)*RecLen, SEEK_SET);
			fwrite(outbuf, RecLen, 1, file);
    	}
		if(feof(file))
			buf[0]=0;
	}
	fclose(file);
}

void ViewStats(void)
{
	char str[256];
	short d;

	Ocls();
	Oprint("\r\n");
	Oprint("`3컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴\r\n");
	Oprint(" `!                       Your Statistics and Settings\r\n");
	Oprint("`3컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴\r\n\r\n");
	sprintf(str,"                `3            Your Name: `@%s\r\n",Alias);
	Oprint(str);
	sprintf(str,"                `3             Your Sex: `@%s\r\n",sex[0]=='M'?"Male":"Female");
	Oprint(str);
	sprintf(str,"                `3Morph Credits on Hand: `@%ld\r\n",Money);
	Oprint(str);
	sprintf(str,"                `3      Credits in Bank: `@%ld\r\n",Stash);
	Oprint(str);
	sprintf(str,"                `3      Your Hit Points: `@%d`3 of `@%ld\r\n",Hit,PlayerHP[Level]);
	Oprint(str);
	sprintf(str,"                `3    Experience Points: `@%ld\r\n",Exp);
	Oprint(str);
	sprintf(str,"                `3            Your Rank: `@%s (%d)\r\n",Rank[Level],Level);
	Oprint(str);
	sprintf(str,"                `3      Hours Remaining:`@ %d\r\n", ClosFight);
	Oprint(str);
	sprintf(str,"                `3        Player Fights:`@ %d\r\n", PlayFight);
	Oprint(str);
	d=WepNum(Weapon);
	if(Weapon=='0')
		d=0;
	sprintf(str,"                `3          Your Weapon: `@%s\r\n", WepName[d]);
	Oprint(str);
	d=WepNum(Defense);
	if(Defense=='0')
		d=0;
	sprintf(str,"                `3         Your Defense: `@%s\r\n", DefName[d]);
	Oprint(str);
	if(NotAgain[3] != ' ')
		Oprint("                `3 You have a Heat Bomb!\r\n");
	Oprint("\r\n\r\n");
	PressAnyKey();
}

short WepNum(char a)
{
	if(a < 'A' || a > 'L')
		return(1);
	return(a-'A'+1);
}

void WriteNews(char *e, short x)
{
	FILE *file;
	
	file=SharedOpen(Today,"a",LOCK_EX);
	if(strlen(e) > 0)
		fprintf(file,"%s\r\n",e);
	if(x > 0)
		fprintf(file,"                         `4컴컴컴�`@컴컴컴�`4컴컴컴�\r\n");
	fclose(file);
}

void readline(char *dest, size_t size, FILE *file)
{
	char *p;

	if(fgets(dest,size,file)!=dest)
		dest[0]=0;
	
	for(p=dest;*p;p++)
	{
		if(*p=='\r' || *p=='\n')
		{
			*p=0;
			break;
		}
	}
}

char *IsHeNew(char *dest)
{
	char kr[256];
	char a[256];
	int Found=0;
	char path[MAXPATHLEN];
	char *fuser;
	char *alias;
	FILE *file;
	
	// '**** REMOVE THIS routine for STUB operation! ****
	// CLOSE #1  /* Why would this be open? */
	kr[0]=0;

	Found = 0;

	strcpy(Bad, "N1");

	sprintf(path,"%snamelist.dat", DatPath);
	file=SharedOpen(path,"r", LOCK_SH);

	dest[0]=0;
	while(!feof(file) && !Found)
	{
		readline(a, sizeof(a), file);
		RTrim(a);
		if((fuser=strtok(a,"~"))!=NULL)
		{
			RTrim(fuser);
			alias=strtok(NULL,"~");
			if(alias != NULL)
			{
				if(strcasecmp(User, fuser)==0)
				{
					strcpy(dest, alias);
					Found=1;
				}
			}
		}
	}
	fclose(file);
	return(dest);
}

char *FindName(char *dest, char *Rr)
{
	char path[MAXPATHLEN];
	FILE *file;
	char a[256];
	char Te[256];
	char *i;
	char te[256];
	char str[256];

	sprintf(path,"%snamelist.dat",DatPath);
	file=SharedOpen(path, "r", LOCK_SH);
	while(!feof(file))
	{
		readline(a, sizeof(a), file);
		if(feof(file))
			continue;
    	i=strtok(a, "~");
		if(i==NULL)
		{
			Oprint("Error in the namelist.dat file! Inform Sysop.\r\n");
			Paus(2);
			exit(1);
		}
    	i=strtok(NULL, "~");
		if(i==NULL)
		{
			Oprint("Error in the namelist.dat file! Inform Sysop.\r\n");
			Paus(2);
			exit(1);
		}
		while(isspace(*i))
		{
			i++;
		}
		RTrim(Mid(Te, i, 1, 15));
		RemoveColor(te, Te);
		strcpy(str,Rr);
		UCase(te);
		UCase(str);
		if(strstr(te, str)!=NULL)
		{
        	OpenStats(Te);
        	if(ch1[0] != 0 && !strcasecmp(ch1, Te))  // 'is it him
			{
				sprintf(str,"`7  Do you mean `%%%s`$ (Y/N)`7? `4",Te);
            	Oprint(str);
				GetYesNo(str);
            	if(str[0] == 'Y')
				{
                	PlayerNum = RecordNumber;
                	strcpy(dest,Te);
                	fclose(file);
                	Oprint("\r\n");
                	return(dest);
            	}
        	}
    	}
	}
	Oprint("\r\n");
	dest[0]=0;
	fclose(file);
	return(dest);
}


short BuyStuff(char *v, char *q) //  '*** STUB routine might not always be needed ***
{
	short t=0;
	long d1, D2;
	long w;
	short m;
	int x;
	char str[256];

	w = atoi(v);
	if(w==0)
		t=-99;
	if(w < 0 || w > ItemCount)
	{
    	Oprint("\r\n");
		Oprint(q);
		Oprint("\r\n");
    	t = -2;
	}
	if(w >= 1 && w <= ItemCount)
	{
    	d1 = into[w][1];
    	D2 = Money;
    	if(D2 < d1)
		{
        	Oprint("\r\n`3  --- `!You don't seem to have enough Morph Credits onhand to buy that. `3---\r\n");
        	return(-2);
    	}
    	m = 0;
    	for(x=8;x>=1;x--)
		{
			if(Invent[x][0]=='-' && Invent[x][1]=='-' && Invent[x][2]=='-')
				m=x;
    	}
    	if(m == 0)
		{
        	Oprint("`4 --- `@You're carrying all you can... you need to get rid of something. `4---\r\n");
        	return(-2);
    	}
		sprintf(str,"`@ Will you pay`$ %ld`@ Morph Credits for the `$%s`@ ? (Y/n): ",into[w][1],ItemName[w]);
    	Oprint(str);
    	GetYesNo(str);
    	if(str[0] == 'N')  // 'don't want to pay for THAT!!!
        	return(-2);
    	SaveInvItem(m, ItemName[w], into[w][1], into[w][2]);
    	D2 = D2 - d1;
		Money = D2;
    	Oprint("\r\n`2 --- `0You pay for the item and put it into your inventory. `2---\r\n");
    	t = -2;
	}
	return(t);
}

short FindTheLevel(long g)
{
	int x,t;
	for(x=11;x>=1;x--)
	{
		if(g >= UpToNext[x])
		{
		    t = x + 1;
    		if(t > 5 && Regis < 1)
				t = 5;
			return(t);
		}
	}
	return(1);
}

short GetHitLev(long lx, long ly, float l1, float L2)
{
	// 'Lx is your Attack power, VS.
	// 'Ly is your opponents Defense power
	long Tv1, Tv2, Tv3, Tv4, Tv5;
	Tv1 = lx - ly;
	if(Tv1 <= 0)
		return(0);
	Tv2 = Tv1 * l1;
	Tv3 = Tv1 * L2;
	Tv4 = abs(Tv3 - Tv2);
	Tv5 = random()%Tv4 + Tv2;
	return(Tv5);
}

short GetMonHp(short l, float l1, float L2)
{
	long Tv1, Tv2, Tv3, Tv4, Tv5;
	Tv1 = MonMinHp[l];
	Tv2 = Tv1 * l1;
	Tv3 = Tv1 * L2;
	Tv4 = abs(Tv3 - Tv2);
	Tv5 = random()%Tv4 + Tv2;
	return(Tv5);
}


long GetValue(long Tv1, float l1, float L2)
{
	long Tv2, Tv3, Tv4, Tv5;

	Tv2 = Tv1 * l1;
	Tv3 = Tv1 * L2;
	Tv4 = abs(Tv3 - Tv2);
	Tv5 = random()%Tv4+Tv2;
	return(Tv5);
}


short TalkToPress(void)
{
	char a[49];
	char str[256];
	Oprint("\r\n`2 Do you wish to make a comment in the daily news? (`0Y`2/`0n`2): ");
	GetYesNo(a);
	Oprint("\r\n");
	if(a[0]=='N')
		return(0);
	Oprint("`2What will say? ");
	MakeInputArea(49);
	KbIn(a,48);
	Oprint("`$\r\n\r\n");
	if(strlen(a)<2)
		return(0);
	sprintf(str,"`2\"%s\"`2 says %s", a, Alias);
	WriteNews(str, 1);
	return(1);
}

short HasItem(char *a)
{
	int g = 0;
	int x;
	
	RTrim(a);
	for(x = 8;x>=1;x--)
	{
		RTrim(Invent[x]);
    	if((atoi(a) == inv[x][2] && inv[x][2] > 0) || !strcasecmp(Invent[x], a))  {
        	g = x;
    	}
	}
	return(g);
}

#if defined(__linux__) || defined(__NetBSD__)
void srandomdev(void)
{
	FILE *dev;
	unsigned long seed;

	dev=fopen("/dev/random","r");
	if(dev!=NULL)
	{
		fread(&seed, sizeof(seed), 1, dev);
	}
	else
	{
		seed=time(NULL);
	}
	srandom(seed);
}
#endif
