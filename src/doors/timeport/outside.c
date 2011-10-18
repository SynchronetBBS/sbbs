#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "doors.h"
#include "tplib.h"

short Room;
char MonsterName[256];
short Mhp;
short Monlev1, Monlev2, Monlev3;
short MaxTitleScreens;
short IsDead;

short GetResponse(long l);
void ShowRoom(long l);

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
	MaxTitleScreens = 7;

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

	IsDead = 0;
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
	if(Room != 4)
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
		if(Hit < 1)
		{
    		NotAgain[1] = '^';
    		Oprint("\r\n`6 Searing pain tortures you as your life ends!\r\n");
    		Paus(2);
    		Quitter();
		}
		if(a == -2)
		{
			x=0;
			continue;
		}
		if(a == -1)
		{
			x=1;
			continue;
		}
		if(a==101)
			Quitter();
		if(a >= 1 && a<= 99)
		{
			Room=a;
			x=1;
			continue;
		}
		Oprint("`@The command you typed is invalid.  Read the menu.\r\n");
		x=0;
	}
	Quitter();
	return(0);
}

short GetResponse(long l)
{
	long Ap1, pp1, x1, x2, gg1, Gld, Ap2, Va;
	float Arf, Ar2, d, b;
	short a, c, Tx;
	int t;
	char str[256];
	char str2[256];
	char v[2];
	short B1,Tmp,Pet;
	char b2[4], b3[4], b4, b5[5];

	t = 0;
	if(l == 8)
	{
		sprintf(str,"`8:`7:`%%:`5 The `@%s`5 Has`# %d`5 Hit Points at level`# %d`5.\r\n",MonsterName, Mhp, Monlev3);
		Oprint(str);
	}
	GetCommand(v);
	v[1]=0;
	Oprint("\r\n");
	// 'Figure out what section to switch to based upon 'L' room and v$ response

	if(v[0]=='?')
		return(-1);
	if(v[0]=='I' && l != 8 && l != 13 && l != 14)
	{
		Inventory();
		return(-1);
	}
	if(v[0]=='V')
	{
		ViewStats();
		return(-1);
	}
	if(v[0] == 'X') // 'toggle ansi/ascii
	{
    	if(NotAgain[14]==' ')
        	NotAgain[14] = '|';
    	else
        	NotAgain[14]=' ';
    	return(-1);
	}

	if(l==4)
	{
    	if(v[0]=='R')
			t = 101;
    	if(v[0]=='S')
			t = 7;
		if(v[0]=='L')
		{
			if(ClosFight <= 0)
			{
        	   Oprint("You have no Hours remaining...\r\n");
        	   return(-2);
			}
			else
			{
				t = 8;
				Monlev3 = 0;
				ClosFight--;
				Monlev1 = random()%Level+random()%3;
				Monlev2 = Monlev1 + random()%3 -1;
				if(random()%2 == 1)
				{
					Monlev1 = Level;
					Monlev2 = Level;
				}
				if(Level <= 2)
				{
					Monlev1 = Level;
					Monlev2 = Level;
				}
				if(Monlev1 < 1)
					Monlev1 = 1;
				else
					if(Monlev1 > 12)
						Monlev1 = 12;
				if(Monlev2 < 1)
					Monlev2 = 1;
				else
					if(Monlev2 > 12)
						Monlev2 = 12;
				sprintf(str,"mutants.%d",Monlev1);
				PickRandom(MonsterName,str);
				Mhp = GetMonHp(Monlev1, .25, 1);
				Monlev3 = Monlev1;
			}
		}
	}

	if(l == 7)
	{
    	if(v[0]=='N')
			t = 4;
    	if(v[0]=='S')
			t = 9;
    	if(v[0] == 'E')  // 'examine the water
		{
        	if(NotAgain[4]==' ')
			{
            	Oprint("`2 Don't worry about the water now.  Try again tomorrow.\r\n");
            	return(-2);
			}
			NotAgain[4]='(';
        	if((random()%7+1)!=3)
			{
				Oprint("`@ You see your reflection. ");
				Oprint(sex[0]=='M'?"Quite a handsome fellow, you decide.":"Quite a gorgeous woman, you decide.");
            	return(-2);
			}
        	// 'otherwise... okay, you got something to happen.
        	Oprint("`0 The water flickers, flashes, and you feel the day grow longer.\r\n");
        	ClosFight += random()%3+3;
        	t = -2;
		}
	}

	if(l == 9)
	{
    	if(v[0] == 'N')
			t = 7;
    	if(v[0] == 'D')
			t = 10;
    	if(v[0] == 'W')
			t = 11;
    	if(v[0] == 'O')
			t = 12;
    	if(v[0]=='E')
		{
        	if(Level < 12)
			{
            	Oprint("`@ You are not yet prepared to enter that building.  But you will be one day.\r\n");
            	return(-2);
			}
        	if(Exp < 12000000)
			{
            	Oprint("`2 The door budges a little, but the voices of the hosts warn you to gain\r\n");
            	Oprint("`2 a bit more experience before you will be ready to return home.\r\n");
            	return(-2);
			}
        	// 'Otherwiese, you can go in
        	Ocls();
        	DisplayTitle("ending11.ans");
        	B1=PlayKill;
        	Mid(b2, NotAgain, 7, 3);
			Mid(b3, NotAgain, 16, 3);
        	b4=NotAgain[10];
        	strcpy(b5,CurCode);
        	InitPlayer();
			NotAgain[12]='Y';
			PlayKill = B1;
			NotAgain[6]=b2[0];
			NotAgain[7]=b2[1];
			NotAgain[8]=b2[2];
			NotAgain[15]=b3[0];
			NotAgain[16]=b3[1];
			NotAgain[17]=b3[2];
			strcpy(CurCode,b5);
			b4++;
			if(b4>'9')
				b4='1';
			NotAgain[10]=b4;
        	SaveGame();
			sprintf(str,"`%%%s `%%Was allowed to return home today after meeting the Hosts.",Alias);
        	WriteNews(str, 1);
        	PressAnyKey();
        	Ocls();
        	DisplayTitle("ending2.ans");
        	PressAnyKey();
        	Ocls();
        	DisplayText("ending.txt");
        	t = 101;
		}
	}

	if(l == 8)	// 'fighting one of the monsters
	{
    	if(v[0]=='R')
		{
        	Oprint("\r\n`6 In fear of your life, you make your retreat from the group of mutants.\r\n\r\n");
        	PressAnyKey();
        	t = 4;
		}
    	if(v[0]=='A')
		{
        	Arf = .5;
			Ar2 = 1;
        	b = .7;
			d = .5;
			a = Level - Monlev3 + 1;
			if(a < 0)
				a = 0;
			c = Monlev3 - Level + 1;
			if(c < 0)
				c = 0;
        	Ap1 = WeaponAdd[WepNum(Weapon)];
        	gg1 = GetValue(DefenseAdd[Monlev2], Arf, Ar2);
        	b = b + (.5 * a);
			Ap1 = Ap1 * b;
        	d = d + (.5 * c);
			gg1 = gg1 * d;
        	x1 = GetHitLev(Ap1, gg1, .75, 1);
//        	'              ^Pwp ^Mdf
        	if(Weapon == '0')
				x1 = x1 / 2;
        	b = 1;
			d = 1;
			a = Level - Monlev3 + 1;
			if(a < 0)
				a = 0;
			c = Monlev3 - Level + 1;
			if(c < 0)
				c = 0;
        	Ap2 = DefenseAdd[WepNum(Defense)];
        	pp1 = GetValue(WeaponAdd[Monlev1], Arf, Ar2);
        	b = b + (.5 * a);
			Ap2 = Ap2 * b;
        	d = d + (.5 * c);
			pp1 = pp1 * d;
        	x2 = GetHitLev(pp1, Ap2, .75, 1);
//        	'              ^Mwp ^Pdf
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
        	Mhp = Mhp - x1;
        	t = -2;
        	if(Mhp <= 0)
			{
            	Oprint("`8:`7:`%: `2You have killed ");
				Oprint(MonsterName);
				Oprint(" `4!!!\r\n\r\n");
            	if(Monlev1 < 0)
					Monlev1 = 0;
            	if(Monlev1 > 12)
					Monlev1 = 12;
            	Gld = GetValue(GoldFromEach[Monlev1], .9, 1);
            	pp1 = GetValue(ExpFromEach[Monlev1], .8, 1);
				sprintf(str,"`6You find`$ %ld`6 Morph Credits.\r\n",Gld);
            	Oprint(str);
				sprintf(str,"`6You gain`$ %ld`6 Experience.\r\n",pp1);
            	Oprint(str);
            	Money += Gld;
            	Exp += pp1;
            	t = 4;
            	PressAnyKey();
			}
        	if(Mhp > 0)
			{
            	if(x2 <= 0)
				{
            		Oprint("`0");
					Oprint(MonsterName);
					Oprint(" What is this line?\r\n");
				}
            	if(x2 > 0)
				{
					sprintf(str,"`8:`7:`%%: `0%s`2 hits you for`@ %ld`2 damage!\r\n",MonsterName,x2);
                	Oprint(str);
                	Hit -= x2;
                	if(Hit < 1) // 'player has been killed
					{
                    	Oprint("\r\n");
                    	Oprint("`5 You have been `#killed `5by ");
						Oprint(MonsterName);
						Oprint(".\r\n");
                    	Oprint("`2 10% experience lost\r\n");
                    	Oprint("`2 Half of On-hand Morph Credits lost.\r\n");
						sprintf(str,"`!%s `3was killed by `!%s", Alias, MonsterName);
                    	WriteNews(str, 0);
                    	Gld = Money / 2;
                    	pp1 = Exp / 10; // 'you lose 10% of your experience if you lose
                    	Money -= Gld;
						if(Money < 0)
							Money = 0;
                    	Exp -= pp1;
                    	Tmp = Level;
                    	Pet = FindTheLevel(Exp);
                    	if(Pet <= Level)
							Level = Pet;
                    	if(Level < Tmp)
						{
							sprintf(str,"`0%s`2 was knocked`0 DOWN`2 to level %d!",Alias, Level);
							WriteNews(str, 0);
						}
                    	PickRandom(str, "deaths.txt");
                    	d = TalkToPress();
                    	if(d == 0)
						{
							FixTheString(str2,str,Alias,MonsterName);
							WriteNews(str2, 1);
						}
					}
                	// 'Exit from this loop and if Player is dead... it exits
				}
			}
		}
	}

	if(l == 11)
	{
    	if(v[0] == 'B')
			t = 14;
    	if(v[0] == 'R')
			t = 9;
    	if(v[0] == 'S' && Weapon == '0')
		{
        	Oprint("`@ You don't have a weapon to sell.\r\n");
        	t = -2;
		}
    	if(v[0] == 'S' && Weapon != '0')
		{
        	t = -2;
        	Oprint("`@ 'I can pay you half of what it's worth to me,' Bleep says.\r\n");
        	Tx = WepNum(Weapon);
			Va = WepCost[Tx] / 2;
			sprintf(str,"`4He offers you %ld Morph Credits for your %s. Accept? (Y/N) ",Va, WepName[Tx]);
        	Oprint(str);
        	SetColor(12, 0, 0);
        	Owait(str);
			UCase(str);
			Oprint("\r\n");
        	if(str[0]=='Y')
			{
				sprintf(str,"`!'Here are %ld Morph Credits,' says Bleep.\r\n",Va);
            	Oprint(str);
            	Money+= Va;
            	Weapon = '0';
			}
        	else
            	Oprint("`2 You decide to keep your weapon.\r\n");
		}
	}

	if(l == 10)
	{
    	if(v[0]=='B')
			t=13;
    	if(v[0]=='R')
			t=9;
    	if(v[0] == 'S' && Defense == '0')
		{
        	Oprint("`@ You don't have a Defense right now...");
        	t = -2;
		}
    	if(v[0] == 'S' && Defense != '0')
		{
        	t = -2;
        	Oprint("\r\n\r\n");
        	Oprint("`@ 'I am willing to pay you half for your Defensive item... Agreed?'\r\n");
        	Tx = WepNum(Defense);
			Va = WepCost[Tx] / 2;
			sprintf(str,"`4He offers you %ld Morph Credits for your %s. Accept? (Y/N) ",Va,DefName[Tx]);
        	Oprint(str);
        	SetColor(12, 0, 0);
        	Owait(str);
			UCase(str);
			Oprint("\r\n");
        	if(str[0] == 'Y')
			{
				sprintf(str,"`!'Here are %ld Morth Credits,' says Bleep.\r\n",Va);
            	Oprint(str);
            	Money += Va;
            	Defense = '0';
			}
			else
            	Oprint("`2 You decide to keep your Defense\r\n");
		}
	}


	if(l == 13 || l == 14)
	{
    	if(v[0] == 'Q' && l == 13)
			return(10);
    	if(v[0] == 'Q' && l == 14)
			return(11);
    	if(v[0] < 'A' || v[0] > 'L')
			return(0);
		pp1 = WepCost[WepNum(v[0])];
		gg1 = Money;
    	if(pp1 > gg1)
		{
        	if(l == 13)
            	Oprint("`6 'You can't afford it' says Blip\r\n");
        	if(l == 14)
            	Oprint("`6 'You can't afford it' says Bleep\r\n");
        	return(-2);
		}
    	if(l == 13 && Defense != '0')
		{
        	Oprint("`6 You already have a defense.\r\n");
        	return(-2);
		}
    	if(l == 14 && Weapon != '0')
		{
        	Oprint("`6 You already have a weapon.\r\n");
        	return(-2);
		}
    	if(l == 14)
			Weapon = v[0];
		else
			Defense = v[0];
    	Oprint("`! 'Sincere Thanks,' says ");
		Oprint(l==14?"Bleep":"Blip");
		Oprint(" as you pay for it.\r\n\r\n");
    	gg1 = gg1 - pp1;
    	Money = gg1;
    	PressAnyKey();
    	if(l == 13)
			t = 10;
		else
			t = 11;
	}

	if(l == 12)
	{
    	t = BuyStuff(v, "`2  Fred shakes his metal head and reminds you not to waste his time.");
    	if(t == -99)
			t = 9; // 'that's the code for "exit from this room." :)
	}
	return(t);
}

void ListWeps(short e)
{
	int x;
	char str[256];
	strcpy(Prompt, "ABCDEFGHIJKLQ");
	for(x=1;x<=12;x++)
	{
		sprintf(str,"`7              (`!%c`8) ",64+x);
    	Oprint(str);
		sprintf(str,"`@%8ld:",WepCost[x]);
    	Oprint(str);
    	if(e == 1)
			sprintf(str,"`0 %s\r\n",WepName[x]);
		else
			sprintf(str,"`0 %s\r\n",DefName[x]);
		Oprint(str);
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

void ShowRoom(long l)
{
	char str[256];

	// 'code to print "headers"
	Oprint("`!T`3ime `!P`3ort `8Version `7");
	Oprint(Version);
	Oprint(" -- `7Location: ");
	if(l == 4)
		Oprint("`%Outside the Hanger\r\n");
	if(l == 7)
		Oprint("`%Near a Tall Building\r\n");
	if(l == 8)
		Oprint("`%Fighting Mutants\r\n");
	if(l == 9)
		Oprint("`%With the Robots\r\n");
	if(l == 10)
		Oprint("`%Talking to Blip\r\n");
	if(l == 11)
		Oprint("`%Talking to Bleep\r\n");
	if(l == 12)
		Oprint("`%Buying from Fred\r\n");
	if(l == 13)
		Oprint("`%Buying from Blip\r\n");
	if(l == 14)
		Oprint("`%Buying from Bleep\r\n");

	Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
	if(l != 13 && l != 14)
	{
		sprintf(str, "room%ld.%s", l, NotAgain[14]==' '?"ans":"asc");
        DisplayTitle(str);
	}

	if(l == 4) // 'outside the hanger
	{
    	strcpy(Prompt, "RLS");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	Oprint("`7  (`%R`8)`@eturn to the Hanger           ");
    	Oprint("`7  (`!L`8)`3ook around a while\r\n");
    	Oprint("`7  (`!S`8)`3outh, away from the Hanger    ");
	}

	if(l == 7) // 'outside a building
	{
    	strcpy(Prompt, "SEN");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	Oprint("`7  (`%N`8)`@orth, toward the hanger       ");
    	Oprint("`7  (`!E`8)`3xamine the water\r\n");
    	Oprint("`7  (`!S`8)`3outh, closer to the building  ");
	}

	if(l == 9) // 'outside with Blip, Bleep, and Bloop
	{
    	strcpy(Prompt, "WESDN");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	Oprint("`7  (`%N`8)`@orth, away from the building  ");
    	Oprint("`7  (`!W`8)`3eapons with Bleep\r\n");
    	Oprint("`7  (`!D`8)`3efenses with Blip             ");
    	Oprint("`7  (`!O`8)`3ther Items with Fred\r\n");
    	Oprint("`7  (`!E`8)`3nter the building             ");
	}

	if(l == 8) //'fighting mutants
	{
    	strcpy(Prompt, "RA");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	Oprint("`7  (`!A`8)`3ttack the Mutant  ");
    	Oprint("`7  (`%R`8)`@un from the Attacker  ");
		sprintf(str,"`6   (`2%d`6 Hours remaining)\r\n",ClosFight);
    	Oprint(str);
	}

	if(l == 10 || l == 11) //'Talking to Blip the Defense dealer (Bloop)
	{
    	strcpy(Prompt, "BSR");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	if(l == 10) 
			Oprint("`7  (`!B`8)`3uy a Defense       `7  (`!S`8)`3ell your Defense       `7  (`%R`8)`@eturn\r\n");
    	if(l == 11) 
			Oprint("`7  (`!B`8)`3uy a Weapon        `7  (`!S`8)`3ell your Weapon        `7  (`%R`8)`@eturn\r\n");
	}

	if(l == 13)
	{
		Oprint("`5 Blip motions toward an obscure area around the building to show you\r\n");
		Oprint("`5 the array of defensive items he has for sale.\r\n");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
		ListWeps(0);
	}

	if(l == 14)
	{
    	Oprint("`5 Bleep prompts you to follow him to a little spot around the building\r\n");
    	Oprint("`5 where he proudly displaces his selection of weapons.\r\n");
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
    	ListWeps(1);
	}

	if(l == 12)
	{
		Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
		sprintf(str,"1  %c  ",LineRest[0]);
    	LoadItems(str);
    	strcpy(Prompt, "210");
    	ListItems(1);
	}

	if(l != 8 && l != 12 && l != 10 && l != 11 && l != 12 && l != 13 && l != 14)
    	Oprint("`7  (`$?`8)`3Re-List menu options\r\n");
	Oprint("`4-`5=`4-`5=`4----`5=`4---------`5=`4---------------------------------------`5=`4---------`5=`4----`5=`4-`5=`4-\r\n");
	if(l != 8)
	{
		sprintf(str,"`2  You have `0%ld`2 Morph Credits on hand, and `0%ld`2 in the Time Bank.\r\n",Money,Stash);
    	Oprint(str);
	}
}

void UseItem(long c, short b) // 'c=1-8 item number, b=the item code (1-???)
{
	long x1;
	int x;
	char str[256];

	if(b == 13)
	{
    	Oprint("`0 You can't fly from here...");
    	return;
	}

	if(b == 7) // 'trying to use a shovel
	{
    	if(LineRest[11]!=' ')
		{
        	Oprint("`2 You've already tried to dig today.  Try again tomorrow.\r\n");
        	return;
    	}
		LineRest[11]='?';
    	if(random()%2)
		{
        	Oprint("`2 You dig here but don't find anything worth while.\r\n");
        	return;
		}
    	x = random()%3+1;
    	if(x == 1)
		{
        	Oprint("`0 You dig for a while, and unearth a Long Board\r\n");
        	SaveInvItem(c, "Long Board", 0, 10);
		}
    	if(x == 2)
		{
        	Oprint("`0 Your digging turns up a Huge Feather.\r\n");
        	SaveInvItem(c, "Huge Feather", 0, 11);
		}
    	if(x == 3)
		{
        	Oprint("`0 You dig around and find some Light Fabric.\r\n");
        	SaveInvItem(c, "Light Fabric", 0, 12);
		}
    	Oprint("`2 You discard the shovel and keep your newfound treasure.\r\n");	
		return;
	}

	if(inv[c][2] == 5 && Room == 7) // 'you have the fishing pole & use it
	{
    	Oprint("`2 You cast out into the murky pool of water and hope to catch a fish.\r\n");
    	if(NotAgain[11]!=' ')
		{
        	Oprint("`2 You get a few nibbles, but aren't able to catch anything.\r\n");
        	return;
		}
    	NotAgain[11]='@';
    	x = random()%4+1;
    	if(x != 1) // 'catch some money
		{
        	x1 = GetValue(GoldFromEach[Level] * 3, .25, .75);
			sprintf(str,"`0 You snag a credit disc worth %ld Morph Credits!\r\n",x1);
        	Oprint(str);
        	Money += x1;
        	return;
		}
    	Oprint("`! You feel a strong tug on the line, and you pull out a large, frisky fish.\r\n");
    	Oprint("`3 Unfortunately, in the struggle to hold on to the fish, you loose your pole.\r\n");
    	SaveInvItem(c, "Fresh Fish", 0, 6);
		sprintf(str,"`@%s`6 caught a fish in a pool outside the Hanger.",Alias);
    	WriteNews(str, 1);
    	return;
	}
	Oprint("`2 You find no use for that item here at this time.\r\n");

	// 'SaveInvItem c%, "-------------------", 0, 0
}

