#include <ctype.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "doors.h"
#include "tplib.h"

short Room;

void ShowRoom(long l);
short GetResponse(long l);
void ListHits(void);
void SaveOtherPlayer(void);

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

	if(Room != 15 && Room != 16)
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

void AskToMake(void)
{
	int a,b,c;

	// 'Check to see if he can make Fishing Pole
	a = HasItem("2");
	b = HasItem("3");
	c = HasItem("4");

	if(a > 0 && a <= 8 && b > 0 && b <= 8 && c > 0 && c <= 8)
	{
    	if(HasItem("5") > 0)
		{
			Oprint("`@ I could make a fishing pole, but you already have one.\r\n");
        	return;
    	}
    	SaveInvItem(a, "Fishing Pole", 0, 5); //  'cost, num
    	SaveInvItem(b, "-------------------", 0, 0);
    	SaveInvItem(c, "-------------------", 0, 0);
    	Oprint("`0 He takes some of your items and makes a fishing pole for you.\r\n");
    	return;
	}

	// 'Can he make the Huge Wings?
	a = HasItem("10");
	b = HasItem("11");
	c = HasItem("12");
	if(a > 0 && a <= 8 && b > 0 && b <= 8 && c > 0 && c <= 8)
	{
    	if(HasItem("13") > 0)
		{
        	Oprint("`@ I could make Huge Wings, but you already have a set.\r\n");
        	return;
    	}
    	SaveInvItem(a, "Huge Wings", 0, 13); //  'cost, num
    	SaveInvItem(b, "-------------------", 0, 0);
    	SaveInvItem(c, "-------------------", 0, 0);
    	Oprint("`0 He takes some of your items and makes a set of Huge Wings!\r\n");
    	return;
	}

	Oprint("`2 He looks at what you have, but can't make anything out of it.\r\n");
}

short GetResponse (long l)
{
	long x1, x2, x3, x4;
	char v[2];
	char bstr[256];
	char cstr[256];
	char str[256];
	int t,oi;

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

	// 'V$ interpretation goes here

	if(l == 15)
	{
    	if(v[0] == 'A')
		{
        	ListHits();
        	t = -1;
    	}
    	if(v[0] == 'L')
		{
        	ListPlayers();
        	t = -1;
    	}
    	if(v[0] == '*')
			t = 101;
    	if(v[0] == 'P')
		{
			if(Regis == 0)
			{ // 'This is a Register-only option
				Oprint("`2 The Gangsters begin to laugh.  After their inside joke is over, one of\r\n");
				Oprint("`2 them says, 'You want to give people an incentive to kill your enemies,\r\n");
				Oprint("`2 do you?  We'll help you do that, after you've given your Sysop an\r\n");
				Oprint("`2 incentive to register Time Port.  You should even consider helping\r\n");
				Oprint("`2 pay the $15 registration fee.'\r\n");
				Oprint("`2 You nod, back away, and begin to think of how cool it would be if you\r\n");
				Oprint("`2 could place a hit on another player.\r\n");
				return(-2);
			}
			Oprint("`# Which Recruit do you want killed? `5(Partial name is okay)\r\n");
			Oprint("`$ ---> ");
			MakeInputArea(16);
			SetColor(11, 4, 0);
			KbIn(bstr, 15);
			Oprint("`7\r\n\r\n");
			RTrim(bstr);
			if(strlen(bstr) < 1)
				return(-2);
			FindName(cstr, bstr);
			if(!cstr[0]) // 'No matching name was found
			{
				Oprint("`6 You can't find that person.  Try another name.\r\n");
				return(-2);
			}
			if(!strcasecmp(cstr, Alias))
			{
				Oprint("\r\n`0 You want *yourself* killed?  I know of a mental hospital for you...\r\n");
				return(-2);
			}
        	OpenStats(cstr); // 'Load the other player's file right now....
			UCase(ch4);
        	if(Ch6[12]!=' ')
			{
				sprintf(str,"\r\n`@ You can't place a hit on %s at this time.\r\n",ch4[0]=='M'?"him":"her");
				Oprint(str);
            	return(-2);
			}
			if(NotAgain[2] != ' ')
			{
				Oprint("\r\n`@ You have already placed a Hit on someone today.\r\n");
            	return(-2);
        	}
        	// 'Okay, now place the hit
        	x1 = GoldFromEach[atoi(Ch11)] * 6;
        	x2 = atoi(Mid(str, ChRest, 2, 8));
        	x3 = x1 - x2; // 'x3 is now set to the MAXIMUM money allowed for a hit
        	if(x3 <= 0) // 'can't hit the person
			{
				sprintf(str,"\r\n`@ The reward for %s`@ is already large enough.\r\n",ch1);
            	Oprint(str);
            	return(-2);
			}
			sprintf(str,"\r\n`0 How much will you give for a reward? (0 - %ld) ",x3);
        	Oprint(str);
        	MakeInputArea(9);
        	KbIn(bstr, 8);
        	Oprint("`7\r\n\r\n");
        	x4 = atoi(bstr);
        	if(x4 > x3)
			{
				sprintf(str,"`@ The others tell you that %s`@ isn't worth that much.\r\n",ch1);
            	Oprint(str);
            	return(-2);
			}
        	if(x4 <= 0)
			{
				sprintf(str, "`@ You decide not to place a hit on %s`@.\r\n", ch1);
            	Oprint(str);
				return(-2);
        	}
        	if(x4 > Money)
			{
            	Oprint("`@ You don't have enough Morph Credits on hand to pay that much.\r\n");
				return(-2);
        	}
        	Money -= x4;
			sprintf(bstr,"%-8ld",(atoi(Mid(str, ChRest, 2, 8))+x4));
			ChRest[1]=bstr[0];
			ChRest[2]=bstr[1];
			ChRest[3]=bstr[2];
			ChRest[4]=bstr[3];
			ChRest[5]=bstr[4];
			ChRest[6]=bstr[5];
			ChRest[7]=bstr[6];
			ChRest[8]=bstr[7];
        	SaveOtherPlayer();
        	Oprint("`# Your hit has now been placed on ");
			Oprint(ch1);
			Oprint("`#!\r\n");
        	t = -2;
		}
	}

	if(l == 16)
	{
    	if(v[0] == '*')
			t = 101;
    	if(v[0] == 'A')
		{
        	AskToMake();
        	t = -2;
		}
    	if(v[0] == 'G')
        	t = 17;
    	if(v[0] == 'W')
        	t = 19;
	}

	if(l == 17)
	{
    	if(v[0] == 'R')
			t = 16;
    	if(v[0] == 'G')
			t = 18;
	}

	if(l == 18)
    	if(v[0] == 'R')
			t = 17;

	if(l == 19)
	{
    	if(v[0] == 'R')
			t = 16;
    	if(v[0] == 'B')
			t = 20;
	}

	if(l == 20)
	{
    	t = BuyStuff(v, "`@  'Fraid I can't help ya with that,' Dale says.");
    	if(t == -99)
			t = 19; // 'that's the code for "exit from this room." :)
	}

	if(l == 5)
	{
    	if(v[0] == 'R')
			return(18);
    	t = atoi(v);
    	if(t < 1 || t > 5)
			return(0);
    	oi = LineRest[10]-'0';
    	if(t > oi) // 'Tried to go toooo fast
		{
        	if(sex[0] == 'M')
			{
            	if(t == 2)
					Oprint("`@ She yanks her hand out of yours before you can kiss it.\r\n");
            	if(t == 3)
					Oprint("`@ She begins laughing, and tells you NO you can't kiss her.\r\n");
            	if(t == 4)
					Oprint("`@ As you begin running your hands over her body, she slaps you.\r\n");
            	if(t == 5)
					Oprint("`@ She jumps up, screams, and tells you to get out before her dad finds you.\r\n");
			}
        	else
			{
            	if(t == 2)
					Oprint("`@ He glances up at you, but seems not to care whatsoever.\r\n");
            	if(t == 3)
					Oprint("`@ He pushes you away and calls you a filthy tramp.\r\n");
            	if(t == 4)
					Oprint("`@ He puts a stop to your groping rather abruptly and tells you to get out.\r\n");
            	if(t == 5)
					Oprint("`@ He seems genuinely shocked at your lewd behavior and tells you that his\r\n");
            	if(t == 5)
					Oprint("`@ father, the bartender, would almost certainly disapprove of this.\r\n");
			}
        	oi--;
			if(oi < 1)
				oi = 1;
        	LineRest[11]=oi+'0';
        	Oprint("`@ You sadly return to the Balcony, totally rejected.\r\n\r\n");
        	PressAnyKey();
        	return(18);
		}
    	if(sex[0] == 'M')
		{
        	if(t == 1) Oprint("`@ She winks at you and blushes at your compliment.\r\n");
        	if(t == 2) Oprint("`@ She smiles shyly as you kiss her hand.\r\n");
        	if(t == 3) Oprint("`@ The gleaming look in her eyes tells you it's safe to kiss her.\r\n");
        	if(t == 4) Oprint("`@ You ravish each others bodies, only stopping when she decides it's enough.\r\n");
        	if(t == 5)
			{
            	x1 = GoldFromEach[Level] * 8;
            	Oprint("`@ You turn out the lights and make love to her.  It is the most fulfilling\r\n");
				sprintf(str,"`@ experience of your entire life, and you gain %ld experience.\r\n",x1);
            	Oprint(str);
            	Exp += x1;
				sprintf(str,"`#%s`3 got laid by the bartender's daughter in the Old West.", Alias);
            	WriteNews(str, 1);
			}
		}
		else
		{
        	if(t == 1) Oprint("`@ He nods, smiles, and tells you that he's often been told that.\r\n");
        	if(t == 2) Oprint("`@ The closer you get, the wider he smiles.  He must like you.\r\n");
        	if(t == 3) Oprint("`@ He holds you tightly as you kiss him.\r\n");
        	if(t == 4) Oprint("`@ He seems to enjoy the attention you give him.\r\n");
        	if(t == 5)
			{
            	x1 = GoldFromEach[Level] * 8;
            	Oprint("`@ You turn out the lights and quickly seduce him.  He makes love to you,\r\n");
            	Oprint("`@ and it is the most fulfilling moment in your life.\r\n");
				sprintf(str,"`@ You gained %ld experience points.\r\n",x1);
            	Oprint(str);
            	Exp += x1;
				sprintf(str,"`#%s`3 got laid by the bartender's son in the Old West.",Alias);
            	WriteNews(str, 1);
			}
		}
    	if(oi == atoi(v))
		{
			oi = oi + 1;
			if(oi > 5)
				oi = 1;
		}
		LineRest[10]=oi+'0';
    	Oprint("`2 with a sense of accompolishment, you return to the Balcony.\r\n\r\n");
    	PressAnyKey();
    	t = 18;
	}

	return(t);
}

void SaveOtherPlayer(void)
{
	SaveStrStats(ch1, ch1, ch2, ch3, ch4, ch5, Ch6, Ch7, Ch8, Ch9, Ch10, Ch11, Ch12, Ch13, Ch14, Ch15, Ch16, ch17, ChRest);
}

void ListHits(void)
{
	char AliasName[1000][16];
	long HitMoney[1000];

	int GoMax = 0;
	FILE *file;
	char path[MAXPATHLEN];
	char bstr[256];
	char str[256];
	char *user;
	char *alias;
	int n,gap,i,j,k,l,Pax,Pottr,xix,Bv,x;

	sprintf(path,"%snamelist.dat",DatPath);
	file=SharedOpen(path,"r",LOCK_SH);

	while(!feof(file))
	{
		readline(bstr,sizeof(bstr),file);
		if(feof(file))
			continue;
		GoMax++;
		HitMoney[GoMax]=0;
		user=strtok(bstr,"~");
		if(user !=NULL)
		{
			alias=strtok(NULL,"~");
			if(alias != NULL)
			{
				RTrim(user);
				RTrim(alias);
				strcpy(AliasName[GoMax],alias);
				OpenStats(alias);
				if(ch1[0])
					HitMoney[GoMax]=atoi(Mid(str,ChRest,2,8));
				if(HitMoney[GoMax] <= 0 || HitMoney[GoMax] > 99999999)
					GoMax--;
			}
		}
	}
	fclose(file);

	if(GoMax > 1)
	{
		// '**** begin shell-sorting
		n = GoMax;
		gap = n / 2;

		while(gap > 0)
		{
    		for(i = gap + 1; i <= n; i++)
			{
				j = i - gap;
				while(j > 0)
				{
					k = j + gap;
					if(HitMoney[j] > HitMoney[k])
						j = 0;
					else
					{
						l=HitMoney[j];
						HitMoney[j]=HitMoney[k];
						HitMoney[k]=l;
						strcpy(str,AliasName[j]);
						strcpy(AliasName[j],AliasName[k]);
						strcpy(AliasName[k],str);
						j = j - gap;
					}
				}
			}
    		gap = gap / 2;
		}
	}

	Ocls();
	Pax = 16;
	Pottr = 1;
	Oprint("\r\n");
	Oprint("`2컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�\r\n");
	Oprint("`!           The Hit List\r\n");
	Oprint("`2컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�\r\n");
	Oprint(" Sex  Name               Reward\r\n");
	Oprint("`2컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�\r\n");

	for(xix = 1;xix<=GoMax;xix++)
	{
    	OpenStats(AliasName[xix]);
    	if(ch1[0])
		{
        	if(Pottr == 0)
			{
            	Ocls();
				Oprint(" Sex  Name               Reward\r\n");
				Oprint("`2컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�\r\n");
			}
        	// 'show the listing
        	Oprint("`$   ");
			if(toupper(ch4[0])=='F')
				Oprint("F  ");
			else
				Oprint("   ");
        	Bv = 19;
        	for(x = 1;x<=strlen(ch1);x++)
			{
				if(ch1[x-1]=='`')
					Bv+=2;
			}
        	if(Bv < 1)
				Bv = 1;
			sprintf(str,"%-*s",Bv,ch1);
			Oprint(str);
			sprintf(str,"`@%-10s\r\n",Mid(bstr,ChRest,2,8));
			Oprint(str);
        	Pottr ++;
        	if(Pottr >= Pax)
			{
            	Pottr = 0;
				Pax = 20;
            	Oprint("\r\n");
            	PressAnyKey();
			}
		}
	}
	Oprint("\r\n");
	if(Pottr > 0)
		PressAnyKey();
}

void SaveGame(void)
{
	// 'put any "save the game" data into this routine
	if(Hit < 0)
		Hit=0;
	if(StatsIn > 0)
		SaveStats(Alias, Alias, Money, Stash, sex, CurCode, NotAgain, Weapon, Defense, Hit, Exp, Level, lastplay, ClosFight, PlayFight, PlayKill, ExeLine, ComLine, LineRest);
}

void ShowRoom(long l)
{
	char str[256];

	// 'code to print "headers"
	Oprint("`!T`3ime `!P`3ort `8Version `7");
	Oprint(Version);
	Oprint(" -- `7Location: ");

	if(l == 15) Oprint("`%1924 New York: Gangster Meeting\r\n");
	if (l == 16) Oprint("`%1874 California: Brettville\r\n");
	if (l == 17) Oprint("`%In the Brettville Saloon\r\n");
	if (l == 18) Oprint("`%Balcony in the Saloon\r\n");
	if (l == 19) Oprint("`%Dale's General Store\r\n");
	if (l == 20) Oprint("`%Buying from Dale\r\n");
	if (l == 5) Oprint("`%A small Bedroom\r\n");

	Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
	if(l != 5)
	{
		sprintf(str,"room%ld.%s",l,NotAgain[14]!=' '?"asc":"ans");
		DisplayText(str);
	}

	if(l == 15)
	{ // 'Gangsters in New York
    	strcpy(Prompt, "LAP*");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	Oprint("`7  (`%*`8)`@Time Phase Back to the Hanger ");
    	Oprint("`7  (`!A`8)`3sk to see the Hit List\r\n");
    	Oprint("`7  (`!P`8)`3lace a Hit on Somebody        ");
    	Oprint("`7  (`!L`8)`3ist Recruits using the TCCL\r\n");
	}

	if(l == 16)
	{ // 'Town Square of Brettville
    	strcpy(Prompt, "AG*");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	Oprint("`7  (`%*`8)`@Time Phase Back to the Hanger ");
    	Oprint("`7  (`!A`8)`3sk Him to Make Something\r\n");
    	Oprint("`7  (`!G`8)`3o into the Saloon             ");
    	Oprint("`7  (`!W`8)`3alk into Dale's General Store\r\n");
	}

	if(l == 17)
	{ // 'In the Bretville saloon
    	strcpy(Prompt, "GR");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	Oprint("`7  (`%R`8)`@eturn to the Town Square      ");
    	Oprint("`7  (`%G`8)`3o up to the Balcony\r\n");
	}

	if(l == 18)
	{ // 'Balcony in the Saloon
    	strcpy(Prompt, "R");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	Oprint("`7  (`%R`8)`@eturn Downstairs to Saloon    ");
	}

	if(l == 19)
	{ // 'Dale's General Store
    	strcpy(Prompt, "BR");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	Oprint("`7  (`%R`8)`@eturn to the Town Square      ");
    	Oprint("`7  (`%B`8)`3uy something from Dale\r\n");
	}

	if(l == 5)
	{ // 'In the Bedroom
		strcpy(Prompt, "12345R");
    	if(LineRest[10] < '1' || LineRest[10] > '5')
    		LineRest[10]='1';
    	if(sex[0] == 'M')
		{
        	Oprint("`6 You are in a small, cozy bedroom on the floor above the Saloon.  As your\r\n");
        	Oprint("`6 eyes adjust to the dim light, you see a very voluptuous woman near the bed.\r\n");
        	Oprint("`6 she smiles at you and sits down on the black sheets.  Your heart pounds\r\n");
        	Oprint("`6 as you realize she is the most beautiful woman you have ever seen.\r\n");
			Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
        	Oprint("`7  (`%1`8)`3Tell her that she is very beautiful\r\n");
        	Oprint("`7  (`%2`8)`3Kiss her hand and hope to flatter her\r\n");
        	Oprint("`7  (`%3`8)`3Ask for the honer of kissing her lips\r\n");
        	Oprint("`7  (`%4`8)`3Sit beside her and ravish her body\r\n");
		}
		else
		{
        	Oprint("`6 You are in a small, dimly-lit bedroom on the floor above the Saloon.\r\n");
        	Oprint("`6 After your eyes adjust to the light, you notice a very handsome and very\r\n");
        	Oprint("`6 well built young man standing near the bed.  He sees you, smiles and sits\r\n");
        	Oprint("`6 down upon the black sheets.\r\n");
			Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
        	Oprint("`7  (`%1`8)`3Tell him that you think he's very handsome\r\n");
        	Oprint("`7  (`%2`8)`3Stand closer and dazzle him with your presence\r\n");
        	Oprint("`7  (`%3`8)`3Kiss him passionately\r\n");
        	Oprint("`7  (`%4`8)`3Sit with him on the bed and run your hands over his body\r\n");
		}
    	Oprint("`7  (`%5`8)`3Turn off the lamp and slide under the sheets\r\n");
    	Oprint("`7  (`%R`8)`@eturn to the balcony right now\r\n");
	}

	if(l == 20)
	{
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	LoadItems("7  8  9  ");
    	strcpy(Prompt, "3210");
    	ListItems(1);
	}

	if(l != 20)
    	Oprint("`7  (`!?`8)`3Re-List menu options\r\n");
	Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");

	sprintf(str,"`2  You have `0%ld`2 Morph Credits on hand, and `0%ld`2 in the Time Bank.\r\n",Money,Stash);
	Oprint(str);
}

void UseItem(long c, short b) // 'c=1-8 item number, b=the item code (1-???)
{
	if(b == 13)
	{
    	Oprint("`0 You can't fly from here...");
    	return;
	}

	if(Room == 18 && b == 8)
	{
    	if(LineRest[9] != ' ')
		{
        	Oprint("`2 Wait until tomorrow, the bartender might walk up here and see you.\r\n");
        	return;
		}
    	LineRest[9]='-';
		if(random()%2)
		{
        	Oprint("`@ You try jamming the lock picks into one of the doors, but it doesn't work.\r\n");
			return;
		}
    	SaveInvItem(c, "-------------------", 0, 0);
    	Oprint("`! The lock on the left-hand door clicks open and you step inside.\r\n");
    	Room = 5;
		KickOut = 1;
		return;
	}

	Oprint("`2 You find no use for that item here at this time.\r\n");
}

