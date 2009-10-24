
/*

Copyright 2007 Jakob Dangarden

 This file is part of Usurper.

    Usurper is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Usurper is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Usurper; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


// Usurper - Bobs Beer
#include <math.h>

static char *name=config.bobsplace;
static const char *expert_prompt="(B,C,D,T,S,R,?)";

struct opponent {
	const char	*name;
	int			cstam;	// -1 indicates same as player...
	bool		male;
	const char	*type;
	const char	*ok;
	const char	*no;
};

const struct opponent opponents[32] = {
	{
		"Mighty John",
		-1,
		true,
		"human",
		"Ok, but DON'T mess with me again!",
		"Haha! Prepare to fight!",
	},
	{
		"Mary Joe",
		-1,
		false,
		"mutant",
		"Alright, run and hide WIMP!",
		"Oh no...you'+chr(39)+'re gonna take what'+chr(39)+'s coming to ya!",
	},
	{
		"Hobbes",
		250,
		true,
		"dwarf",
		"We all make mistakes!",
		"Grrrrr!",
	},
	{
		"Sony Blue",
		56,
		true,
		"elf",
		"Run loser!",
		"No way! Stand and fight!",
	},
	{
		"Rex",
		100,
		true,
		"mutant",
		"Kiss my leather boots too!",
		"To late sucker!",
	},
	{
		"Sandra",
		70,
		false,
		"human",
		"Next time I'+chr(39)+'ll mess ya up bad honey!",
		"Fistfight!",
	},
	{
		"Nick",
		15,
		true,
		"orc",
		"Get out of here before I change my mind!",
		"I need practice! hehe!",
	},
	{
		"Banco",
		350,
		true,
		"troll",
		"Hohoho...Get lost WORM!",
		"You have crossed the line punk!",
	},
	{
		"Holly",
		40,
		true,
		"human",
		"Get lost scum!",
		"I'+chr(39)+'m gonna trash you to pieces!",
	},
	{
		"Soufie",
		30,
		false,
		"gnoll",
		"Get lost goofy!",
		"Too late for nice talk!",
	},
	{
		"Fat Pa",
		219,
		true,
		"troll",
		"Get lost creep!",
		"Hehehehe! NO WAY!",
	},
	{
		"Ma Teng",
		45,
		true,
		"mutant",
		"Apology accepted!",
		"BUTT-KICKING TIME!",
	},
	{
		"Lucky",
		65,
		false,
		"hobbit",
		"Ssssss...",
		"Nope! Stand up and fight!",
	},
	{
		"Oneeyed Rose",
		30,
		false,
		"dwarf",
		"Hahaha...get lost COWARD!",
		"BAHAHA! I don'+chr(39)+'t think you realize who I am!",
	},
	{
		"Fat Irma",
		50,
		false,
		"human",
		"Mehehe!! Crawl back mama before you get into trouble!",
		"Yihhaaa! I need a sandbag like you to train with!",
	},
	{
		"Sherkh",
		50,
		true,
		"gnome",
		"Ok.",
		"No way partner! Time to slug it out with me!",
	},
	{
		"Nitt",
		15,
		true,
		"gnoll",
		"Coward!",
		"Haha! Fight me instead!!",
	},
	{
		"Ehsan Kahn",
		95,
		true,
		"human",
		"Run along then!",
		"No, we must fight!",
	},
	{
		"Blue",
		95,
		false,
		"Half-elf",
		"Grrrrr!",
		"Grrrrr!",
	},
	{
		"Highlord",
		125,
		true,
		"Half-elf",
		"I accept your apology!",
		"I'+chr(39)+'m afraid it'+chr(39)+'s too late for that!",
	},
	{
		"Sbort Afsghar",
		162,
		true,
		"orc",
		"Alright, run along my friend!",
		"Mohahaa! NO WAY!",
	},
	{
		"Valin Junior",
		262,
		true,
		"dwarf",
		"Go in peace!",
		"You are DEAD!",
	},
	{
		"Marty",
		35,
		true,
		"mutant",
		"Get lost hairbag!",
		"FIGHT OR PERISH!",
	},
	{
		"Trudi",
		55,
		false,
		"hobbit",
		"You were lycky this time...",
		"My name is TRUDI! I love fighting!",
	},
	{
		"Conway",
		155,
		true,
		"human",
		"It'+chr(39)+'s your lucky day!",
		"Fight!",
	},
	{
		"Morgan Razorblade",
		175,
		true,
		"orc",
		"Escape while you can...",
		"Your subordinance must come to an end!",
	},
	{
		"Mufti",
		40,
		true,
		"mutant",
		"Get lost loser!",
		"You must be punished!",
	},
	{
		"Carla",
		100,
		false,
		"elf",
		"Get out of my face!",
		"Bah! Liar!",
	},
	{
		"Mino",
		15,
		true,
		"half-elf",
		"Oh my! Getting nervous, are we?",
		"Let'+chr(39)+'s solve this problem with our fists!",
	},
	{
		"Rooney",
		75,
		true,
		"mutant",
		"Sissy!",
		"Pay the price for your rudness!",
	},
	{
		"Kaya the Evil",
		275,
		false,
		"troll",
		"You get away this time!",
		"Traitor! You must be punished!",
	},
	{
		"Crazy Moe",
		130,
		true,
		"human",
		"Mehehe!! Crawl back mama before you get into trouble!",
		"Yihhaaa! I need a sandbag like you to train with!",
	},
};
		
static void Meny(void)
{
	newscreen();
	pbreak();
	HEADER("-*- ",config.bobsplace, " -*-");
	pbreak();
	TEXT("This place is the rendezvous for every lowlife in town. "
			"Unexperienced characters should not enter here alone. "
			"If you are looking for trouble, here it is! "
			"As you enter the hut, there is a short moment of silence. "
			"But things soon get back to normal when your entrance "
			"has been noticed.");
	pbreak();
	menu2("(B)rawl      ");
	menu("(C)ompete in Beer Drinking");
	menu2("(D)ance      ");
	menu("(T)hievery");
	menu("(S)tatus");
	menu("(R)eturn");
}

static char Shakedown_Move(char ch)
{
	if(ch=='?') {
		pbreak();
		EVENT("Your move?");
		menu(Asprintf("(G)ive them your cash (%s %s %s)", commastr(player->gold), config.moneytype, player->gold==1?config.moneytype2:config.moneytype3);
		menu("(D)efend yourself!");
	}

	return(toupper(gchar()));
}

static void Shakedown(void)
{
	char ch;
	struct monster	monster[5];
	struct monster	*monsters[6]={&monster[0], &monster[1], &monster[2], &monster[3], &monster[4], &monster[5], NULL};

	BAD("SHAKEDOWN!");
	TEXT("A bunch of rough characters suddenly appear from the shadows. "
			"The stranger smirks and pulls out a long knife from his robes!");
	ch='?';

	do {
		ch=Shakwdown_Move(ch);
	} while(strchr("DG", ch)==NULL);

	switch(ch) {
		case 'D':
			strcpy(monster[0].name, "thug");
			sprintf(monster[0].phrase, "Give us your %s!", config.moneytype);
			strcpy(monster[0].weapon, "Warp dagger");
			monster[0].defence=50;
			monster[0].grabweap=false;
			monster[0].grabarm=false;
			monster[0].poisoned=false;
			monster[0].disease=false;
			monster[0].punch=500;
			for(i=1; i<5; i++) {
				memcpy(&monster[i], &monster[0], sizeof(monster[0]));
			}
			for(i=0; i<5; i++) {
				monster[0].hps=player->hps - rand_num(25);
				if(monster[i].hps < 20)
					monster[i].hps=20+rand_num(10);
				monster[0].strength=player->strength-rand_num(25);
				if(monster[i].strength < 20)
					monster[i].strength=20+rand_num(10);
				monster[0].weappow=player->level*50;
				monster[0].armpow=player->level*50;
			}
			strcpy(monster[0].name, "Thug Leader");
			strcpy(monster[0].weapon, "Evil dagger");
			monster[0].hps += rand_num(200);
			monster[0].punch=70;
			global_begged=false;
			global_nobeg=true;
			player_vs_monsters(Pl_Vs_DoorGuards, monsters, NULL);
			if(player->hps > 0) {
				for(i=0; i<12)
					player->spell[i][2]=false;
				GOOD("GOOD WORK!");
				TEXT("The streets are a bit safer now!");

				newsy(true, "Cleaner", 
						Asprintf(" %s%s%s fought a band of rogues outside %s!", config.plycolor, player.name2, config.textcol1, config.bobsplace), 
						Asprintf(" %s%s%s was victorious!",  config.plycolor, player.name2, config.textcol1),
						NULL);
			}
			else {
				player->hps=0;
				player->gold=0;
				newsy(true, "Dead end!", 
						Asprintf(" %s%s%s was tricked in %s. The stubborn fool refused", config.plycolor, player.name2, config.textcol1, config.bobsplace), 
						Asprintf(" to give up %s %s to a band of thugs.",  sex3[player->sex], config.moneytype),
						Asprintf(" %s%s%s was killed!", config.plycolor, player.name2, config.textcol1), 
						NULL);

				// Mail player
				post(MailSend, player->name2, player->ai, false, mailrequest_nothing(),
						"",
						Asprintf("%sLast breath%s", umailheadc, config.textcol1),
						mkstring(11,196),
						Asprintf("You were killed by a band of thieves outside %s.", config.bobsplace),
						Asprintf("They took all your %s.", config.moneytype),
						"You ended up with a dagger in the back...",
						NULL);

				TEXT("You are dead....!");
				TEXT(Asprintf("Kill your precious %s goodbye!", config.moneytype));
				normal_exit();
			}
			break;
		case 'G':
			player->gold=0;
			pbreak();
			DLC(config.badcolor, "\"Wise decision peasant!\"", config.textcolor, ", the leader of the scoundrels says as you hand over the ", config.textcolor, config.moneytype, config.textcolor, ".");
			mswait(300);
			TEXT("Suddenly you are alone in the alley. You think for yourself that "
					"perhaps you should have fought those men. Would it been worth it?");
			upause();
			break;
	}
}

static void Vitamin_Offer(void)
{
	int x=player->level*10000;

	TEXT("The stranger offers you a vitamine cure which he claims could enhance your strength. The man wants ", commastr(x), " ", config.moneytype, " ", config.moneytype3, " for the cure.");
	if(player->gold < x) {
		TEXT("When you explain that you don't have the ", config.moneytype, " he is asking"
				"the stranger leaves.");
	}
	else {
		pbreak();
		if(confirm("Buy a bottle", 'N')) {
			decplayermoney(player, x);
			TEXT("You hand over the ", config.moneytype, " and recieve a bottle filled with something which looks like syrup......");
			mswait(800);
			pbreak();
			d(config.talkcol, "Down the hatch!");
			mswait(1000);
			pbreak();
			pbreak();
			x=3+rand_num(3);
			DLC(config.textcolor, "You wake up a short moment later. You are laying in the "
						"alley. All your belongings are left untouched. "
						"Your strength went up with ",
					config.goodcolor, commastr(x);
					config.textcolor, " !");
			upause();
		}
		else {
			GOOD("NO THANKS!");
		}
	}
}

static void Teleport_UMAN(void)
{
	int	x=player->level*1500;

	DLC(config.textcolor, "The stranger says that he will guide you to the UMAN caves for only ",
			config.moneycolor, commastr(x),
			config.textcolor, " ",
			config.textcolor, config.moneytype,
			config.textcolor, " ",
			config.textcolor, config.moneytype3);
	if(player->gold < x) {
		TEXT("When you explain that you don't have the ", config.moneytyp, " he is asking "
				"the stranger leaves.");
	}
	else {
		decplayermoney(player, x);
		pbreak();
		TEXT("You hand over the ", config.moneytype, " and start following the stranger...");
		pbreak();
		player->auto_probe=UmanCave;
	}
}

static void Man_In_Robes(void)
{
	// strange man i robes approaches!
	if(rand_num(5)==0 && player->gold > 1000) {
		pbreak();
		EVENT("You are approached by a stranger in robes!");
		GOOD("He offers you to see some fine wares he has in store.");
		pbreak();
		if(confirm("Follow the stranger outside ",'N')) {
			// Update player location
			onliner.location=ONLOC_BobThieves;
			onliner.doing=location_desc(onliner.location);
			pbreak();
			TEXT("You follow the stranger out into the nearby alley...");
			mswait(800);
			pbreak();
			switch(rand_num(3)) {
				case 0:
					Shakedown();
					break;
				case 1:
					Vitamin_Offer();
					break;
				case 2:	// Teleport to UMAN cave
					Teleport_UMAN();
					break;
			}
		}
		else {
			pbreak();
			GOOD("NO THANKS!");
		}
	}
}

void Bobs_Inn(void)
{
	struct opponent opp;
	double	rr;

	do {
		if(player->auto_probe != NoWhere)
			break;
		if(rand_num(2)==0 && player->darknr > 0) {	// spaghetti incident?
			pbreak();
			pbreak();
			player->darknr--;
			D(YELLOW, "Insulted !?");

			// discovery
			memcpy(opp, opponents[rand_num(sizeof(opponents)/sizeof(opponents[0]))], sizeof(opp));
			if(opp.cstam==-1)
				opp.cstam=player->stamina;
			switch(rand_num(5)) {
				case 0:
					TEXT("A ", opp.type, " taps you gently on your head!");
					break;
				case 1:
					TEXT("A ", opp.type, " spills some beer on your clothes!");
					break;
				case 2:
					TEXT("A ", opp.type, " laughs at your clothes!");
					break;
				case 3:
					TEXT("A ", opp.type, " spits in your hair!");
					break;
				case 4:
					TEXT("A ", opp.type, " pukes at your clothes!");
					break;
			}

			switch(rand_num(3)) {
				case 0:
					TEXT("Many guests laugh at your !");
					break;
				case 1:
					TEXT("There is a sudden silence in the room!");
					break;
				case 2:
					TEXT("Everybody is looking your way!");
					break;
			}

			if(!confirm("Are you gonna put up with this treatment",'Y')) {
				pbreak();
				D(YELLOW, "Yeah! REVENGE!");
				pbreak();
				TEXT("You walk up to the ", config.plycolor, opp.type, config.textcol1, " and tap ", opp.mail?"him":"her", " on the shoulder...");
				upause();
				TEXT("The ", opp.type, " turns to you and says : ");
				switch(rand_num(6)) {
					case 0:
						SAY("  What is it WORM?");
						break;
					case 1:
						SAY("  Wanna make something out of this?");
						break;
					case 2:
						SAY("  Do you want to say something?");
						break;
					case 3:
						SAY("  Yeah?");
						break;
					case 4:
						SAY("  Ohhh! I am really SCARED!  Hahaha!");
						break;
					case 5:
						SAY("  What do you want ", race_display(2, player->race, 0),"?");
						break;
				}
				pbreak();
				DLC(config.textcolor, "It's ", config.plycolor, opp.name, " that you have run across!");
				TEXT(" Your chances : ");

				pstam = player->stamina;
				cstammax = opp.cstam
				reward = cstam*100+rand_num(500);
				diff=cstam-pstam;

				a2="YOU ARE MAD!";

				if(diff < 100)
					a2="Don't even think about it!";
				if(diff < 90)
					a2="The chances are VERY VERY small!";
				if(diff < 80)
					a2="You're gonna get messed up!";
				if(diff < 70)
					a2="No way you can win this!";
				if(diff < 60)
					a2="Are you mad?";
				if(diff < 50)
					a2="Feel lucky punk?";
				if(diff < 40)
					a2="Risky!";
				if(diff < 30)
					a2="Can get rough...";
				if(diff < 20)
					a2="You can't lose!";
				if(diff < 10)
					a2="Almost too easy...";
				if(diff < 0)
					a2="Piece of Cake!";
				if(diff < -10)
					a2="You can do it blindfolded!";

				DL(MAGENTA, a2);
				pbreak();

				TEXT("Make your move :");
				menu("(E)lbow in the face");
				menu("(S)traight punch in the stomach");
				menu("(T)rip the scumbag");
				menu("(B)ite ear");
				menu(Asprintf("(A)pologize for disturbing %s", opp.name);
				PART(":");

				do {
					ch=toupper(gchar());
				} while(strchr("ESTBA", ch)==NULL);

				if(ch=='A') {
					if(rand_num(2)==0) {
						pbreak();
						D(YELLOW, "\"", opp.no, "\"");
					}
					else {
						pbreak();
						D(YELLOW, "\"", opp.ok, "\"");
						upause();

						break;
					}
				}

				pbreak();

				if(cstam < 5)
					cstam=5;

				switch(ch) {
					case 'E':
						if(rand_num(2)==0) {
							DL(BRIGHT_RED, "Bullseye! Your elbow almost crushed ", opp.male?"his":"her", " nose!");
							cstam -= rand_num(3);
						}
						else
							DLC(config.plycolor, opp.name, config.textcolor, " managed to avoid your attack!");
						break;
					case 'S':
						if(rand_num(2)==0) {
							DLC(BRIGHT_RED, "Dynamite! ", config.plycolor, opp.name, config.textcolor, " staggers from your unexpected attack!");
							cstam -= random(3);
						}
						else
							DLC(config.plycolor, opp.name, config.textcolor, " saw that one coming!");
						break;
					case 'T':
						if(rand_num(2)==0) {
							DL(BRIGHT_RED, "Success! ", opp.name, " falls heavily to the floor!");
							cstam -= random(3);
						}
						else
							TEXT(opp.name, " managed to retreat from your assault!");
						break;
					case 'S':
						if(rand_num(2)==0) {
							DLC(config.plycolor, opp.name, BRIGHT_RED, " screams loud when you get a bit of ", BRIGHT_RED, opp.male?"his":"her", BRIGHT_RED, "ear!");
							cstam -= random(3);
						}
						else
							DLC(config.plycolor, opp.name, BRIGHT_RED, "  managed to avoid your clumsy attack!");
						break;
				}

				do {
					do {
						ch='?';

						rr=pstam/player->stamina;
						xx=round(rr*100);

						a="Excellent Health";
						if(xx<100)
							a="You have some small wounds and bruises";
						if(xx<80)
							a="You have some wounds";
						if(xx<70)
							a="You are pretty hurt!";
						if(xx<50)
							a="You are in a bad shape!";
						if(xx<30)
							a="You have some BIG and NASTY wounds!";
						if(xx<20)
							a="You are in a awful condition!";
						if(xx<5)
							a="You are bleeding from wounds ALL over your body!";
						pbreak();
						DLC(CYAN, "Your status : ", BRIGHT_RED, a);

						rr=cstam/cstammax;
						xx=round(rr*100);

						a="Excellent Health";
						if(xx<100)
							a="Small wounds and bruises";
						if(xx<80)
							a="Some wounds";
						if(xx<70)
							a="Pretty hurt!";
						if(xx<50)
							a="Bad condition!";
						if(xx<30)
							a="Have some BIG and NASTY wounds!";
						if(xx<20)
							a="Is in a awful condition!";
						if(xx<5)
							a="AWFUL CONDITION!";
						DLC(config.plycolor, opp.name, CYAN, " status : ", BRIGHT_RED, a);
						pbreak();

						DC(config.textcolor, "Action (", config.textcolor2, "?", config.textcolor, " for moves) :");

						do {
							ch=toupper(gchar());
						} while((ch < 'A' || ch > 'N') && ch != '?');

						pbreak();
						
						if(ch=='?') {
							char chstr[2];

							chstr[1]=0;
							pbreak();
							for(i=0; i<14; i++) {
								char	line[80];
								sprintf(line, ") %-32s %s", bash_name(i), bash_rank(player->skill[i]));
								DLC(MAGENTA, chstr, config.textcolor, line);
							}
						}
					} while(ch <'A' || ch > 'N');

					pbreak();
					switch(ch) {
						case 'A':
							pbreak();
							DLC(config.textcolor, "Here goes...You take run and then charge ", config.plycolor, opp.name, config.textcolor, " and try a tackle!");
							if(rand_num(hitchance(player->skill[ch-'A']))!=0) {
								DLC(config.plycolor, s.name, copnfig.textcolor, "  evade your attack! You stumble past ", config.textcolor, opp.male?"him":"her");

      if ch='?' then begin
       crlf;
       for i:=1 to 14 do begin
        sd(5,chr(64+i));
        if length(bash_name(i))<>32 then begin
         a^:='                        ';
         x:=length(bash_name(i));
         x:=18-x;
         sd(config.textcolor,') ');
         sd(global_bashcol,bash_name(i)+copy(a^,1,x));
         sd(config.textcolor,bash_rank(player.skill[i]));
        end;
        crlf;
       end;
      end;
     until ch in ['A'..'N'];

     crlf;
     case ch of
     'A':begin
         crlf;
         d(config.textcolor,'Here goes...You take run and then charge '+uplc+s^+config.textcol1+' and try a tackle!');
         if random(hitchance(player.skill[1]))<>0 then begin
          if male=true then d(global_plycol,s^+config.textcol1+' evade your attack! You stumble past him!')
          else d(global_plycol,s^+config.textcol1+' evade your attack! You stumble past her!');
         end
         else begin
          d(config.textcolor,'Your tackle hit '+uplc+s^+config.textcol1+' VERY hard!');
          d(global_plycol,s^+config.textcol1+' lose sense of all directions!');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     'B':begin
         d(config.textcolor,'BANZAI! You rush '+uplc+s^+config.textcol1+' and try a Drop-Kick!');
         if random(hitchance(player.skill[2]))<>0 then begin
          if male=true then d(global_plycol,s^+config.textcol1+' simply jumps aside, and you fly past him!')
          else d(global_plycol,s^+config.textcol1+' simply jumps aside, and you fly past her!');
         end
         else begin
          d(config.textcolor,'Your kick hit '+uplc+s^+config.textcol1+' hard!');
          if male=true then d(config.textcolor,'He goes down!')
          else d(config.textcolor,'She goes down!');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     'C':begin
         d(config.textcolor,'You try an uppercut against '+uplc+s^+config.textcol1+'!');
         if random(hitchance(player.skill[3]))<>0 then begin
          d(global_plycol,s^+config.textcol1+' elegantly blocks your assault!');
         end
         else begin
          d(config.textcolor,'You hit '+uplc+s^+config.textcol1+' right on the chin!');
          if male=true then d(config.textcolor,'He goes down!')
          else d(config.textcolor,'She goes down!');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     'D':begin
         d(config.textcolor,'You trow yourself against '+uplc+s^+config.textcol1+' with a fearsome battlecry!');
         if random(hitchance(player.skill[4]))<>0 then begin
          d(config.textcolor,'But '+uplc+s^+config.textcol1+' elegantly avoid your clumsy assault!');
         end
         else begin
          if male=true then d(global_plycol,s^+config.textcol1+' screams in horror as you bite him hard!')
          else d(global_plycol,s^+config.textcol1+' screams in horror as you bite her hard!');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     'E':begin
         d(config.textcolor,'You try a Leg-Sweep against '+uplc+s^+config.textcol1+'!');
         if random(hitchance(player.skill[5]))<>0 then begin
          d(config.textcolor,'But '+uplc+s^+config.textcol1+' manage to avoid the assault!');
          crlf;
         end
         else begin
          if male=true then d(global_plycol,s^+config.textcol1+' falls heavily to the floor as you sweep away his legs!')
          else d(global_plycol,s^+config.textcol1+' falls heavily to the floor as you sweep away her legs!');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     'F':begin
         d(config.textcolor,'You try to get a good grip on '+uplc+s^+config.textcol1+'!');
         if random(hitchance(player.skill[6]))<>0 then begin
          d(config.textcolor,'But '+uplc+s^+config.textcol1+' blocks your attack!');
         end
         else begin
          if male=true then d(global_plycol,s^+config.textcol1+' screams in pain as you crack a bone in his body! Hehe!')
          else d(global_plycol,s^+config.textcol1+' screams in pain as you crack a bone in her body! Hehe!');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     'G':begin
         d(config.textcolor,'You let your right hand go off in an explosive stroke against '+uplc+s^+config.textcol1+'!');
         if random(hitchance(player.skill[7]))<>0 then begin
          d(config.textcolor,'But '+uplc+s^+config.textcol1+' blocks your attack!');
         end
         else begin

          if male=true then d(global_plycol,s^+config.textcol1+' looks surprised when your punch hit him hard in the chest!')
          else d(global_plycol,s^+config.textcol1+' looks surprised when your punch hit her hard in the chest!');
          if male=true then d(config.textcolor,'He goes down oh his knees, moaning!')
          else d(config.textcolor,'She goes down oh his knees, moaning!');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     'H':begin
         d(config.textcolor,'You send off a stroke against your foe!');
         if random(hitchance(player.skill[8]))<>0 then begin
          d(config.textcolor,'But '+uplc+s^+config.textcol1+' avoids your attack with elegance!');
          d(config.textcolor,'Instead you collect a counterattack against your upper body!');
          cstam:=cstam-(random(4)+4);
         end
         else begin
          d(global_plycol,s^+config.textcol1+' is stunned from your effective punch!');
          if male=true then d(config.textcolor,'He goes down!')
          else d(config.textcolor,'She goes down!');
          cstam:=cstam-(random(4)+4);
          stunned2:=true;
         end;
         end;
     'I':begin
         d(config.textcolor,'You try to get your hands around '+uplc+s^+'s'+config.textcol1+' throat!');
         if random(hitchance(player.skill[9]))<>0 then begin
          d(config.textcolor,'But '+uplc+s^+config.textcol1+' takes a couple of ballet steps, and avoid your attack!');
         end
         else begin
          if male=true then d(global_plycol,s^+'s'+config.textcol1+' face turns red when he chokes!')
          else d(global_plycol,s^+'s'+config.textcol1+' face turns red when she chokes! (hehe!)');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     'J':begin
         d(config.textcolor,'You try a Headbash against '+uplc+s^+config.textcol1+'!');
         if random(hitchance(player.skill[10]))<>0 then begin
          d(config.textcolor,'But '+uplc+s^+config.textcol1+' quickly takes a few steps back!');
         end
         else begin
          d(config.textcolor,'You hit '+uplc+s^+'s'+config.textcol1+' head with a massive blow!');
          if male=true then d(config.textcolor,'He goes down bleeding from some nasty wounds!')
          else d(config.textcolor,'She goes down bleeding from some nasty wounds!');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     'K':begin
         d(config.textcolor,'You try to grab hold of '+uplc+s^+'s'+config.textcol1+' hair!');
         if random(hitchance(player.skill[11]))<>0 then begin
          d(config.textcolor,'But '+uplc+s^+config.textcol1+' manages to evade your attack!');
         end
         else begin
          if male=true then d(config.textcolor,'You get a firm grip of his hair! You drag him around the room!')
          else d(config.textcolor,'You get a firm grip of her hair! You drag her around the room!');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     'L':begin
         d(config.textcolor,'You try a Kick against '+uplc+s^+config.textcol1+'!');
         if random(hitchance(player.skill[12]))<>0 then begin
          d(config.textcolor,'But '+uplc+s^+config.textcol1+' blocks it!');
          pause;
         end
         else begin
          if male=true then d(config.textcolor,'He staggers under your blow!')
          else d(config.textcolor,'She staggers under your blow!');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     'M':begin
         d(config.textcolor,'You try a Punch against '+uplc+s^+config.textcol1+'!');
         if random(hitchance(player.skill[13]))<>0 then begin
          d(config.textcolor,'But '+uplc+s^+config.textcol1+' blocks it!');
         end
         else begin
          if male=true then d(config.textcolor,'You hit him right on the chin! He is dazed!')
          else d(config.textcolor,'You hit her right on the chin! Se is dazed!');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     'N':begin
         d(config.textcolor,'You rush '+uplc+s^+config.textcol1+' with your head lowered!');
         if random(hitchance(player.skill[14]))<>0 then begin
          if male=true then d(config.textcolor,'But he manages to jump aside just before you reach him!')
          else d(config.textcolor,'But she manages to jump aside just before you reach her!');
         end
         else begin
          if male=true then d(config.textcolor,'You hit '+uplc+s^+config.textcol1+' right in Solar-Plexus!'
                              +' You send him flying!')
          else d(config.textcolor,'You hit '+uplc+s^+config.textcol1+' right in Solar-Plexus! You send her flying!');
          cstam:=cstam-(random(4)+4);
         end;
         end;
     end;

     {enemy move}
     x:=random(14)+1;
     if male=true then a2^:='He'
                  else a2^:='She';

     case x of
      1: d(config.textcolor,a2^+' tries to tackle you!');
      2: d(config.textcolor,a2^+' tries a drop-kick against you!');
      3: d(config.textcolor,a2^+' tries an uppercut against your head!');
      4: d(config.textcolor,a2^+' tries to bite you in the leg!');
      5: d(config.textcolor,a2^+' goes for a leg-sweep against you!');
      6: d(config.textcolor,a2^+' tries to break your arm!');
      7: d(config.textcolor,a2^+' sends off a Nicehand, aimed at your chest!');
      8: d(config.textcolor,a2^+' sends off a nervepunch!');
      9: d(config.textcolor,'The bastard tries to strangle you!');
      10: d(config.textcolor,a2^+' goes for a Headbash! Watch it!');
      11: d(config.textcolor,a2^+' tries to pull your hair!');
      12: d(config.textcolor,a2^+' goes for a kick, aimed at your body!');
      13: d(config.textcolor,a2^+' goes for a straight punch in your face!');
      14: d(config.textcolor,a2^+' comes rushing at you in high speed! RAM!');
     end; {case .end.}

     y:=1;
     if pstam>5 then y:=y+1;
     if (player.class=Assassin) or (player.class=Jester) then y:=y+1;
     if player.agility>50 then y:=y+1;
     if player.agility>200 then y:=y+1;
     if player.agility>700 then y:=y+1;
     if player.agility>1300 then y:=y+1;
     if player.level>50 then y:=y+1;
     if player.level>95 then y:=y+1;

     if random(y)<2 then begin
      d(config.textcolor,'You didn''t manage to evade!');
      if x=8 then stunned1:=true;
      case x of
       1:d(config.textcolor,'The hard tackle sends you across the floor! You lose your breath!');
       2:d(config.textcolor,'You stagger under the impact from the kick!');
       3:d(config.textcolor,'The stroke lands neately on your chin! You see stars and moons!');
       4:begin
          if male=true then d(config.textcolor,'His bite goes deep in your leg! You are bleeding!')
          else d(config.textcolor,'Her bite goes deep in your leg! You are bleeding!');
         end;
       5:d(config.textcolor,'You fall heavily to the floor!');
       6:d(config.textcolor,'Cracck... Something just broke in your body! You are hurt bad!');
       7:begin
          if male=true then d(config.textcolor,'You lose your breath when his steelhand goes right in your chest!')
          else d(config.textcolor,'You lose your breath when her steelhand goes right in your chest!');
         end;
       8:d(config.textcolor,'The stroke hits a nerve! You are stunned!');
       9:begin
          if male=true then d(config.textcolor,'He got you in a chokeholt! You can''t breathe!')
          else d(config.textcolor,'She got you in a chokeholt! You can''t breathe!');
         end;
       10:d(config.textcolor,'BONK! You are feeling dizzy! Your head aches!');
       11:d(config.textcolor,'Arrgghhh! That really hurt! What a dirty trick!');
       12:d(config.textcolor,'The well placed kick lands right in solar-plexus! Ouch!');
       13:d(config.textcolor,'The punch lands right on your nose! You lose sense of all directions!');
       14:begin
           if male=true then d(config.textcolor,'His attack sends you sprawling! You fall to the floor!')
           else d(config.textcolor,'Her attack sends you sprawling! You fall to the floor!');
          end;
      end;
      pstam:=pstam-(random(6)+4);

     end;

     {fusk}
     {d(15,commastr(pstam));
     d(15,commastr(cstam));}

    until (pstam<=0) or (cstam<=0);

    if pstam<=0 then begin
     d(config.textcolor,'Your head starts to spin, and you feel darkness...');
     d(config.textcolor,'All your strength is gone!');
     d(config.textcolor,'Before you slide into unconsciousness you feel how you are being');
     d(config.textcolor,'frisked...');
     crlf;

     case random(2) of
      0:begin
         newsy(true,
         'Dork',
         ' '+uplc+player.name2+config.textcol1+' tried to show off at '+config.bobsplace+'.',
         ' '+sex2[player.sex]+' was knocked out after a short fight against '+uplc+s^+config.textcol1+', the '+ts^+'.',
         '',
         '',
         '',
         '',
         '',
         '',
         '');
        end;
      1:begin
         newsy(true,
         'Brawl at '+config.bobsplace+'!',
         ' '+uplc+player.name2+config.textcol1+' accepted a challenge in '+config.bobsplace+'.',
         ' The violence ended when '+uplc+player.name2+config.textcol1+' was knocked out',
         ' by '+uplc+s^+config.textcol1+', the '+ts^+'.',
         '',
         '',
         '',
         '',
         '',
         '');
        end;
     end;
     player.gold:=0;
     pause;

     post(MailSend,
     player.name2,
     player.ai,
     false,
     mailrequest_nothing,
     '',
     umailheadc+'Defeat!'+config.textcol1,
     mkstring(7,underscore),
     'You were knocked out in '+config.bobsplace+'!',
     'Better train some more before starting a brawl!',
     '',
     '',
     '',
     '',
     '',
     '',
     '',
     '',
     '',
     '',
     '');

     normal_exit;

    end
    else begin
     crlf;d(14,'Victory!');
     d(config.textcolor,'You have defeated '+s^+'!');
     sd(config.textcolor,'You frisk your foe and find ');
     sd(14,commastr(reward));
     sd(config.textcolor,' '+many_money(reward)+'!');crlf;

     xx:=player.level*(random(60)+10);
     sd(config.textcolor,'You also receive ');
     sd(14,commastr(xx));
     sd(config.textcolor,' experience points!');
     crlf; d(config.textcolor,'...');
     incplayermoney(player,reward);
     player.exp:=player.exp+xx;

     newsy(true,
     'Fistfighter!',
     ' '+uplc+player.name2+config.textcol1+' fought a staggering battle at '+config.bobsplace+'.',
     ' It turned out to be a new victory in '+sex3[player.sex]+' promising career!',
     '',
     '',
     '',
     '',
     '',
     '',
     '');

     pause;
    end;

   end
   else begin
    crlf;
    d(5,'You leave things as they are...');
    d(5,'Wise perhaps, but not very brave.');
    if player.charisma>5 then player.charisma:=player.charisma-random(3);
   end;
   crlf;
  end;

  if onliner.location<>onloc_bobsbeer then begin
   refresh:=true;
   onliner.location :=onloc_bobsbeer;
   onliner.doing    :=location_desc(onliner.location);
   add_onliner(OUpdateLocation,onliner);
  end;

  display_menu(true,true);

  ch:=upcase(getchar);

  case ch of
   '?':begin
        if player.expert=true then display_menu(true,false)
                              else display_menu(false,false);
       end;
   'S':begin
       clearscreen;
       Status(player);
       end;
   'D':begin
        crlf; crlf;
        d(5,'You strut your stuff!');
        d(config.textcolor,'Many adventurers look your way.');
        d(config.textcolor,'They don'+chr(39)+'t seem to appreciate your performance!');
        d(config.textcolor,'You are being ridiculed!');
        crlf;
        pause;
       end;
   'T':begin
       if player.thiefs<1 then begin
        d(5,'You have tried enough for today.');
       end
       else begin
        y:=10;
        if player.class=Assassin then y:=y-1;
        if player.dex>10 then dec(y);
        if player.dex>20 then dec(y);
        if player.dex>40 then dec(y);
        if player.dex>80 then dec(y);
        if player.dex>140 then dec(y);
        if player.dex>260 then dec(y);
        if player.dex>500 then dec(y);

        s^:='Master Thief!';
        case y of
         0: s^:='Master Thief';
         1: s^:='Extremely Good!';
         2: s^:='Skilled Pick-Pocket';
         3: s^:='Pick Pocket';
         4: s^:='Very Good';
         5: s^:='Good';
         6: s^:='Pretty Good';
         7: s^:='Average';
         8: s^:='Poor';
         9: s^:='Bad';
        10: s^:='Worthless';
        end;
        crlf;
        sd(config.textcolor,'Your thieving skill is ');
        sd(14,s^);
        crlf;

        if confirm('Do you dare to attempt thievery in here','n')=true then begin

         player.thiefs:=player.thiefs-1;
         if random(y)=0 then begin
          xx:=random(3000)+500;
          xx:=xx*player.level;
          incplayermoney(player,xx);
          d(config.textcolor,'Success! You managed to steal '+commastr(xx)+' '+many_money(xx)+'!');
          crlf;
          pause;
         end
         else begin
          d(config.textcolor,'Failure!! Your clumsy attempt was detected!');
          if random(5)=0 then begin
           d(config.textcolor,'You were caught! You must spend the night in jail!');
           d(config.textcolor,'Come back tomorrow.');
           crlf;

           newsy(true,
           'Thief Caught!',
           ' '+uplc+player.name2+config.textcol1+' was caught stealing!',
           ' '+uplc+player.name2+config.textcol1+' was thrown in jail!',
           '',
           '',
           '',
           '',
           '',
           '',
           '');

           {player.allowed:=false;}
           Reduce_Player_Resurrections(player,true);
           pause;
           normal_exit;
          end
          else begin
           d(config.textcolor,'You managed to escape!');
           crlf;
           pause;
          end;
         end;
        end;
       end;
       end;
   'B':begin {slagsm†l! BRAWL!}
        if player.brawls<1 then begin
         crlf;
         d(5,'You are too exhausted!');
         crlf;
        end
        else begin
         crlf;
         if confirm('Start a fight ','n')=true then begin
          brawl;
         end;
        end;

       end;
   'C':begin {drinking competition}

        if player.chivnr<1 then begin
         crlf;
         d(12,'You are too exhausted!');
         pause;
         break;
        end;

        crlf;
        crlf;

        if confirm('Initiate a Beerdrinking contest','N')=true then begin
         dec(player.chivnr);;
         drinking_competition;
        end;

       end;

  end; {case .end.}

 until ch='R';
 crlf;

 {dispose pointer variables}
 dispose(rr);
 dispose(a);
 dispose(a2);
 dispose(s);
 dispose(ts);
 dispose(ok);
 dispose(no);

end; {Bobs_Inn *end*}

end. {Unit Bobs .end.}
