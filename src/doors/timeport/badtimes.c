#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

#include "doors.h"
#include "tplib.h"

short Room;
void ShowRoom(long l);
short GetResponse(long l);

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

void Quitter(void)
{
	SaveTime();
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
		Quitter();
		exit(0);
	}
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

int main(int argc, char **argv)
{
	struct itimerval it;
	int x;
	short a;

	ParseCmdLine(argc,argv);	
	srandomdev();

	strcpy(Alias, "Who is it?");
	StatsIn = 0;
	Playercount = 0;
	ReadLinkTo(); // 'initialize the modem
	ReadConfig(); // 'get default configuartion information
	LoadTime();
	
	signal(SIGALRM, LessTime);
	it.it_interval.tv_sec=1;
	it.it_interval.tv_usec=1;
	it.it_value.tv_sec=1;
	it.it_value.tv_usec=1;
	setitimer(ITIMER_REAL, &it, NULL);

	ChatEnabled = 0;
   
	PlayingNow = 1;

    // 'Is this a new player?
    IsHeNew(Alias);
    LoadGame();
    NotAgain[12]='+';
    StatsIn = 1;
	SaveGame();
	OpenInventory();

	Ocls();
	// 'set opening "variables"
	for(x=1;x<argc;x++)
	{
		if(argv[x][0]=='R')
		{
			Room=atoi(argv[x]+1);
			break;
		}
	}

	if(Room < 1 || Room > MaxBadTimes)
		Quitter();

	// '**** begin the REPEATER code
	x=1;
	while(1)
	{
		if(x)
		{
			Ocls();
			ShowRoom(Room);
		}
		a = GetResponse(Room);
		SaveGame();
		if(Hit<1)
		{
			NotAgain[1]='^';
			Oprint("\r\n`6 Searing pain tortures you as your life ends!\r\n");
			Paus(2);
			Quitter();
		}

		if(a==-2)
		{
			x=0;
			continue;
		}
		if(a==-1)
		{
			x=1;
			continue;
		}
		if(a==101)
		{
			Quitter();
			continue;
		}
		if(a>=1 && a<=99)
		{
			Room=a;
			x=1;
			continue;
		}
		Oprint("`@The command you typed is invalid.  Read the menu.\r\n");
		x=0;
	}
	return(0);
}

short GetResponse(long l)
{
	short t,a;
	char str[256];
	char v[2];

	t = 0;
	GetCommand(v);
	Oprint("\r\n");
	// 'Figure out what section to switch to based upon 'L' room and v$ response

	if(v[0] == '?')
		return(-1);
	if(v[0]=='I' && l != 1)
	{
		Inventory();
		return(-1);
	}
	if(v[0]=='V')
	{
		ViewStats();
		return(-1);
	}
	if(v[0]=='X') // 'toggle ansi/ascii
	{
		if(NotAgain[14]==' ')
			NotAgain[14]='|';
		else
			NotAgain[14]=' ';
    		return(-1);
	}

	if(v[0]=='G')
	{
		sprintf(str,"`$%s`! awoke to find %s in a dangerous place.",Alias,sex[0]=='M'?"himself":"herself");
		WriteNews(str, 0);
		a = TalkToPress();
		if(a == 0)
		{
			sprintf(str,"`3%s`3 quickly Time Phased back to the Hanger.",Alias);
			WriteNews(str, 1);
		}
		t = 101;
	}
	return(t);
}

void SaveGame(void)
{
	if(Hit<0)
		Hit=0;
	if(StatsIn > 0)
		SaveStats(Alias, Alias, Money, Stash, sex, CurCode, NotAgain, Weapon, Defense, Hit, Exp, Level, lastplay, ClosFight, PlayFight, PlayKill, ExeLine, ComLine, LineRest);
}

void ShowRoom(long l)
{

	// 'code to print "headers"
	Oprint("`!T`3ime `!P`3ort `8Version `7");
	Oprint(Version);
	Oprint(" -- `7Location: ");

	Oprint("`%You Have No Clue Whatsoever\r\n");

	Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");

	strcpy(Prompt, "G");

	if(l == 1)
	{
		Oprint("`2 You are standing in a large, open field.  From every direction, hundreds\r\n");
		Oprint("`2 upon hundreds of screaming, sword-wielding Scotts are converging on you.\r\n");
	}

	if(l == 2)
	{
		Oprint("`2 You are standing in the center of a busy intersection.  Cars are zipping\r\n");
		Oprint("`2 past you rapidly.  A bus is even headed straight at you!\r\n");
	}

	if(l == 3)
	{
		Oprint("`2 The ground is hundreds of meters below you, and it seems to be spinning\r\n");
		Oprint("`2 up towards you.  You look up and see a plane and several men in the door\r\n");
		Oprint("`2 sneering as you fall towards the ground.\r\n");
	}

	if(l == 4)
	{
		Oprint("`2 You're on a tall bridge over a rushing river.  When you hear a very loud\r\n");
		Oprint("`2 whistling sound, you look down and see train tracks.  The train is coming\r\n");
		Oprint("`2 straight at you and you have nowhere to run.\r\n");
	}

	if(l == 5)
	{
		Oprint("`2 You are standing against a cold brick wall, wearing a blindfold.  You are\r\n");
		Oprint("`2 just able to peek under the black material to see six men with rifles,\r\n");
		Oprint("`2 and they're aimed straight at you.  A cigarette falls from your lips.\r\n");
	}

	Oprint("`2 You blink several times, but it's really happening.  It's not a dream.\r\n");
	Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
	Oprint("`7  (`%G`8)`@et the H*** out of here!      ");
	Oprint("`7  (`!?`8)`3Re-List menu options\r\n");
	Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
}

void UseItem(long c, short b) // 'c=1-8 item number, b=the item code (1-???)
{
	Oprint("`2 You find no use for that item here at this time.\r\n");
}
