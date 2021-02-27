/* Copyright 2004 by Darryl Perry
 * 
 * This file is part of Space Dynasty Elite.
 * 
 * Space Dynasty Elite is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Foobar; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _DYNASTY_H_
#define _DYNASTY_H_

#define __UNIX__

#ifdef __UNIX__
#else
#include <dos.h>
#endif
#include <stdlib.h>
#include <time.h> 


#define	TROOPS 		0
#define	FIGHTERS	1
#define	DEFSTATN	2
#define	GENERALS	3
#define	CARRIERS	4
#define	HVYCRSRS	5
#define	CAGENTS		6

#define PAYAF		0
#define PAYPOP		1
#define PAYCOPS		2
#define FEEDPOP		3
#define FEEDAF		4

#ifdef __UNIX__
#define randomize() srand((unsigned)time(NULL)|1)
#define strcmp strcasecmp
#endif

typedef struct{
	int ddaya;
	char dayb[11];
	char bbsn[31];
	char loc[31];
	char Sanspath[91];
	char Stxtpath[91];
	char Nanspath[91];
	char Ntxtpath[91];
	char Yanspath[91];
	char Ytxtpath[91];
	int todayplayers;
	long mktfood;
}cfgrec;

typedef struct{
	char moniker[31];
	int  turns;
	long credits;
	long score;
	int population;
	int plano,planf,plans;
	int troops,fighters,defstatns;
	int carriers,generals;
	int food;
}regrec;

typedef struct{
	char realname[36];
	char moniker[36];
	char laston[11];
	char password[9];
	int insurgency;
	int alliances[25];
	int ldp,key;
	int turns;
	long score,
		 cs,
		 credits,
		 population,
		 food,
		 troops,
		 fighters,
		 generals,
		 defstatns,
		 carriers,
		 cruisers,
		 planets,
		 planf,
		 plano,
		 plans,
		 agents;

}plyrrec;


cfgrec cfg;
regrec reg;
plyrrec plyr,othr;


char insurtxt[9][25];
char diptxt[4][21];
char cvstr3[5][25];
char cvstr2[5][16];
char cvstr1[5][16];
long cvamt[5];



char gamedir[81],
	 workdir[81],
	 datetxt[21],
	 username[31],
	 passwdtxt[9];

int isnewday,
   alout,
   daya,
   faflag,
   fpflag,
   crpflag,
   plmflag,
   armfflag;
int checkifdone[20];
long xx,yy,zz,rnd_ret,xxx,yyy,lost_off,lost_def;

void gamedit();
void plyredit();
void centline(char *str);
void q2bbs();
char yesno();
char noyes();
void pausescr();
void makeplyr();
void listscores();
void outscores();
void readmail();
void showmail();
void showglbl();
void status();
void update();
void newplyr();
void armfcred();
void plmaint();
void crimprev();
void fdcmmkt();
void feedpopn();
void feedarmy();
void covrtact();
void results();
int continuey();
void commktmu();
void commkt();
void buyplan();
void buyitems(int which);
void attack();
void takeplan(long winplan);
void getforce();
void fight();
void trade();
int hilo(int lo, int hi);
int wplyr(int fl1,int fl2);
void engage(long commship, long offense, long defense);
void brtr();
void qas();
void embsycon();
void pamnu();
void viewrltn();
void downinsu();
void insres();
void treaty(int trtype);
void pab();
void sendmail();
void bulletins();
void delplyr();
void maint();
void bombloc(int ww);
void makealiance(int p1, int p2, int al1, int al2);
void dplomenu();
void chckali();
char *strrepl(char *Str, size_t BufSiz, char *OldStr, char *NewStr);
int randomx(int maxnum);
void dirname(char *path);
int findaliance(int p1, int p2);
int fexist(char *fname);
void nl();

#endif  /* _DYNASTY_H_ */
