#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "doors.h"
#include "tplib.h"

short Room;
char MonsterName[256];
short mhp;
short MonLev1, MonLev2, MonLev3;

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

	if(Room != 6)
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

char *Mprompt(char *dest)
{
	Oprint("`$ ---> ");
	MakeInputArea(16);
	SetColor(11, 4, 0);
	KbIn(dest,15);
	Oprint("`7\r\n\r\n");
	return(dest);
}

void SaveOtherPlayer(void)
{
	SaveStrStats(ch1, ch1, ch2, ch3, ch4, ch5, Ch6, Ch7, Ch8, Ch9, Ch10, Ch11, Ch12, Ch13, Ch14, Ch15, Ch16, ch17, ChRest);
}

short GetResponse(long l)
{
	long Ap1, pp1, x1, x2, x4;
	long gg1, Gld, Ap2;
	float Arf, Ar2, b, d;
	short t,a,c;
	char str[256];
	char v[2];
	char bstr[16];
	char cstr[256];
	char astr[256];
	int x, Tmp, Pet;

	t = 0;
	if(l == 1)
	{
		sprintf(str,"`8:`7:`%%: `@%s`5 Has`# %d`5 Hit Points at level`# %d`5.\r\n",MonsterName,mhp,MonLev3);
		Oprint(str);
	}
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

	if(l == 13)
	{
		if(v[0]=='C')
			t = 6;
		if(v[0]=='L')  {
			Ocls();
			sprintf(str,"oldman.%d",Level);
			DisplayText(str);
			t = -1;
		}
		if(v[0]=='G')  { // 'Give the old man a fish
			b = HasItem("6");
			if(b < 1)  { // 'doesn't have a fish to give him
				Oprint("`2 The old man sees that you don't have a fish to give him.  'If you bring one\r\n");
				Oprint("`2 to me,' he tells you, 'I'll help you *fix* someones TCCL.'\r\n");
				return(-2);
			}
			Oprint("`# 'Which Recruit's TCCL should I tamper with?' `5(Partial name is okay)\r\n");
			Mprompt(bstr);
			RTrim(bstr);
			if(strlen(bstr)<1)
				return(-2);
			FindName(cstr, bstr);
			if(cstr[0] == 0)  { // 'No matching name was found
				Oprint("`0 'I have never met that person.  Try another name.'\r\n");
				return(-2);
			}
			if(!strcasecmp(cstr,Alias))
			{
				Oprint("\r\n`0 Leave your own TCCL alone.\r\n");
				return(-2);
			}
			OpenStats(cstr); // 'Load the other player's file right now....
			if(Ch6[12] != ' ')
			{
				Oprint("\r\n`0 'You should choose somebody else at this time.'\r\n");
				return(-2);
			}
			// 'Okay, mess up that player's TCCL
			sprintf(str,"\r\n`@ 'Okay, when I see %s, I'll *fix* the TCCL.  Leave it to me.\r\n",ch4[0]=='M'?"him":"her");
			Oprint(str);
			x1 = GetValue(GoldFromEach[Level] * 4, .15, .95);
			sprintf(str,"`6 Note: You also gained %ld experience for doing this.\r\n",x1);
			Oprint(str);
			Exp += x1;
			x = random()%MaxBadTimes+1;
			strcpy(ch17,"badtimes");
			sprintf(Ch16,"R%d N*",x);
			SaveInvItem(b, "-------------------", 0, 0);
			SaveOtherPlayer();
			t = -2;
		}
	}

	if(l== 6)  {
		if(v[0]=='C')
			t = 13;
		if(v[0]=='A')  { // 'attack another recruit!
			Oprint("`# Which Recruit do you wish to attack? `5(Partial name is okay)\r\n");
       		Mprompt(bstr);
			RTrim(bstr);
			if(strlen(bstr)<1)
				return(-2);
			FindName(cstr, bstr);
			if(cstr[0]==0)
			{ // 'No matching name was found
				Oprint("`6 You can't find that person.  Try another name.\r\n");
				return(-2);
			}
			if(!strcasecmp(cstr,Alias))
			{
				Oprint("\r\n`0 You beat yourself up until another recruit shakes some sense into you.\r\n");
				return(-2);
			}
			OpenStats(cstr); // 'Load the other player's file right now....
			if(Level - atoi(Ch11) > 1)  {
				sprintf(str,"\r\n`@Seriously?  %s`@ is too weak to fight you.\r\n",ch1);
				Oprint(str);
				return(-2);
			}
			if(Ch6[1]!=' ' || Ch6[12]!=' ')
			{
				sprintf(str,"\r\n`@ You scan the field for %s`@ but don't see %s.\r\n",ch1,ch4[0]=='M'?"him":"her");
				Oprint(str);
				if(Ch6[1]!=' ')
					sprintf(str," You conclude that %s`@ is already dead.\r\n",ch1);
				else
					sprintf(str," You conclude that %s`@ is in some other time.\r\n",ch1);
				Oprint(str);
            		return(-2);
			}
			if(PlayFight < 1)  {
				Oprint("`@ You don't feel frisky enough to engage in battle again today.\r\n");
				return(-2);
			}
			sprintf(str,"`#%s `5carries %s`5 as a weapon.\r\n",ch1,Ch7[0]!='0'?WepName[WepNum(Ch7[0])]:"Nothing");
			Oprint (str);
			Oprint("`# Do you really want to attack? `%(Y/n): ");
			GetYesNo(str);
			if(str[0]=='N')
			{
				sprintf(str,"`@ You decide to leave %s`@ alone for now.\r\n",ch1);
				Oprint(str);
				return(-2);
			}
			sprintf(str,"`%%%s`7 attacked you in the Stone Age.",Alias);
        	AddToMail(PlayerNum, str);
        	strcpy(MonsterName, ch1);
			PlayFight--;
			MonLev1 = WepNum(Ch7[0]);
        	MonLev2 = WepNum(Ch8[0]);
        	MonLev3 = atoi(Ch11);
        	mhp = atoi(Ch9);
        	t = 1;
		}
    	if(v[0]=='*')
			t = 101;
    	if(v[0]=='L')
		{
			ListPlayers();
			t = -1;
    	}
	}

	if(l== 1)  { // 'fighting one of the monsters
    	if(v[0]=='R')  {
        	Oprint("\r\n`6 You make a quick and embarassing retreat from your opponent.\r\n\r\n");
			sprintf(str,"`%%%s`7 ran away in fear of being killed by you!",Alias);
        	AddToMail(PlayerNum, str);
        	AddToMail(PlayerNum, " ");
        	PressAnyKey();
        	t = 6;
    	}
    	if(v[0]=='A')  {
        	Arf = .95;
        	Ar2 = 1;
        	b = .7;
			d = .5;
			a = Level - MonLev3 + 1;
			if(a < 0)
				a = 0;
			c = MonLev3 - Level + 1;
			if(c < 0)
				c = 0;
        	Ap1 = WeaponAdd[WepNum(Weapon)];
        	gg1 = GetValue(DefenseAdd[MonLev2], Arf, Ar2);
        	b = b + (.5 * a);
			Ap1 = Ap1 * b;
			if(NotAgain[3]!=' ') // 'has Heat Bomb
			{
				sprintf(str,"`%% -- You use your Heat Bomb against %s --\r\n",ch1);
				Oprint(str);
            	NotAgain[3]=' ';
            	Ap1 = Ap1 * (14 - Level);
        	}
        	d = d + (.5 * c);
			gg1 = gg1 * d;
        	x1 = GetHitLev(Ap1, gg1, .25, .75);
        	// '              ^Pwp ^Mdf
        	if(Weapon == '0')
				x1 = x1 / 2;
        	x1 = x1 * (random()%2+1); // 'player fight
        	b = 1;
			d = 1;
			a = Level - MonLev3 + 1;
			if(a < 0)
				a = 0;
			c = MonLev3 - Level + 1;
			if(c < 0)
				c = 0;
        	Ap2 = DefenseAdd[WepNum(Defense)];
        	pp1 = GetValue(WeaponAdd[MonLev1], Arf, Ar2);
        	b = b + (.5 * a);
			Ap2 = Ap2 * b;
        	d = d + (.5 * c);
			pp1 = pp1 * d;
        	x2 = GetHitLev(pp1, Ap2, .25, .75);
        	// '              ^Mwp ^Pdf
        	x2 = x2 * (random()%2 + 1);
        	if(x1 <= 0)
			{
				Oprint("`8:`7:`%: `2You Miss `0");
				Oprint(MonsterName);
				Oprint("`2 Completely!\r\n");
			}
        	if(x1 > 0)
			{
				sprintf(str,"\r\n`8:`7:`%%: `2You attack %s For `@%ld`2 damage!\r\n",MonsterName,x1);
				Oprint(str);
			}
        	mhp = mhp - x1;
        	t = -2;
        	if(mhp <= 0)
			{
				Oprint("`8:`7:`%: `2You have killed ");
				Oprint(MonsterName);
				Oprint(" `4!!!\r\n\r\n");
            	Gld = atoi(ch2) / 2;
            	x1 = atoi(ch3) / 10;
            	pp1 = atoi(Ch10) / 10; // 'you take 10% of Ch1$'s experience.
				sprintf(str,"`6You find`$ %ld`6 Morph Credits.\r\n",Gld);
            	Oprint(str);
            	if(random()%4==1)
				{
					sprintf(str,"`6You steal`0 %ld`6 Morph Credits using %s`6's TCCL.\r\n",x1,ch1);
                	Oprint(str);
                	sprintf(ch3,"%ld",atoi(ch3)-x1);
                	Money += x1;
					sprintf(str,"`@%ld Morph Credits were taken from your account!",x1);
            		AddToMail(PlayerNum, str);
				}
				sprintf(str,"`6You gain `$%ld`6 Experience.\r\n",pp1);
            	Oprint(str);
				Money += Gld;
				if(Money > 49999999)
					Money = 49999999;
            	sprintf(ch2,"%ld",atoi(ch2)-Gld);
				if(ch2[0]=='-')
					strcpy(ch2,"0");
            	Exp += pp1;
				sprintf(Ch10,"%ld",atoi(Ch10)-pp1);
				strcpy(Ch9,"0");
            	PlayKill += 1;
            	Ch6[1]='^';
				x4=atoi(Mid(bstr,ChRest,2,8));
            	if(x4 > 0)
				{ // 'there is a hit to be collected!
					sprintf(str,"`6You collect`0 %ld`6 in Reward Money.\r\n",x4);
                	Oprint(str);
					ChRest[1]='0';
					ChRest[2]=' ';
					ChRest[3]=' ';
					ChRest[4]=' ';
					ChRest[5]=' ';
					ChRest[6]=' ';
					ChRest[7]=' ';
					ChRest[8]=' ';
                	Money += x4;
            	}
				sprintf(str,"`9%s has `$killed `9you!",Alias);
            	AddToMail(PlayerNum, str);
            	AddToMail(PlayerNum, " ");
            	Tmp = atoi(Ch11);
            	sprintf(astr,"%d",FindTheLevel(atoi(Ch10)));
            	if(atoi(astr) <= atoi(Ch11))
					strcpy(Ch11, astr);
            	SaveOtherPlayer();
				sprintf(str,"`%%%s `0Killed `7%s`0 in a player fight.",Alias, ch1);
            	WriteNews(str, 0);
            	if(x4 > 0)
				{
					sprintf(str,"`@Reward Money was paid to %s`@ for the Hit.",Alias);
					WriteNews(str, 0);
				}
            	if(atoi(Ch11) < Tmp)
				{
					sprintf(str,"`0%s`7 was knocked DOWN to level %s!",ch1,Ch11);
					WriteNews(str, 0);
				}
            	PickRandom(astr, "deaths.txt");
            	d = TalkToPress();
            	if(d == 0)
				{
					WriteNews(FixTheString(str, astr, MonsterName, Alias), 1);
				}
            	t = 6;
            	PressAnyKey();
        	}
        	if(mhp > 0)  {
            	if(x2 <= 0)  {
            		Oprint("`0");
					Oprint(MonsterName);
					Oprint(" What is this line?\r\n");
            	}
            	if(x2 > 0)  {
					sprintf(str,"`8:`7:`%%: `0%s`2 hits you for`@ %ld`2 damage!\r\n",MonsterName,x2);
                	Oprint(str);
                	Hit -= x2;
                	if(Hit < 1)
					{ // 'player has been killed
                    	Oprint("\r\n");
                    	Oprint("`5 You have been `#killed `5by ");
						Oprint(MonsterName);
						Oprint(".\r\n");
                    	Oprint("`2 10% experience lost\r\n");
                    	Oprint("`2 Half of On-hand Morph Credits lost.\r\n");
                    	Gld = Money / 2;
                    	pp1 = Exp / 10; // 'you lose 10% of your experience if you lose
                    	Money -= Gld;
						if(Money < 0)
							Money = 0;
						sprintf(ch2,"%ld",atoi(ch2)+Gld);
						if(atoi(ch2) > 49999999)
							strcpy(ch2, "49999999");
						Exp -= pp1;
                    	Tmp = Level;
                    	Pet = FindTheLevel(Exp);
                    	if(Pet <= Level)
							Level = Pet;
                    	if(Level < Tmp)
						{
							sprintf(str,"`0%s`2 was knocked`0 DOWN`2 to level %d!",Alias,Level);
							WriteNews(str, 0);
						}
						sprintf(Ch10,"%ld",atoi(Ch10)+pp1);
						NotAgain[1]='^';
						sprintf(str,"`%%%s`7 broke into your room.\r\n",Alias);
                    	AddToMail(PlayerNum, str);
						sprintf(str,"`0You have killed `9%s`0 in self Defense!",Alias);
                    	AddToMail(PlayerNum, str);
						sprintf(str,"`0You took %ld Morph Credits from the dead corpse of %s.",Gld,Alias);
                    	AddToMail(PlayerNum, str);
						sprintf(str,"`0You gained %ld Experience points.",pp1);
                    	AddToMail(PlayerNum, str);
                    	AddToMail(PlayerNum, " ");
                    	sprintf(Ch15,"%d",atoi(Ch15)+1);
                    	SaveOtherPlayer();
						sprintf(str,"`%%%s `9Killed `7%s`9 in self-defense.",ch1,Alias);
                    	WriteNews(str, 0);
                    	PickRandom(astr, "deaths.txt");
                    	d = TalkToPress();
                    	if(d == 0)
						{
							WriteNews(FixTheString(str, astr, Alias, MonsterName), 1);
						}
                	}
                	// 'Exit from this loop and if Player is dead... it exits
            	}
        	}
    	}
	}

	return(t);
}


char *MakeFly(char *dest)
{
	int x,y;

	dest[0]=0;
	for(x = 1; x<=4;x++)
	{
	    y = random()%4+1;
	    if(y==1)
			strcat(dest, "N");
	    if(y==2)
			strcat(dest, "E");
	    if(y==3)
			strcat(dest, "S");
	    if(y==4)
			strcat(dest, "W");
	}
	return(dest);
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

	if(l== 6)
		Oprint("`%Stone Age: By a Mountain\r\n");
	if(l== 1)
	{
		sprintf(str,"`%%Fighting Against %s\r\n",ch1);
		Oprint(str);
	}
	if(l== 13)
		Oprint("`%Old Man of the Mountain\r\n");

	Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
	if(l != 1)
	{
		sprintf(str,"room%ld.%s",l,NotAgain[14]!=' '?"asc":"ans");
		DisplayText(str);
    }

	if(l== 6)  { // 'Stone Age Mountain
    	strcpy(Prompt, "CAL*");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	Oprint("`7  (`%*`8)`@Time Phase Back to the hanger ");
    	Oprint("`7  (`!A`8)`3ttack another recruit\r\n");
    	Oprint("`7  (`!C`8)`3limb up the mountain          ");
    	Oprint("`7  (`!L`8)`3ist Recruits using the TCCL\r\n");
	}

	if(l== 1)  {
    	strcpy(Prompt, "RA");
    	Oprint("`6  You rush towards ");
		Oprint(ch1);
		Oprint("`6 and prepare yourself for the attack.\r\n");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	Oprint("`7  (`!A`8)`3ttack ");
		Oprint(ch1);
		Oprint("`7         (`%R`8)`@un away\r\n");
	}

	if(l== 13)  {
    	strcpy(Prompt, "GLC");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	Oprint("`7  (`!L`8)`3isten to his Story            ");
    	Oprint("`7  (`%C`8)`@limb Back Down the Mountain\r\n");
    	Oprint("`7  (`!G`8)`3ive Him a Fish                ");
	}

	if(l != 1)
	    Oprint("`7  (`!?`8)`3Re-List menu options\r\n");

	Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
	if(l != 1)  {
		sprintf(str,"`2  You have `0%ld`2 Morph Credits on hand, and `0%ld`2 in the Time Bank.\r\n",Money,Stash);
    	Oprint(str);
	}
}

void UseItem (long c, short b) // 'c=1-8 item number, b=the item code (1-???)
{
	long xxx;
	char str[256];
	char sstr[256];
	char astr[256];
	int Tot,x;

	if(b == 13)
	{
    	if(Room != 13)  {
        	Oprint("`0 You can't fly from here...");
        	return;
    	}
    	if(LineRest[12] != ' ')
		{
        	Oprint("`2 You already flew today... and look what that got you.  Wait until tomorrow.\r\n");
        	return;
    	}
		LineRest[12]='%';
    	Ocls();
    	DisplayTitle("flying.ans");
    	Mid(sstr, LineRest, 14, 4);
    	if(sstr[0]==' ')
		{
        	MakeFly(sstr);
			LineRest[13]=sstr[0];
			LineRest[14]=sstr[1];
			LineRest[15]=sstr[2];
			LineRest[16]=sstr[3];
    	}
    	Oprint("`0  What directions will you fly? ");
    	MakeInputArea(5);
		KbIn(astr,4);
		UCase(astr);
		Pad(str, astr, 4);
		strcpy(astr,str);
    	Oprint("`7\r\n\r\n");
    	Tot = 0;
    	for(x = 0; x<4;x++)
		{
        	if(astr[x]==sstr[x])
				Tot++;
		}
		sprintf(str,"`$ You were able to fly for`@ %d%s\r\n",Tot,Tot==1?"`$ minute":"`$minutes");
    	Oprint(str);
    	if(Tot < 4)  {
        	Oprint("`6 After that, you plummet to the ground.\r\n");
        	Room = 6;
			KickOut = 1;
        	return;
    	}
    	// 'Total was good, now you get Morph Credits
    	xxx = GoldFromEach[Level] * 7;
    	Oprint("\r\n`0 The crisis passes and the wind stops tossing you around.\r\n");
    	Oprint("`2 Great job.  You land on the ground where several awe-struck spectators\r\n");
		sprintf(str,"`2 give you %ld Morph Credits for your outstanding performance.\r\n",xxx);
    	Oprint(str);
    	Money += xxx;
    	Room = 6;
		KickOut = 1;
		MakeFly(sstr);
    	LineRest[13]=sstr[0];
    	LineRest[14]=sstr[1];
    	LineRest[15]=sstr[2];
    	LineRest[16]=sstr[3];
    	return;
	}

	Oprint("`2 You find no use for that item here at this time.\r\n");

// 'SaveInvItem c, "-------------------", 0, 0
}
