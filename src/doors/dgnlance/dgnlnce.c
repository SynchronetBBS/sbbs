#include <ctype.h>
#include <stdio.h>
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
char	emptykey=0;
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

void cls(void)
{
	od_clr_scr();
}

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

void ansic(int c)
{
	if(c==1 || c==0)
		c=0;
	else {
		if(c==2)
			c=7;
		else
			c=c-2;
	}
	switch(c) {
		case 0: addtofore=0; textcolor(7); textbackground(0); break;
		case 1: addtofore=8; textcolor(11); textbackground(0); break;
		case 2: addtofore=8; textcolor(14); textbackground(0); break;
		case 3: addtofore=0; textcolor(5); textbackground(0); break;
		case 4: addtofore=8; textcolor(15); textbackground(1); break;
		case 5: addtofore=0; textcolor(2); textbackground(0); break;
		case 6: addtofore=8; textcolor(12); textbackground(8); break;
		case 7: addtofore=8; textcolor(9); textbackground(0); break;
		case 8: addtofore=0; textcolor(1); textbackground(0); break;
		case 9: addtofore=8; textcolor(15); textbackground(0); break;
	}
}

void prompt(char *prmpt)
{
	char *p;

	for(p=prmpt; *p; p++) {
		switch(*p) {
			case 10:
				ansic(0);
				/* Fall through */
			default:
				od_putch(*p);
		}
	}
}

void nugetkey(char *c)
{
	if(emptykey) {
		*c=emptykey;
		emptykey=0;
		return;
	}
	*c=od_get_key(TRUE);
}

void pausescr(void)
{
	char	c;

	ansic(3);
	prompt("[Pause]");
	ansic(0);
	nugetkey(&c);
	prompt("\010 \010\010 \010\010 \010\010 \010\010 \010\010 \010\010 \010");
}

void nl(void)
{
	prompt("\r\n");
}

void print(char *pr)
{
	prompt(pr);
	nl();
}

void prt(char *i)
{
	ansic(4);
	prompt(i);
	ansic(0);
}

BOOL empty(void)
{
	if(emptykey)
		return(FALSE);
	emptykey=od_get_key(FALSE);
	if(emptykey)
		return(FALSE);
	return(TRUE);
}

void wkey(BOOL *abrt, BOOL *next)
{
	char	cc;

	for(;!(empty() || abrt);) {
		nugetkey(&cc);
		if(cc==' ' || cc==3 || cc==24 || cc==11)
			*abrt=TRUE;
		if(cc==14) {
			*abrt=TRUE;
			*next=TRUE;
		}
		if(cc==19 || cc=='P' || cc=='p')
			nugetkey(&cc);
	}
}

void printa(char *i, BOOL *abrt, BOOL *next)
{
	int	c;
	*abrt=FALSE;
	*next=FALSE;
	if(!empty())
		wkey(abrt, next);
	for(c=0; i[c];) {
		if(i[c]==3) {
			switch(i[c+1]) {
				case 0:
				case '0':
					ansic(0);
					break;
				case 1:
					ansic(1);
					break;
				case 2:
					ansic(2);
					break;
				case 3:
				case '1':
					ansic(3);
					break;
				case 4:
				case '2':
					ansic(4);
					break;
				case 5:
				case '3':
					ansic(5);
					break;
				case 6:
				case '4':
					ansic(6);
					break;
				case 7:
				case '5':
					ansic(7);
					break;
				case 8:
				case '6':
					ansic(8);
					break;
				case '7':
					ansic(9);
					break;
				case '8':
					ansic(10);
					break;
				case '9':
					ansic(11);
					break;
			}
		}
		else if(i[c]==1) {
			switch(i[c+1]) {
				case 'K': textcolor(0+addtofore); break;
				case 'R': textcolor(4+addtofore); break;
				case 'G': textcolor(2+addtofore); break;
				case 'Y': textcolor(6+addtofore); break;
				case 'B': textcolor(1+addtofore); break;
				case 'M': textcolor(5+addtofore); break;
				case 'C': textcolor(3+addtofore); break;
				case 'W': textcolor(7+addtofore); break;
				case '0': textbackground(0); break;
				case '1': textbackground(4); break;
				case '2': textbackground(2); break;
				case '3': textbackground(6); break;
				case '4': textbackground(1); break;
				case '5': textbackground(5); break;
				case '6': textbackground(3); break;
				case '7': textbackground(7); break;
				case 'H': addtofore=8; break;
				case 'N': 
					addtofore=0;
					textcolor(7);
					textbackground(0);
					break;
				case 1:
					od_putch(1);
					break;
				case 0:
					nl();
					break;
			}
		}
		if(!empty())
			wkey(abrt, next);
		if((i[c]==3 || i[c]==1) && i[c+1])
			c++;
		else
			od_putch(i[c]);
		c++;
	}
}

void printacr(char *i, BOOL *abrt, BOOL *next)
{
	if(!*abrt) {
		printa(i,abrt,next);
		if(*(lastchar(i))!=1)
			printa("\001",abrt,next);
	}
}

void pfl(char *fn, BOOL *abrt, BOOL cr)
{
	FILE	*fil;
	char	i[256];
	BOOL	next;
	INT16	linecount=0;

	fil=fopen(fn,"rb");
	if(fil==NULL) {
		prompt(fn);
		print(" not found.");
	}
	else {
		*abrt=FALSE;
		while(!feof(fil) && (!*abrt)) {
			fgets(i,sizeof(i),fil);
			truncsp(i);
			if(cr) {
				printacr(i,abrt,&next);
				if(++linecount == od_control.user_screen_length-1) {
					pausescr();
					linecount=0;
				}
			}
			else
				printa(i,abrt,&next);
		}
        fclose(fil);
	}
	nl();
	nl();
}

void printfile(char *fn)
{
	BOOL	abt;
	pfl(fn,&abt,TRUE);
}

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
	sprintf(temp, "%1.0f", player[b].bank);
	nl();
	ansic(3);
	prompt(temp);
	print(" Steel pieces are in the bank. ");
}

void withdrawbank(void)
{
	double	tempbank;
	tempbank=player[b].bank;
	if(tempbank<0)
		tempbank=0;
	player[b].gold=player[b].gold+tempbank;
	player[b].bank=player[b].bank-tempbank;
	sprintf(temp,"%1.0f",player[b].gold);
	nl();
	ansic(5);
	prompt("You are now carring ");
	prompt(temp);
	print(" Steel pieces.");
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
	INT16		q;
	INT16		v;
	INT16		spc;
	char		ts[256];
	char		tch[6];
	char		line[81];

	checkday();
	cls();
	nl();
	ansic(5);
	print("Hero Rankings:");
	print("!!!!!!!!!!!!!!");
	a=1;
	for(a=1;a<=number_of_players && !abrt; a++) {
		nl();
		sprintf(temp,"%f",a);
		sprintf(tempaa,"%2.0f",player[a].r);
		sprintf(tempcc,"%2.0f",player[a].wins);
		sprintf(tempdd,"%2.0f",player[a].loses);
		if(player[a].status==1)
			SAFECOPY(tch,"SLAIN");
		else
			SAFECOPY(tch,"ALIVE");
		ansic(5);
		if(a<10)
			prompt(" ");
		prompt(temp);
		prompt(".  ");
		ansic(3);
		prompt(player[a].pseudo);
		for(spc=1; spc<=(30-strlen(player[a].pseudo)); spc++)
			prompt(".");
		ansic(2);
		sprintf(line,"Lev=%s  W=%s  L=%s  S=%s",tempaa,tempcc,tempdd,tch);
		printa(line,&abrt,&next);
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
		fprintf(outfile,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
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
	char	tempts[51];
	healall=FALSE;
	cls();
	sprintf(temp,"%d",round(8*player[b].r));
	ansic(5);
	prompt("Clerics want ");
	prompt(temp);
	print(" Steel pieces per wound.");
	sprintf(tempts,"%1.0f",player[b].damage);
	ansic(4);
	prompt("You have ");
	prompt(tempts);
	print(" points of damage to heal.");
	ansic(3);
	prompt("How many do points do you want healed? ");
	od_input_str(temp,3,'0','9');
	opt=atoi(temp);
	if(opt<0)
		opt=0;
	if(healall)
		opt=player[b].damage;
	if(((round(opt))*(player[b].r)*10)>player[b].gold) {
		ansic(8);
		print("Sorry, you do not have enough Steel .");
		opt=0;
	}
	else if(opt>player[b].damage)
		opt=player[b].damage;
	player[b].damage=player[b].damage-opt;
	player[b].gold=player[b].gold-8*opt*player[b].r;
	sprintf(temp,"%1.0f",opt);
	prompt(temp);
	print(" hit points healed.");
}

void findo(void)
{
	nl();
	ansic(4);
	print("The Vile Enemy Dropped Something!");
	prompt("Do you want to get it? ");
	if(od_get_answer("YN")=='Y') {
		print("Y");
		okea=round(xp_random(99)+1);
		if ((okea < 10) && (player[b].weapon>=25)) {
			player[b].weapon=player[b].weapon+1;
			prompt("you have found a ");
			prompt(wname[round(player[b].weapon)]);
			print(".");
		}
		if ((okea > 11) && (okea < 40)) {
			player[b].gold=player[b].gold+40;
			print("you have come across 40 Steel pieces");
		}
		else
			print("it's gone!!!!");
	}
	else {
		print("N");
		print("I wonder what it was?!?");
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
	char	tmpstr[100];

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
	sprintf(temp,"%0.0f",opp.gold);
	nl();
	prompt("You take his ");
	prompt(temp);
	print(" Steel pieces.");
	player[b].gold=player[b].gold+opp.gold;
	if(doga) {
		nl();
		ansic(7);
		print("The Last Words He Utters Are...");
		nl();
		prompt("\"");
		prompt(player[a].gaspd);
		print("\"");
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
			ansic(7);
			print("You Hath Taken His Weapon.");
		}
		if(player[a].vehicle>player[b].vehicle) {
			d=player[b].vehicle;
			player[b].vehicle=player[a].vehicle;
			player[a].vehicle=d;
			ansic(4);
			print("You Hath Taken His Armour.");
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
	sprintf(temp,"%0.0f",z);
	prompt("You obtain ");
	prompt(temp);
	print(" exp points.");
	doga=FALSE;
}

void levelupdate(void)
{
	INT16 x,a;

	sprintf(temp,"%3d",round(player[b].r+1));
	ansic(7);
	sprintf(temp,"%9.0f",player[b].experience);
	if(player[b].experience>required[round(player[b].r+1)]) {
		player[b].r++;
		sprintf(tempaa,"%2.0f",player[b].r);
		ansic(4);
		prompt("Welcome to level ");
		prompt(tempaa);
		print("!");
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
	print("The Gods of Krynn Disapprove of your actions!");
	exit(1);
}

void amode(void)
{
	roll=playerattack();
	if(roll<1.5) {
		tint=round(xp_random(3)+1);
		switch(tint) {
			case 1:print("Swish you missed!"); break;
			case 2:print("HA HA! He dodges your swing!"); break;
			case 3:print("CLANG, The attack is blocked!"); break;
			case 4:print("You Fight vigorously and miss!"); break;
		}
	}
	else {
	      roll=playerattack2();
		if(roll>5*player[b].power)
			roll=5*player[b].power;
		opp.shield=opp.shield-roll;
		sprintf(temp,"%d",round(roll));
		tint=round(xp_random(2)+1);
		switch(tint) {
			case 1:
				prompt("You Sliced him for ");
				prompt(temp);
				print(".");
				break;
			case 2:
				prompt("You made contact to his body for ");
				prompt(temp);
				print(".");
				break;
			case 3:
				prompt("You hacked him for ");
				prompt(temp);
				print(".");
				break;
		}
		if(opp.shield<=0) {
			nl();
			tint=round(xp_random(3)+1);
			switch(tint) {
				case 1:print("A Painless Death!"); break;
				case 2:print("It Hath died!"); break;
				case 3:print("A Smooth killing!"); break;
				case 4:print("It has gone to the Abyss!"); break;
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
	INT16	bt;

	if((opp.shield>0) && live) {
		roll=opponentattack();
		if(roll<1.5) {
			ansic(7);
			print("His attack tears your armour.");
		}
		else {
			roll=opponentattack2();
			if(roll>5*opp.weapon2)
				roll=5*opp.weapon2;
			sprintf(temp,"%d",round(roll));
			tint=round(xp_random(2)+1);
			switch(tint) {
				case 1:
					prompt("He hammered you for ");
					prompt(temp);
					print(".");
					break;
				case 2:
					prompt("He swung and hit for ");
					prompt(temp);
					print(".");
					break;
				case 3:
					prompt("You are suprised when he hits you for ");
					prompt(temp);
					print(".");
					break;
			}
			player[b].damage=player[b].damage+roll;
			if(player[b].damage>=player[b].shield) {
				nl();
				tint=round(xp_random(3)+1);
				switch(tint) {
					case 1:print("Return This Knight To Huma's Breast...."); break;
					case 2:print("You Hath Been Slain!!"); break;
					case 3:print("Return Soon Brave Warrior!"); break;
					case 4:print("May Palidine Be With You!!"); break;
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
	char	word[21];

	cls();
	sprintf(tempaa,"%1.0f",player[b].r);
	sprintf(tempbb,"%1.0f",player[b].gold);
	sprintf(tempcc,"%2.0f",player[b].strength);
	sprintf(tempdd,"%2.0f",player[b].intelligence);
	sprintf(tempee,"%2.0f",player[b].luck);
	sprintf(tempff,"%2.0f",player[b].dexterity);
	sprintf(tempmm,"%2.0f",player[b].constitution);
	sprintf(tempnn,"%2.0f",player[b].charisma);
	sprintf(tempgg,"%1.0f",player[b].experience);
	sprintf(temphh,"%1.0f",player[b].shield-player[b].damage);
	sprintf(tempii,"%1.0f",player[b].shield);
	sprintf(tempjj,"%1.0f",player[b].bank);
	sprintf(tempkk,"%d",player[b].flights);
	sprintf(tempoo,"%d",round(3-dog));
	sprintf(temppp,"%d",round(player[b].wins));
	sprintf(tempqq,"%d",round(player[b].loses));
	sprintf(templl,"%d",10-trips);

	ansic(5);
	prompt("Name: ");
	prompt(player[b].pseudo);
	prompt("   Level: ");
	print(tempaa);
	ansic(3);
	prompt("W/L: ");
	prompt(temppp);
	prompt("/");
	prompt(tempqq);
	prompt("   Exp: ");
	print(tempgg);
	nl();
	ansic(4);
	prompt("Steel  (in hand): ");
	print(tempbb);
	ansic(4);
	prompt("Steel  (in bank): ");
	print(tempjj);
	nl();
	ansic(2);
	prompt("Battles: ");
	prompt(tempoo);
	prompt("   Retreats: ");
	prompt(tempkk);
	prompt("    Fights: ");
	prompt(templl);
	prompt("   Hps: ");
	prompt(temphh);
	prompt("(");
	prompt(tempii);
	print(")");
	nl();
	ansic(3);
	prompt("Weapon: ");
	prompt(wname[round(player[b].weapon)]);
	prompt("     Armor: ");
	print(sname[round(player[b].vehicle)]);
}

void incre(void)
{
	prompt("Increase which stat? ");
	switch(od_get_answer("123456Q")) {
		case '1':print("1"); player[b].strength++; break;
		case '2':print("2"); player[b].intelligence++; break;
		case '3':print("3"); player[b].dexterity++; break;
		case '4':print("4"); player[b].luck++; break;
		case '5':print("5"); player[b].constitution++; break;
		case '6':print("6"); player[b].charisma++; break;
		case 'Q':print("Q"); partone=FALSE; break;
	}
}

void decre(void)
{
	prompt("Decrease which stat? ");
	switch(od_get_answer("123456")) {
		case '1':print("1"); player[b].strength-=2; break;
		case '2':print("2"); player[b].intelligence-=2; break;
		case '3':print("3"); player[b].dexterity-=2; break;
		case '4':print("4"); player[b].luck-=2; break;
		case '5':print("5"); player[b].constitution-=2; break;
		case '6':print("6"); player[b].charisma-=2; break;
	}
}

void ministat(void)
{
	yaya=TRUE;
	partone=FALSE;
	sprintf(tempaa,"%1.0f",player[b].strength);
	sprintf(tempbb,"%1.0f",player[b].intelligence);
	sprintf(tempdd,"%1.0f",player[b].dexterity);
	sprintf(tempcc,"%1.0f",player[b].luck);
	sprintf(tempee,"%1.0f",player[b].constitution);
	sprintf(tempff,"%1.0f",player[b].charisma);
	nl();
	print("Status Change:");
	print("^^^^^^^^^^^^^^");
	prompt("1> Str: ");
	print(tempaa);
	prompt("2> Int: ");
	print(tempbb);
	prompt("3> Dex: ");
	print(tempdd);
	prompt("4> Luk: ");
	print(tempcc);
	prompt("5> Con: ");
	print(tempee);
	prompt("6> Chr: ");
	print(tempff); 
	nl();
	if(player[b].strength < 6) {
		yaya=FALSE;
		nl();
		print("Strength cannot go below 6");
	}
	if(player[b].intelligence < 6) {
		yaya=FALSE;
		nl();
		print("Intelligence cannot go below 6");
	}
	if(player[b].dexterity < 6) {
		yaya=FALSE;
		nl();
		print("Dexterity cannot go below 6");
	}
	if(player[b].luck < 6) {
		yaya=FALSE;
		nl();
		print("Luck cannot go below 6");
	}
	if(player[b].constitution < 6) {
		yaya=FALSE;
		nl();
		print("Constitution cannot go below 6");
	}
	if(player[b].charisma < 6) {
		yaya=FALSE;
		nl();
		print("Charisma cannot go below 6");
	}
	if(yaya) {
		prompt("Is this correct? ");
		if(od_get_answer("YN")=='Y') {
			print("Y");
			player[b].strength=temp1a;
			player[b].intelligence=temp1b;
			player[b].dexterity=temp1d;
			player[b].luck=temp1c;
			player[b].constitution=temp1e;
			player[b].charisma=temp1f;
			bothover=FALSE;
		}
		else {
			print("N");
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
	cls();
	bothover=FALSE;
	temp1a=player[b].strength;
	temp1b=player[b].intelligence;
	temp1c=player[b].luck;
	temp1d=player[b].dexterity;
	temp1e=player[b].constitution;
	temp1f=player[b].charisma;

	sprintf(tempaa,"%1.0f",player[b].strength);
	sprintf(tempbb,"%1.0f",player[b].intelligence);
	sprintf(tempdd,"%1.0f",player[b].dexterity);
	sprintf(tempcc,"%1.0f",player[b].luck);
	sprintf(tempee,"%1.0f",player[b].constitution);
	sprintf(tempff,"%1.0f",player[b].charisma);

	for(;;) {
		partone=TRUE;
		print("Status Change:");
		print("@@@@@@@@@@@@@");
		print("You may increase any stat by one,");
		print("yet you must decrease another by two.");
		nl();
		prompt("1> Str: ");
		print(tempaa);
		prompt("2> Int: ");
		print(tempbb);
		prompt("3> Dex: ");
		print(tempdd);
		prompt("4> Luk: ");
		print(tempcc);
		prompt("5> Con: ");
		print(tempee);
		prompt("6> Chr: ");
		print(tempff); 
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
	unsigned char	buf[101];
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
	char	tmpgg[11];
	char	tmphh[3];
	char	tch[6];
	INT16	spc,a;
	BOOL	gono;
	char	line[81];

	if(dog<=3) {
		nl();
		checkday();
		for(finder=FALSE; finder==FALSE;) {
			for(enemy=0; enemy!=0;) {
				cls();
				ansic(4);
				print("Battle Another Hero:");
				print("********************");
				if(number_of_players==1) {
					print("You are the only hero on Krynn!");
					return;
				}
				else {
					a=1;
					for(a=1; a<=number_of_players && !abrt;) {
						if(player[a].r>player[b].r-4) {
							nl();
							sprintf(temp,"%d",a);
							sprintf(tempaa,"%2.0f",player[a].r);
							sprintf(tempcc,"%2.0f",player[a].wins);
							sprintf(tempdd,"%2.0f",player[a].loses);
							if(player[a].status==1)
								SAFECOPY(tch,"SLAIN");
							else
								SAFECOPY(tch,"ALIVE");
							ansic(5);
							if(a<10) {
								prompt(" ");
								prompt(temp);
								prompt(".  ");
							}
							else {
								prompt(temp);
								prompt(".  ");
							}
							ansic(3);
							prompt(player[a].pseudo);
							for(spc=1;spc<=30-strlen(player[a].pseudo);spc++)
								prompt(".");
							ansic(2);
							sprintf(line,"Lev=%s  W=%s  L=%s  S=%s",tempaa,tempcc,tempdd,tch);
							printa(line,&abrt,&next);
						}
						a+=1;
					}
					nl();
					ansic(3);
					prompt("Enter the rank # of your opponent: ");
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
	double 	loss,playerrem;
	char		tempxx[51];

	nl();
	live=TRUE;
	while(live==TRUE) {
		playerrem=player[b].shield-player[b].damage;
		sprintf(tempxx,"%1.0f",playerrem);
		nl();
		ansic(4);
		prompt("You are attacked by a ");
		prompt(opp.name);
		print(".");
		for(option='?'; option=='?';) {
			ansic(2);
			prompt("Combat (");
			prompt(tempxx);
			prompt(" hps): (B,F,S): ");
			option=od_get_answer("BFS?");
			if(option=='?') {
				nl();
				nl();
				print("(B)attle your opponent.");
				print("(F)lee from your opponent.");
				print("(S)tatus check.");
				nl();
			}
		}
		switch(option) {
			case 'B':attackmodes(); break;
			case 'S':statshow(); break;
			case 'F':
				if((xp_random(4)+1)+player[b].dexterity>opp.dexterity) {
					nl();
					ansic(7);
					print("You Ride away on a Silver Dragon.");
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
	cls();
	ansic(7);
	print("Dragonlance 3.0 Credits");
	ansic(7);
	print("@@@@@@@@@@@@@@@@@@@@@@@");
	print("Original Dragonlance   :  Raistlin Majere & TML   ");
	print("Special Thanks To      : The Authors Of Dragonlance");
	print("Dragonlance's Home     : The Tower Of High Sorcery");
	print("Originally modified from the Brazil Source Code.");
	print("C Port                 : Deuce");
	print("Home Page              : http://doors.synchro.net/");
	print("Support                : deuce@nix.synchro.net");
	nl();
	pausescr();
}

void docs()
{
	cls();
	printfile("text/docs.lan");
	pausescr();
}

void vic(void)
{
	BOOL ahuh;

	for(ahuh=FALSE; !ahuh; ) {
		nl();
		ansic(3);
		print("Enter your new Battle Cry.");
		prompt(">");
		od_input_str(ugh3, 60, ' ', '~');
		prompt("Is this correct? ");
		if(od_get_answer("YN")=='Y') {
			ahuh=TRUE;
			print("Y");
		}
		else {
			ahuh=FALSE;
			print("N");
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

void strait(void)
{
	INT16	p;
	INT16	add;

	add=25-strlen(strip);
	for(p=1;p<=add;p++)
		strcat(strip," ");
}

void stripe(void)
{
	INT16	p;
	INT16	add;

	add=25-strlen(cross);
	for(p=1;p<=add;p++)
		strcat(cross," ");
}

void weaponlist(void)
{
	char	line[81];

	nl();
	nl();
	a=1;
	print("  Num. Weapon                      Armour                          Price   ");
	print("(-------------------------------------------------------------------------)");
	for(a=1; a<=25 && !abrt; a++) {
		sprintf(temp,"%9.0f",cost[a]);
		sprintf(opp.name,"%2d",a);
		ansic(4);
		SAFECOPY(strip,wname[a]);
		strait();
		SAFECOPY(cross,sname[a]);
		stripe();
		sprintf(line,"  %s>  %s   %s   %s",opp.name,strip,cross,temp);
		printa(line,&abrt,&next);
		nl();
	}
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
	char	tmpvv[3];
	cls();
	ansic(4);
	print("Weapon & Armour Shop");
	ansic(7);
	print("$$$$$$$$$$$$$$$$$$$$");
	while(1) {
		nl();
		prompt("(B)rowse, (S)ell, (P)urchase, or (Q)uit? ");
		opty=od_get_answer("BSQP");
		switch(opty) {
			case 'Q':
				print("Q");
				return;
			case 'B':
				print("B");
				weaponlist();
				break;
			case 'P':
				print("P");
				nl();
				nl();
				prompt("Enter weapon/armour # you wish buy: ");
				od_input_str(temp,2,'0','9');
				buy=atoi(temp);
				if(buy==0)
					return;
				if(cost[round(buy)]>player[b].gold)
					print("You do not have enough Steel .");
				else {
					nl();
					prompt("(W)eapon or (A)rmour: ");
					opty=od_get_answer("WA");
					switch(opty) {
						case 'W':
							print("W");
							prompt("Are you sure you want buy it? ");
							if(od_get_answer("YN")=='Y') {
								player[b].gold-=cost[round(buy)];
								player[b].weapon=buy;
								nl();
								ansic(5);
								prompt("You've bought a ");
								print(wname[round(buy)]);
								player[b].attack=find1();
								player[b].power=find2();
							}
							break;
						case 'A':
							print("A");
							prompt("Are you sure you want buy it? ");
							if(od_get_answer("YN")=='Y') {
								player[b].gold-=cost[round(buy)];
								player[b].vehicle=buy;
								nl();
								ansic(5);
								prompt("You've bought a ");
								print(sname[round(buy)]);
							}
							break;
					}
				}
				break;
			case 'S':
				print("S");
				nl();
				prompt("(W)eapon,(A)rmour,(Q)uit : ");
				opty=od_get_answer("AWQ");
				switch(opty) {
					case 'Q':
						print("Q");
						return;
					case 'W':
						print("W");
						y=(round(player[b].weapon));
						x=player[b].charisma;
						x=x*cost[y];
						x=((1/20)*x);
						sprintf(temp,"%0.0f",x);
						nl();
						prompt("I will purchase it for ");
						prompt(temp);
						prompt(", okay? ");
						opty=od_get_answer("YN");
						if(opty=='Y') {
							print("Y");
							ansic(7);
							print("Is it Dwarven Made?");
							player[b].weapon=1;
							player[b].gold=player[b].gold+x;
						}
						else
							print("N");
						break;
					case 'A':
						x=((1/20)*(player[b].charisma)*(cost[round(player[b].vehicle)]));
						sprintf(temp,"%7.0f",x);
						nl();
						prompt("I will purchase it for ");
						prompt(temp);
						prompt(", okay? ");
						opty=od_get_answer("YN");
						if(opty=='Y') {
							print("Y");
							print("Fine Craftsmanship!");
							player[b].vehicle=1;
							player[b].gold=player[b].gold+x;
						}
						else
							print("N");
						break;
				}
		}
	}
}

void listplayers(void)
{
	INT16	a;
	cls();
	ansic(4);
	print("Heroes That Have Been Defeated");
	ansic(5);
	print("+++++++++++++++++++++++++++++++");
	readla();
	for(a=1;a<number && !abrt; a++) {
		ansic(3);
		printa(result[a],&abrt,&next);
	}
}

void spy(void)
{
	char	aa[31];
	char	bb[31];
	char	cc[31];
	char	dd[31];
	char	ee[31];
	char	ff[31];
	char	gg[31];
	char	hh[31];
	INT16	a;
	cls();
	print("Spying on another user eh.. well you may spy, but to keep");
	print("you from copying this person's stats, they will not be   ");
	print("available to you.  Note that this is gonna cost you some ");
	print("cash too.  Cost: 20 Steel pieces                         ");
	nl();
	prompt("Who do you wish to spy on? ");
	od_input_str(aa,sizeof(aa)-1,' ','~');
	for(a=1;a<=number_of_players;a++) {
		if(!stricmp(player[a].pseudo,aa)) {
			if(player[b].gold<20) {
				ansic(8);
				print("You do not have enough Steel!");
			}
			else {
				player[b].gold-=20;
				sprintf(bb,"%1.0f",player[a].r);
				sprintf(cc,"%1.0f",player[a].experience);
				sprintf(dd,"%d",player[a].flights);
				sprintf(ee,"%1.0f",(player[a].shield-player[a].damage));
				sprintf(ff,"%1.0f",player[a].bank);
				sprintf(gg,"%1.0f",player[a].gold);
				sprintf(hh,"%1.0f",player[a].shield);
				nl();
				ansic(8);
				print(player[a].pseudo);
				nl();
				prompt("Level  :");
				print(bb);
				prompt("Exp    :");
				print(cc);
				prompt("Flights:");
				print(dd);
				prompt("Hps    :");
				prompt(ee);
				prompt("(");
				prompt(hh);
				print(")");
				nl();
				ansic(5);
				prompt("Weapon :");
				print(wname[round(player[a].weapon)]);
				ansic(5);
				prompt("Armour :");
				print(sname[round(player[a].vehicle)]);
				nl();
				ansic(4);
				prompt("Steel  (in hand):");
				print(gg);
				ansic(4);
				prompt("Steel  (in bank):");
				print(ff);
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
		print("The Shooting Gallery is closed until tomorrow!");
	else {
		cls();
		print("  Welcome to the Shooting Gallery");
		print(" Maximum wager is 25,000 Steel pieces");
		nl();
		prt("How many Steel pieces do you wish to wager? ");
		od_input_str(tempgd,sizeof(tempgd)-1,'0','9');
		realgold=round(atof(tempgd));
		if(realgold>player[b].gold) {
			nl();
			print("You do not have enough Steel !");
		}
		if ((realgold!=0) && ((player[b].gold>=realgold) && (realgold<=25000) && (realgold>=1))) {
			okea=round(xp_random(99)+1);
			if(okea <= 3) {
				realgold*=100;
				player[b].gold+=realgold;
				sprintf(tempgd,"%1.0f",realgold);
				prompt("You shot all the targets and win ");
				prompt(tempgd);
				print(" Steel pieces!");
			}
			else if ((okea>3) && (okea<=15)) {
				realgold*=10;
				player[b].gold+=realgold;
				sprintf(tempgd,"%1.0f",realgold);
				prompt("You shot 50% of the targets and win ");
				prompt(tempgd);
				print(" Steel pieces!");
			}
			else if ((okea>15) && (okea<=30)) {
				realgold*=3;
				player[b].gold+=realgold;
				sprintf(tempgd,"%1.0f",realgold);
				prompt("You shot 25% of the targets and win ");
				prompt(tempgd);
				print(" Steel pieces!");
			}
			else {
				player[b].gold-=realgold;
				print("Sorry You Hath Lost!");
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
			ansic(5);
			print("It's Getting Dark Out!");
			ansic(5);
			print("Return to the Nearest Inn!");
			uni=FALSE;
		}
		else {
			cls();
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

	cls();
  	countr=0;
	endfil=FALSE;
	printfile("text/bullet.lan");
	nl();
	prompt("Do you wish to enter a News Bulletin? ");
	if(od_get_answer("YN")=='Y') {
		print("Y");
		nl();
		while(!endfil) {
			countr++;
			sprintf(tempcoun,"%d",countr);
			prt(tempcoun);
			prt("> ");
			od_input_str(blt[countr],60,' ','~');
			if(countr==4)
				endfil=TRUE;
		}
		nl();
		prompt("Is the bulletin correct? ");
		if(od_get_answer("YN")=='Y') {
			print("Y");
			print("Saving Bulletin...");
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
			print("N");
	}
	else
		print("N");
}

void training(void)
{
	char		temptrain[2];
	INT16		realtrain;
	INT16		tttgld;

	nl();
	if(trips>10)
		print("The Training Grounds are closed until tomorrow!");
	else {
		cls();
		print("Training Grounds");
		print("%%%%%%%%%%%%%%%%");
		print("Each characteristic you wish to upgrade");
		print("will cost 1,000,000 Steel pieces per point.");
		nl();
		tttgld=10000;
		prompt("Do you wish to upgrade a stat? ");
		if(od_get_answer("YN")=='Y') {
			if(player[b].gold<(tttgld*100))
				print("Sorry, but you do not have enough Steel!");
			else {
				nl();
				print("1> Strength       2> Intelligence");
				print("3> Dexterity      4> Luck        ");
				print("5> Constitution   6> Charisma    ");
				nl();
				prt("Which stat do you wish to increase? ");
				temptrain[1]=0;
				temptrain[0]=od_get_answer("123456");
				print(temptrain);
				realtrain=temptrain[0]-'0';
				prompt("Are you sure? ");
				if(od_get_answer("YN")=='Y') {
					print("Y");
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
					print("N");
			}
		}
		else
			print("N");
	}
}

void menuit(void)
{
	cls();
	printfile("text/menu.lan");
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
	cls();
	prt("----------");ansic(8);prompt("   -=-=DRAGONLANCE=-=-      ");prt("    /\\     ");nl();
	prt("\\        /");ansic(1);prompt("       Version 3.0          ");prt("    ||     ");nl();
	prt(" \\      / ");ansic(1);prompt("                            ");prt("    ||     ");nl();
	prt("  \\    /  ");ansic(2);prompt("  Welcome To The World of   ");prt("    ||     ");nl();
	prt("   |  |   ");ansic(3);prompt("  Krynn, Where the gods     ");prt("    ||     ");nl();
	prt("  /    \\  ");ansic(2);prompt("  of good and evil battle.  ");prt("  \\ || /   ");nl();
	prt(" /      \\ ");ansic(3);prompt("  Help the People Of Krynn. ");prt("   \\==/    ");nl();
	prt("/        \\");ansic(1);prompt("                            ");prt("    ##     ");nl();
	prt("----------");ansic(8);prompt("        ON WARD!!!          ");prt("    ##     ");nl();
	nl();
	print("News Bulletin:");
	nl();
	printfile("text/bullet.lan");
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
		ansic(3);
		prompt("A defeat was lead over you by ");
		prompt(player[b].killer);
		print(".");
	}
	checkday();
	if(player[b].flights<1) {
		trips=12;
		dog=4;
	}
	player[b].flights--;
	nl();
	pausescr();
	cls();
	infile=fopen("data/weapons.lan","rb");
	ansic(2);
	for(a=1; a<=25; a++) {
		fgets(wname[a],sizeof(wname[a]),infile);
		truncsp(wname[a]);
		w2[a]=readnumb();
		w3[a]=readnumb();
		endofline();
	}
	fclose(infile);
	prompt("\r");
	infile=fopen("data/armor.lan","rb");
	ansic(2);
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
	ansic(4);
	player[b].status=0;
	vary2=1;
	statshow();
	mg=FALSE;
	for(play=TRUE; play && gonado;) {
		levelupdate();
		vary2=1;
		if (((player[b].wins+1)*4)-(player[b].loses)<0) {
			nl();
			print("As you were Travelling along a Wilderness Path an   ");
			print("Evil Wizard Confronted You.  When you tried to fight");
			print("Him off, he cast a Spell of Instant Death Upon you.  ");
			print("Instantly you were slain by the Archmage, Leaving you");
			print("as nothing more than a pile of ashes.  Re-Rolled!   ");
			nl();
			pausescr();
			create(FALSE);
			player[b].flights--;
		}
		nl();
		nl();
		ansic(4);
		prompt("Command (?): ");
		opty=od_get_answer("QVP12345CHWLADGFRSTX+-?EZ*#");
		temp[0]=opty;
		temp[1]=0;
		print(temp);
		switch(opty) {
			case 'Q':
				prompt("LEAVE KRYNN? Are you sure? ");
				if(od_get_answer("YN")=='Y') {
					print("Y");
					play=FALSE;
				}
				else
					print("N");
				break;
			case '1':afight(1); break;
			case '2':afight(2); break;
			case '3':afight(3); break;
			case '4':afight(4); break;
			case '5':afight(5); break;
			case 'C':chstats(); break;
			case 'H':heal(); break;
			case 'W':weaponshop(); break;
			case 'L':levelupdate(); break;
			case 'A':doggie(); break;
			case 'D':docs(); break;
			case 'G':gamble(); break;
			case 'F':listplayers(); break;
			case 'R':playerlist(); break;
			case 'S':statshow(); break;
			case 'T':training(); break;
			case 'X':
				print("Please note that this will completely purge");
                        print("your current hero of all atributes!");
				nl();
                        prompt("Are you sure you want to REROLL your character? ");
				if(od_get_answer("YN")=='Y') {
					print("Y");
					create(FALSE);
					player[b].flights--;
				}
				else
					print("N");
				break;
			case '+':depobank(); break;
			case 'P':printfile("text/plug.lan"); break;
			case '-':withdrawbank(); break;
			case '?':menuit(); break;
			case 'E':bulletin(); break;
			case 'Z':spy(); break;
			case 'V':
				ansic(2); 
				print("This Is Dragonlance version 3.0");
				pausescr();
				break;
			case '*':
				nl();
				ansic(3);
				print("Your family crest has been stolen, they");
				ansic(3);
				print("inscribe a new one with the name...");
				prompt(">");
				od_input_str(ugh,30,' ','~');
				nl();
				prompt("Are you sure? ");
				if(od_get_answer("YN")=='Y') {
					print("Y");
					if(strlen(ugh))
						SAFECOPY(player[b].pseudo,ugh);
				}
				else
					print("N");
				break;
			case '#':
				vic(); 
				SAFECOPY(player[b].gaspd,ugh3);
				break;
		}
	}
	return(0);
}
