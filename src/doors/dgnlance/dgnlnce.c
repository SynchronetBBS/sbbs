#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "OpenDoor.h"

#include "gen_defs.h"
#include "genwrap.h"

struct playertype {
	char		name[31];
	char		pseudo[31];
	char		password[31];
	char		ally[31];
	char		killer[31];
	char		gaspd[61];
	INT16		status;
	INT16		flights;
	INT16		alliance;
	INT16		plus;
	double	r;
	double	experience;
	double	strength;
	double	intelligence;
	double	gold;
	double	luck;
	double	dexterity;
	double	constitution;
	double	charisma;
	double	weapon;
	double	vehicle;
	double	shield;
	double	damage;
	double	bank;
	double	attack;
	double	power;
	double	wins;
	double	loses;
};

struct opponent {
	char	name[31];
	double	gold;
	double	strength;
	double	dexterity;
	double	shield;
	double	luck;
	double	experience;
	double	weapon;
	double	weapon2;
	double	attack;
	double	power;
	double	vehicle;
};

const char dots[]="...............................................";
const char spaces[]="                                               ";

double	required[29];	/* In these arrays, 0 isn't actually used */
char		wname[26][31];
char		sname[26][31];
char		cross[51];
char		strip[51];
char		string[51];
char		temp[51];
double	cost[26];
double	w2[26];
double	w3[26];
double	vary;
double	vary2;
double	d;
double	e;
double	rd;
double	roll;
double	z;
double	opt;
double	x;
double	buy;
double	g;
char		option;
char		mess[21][86];
char		blt[5][61];
FILE		*infile;
FILE		*outfile;
FILE		*messfile;
struct opponent	opp;
char		name[31];
char		password[31];
char		ugh[31];
char		ugh2[61];
char		ugh3[61];
INT16		enemy;
INT16		a;
INT16		b;
INT16		m;
INT16		j;
INT16		number;
INT16		dog;
INT16		f;
INT16		y;
INT16		p;
INT16		number_of_players;
INT16		number2;
INT16		verify;
INT16		okea;
INT16		gam;
INT16		trips;
INT16		tint;
INT16		spc;
INT16		amount;
BOOL		allied;
BOOL		finder;
BOOL		play;
BOOL		doga;
BOOL		uni;
BOOL		live;
BOOL		found;
BOOL		ftn;
BOOL		badchar;
BOOL		mg;
BOOL		fini;
BOOL		next;
BOOL		partone;
BOOL		bothover;
BOOL		healall;
struct playertype	player[31];
char		result[31][81];
char		opty;
BOOL		abrt;
BOOL		gonado;
BOOL		logit;
BOOL		yaya;
unsigned char	currfg=7;
unsigned char	currbg=0;
char	tempaa[51];
char	tempbb[51];
char	tempcc[51];
char	tempdd[51];
char	tempee[51];
char	tempff[51];
char	tempgg[51];
char	temphh[51];
char	tempii[51];
char	tempjj[51];
char	tempkk[51];
char	templl[51];
char	tempmm[51];
char	tempnn[51];
char	tempoo[51];
char	temppp[51];
char	tempqq[51];
double temp1a,temp1b,temp1c,temp1d,temp1e,temp1f;
int	addtofore=0;

/*****************************/
/* Functions from Pascal RTL */
/*****************************/
long int round(double v)
{
	long int ret;

	ret=(v+0.5);
	return(ret);
}

/*****************************/
/* Functions from common.pas */
/*****************************/

char	*date(void)
{
	static	char retdate[9];
	time_t	now;

	now=time(NULL);
	strftime(retdate, sizeof(retdate), "%m/%d/%y", localtime(&now));
	return(retdate);
}

void textcolor(int c)
{
	currfg=c;
	od_set_color(currfg,currbg);
}

void textbackground(int c)
{
	currbg=c;
	od_set_color(currfg,currbg);
}

void pausescr(void)
{
	od_set_color(L_CYAN,D_BLACK);
	od_disp_str("[Pause]");
	od_set_color(D_GREY,D_BLACK);
	od_get_key(TRUE);
	od_disp_str("\010 \010\010 \010\010 \010\010 \010\010 \010\010 \010\010 \010");
}


#define nl()	od_disp_str("\r\n");

/********************************/
/* Functions from original .pas */
/********************************/

double playerattack(void)
{
	return ((vary*player[b].attack*player[b].strength*player[b].dexterity*(xp_random(5)+1))/
                  (opp.vehicle*opp.dexterity*opp.luck*vary2));
}

double playerattack2(void)
{
	return(round((vary*player[b].power*player[b].strength*(xp_random(5)+1)*(xp_random(5)+1))/
                   ((opp.vehicle)*opp.luck*vary2)));
}

INT16	roller(void)
{
	return(round(xp_random(3)+1));
}

double opponentattack(void)
{
	return((vary2*opp.weapon*opp.strength*opp.dexterity*(xp_random(5)+1))/
                    ((player[b].vehicle)*player[b].dexterity*player[b].luck*vary));
}

double opponentattack2(void)
{
	return(round((vary2*opp.weapon2*opp.strength*(xp_random(5)+1)*((xp_random(5))+1))/
                     ((player[b].vehicle)*player[b].luck*vary)));
}

double find1(void)
{
	return(w2[round(player[b].weapon)]);
}

double find2()
{
	return(w3[round(player[b].weapon)]);
}

double supplant(void)
{
	return(xp_random(4)/10+(0.8));
}

double experience(void)
{
	return(((opp.vehicle-1+(opp.weapon+opp.weapon2-2)/2)*20+(opp.shield*7.5)+((opp.strength-10)*20)+
		(opp.luck+opp.dexterity-20)*17));
}

double spin(void)
{
	return((xp_random(6)+1)+(xp_random(6)+1)+(xp_random(6)+1));
}

void depobank(void)
{
	double tempbank;
	tempbank=player[b].gold;
	if(tempbank<0)
		tempbank=0;
	player[b].gold=player[b].gold-tempbank;
	player[b].bank=player[b].bank+tempbank;
	nl();
	od_set_color(L_CYAN,D_BLACK);
	od_printf("%1.0f Steel pieces are in the bank.\r\n",player[b].bank);
}

void withdrawbank(void)
{
	double	tempbank;
	tempbank=player[b].bank;
	if(tempbank<0)
		tempbank=0;
	player[b].gold=player[b].gold+tempbank;
	player[b].bank=player[b].bank-tempbank;
	nl();
	od_set_color(D_MAGENTA,D_BLACK);
	od_printf("You are now carrying %1.0f Steel pieces.\r\n",player[b].gold);
}

void checkday(void)
{
	char	oldy[256];
	INT16	h,a;
	for(a=1; a<number_of_players; a++) {
		for(h=a+1; h<= number_of_players; h++) {
			if(player[h].experience>player[a].experience) {
				if(h==b)
					b=a;
				else if(a==b)
					b=h;
				player[number_of_players+1]=player[h];
				player[h]=player[a];
				player[a]=player[number_of_players+1];
			}
		}
	}

	infile=fopen("data/date.lan","r+b");
	fgets(oldy,sizeof(oldy),infile);
	truncsp(oldy);

	if(strcmp(oldy,date())) {
		fseek(infile, 0, SEEK_SET);
		fputs(date(),infile);
		fputs("\n",infile);
		for(a=1; a<=number_of_players; a++) {
			player[a].flights=3;
			player[a].status=0;
		}
		outfile=fopen("data/record.lan","wb");
		fputs("0",outfile);
		fputs("\n",outfile);
		fclose(outfile);
	}
	fclose(infile);
}

void	playerlist(void)
{
	checkday();
	od_clr_scr();
	nl();
	od_set_color(D_MAGENTA,D_BLACK);
	od_disp_str("Hero Rankings:\r\n");
	od_disp_str("!!!!!!!!!!!!!!\r\n");
	a=1;
	for(a=1;a<=number_of_players && !abrt; a++) {
		nl();
		od_set_color(D_MAGENTA,D_BLACK);
		od_printf("%2d.  `bright cyan`%.30s%.*s`green`Lev=%-2.0f  W=%-2.0f  L=%-2.0f  S=%s"
				,a,player[a].pseudo,30-strlen(player[a].pseudo),dots,player[a].r
				,player[a].wins,player[a].loses
				,(player[a].status==1?"SLAIN":"ALIVE"));
		a++;
	}
}

void leave(void)
{
	INT16		a;

	nl();
	outfile=fopen("data/characte.lan","wb");
	fprintf(outfile,"%d\n",number_of_players);
	for(a=1;a<=number_of_players;a++) {
		fprintf(outfile,"%s\n",player[a].name);
		fprintf(outfile,"%s\n",player[a].pseudo);
		fprintf(outfile,"%s\n",player[a].gaspd);
		fprintf(outfile,"%d\n",player[a].status);
		if(player[a].status==1)
			fprintf(outfile,"%s\n",player[a].killer);
		fprintf(outfile,"%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %d %ld %ld %ld %d\n",
				round(player[a].strength),round(player[a].intelligence),
				round(player[a].luck),round(player[a].dexterity),round(player[a].constitution),
				round(player[a].charisma),round(player[a].experience),
				round(player[a].r),round(player[a].shield),round(player[a].weapon),
				round(player[a].vehicle),round(player[a].gold),
				player[a].flights,round(player[a].bank),round(player[a].wins),round(player[a].loses),
				player[a].plus);
	}
	fclose(outfile);
}

void heal(void)
{
	healall=FALSE;
	od_clr_scr();
	od_set_color(D_MAGENTA,D_BLACK);
	od_printf("Clerics want %d Steel pieces per wound.\r\n",round(8*player[b].r));
	od_set_color(L_YELLOW,D_BLACK);
	od_printf("You have %1.0f points of damage to heal.\r\n",player[b].damage);
	od_set_color(L_CYAN,D_BLACK);
	od_disp_str("How many do points do you want healed? ");
	od_input_str(temp,3,'0','9');
	opt=atoi(temp);
	if(opt<0)
		opt=0;
	if(healall)
		opt=player[b].damage;
	if(((round(opt))*(player[b].r)*10)>player[b].gold) {
		od_set_color(L_RED,B_BLACK);
		od_disp_str("Sorry, you do not have enough Steel.\r\n");
		opt=0;
	}
	else if(opt>player[b].damage)
		opt=player[b].damage;
	player[b].damage=player[b].damage-opt;
	player[b].gold=player[b].gold-8*opt*player[b].r;
	od_printf("%1.0f hit points healed.\r\n",opt);
}

void findo(void)
{
	nl();
	od_set_color(L_YELLOW,D_BLACK);
	od_disp_str("The Vile Enemy Dropped Something!\r\n");
	od_disp_str("Do you want to get it? ");
	if(od_get_answer("YN")=='Y') {
		od_disp_str("Yes\r\n");
		okea=round(xp_random(99)+1);
		if ((okea < 10) && (player[b].weapon>=25)) {
			player[b].weapon=player[b].weapon+1;
			od_printf("You have found a %s.\r\n",wname[round(player[b].weapon)]);
		}
		if ((okea > 11) && (okea < 40)) {
			player[b].gold=player[b].gold+40;
			od_disp_str("You have come across 40 Steel pieces\r\n");
		}
		else
			od_disp_str("It's gone!!!!\r\n");
	}
	else {
		od_disp_str("No\r\n");
		od_disp_str("I wonder what it was?!?\r\n");
	}
}

void battle(void);

void recorda(void)
{
	INT16	o;
	outfile=fopen("data/record.lan","wb");
	fprintf(outfile,"%d\n",number);
	for(o=1; o<= number; o++)
		fprintf(outfile,"%s\n",result[o]);
	fclose(outfile);
}

void readla(void)
{
	INT16	o;

	infile=fopen("data/record.lan","rb");
	fgets(temp,sizeof(temp),infile);
	truncsp(temp);
	number=atoi(temp);
	if(number >30)
		number=30;
	for(o=1; o<=number; o++) {
		fgets(result[o],sizeof(result[o]),infile);
		truncsp(result[o]);
	}
	fclose(infile);
}

void mutantvictory(void)
{
	INT16	bt;
	a=m;
	if(!doga)
		opp.gold=opp.gold*supplant();
	nl();
	od_printf("You take his %1.0f Steel pieces.\r\n",opp.gold);
	player[b].gold=player[b].gold+opp.gold;
	if(doga) {
		nl();
		od_set_color(D_GREEN,D_BLACK);
		od_disp_str("The Last Words He Utters Are...\r\n");
		nl();
		od_printf("\"%s\"\r\n",player[a].gaspd);
		nl();
		player[b].wins++;
		player[a].loses++;
		SAFECOPY(player[a].killer,player[b].name);
		player[a].status=1;
		player[a].gold=0;
		if(player[a].weapon>player[b].weapon) {
			d=player[b].weapon;
			player[b].weapon=player[a].weapon;
			player[a].weapon=d;
			bt=player[b].plus;
			player[b].plus=player[a].plus;
			player[a].plus=bt;
			od_set_color(D_GREEN,D_BLACK);
			od_disp_str("You Hath Taken His Weapon.\r\n");
		}
		if(player[a].vehicle>player[b].vehicle) {
			d=player[b].vehicle;
			player[b].vehicle=player[a].vehicle;
			player[a].vehicle=d;
			od_set_color(L_YELLOW,D_BLACK);
			od_disp_str("You Hath Taken His Armour.\r\n");
		}
		player[b].attack=find1();
		player[b].power=find2();
		readla();
		number++;
		sprintf(result[number],"%s conquered %s",player[b].pseudo,player[a].pseudo);
		recorda();
	}
	z*=supplant();
	player[b].experience+=z;
	od_printf("You obtain %1.0f exp points.\r\n",z);
	doga=FALSE;
}

void levelupdate(void)
{
	INT16 x;

	if(player[b].experience>required[round(player[b].r+1)]) {
		player[b].r++;
		od_set_color(L_YELLOW,D_BLACK);
		od_printf("Welcome to level %1.0f!\r\n",player[b].r);
		x=xp_random(6)+1;
		switch(x) {
            	case 1:player[b].strength++; break;
            	case 2:player[b].intelligence++; break;
            	case 3:player[b].luck++; break;
            	case 4:player[b].dexterity++; break;
            	case 5:player[b].constitution++; break;
            	case 6:player[b].charisma++; break;
		}
		player[b].shield=player[b].shield+(xp_random(5)+1)+(player[b].constitution/4);
	}
}

void quickexit(void)
{
	od_disp_str("The Gods of Krynn Disapprove of your actions!\r\n");
	exit(1);
}

void amode(void)
{
	roll=playerattack();
	if(roll<1.5) {
		tint=round(xp_random(3)+1);
		switch(tint) {
			case 1:od_disp_str("Swish you missed!\r\n"); break;
			case 2:od_disp_str("HA HA! He dodges your swing!\r\n"); break;
			case 3:od_disp_str("CLANG, The attack is blocked!\r\n"); break;
			case 4:od_disp_str("You Fight vigorously and miss!\r\n"); break;
		}
	}
	else {
	      roll=playerattack2();
		if(roll>5*player[b].power)
			roll=5*player[b].power;
		opp.shield=opp.shield-roll;
		tint=round(xp_random(2)+1);
		switch(tint) {
			case 1:
				od_printf("You sliced him for %d.\r\n",round(roll));
				break;
			case 2:
				od_printf("You made contact to his body for %d.\r\n",round(roll));
				break;
			case 3:
				od_printf("You hacked him for %d.",round(roll));
				break;
		}
		if(opp.shield<=0) {
			nl();
			tint=round(xp_random(3)+1);
			switch(tint) {
				case 1:od_disp_str("A Painless Death!\r\n"); break;
				case 2:od_disp_str("It Hath died!\r\n"); break;
				case 3:od_disp_str("A Smooth killing!\r\n"); break;
				case 4:od_disp_str("It has gone to the Abyss!\r\n"); break;
			}
			okea=round(xp_random(99)+1);
			if(okea<30)
				findo();
			mutantvictory();
			live=FALSE;
		}
	}
}

void bmode(void)
{
	if((opp.shield>0) && live) {
		roll=opponentattack();
		if(roll<1.5) {
			od_set_color(D_GREEN,D_BLACK);
			od_disp_str("His attack tears your armour.\r\n");
		}
		else {
			roll=opponentattack2();
			if(roll>5*opp.weapon2)
				roll=5*opp.weapon2;
			tint=round(xp_random(2)+1);
			switch(tint) {
				case 1:
					od_printf("He hammered you for %d.\r\n",round(roll));
					break;
				case 2:
					od_printf("He swung and hit for %d.\r\n",round(roll));
					break;
				case 3:
					od_printf("You are surprised when he hits you for %d.\r\n",round(roll));
					break;
			}
			player[b].damage=player[b].damage+roll;
			if(player[b].damage>=player[b].shield) {
				nl();
				tint=round(xp_random(3)+1);
				switch(tint) {
					case 1:od_disp_str("Return This Knight To Huma's Breast....\r\n"); break;
					case 2:od_disp_str("You Hath Been Slain!!\r\n"); break;
					case 3:od_disp_str("Return Soon Brave Warrior!\r\n"); break;
					case 4:od_disp_str("May Palidine Be With You!!\r\n"); break;
				}
				if(doga) {
					player[a].wins++;
					player[b].loses++;
					player[a].gold+=player[b].gold;
					player[b].gold=0;
					readla();
					number++;
					sprintf(result[number],"%s killed %s",player[b].name,player[a].name);
					recorda();
				}
				live=FALSE;
				leave();
				play=FALSE;
			}
		}
	}
}

void statshow(void)
{
	od_clr_scr();

	od_set_color(D_MAGENTA,D_BLACK);
	od_printf("Name: %s   Level: %1.0f\r\n",player[b].pseudo,player[b].r);
	od_set_color(L_CYAN,D_BLACK);
	od_printf("W/L: %1.0f/%1.0f   Exp: %1.0f\r\n",player[b].wins,player[b].loses,player[b].experience);
	nl();
	od_set_color(L_YELLOW,D_BLACK);
	od_printf("Steel  (in hand): %1.0f\r\n",player[b].gold);
	od_printf("Steel  (in bank): %1.0f\r\n",player[b].bank);
	nl();
	od_set_color(L_BLUE,D_BLACK);
	od_printf("Battles: %d   Retreats: %1.0f    Fights: %d   Hps: %1.0f(%1.0f)\r\n",round(3-dog),player[b].flights,10-trips,player[b].shield-player[b].damage,player[b].shield);
	nl();
	od_set_color(L_CYAN,D_BLACK);
	od_printf("Weapon: %s     Armor: %s\r\n",wname[round(player[b].weapon)],sname[round(player[b].vehicle)]);
}

void incre(void)
{
	od_disp_str("Increase which stat? ");
	switch(od_get_answer("123456Q")) {
		case '1':od_disp_str("Strength\r\n"); player[b].strength++; break;
		case '2':od_disp_str("Intelligence\r\n"); player[b].intelligence++; break;
		case '3':od_disp_str("Dexterity\r\n"); player[b].dexterity++; break;
		case '4':od_disp_str("Luck\r\n"); player[b].luck++; break;
		case '5':od_disp_str("Constitution\r\n"); player[b].constitution++; break;
		case '6':od_disp_str("Charisma\r\n"); player[b].charisma++; break;
		case 'Q':od_disp_str("Quit\r\n"); partone=FALSE; break;
	}
}

void decre(void)
{
	od_disp_str("Decrease which stat? \r\n");
	switch(od_get_answer("123456")) {
		case '1':od_disp_str("Strength\r\n"); player[b].strength-=2; break;
		case '2':od_disp_str("Intelligence\r\n"); player[b].intelligence-=2; break;
		case '3':od_disp_str("Dexterity\r\n"); player[b].dexterity-=2; break;
		case '4':od_disp_str("Luck\r\n"); player[b].luck-=2; break;
		case '5':od_disp_str("Constitution\r\n"); player[b].constitution-=2; break;
		case '6':od_disp_str("Charisma\r\n"); player[b].charisma-=2; break;
	}
}

void ministat(void)
{
	yaya=TRUE;
	partone=FALSE;
	nl();
	od_disp_str("Status Change:\r\n");
	od_disp_str("^^^^^^^^^^^^^^\r\n");
	od_printf("1> Str: %1.0f\r\n",player[b].strength);
	od_printf("2> Int: %1.0f\r\n",player[b].intelligence);
	od_printf("3> Dex: %1.0f\r\n",player[b].dexterity);
	od_printf("4> Luk: %1.0f\r\n",player[b].luck);
	od_printf("5> Con: %1.0f\r\n",player[b].constitution);
	od_printf("6> Chr: %1.0f\r\n",player[b].charisma);
	nl();
	if(player[b].strength < 6) {
		yaya=FALSE;
		nl();
		od_disp_str("Strength cannot go below 6\r\n");
	}
	if(player[b].intelligence < 6) {
		yaya=FALSE;
		nl();
		od_disp_str("Intelligence cannot go below 6\r\n");
	}
	if(player[b].dexterity < 6) {
		yaya=FALSE;
		nl();
		od_disp_str("Dexterity cannot go below 6\r\n");
	}
	if(player[b].luck < 6) {
		yaya=FALSE;
		nl();
		od_disp_str("Luck cannot go below 6\r\n");
	}
	if(player[b].constitution < 6) {
		yaya=FALSE;
		nl();
		od_disp_str("Constitution cannot go below 6\r\n");
	}
	if(player[b].charisma < 6) {
		yaya=FALSE;
		nl();
		od_disp_str("Charisma cannot go below 6\r\n");
	}
	if(yaya) {
		od_disp_str("Is this correct? ");
		if(od_get_answer("YN")=='Y') {
			od_disp_str("Yes\r\n");
			player[b].strength=temp1a;
			player[b].intelligence=temp1b;
			player[b].dexterity=temp1d;
			player[b].luck=temp1c;
			player[b].constitution=temp1e;
			player[b].charisma=temp1f;
			bothover=FALSE;
		}
		else {
			od_disp_str("No\r\n");
			bothover=FALSE;
		}
	}
	else {
		if(!yaya) {
			player[b].strength=temp1a;
			player[b].intelligence=temp1b;
			player[b].dexterity=temp1d;
			player[b].luck=temp1c;
			player[b].constitution=temp1e;
			player[b].charisma=temp1f;
			bothover=FALSE;
		}
		else
			bothover=FALSE;
	}
}

void chstats(void)
{
	od_clr_scr();
	bothover=FALSE;
	temp1a=player[b].strength;
	temp1b=player[b].intelligence;
	temp1c=player[b].luck;
	temp1d=player[b].dexterity;
	temp1e=player[b].constitution;
	temp1f=player[b].charisma;

	for(;;) {
		partone=TRUE;
		od_disp_str("Status Change:\r\n");
		od_disp_str("@@@@@@@@@@@@@\r\n");
		od_disp_str("You may increase any stat by one,\r\n");
		od_disp_str("yet you must decrease another by two.\r\n");
		nl();
		od_printf("1> Str: %1.0f\r\n",player[b].strength);
		od_printf("2> Int: %1.0f\r\n",player[b].intelligence);
		od_printf("3> Dex: %1.0f\r\n",player[b].dexterity);
		od_printf("4> Luk: %1.0f\r\n",player[b].luck);
		od_printf("5> Con: %1.0f\r\n",player[b].constitution);
		od_printf("6> Chr: %1.0f\r\n",player[b].charisma);
		nl();
		incre();
		if(partone)
			decre();
		else
			partone=FALSE;
		if(partone)
			bothover=TRUE;
		if(!partone)
			bothover=FALSE;
		else
			ministat();
		if(!bothover)
			break;
	}
}

void attackmodes(void)
{
	if(opp.dexterity>player[b].dexterity) {
		if(play)
			bmode();
		if(play)
			amode();
	}
	else if(opp.dexterity<player[b].dexterity) {
		if(play)
			amode();
		if(play)
			bmode();
	}
	else {
		if(play)
			amode();
		if(play)
			bmode();
	}
}

double readnumb(void)
{
	char	buf[101];
	int	pos=0;
	int	rd;
	double ret;

	buf[0]=0;
	while((rd=fgetc(infile))!=EOF) {
		if(rd=='\r')
			continue;
		if(rd=='\n') {
			ungetc('\n',infile);
			break;
		}
		/* Skip leading spaces */
		if(!pos && isspace(rd))
			continue;
		if(isspace(rd))
			break;
		buf[pos++]=rd;
	}
	buf[pos++]=0;
	ret=atol(buf);
	return(ret);
}

void endofline(void)
{
	int rd;

	while((rd=fgetc(infile))!=EOF) {
		if(rd=='\n');
			break;
	}
}

void searcher(void)
{
	INT16 a;

	trips++;
	fgets(tempaa,sizeof(tempaa),infile);
	amount=atoi(tempaa);
	rd=xp_random(amount-1)+1;
	amount=round(rd);
	for(a=1;a<=amount;a++) {
		fgets(opp.name,sizeof(opp.name),infile);
		truncsp(opp.name);
		opp.shield=readnumb();
		opp.weapon=readnumb();
		opp.weapon2=readnumb();
		opp.vehicle=readnumb();
		opp.luck=readnumb();
		opp.strength=readnumb();
		opp.dexterity=readnumb();
		opp.gold=readnumb();
		z=readnumb();
		endofline();
	}
	fclose(infile);
	opp.weapon*=supplant();
	opp.weapon2*=supplant();
	opp.vehicle*=supplant();
	opp.luck*=supplant();
	opp.strength*=supplant();
	opp.dexterity*=supplant();
	opp.shield*=supplant();
}

void fight(char *filename)
{
	infile=fopen(filename,"rb");
	searcher();
	battle();
}

void doggie(void)
{
	char	tmphh[3];
	INT16	a;

	if(dog<=3) {
		nl();
		checkday();
		for(finder=FALSE; finder==FALSE;) {
			for(enemy=0; enemy!=0;) {
				od_clr_scr();
				od_set_color(L_YELLOW,D_BLACK);
				od_disp_str("Battle Another Hero:\r\n");
				od_disp_str("********************\r\n");
				if(number_of_players==1) {
					od_disp_str("You are the only hero on Krynn!\r\n");
					return;
				}
				else {
					a=1;
					for(a=1; a<=number_of_players && !abrt;) {
						if(player[a].r>player[b].r-4) {
							nl();
							od_set_color(D_MAGENTA,D_BLACK);
							od_printf("%2d.  `bright cyan`%.30s%*s`bright blue`Lev=%-2.0f  W=%-2.0f  L=%-2.0f  S=%s"
									,a,player[a].pseudo,30-strlen(player[a].pseudo),dots
									,player[a].r,player[a].wins,player[a].loses
									,(player[a].status==1?"SLAIN":"ALIVE"));
						}
						a+=1;
					}
					nl();
					od_set_color(L_CYAN,D_BLACK);
					od_disp_str("Enter the rank # of your opponent: ");
					od_input_str(tmphh,2,'0','9');
					enemy=atoi(tmphh);
					if((enemy==0) || (!strcmp(player[enemy].pseudo,player[b].pseudo)) || (player[enemy].status==1))
						return;
				}
			}
			for(a=1; a<=number_of_players; a++) {
				if(enemy==a) {
					if(player[a].r>(player[b].r-4)) {
						finder=TRUE;
						m=a;
					}
				}
			}
		}
		a=m;
		SAFECOPY(opp.name,player[a].pseudo);
		opp.shield=player[a].shield;
		f=b;
		vary2=supplant();
		doga=TRUE;
		d=player[b].attack;
		e=player[b].power;
		g=player[b].weapon;
		b=a;
		dog=dog+1;
		opp.weapon=find1();
		opp.weapon2=find2();
		b=f;
		player[b].weapon=g;
		player[b].attack=d;
		player[b].power=e;
		opp.vehicle=player[a].vehicle;
		opp.luck=player[a].luck;
		opp.strength=player[a].strength;
		opp.dexterity=player[a].dexterity;
		opp.gold=player[a].gold;
		if(opp.gold<0)
			opp.gold=0;
		z=player[a].experience/10;
		finder=FALSE;
		player[b].alliance=0;
		player[a].alliance=0;
		battle();
	}
}

void battle(void)
{
	double 	playerrem;

	nl();
	live=TRUE;
	while(live==TRUE) {
		playerrem=player[b].shield-player[b].damage;
		nl();
		od_set_color(L_YELLOW,D_BLACK);
		od_printf("You are attacked by a %s.",opp.name);
		for(option='?'; option=='?';) {
			od_set_color(L_BLUE,D_BLACK);
			od_printf("Combat (%1.0f hps): (B,F,S): ",playerrem);
			option=od_get_answer("BFS?");
			if(option=='?') {
				od_disp_str("Help\r\n");
				nl();
				nl();
				od_disp_str("(B)attle your opponent.\r\n");
				od_disp_str("(F)lee from your opponent.\r\n");
				od_disp_str("(S)tatus check.\r\n");
				nl();
			}
		}
		switch(option) {
			case 'B':od_disp_str("Battle\r\n");attackmodes(); break;
			case 'S':od_disp_str("Stats\r\n");statshow(); break;
			case 'F':
				od_disp_str("Flee\r\n");
				if((xp_random(4)+1)+player[b].dexterity>opp.dexterity) {
					nl();
					od_set_color(D_GREEN,D_BLACK);
					od_disp_str("You Ride away on a Silver Dragon.\r\n");
					doga=FALSE;
					live=FALSE;
					uni=FALSE;
				}
				break;
		}
	}
}

void credits(void)
{
	od_clr_scr();
	od_set_color(D_GREEN,D_BLACK);
	od_disp_str("Dragonlance 3.0 Credits\r\n");
	od_disp_str("@@@@@@@@@@@@@@@@@@@@@@@\r\n");
	od_disp_str("Original Dragonlance   :  Raistlin Majere & TML   \r\n");
	od_disp_str("Special Thanks To      : The Authors Of Dragonlance\r\n");
	od_disp_str("Dragonlance's Home     : The Tower Of High Sorcery\r\n");
	od_disp_str("Originally modified from the Brazil Source Code.\r\n");
	od_disp_str("C Port                 : Deuce\r\n");
	od_disp_str("Home Page              : http://doors.synchro.net/\r\n");
	od_disp_str("Support                : deuce@nix.synchro.net\r\n");
	nl();
	pausescr();
}

void docs()
{
	od_clr_scr();
	od_send_file("text/docs");
	pausescr();
}

void vic(void)
{
	BOOL ahuh;

	for(ahuh=FALSE; !ahuh; ) {
		nl();
		od_set_color(L_CYAN,D_BLACK);
		od_disp_str("Enter your new Battle Cry.\r\n");
		od_disp_str("> ");
		od_input_str(ugh3, 60, ' ', '~');
		od_disp_str("Is this correct? ");
		if(od_get_answer("YN")=='Y') {
			ahuh=TRUE;
			od_disp_str("Yes\r\n");
		}
		else {
			ahuh=FALSE;
			od_disp_str("No\r\n");
		}
	}
	ahuh=TRUE;
}

void create(BOOL isnew)
{
	if(isnew) {
		nl();
		a=number_of_players+1;
		SAFECOPY(player[a].name,od_control.user_name);
		SAFECOPY(player[a].pseudo,od_control.user_name);
		a=number_of_players+1;
		vic();
	}
	else {
		a=b;
	}
      player[a].strength=12;
      player[a].status=0;
      if(isnew)
		number_of_players++;
      player[a].intelligence=12;
      player[a].luck=12;
      player[a].alliance=0;
      player[a].damage=0;
      SAFECOPY(player[a].gaspd,ugh3);
      player[a].dexterity=12;
      player[a].constitution=12;
      player[a].charisma=12;
      player[a].gold=round(xp_random(100)+175);
      player[a].weapon=1;
      player[a].vehicle=1;
      player[a].experience=0;
      player[a].plus=0;
      player[a].bank=(xp_random(199)+1);
      player[a].r=1;
      player[a].shield=(xp_random(4)+1)+player[a].constitution;
      player[a].flights=3;
      player[a].wins=0;
      player[a].loses=0;
	nl();
	b=a;
}

void weaponlist(void)
{
	nl();
	nl();
	a=1;
	od_disp_str("  Num. Weapon                      Armour                          Price   \r\n");
	od_disp_str("(-------------------------------------------------------------------------)\r\n");
	od_set_color(L_YELLOW,D_BLACK);
	for(a=1; a<=25 && !abrt; a++)
		od_printf("  %2d>  %25.25s   %25.25s   %9.0f\r\n",a,wname[a],sname[a],cost[a]);
}

void readlist(void)
{
	infile=fopen("data/characte.lan","rb");
	fgets(temp,sizeof(temp),infile);
	truncsp(temp);
	number_of_players=atoi(temp);
	a=1;
	b=1;
	while (a<=number_of_players) {
		fgets(player[a].name,sizeof(player[a].name),infile);
		truncsp(player[a].name);
		fgets(player[a].pseudo,sizeof(player[a].pseudo),infile);
		truncsp(player[a].pseudo);
		fgets(player[a].gaspd,sizeof(player[a].gaspd),infile);
		truncsp(player[a].gaspd);
		fgets(temp,sizeof(temp),infile);
		player[a].status=atoi(temp);
		if(player[a].status==1) {
			fgets(player[a].killer,sizeof(player[a].killer),infile);
			truncsp(player[a].killer);
		}
		player[a].strength=readnumb();
		player[a].intelligence=readnumb();
		player[a].luck=readnumb();
		player[a].dexterity=readnumb();
		player[a].constitution=readnumb();
		player[a].charisma=readnumb();
		player[a].experience=readnumb();
		player[a].r=readnumb();
		player[a].shield=readnumb();
		player[a].weapon=readnumb();
		player[a].vehicle=readnumb();
		player[a].gold=readnumb();
		player[a].flights=readnumb();
		player[a].bank=readnumb();
		player[a].wins=readnumb();
		player[a].loses=readnumb();
		player[a].plus=readnumb();
		endofline();
		a+=b;
	}
	fclose(infile);
}

void weaponshop(void)
{
	od_clr_scr();
	od_set_color(L_YELLOW,D_BLACK);
	od_disp_str("Weapon & Armour Shop\r\n");
	od_set_color(D_GREEN,D_BLACK);
	od_disp_str("$$$$$$$$$$$$$$$$$$$$\r\n");
	while(1) {
		nl();
		od_disp_str("(B)rowse, (S)ell, (P)urchase, or (Q)uit? ");
		opty=od_get_answer("BSQP");
		switch(opty) {
			case 'Q':
				od_disp_str("Quit\r\n");
				return;
			case 'B':
				od_disp_str("Browse\r\n");
				weaponlist();
				break;
			case 'P':
				od_disp_str("Purchase\r\n");
				nl();
				nl();
				od_disp_str("Enter weapon/armour # you wish buy: ");
				od_input_str(temp,2,'0','9');
				buy=atoi(temp);
				if(buy==0)
					return;
				if(cost[round(buy)]>player[b].gold)
					od_disp_str("You do not have enough Steel.\r\n");
				else {
					nl();
					od_disp_str("(W)eapon or (A)rmour: ");
					opty=od_get_answer("WA");
					switch(opty) {
						case 'W':
							od_disp_str("Weapon\r\n");
							od_disp_str("Are you sure you want buy it? ");
							if(od_get_answer("YN")=='Y') {
								od_disp_str("Yes\r\n");
								player[b].gold-=cost[round(buy)];
								player[b].weapon=buy;
								nl();
								od_set_color(D_MAGENTA,D_BLACK);
								od_printf("You've bought a %s\r\n",wname[round(buy)]);
								player[b].attack=find1();
								player[b].power=find2();
							}
							else
								od_disp_str("No\r\n");
							break;
						case 'A':
							od_disp_str("Armour\r\n");
							od_disp_str("Are you sure you want buy it? ");
							if(od_get_answer("YN")=='Y') {
								od_disp_str("Yes\r\n");
								player[b].gold-=cost[round(buy)];
								player[b].vehicle=buy;
								nl();
								od_set_color(D_MAGENTA,D_BLACK);
								od_printf("You've bought a %s\r\n",sname[round(buy)]);
							}
							else
								od_disp_str("No\r\n");
							break;
					}
				}
				break;
			case 'S':
				od_disp_str("Sell\r\n");
				nl();
				od_disp_str("(W)eapon,(A)rmour,(Q)uit : ");
				opty=od_get_answer("AWQ");
				switch(opty) {
					case 'Q':
						od_disp_str("Quit\r\n");
						return;
					case 'W':
						od_disp_str("Weapon\r\n");
						y=(round(player[b].weapon));
						x=player[b].charisma;
						x=x*cost[y];
						x=((1/20)*x);
						nl();
						od_printf("I will purchase it for %1.0f, okay? ",x);
						opty=od_get_answer("YN");
						if(opty=='Y') {
							od_disp_str("Yes\r\n");
							od_set_color(D_GREEN,D_BLACK);
							od_disp_str("Is it Dwarven Made?\r\n");
							player[b].weapon=1;
							player[b].gold=player[b].gold+x;
						}
						else
							od_disp_str("No\r\n");
						break;
					case 'A':
						od_disp_str("Armour\r\n");
						x=((1/20)*(player[b].charisma)*(cost[round(player[b].vehicle)]));
						nl();
						od_printf("I will purchase it for %1.0f, okay? ",x);
						opty=od_get_answer("YN");
						if(opty=='Y') {
							od_disp_str("Yes\r\n");
							od_disp_str("Fine Craftsmanship!\r\n");
							player[b].vehicle=1;
							player[b].gold=player[b].gold+x;
						}
						else
							od_disp_str("No\r\n");
						break;
				}
		}
	}
}

void listplayers(void)
{
	INT16	a;
	od_clr_scr();
	od_set_color(L_YELLOW,D_BLACK);
	od_disp_str("Heroes That Have Been Defeated\r\n");
	od_set_color(D_MAGENTA,D_BLACK);
	od_disp_str("+++++++++++++++++++++++++++++++\r\n");
	readla();
	od_set_color(L_CYAN,D_BLACK);
	for(a=1;a<number && !abrt; a++)
		od_disp_str(result[a]);
}

void spy(void)
{
	char	aa[31];
	INT16	a;
	od_clr_scr();
	od_disp_str("Spying on another user eh.. well you may spy, but to keep\r\n");
	od_disp_str("you from copying this person's stats, they will not be   \r\n");
	od_disp_str("available to you.  Note that this is gonna cost you some \r\n");
	od_disp_str("cash too.  Cost: 20 Steel pieces                         \r\n");
	nl();
	od_disp_str("Who do you wish to spy on? ");
	od_input_str(aa,sizeof(aa)-1,' ','~');
	for(a=1;a<=number_of_players;a++) {
		if(!stricmp(player[a].pseudo,aa)) {
			if(player[b].gold<20) {
				od_set_color(L_RED,B_BLACK);
				od_disp_str("You do not have enough Steel!\r\n");
			}
			else {
				player[b].gold-=20;
				nl();
				od_set_color(L_RED,B_BLACK);
				od_printf("%s\r\n",player[a].pseudo);
				nl();
				od_printf("Level  : %1.0f\r\n",player[a].r);
				od_printf("Exp    : %1.0f\r\n",player[a].experience);
				od_printf("Flights: %1.0f\r\n",player[a].flights);
				od_printf("Hps    : %1.0f(%1.0f)\r\n",player[a].shield-player[a].damage,player[a].shield);
				nl();
				od_set_color(D_MAGENTA,D_BLACK);
				od_printf("Weapon : %s\r\n",wname[round(player[a].weapon)]);
				od_printf("Armour : %s\r\n",sname[round(player[a].vehicle)]);
				nl();
				od_set_color(L_YELLOW,D_BLACK);
				od_printf("Steel  (in hand): %1.0f\r\n",player[a].gold);
				od_printf("Steel  (in bank): %1.0f\r\n",player[a].bank);
				nl();
				pausescr();
			}
		}
	}
}

void gamble(void)
{
	char		tempgd[6];
	double	realgold;

	nl();
	if(trips>10)
		od_disp_str("The Shooting Gallery is closed until tomorrow!\r\n");
	else {
		od_clr_scr();
		od_disp_str("  Welcome to the Shooting Gallery\r\n");
		od_disp_str(" Maximum wager is 25,000 Steel pieces\r\n");
		nl();
		od_set_color(L_YELLOW,D_BLACK);
		od_disp_str("How many Steel pieces do you wish to wager? ");
		od_set_color(D_GREY,D_BLACK);
		od_input_str(tempgd,sizeof(tempgd)-1,'0','9');
		realgold=round(atof(tempgd));
		if(realgold>player[b].gold) {
			nl();
			od_disp_str("You do not have enough Steel!\r\n");
		}
		if ((realgold!=0) && ((player[b].gold>=realgold) && (realgold<=25000) && (realgold>=1))) {
			okea=round(xp_random(99)+1);
			if(okea <= 3) {
				realgold*=100;
				player[b].gold+=realgold;
				od_printf("You shot all the targets and win %1.0f Steel pieces!\r\n",realgold);
			}
			else if ((okea>3) && (okea<=15)) {
				realgold*=10;
				player[b].gold+=realgold;
				od_printf("You shot 50% if the targets and win %1.0f Steel pieces!\r\n",realgold);
			}
			else if ((okea>15) && (okea<=30)) {
				realgold*=3;
				player[b].gold+=realgold;
				od_printf("You shot 25% if the targets and win %1.0f Steel pieces!\r\n",realgold);
			}
			else {
				player[b].gold-=realgold;
				od_disp_str("Sorry You Hath Lost!\r\n");
			}
		}
	}
}

void afight(int lev)
{
	char	fname[32];

	uni=TRUE;
	while(uni) {
		if(trips>10) {
			nl();
			od_set_color(D_MAGENTA,D_BLACK);
			od_disp_str("It's Getting Dark Out!\r\n");
			od_disp_str("Return to the Nearest Inn!\r\n");
			uni=FALSE;
		}
		else {
			od_clr_scr();
			sprintf(fname,"data/junkm%d.lan",lev);
			fight(fname);
			uni=FALSE;
		}
	}
}

void bulletin(void)
{
	BOOL	endfil;
	INT16	countr;
	char	tempcoun[3];

	od_clr_scr();
  	countr=0;
	endfil=FALSE;
	od_send_file("text/bullet");
	nl();
	od_disp_str("Do you wish to enter a News Bulletin? ");
	if(od_get_answer("YN")=='Y') {
		od_disp_str("Yes\r\n");
		nl();
		while(!endfil) {
			countr++;
			sprintf(tempcoun,"%d",countr);
			od_set_color(L_YELLOW,D_BLACK);
			od_disp_str(tempcoun);
			od_disp_str("> ");
			od_set_color(D_GREY,D_BLACK);
			od_input_str(blt[countr],60,' ','~');
			if(countr==4)
				endfil=TRUE;
		}
		nl();
		od_disp_str("Is the bulletin correct? ");
		if(od_get_answer("YN")=='Y') {
			od_disp_str("Yes\r\n");
			od_disp_str("Saving Bulletin...\r\n");
			messfile=fopen("text/bullet.lan","ab");
			fputs(blt[1],messfile);
			fputs("\n",messfile);
			fputs(blt[2],messfile);
			fputs("\n",messfile);
			fputs(blt[3],messfile);
			fputs("\n",messfile);
			fputs(blt[4],messfile);
			fputs("\n",messfile);
			fclose(messfile);
		}
		else
			od_disp_str("No\r\n");
	}
	else
		od_disp_str("No\r\n");
}

void training(void)
{
	char		temptrain[2];
	INT16		realtrain;
	INT16		tttgld;

	nl();
	if(trips>10)
		od_disp_str("The Training Grounds are closed until tomorrow!\r\n");
	else {
		od_clr_scr();
		od_disp_str("Training Grounds\r\n");
		od_disp_str("%%%%%%%%%%%%%%%%\r\n");
		od_disp_str("Each characteristic you wish to upgrade\r\n");
		od_disp_str("will cost 1,000,000 Steel pieces per point.\r\n");
		nl();
		tttgld=10000;
		od_disp_str("Do you wish to upgrade a stat? ");
		if(od_get_answer("YN")=='Y') {
			od_disp_str("Yes\r\n");
			if(player[b].gold<(tttgld*100))
				od_disp_str("Sorry, but you do not have enough Steel!\r\n");
			else {
				nl();
				od_disp_str("1> Strength       2> Intelligence\r\n");
				od_disp_str("3> Dexterity      4> Luck        \r\n");
				od_disp_str("5> Constitution   6> Charisma    \r\n");
				nl();
				od_set_color(L_YELLOW,D_BLACK);
				od_disp_str("Which stat do you wish to increase? ");
				od_set_color(D_GREY,D_BLACK);
				temptrain[0]=od_get_answer("123456");
				switch(temptrain[0]) {
					case 1: od_disp_str("Strength\r\n"); break;
					case 2: od_disp_str("Intelligence\r\n"); break;
					case 3: od_disp_str("Dexterity\r\n"); break;
					case 4: od_disp_str("Luck\r\n"); break;
					case 5: od_disp_str("Constitution\r\n"); break;
					case 6: od_disp_str("Charisma\r\n"); break;
				}
				realtrain=temptrain[0]-'0';
				od_disp_str("Are you sure? ");
				if(od_get_answer("YN")=='Y') {
					od_disp_str("Yes\r\n");
					player[b].gold-=tttgld*100;
					switch(realtrain) {
						case 1:
							player[b].strength++;
							break;
						case 2:
							player[b].intelligence++;
							break;
						case 3:
							player[b].dexterity++;
							break;
						case 4:
							player[b].luck++;
							break;
						case 5:
							player[b].constitution++;
							break;
						case 6:
							player[b].charisma++;
							break;
					}
				}
				else
					od_disp_str("No\r\n");
			}
		}
		else
			od_disp_str("N\r\n");
	}
}

void menuit(void)
{
	od_clr_scr();
	od_send_file("text/menu");
}

int main(int argc, char **argv)
{
	atexit(leave);

	od_init();
	gonado=TRUE;
	allied=FALSE;
	ftn=FALSE;
	vary=supplant();
	dog=0;
	doga=FALSE;
	for(a=1; a<=30; a++)
		player[a].damage=0;
	play=TRUE;
	trips=0;
	found=FALSE;
	verify=0;
	SAFECOPY(name,od_control.user_name);
	nl();
	nl();
	credits();
	od_clr_scr();
	od_printf("`bright yellow`----------`blinking red`   -=-=DRAGONLANCE=-=-      `bright yellow`    /\\     \r\n");
	od_printf("\\        /`white`       Version 3.0          `bright yellow`    ||     \r\n");
	od_printf(" \\      /                                 ||     \r\n");
	od_printf("  \\    /  `bright blue`  Welcome To The World of   `bright yellow`    ||     \r\n");
	od_printf("   |  |   `bright cyan`  Krynn, Where the gods     `bright yellow`    ||     \r\n");
	od_printf("  /    \\  `bright blue`  of good and evil battle.  `bright yellow`  \\ || /   \r\n");
	od_printf(" /      \\ `bright cyan`  Help the People Of Krynn. `bright yellow`   \\==/    \r\n");
	od_printf("/        \\                                ##     \r\n");
	od_printf("----------`blinking red`        ON WARD!!!          `bright yellow`    ##     \r\n");
	nl();
	od_disp_str("News Bulletin:\r\n");
	nl();
	od_send_file("text/bullet");
	infile=fopen("data/characte.lan","rb");
	fgets(temp,sizeof(temp),infile);
	number=atoi(temp);
      number_of_players=number;
	a=1;
	b=1;
	while(a<=number_of_players) {
		fgets(player[a].name,sizeof(player[a].name),infile);
		truncsp(player[a].name);
		fgets(player[a].pseudo,sizeof(player[a].pseudo),infile);
		truncsp(player[a].pseudo);
		fgets(player[a].gaspd,sizeof(player[a].gaspd),infile);
		truncsp(player[a].gaspd);
		fgets(temp,sizeof(temp),infile);
		player[a].status=atoi(temp);
		if(player[a].status==1) {
			fgets(player[a].killer,sizeof(player[a].killer),infile);
			truncsp(player[a].killer);
		}
		player[a].strength=readnumb();
		player[a].intelligence=readnumb();
		player[a].luck=readnumb();
		player[a].dexterity=readnumb();
		player[a].constitution=readnumb();
		player[a].charisma=readnumb();
		player[a].experience=readnumb();
		player[a].r=readnumb();
		player[a].shield=readnumb();
		player[a].weapon=readnumb();
		player[a].vehicle=readnumb();
		player[a].gold=readnumb();
		player[a].flights=readnumb();
		player[a].bank=readnumb();
		player[a].wins=readnumb();
		player[a].loses=readnumb();
		player[a].plus=readnumb();
		endofline();
		a+=b;
	}
	fclose(infile);
	for(a=1; a<=number_of_players; a++) {
		if(!strcmp(player[a].name,name)) {
			found=TRUE;
			b=a;
		}
	}
	if(!found) {
		if(number_of_players >= 30)
			quickexit();
		else
			create(TRUE);
	}
	if(player[b].status==1) {
		nl();
		od_set_color(L_CYAN,D_BLACK);
		od_printf("A defeat was lead over you by %s.",player[b].killer);
	}
	checkday();
	if(player[b].flights<1) {
		trips=12;
		dog=4;
	}
	player[b].flights--;
	nl();
	pausescr();
	od_clr_scr();
	infile=fopen("data/weapons.lan","rb");
	od_set_color(L_BLUE,D_BLACK);
	for(a=1; a<=25; a++) {
		fgets(wname[a],sizeof(wname[a]),infile);
		truncsp(wname[a]);
		w2[a]=readnumb();
		w3[a]=readnumb();
		endofline();
	}
	fclose(infile);
	infile=fopen("data/armor.lan","rb");
	od_set_color(L_BLUE,D_BLACK);
	for(a=1; a<=25; a++) {
		fgets(sname[a],sizeof(sname[a]),infile);
		truncsp(sname[a]);
	}
	fclose(infile);
	infile=fopen("data/prices.lan","rb");
	for(a=1; a<=25; a++) {
		cost[a]=readnumb();
		endofline();
	}
	fclose(infile);
	player[b].attack=find1();
	player[b].power=find2();
	infile=fopen("data/experience.lan","rb");
	for(a=1; a<=28; a++) {
		required[a]=readnumb();
		endofline();
	}
	fclose(infile);
	od_set_color(L_YELLOW,D_BLACK);
	player[b].status=0;
	vary2=1;
	statshow();
	mg=FALSE;
	for(play=TRUE; play && gonado;) {
		levelupdate();
		vary2=1;
		if (((player[b].wins+1)*4)-(player[b].loses)<0) {
			nl();
			od_disp_str("As you were Travelling along a Wilderness Path an   \r\n");
			od_disp_str("Evil Wizard Confronted You.  When you tried to fight\r\n");
			od_disp_str("Him off, he cast a Spell of Instant Death Upon you.  \r\n");
			od_disp_str("Instantly you were slain by the Archmage, Leaving you\r\n");
			od_disp_str("as nothing more than a pile of ashes.  Re-Rolled!   \r\n");
			nl();
			pausescr();
			create(FALSE);
			player[b].flights--;
		}
		nl();
		nl();
		od_set_color(L_YELLOW,D_BLACK);
		od_disp_str("Command (?): ");
		opty=od_get_answer("QVP12345CHWLADGFRSTX+-?EZ*#");
		temp[0]=opty;
		temp[1]=0;
		switch(opty) {
			case 'Q':
				od_disp_str("Quit\r\n");
				od_disp_str("LEAVE KRYNN? Are you sure? ");
				if(od_get_answer("YN")=='Y') {
					od_disp_str("Y\r\n");
					play=FALSE;
				}
				else
					od_disp_str("N\r\n");
				break;
			case '1':od_disp_str("1\r\n");afight(1); break;
			case '2':od_disp_str("2\r\n");afight(2); break;
			case '3':od_disp_str("3\r\n");afight(3); break;
			case '4':od_disp_str("4\r\n");afight(4); break;
			case '5':od_disp_str("5\r\n");afight(5); break;
			case 'C':od_disp_str("Change Stats\r\n");chstats(); break;
			case 'H':od_disp_str("Heal\r\n");heal(); break;
			case 'W':od_disp_str("Weapon Shop\r\n");weaponshop(); break;
			case 'L':od_disp_str("Level Update\r\n");levelupdate(); break;
			case 'A':od_disp_str("Battle Another User\r\n");doggie(); break;
			case 'D':od_disp_str("Docs\r\n");docs(); break;
			case 'G':od_disp_str("Gamble\r\n");gamble(); break;
			case 'F':od_disp_str("Battles Today\r\n");listplayers(); break;
			case 'R':od_disp_str("Rank Players\r\n");playerlist(); break;
			case 'S':od_disp_str("Status\r\n");statshow(); break;
			case 'T':od_disp_str("Training Grounds\r\n");training(); break;
			case 'X':
				od_disp_str("Re-Roll\r\n");
				od_disp_str("Please note that this will completely purge\r\n");
                        od_disp_str("your current hero of all atributes!\r\n");
				nl();
                        od_disp_str("Are you sure you want to REROLL your character? ");
				if(od_get_answer("YN")=='Y') {
					od_disp_str("Y\r\n");
					create(FALSE);
					player[b].flights--;
				}
				else
					od_disp_str("N\r\n");
				break;
			case '+':od_disp_str("Deposit\r\n");depobank(); break;
			case 'P':od_disp_str("Plug\r\n");od_send_file("text/plug"); break;
			case '-':od_disp_str("Withdraw\r\n");withdrawbank(); break;
			case '?':od_disp_str("Help\r\n");menuit(); break;
			case 'E':od_disp_str("Edit Announcement\r\n");bulletin(); break;
			case 'Z':od_disp_str("Spy\r\n");spy(); break;
			case 'V':
				od_disp_str("Version\r\n");
				od_set_color(L_BLUE,D_BLACK);
				od_disp_str("This Is Dragonlance version 3.0\r\n");
				pausescr();
				break;
			case '*':
				od_disp_str("Change Name\r\n");
				nl();
				od_set_color(L_CYAN,D_BLACK);
				od_disp_str("Your family crest has been stolen, they\r\n");
				od_disp_str("inscribe a new one with the name...\r\n");
				od_disp_str("> ");
				od_input_str(ugh,30,' ','~');
				nl();
				od_disp_str("Are you sure? ");
				if(od_get_answer("YN")=='Y') {
					od_disp_str("Yes\r\n");
					if(strlen(ugh))
						SAFECOPY(player[b].pseudo,ugh);
				}
				else
					od_disp_str("No\r\n");
				break;
			case '#':
				od_disp_str("Change Battle Cry\r\n");
				vic(); 
				SAFECOPY(player[b].gaspd,ugh3);
				break;
		}
	}
	return(0);
}
