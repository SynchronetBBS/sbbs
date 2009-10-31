
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
#include <ctype.h>
#include <math.h>
#include <stdbool.h>

#include "Status.h"

#include "IO.h"
#include "Config.h"

#include "macros.h"
#include "str.h"
#include "various.h"

#include "todo.h"

static const char *expert_keys="(B,C,D,T,S,R,?)";

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
		"Oh no...you're gonna take what's coming to ya!",
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
		"Next time I'll mess ya up bad honey!",
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
		"I'm gonna trash you to pieces!",
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
		"BAHAHA! I don't think you realize who I am!",
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
		"I'm afraid it's too late for that!",
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
		"It's your lucky day!",
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
		"Let's solve this problem with our fists!",
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
		
static void Meny(void *cbdata)
{
	clr();
	nl();
	HEADER("-*- ",config.bobsplace, " -*-");
	nl();
	TEXT("This place is the rendezvous for every lowlife in town. "
			"Unexperienced characters should not enter here alone. "
			"If you are looking for trouble, here it is! "
			"As you enter the hut, there is a short moment of silence. "
			"But things soon get back to normal when your entrance "
			"has been noticed.");
	nl();
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
		nl();
		EVENT("Your move?");
		menu(Asprintf("(G)ive them your cash (%s %s %s)", commastr(player->gold), config.moneytype, MANY_MONEY(player->gold)));
		menu("(D)efend yourself!");
	}

	return(toupper(gchar()));
}

static void Shakedown(void)
{
	char			ch;
	int				i;
	struct monster	monster[5];
	struct monster	*monsters[6]={&monster[0], &monster[1], &monster[2], &monster[3], &monster[4], NULL};

	BAD("SHAKEDOWN!");
	TEXT("A bunch of rough characters suddenly appear from the shadows. "
			"The stranger smirks and pulls out a long knife from his robes!");
	ch='?';

	do {
		ch=Shakedown_Move(ch);
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
				for(i=0; i<12; i++)
					player->spell[i][1]=false;
				GOOD("GOOD WORK!");
				TEXT("The streets are a bit safer now!");

				newsy(true, "Cleaner", 
						Asprintf(" %s%s%s fought a band of rogues outside %s!", config.plycolor, player->name2, config.textcolor, config.bobsplace), 
						Asprintf(" %s%s%s was victorious!",  config.plycolor, player->name2, config.textcolor),
						NULL);
			}
			else {
				player->hps=0;
				player->gold=0;
				newsy(true, "Dead end!", 
						Asprintf(" %s%s%s was tricked in %s. The stubborn fool refused", config.plycolor, player->name2, config.textcolor, config.bobsplace), 
						Asprintf(" to give up %s %s to a band of thugs.",  sex3[player->sex], config.moneytype),
						Asprintf(" %s%s%s was killed!", config.plycolor, player->name2, config.textcolor), 
						NULL);

				// Mail player
				Post(MailSend, player->name2, player->ai, false, MAILREQUEST_Nothing,
						"",
						Asprintf("%sLast breath%s", config.umailheadcolor, config.textcolor),
						mkstring(11,196),
						Asprintf("You were killed by a band of thieves outside %s.", config.bobsplace),
						Asprintf("They took all your %s.", config.moneytype),
						"You ended up with a dagger in the back...",
						NULL);

				TEXT("You are dead....!");
				TEXT(Asprintf("Kiss your precious %s goodbye!", config.moneytype));
				normal_exit();
			}
			break;
		case 'G':
			player->gold=0;
			nl();
			DL(config.badcolor, "\"Wise decision peasant!\"", config.textcolor, ", the leader of the scoundrels says as you hand over the ", config.moneytype, ".");
			SLEEP(300);
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
		nl();
		if(confirm("Buy a bottle", 'N')) {
			decplayermoney(player, x);
			TEXT("You hand over the ", config.moneytype, " and recieve a bottle filled with something which looks like syrup......");
			SLEEP(800);
			nl();
			D(config.talkcolor, "Down the hatch!");
			SLEEP(1000);
			nl();
			nl();
			x=3+rand_num(3);
			DL(config.textcolor, "You wake up a short moment later. You are laying in the "
						"alley. All your belongings are left untouched. "
						"Your strength went up with ",
					config.goodcolor, commastr(x),
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

	DL(config.textcolor, "The stranger says that he will guide you to the UMAN caves for only ",
			config.moneycolor, commastr(x), config.textcolor, " ", config.moneytype, " ", config.moneytype3);
	if(player->gold < x) {
		TEXT("When you explain that you don't have the ", config.moneytype, " he is asking "
				"the stranger leaves.");
	}
	else {
		decplayermoney(player, x);
		nl();
		TEXT("You hand over the ", config.moneytype, " and start following the stranger...");
		nl();
		player->auto_probe=UmanCave;
	}
}

static void Man_In_Robes(void)
{
	// strange man i robes approaches!
	if(rand_num(5)==0 && player->gold > 1000) {
		nl();
		EVENT("You are approached by a stranger in robes!");
		GOOD("He offers you to see some fine wares he has in store.");
		nl();
		if(confirm("Follow the stranger outside ",'N')) {
			// Update player location
			onliner->location=ONLOC_BobThieves;
			strcpy(onliner->doing,location_desc(onliner->location));
			nl();
			TEXT("You follow the stranger out into the nearby alley...");
			SLEEP(800);
			nl();
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
			nl();
			GOOD("NO THANKS!");
		}
	}
}

void BarFight(int *pstam, struct opponent *opp)
{
	char	ch;
	int		pcnt;
	int		i;
	int		attack;
	int		chance;
	char	*str;

	do {
		ch='?';

		pcnt=*pstam*100/player->stamina;

		str="Excellent Health";
		if(pcnt<100)
			str="You have some small wounds and bruises";
		if(pcnt<80)
			str="You have some wounds";
		if(pcnt<70)
			str="You are pretty hurt!";
		if(pcnt<50)
			str="You are in a bad shape!";
		if(pcnt<30)
			str="You have some BIG and NASTY wounds!";
		if(pcnt<20)
			str="You are in a awful condition!";
		if(pcnt<5)
			str="You are bleeding from wounds ALL over your body!";
		nl();
		DL(cyan, "Your status : ", lred, str);

		pcnt=opp->cstam*100/player->stamina;

		str="Excellent Health";
		if(pcnt<100)
			str="Small wounds and bruises";
		if(pcnt<80)
			str="Some wounds";
		if(pcnt<70)
			str="Pretty hurt!";
		if(pcnt<50)
			str="Bad condition!";
		if(pcnt<30)
			str="Have some BIG and NASTY wounds!";
		if(pcnt<20)
			str="Is in a awful condition!";
		if(pcnt<5)
			str="AWFUL CONDITION!";
		DL(config.plycolor, opp->name, cyan, " status : ", lred, str);
		nl();

		D(config.textcolor, "Action (", config.textcolor2, "?", config.textcolor, " for moves) :");

		do {
			ch=toupper(gchar());
		} while((ch < 'A' || ch > 'N') && ch != '?');

		nl();
						
		if(ch=='?') {
			char chstr[2];

			chstr[1]=0;
			nl();
			for(i=0; i<14; i++) {
				char	line[80];
				sprintf(line, ") %-32s %s", UCBashName[i], BashRank[player->skill[i]]);
				DL(magenta, chstr, config.textcolor, line);
			}
		}
	} while(ch <'A' || ch > 'N');

	nl();
	switch(ch) {
		case 'A':
			nl();
			DL(config.textcolor, "Here goes...You take run and then charge ", config.plycolor, opp->name, config.textcolor, " and try a tackle!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.plycolor, opp->name, config.textcolor, " evade your attack! You stumble past ", opp->male?"him":"her");
			else {
				DL(config.textcolor, "Your tackle hit ", config.plycolor, opp->name, config.textcolor, " VERY hard!");
				DL(config.plycolor, opp->name, config.textcolor, " lose sense of all directions!");
				opp->cstam-=rand_num(4)+4;
			}
			break;
		case 'B':
			DL(config.textcolor, "BANZAI! You rush ", config.plycolor, opp->name, config.textcolor, " and try a Drop-Kick!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.plycolor, opp->name, config.textcolor, "  simply jumps aside, and you fly past ", opp->male?"him":"her", "!");
			else {
				DL(config.textcolor, "Your kick hit ", config.plycolor, opp->name, config.textcolor, " hard!");
				DL(config.textcolor, opp->male?"He":"She", " goes down!");
				opp->cstam -= rand_num(4)+4;
			}
			break;
		case 'C':
			DL(config.textcolor, "You try an uppercut against ", config.plycolor, opp->name, config.textcolor, "!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.plycolor, opp->name, config.textcolor, " elegantly blocks your assault!");
			else {
				DL(config.textcolor, "You hit ", config.plycolor, opp->name, config.textcolor, " right on the chin!");
				DL(config.textcolor, opp->male?"He":"She", " goes down!");
				opp->cstam -= rand_num(4)+4;
			}
			break;
		case 'D':
			DL(config.textcolor, "You throw yourself against ", config.plycolor, opp->name, config.textcolor, " with a fearsome battlecry!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.textcolor, "But ", config.plycolor, opp->name, config.textcolor, " elegantly avoid your clumsy assault!");
			else {
				DL(config.plycolor, opp->name, config.textcolor, " screams in horror as you bite ", opp->male?"him":"her", " hard!");
				opp->cstam -= rand_num(4)+4;
			}
			break;
		case 'E':
			DL(config.textcolor, "You try a Leg-Sweep against ", config.plycolor, opp->name, config.textcolor, "!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.textcolor, "But ", config.plycolor, opp->name, config.textcolor, " manage to avoid the assault!");
			else {
				DL(config.plycolor, opp->name, config.textcolor, " falls heavily to the floor as you sweep away ", opp->male?"his":"her", " legs!");
				opp->cstam -= rand_num(4)+4;
			}
			break;
		case 'F':
			DL(config.textcolor, "You try to get a good grip on ", config.plycolor, opp->name, config.textcolor, "!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.textcolor, "But ", config.plycolor, opp->name, config.textcolor, " blocks your attack!");
			else {
				DL(config.plycolor, opp->name, config.textcolor, " screams in pain as you crack a bone in ", opp->male?"his":"her", " body! Hehe!");
				opp->cstam -= rand_num(4)+4;
			}
			break;
		case 'G':
			DL(config.textcolor, "You let your right hand go off in an explosive stroke against ", config.plycolor, opp->name, config.textcolor, "!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.textcolor, "But ", config.plycolor, opp->name, config.textcolor, " blocks your attack!");
			else {
				DL(config.plycolor, opp->name, config.textcolor, " looks surprised when your punch hit ", opp->male?"him":"her", " hard in the chest!");
				DL(config.textcolor, opp->male?"He":"She", " goes down on ", opp->male?"his":"her", " knees, moaning!");
				opp->cstam -= rand_num(4)+4;
			}
			break;
		case 'H':
			DL(config.textcolor, "You send off a stroke against your foe!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0) {
				DL(config.textcolor, "But ", config.plycolor, opp->name, config.textcolor, " avoids your attack with elegance!");
				DL(config.textcolor, "Instead you collect a counterattack against your upper body!");
				*pstam -= rand_num(4)+4;	// TODO: This was a BUG!!!
			}
			else {
				DL(config.plycolor, opp->name, config.textcolor, " is stunned from your effective punch!");
				DL(config.textcolor, opp->male?"He":"She", " goes down!");
				opp->cstam -= rand_num(4)+4;
				//stunned2=true;
			}
			break;
		case 'I':
			DL(config.textcolor, "You try to get your hands around ", config.plycolor, opp->name, "s", config.textcolor, " throat!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.textcolor, "But ", config.plycolor, opp->name, config.textcolor, " takes a couple of ballet steps, and avoid your attack!");
			else {
				DL(config.plycolor, opp->name, config.textcolor, " face turns red when ", opp->male?"he":"she", " chokes!", opp->male?"":" (hehe!)");
				opp->cstam -= rand_num(4)+4;
			}
			break;
		case 'J':
			DL(config.textcolor, "You try a Headbash against ", config.plycolor, opp->name, config.textcolor, "!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.textcolor, "But ", config.plycolor, opp->name, config.textcolor, " quickly takes a few steps back!");
			else {
				DL(config.textcolor, "You hit ", config.plycolor, opp->name, "s", config.textcolor, " head with a massive blow!");
				DL(config.textcolor, opp->male?"He":"She", " goes down bleeding from some nasty wounds!");
				opp->cstam -= rand_num(4)+4;
			}
			break;
		case 'K':
			DL(config.textcolor, "You try to grab hold of ", config.plycolor, opp->name, "s", config.textcolor, " hair!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.textcolor, "But ", config.plycolor, opp->name, config.textcolor, " manages to evade your attack!");
			else {
				DL(config.textcolor, "You get a firm grip of ", opp->male?"his":"her", " hair! You drag ", opp->male?"his":"her", " around the room!");
				opp->cstam -= rand_num(4)+4;
			}
			break;
		case 'L':
			DL(config.textcolor, "You try a Kick against ", config.plycolor, opp->name, config.textcolor, "!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.textcolor, "But ", config.plycolor, opp->name, config.textcolor, " blocks it!");
			else {
				DL(config.textcolor, opp->male?"He":"She", " staggers under your blow!");
				opp->cstam -= rand_num(4)+4;
			}
			break;
		case 'M':
			DL(config.textcolor, "You try a Punch against ", config.plycolor, opp->name, config.textcolor, "!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.textcolor, "But ", config.plycolor, opp->name, config.textcolor, " blocks it!");
			else {
				DL(config.textcolor, "You hit ", opp->male?"him":"her", " right on the chin! ", opp->male?"He":"She", " is dazed!");
				opp->cstam -= rand_num(4)+4;
			}
			break;
		case 'N':
			DL(config.textcolor, "You rush ", config.plycolor, opp->name, config.textcolor, " with your head lowered!");
			if(rand_num(HitChance(player->skill[ch-'A']))!=0)
				DL(config.textcolor, "But ", opp->male?"he":"she", " manages to jump aside just before you reach ", opp->male?"him":"her", "!");
			else {
				DL(config.textcolor, "You hit ", config.plycolor, opp->name, config.textcolor, " right in Solar-Plexus! You send ", opp->male?"him":"her", " flying!");
				opp->cstam -= rand_num(4)+4;
			}
			break;
	}

	// enemy move
	attack=rand_num(14);
	str=opp->male?"He":"She";
	switch(attack) {
		case 0: DL(config.textcolor, str, " tries to tackle you!"); break;
		case 1: DL(config.textcolor, str, " tries a drop-kick against you!"); break;
		case 2: DL(config.textcolor, str, " tries an uppercut against your head!"); break;
		case 3: DL(config.textcolor, str, " tries to bite you in the leg!"); break;
		case 4: DL(config.textcolor, str, " goes for a leg-sweep against you!"); break;
		case 5: DL(config.textcolor, str, " tries to break your arm!"); break;
		case 6: DL(config.textcolor, str, " sends off a Nicehand, aimed at your chest!"); break;
		case 7: DL(config.textcolor, str, " sends off a nervepunch!"); break;
		case 8: DL(config.textcolor, "The bastard tries to strangle you!"); break;
		case 9: DL(config.textcolor, str, " goes for a Headbash! Watch it!"); break;
		case 10: DL(config.textcolor, str, " tries to pull your hair!"); break;
		case 11: DL(config.textcolor, str, " goes for a kick, aimed at your body!"); break;
		case 12: DL(config.textcolor, str, " goes for a straight punch in your face!"); break;
		case 13: DL(config.textcolor, str, " comes rushing at you in high speed! RAM!"); break;
	}

	chance=1;
	if(*pstam>5) chance++;
	if(player->class==Assassin || player->class==Jester) chance++;
	if(player->agility > 50) chance++;
	if(player->agility > 200) chance++;
	if(player->agility > 700) chance++;
	if(player->agility > 1300) chance++;
	if(player->level > 50) chance++;
	if(player->level > 95) chance++;

	if(rand_num(chance) < 2) {
		DL(config.textcolor, "You didn't manage to evade!");
		//if(x==7)
		//	stunned1=true;
		switch(attack) {
			case 0: DL(config.textcolor, "The hard tackle sends you across the floor! You lose your breath!"); break;
			case 1: DL(config.textcolor, "You stagger under the impact from the kick!"); break;
			case 2: DL(config.textcolor, "The stroke lands neately on your chin! You see stars and moons!"); break;
			case 3: DL(config.textcolor, opp->male?"His":"Her", " bite goes deep in your leg! You are bleeding!"); break;
			case 4: DL(config.textcolor, "You fall heavily to the floor!"); break;
			case 5: DL(config.textcolor, "Cracck... Something just broke in your body! You are hurt bad!"); break;
			case 6: DL(config.textcolor, "You lose your breath when ", opp->male?"his":"her", " steelhand goes right in your chest!"); break;
			case 7: DL(config.textcolor, "The stroke hits a nerve! You are stunned!"); break;
			case 8: DL(config.textcolor, opp->male?"He":"She", " got you in a chokeholt! You can't breathe!"); break;
			case 9: DL(config.textcolor, "BONK! You are feeling dizzy! Your head aches!"); break;
			case 10: DL(config.textcolor, "Arrgghhh! That really hurt! What a dirty trick!"); break;
			case 11: DL(config.textcolor, "The well placed kick lands right in solar-plexus! Ouch!"); break;
			case 12: DL(config.textcolor, "The punch lands right on your nose! You lose sense of all directions!"); break;
			case 13: DL(config.textcolor, opp->male?"His":"Her"," attack sends you sprawling! You fall to the floor!"); break;
		}
		*pstam -= rand_num(6)+4;
	}
}

void Bobs_Inn(void)
{
	int				pstam;
	int				reward;
	int				diff;
	struct opponent	opp;
	char			*str;
	char			ch;
	bool			refresh;
	int				chance;

	do {
		if(player->auto_probe != NoWhere)
			break;
		if(rand_num(2)==0 && player->darknr > 0) {	// spaghetti incident?
			nl();
			nl();
			player->darknr--;
			DL(yellow, "Insulted !?");

			// discovery
			memcpy(&opp, &opponents[rand_num(sizeof(opponents)/sizeof(opponents[0]))], sizeof(opp));
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
				nl();
				DL(yellow, "Yeah! REVENGE!");
				nl();
				TEXT("You walk up to the ", config.plycolor, opp.type, config.textcolor, " and tap ", opp.male?"him":"her", " on the shoulder...");
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
						SAY("  What do you want ", LCRaceName[player->race],"?");
						break;
				}
				nl();
				DL(config.textcolor, "It's ", config.plycolor, opp.name, " that you have run across!");
				TEXT(" Your chances : ");

				pstam = player->stamina;
				reward = opp.cstam*100+rand_num(500);
				diff=opp.cstam-pstam;

				str="YOU ARE MAD!";

				if(diff < 100)
					str="Don't even think about it!";
				if(diff < 90)
					str="The chances are VERY VERY small!";
				if(diff < 80)
					str="You're gonna get messed up!";
				if(diff < 70)
					str="No way you can win this!";
				if(diff < 60)
					str="Are you mad?";
				if(diff < 50)
					str="Feel lucky punk?";
				if(diff < 40)
					str="Risky!";
				if(diff < 30)
					str="Can get rough...";
				if(diff < 20)
					str="You can't lose!";
				if(diff < 10)
					str="Almost too easy...";
				if(diff < 0)
					str="Piece of Cake!";
				if(diff < -10)
					str="You can do it blindfolded!";

				DL(magenta, str);
				nl();

				TEXT("Make your move :");
				menu("(E)lbow in the face");
				menu("(S)traight punch in the stomach");
				menu("(T)rip the scumbag");
				menu("(B)ite ear");
				menu(Asprintf("(A)pologize for disturbing %s", opp.name));
				PART(":");

				do {
					ch=toupper(gchar());
				} while(strchr("ESTBA", ch)==NULL);

				if(ch=='A') {
					if(rand_num(2)==0) {
						nl();
						DL(yellow, "\"", opp.no, "\"");
					}
					else {
						nl();
						DL(yellow, "\"", opp.ok, "\"");
						upause();

						break;
					}
				}

				nl();

				if(opp.cstam < 5)
					opp.cstam=5;

				switch(ch) {
					case 'E':
						if(rand_num(2)==0) {
							DL(lred, "Bullseye! Your elbow almost crushed ", opp.male?"his":"her", " nose!");
							opp.cstam -= rand_num(3);
						}
						else
							DL(config.plycolor, opp.name, config.textcolor, " managed to avoid your attack!");
						break;
					case 'S':
						if(rand_num(2)==0) {
							DL(lred, "Dynamite! ", config.plycolor, opp.name, config.textcolor, " staggers from your unexpected attack!");
							opp.cstam -= rand_num(3);
						}
						else
							DL(config.plycolor, opp.name, config.textcolor, " saw that one coming!");
						break;
					case 'T':
						if(rand_num(2)==0) {
							DL(lred, "Success! ", opp.name, " falls heavily to the floor!");
							opp.cstam -= rand_num(3);
						}
						else
							TEXT(opp.name, " managed to retreat from your assault!");
						break;
					case 'B':
						if(rand_num(2)==0) {
							DL(config.plycolor, opp.name, lred, " screams loud when you get a bite of ", opp.male?"his":"her", "ear!");
							opp.cstam -= rand_num(3);
						}
						else
							DL(config.plycolor, opp.name, lred, "  managed to avoid your clumsy attack!");
						break;
				}

				do {
					BarFight(&pstam, &opp);
				} while (pstam > 0 && opp.cstam > 0);

				if(pstam <=0) {
					DL(config.textcolor, "Your head starts to spin, and you feel darkness...");
					DL(config.textcolor, "All your strength is gone!");
					DL(config.textcolor, "Before you slide into unconsciousness you feel how you are being frisked...");
					nl();

					switch(rand_num(2)) {
						case 0:
							newsy(true, "Dork", Asprintf(" %s%s%s tried to show off at %s.", config.plycolor, player->name2, config.textcolor, config.bobsplace),
									Asprintf(" %s was knocked out after a short fight against %s%s%s, the %s.", sex2[player->sex], config.plycolor, opp.name, config.textcolor, opp.type), NULL);
							break;
						case 1:
							newsy(true, Asprintf("Brawl at %s!", config.bobsplace),
									Asprintf(" %s%s%s accepted a challenge in %s.", config.plycolor, player->name2, config.textcolor, config.bobsplace),
									Asprintf(" The violence ended when %s%s%s was knocked out", config.plycolor, player->name2, config.textcolor),
									Asprintf(" by %s%s%s, the %s.",config.plycolor, opp.name, config.textcolor, opp.type), NULL);
							break;
					}
					player->gold=0;
					upause();

					Post(MailSend, player->name2, player->ai, false, MAILREQUEST_Nothing, "", Asprintf("%sDefault!%s", config.umailheadcolor, config.textcolor),
							mkstring(7, 196),
							Asprintf("You were knocked out in %s!", config.bobsplace),
							"Better train some more before starting a brawl!", NULL);
					normal_exit();
				}
				else {
					nl();
					DL(yellow, "Victory!");
					DL(config.textcolor, "You have defeated ", opp.name, "!");
					DL(config.textcolor, "You frisk your foe and find ", yellow, commastr(reward), config.textcolor, " ", MANY_MONEY(reward), "!");

					reward=player->level*rand_num(60)+10;
					DL(config.textcolor, "You also receive ", yellow, commastr(reward), config.textcolor, " experience points!");
					DL(config.textcolor, "...");
					incplayermoney(player, reward);
					player->exp += reward;

					newsy(true, "Fistfighter!",
							Asprintf(" %s%s%s  fought a staggering battle at %s.", config.plycolor,player->name2, config.textcolor, config.bobsplace),
							Asprintf(" It turned out to be a new victory in %s promising career!", sex3[player->sex]), NULL);
					upause();
				}
			}
			else {
				nl();
				DL(magenta, "You leave things as they are...");
				DL(magenta, "Wise perhaps, but not very brave.");
				if(player->charisma > 5)
					player->charisma -= rand_num(3);
			}
			nl();
		}

		if(onliner->location != ONLOC_BobsBeer) {
			refresh=true;
			onliner->location=ONLOC_BobsBeer;
			strcpy(onliner->doing,location_desc(onliner->location));
		}

		Display_Menu(true, true, &refresh, config.bobsplace, expert_keys, Meny, NULL);

		ch=toupper(gchar());

		switch(ch) {
			case '?':
				Display_Menu(player->expert, false, &refresh, config.bobsplace, expert_keys, Meny, NULL);
				break;
			case 'S':
				clr();
				Status(player);
				break;
			case 'D':
				nl();
				nl();
				DL(magenta, "You strut your stuff!");
				DL(config.textcolor, "Many adventurers look your way. They don't seem to appreciate your performance! You are being ridiculed!");
				nl();
				upause();
				break;
			case 'T':
				if(player->thiefs < 1)
					DL(magenta, "You have tried enough for today.");
				else {
					chance=10;
					if(player->class==Assassin) chance--;
					if(player->dex > 10) chance--;
					if(player->dex > 20) chance--;
					if(player->dex > 40) chance--;
					if(player->dex > 80) chance--;
					if(player->dex > 140) chance--;
					if(player->dex > 260) chance--;
					if(player->dex > 500) chance--;

					switch(chance) {
						default:
						case 0: str="Master Thief!"; break;
						case 1: str="Extremely Good!"; break;
						case 2: str="Skilled Pick-Pocket"; break;
						case 3: str="Pick Pocket"; break;
						case 4: str="Very Good"; break;
						case 5: str="Good"; break;
						case 6: str="Pretty Good"; break;
						case 7: str="Average"; break;
						case 8: str="Poor"; break;
						case 9: str="Bad"; break;
						case 10: str="Worthless"; break;
					}
					nl();
					DL(config.textcolor, "Your thieving skill is ", yellow, str);
					if(confirm("Do you dare to attempt thievery in here", 'N')) {
						player->thiefs--;
						if(rand_num(chance)==0) {
							reward=rand_num(3000)+500;
							reward*=player->level;
							incplayermoney(player, reward);
							DL(config.textcolor, "Success! You managed to steal ", commastr(reward), " ", MANY_MONEY(reward), "!");
							nl();
							upause();
						}
						else {
							DL(config.textcolor, "Failure!! Your clumsy attempt was detected!");
							if(rand_num(5)==0) {
								DL(config.textcolor, "You were caught! You must spend the night in jail!");
								DL(config.textcolor, "Come back tomorrow.");
								nl();
								newsy(true, "Thief Caught", Asprintf(" %s%s%s was caught stealing!", config.plycolor, player->name2, config.textcolor),
										Asprintf(" %s%s%s was thrown in jail!", config.plycolor, player->name2, config.textcolor), NULL);
								Reduce_Player_Resurrections(player, true);
								upause();
								normal_exit();
							}
							else {
								DL(config.textcolor, "You managed to escape!");
								nl();
								upause();
							}
						}
					}
				}
				break;
			case 'B':	// BRAWL!
				if(player->brawls < 1) {
					nl();
					DL(magenta, "You are too exhausted!");
					nl();
				}
				else {
					nl();
					if(confirm("Start a fight ",'N'))
						Brawl();
				}
				break;
			case 'C':	// Drinking Competition
				if(player->chivnr < 1) {
					nl();
					DL(lred, "You are too exhausted!");
					upause();
				}
				else {
					nl();
					nl();
					if(confirm("Initiate a Beerdrinking contest", 'N')) {
						player->chivnr--;
						Drinking_Competition();
					}
				}
				break;
		}
	} while (ch != 'R');
	nl();
}
