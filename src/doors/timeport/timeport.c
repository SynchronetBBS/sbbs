#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "doors.h"
#include "tplib.h"
#include "timeport.h"

short Room;
// char Quote[11][256];
char OtherPlaces[27][256];
char OtherDos[27][256];
short OtherCount;
short MaxTitleScreens;
short IsDead;

/*
'The Game: Struct
'Set ComLine$ to an 8-char prog name
'Set ExeLine$ to a command-line perameter
'Timeport will keep "shelling out" to that program until it detects that
'ComLine$ is null (no more chained IGM's)
*/

/*
'These portions of ChRest$ (LineRest$) are described below
'1-1   (1): Thing that Fred is selling today, hook, line, or pole
'2-9   (8): Amount of hit placed you at this time
'10-10 (1): Has tried to pick door lock today?
'11-11 (1): What Flirt number the player is on 1-5
'12-12 (1): Has tried to dig today?
'13-13 (1): Has flown today?
'14-17 (4): 4-digit directions code. :)
*/
void quitter(void)
{
	NotAgain[12] = ' ';
	ExitGame();
}

void LessTime(int sig)
{
	SecsLeft = SecsLeft - 1;
	if(SecsLeft < 0)
	{
		SecsLeft = 59;
		MinsLeft = MinsLeft - 1;
	}
	if(PlayingNow <= 0)
		return;
	if(MinsLeft < 1)
	{
    	Oprint("`%\r\n\r\n");
    	Oprint("Your time is up, man!\r\n");
		quitter();
		exit(0);
	}
}

int main(int argc, char **argv)
{
	FILE *file;
	char path[MAXPATHLEN];
	struct itimerval it;
	char a[256];
	char b[256];
	char d[256];
	char p[256];
	char str[256];
	char k[256];
	time_t currtime;
	short quick;
	short begin;
	char DATE[9];
	short A;
	short t;
	short DidList;
	short DidView;
	short DidNews;

	ParseCmdLine(argc,argv);	
	strcpy(Bad,"W2");
	srandomdev();
	MaxTitleScreens = 7;
	strcpy(Alias, "Who is it?");
	StatsIn = 0;
	Playercount = 0;
	ReadLinkTo(); // 'initialize the modem
	ReadConfig(); // 'get default configuartion information

	strcpy(Bad,"N3");

	strcpy(Bad,"WX");

	sprintf(path,"%snamelist.dat",DatPath);
	file=SharedOpen(path, "a", LOCK_SH);
	fclose(file);

	if(GraphMode <= 0)
	{
		Oprint("\r\n\r\nANSI was not detected.  if this is in `$C`!O`%L`@O`#R`7");
		Oprint(" you have ANSI.  If it's garbage, you do not have ANSI.\r\n");
		Oprint("Do you have ANSI? ");
		GetYesNo(path);
		if(path[0] == 'N')
		{
			ExitGame();
			return(0);
		}
	}

	signal(SIGALRM, LessTime);
	it.it_interval.tv_sec=1;
	it.it_interval.tv_usec=1;
	it.it_value.tv_sec=1;
	it.it_value.tv_usec=1;
	setitimer(ITIMER_REAL, &it, NULL);
	
	sprintf(path,"%sthedate.dat",DatPath);
	file=SharedOpen(path, "a", LOCK_EX);
	fwrite(" ", 1, 1, file);
	fclose(file);
	file=SharedOpen(path, "r", LOCK_SH);
	readline(d, sizeof(d), file);
	RTrim(d);
	fclose(file);
	currtime=time(NULL);
	strftime(DATE, sizeof(DATE), "%D", localtime(&currtime));
	if(strcmp(d, DATE))
	{
		sprintf(str, "%stp-event N*", DatPath);
		MakeNode(k, str);
		system(k);
		k[0]=0;
	}
	InitPort();
	file=SharedOpen(path,"w", LOCK_EX);
	fputs(DATE, file);
	fclose(file);
	IsDead = 0;
	ChatEnabled = 0;
	Playercount = 0;
	MaximumPlayers = 50;

	sprintf(path,"%sigm.dat",DatPath);
	file=SharedOpen(path, "a", LOCK_SH); // 'initialize IGM.DAT in case
	fclose(file);

/*
'*************************************************************************
' READ IN OTHER PLACES!!!  -- From IGM.DAT
'*************************************************************************
*/
	file=SharedOpen(path,"r", LOCK_SH);
	t = 0;
	k[0]=0;
	OtherCount = 0;
	while(!feof(file) && OtherCount < 26)
	{
    	readline(a,sizeof(a),file);
		RTrim(a);
		if(a[0]!=';' && strlen(a) > 0)
		{
        	if(t == 1)
			{
        	   OtherCount++;
        	   strcpy(OtherPlaces[OtherCount], p);
        	   strcpy(OtherDos[OtherCount], a);
        	}
        	if(t == 0)
				strcpy(p, a);
        	t = 1 - t;
    	}
	}
	fclose(file);
	if(OtherCount > 3 && Regis == 0)
		OtherCount = 3;

/*
'***************************************************************************
*/
	PlayingNow = 1;

/*
'*** the following code is for the Master program: It gets the new player or
'locates old one.  For "stub" this should be replaced by a read-from file
*/
   
	ShowTitle(); // 'not necessary for "stub" operation
// 'show the copyright message screen here... :)
    CountPlayers();
//    'Is this a new player?
    IsHeNew(Alias);
    if(Alias[0]==0)
		NewPlayer();
    strcpy(Bad, "N2");
    LoadGame();
    strcpy(Bad, "N7");
    NotAgain[12]='+';
	StatsIn = 1;
	strcpy(Bad, "N6");
	SaveGame();
    strcpy(Bad, "N8");
    OpenInventory();

/*
'******* STATS ARE IN and game is ready to play.  This should call the input
'routines, and so on. :)
*/

//   'insert "shuttle log-on here"
    DidList = 0;
	DidView = 0;
	DidNews = 0;

	
	quick=0;
	begin=0;
	while(!begin)
	{
		if(!quick)
		{
		    Ocls();
			DisplayTitle("shuttle.ans");
		}
		SetColor(14, 0, 0);
		Position(73, 13);
		Owait(str);
		switch(toupper(str[0]))
		{
			case 'B':
				begin=1;
				break;
			case 'Y':
				Ocls();
				ReadNews(Yesterday);
				quick=0;
				break;
			case 'D':
				Ocls();
				ReadNews(Today);
				DidNews = 1;
				quick=0;
				break;
			case 'I':
				Ocls();
				DisplayText("instruct.doc");
				quick=0;
				break;
			case 'L':
				ListPlayers();
				DidList = 1;
				quick=0;
				break;
			case 'V':
				ViewStats();
				quick=0;
				break;
			case 'Q':
				Ocls();
				NotAgain[12] = ' ';
				ExitGame();
				return(0);
			default:
				quick=1;
		}
	}

	IsDead = 0;
    NotAgain[9] = '0'; //  '0 days since last play now....
	
//    'code to check the date (Ch12$) and if they can't play, let them know!
    if(NotAgain[1] == '#')
	{
		NotAgain[1] = ' ';
        Hit = PlayerHP[Level];
    }
    if(!strcmp(lastplay,  DATE) && NotAgain[1] != ' ')
	{
        // 'player is dead until tomorrow
        IsDead = 1;
    }
    if(IsDead == 0)
	{
        strcpy(lastplay, DATE);
        if(NotAgain[1] != ' ')
		{
			// 'this is refresh code, overwrite ch9$
            Hit = PlayerHP[Level];
		}
        NotAgain[1]=' ';
	}

	Ocls();
	ReadMail(OriginNumber);
	if(DidNews == 0)
	{
		Ocls();
		ReadNews(Today);
	}
	// 'set opening "variables"
	Room = 1; // 'start out in the Time Port Hanger

	if(IsDead > 0)
	{
	    Oprint("\e\n`4 You have been killed today.  You may play again tomorrow.\r\n"); // 'dupj
    	NotAgain[12] = ' ';
    	Paus(2);
    	quitter();
		return(0);
	}

	// '**** begin the REPEATER code
	SaveGame();

	quick=0;
	while(1)
	{
		if(!quick)
		{
			Ocls();
			RTrim(ComLine);
			strcpy(b, ComLine);
			if(strlen(b) > 0)
			{
				A = GetResponse(Room, 1);
				Ocls();
			}
			if(NotAgain[12] == ' ')
			{
				quitter();
				return(0);
			}
			ShowRoom(Room);
		}
		A = GetResponse(Room, 0);
		SaveGame();
		if(NotAgain[12]==' ')
		{
			quitter();
			return(0);
		}
		if(Hit < 1)
		{
			NotAgain[1] = '^';
			Oprint("\r\n`4 You have been killed today.  You may play again tomorrow.\r\n");
			Paus(2);
			quitter();
			return(0);
		}
		switch(A)
		{
			case -2:
				quick=1;
				break;
			case -1:
				quick=0;
				break;
			case 101:
				quitter();
				return(0);
			default:
				if(A>=1 && A<=99)
				{
					Room = A;
					quick=0;
					break;
				}
				else
					Oprint("`@The command you typed is invalid.  Read the menu.\r\n");
		}
	}
	return(0);
}

/*
ErrorInGame: RESUME Err2
Err2: ON ERROR GOTO ErrTrap2
Oprint "SYSOP: An error was encountered in the game: TimePort code:" + Bad$ + ret$
d = TIMER
DO WHILE ABS(TIMER - d) < 3: LOOP
GOTO quitter
*/

/*
ErrTrap2: PRINT
   COLOR 15, 0
   PRINT "  SYSOP: Recursive errors encounterd, program aborted!"
   COLOR 7, 0
   Paus 3
   RESUME en
en: END
*/

void CountPlayers(void)
{
	// '**** GET RID of this routine in STUB operation!
	// 'Count the Players
	FILE *file;
	char path[MAXPATHLEN];
	char a[256];
	char *x;
	
	sprintf(path,"%snamelist.dat",DatPath);
	file=SharedOpen(path, "r", LOCK_SH);
	while(!feof(file))
	{
		readline(a, sizeof(a), file);
		RTrim(a);
		x = strchr(a, '~');
		if(x==NULL)
			x=a;
		else
			while(isspace(*(++x)));
		OpenStats(x);
		if(strlen(ch1) > 0)
			Playercount++;
	}
	fclose(file);
}

void FundTransfer(short x)
{
	long Am;
	char a[256];
	char b[256];
	
	sprintf(a,"`2 On Hand: `0%ld\r\n", Money);
	Oprint(a);
	sprintf(a,"`2 In Bank: `0%ld\r\n\r\n", Stash);
	Oprint(a);
	Oprint("`2 Deposit how much? ");
	if(x != 0)
		Oprint("`2 Withdraw how much? `3(Use `!1`3 for `!All`3) ");
	MakeInputArea(9);
	KbIn(b, 9);
	Oprint("`7\r\n\r\n");
	Am = atoi(b);
	if(Am == 1 && x == 0)
		Am = Money;
	if(Am == 1 && x != 0)
		Am = Stash;

	if(Am <= 0)
	{
	    Oprint("`6 A transfer of no money cannot take place.  Sorry.\r\n");
		Oprint("\r\n");
		PressAnyKey();
		return;
	}

	if(x == 0)
	{
		if(Am > Money)
		{
        	Oprint("`3 You don't have that much money on-hand!\r\n");
			Oprint("\r\n");
			PressAnyKey();
			return;
		}
    	Stash += Am;
	    Money -= Am;
    	Oprint("`! Fund Transfer completed: `3Deposit successful.\r\n");
		Oprint("\r\n");
		PressAnyKey();
		return;
	}

	if(x != 0)
	{
    	if(Am > Stash)
		{
        	Oprint("`3 You don't have that much money in the bank!\r\n");
			Oprint("\r\n");
			PressAnyKey();
			return;
		}
	    Money += Am;
		Stash -= Am;
	    Oprint("`! Fund Transfer completed: `3Withdrawal successful.\r\n");
		Oprint("\r\n");
		PressAnyKey();
		return;
	}
}

void Pline(void)
{
	Oprint("`%----------------------------------------------------------------------------\r\n");
}

void SaveTime(void)
{
	char path[MAXPATHLEN];
	FILE *file;

	sprintf(path,"timeleft.%d",NodeNum);
	file=SharedOpen(path,"w",LOCK_EX);
	fprintf(file,"%ld\r\n",MinsLeft);
	fprintf(file,"%ld\r\n",SecsLeft);
	fclose(file);
}

void LoadTime(void)
{
	char path[MAXPATHLEN];
	FILE *file;
	char line[256];

	sprintf(path,"timeleft.%d",NodeNum);
	file=SharedOpen(path,"r",LOCK_SH);
	readline(line,sizeof(line), file);
	RTrim(line);
	MinsLeft=atoi(line);
	readline(line,sizeof(line), file);
	RTrim(line);
	SecsLeft=atoi(line);
	fclose(file);
}


short GetResponse(short l, short g)
{
	char v[256];
	char tstr[256];
	short t;
	short Shellout=0;
	long x1,x2;
	char str[256];
	char aa[74];
	char path[MAXPATHLEN];
	short d;
	char Rr[256];
	char k[256];
	FILE *file;
	char *O;
	char a[MAXPATHLEN];
	short r;

	if(g==0)
	{
		t = 0;

		GetCommand(v);

		Oprint("\r\n"); // 'blank line before printing any messages after the user command

		// 'Figure out what section to switch to based upon 'L' room and v$ response

		switch(v[0])
		{
			case '?':
				return(-1);
			case 'I':
				Inventory();
				return(-1);
			case 'V':
				ViewStats();
				return(-1);
			case 'X':
				if(NotAgain[14]==' ')
					NotAgain[14]='|';
				else
					NotAgain[14]=' ';
				return(-1);
		}
		if(l == 1)
		{
			switch(v[0])
			{
		   		case 'S':
					t=5;
					break;
				case 'G':
					MakeNode(str,"outside N* R4");
        			sprintf(a,"%s%s",DatPath,str);
        			Shellout=1;
					break;
				case '*':
					t=101;
					break;
				case 'L':
					t=2;
					break;
				case 'A':
					t=14;
					break;
			}
		}

		if(l==2)
		{
			switch(v[0])
			{
				case 'C':
        			if(Regis == 0)
					{
            			Oprint("`5 The TCCL hums and vibrates and squeaks and generally makes a huge amount\r\n");
            			Oprint("`5 of noise.  A message flashes violently on the screen, and as its behavior\r\n");
            			Oprint("`5 calms a little, you see the following words:\r\n");
            			Oprint("`# Name Changes Disabled: Time Port has not yet been registered for this BBS.\r\n");
            			return(-2);
        			}
        			x1 = GoldFromEach[Level] * 5;
					sprintf(str, "`2 A name change will require a payment of %ld Morph Credits.\r\n",x1);
        			Oprint(str);
        			Oprint("`2 This will be taken from your Bank balance.  Change your name? (Y/n): ");
        			GetYesNo(tstr);
					Oprint("\r\n");
        			if(tstr[0] == 'N')
					{
            			sprintf(str,"`2 You decide to continue using %s`2 as your name.\r\n",Alias);
						Oprint(str);
            			return(-2);
        			}
        			if(x1 > Stash)
					{
            			Oprint("`2 You Bank balance will not cover the charge to change your name.\r\n");
            			return(-2);
					}
        			Oprint("`2 What will your new name be? ");
        			MakeInputArea(16);
					KbIn(aa,15);
        			Oprint("`2\r\n");
        			if(strlen(aa) < 2)
					{
            			Oprint("`2 The Time Control Computer retains your orginal name; No change made.\r\n");
            			return(-2);
        			}
        			sprintf(str,"`2 Change your name to %s`2? (Y/n): ",aa);
					Oprint(str);
        			GetYesNo(tstr);
        			Oprint("`2\r\n");
        			if(tstr[0] == 'N')
					{
            			Oprint("`2 The Time Control Computer retains your orginal name; No change made.\r\n");
            			return(-2);
					}
					if(!strcasecmp(Alias, aa))
					{
            			sprintf(str,"`2 Your name is already %s`2.\r\n",Alias);
						Oprint(str);
            			return(-2);
        			}
					OpenStats(aa);
        			if(ch1[0])
					{
            			Oprint("`2 Somebody is already using that name.  Better pick something else.\r\n");
						return(-2);
        			}
        			SaveStats(aa, Alias, Money, Stash, sex, CurCode, NotAgain, Weapon, Defense, Hit, Exp, Level, lastplay, ClosFight, PlayFight, PlayKill, guardname, BribeGuard, LineRest);
					DelOldFromNameList();
					strcpy(Alias, aa);
					sprintf(path,"%snamelist.dat",DatPath);
					file=SharedOpen(path, "a", LOCK_EX);
					fprintf(file, "%s ~%s\r\n", User, Alias);
					fclose(file);
					sprintf(str,"`2 You are now known as %s`2.\r\n\r\n",Alias);
        			Oprint(str);
        			PressAnyKey();
        			Stash -= x1;
        			return(-1);
				case 'R': // 'Request an Update in Status
        			if(Level >= 12)
					{
            			Oprint("`@ You have obtained the maximum status level!\r\n");
            			return(-2);
        			}
        			Pline();
        			Oprint("`# You key in a request for a review of your status.  The TCCL responds:\r\n");
        			x1 = Exp;
        			if(x1 > UpToNext[Level])
					{
						if(Level >= 5 && Regis == 0)
						{
							Oprint("`! You are at Maximum Status for an Unregistered game of Time Port.\r\n");
                			Pline();
                			return(-2);
            			}
                		Level ++;
						Hit = PlayerHP[Level];
						sprintf(str,"`!%s`3 was promoted to the Rank of `!%s`3 (%d) today.",Alias,Rank[Level],Level);
                		WriteNews(str, 1);
						sprintf(str, "`! You've been promoted to Rank %d: %s!\r\n",Level,Rank[Level]);
                		Oprint(str);
                		Pline();
                		return(-2);
            		}
        			else
					{
						x2 = UpToNext[Level] - x1;
						sprintf(str,"`! A Promotion may be granted after obtaining`%% %ld`! more Experience.\r\n",x2);
						Oprint(str);
            			Pline();
            			return(-2);
        			}
					break;
    			case 'Q':
					t=1;
					break;
	    		case 'N':
					Ocls();
        			ReadNews(Today);
        			t = -1;
					break;
				case 'Y':
					Ocls();
					ReadNews(Yesterday);
					t=-1;
					break;
				case 'M':
					d = WriteALetter(Alias, "", 0);
					t = -2;
					break;
				case 'L':
					ListPlayers();
					t = -1;
					break;
				case 'S':
        			Oprint("Write E-Mail to Who? (Name or partial search string): ");
        			MakeInputArea(16);
        			KbIn(Rr,15);
        			Oprint("`!\r\n");
        			if(strlen(Rr) > 0)
					{
            			FindName(k, Rr);
            			if(strlen(k) < 1)
						{
                			Oprint("`@That wasn't found in the TimeTravel Rebellion listing.\r\n");
                			return(-2);
            			}
            			if(!strcasecmp(Alias, ch1))
						{
                			Oprint("`@The TCC prohibits you from padding your own electronic mailbox.\r\n");
                			return(-2);
            			}
            			d = WriteALetter(Alias, ch1, PlayerNum);
						if(d > 0)
						{
							sprintf(str,"|A%-4d%s",OriginNumber,Alias);
							AddToMail(PlayerNum, str);
						}
					}
        			t = -2;
					break;
				case 'W':
	        		FundTransfer(1);
        			t = -1;
					break;
				case 'D':
	        		FundTransfer(0);
        			t = -1;
					break;
			}
		}

		if(l == 3)
		{
    		if(v[0]=='0')
				return(14);
    		r = v[0] - 64;
    		if(r <= 0 || r > OtherCount)
        		t = 3;
    		else
			{
        		MakeNode(a, OtherDos[r]);
        		Shellout=1;
    		}
		}

		if(l == 14)
		{
    		switch(v[0])
			{
				case '0':
					t = 1;
					break;
				case '1':
					t = 3;
					break;
    			case '2':
					MakeNode(str,"stoneage N* R6");
        			sprintf(a,"%s%s",DatPath,str);
        			Shellout=1;
					break;
    			case '3':
					MakeNode(str,"twenties N* R15");
        			sprintf(a,"%s%s",DatPath,str);
        			Shellout=1;
					break;
    			case '4':
					MakeNode(str,"twenties N* R16");
        			sprintf(a,"%s%s",DatPath,str);
        			Shellout=1;
					break;
			}
		}

		if(l == 5)
		{
			switch(v[0])
			{
				case 'R':
					t=1;
					break;
				case 'T':
					t=6;
					break;
				case 'P':
        			if(NotAgain[0]!=' ')
					{
            			Oprint("`@ You have already tried to press the buttons today.  Wait until tomorrow.\r\n");
            			return(-2);
        			}
        			TypeCode();
        			t = -1;
    		}
		}

		if(l == 6)
		{
    		if(v[0]=='R')
				t=5;
    		if(v[0]=='1' || v[0]=='2' || v[0]=='3' || v[0]=='4')
			{
        		Oprint(" `2What do you wish to say in this conversation?\r\n");
        		Oprint("`8->");
        		MakeInputArea(74);
        		KbIn(aa,73);
        		Oprint("`5\r\n");
        		if(strlen(aa)<3)
				{
            		Oprint(" `2That's not going to be good enough.  Better say more, or nothing at all.\r\n");
            		return(-2);
        		}
        		if(v[0]=='1')
					O = "says";
        		if(v[0]=='2')
	        		O = "adds";
        		if(v[0]=='3')
        			O = "replies";
        		if(v[0]=='4')
        			O = "asks";
				else
					O = "is a hacker freak!";
				sprintf(str,"`%%%s`! %s",Alias,O);
        		WriteTherapy(aa, str);
        		t = 5;
    		}
		}
		if(!Shellout)
			return(t);
	}
	while(1)
	{
		if(Shellout)
		{
			NotAgain[12]='+';
			SaveGame();
			SaveTime();
			system(a);
			InitPort();
			LoadTime();
			LoadGame();
			OpenInventory();
		}
		// 'now, see if they've shelled to yet ANOTHER IGM!
		RTrim(ComLine);
		if(strlen(ComLine)<1)
			return(-1);
		if(NotAgain[12] == ' ')
			return(0);
		strcpy(ComLine," ");
		SaveGame();
		sprintf(str, "%s %s", ComLine, ExeLine);
		MakeNode(a, str);
		NotAgain[12] = '+';
		Shellout=1;
	}
}

void MakeNewCode(void)
{
	int x1, x2, x3, x4;
	CurCode[0]=0;
	
	
	do
	{
		x1 = random()%4+1;
		x2 = random()%4+1;
		x3 = random()%4+1;
		x4 = random()%4+1;
	} while(x1 == x2 || x1 == x3 || x1 == x4 || x2 == x3 || x2 == x4 || x3 == x4);
	sprintf(CurCode,"%1d%1d%1d%1d",x1,x2,x3,x4);
}

void NewPlayer()
{
	char str[256];
	char a[2];
	char path[MAXPATHLEN];
	FILE *file;

	// '**** GET RID of this for STUB operation
	Ocls();
	Position(1, 4);

	if(Playercount >= MaximumPlayers)
	{
	    // 'beef this up a little ToDo
	    Oprint("The game is full!\r\n");
	    PressAnyKey();
	    ExitGame();
	}

	Oprint("`#\"Welcome to the 26th Century.\"\r\n\r\n");
	Oprint("`3 Rule 1: Don't ask how you got here.  Not yet, anyway.\r\n");
	Oprint("`3 Rule 2: Don't ask why you're here.  We will tell you that.\r\n");
	Oprint("`3 Rule 3: Don't ask who we are.  You will find out in time.\r\n");

	Alias[0]=0;
	while(!Alias[0])
	{
		strcpy(Bad, "A2");
		Oprint("\r\n`7(If you know how to use color codes, you can put color in your name)\r\n");
		Oprint("\r\n`@By what name will you be know? `4(ENTER alone to quit): ");
		MakeInputArea(16);
		SetColor(15, 4, 0);
		KbIn(Alias, 15);
		RTrim(Alias);
		Oprint("`7\r\n\r\n");
		if(strlen(Alias) < 1 || (toupper(Alias[0])=='X' && Alias[1]==0))
			ExitGame();

		strcpy(Bad, "A3");
		if(Alias[0])
			Alias[0]=toupper(Alias[0]);
		RTrim(Alias);
		sprintf(str,"`#%s? `#Is This Correct (Y/N)                    \b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b",Alias);
		Oprint(str);
		GetYesNo(str);
		if(str[0]=='N')
		{
			Alias[0]=0;
			continue;
		}
/* This didn't work in the original code (no a$ variable existed) *shrug*
		IF INSTR(a$, "FUCK") OR INSTR(a$, "CRAP") OR INSTR(a$, "DAMN") OR INSTR(a$, "SHIT") THEN
		    Oprint "`# You can't use that in your name!\r\n"
		    GOTO Again
		}
*/

		OpenStats(Alias);
		if(ch1[0])
		{
		     strcpy(Bad, "A1");
		     Position(10, 20);
		     Oprint("`%Somebody already has that name!\r\n");
		     Alias[0]=0;
			 continue;
		}
		sprintf(path,"%snamelist.dat", DatPath);
		file=SharedOpen(path,"a",LOCK_EX);
		sprintf(str,"%s ~%s\r\n", User, Alias);
		fputs(str,file);
		fclose(file);
		a[0]=0;
	}

	while(!a[0])
	{
		Oprint("\r\n`@Are you (`$M`@)ale or (`$F`@)emale? ");
		a[0]=0;
		while(!a[0])
		{
			Owait(a);
			UCase(a);
			if(a[0] != 'M' && a[0] != 'F')
				a[0]=0;
		}
		Oprint("\r\n");
		strcpy(sex,a);
		sprintf(str,"You are %s? `#(Y/N)",a[0]=='M'?"Male":"Female");
		Oprint(str);
		GetYesNo(a);
		if(a[0]=='N')
			a[0]=0;
	}
	//'Create initial entry in the PLAYERS.DAT file"
	OpenStats("DELETED2"); // 'seach for any "deleted" slots
	sprintf(str,"%-*.*s",RecLen,RecLen,Alias);
	sprintf(path,"%splayers.dat",DatPath);
	file=SharedOpen(path,"r+",LOCK_EX);
	fseek(file, RecLen*(FirstAvail-1), SEEK_SET);
	fwrite(str, RecLen, 1, file);
	fclose(file);

	InitPlayer();
	MakeNewCode();

	StatsIn = 1;
	SaveGame();
	StatsIn = 0;
	sprintf(str,"`4Another outsider, `@%s`4 was recruited today.",Alias);
	WriteNews(str, 1);
}

char *Prefix (char *dest, char *a, int l)
{
	sprintf(dest,"%*.*s",l,l,a);
	return(dest);
}

void ReadMail(short t)
{
	// '**** GET RID of this (as needed) for a STUB program
	short v;
	// 'MCI Mail Codes
	// '|A1   Sombody     > 4 as account num, 15 as from$
	// '|B                > |B plus 2 spaces, begin quotable section
	// '|C                > |C End quotable section

	short BeginQuote = 0;
	short LinesQuoted = 0;
	char path[MAXPATHLEN];
	char str[256];
	FILE *file;
	int c;
	char aa[256];
	char from[16];
	int x;
	int d;
	char Quote[11][256];

	sprintf(path,"%smail.%d",DatPath,t);
	file=SharedOpen(path,"a",LOCK_EX);
	fclose(file);
	c = 4;
	file=SharedOpen(path,"r",LOCK_SH);
	Oprint(BarBar);
	Oprint("`%                    The `$TCCL `%begins to download your mail\r\n");
	Oprint(BarBar);
	Oprint("\r\n");
	if(feof(file))
		Oprint("`3 Your electronic mailbox is empty.\r\n\r\n\r\n");

	while(!feof(file))
	{
    	readline(aa, sizeof(aa), file);
		RTrim(aa);
    	if(aa[0]=='|')
		{
        	// 'translate the translation code
			switch(toupper(aa[1]))
			{
        		case 'B':
					BeginQuote = 1;
					break;
        		case 'C':
					BeginQuote = 0;
					break;
				case 'A':
            		sscanf(aa,"%*2c%4hd%15c",&v,from);
            		Oprint("`$  Do you wish to write back? `6(Y/N): ");
            		c = 0;
            		GetYesNo(str);
            		if(str[0]=='Y')
					{
	                	Oprint("`$         Quote that message? `6(Y/N): ");
    	            	GetYesNo(str);
        	        	if(str[0]=='Y')
						{
            	        	for(x=1;x<=LinesQuoted;x++)
							{
                	        	if(x == 1)
								{
                    	        	AddToMail(v, "`3Retro-quoter engaged: In response to `!your `3comments below:");
                        		}
								sprintf(str,"`3=ð> `2%1.75s",Quote[x]);
                        		AddToMail(v, str);
	                    	}
    	            	}
						else
							Oprint("\r\n");
        	        	d = WriteALetter(Alias, from, v);
            	    	if(d > 0)
						{
							sprintf(str,"|A%4d%-15.15s",OriginNumber,Alias);
							AddToMail(v, str);
						}
                		BeginQuote = 0;
						LinesQuoted = 0;
	            	}
            		else
						Oprint("\r\n");
						
			}
			aa[0]=0;
    	}
    	if(BeginQuote > 0 && aa[0] && LinesQuoted <= 10)
		{
        	LinesQuoted++;
        	strcpy(Quote[LinesQuoted],aa);
    	}
    	Oprint(aa);
		Oprint("\r\n");
    	c++;
    	if(c > 20)
		{
			PressAnyKey();
			Oprint("\r\n");
			c = 0;
		}
	}
	PressAnyKey();
	Oprint("\r\n");
	fclose(file);
	unlink(path);
}

void ReadNews(char *a)
{
	FILE *file;
	char str[256];
	int d;

	Oprint(BarBar);
	Oprint("`%                      The `$TCCL `%downloads the news file\r\n");
	Oprint(BarBar);
	Oprint("\r\n");

	file=SharedOpen(a,"a",LOCK_EX);
	fclose(file);
	d = 4;
	if(IsDead > 0)
		d = d + 3;
	file=SharedOpen(a,"r",LOCK_SH);
	while(!feof(file))
	{
		readline(str, sizeof(str), file);
		RTrim(str);
    	Oprint(str);
		Oprint("\r\n");
    	d = d + 1;
    	if(d > 20)
		{
        	PressAnyKey();
        	Oprint("\r\n");
        	d = 0;
    	}
	}
	fclose(file);
	if(d > 0)
	{
		PressAnyKey();
		Ocls();
	}
}

void SaveGame(void)
{
	// 'put any "save the game" data into this routine
	if(Hit < 0)
		Hit=0;
	if(StatsIn > 0)
		SaveStats(Alias, Alias, Money, Stash, sex, CurCode, NotAgain, Weapon, Defense, Hit, Exp, Level, lastplay, ClosFight, PlayFight, PlayKill, ExeLine, ComLine, LineRest);
}


void Barr(void)
{
	Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
}

void ShortIntro(void)
{
	Barr();
	Oprint("`!Time Port `#Version `5");
	Oprint(Version);
	Oprint("  ");
	Oprint("`1No dedication yet, until I decide on one. :)\r\n");
	Barr();
	Oprint("`!     Time Port is Shareware, Copyright (C) 1995 by HCI & Mike Snyder.\r\n");
	Oprint("`3     The latest version of Time Port is available on Orion's Realm BBS.\r\n");
	Oprint("`3     Just log on to 405-924-7939.  You'll also find the latest Lunatix.\r\n");
	Barr();
	Oprint("`5       Time Port offers several global commands.  These are available:\r\n");
	Barr();
	Oprint("`7            (`$V`8)`@iew your stats        `7 (`$I`8)`@nventory list and change\r\n");
	Oprint("`7            (`$X`8)`@Ansi Room toggle      `7 (`$?`8)`@Re-show the current room\r\n");
	Barr();
	if(Regis)
	{
    	Oprint("`5 Time Port is `#REGISTERED `5to `#");
		Oprint(RegisterTo1);
		Oprint(" `5of `#");
		Oprint(RegisterTo2);
		Oprint("\r\n");
	}
	else
	{
	    Oprint("`5   Time Port has not yet been registered for this BBS.  If you'd like to\r\n");
	    Oprint("`5   advance above Rank 5, get 10% interest daily on money in the bank,\r\n");
	    Oprint("`5   have access to more than 3 IGM's, place hits on other players, and\r\n");
	    Oprint("`5   best of all to WIN the game, then please encourage your Sysop to\r\n");
	    Oprint("`5   register.  You could even help out.\r\n");
	}
	Barr();

	Oprint("\r\n\r\n");
	PressAnyKey();
	Ocls();
}

void ShowRoom(long l)
{
	char str[256];
	FILE *file;
	char path[MAXPATHLEN];
	int x;
	int pp;

	// 'code to print "headers"
	// 'l=4 is now in the OUTSIDE Module!

	if(l != 6)
	{
    	Oprint("`!T`3ime `!P`3ort `8Version `7");
		Oprint(Version);
		Oprint(" -- `7Location: ");
    	if(l == 1)
			Oprint("`%Time Port Hangar\r\n");
		if(l == 2)
			Oprint("`%Time Control Computer\r\n");
		if(l == 14)
			Oprint("`%Time Portal Choice\r\n");
		if(l == 3)
			Oprint("`%Other Eras in Time\r\n");
		if(l == 5)
			Oprint("`%Discussion Chamber\r\n");
	}

	Oprint(BarBar);

	sprintf(Bad, "NRM");
	if(l != 6)
	{
		sprintf(str,"room%ld.%s",l,NotAgain[14]==' '?"ans":"asc");
		DisplayTitle(str);
	}
	sprintf(Bad, "ANM");

	if(l == 1)	// 'in the hanger
	{
    	strcpy(Prompt, "*EGAL");
    	Oprint(BarBar);
    	Oprint("`7  (`!A`8)`3ctivate the Time Port         ");
    	Oprint("`7  (`!S`8)`3tep through a narrow door\r\n");
    	Oprint("`7  (`!L`8)`3ink to Time Control Computer  ");
		Oprint("`7  (`!E`8)`3xamine a HyberCapsule\r\n");
    	Oprint("`7  (`!G`8)`3o Outside the Hanger\r\n");
    	Oprint("`7  (`%*`8)`@Exit back to the BBS          ");
	}

	if(l == 5)	// 'Discussion Chamber
	{
    	strcpy(Prompt, "TPR");
    	Oprint(BarBar);
    	Oprint("`7  (`!T`8)`3alk to the Other People       ");
    	Oprint("`7  (`!P`8)`3ush the Buttons\r\n");
    	Oprint("`7  (`%R`8)`@eturn to the Main Area        ");
	}

	if(l == 6)	// 'Talking to the Discussion People
	{
		strcpy(Prompt, "1234R");
		sprintf(path,"%sconvers.txt",DatPath);
		file=SharedOpen(path,"r",LOCK_SH);
		for(x=1;x<=16;x++)
		{
			str[0]=0;
        	fread(str, 80, 1, file);
			str[78]=0;
        	Oprint(str);
			Oprint("\r\n");
    	}
		fclose(file);
    	Oprint(BarBar);
    	Oprint("`7  (`!1`8)`3Say `7     (`!2`8)`3Add `7     (`!3`8)`3Reply `7     (`!4`8)`3Ask `7     (`%R`8)`@eturn\r\n");
	}

	if(l == 2)
	{
    	strcpy(Prompt, "MNYLSDWQ");
    	Oprint(BarBar);
    	Oprint("`7  (`!M`8)`3ake an announcement on TCC    ");
    	Oprint("`7  (`!N`8)`3ews and reports today\r\n");
    	Oprint("`7  (`!Y`8)`3esterday's news and reports   ");
    	Oprint("`7  (`!L`8)`3ist the TimeTravel Rebellion\r\n");
    	Oprint("`7  (`%Q`8)`@uit connection to the TCC     ");
    	Oprint("`7  (`!S`8)`3end private E-Mail\r\n");
    	Oprint("`7  (`!D`8)`3eposit Morph Credits into Bank");
    	Oprint("`7  (`!W`8)`3ithdraw Morph Credits\r\n");
    	Oprint("`7  (`!R`8)`3equest an Update in Status    ");
    	Oprint("`7  (`!C`8)`3hange your name\r\n");
	}

	if(l == 3)
	{
    	// 'read IGM file and print other choices first
    	Oprint(BarBar);
		pp = 0;
		strcpy(Prompt, "0");
    	for(x=1; x<=OtherCount; x++)
		{
			strcpy(str,"`7  (`!0`8)`3 ");
			str[7]=64+x;
			strcat(str,OtherPlaces[x]);
			Oprint(str);
        	Prompt[x]=64 + x;
			Prompt[x+1]=0;
        	if(pp == 0)
				Oprint("\e[255D\e[40C");	// Move to column 40 of current line
			else
				Oprint("\r\n");
        	pp = 1 - pp;
    	}
    	Oprint("`7  (`%0`8)`@ None of these\r\n");
	}

	if(l == 14)		// 'In the Defaulting time periods
	{
		strcpy(Prompt, "43210");
		Oprint(BarBar);
		Oprint("`7  (`%0`8)`@ Disengage the Time Portal         ");
		Oprint("`7  (`!1`8)`3 Other Eras in Time\r\n");
		Oprint("`7  (`!2`8)`3 The `7Stone `3Age (`2385,000 BC`3)        ");
		Oprint("`7  (`!3`8)`3 New York, `21924 AD\r\n");
		Oprint("`7  (`!4`8)`3 California, `21874 AD\r\n");
	}

	if(l != 3 && l != 14 && l != 6)
		Oprint("`7  (`!?`8)`3Re-List menu options\r\n");
	Oprint(BarBar);
	sprintf(str,"`2  You have `0%ld`2 Morph Credits on hand, and `0%ld`2 in the Time Bank.\r\n",Money,Stash);
	Oprint(str);
}

void ShowTitle(void)
{
	char ke[16];
	int p;
	
	p=(random()%MaxTitleScreens)+1;
	sprintf(ke,"title%d.ans",p);
    Ocls();
    DisplayTitle(ke);
    Owait(ke);
    Ocls();
    ShortIntro();
}

void TypeCode(void)
{
	char g[5];
	int Gd;
	int x;
	int Tt;
	char numb[4];

	Ocls();
	DisplayTitle("buttons.ans");

	Position(70, 10);
	SetColor(11, 4, 0);

	KbIn(g,4);
	Oprint("`7\r\n");

	Gd = 0;
	for(x = 1; x<=4; x++)
	{
		if(g[x-1]<'1' || g[x-1]>'4')
			Gd=1;
	}
	if(g[0]==g[1] || g[0]==g[2] || g[0]==g[3] || g[1]==g[2] || g[1]==g[3] || g[2]==g[3])
		Gd=1;

	if(Gd)
	{
    	Position(1, 22);
    	Oprint("`2   The sequence your tried was invalid.  You must type 1,2,3,4 once each.\r\n");
    	PressAnyKey();
    	return;
	}

	Tt = 0;
	
	for(x = 0; x<4; x++)
	{
		if(CurCode[x]==g[x])
			Tt++;
	}
	
	if(Tt)
	{
		Position(10 * Tt + 2, 11);
		Oprint("`%-=-");
	}
	Position(1, 22);
	NotAgain[0]='&';
	if(Tt < 4)
    	Oprint("                  `@The sequence you tried was incorrect.\r\n");
	else
	{	// 'you got it right... something happens now.. but what???
    	Oprint("`% The buttons reset, and a panel swooshes open and drops a Heat Bomb.\r\n");
    	Oprint("`7 You pick it up carefully to use against another Time Traveler.\r\n\r\n");
    	NotAgain[3]='@';	// 'Now has a Heat Bomb
    	MakeNewCode();
    	// MID$(NotAgain$, 16, 3) = LEFT$(LTRIM$(STR$(VAL(MID$(NotAgain$, 16, 3)) + 1)), 3)
		numb[3]=0;
		numb[0]=NotAgain[15];
		numb[1]=NotAgain[16];
		numb[2]=NotAgain[17];
		x=atoi(numb);
		x++;
		sprintf(numb,"%-3d",x);
		NotAgain[15]=numb[0];
		NotAgain[16]=numb[1];
		NotAgain[17]=numb[2];
	}
	PressAnyKey();
}

void UseItem(long c,short b)
{
	Oprint("`2 You find no use for that item here at this time.\r\n");
	// 'SaveInvItem c, "-------------------", 0, 0
}

short WriteALetter(char *From, char *To, short Vx) // 'Vx is destination account number
{
	char e[256];
	int Treks;
	char str[256];

	RTrim(To);
	Treks = 0;
	Oprint("`@What do you wish to say.  Remember to press <enter> on a line to quit.\r\n");
	while(1)
	{
		MakeInputArea(74);
		KbIn(e,73);
		Oprint("`9\r\n");
		if(strlen(e) < 1)
		{
			if(Treks > 0)
			{
        		if(Vx > 0)
				{
            		Oprint("\r\n`#");
					Oprint(To);
					Oprint("`# Will get your letter.");
					Oprint("\r\n");
					AddToMail(Vx, "|C  ");
				}
				if(Vx == 0)
				{
					Oprint("`@The TCCL hums as your message is posted on the TCC Net.\r\n");
					WriteNews("", 1);
				}
			}
			return(Treks);
		}
		if(Treks == 0)
		{
    		Treks = 1;
			if(Vx > 0)
			{
				sprintf(str,"`%%%s`! wrote you a letter:`3",From);
        		AddToMail(Vx, str);
        		AddToMail(Vx, "|B  ");
			}
    		if(Vx == 0)
			{
				sprintf(str,"`%%%s Announces:`3",From);
				WriteNews(str, 0);
			}
		}
		if(Vx > 0 && strlen(e) > 0)
			AddToMail(Vx, e);
		if(Vx == 0 && strlen(e) > 0)
			WriteNews(e, 0);
	}
}

void WriteTherapy(char *a, char *n)
{
	FILE *file;
	char path[MAXPATHLEN];
	int x;
	char buf[80];
	
	sprintf(path,"%sconvers.txt",DatPath);
	file=SharedOpen(path, "r+", LOCK_EX);
	for(x=3;x<=16;x++)
	{
    	fseek(file,80*(x-1),SEEK_SET);
		buf[0]=0;
		fread(buf, 80, 1, file);
    	fseek(file,80*(x-3),SEEK_SET);
		fwrite(buf, 80, 1, file);
	}
	fseek(file,80*(14),SEEK_SET);
	fwrite(n,80,1,file);
	snprintf(buf,80,"`3%s",a);
	fwrite(buf,80,1,file);
	fclose(file);
}
