/* Copyright 2004 by Darryl Perry
 * 
 * This file is part of Foobar.
 * 
 * Foobar is free software; you can redistribute it and/or modify
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "dynasty.h"
#include "defs.h"
#include <OpenDoor.h>
#include "mci.h"
#include "commafmt.h"
#include "sde_input.h"

void godmenu();
void gamelog(char *logentry);
void logon();
int readplyr(int recno, plyrrec *pr);
int saveplyr(int recno, plyrrec *pr);
int IsInactivePlayer();
void WriteNews(char *newstxt, ...);
void initnews();
void dailynews();
int DisplayFile(char *fname);
void init_othr();
void WriteText(char *fname, char *newstxt, ...);
void makescoresfile();
int flexist(char *fname);
void engage1(long commship, long offense, long defense);
void mainmenu();

void init(int argv, char *argc[])
{
	strcpy(od_control.od_prog_name, "Space Dynasty Elite");
		strcpy(od_control.od_prog_version, "Version 3.0");
	strcpy(od_control.od_prog_copyright, "Copyright 2000-2003 by Darryl Perry");
	strcpy(od_control.od_logfile_name, "sdynasty.log");
	strcpy(od_registered_to, "Darryl Perry");
	strcpy(od_control.user_name,"Darryl Perry");
	od_parse_cmd_line(argv, argc); 
	od_registration_key = 10664;
	od_init(); 
	randomize();
}

void gamelog(char *logentry)
{
	time_t t;
	struct tm *tt;
	FILE *fptr;
	char path[91],dtxt[31];

	sprintf(path,"%s%s",gamedir,od_control.od_logfile_name);
	fptr=fopen(path,"at+");
	if(fptr!=NULL) {
		time(&t);
		tt=localtime(&t);
		strftime(dtxt,21,"%m/%d/%Y %H:%M",tt);
		sprintf(path,"%s # %s # %s\n",dtxt,plyr.moniker,logentry);
		fputs(path,fptr);
		fclose(fptr);
		}
}

void nl()
{
	mci("\n\r");
}

void intro()
{
	mci("~SC~SMWelcome to Space Dyansty Elite~SM~SP");
}

int DisplayFile(char *fname)
{
	FILE *fptr;
	char path[91],ttxt[128];
	int retval=0,count=0,done=FALSE,cont=FALSE;

	sprintf(path,"%s%s",gamedir,fname);
	fptr=fopen(path,"rt");
	if(fptr!=NULL){
		retval=TRUE;
		while(fgets(ttxt,127,fptr) && !done){
				mci(ttxt);
				count++;
				if(count>23 && !cont){
					mci("More? (Y/n/c): ");
					switch(od_get_answer("YNC")){
						case 'Y':	
							mci("~SM");
							count=0;
							break;
						case 'N':	
							done=TRUE;
							mci("~SC");
							break;
						case 'C':
							cont=TRUE;
							break;
						}
					}
				}
		fclose(fptr);
		}
	return(retval);
}

int savereg()
{
	FILE *fptr;
	char path[91];

	sprintf(path,"%ssdynasty.reg",gamedir);
	fptr=fopen(path,"wb");
	if(fptr!=NULL){
		fwrite(&reg,sizeof(regrec),1,fptr);
		fclose(fptr);
		return(TRUE);
		}
	return(FALSE);
}

int readreg()
{
	FILE *fptr;
	char path[91];

	sprintf(path,"%ssdynasty.reg",gamedir);
	fptr=fopen(path,"rb");
	if(fptr!=NULL){
		fread(&reg,sizeof(regrec),1,fptr);
		fclose(fptr);
		return(TRUE);
		}
	return(FALSE); 
}

int savecfg()
{
	FILE *fptr;
	char path[91];

	sprintf(path,"%ssdynasty.cfg",gamedir);
	fptr=fopen(path,"wb");
	if(fptr!=NULL){
		fwrite(&cfg,sizeof(cfgrec),1,fptr);
		fclose(fptr);
		return(TRUE);
		}
	return(FALSE);
}

int readcfg()
{
	FILE *fptr;
	char path[91];

	sprintf(path,"%ssdynasty.cfg",gamedir);
	fptr=fopen(path,"rb");
	if(fptr!=NULL){
		fread(&cfg,sizeof(cfgrec),1,fptr);
		fclose(fptr);
		return(TRUE);
		}
	return(FALSE); 
}

int IsValidName(char *uname)
{
	int i=0,x=-1;
	while(readplyr(i++,&othr) && x<0){
		if(strcmp(othr.realname,uname)==0){
			x=othr.key;
			}
		}	
	return(x);
}

int IsValidAlias(char *uname)
{
	int i=0,x=-1;
	while(readplyr(i++,&othr) && x==-1){
		if(strcmp(othr.moniker,uname)==0){
			x=othr.key;
			}
		}	
	return(x);
}

int IsInactivePlayer()
{
	int i=0,x=-1;
	while(readplyr(i++,&othr) && x==-1){
		if((daya-othr.ldp)>=10){
			x=othr.key;
			}
		}
	return(x);
}

void logon()
{
	int i;

	i=IsValidName(od_control.user_name);
	if(i<0){
		mci("~SM  User %s not found.~SM  Create new account? ",
			od_control.user_name);
		newplyr();		
		}
	else{
		readplyr(i,&plyr);
		}
	if(daya!=plyr.ldp){
		strcpy(plyr.laston,datetxt);
		plyr.turns=reg.turns;
		plyr.ldp=daya;
		}
	if(plyr.turns < 1){
		mci("~SM%s, you must wait until tommorrow to play.~SM~SP",plyr.moniker);
		}
	else
		{
		showglbl();
		}
}

void make_today()
{
	time_t t;
	struct tm *tt;
	time(&t);
	tt=localtime(&t);
	strftime(datetxt,21,"%m/%d/%Y",tt);
}

void begining()
{
	int i,cnt;

	dirname(gamedir);
	isnewday=FALSE;
	make_today();

	if(!readreg()){
		reg.turns=100;
		reg.credits=50000;
		reg.food=7600;
		reg.plano=1;
		reg.planf=1;
		reg.plans=0;
		reg.carriers=1;
		reg.generals=2;
		reg.troops=100;
		reg.population=6;
		savereg();
		}

	if(!readcfg()){
		strcpy(cfg.bbsn,"Noname BBS");
		strcpy(cfg.loc,"Nowhere, USA");
		strcpy(cfg.dayb,datetxt);
		cfg.Nanspath[0]='\0';
		cfg.Ntxtpath[0]='\0';
		cfg.Yanspath[0]='\0';
		cfg.Ytxtpath[0]='\0';
		cfg.Sanspath[0]='\0';
		cfg.Stxtpath[0]='\0';
		cfg.ddaya=1;
		cfg.todayplayers=0;
		cfg.mktfood=reg.food;
		isnewday=TRUE;
		savecfg();
		}
	else{
		if(strcmp(cfg.dayb,datetxt)!=0){
			strcpy(cfg.dayb,datetxt);
			cfg.ddaya++;
			isnewday=TRUE;
			savecfg();
			}
		}
	readcfg();
	daya=cfg.ddaya;
	intro();
	if(!fexist("sdplyr.dat"))
		makeplyr();

	if(isnewday)		maint();
	cfg.todayplayers++;
	savecfg();
	if(!flexist(cfg.Nanspath))	initnews();
	gamelog("... Begins playing");

	cnt=0;
	for(i=0;i<24;i++){
		readplyr(i,&othr);
		if(strcmp(othr.realname,"empty")!=0){
			cnt++;
			}
		}

	while(1){
		mci("~SC~SM");
		center("`0EWelcome to~SM~SM");
		center("`09Space Dynasty Elite~SM~SM");
		center("`0ECopyright 1999-2003 by Darryl Perry.~SM");
		center("`0EAll Rights Reserved.~SM~SM");
		center("`0DThis game has been running for `0F%d `0Ddays.~SM",cfg.ddaya);
		center("`0AThere are `0F%d`0A active players.~SM",cnt);
		center("`0CThe GameOp allows for `0F%d`0C turns a day.~SM",reg.turns);

		mci("~SM");
		center("`0E(`0FB`0E)`0Begin Space Dynasty Elite   `0E(`0FT`0E)`0Boday's News    ~SM");
		center("`0E(`0FL`0E)`0Bist Dynasties              `0E(`0FY`0E)`0Besterday's News~SM");
		center("`0E(`0FI`0E)`0Bnstructions                `0E(`0FR`0E)`0Bead Mail       ~SM");
		center("`0E(`0FQ`0E)`0Buit                        `0E(`0FW`0E)`0Brite Mail      ~SM~SM");
		center("`0EYour Command: `0F-> ");
		switch(od_get_answer("BGLIQYTR")){
			case 'L':	
					mci("~SC");
					listscores();	
					mci("~SM~SP");	
					break;
			case 'I':	
					mci("~SC");
					DisplayFile("dynasty.doc");	
					mci("~SM~SP");	
					break;
			case 'B':	
					logon();
					mainmenu();
					break;
			case 'Q':	
					q2bbs();	
					break;
			case 'Y':	
					mci("~SC");
					DisplayFile("sdolds.txt");	
					mci("~SM~SP");	
					break;
			case 'T':	
					mci("~SC");
					od_send_file(cfg.Nanspath);	
					mci("~SM~SP");	
					break;
			case 'G':	
					gamelog("Player ran godmenu");	
					godmenu();	
					break;
			}
		}
}

int flexist(char *fname)
{
	int x=FALSE;
	FILE *fptr;

	fptr=fopen(fname,"rb");
	if(fptr!=NULL){
		fclose(fptr);
		x=TRUE;
		}
	return(x);
}

int fexist(char *fname)
{
	int x=FALSE;
	FILE *fptr;
	char path[91];

	sprintf(path,"%s%s",gamedir,fname);
	fptr=fopen(path,"rb");
	if(fptr!=NULL){
		fclose(fptr);
		x=TRUE;
		}
	return(x);
}

void mailmenu()
{
	int done=FALSE;
	char ch;
	while(!done){
		mci("~SMMail Menu: (R)ead Mail, (S)end Mail, (Q)uit: ");
		ch=od_get_answer("RSQ");
		switch(ch){
			case 'R': readmail();	break;
			case 'S': sendmail();	break;
			case 'Q': done=TRUE;	break;
			}
		}
}

void infomenu()
{
	int done=FALSE;
	char ch;
	while(!done){
		mci("~SMHelp & Info Menu: (H)elp, (D)ynasty Doc, (B)ulletins: ");
		ch=od_get_answer("HDBQ");
		switch(ch){
			case 'B':	bulletins();					pausescr(); break;
			case 'D':	DisplayFile("dynasty.doc");	pausescr();	break;
			case 'H':	DisplayFile("dyansty.hlp");	pausescr();	break;
			case 'Q':	done=TRUE;
			}
		}
}


void mainmenu()
{
	int done=TRUE;

	if(plyr.turns>0){
		done=FALSE;
		dailynews();
		chckali();
		dplomenu();
		}
	else{
		mci("~SMYou have used all your turns for today.  ");
		mci("~SMTry again tommorrow.~SM");
		}

	while(!done){
		update();
		status();
		armfcred();
		plmaint();
		crimprev();
		downinsu();
		fdcmmkt();
		feedarmy();
		feedpopn();
		covrtact();
		commkt();
		attack();
		results();
		trade();
		plyr.turns--;
		saveplyr(plyr.key,&plyr);
		if(plyr.turns<1)	done=TRUE;
		else                done=continuey();
		}
}



void godmenu()
{
	char ch2;
	int done=FALSE;

	while(!done){
		od_clr_scr();
		mci("   %s Sysop Menu~SM~SM",od_control.od_prog_name);
		mci("ÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ~SM");
		mci("   (A) BbS Name                 : %s~SM",cfg.bbsn);
		mci("   (B) BBS Location             : %s~SM",cfg.loc);
		mci("   (D) Player Editor              ~SM");
		mci("   (E) Game Defaults Editor       ~SM");
		mci("   (F) Ansi Scores Path         : %s~SM",cfg.Sanspath);
		mci("   (G) Text Scores Path         : %s~SM",cfg.Stxtpath);
		mci("   (H) Ansi Today's News Path   : %s~SM",cfg.Nanspath);
		mci("   (I) Text Today's News Path   : %s~SM",cfg.Ntxtpath);
		mci("   (J) Ansi Yesterdy's News Path: %s~SM",cfg.Yanspath);
		mci("   (K) Text Yesterdy's News Path: %s~SM",cfg.Ytxtpath);
		mci("   (Q) Quit Sysop Menu~SM");
		mci("~SM   command: ");
		ch2=od_get_answer("ABCDEFGHIJKQ");
		switch(ch2){
			case 'A':
				mci("~SMCurrent BBS name: %s~SM",cfg.bbsn);
				mci("~SMNew BBS name: ");
				cfg.bbsn[0]='\0';
				sde_input_str(cfg.bbsn,30,32,127);
				break;
			case 'B':
				mci("~SMCurrent BBS Location: %s~SM",cfg.loc);
				mci("~SMNew BBS Location: ");
				cfg.loc[0]='\0';
				sde_input_str(cfg.loc,30,32,127);
				break;
			case 'D':	plyredit();	break;
			case 'E':	gamedit();	break;
			case 'F':
				mci("~SMNew Ansi Scores Path~SM:");
				sde_input_str(cfg.Sanspath,77,33,127);
				break;
			case 'G':
				mci("~SMNew Text Scores Path~SM:");
				sde_input_str(cfg.Stxtpath,77,33,127);
				break;
			case 'H':
				mci("~SMNew Ansi Todays's News Path~SM:");
				sde_input_str(cfg.Nanspath,77,33,127);
				break;
			case 'I':
				mci("~SMNew Text Today's News Path~SM:");
				sde_input_str(cfg.Ntxtpath,77,33,127);
				break;
			case 'J':
				mci("~SMNew Ansi Yesterday's News Path~SM:");
				sde_input_str(cfg.Yanspath,77,33,127);
				break;
			case 'K':
				mci("~SMNew Text Yesterday's News Path~SM:");
				sde_input_str(cfg.Ytxtpath,77,33,127);
				break;
			case 'Q':	savecfg();	done=TRUE;	break;
		}
	}
}

void gamedit()
{
	char onekey, ttxt[21];
	int done=FALSE;

	while(!done){
		od_clr_scr();
		center("Space Dynasty game defaults editor~SM");
		mci("~SM~SMÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ");
		mci("~SM(B) Number of plays per day   : %d",reg.turns);
		mci("~SM(C) Number of credits to start: %ld",reg.credits);
		mci("~SM(D) Number of Troops to start : %d",reg.troops);
		mci("~SM(E) Megatons of food to start : %d",reg.food);
		mci("~SM(F) Ore planets to start      : %d",reg.plano);
		mci("~SM(G) Food planets to start     : %d",reg.planf);
		mci("~SM(H) Soldier planets to start  : %d",reg.plans);
		mci("~SM(I) Carriers to start         : %d",reg.carriers);
		mci("~SM(J) Generals to start         : %d",reg.generals);
		mci("~SM(K) Figherts to start         : %d",reg.fighters);
		mci("~SM(L) Def Stations to start     : %d",reg.defstatns);
		mci("~SM(M) Population to start       : %d Million",reg.population);
		mci("~SM(Q) quit game edit menu");
		mci("~SM~SMCommand: ");
		onekey=od_get_answer("ABCDEFGHIQ");
		switch(onekey){
			case 'B':
				mci("~SMCurrent number of plays per day: %d",reg.turns);
				mci("~SMNew number of plays per day: ");
				ttxt[0]='\0';
				sde_input_str(ttxt,5,'0','9');
				reg.turns=atoi(ttxt);
				break;
			case 'C':
				mci("~SMCurrent number of credits to start: %d",reg.credits);
				mci("~SMNew number of credits to start: ");
				sde_input_str(ttxt,5,'0','9');
				reg.credits=atoi(ttxt);
				break;
			case 'D':
			mci("~SMCurrent number of megatons of food to start: %d",reg.food);
			mci("~SMNew number of megatons of food to start: ");
				sde_input_str(ttxt,5,'0','9');
				reg.food=atoi(ttxt);
				break;
			case 'E':
				mci("~SMCurrent number of ore planets to start: %s",reg.plano);
				mci("~SMNew number of ore planets to start: ");
				sde_input_str(ttxt,5,'0','9');
				reg.plano=atoi(ttxt);
				break;
			case 'F':
				mci("~SMCurrent number of food planets to start: %d",reg.planf);
				mci("~SMNew number of food planets to start: ");
				sde_input_str(ttxt,5,'0','9');
				reg.planf=atoi(ttxt);
				break;
			case 'G':
		mci("~SMCurrent number of soldier planets to start: %d",reg.plans);
				mci("~SMNew number of soldier planets to start: ");
				sde_input_str(ttxt,5,'0','9');
				reg.plans=atoi(ttxt);
				break;
			case 'H':
		mci("~SMCurrent number of carriers to start: %d",reg.carriers);
				mci("~SMNew number of carriers to start: ");
				sde_input_str(ttxt,5,'0','9');
				reg.carriers=atoi(ttxt);
				break;
			case 'I':
				mci("~SMCurrent number of generals to start: %d",reg.generals);
				mci("~SMNew number of generals to start: ");
				sde_input_str(ttxt,5,'0','9');
				reg.generals=atoi(ttxt);
				break;
			case 'Q':
				if(!savereg()){
					mci("~SMError saving reg file!~SM~SP");
					}
				done=TRUE;
				break;
			}
		}
}

void plyredit()
{
	int i,j,done=FALSE;
	char ttxt[35],ttxt1[31],ttxt2[31],ttxt3[31];

	i=0;	
	while(!done){
		if(!readplyr(i,&othr))	i=0;
		mci("~SC~SM%s %s~SM",
			od_control.od_prog_name,od_control.od_prog_version);
		mci("~SMPlayer Editor --- %s --- %s~SM",othr.realname,othr.laston);
		mci("~SM(A) Dynasty  : %-23s ",othr.moniker);
		mci("(B) Username : %s",othr.realname);
		mci("~SM(C) Password : %-23s ",othr.password);
		mci("(D) Alliances: ");
			for(j=0;j<25;j++)	mci("%d",othr.alliances[j]);
		mci("~SM(E) Credits  : %-23ld ",othr.credits);
		mci("(F) Planets  : F=%d,O=%d,T=%d (%d)",
			othr.planf,othr.plano,othr.plans,othr.planets);
		mci("~SM(G) Generals : %-23d ",othr.generals);
		mci("(H) Fighters : %d",othr.fighters);
		mci("~SM(I) DefStatns: %-23d ",othr.defstatns);
		mci("(J) Troops   : %d",othr.troops);
		mci("~SM(K) HCruiser : %-23d ",othr.cruisers);
		mci("(L) Pop      : %d Million",othr.population);
		mci("~SM(M) Food     : %-23d ",othr.food);
		mci("(N) Carriers : %d",othr.carriers);
		mci("~SM(O) Agents   : %-23d ",othr.agents);
		mci("(P) Turns    : %d",othr.turns);
		mci("~SM(S) CShip    : %-23d ",othr.cs);
		mci("(T) Insurgncy: %d",othr.insurgency);
		mci("~SM~SMPlayer Editor (]/[=Next/Prev, Q=Quit, R=Reset, X=Delete): ");
		switch(od_get_answer("[]ABCEFIJKLMNOPQRX")){
			case 'R':	
				strcpy(ttxt1,othr.moniker);
				strcpy(ttxt2,othr.realname);
				strcpy(ttxt3,othr.password);
				init_othr();
				strcpy(othr.moniker,ttxt1);
				strcpy(othr.realname,ttxt3);
				strcpy(othr.password,ttxt3);
				saveplyr(othr.key,&othr);
				break;
			case 'X':
				init_othr();
				break;
			case 'Q':	done=TRUE;	break;	
			case ']':	i++;	break;
			case '[':	i--;	break;
			case 'A':
				mci("~SMNew Dynasty Name: ");
				ttxt[0]='\0';
				sde_input_str(ttxt,25,32,127);
				strcpy(othr.moniker,ttxt);
				saveplyr(othr.key,&othr);
				break;
			case 'B':
				mci("~SMNew user Name: ");
				ttxt[0]='\0';
				sde_input_str(ttxt,25,32,127);
				strcpy(othr.realname,ttxt);
				saveplyr(othr.key,&othr);
				break;
			case 'C':
				mci("~SMNew password: ");
				ttxt[0]='\0';
				sde_input_str(ttxt,8,32,127);
				strcpy(othr.password,ttxt);
				saveplyr(othr.key,&othr);
				break;
			case 'E':
				mci("~SMNew Credits Amount: ");
				ttxt[0]='\0';
				sde_input_str(ttxt,10,'0','9');
				othr.credits=atol(ttxt);
				saveplyr(othr.key,&othr);
				break;
			case 'F':
				break;
			case 'I':
				mci("~SMNew Def Stations: ");
				ttxt[0]='\0';
				sde_input_str(ttxt,12,32,127);
				othr.defstatns=atoi(ttxt);
				saveplyr(othr.key,&othr);
				break;
			case 'J':
				mci("~SMNew Troops: ");
				ttxt[0]='\0';
				sde_input_str(ttxt,12,32,127);
				othr.troops=atoi(ttxt);
				saveplyr(othr.key,&othr);
				break;
			}	
		}
}


void WriteNews(char *newstxt, ...)
{
	FILE *fptr;
	char s[512],ttxt[512];
	va_list ap;

	va_start(ap,newstxt);
	vsprintf(s,newstxt,ap);
	va_end(ap);
	strcpy(ttxt,s);

	if(strlen(cfg.Nanspath)>0){
		fptr=fopen(cfg.Nanspath,"at+");
		if(fptr!=NULL){	
			fprintf(fptr,"%s",ttxt);
			fclose(fptr);
			}
		}

	if(strlen(cfg.Ntxtpath)>0){
		fptr=fopen(cfg.Ntxtpath,"at+");
		if(fptr!=NULL){	
			fprintf(fptr,"%s",ttxt);
			fclose(fptr);
			}
		}
	
}

void WriteText(char *fname, char *newstxt, ...)
{
	FILE *fptr;
	char path[91],s[512],ttxt[512];
	va_list ap;

	va_start(ap,newstxt);
	vsprintf(s,newstxt,ap);
	va_end(ap);
	strcpy(ttxt,s);

/*	sprintf(path,"%s%s",gamedir,fname); */
/*	fptr=fopen(path,"at+"); 	*/
	fptr=fopen(fname,"at+");
	if(fptr!=NULL){	
		fprintf(fptr,"%s",ttxt);
		fclose(fptr);
		}
	else{
		mci("~SMError writing to %s~SM~SP",path);
		}
}

int readplyr(int recno, plyrrec *pr)
{
	FILE *fptr;
	int x=FALSE;
	char path[91];

	sprintf(path,"%ssdplyr.dat",gamedir);
	fptr=fopen(path,"rb+");
	if(fptr==NULL);
	if(fseek(fptr,(long) recno * sizeof(plyrrec),SEEK_SET)==0){
		x=fread(pr,sizeof(plyrrec),1,fptr);
		fclose(fptr);
		}
	return(x);
}

int saveplyr(int recno, plyrrec *pr)
{
	FILE *fptr;
	int x=FALSE;
	char path[91];

	sprintf(path,"%ssdplyr.dat",gamedir);
	fptr=fopen(path,"rb+");
	if(fptr==NULL){
		fptr=fopen(path,"wb");
		x=fwrite(pr,sizeof(plyrrec),1,fptr);
		fclose(fptr);
		}
	else{
		if(fseek(fptr,(long) recno * sizeof(plyrrec),SEEK_SET)==0){
			x=fwrite(pr,sizeof(plyrrec),1,fptr);
			fclose(fptr);
			}
		}
	return(x);
}

int main(int argv, char *argc[])
{
	strcpy(gamedir,argc[0]);
	init(argv, argc);
	begining();
	return 0;
}


void centline(char *str)
{
	int x;

	x=strlen(str);
	od_repeat(' ',40-(x/2));
	mci(str);nl();
}


void q2bbs()
{
	od_clr_scr();
	mci("Returning to %s~SM",cfg.bbsn);
	pausescr();
	savecfg();
	makescoresfile();
/*	saveplyr(plyr.key,&plyr); */
	gamelog("Player quits.");
	od_exit(0,FALSE);
}



char yesno()
{
	char yn;

	mci(" `09(`0FY`09/`0Fn`09) : ");
	yn=od_get_answer("YN\n\r");
	if(yn=='Y' || yn=='\n' || yn=='\r'){
		mci("Yes..");
		yn='Y';
		}
	else
		mci("No..");
	return(yn);
}

char noyes()
{
	char yn;

	mci(" `09(`0FN`09/`0Fy`09) : ");
	yn=od_get_answer("YN\n\r");
	if(yn=='N' || yn=='\n' || yn=='\r'){
		mci("No..");
		yn='N';
		}
	else
		mci("Yes..");
	return(yn);
}


void pausescr()
{
	mci("~SM`0D[PAUSE]`0B");
	od_get_key(TRUE);
	mci("\b\b\b\b\b\b\b       \b\b\b\b\b\b\b");
}


void dailynews()
{
	if(flexist(cfg.Nanspath))
		od_send_file(cfg.Nanspath);
	pausescr();
}

void init_othr()
{
	int i;

	strcpy(othr.moniker,"empty");
	strcpy(othr.realname,"empty");
	othr.password[0]='\0';
	othr.planf=reg.planf;
	othr.plano=reg.plano;
	othr.plans=reg.plans;
	othr.planets=othr.planf+othr.plano+othr.plans;
	othr.troops=reg.troops;
	othr.carriers=reg.carriers;
	othr.generals=reg.generals;
	othr.food=reg.food;
	othr.population=reg.population;
	othr.credits=reg.credits;
	othr.turns=reg.turns;
	othr.score=115;
	othr.insurgency=0;
	othr.agents=0;
	othr.cs=0;
	othr.defstatns=reg.defstatns;
	othr.fighters=reg.fighters;
	othr.generals=reg.generals;
	othr.ldp=0;
	for(i=0;i<25;i++)	othr.alliances[i]=0;
	saveplyr(othr.key,&othr);
}


void makeplyr()
{
	int i;

	init_othr();
	i=0;
	while(i<25){
		othr.key=i;
		saveplyr(othr.key,&othr);
		init_othr();
		i++;
		}
}

void makescoresfile()
{
	long skr=0L;
	char str[31],ttxt[31];
	int i,fone=FALSE;

	if(strlen(cfg.Sanspath)>0){
		if(flexist(cfg.Sanspath)){
			remove(cfg.Sanspath);
			}
	
		mcistr(ttxt2,"~SC~SM`0B%s %s~SM",
			od_control.od_prog_name,od_control.od_prog_version);
		WriteText(cfg.Sanspath,ttxt2);

		mcistr(ttxt2,"~SM`0BList of Players/Scores~SM");
		WriteText(cfg.Sanspath,ttxt2);
		mcistr(ttxt2,"~SM   Player                                    Planets         Score`09~SM");
		WriteText(cfg.Sanspath,ttxt2);
		WriteText(cfg.Sanspath,"ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n");
		i=0;
		while(readplyr(i++,&othr)){
			if(strcmp(othr.realname,"empty")!=0){
				commafmt(ttxt,sizeof(ttxt),othr.score);
				mcistr(ttxt2,"`0B%c  `0F%-38s `0B%10u `0B%13s~SM",
					i-1+'A',othr.moniker,othr.planets,ttxt);
				WriteText(cfg.Sanspath,ttxt2);
				fone=TRUE;
				if(skr < othr.score){
					skr=othr.score;
					strcpy(str,othr.moniker);
					}
				}
			}
	
		if(!fone){
			WriteText(cfg.Sanspath,"No players yet!\n");
			}
		WriteText(cfg.Sanspath,"ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n");
		if(fone){
			mcistr(ttxt2,"`0BThe Current Leader is `0F%s~SM",str);
				WriteText(cfg.Sanspath,ttxt2);
			}
		}
	if(strlen(cfg.Stxtpath)>0){
		if(flexist(cfg.Stxtpath)){
			remove(cfg.Stxtpath);
			}

		WriteText(cfg.Stxtpath,"\n%s %s\n",
			od_control.od_prog_name,od_control.od_prog_version);

		WriteText(cfg.Stxtpath,"\nList of Players/Scores\n");
		WriteText(cfg.Stxtpath,"\n   Player                                    Planets         Score\n");
		WriteText(cfg.Stxtpath,"------------------------------------------------------------------------------\n");
		i=0;
		while(readplyr(i++,&othr)){
			if(strcmp(othr.realname,"empty")!=0){
				commafmt(ttxt,sizeof(ttxt),othr.score);
				WriteText(cfg.Stxtpath,"%c  %-38s %10u %13s\n",
					i-1+'A',othr.moniker,othr.planets,ttxt);
				fone=TRUE;
				if(skr < othr.score){
					skr=othr.score;
					strcpy(str,othr.moniker);
					}
				}
			}
	
		if(!fone){
			WriteText(cfg.Stxtpath,"No players yet!\n");
			}
		WriteText(cfg.Stxtpath,"------------------------------------------------------------------------------\n");
		if(fone){
			WriteText(cfg.Stxtpath,"The Current Leader is %s\n",str);
			}
		}
}

void listscores()
{
	long skr=0L;
	char str[31],ttxt[31];
	int i,fone=FALSE;

	mci("~SM`0BList of Players/Scores~SM");
	mci("~SM   `0BPlayer                                    Planets         Score`09~SM");
	od_repeat(196,78);nl();
	i=0;
	while(readplyr(i++,&othr)){
		if(strcmp(othr.realname,"empty")!=0){
			commafmt(ttxt,sizeof(ttxt),othr.score);
			mci("`0B%c  `0F%-38s `0B%10u `0F%13s`09~SM",
				i-1+'A',othr.moniker,othr.planets,ttxt);
			fone=TRUE;
			if(skr < othr.score){
				skr=othr.score;
				strcpy(str,othr.moniker);
				}
			}
		}

	if(!fone){
		mci("No players yet!~SM");
		}
	od_repeat(196,78);nl();
	if(fone){
		mci("`09The Current Leader is `0F%s~SM",str);
		}
}

void outscores()
{
}

void readmail()
{
}

void showmail()
{
}

void showglbl()
{
	mci("~SC~SMWelcome back %s~SM~SP",plyr.moniker);
}

void status()
{
	char ttxt[31],ttxt2[31],ttxt3[31];
	od_clr_scr();
	commafmt(ttxt,sizeof(ttxt),plyr.plano*20000);
	mci("`0F%s`02 Credits worth of ore has been mined~SM",ttxt);
	commafmt(ttxt,sizeof(ttxt),plyr.planf*3500);
	mci("`0F%s `02Megatons of food grown, ~SM",ttxt);
	commafmt(ttxt,sizeof(ttxt),plyr.plans*10);
	mci("`0F%s `02new troops trained~SM",ttxt);
	commafmt(ttxt,sizeof(ttxt),(plyr.population/.004)+1);
	mci("`02You collect `0F%s `02credits in taxes~SM~SM",ttxt);
	mci("`0F%s `02Status Report~SM",plyr.moniker);
	mci("`0DÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ~SM");
	commafmt(ttxt,sizeof(ttxt),plyr.score);
	mci("`02Score    : `0F%s~SM",ttxt);
	commafmt(ttxt,sizeof(ttxt),plyr.turns);
	mci("`02Turns    : `0F%s~SM",ttxt);
	commafmt(ttxt,sizeof(ttxt),plyr.credits);
	mci("`02Credits  : `0F%s~SM",ttxt);
	commafmt(ttxt,sizeof(ttxt),plyr.population);
	mci("`02Pop'n    : `0F%s `02Million~SM",ttxt);
	commafmt(ttxt,sizeof(ttxt),plyr.food);
	mci("`02Food     : `0F%s `01Megatons~SM",ttxt);
	commafmt(ttxt,sizeof(ttxt),plyr.troops);
	commafmt(ttxt2,sizeof(ttxt2),plyr.generals);
	mci("`02Military : ""`0D[`02Troops=`0F%s`0D] [`02Generals=`0F%s`0D]~SM",
		ttxt,ttxt2);
	commafmt(ttxt,sizeof(ttxt),plyr.fighters);
	commafmt(ttxt2,sizeof(ttxt2),plyr.carriers);
	mci("           `0D[`02Fighters=`0F%s`0D] [`02Carriers=`0F%s`0D]~SM",
		ttxt,ttxt2);
	commafmt(ttxt,sizeof(ttxt),plyr.defstatns);
	commafmt(ttxt2,sizeof(ttxt2),plyr.cruisers);
	mci("           `0D[`02Def Stations=`0F%s`0D] [`02Heavy Cruisers=`0F%s`0D]~SM",
		ttxt,ttxt2);
	commafmt(ttxt,sizeof(ttxt),plyr.planf);
	commafmt(ttxt2,sizeof(ttxt2),plyr.plano);
	commafmt(ttxt3,sizeof(ttxt3),plyr.plans);
	mci("`02Planets  : `0D[`02Food=`0F%s`0D] [`02Ore=`0F%s`0D] [`02Soldier=`0F%s`0D]~SM",ttxt,ttxt2,ttxt3);
	mci("`02Command Ship  : `0F%ld%% completed~SM",plyr.cs/20*100);
	commafmt(ttxt,sizeof(ttxt),plyr.agents);
	mci("`02Covert Agents : `0F%s~SM",ttxt);
	mci("`02Insurgency    : `0F%s~SM",insurtxt[plyr.insurgency]);
	mci("`0DÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ");
}

void update()
{
	plyr.credits+=(plyr.plano*20000);
	plyr.food+=(plyr.planf*3500);
	plyr.troops+=(plyr.plans*10);
	plyr.score+=75;
	plyr.score+=25*(plyr.plano+plyr.plans+plyr.planf);
	plyr.credits+=(plyr.population / .004)+1;
	if(plyr.cs > 0 && plyr.cs < 20)
		plyr.cs++;
}

void getalias()
{
	char str1[81];
	int toto=3,done=FALSE;

	while(toto && !done){
		mci("~SC~SM`0BHello, New player.~SM");
		mci("~SMPlease enter a name for your Dynasty.~");
		mci("~SM        <ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ->");
		mci("~SMDynasty: ");
		str1[0]='\0';
		sde_input_str(str1,30,32,127);
		if(strlen(str1)==0){
			sprintf(str1,"%s Dynasty",od_control.user_name);
			}
		mci("~SM%s",str1);
		if(IsValidAlias(str1)==-1){
			strcpy(plyr.moniker,str1);
			done=TRUE;
			}
		toto--;
		}

	if(!done && !toto){
		mci("~SMTry again!");
		od_exit(12,FALSE);
		}
	else{
		strcpy(plyr.realname,od_control.user_name);
		saveplyr(plyr.key,&plyr);
		}
}


void newplyr()
{
	int x=-1;

	x=IsValidName("empty");

	if(x==-1){                  /* Can't find any empty players */
		x=IsInactivePlayer();   /* check for inactive players */
		if(x==-1){
			mci("~SM  Sorry, but this game is full.  please ask your GameOp");
			mci("~SM  to start another game.~SM~SP");
			q2bbs();
			}
		else{
			readplyr(x,&plyr);
			getalias();
			}
		}
	else{
		readplyr(x,&plyr);
		getalias();
		saveplyr(plyr.key,&plyr);
		mcistr(ttxt2,"~SM `0Bş `0F%s `0Bhas joined in Galactic Federation.~SM",
			plyr.moniker);
		WriteNews(ttxt2);
		gamelog("Initiated player account");
		}
}

void ltoc(char *ttxt, long ln)
{
	int i,cnt=-1;
	char t1[21];

	sprintf(t1,"%ld",ln);

	for(i=strlen(t1)+1;i>0;i--){
		if(cnt==3){
			t1[i-1]=','; 
			cnt=0;	
			}
		else
			t1[i]=t1[i-1];	
		cnt++;		
		}
	strcpy(ttxt,t1);
}

long howmuch( long inxxx)
{
	char ttxt[11];

	commafmt(ttxt,sizeof(ttxt),inxxx);

	mci("~SM`0BHow much will you give `09[`0F%s`09]`0F: ",ttxt);
	ttxt[0]='\0';
	sde_input_str(ttxt,10,'0','9');
	if(strlen(ttxt) != 0){
		inxxx=atol(ttxt);
		}
	else{
	    mci("`0F%ld",inxxx);
		}
	return(inxxx);
}


void armfcred()
{
	int ok=FALSE;
	char ttxt[11];

	xxx=(plyr.troops*100)+(plyr.fighters*100)+(plyr.generals*10);
	while(!ok){
		commafmt(ttxt,sizeof(ttxt),xxx);
		mci("~SM~SM`0FYour armed forces require `0B%s `0Fcredits.",ttxt);
		yyy=howmuch(xxx);
		if(yyy > plyr.credits){
			commafmt(ttxt,sizeof(ttxt),plyr.credits);
			mci("~SM`0CYou can only afford to pay `0F%s `0Ccredits.`07~SM",
				ttxt);
			}
		else
			ok=TRUE;
		}

	if(yyy < xxx){
		if(yyy < (xxx * 0.90))
			armfflag=2;
		else
			armfflag=1;
		}
	else
		armfflag=0;

	plyr.credits-=yyy;
	saveplyr(plyr.key,&plyr);
}


void plmaint()
{
	int ok=FALSE;
	char ttxt[11];

	xxx=plyr.planets*10000;

	while(!ok){
		commafmt(ttxt,sizeof(ttxt),xxx);
		mci("~SM~SM`0BMaintaining your planets requires `0F%s `0Bcredits.",
			ttxt);
		yyy=howmuch(xxx);
		if(yyy > plyr.credits){
			commafmt(ttxt,sizeof(ttxt),plyr.credits);
			mci("~SM`0CYou can only afford to pay `0F%s `0Ccredits.~SM",ttxt);
			}
		else
			ok=TRUE;
		}
	if(yyy < xxx){
		if(yyy < (xxx * 0.90))
			plmflag=2;
		else
			plmflag=1;
		}
	else
		plmflag=0;
	plyr.credits-=yyy;
	saveplyr(plyr.key,&plyr);
}


void crimprev()
{
	int ok=FALSE;
	char ttxt[11];

	xxx=plyr.population*10;

	while(!ok){
		commafmt(ttxt,sizeof(ttxt),xxx);
		mci("~SM~SM`0BYour Crime Prevention Agencies require `0F%s `0Bcredits.",
			ttxt);
		yyy=howmuch(xxx);
		if(yyy > plyr.credits){
			commafmt(ttxt,sizeof(ttxt),plyr.credits);
			mci("~SM`0CYou can only afford to pay `0F%s `0Ccredits.~SM",ttxt);
			}
		else{
			ok=TRUE;
			}
		}
	if(yyy < xxx)
		crpflag=FALSE;
	else
		crpflag=TRUE;
	plyr.credits-=yyy;
	saveplyr(plyr.key,&plyr);
}



void fdcmmkt()
{
	int buyit,sellit;
	char str[6],ch;
	char ttxt[41];

	commafmt(ttxt,sizeof(ttxt),cfg.mktfood);
	mci("~SM`0BWelcome to the Food Common Market...");
	mci("~SM`0BCurrent quantity available is `1F%s`0B megatons",ttxt);
	if(cfg.mktfood < 100000){
		buyit=3;
		sellit=1;
		strcpy(str,"High");
		}
	else{
		buyit=1;
		sellit=3;
		strcpy(str,"Low");
		}
	commafmt(ttxt,sizeof(ttxt),plyr.food);
	mci("~SM`0BFood is in `1F%s`0B demand",str);
	mci("~SM`0BYou currently have `1F%s`0B megatons of food.",ttxt);
	mci("~SM`0BWould you like to `09[`0BB`09]`0Buy, `09[`0BS`09]`0Bell, or `09[`0BC`09]`0Bontinue? : ");
	ch=od_get_answer("BSC\r\n");
	switch(ch){
		case 'B':
			commafmt(ttxt,sizeof(ttxt),sellit);
			mci("~SM`0BWe are selling food for `1F%s`0B credits per megaton.",
				ttxt);
			mci("~SM`0BHow many megatons would you like to buy: ");
			yyy=howmuch(cfg.mktfood);
			if(yyy>cfg.mktfood){
				mci("~SM`0AWe don't have that much food to sell!~SM~SP`0B");
				}
			else{
				plyr.food+=yyy;
				plyr.credits-=(yyy*sellit);
				cfg.mktfood-=yyy;
				mci("~SM`0ESold!~SM`0B");
				}
			break;
		case 'S':
			commafmt(ttxt,sizeof(ttxt),buyit);
			mci("~SM~SM`0BWe are buying food for `1F%s`0B credits per megaton.",ttxt);
			mci("~SM`0BHow many megatons would you like to sell: ");
			yyy=howmuch(plyr.food);
			if(yyy>plyr.food){
				mci("~SM`0Ayou don't have that much food to sell!~SM~SP`0B");
				}
			else{
				plyr.food-=yyy;
				plyr.credits+=(yyy*buyit);
				cfg.mktfood+=yyy;
				mci("~SM`0ESold!~SM`0B");
				}
			break;
		default: {
			}
		}
	saveplyr(plyr.key,&plyr);
	savecfg();
}

void feedpopn()
{
	int ok=FALSE;
	char ttxt[41];

	xxx=plyr.population*100;

	while(!ok){
		commafmt(ttxt,sizeof(ttxt),xxx);
		mci("~SM~SM`0BYour people require `0F%s `0Bmegatons of food.",ttxt);
		yyy=howmuch(xxx);
		if(yyy > plyr.food){
			commafmt(ttxt,sizeof(ttxt),plyr.food);
			mci("~SM`0CYou can only afford to give `0F%s `0Cmegatons.~SM",ttxt);
			}
		else{
			ok=TRUE;
			}
		}
	if(yyy < xxx){
		if(yyy < (xxx * 0.90))
			fpflag=2;
		else
			fpflag=1;
		}
	else
		fpflag=0;

	plyr.food-=yyy;
	saveplyr(plyr.key,&plyr);
}

void feedarmy()
{
	int ok=FALSE;
	char ttxt[41];
	xxx=plyr.troops*10;
	
	while(!ok){
		commafmt(ttxt,sizeof(ttxt),xxx);
		mci("~SM~SM`0BYour Army needs require `0F%s `0Bmegatons of food.",ttxt);
		yyy=howmuch(xxx);
		if(yyy > plyr.food){
			commafmt(ttxt,sizeof(ttxt),plyr.food);
			mci("~SM`0CYou can only afford to give `0F%s `0Cmegatons.~SM",ttxt);
			}
		else{
			ok=TRUE;
			}
		}
	if(yyy < xxx){
		if(yyy < (xxx * 0.90))
			faflag=2;
		else
			faflag=1;
		}
	else
		faflag=0;
	plyr.food-=yyy;
	saveplyr(plyr.key,&plyr);
}


void covrtact()
{
	char ch,ttxt[21];
	int inum,killed,done=FALSE,i2;

	if(plyr.agents < 1)
		return;

	while(plyr.agents > 0 && !done){
		commafmt(ttxt,sizeof(ttxt),plyr.credits);
		mci("~SM~SM`0BCovert operations");
		mci("~SM`0CÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄ");
		mci("~SM`04<`0C1`04> `0BSpy Mission`03........`0B7,500");
		mci("~SM`04<`0C2`04> `0BAssassination`03.....`0B10,000");
		mci("~SM`04<`0C3`04> `0BSabotage`03..........`0B12,500");
		mci("~SM`04<`0C4`04> `0BInsurgency`03.......`0B100,000");
		mci("~SM`04<`0C5`04> `0BPlant a Bomb`03...`0B1,000,000");
		mci("~SM`04<`0C0`04> `0BDone                    ");
		mci("~SM`0CÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄ");
		mci("~SM`0CYou have `0B%d `0CAgents and `0B%s `0CCredits.~SM", plyr.agents,ttxt);
		mci("~SM`0BCovert operation? `0C(`0B? = Menu`0C) `0F: ");
		ch=od_get_answer("123450\n\r");
		switch(ch){
			case '\n':
			case '\r':
			case '0':	done=TRUE;	break;
			case '?':	break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
				inum=ch-'1';
				mci(cvstr1[inum]);
				if(plyr.credits >= cvamt[inum]){
					if(wplyr(2,1)!=-1){
						mci("~SM%s%s?",cvstr2[inum],othr.moniker);
						if(yesno()=='Y'){
							plyr.credits-=cvamt[inum];
							i2=1;
							killed=FALSE;
							while(i2<=othr.agents && !killed){
								rnd_ret=randomx(100);
								if(rnd_ret < 25)
									killed=TRUE;
								}
							i2++;
							}
						if(killed){
			mci("~SM~SM`0CYour agent was captured and executed.(%d)`0B~SM~SM",rnd_ret);
							plyr.agents--;
							}
						else{
							nl();
							switch(inum){
								case 0:
			mci("~SM`0F%s `0Ehas `0F%ld `0ETroop units and `0F%ld `0EDefense",
						othr.moniker,othr.troops,othr.defstatns);
			mci("~SMStations.  He is at an insurgency level of `0F%s`0E.`0B~SM",
						insurtxt[othr.insurgency]);
										saveplyr(othr.key,&othr);
									break;
								case 1:
									if(othr.generals > 0){
										othr.generals--;
			mci("~SM`0EYou have assasinated one of `0F%s`0E's Generals.`0B~SM",
							othr.moniker);
										saveplyr(othr.key,&othr);
										}
									else{
			mci("~SM`0F%s `0Ehas no Generals to assasinate.~SM",
							othr.moniker);
										}
									break;
								case 2:
									if(othr.carriers > 0){
										othr.carriers--;
			mci("~SM`0EYou have destroyed one of `0F%s`0E's Carriers`0B~SM",
							othr.moniker);
										saveplyr(othr.key,&othr);
										}
									else{
			mci("~SM`0F%s `0Ehas no Carriers to destroy.`0B~SM",
							othr.moniker);
										}
									break;
								case 3:
									if(othr.insurgency >= 9)
			mci("~SM`0EThat dynasty is already under coup!`0B");
									else{
										othr.insurgency++;
			mci("~SM`0F%s `0Eis now at an Insurgency Level of `0F%s`0E.",
							othr.moniker,insurtxt[othr.insurgency]);
										saveplyr(othr.key,&othr);
										}
									break;
								case 4:	pab();	break;
								}
							nl();
							}
						}
					saveplyr(plyr.key,&plyr);
					}
				else{
					mci("~SMYou not have enough credits~SM~SM");
					}
				}
			}
}

void results()
{
	long xyz;
	char ttxt[21];

	if(!crpflag){
		xyz=plyr.population / plyr.planets;
		rnd_ret=randomx(xyz)+1;
		commafmt(ttxt,sizeof(ttxt),rnd_ret);
		mci("~SM`0BA crime wave breaks out in your dynasty!~SM");
		mci("You pay `0F%s`0B credits to repair vandalizim damage!~SM",ttxt);
		plyr.score-=plyr.score/200;
		plyr.credits-=rnd_ret;
		if(plyr.credits < 0)
			plyr.credits=0L;
		}

	if(fpflag > 1 || plmflag > 1 || armfflag > 1 || faflag > 1){
		mcistr(ttxt2,"~SM `0Cş `0BCivil War breaks out in `0E%s~SM",plyr.moniker);
		WriteNews(ttxt2);
		mci("~SM~SMCIVIL WAR breaks out in your dynasty!~SM");
		mci("Your dynasty loses half its planets under your cruel leadership!~SM");

		plyr.score-=(plyr.score/4);
		plyr.plano=(plyr.plano/2);
		plyr.planf=(plyr.planf/2);
		plyr.plans=(plyr.plans/2);
		plyr.planets=plyr.plano+plyr.planf+plyr.plans;
		plyr.population=(plyr.population/2);
		plyr.troops=(plyr.troops/2);
		plyr.insurgency=4;
		if(plyr.plano<1 || plyr.planf<1 || plyr.population<1)
			delplyr();
		}
	else{
		if(plyr.planets>=5)
			rnd_ret=randomx(plyr.planets/5)+1;
		else
			rnd_ret=2;
			
		commafmt(ttxt,sizeof(ttxt),rnd_ret);
		mci("~SM~SMYour population gained %s Million.~SM",ttxt);
		plyr.population+=rnd_ret;
		}
	saveplyr(plyr.key,&plyr);
}

int continuey()
{
	int x=FALSE;
	if(plyr.turns > 0){
		mci("~SM~SMContinue?");
		if(yesno()=='N'){
			listscores();
			pausescr();
			x=TRUE;
			}
		}
	return x;
}


void commktmu()
{
	 long pc;
	char stuff[10][15];

	pc=22000+(220*(plyr.planets-2));
	commafmt(stuff[0],sizeof(stuff[0]),plyr.troops);
	commafmt(stuff[1],sizeof(stuff[1]),plyr.fighters);
	commafmt(stuff[2],sizeof(stuff[2]),plyr.defstatns);
	commafmt(stuff[3],sizeof(stuff[3]),plyr.generals);
	commafmt(stuff[4],sizeof(stuff[4]),plyr.cs*10);
	commafmt(stuff[5],sizeof(stuff[5]),pc);
	commafmt(stuff[6],sizeof(stuff[6]),plyr.carriers);
	commafmt(stuff[7],sizeof(stuff[7]),plyr.agents);
	commafmt(stuff[8],sizeof(stuff[8]),plyr.cruisers);
	commafmt(stuff[9],sizeof(stuff[9]),plyr.planets);

	mci("~SM`0BWelcome to the Common Market.~SM");
	mci("~SM`0BItem                         Cost       Owned");
	mci("~SM`09ÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ-");
	mci("~SM`09<`0B1`09> `0BSoldier Troops`03..........`0B1,000`03");
		od_repeat('.',12-strlen(stuff[0]));mci("`0B%s",stuff[0]);
	mci("~SM`09<`0B2`09> `0BFighter`03.................`0B1,500`03");
		od_repeat('.',12-strlen(stuff[1]));mci("`0B%s",stuff[1]);
	mci("~SM`09<`0B3`09> `0BDef Stations`03............`0B2,000`03");
		od_repeat('.',12-strlen(stuff[2]));mci("`0B%s",stuff[2]);
	mci("~SM`09<`0B4`09> `0BGenerals`03...............`0B11,000`03");
		od_repeat('.',12-strlen(stuff[3]));mci("`0B%s",stuff[3]);
	mci("~SM`09<`0B5`09> `0BBuy Command Ship`03.......`0B60,000`03");
		od_repeat('.',12-strlen(stuff[4]));mci("`0B%s%%",stuff[4]);
	mci("~SM`09<`0B6`09> `0BColonize Planet`03...");
		od_repeat('.',11-strlen(stuff[5]));mci("`0B%s",stuff[5]);
		od_repeat('.',12-strlen(stuff[9]));mci("`0B%s",stuff[9]);
	mci("~SM`09<`0B7`09> `0BCarriers`03...............`0B15,000`03");
		od_repeat('.',12-strlen(stuff[6]));mci("`0B%s",stuff[6]);
	mci("~SM`09<`0B8`09> `0BCovert Agents`03...........`0B5,000`03");
		od_repeat('.',12-strlen(stuff[7]));mci("`0B%s",stuff[7]);
	mci("~SM`09<`0B9`09> `0BHeavy Cruiser`03...........`0B4,000`03");
		od_repeat('.',12-strlen(stuff[8]));mci("`0B%s",stuff[8]);
	mci("~SM`09<`0B0`09> `0BDone");
	mci("~SM`09ÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ-~SM");
}


void commkt()
{
	char ch,ttxt[11];
	int done3=FALSE;

	commktmu();
	while(!done3){
		commafmt(ttxt,sizeof(ttxt),plyr.credits);
		mci("~SM`0BYour credits amount to %s",ttxt);
		mci("~SMYour choice (?=Menu) : ");
		ch=od_get_answer("1234567890?\n\r");
		switch(ch){
			case '1':
				mci(" `0FBuy Troops...~SM");
				buyitems(TROOPS);
				break;
			case '2':
				mci(" `0FBuy Fighters...~SM");
				buyitems(FIGHTERS);
				break;
			case '3':
				mci(" `0FBuy Defensive Stations...~SM");
				buyitems(DEFSTATN);
				break;
			case '4':
				mci(" `0FBuy Generals...~SM");
				buyitems(GENERALS);
				break;
			case '5':
				mci(" `0FBuild Command Ship...~SM");
				if(plyr.cs==0L){
					if(plyr.credits >= 60000L){
					mci("`0BConstruction begins on your new command ship~SM~SM");
						plyr.credits-=60000L;
						plyr.cs=1;
						}
					else{
						mci("~SM`0CYou don't have enough credits!`0B~SM~SM");
						}
					}
				else{
					mci("`0EYou already have a command ship`0B~SM~SM");
					}
				break;
			case '6':
				mci(" `0FColonize planets...~SM");
				buyplan();
				break;
			case '7':
				mci(" `0FBuy Carriers...~SM");
				buyitems(CARRIERS);
				break;
			case '8':
				mci(" `0FRecruit covert agents...~SM");
				buyitems(CAGENTS);
				break;
			case '9':
				mci(" `0FBuy Heavy Cruisers...~SM");
				buyitems(HVYCRSRS);
				break;
			case '\n':
			case '\r':
			case '0':
				mci(" `0FDone...");
				done3=TRUE;
				break;
			case '?':
				commkt();
				break;
			}
		}
}

void buyplan()
{
	long num1,num3,plrpln,maxplan,cost;
	int done2,done1=FALSE,x1;
	char ch,ttxt[21],ttxt1[21],ttxt2[21];


	mci("`0BThe maximum number of planets that can be colonized at one time is 999.");

	plrpln=plyr.planets-2;
	cost=220*plrpln;
	cost+=22000;
	maxplan=(plyr.credits/cost);
	if(maxplan<0)	maxplan=0;
	if(maxplan < 1){
		mci("~SM`0CYou not have enough credits.~SM~SM");
		return;
		}
	done2=FALSE;
	while(!done2){
		commafmt(ttxt,sizeof(ttxt),plyr.credits);
		commafmt(ttxt1,sizeof(ttxt1),plyr.planets);
		commafmt(ttxt2,sizeof(ttxt2),maxplan);
		mci("~SM`0BHow many do you want? `09[`0FMAX=%s`09] `0B: ",ttxt2);
		ttxt[0]='\0'; 
		sde_input_str(ttxt,10,'0','9');
		if(strlen(ttxt)<1)	{
			num1=maxplan;
			done2=TRUE;
			}	
		else{
			num1=atol(ttxt);
			if(num1 >= 0 && num1 <= maxplan)
				done2=TRUE;
			}
		}

	done1=FALSE;
	done2=FALSE;
	x1=0;
	while(x1<num1 && !done1 && !done2){  
	mci("~SM~SM`09[`0FF`09]`0Bood, `09[`0FO`09]`0Bre, or `09[`0FS`09]`0Boldier planets? : `0F");
		ch=od_get_answer("FOSQ");
		switch(ch){
			case 'F': mci(" `0FFood planet...~SM"); break;
			case 'O': mci(" `0FOre planet...~SM"); break;
			case 'S': mci(" `0FSoldier planet...~SM");break;
			case 'Q': done1=TRUE; break;
			}
		while(!done1){  
			commafmt(ttxt,sizeof(ttxt),num1-x1);
			mci("`0BHow many? `09[`0FC/R=%s`09] : ",ttxt);
			ttxt[0]='\0';
			sde_input_str(ttxt,10,'0','9');
			if(strlen(ttxt) < 1)
				num3=num1-x1;
			else
				num3=atol(ttxt);
			if(num3 > 0 && num3 <= num1-x1){
				plyr.credits-=(num3 * (22000+(220*(plrpln))));
				switch(ch){
					case 'F':	plyr.planf+=num3;	break;
					case 'O':	plyr.plano+=num3;	break;
					case 'S':	plyr.plans+=num3;	break;
					}
				plyr.planets+=num3;
				plyr.score+=num3;
				x1+=num3;
				done1=TRUE; 
				commafmt(ttxt,sizeof(ttxt),plyr.planets);
				mci("~SM`0BYou now have `0F%s`0B planets.~SM",ttxt);
				}
			} 
		if(x1>=num1)	done2=TRUE;	else done1=FALSE;
		} 
	saveplyr(plyr.key,&plyr);
}

void buyitems(int which)
{

	long max,num3;
	char num1[21],ttxt[21];
	int done1=FALSE;
	char iname[7][20]={"Troops","Fighters","Defensive Stations","Generals",
						"Carriers","Heavy Cruisers","Covert Agents"};
	long icost[7]={1000,1500,2000,11000,15000,4000,5000};

	max=(plyr.credits/icost[which]);
	if(max < 1){
		mci("~SMYou not have enough credits.~SM~SM");
		return;
		}

	while(!done1){
		commafmt(ttxt,sizeof(ttxt),max);
		mci("~SMHow many %s do you want? [MAX=%s] : ",iname[which],ttxt);
		num1[0]='\0';
		sde_input_str(num1,10,'0','9');
		if(strlen(num1) < 1) 	
			sprintf(num1,"%ld",max);
		num3=atol(num1);

		if(num3 > 0 && num3 <= max){
			switch(which){
				case TROOPS:	plyr.troops+=num3;		break;
				case FIGHTERS:	plyr.fighters+=num3;	break;
				case DEFSTATN:	plyr.defstatns+=num3;	break;
				case GENERALS:	plyr.generals+=num3;	break;
				case CARRIERS:	plyr.carriers+=num3;	break;
				case HVYCRSRS:	plyr.cruisers+=num3;	break;
				case CAGENTS:	plyr.agents+=num3;		break;
				}
			plyr.credits-=(num3 * icost[which]);
			done1=TRUE;
			saveplyr(plyr.key,&plyr);
			commafmt(ttxt,sizeof(ttxt),num3);
			mci("~SMYou have purchased %ld %ss",num3,iname[which]);
			}
		else {
			mci("~SMPuchase aborted.");
			done1=TRUE;
			}
		}
		
}

void attack()
{
	int nofight=FALSE;

	if(plyr.insurgency > 1){
		insres();
		return;
		}
	if(faflag > 0){
		mci("~SM`0CYour hungry army is immobilized!`0B");
		nofight=TRUE;
		}
	if(armfflag > 0){
		mci("~SM`0CYour underpaid army refuses to fight!`0B");
		nofight=TRUE;
		}
	if(!nofight){
		if(plyr.generals < 0){
			mci("~SM`0CYou not have enough generals!`0B");
			nofight = TRUE;
			}
		}
	if(nofight)
		return;

	mci("~SM~SM`0BDo you wish to attack someone?");
	if(noyes()=='Y'){
		if(wplyr(2,1)==-1)
			return;

		alout=findaliance(plyr.key,othr.key);
		if(alout==2 || alout==3){
			mci("~SM`0CYou may not attack your ally!`0B~SM~SM");
			}
		else{
			mci("~SM`0EAttack %s?",othr.moniker);
			if(yesno()=='Y'){
				mci("`0B");
				if((othr.score > 1000 && (daya-othr.ldp)<=10)){
					plyr.score-=25;
					fight();
					makealiance(plyr.key,othr.key,3,3);
					}
				else{
					mci("~SM`0EThe geneva convention of 2186 prevents the attack of new dynasties.`0B~SM~SM");
					return;
					}
				}
			}
		}
}


void takeplan( long winplan)
{
	long amt1,amt2,l1;
	char ch3,str1[11],ttxt[21];

	commafmt(ttxt,sizeof(ttxt),winplan);
	mci("`0BYou capture %s planet",ttxt);
	if(winplan > 1L)
		mci("s!");
	else
		mci("!");
	l1=0L;
	while(l1<winplan){
		mci("~SM`0BTake `0C[`0BF`0C]`0Bood, `0C[`0BO`0C]`0Bre, or `0C[`0BS`0C]`0Boldier Planets? : ");
		ch3=od_get_answer("FOS");
		switch(ch3){
			case 'F':	amt1=othr.planf;	break;
			case 'O':	amt1=othr.plano;	break;
			case 'S':	amt1=othr.plans;	break;
			}
		if(amt1 > winplan)
			amt1=winplan;
		mci("~SM`0BHow many? `09[`0F%ld`09] `0B",amt1);
		sprintf(str1,"%ld",amt1);
		sde_input_str(str1,10,'0','9');
		if(strlen(str1) > 0)
			amt2=atol(str1);
		else
			amt2=amt1;
		if(amt2>amt1)
			amt2=amt1;

		switch(ch3){
			case 'F':	plyr.planf+=amt1;	break;
			case 'O':	plyr.plano+=amt1;	break;
			case 'S':	plyr.plans+=amt1;	break;
			}
		plyr.planets=plyr.plano+plyr.plans+plyr.planf;

		switch(ch3){
			case 'F':	othr.planf-=amt2;	break;
			case 'O':	othr.plano-=amt2;	break;
			case 'S':	othr.plans-=amt2;	break;
			}
		othr.planets=othr.plano+othr.plans+othr.planf;
		l1+=amt2;
		}
	saveplyr(plyr.key,&plyr);
	saveplyr(othr.key,&othr);
}


void getforce()
{
	long tr,fg,hc;
	char stri[21],ttxt[31],ttxt1[32];

	tr=plyr.generals * 50;
	fg=plyr.carriers * 100;
	hc=plyr.cruisers;
	xx=-1L;
	while(xx < 0 || xx > tr){
		commafmt(ttxt,sizeof(ttxt),plyr.generals);
		commafmt(ttxt1,sizeof(ttxt1),tr);
		mci("~SM~SM`0BWith %s Generals, you can command up to %s Soldiers.",
			ttxt,ttxt1);
		if(plyr.troops < 1){
			mci("~SMYou have 0 troops");
			xx=0;
			}
		else{
			if(tr > plyr.troops)
				tr=plyr.troops;
			}
		commafmt(ttxt,sizeof(ttxt),plyr.troops);
	mci("~SM`0BYou have %s troops.  Deploy how many? `09[`0F%s`09]`0B : ",
		ttxt,ttxt1);
		stri[0]='\0';
		sde_input_str(stri,10,'0','9');
		if(strlen(stri) < 1)
			xx=tr;
		else
			if(isdigit(stri[0]))
				xx=atol(stri);
		}

	yy=-1L;
	while(yy < 0 || yy > fg){
		commafmt(ttxt,sizeof(ttxt),plyr.carriers);
		commafmt(ttxt1,sizeof(ttxt1),fg);
	mci("~SM`0BWith %s Carriers, you can carry up to %s Fighters.",
		ttxt,ttxt1);
	if(plyr.fighters < 1 || plyr.carriers < 1)
		if(plyr.fighters < 1){
			mci("~SM`0CYou have 0 Fighters.`0B~SM");
			yy=0;
			}
		if(plyr.carriers < 1){
			mci("~SM`0BYou have 0 Carriers");
			yy=0;
			}
		else{
			if(fg > plyr.fighters)
				fg=plyr.fighters;
			commafmt(ttxt,sizeof(ttxt),plyr.fighters);
			commafmt(ttxt1,sizeof(ttxt1),fg);
			mci("~SM`0BYou have %s Fighters.  ",ttxt);
			mci("Deploy how many fighters? `09[`0F%s`09] `0B: ",ttxt1);
			stri[0]='\0';
			sde_input_str(stri,10,'0','9');
			if(strlen(stri) < 1)
				yy=fg;
			else
				if(isdigit(stri[0]))
					yy=atol(stri);
			}
		}

	zz=-1;
	while(zz < 0 || zz > hc){
		if(plyr.cruisers < 1){
			mci("~SM`0BYou have 0 Heavy Cruisers~SM");
			zz=0;
			}
		else{
			if(hc > plyr.cruisers) hc=plyr.cruisers;
			commafmt(ttxt,sizeof(ttxt),plyr.cruisers);
			commafmt(ttxt1,sizeof(ttxt1),hc);
			mci("~SM`0BYou have %s Heavy Cruisers.  ",ttxt);
			mci("Deploy how many? `09[`0F%c`09]`0B : ",ttxt1);
			stri[0]='\0';
			sde_input_str(stri,10,'0','9');
			if(strlen(stri) < 1)
				zz=hc;
			else
				if(isdigit(stri[0]))
					zz=atol(stri);
			}
		}

	if(xx<1 && yy<1 && zz<1){
		mci("~S`0CMAborting.`0B~SM~SM");
		return;
		}
}



void fight()
{
	long ptr,pfg,phc,otr,ods,ptdam,
		etdam,pftdam,edsdam,phcdam,otot,winplan;
	int wonit,comship,i;

	getforce();
	if(xx<1 && yy<1 && zz<1){
		mci("~SM`0CAborting.`0B~SM~SM");
		return;
		}
	else{
		mci("~SMYou send a force of %d troops, %d fighters, and %d cruisers~SM",
			xx,yy,zz);
		}

	mci("~SM`0F%s `0Emeets you with `0F%d `0Etroops, and `0F%d `0Edefensive stations",
			othr.moniker, othr.troops, othr.defstatns);
	mci("~SMand has %d planets ripe for plucking!~SM",othr.planets);


	ptr=xx;
	pfg=yy;
	phc=zz;
	otr=othr.troops;
	if(othr.troops > othr.generals * 50)
		otr=othr.generals * 50;

	otr=(otr/3)*2;
	ods=(othr.defstatns/3)*2;
	if(plyr.cs >= 20)
		comship=TRUE;
	else
		comship=FALSE;
	if(ptr>0 && otr>0){
		engage(comship,ptr,otr);
		ptdam=lost_off;
		etdam=lost_def;
		}
	else{
		ptdam=0;
		etdam=ptr;
		}
	if(pfg>0 && ods>0){
		engage(comship,pfg,ods);
		pftdam=lost_off;
		edsdam=pfg;
		}
	else{
		pftdam=0;
		edsdam=0;
		}
	if(phc>0 && ods>0){
		engage(comship,phc,ods);
		phcdam=lost_off;
		edsdam+=lost_def;
		}
	else{
		phcdam=0;
		edsdam=0;
		}

/*
	otot=((othr.troops+othr.defstatns)+1)/(othr.planets+1);
	winplan=((etdam+edsdam)+1/otot+1);
*/
	otot=othr.planets/5;
	winplan=hilo(1,otot*4);
	if(winplan>othr.planets)	winplan=othr.planets;
	nl();
	wonit=FALSE;
	mcistr(ttxt2,"~SM `0Cş `0F%s `0Battacked `0E%s`0B",
		plyr.moniker,othr.moniker);
	WriteNews(ttxt2);
	if(winplan>0){
		mci("~SM`0EYou are victorious!`0B");
		wonit=TRUE;
		mcistr(ttxt2," and was `09victorious!`0B~SM");
		WriteNews(ttxt2);
		}
	else{
		mci("~SM`0F%s`0C defeats your invading force.`0B",othr.moniker);
		plyr.score-=25;
		mcistr(ttxt2," and was `0Cdefeated!`0B~SM");
		WriteNews(ttxt2);
		}

	mci("~SM~SM`0EYou took out `0F%ld `0Eenemy Troops, and ",etdam);
	mci("`0F%ld`0E Defense Stations...",edsdam);
	mci("~SM`0DYou lost `0F%ld`0D Troops, ",ptdam);
	mci("`0F%ld`0D Fighters, and `0F%ld`0D Heavy Cruisers.~SM",
		pftdam,phcdam);

	plyr.troops-=ptdam;
	plyr.fighters-=pftdam;
	plyr.cruisers-=phcdam;
	saveplyr(plyr.key,&plyr);

	othr.troops-=etdam;
	if(othr.troops<0)	
		othr.troops=0;
	othr.defstatns-=edsdam;
	if(othr.defstatns<0)
		othr.defstatns=0;		
	saveplyr(othr.key,&othr);
	if(wonit){
		takeplan(winplan);
		if(othr.planets < 1){
			init_othr();
			saveplyr(othr.key,&othr);
			mci("~SM`0ECongratulations!");
			mci("~SM`0EYou have destroyed this dynasty!`0B~SM");
		}

		for(i=0;i<25;i++){
			readplyr(i,&othr);
			if(strcmp(othr.realname,"empty")){
				makealiance(plyr.key,othr.key,1,1);
				saveplyr(i,&othr);
				}
			}
		}
	pausescr();
}


void trade()
{
	char tstr[3][15]={" Tons of food."," Troops."," Credits."};
	long num1,maxy,maxx=plyr.carriers * 1000,amt;
	char ch,str2[11],ttxt[121];
	int ww;

	mci("~SM`0BDo you wish to do any trading?");
	if(noyes()=='N')
		return;
	else{
		if(wplyr(2,1)==-1){
			mci("~SM`0CCould not find that Dynasty!~SM`0B");
			return;
			}
		}
	mci("~SM`0BSend supplies to %s?",othr.moniker);
	if(yesno()=='Y'){
		mci("~SM`0BSend [`0FF`0C]ood, [`0FT`0B]roops, or [`0FC`0B]redits? : ");
		ch=od_get_answer("FTC");
		switch(ch){
			case 'F':	amt=plyr.food;		ww=0;	break;
			case 'T':	amt=plyr.troops;	ww=1;	break;
			case 'C':	amt=plyr.credits;	ww=2;	break;
			}
		mci("~SM`0BYou have %ld Carriers so you can send up to %ld units.",
			plyr.carriers,maxx);
		mci("~SMYou have %ld %s",amt,tstr[ww]);
		maxy=amt;
		if(amt > maxx)
			maxy=maxx;
		mci("~SM`0BHow many do you wish to send? (MAX=%ld) : ",maxy);
		str2[0]='\0';
		sde_input_str(str2,10,'0','9');
		if(strlen(str2) > 0){
			num1=atol(str2);
			if(num1 <= maxy && num1 > 0){
				amt -= num1;
				mci("~SM`0ECargo sent.~SM`0B");
				sprintf(ttxt,"Sent %ld %s to %s",num1,tstr[ww],othr.moniker);
				gamelog(ttxt);
			}
		else
			return;
		}
	else
		return;
	}
}

int hilo( int lo,  int hi)
{
	return( ((int)randomx(hi)-lo) + lo);
}


void engage(long commship, long offense, long defense)
{
	long abc;

	lost_off=0;
	lost_def=0;
	while(offense>0 && defense>0){
		abc=hilo(1,100);
		if(abc>75){
			lost_off++;
			offense--;
			}
		else{
			lost_def++;
			defense--;
			}
		}
}


void engage2(long commship, long offense, long defense)
{
	long faxtor=0;

	lost_off=offense-hilo(0,offense);
	if(commship)
		faxtor=(defense/5);
	lost_def=defense-hilo(0,defense);
	if(lost_off>offense)
		lost_off=offense;
	if(lost_def>defense)
		lost_def=defense;
}


void engage1(long commship, long offense, long defense)
{
	long main1,excess;

	if(offense > defense){
		main1 = defense;
		excess = offense - defense;
		lost_off = (excess*0.0875)+randomx(excess*0.005);
		lost_def = 0;
		}
	else{
		main1 = offense;
		excess = defense - offense;
		lost_off = 0;
		}
	if(commship)
		lost_def = (excess*0.0275)+randomx(excess*0.005);
	else
		lost_def = (excess*0.0375)+randomx(excess*0.005);


	lost_off = lost_off+(main1*0.45)+randomx(main1*0.10);
	if(commship)
		lost_def = lost_def+(main1*0.35)+randomx(main1*0.10);
	else
		lost_def = lost_def+(main1*0.45)+randomx(main1*0.10);
}


void dipmnu()
{
	mci("~SM~SM`0CDaily Diplomatic Phase");
	mci("~SM`09ÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄ");
	mci("~SM`09<`0F1`09> `0BPropose Alliance");
	mci("~SM`09<`0F2`09> `0BBreak Treaty");
	mci("~SM`09<`0F3`09> `0BView Relations");
	mci("~SM`09<`0F4`09> `0BQuery Allied Support");
	mci("~SM`09<`0F5`09> `0BCasualty Report");
	mci("~SM`09<`0F6`09> `0BGalactic News");
	mci("~SM`09<`0F7`09> `0BEmbassy Contacts");
	mci("~SM`09<`0F0`09> `0BDone");
	mci("~SM`09ÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄ~SM");
}



void dplomenu()
{
	char ch1;

	dipmnu();
	while(TRUE){
		mci("~SM`0BWhat would you like to do? `03(`0B0`03-`0B7`03, `0D?=Menu`03)`0B: ");
		ch1=od_get_answer("12345670?\n\r");
		switch(ch1){
			case '?':
				dipmnu();
				break;
			case '1':
				mci("Propose Alliance...~SM");
				pamnu();
				break;
			case '2':
				mci("Break Treaty...~SM");
				brtr();
				break;
			case '3':
				mci("View Relations...~SM");
				viewrltn();
				break;
			case '4':
				mci("Query Allied Support...~SM");
				qas();
				break;
			case '5':
				mci("Casualty Report...~SM");
				break;
			case '6':
				mci("Galactic News...~SM");
				break;
			case '7':
				mci("Eembassy Contacts...~SM");
				embsycon();
				break;
			case '0':
			case '\n':
			case '\r':
				mci("Done.~SM");
				return;
			}
		}
}

void brtr()
{
	if(wplyr(2,1)!=-1){
		alout=findaliance(plyr.key,othr.key);
		if(alout==1 || alout==2)
			{ mci("~SMBreak %s with %s?",diptxt[alout],othr.moniker);
			if(yesno()=='Y'){
				makealiance(plyr.key,othr.key,0,0);
				mci("~SMTreaty broken~SM~SM");
				}
			else{
				mci("~SMYou do not have a treaty with %s!~SM~SM",othr.moniker);
				}
			}
		else{
			mci("~SMPlayer not found!~SM~SM");
			}
		}
}



void qas()
{
	long tt=0L,td=0L,th=0L;
	int i=0;

	while(readplyr(i++,&othr)){
		if(strcmp(othr.realname,"empty")==0){
			alout=findaliance(plyr.key,othr.key);
			if(alout==1){
				tt+=othr.troops/10;
				td+=othr.defstatns/10;
				th+=othr.cruisers/10;
				}
			if(alout==2){
				tt+=othr.troops/5;
				td+=othr.defstatns/5;
				th+=othr.cruisers/5;
				}
			}
		}
	mci("~SM`0EAn estimated `0F%ld`0E Troops,");
	mci("`0F %ld`0E Defensive Stations,~SM",tt,td);
	mci("and `0F%ld`0E Heavy Cruisers would come to your aid if",th);
	mci(" you were attacked.~SM~SM");
}

void embsycon()
{
	int i=0;

	mci("~SM`0ELast Known Embassy Contact~SM");
	mci("~SM`09ÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄ~SM");
	mci("~SM`0BName                          Date");
	mci("~SM`09ÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ");
	while(readplyr(i++,&othr)){
		if(strcmp(othr.realname,"empty")!=0){
			mci("~SM`0F%c `0E%-27s `0D%s",
				othr.key+'A',othr.moniker,othr.laston);
			}
		}
	mci("~SM`09ÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ~SM");
}

void pamnu1()
{
	mci("~SM`0BPropose Alliance");
	mci("~SM`09ÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄ");
	mci("~SM`09<`0F1`09> `0BNeutrality/Peace Treaty");
	mci("~SM`09<`0F2`09> `0BMinor Alliance Pact");
	mci("~SM`09<`0F3`09> `0BMajor Alliance Pact");
	mci("~SM`09<`0F0`09> `0BDone");
	mci("~SM`09ÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄ");
}


void pamnu()
{
	char ch2;
	pamnu1();
	while(TRUE){
		mci("~SM`0BWhat would you like to do? (0-3, ?=Menu): ");
		ch2=od_get_answer("1230?\n\r");
		switch(ch2){
			case '1':
				mci(" `0FNeutrality/Peace Treaty...~SM");
				treaty(0);
				break;
			case '2':
				mci(" `0FMinor Alliance Pact...~SM");
				treaty(1);
				break;
			case '3':
				mci(" `0FMajor alliance pact...~SM");
				treaty(2);
				break;
			case '\n':
			case '\r':
			case '0':
				mci(" `0FDone.~SM");
				return;
				break;
			case '?':	pamnu1();	break;
			}
		}
}

void viewrltn()
{
	int i=0,aint;

	mci("~SMDiplomatic Relations~SM");
	mci("~SMÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ~SM");
	mci("~SMName                                  Planets          Score         Relation");
	mci("~SMÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ");
	while(readplyr(i++,&othr)){
		if(strcmp(othr.realname,"empty")!=0){
			if(othr.key!=plyr.key){
				aint=findaliance(plyr.key,othr.key);
				mci("~SM%c  %-26s  %14u %14u %16s",
					othr.key+'A',othr.moniker,
					othr.planets,othr.score,diptxt[aint]); 
				}
			}
		}
	mci("~SMÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ~SM");
}


int wplyr(int fl1,int fl2)
{
	int ret=-1,done=FALSE;
	char wp;

	while(!done){
		mci("~SM`09Which Player (`0FA `09- `0FZ`09, `0F?`09=`0FList, `0FQ`09=`0FQuit`09)? `0F: ");
		wp=od_get_answer("ABCDEFGHIJKLMNOPQRSTUVWXYZ?");
		if(wp=='Q'){
			done=TRUE;
			return FALSE;
			}
		if(wp=='?'){
			if(fl1 == 1)
				listscores();
			else
				viewrltn();
			}
		else{
			readplyr(wp-'A',&othr);
			if(strcmp(othr.moniker,"empty")!=0){
				if(fl2<2){
					done=TRUE;
					ret=wp-'A';
					}
				else{
					mci("~SMYou can't that to your self!~SM~SM");
					}
				}
			else{
				mci("~SMThere is no such player~SM~SM");
				}
			}
		}
	return ret;
}



void downinsu()
{
	long amt1;

	mci("~SM~SMYour dynasty is currently in a state of %s, ",
		insurtxt[plyr.insurgency]);
	if(plyr.insurgency<1){
		mci("~SMand does not need your attention.~SM");
		return;
		}

	amt1=((plyr.credits/100)*(plyr.insurgency));
	commafmt(ttxt2,sizeof(ttxt2),amt1);
	mci("~SMand may be reduced for a fee of %s credits.",ttxt2);
	mci("~SMDo you wish to pay for insurgency reduction?");
	if(yesno()=='Y'){
		plyr.insurgency--;
		plyr.credits-=amt1;
		mci("~SMYour dynasty is now in a state of %s.~SM",
			insurtxt[plyr.insurgency]);
		}
	nl();
}

void insres()
{
	long dt,df,dh;

	dt=hilo(0,(plyr.troops/100)*(plyr.insurgency));
	if(dt>plyr.troops)
		dt=plyr.troops;
	df=hilo(0,(plyr.fighters/100)*(plyr.insurgency));
	if(df>plyr.fighters)
		df=plyr.fighters;
	dh=hilo(0,(plyr.cruisers/100)*(plyr.insurgency));
	if(dh>plyr.cruisers)
		dh=plyr.cruisers;
	mci("~SM~SMYou lose %ld Troops, %ld Fighters, and %ld Cruisers due to",
			dt,df,dh);
	mci("~SMInsurgent conditions in your dynasty.~SM");

	plyr.troops-=dt;
	plyr.fighters-=df;
	plyr.cruisers-=dh;
	saveplyr(plyr.key,&plyr);
}

void initnews()
{
	char path1[91];
	long scr;
	int i;

	WriteNews("[2J[1;33m%s [37m%s [36mNews file for [33m%s\n\n",
		od_control.od_prog_name,od_control.od_prog_version,datetxt);
	WriteNews("                        [1;36m-[37m=[33mThe Galactice Tribune[37m=[36m-\n");
	WriteNews("  [1;34mÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ[36m\n");

	scr=0L;
	for(i=0;i<24;i++){
		readplyr(i,&othr);
		if(othr.score>scr){
			sprintf(path1,othr.moniker);
			scr=othr.score;
			}
		}
	WriteNews("  [1;34mş [33m%s [36mhas risen to the ranks of Top Dynasty\n",path1);
}

void maint()
{
	mci("~SMPerforming Daily Maintenance.  Please wait.~SM!SM");
	cfg.todayplayers=0;
	savecfg();
	
	remove(cfg.Yanspath);
	rename(cfg.Nanspath,cfg.Yanspath);
	remove(cfg.Ytxtpath);
	rename(cfg.Ntxtpath,cfg.Ytxtpath);
	initnews();

	mci("~SMFinished Daily Maintenance!SM");
}


void treaty(int trtype)
{
	if(wplyr(2,1)!=-1){
		mci("~SM`0BPropose a `0F%s `0Balliance with `0F%s`0B?",
			diptxt[trtype],othr.moniker);
		if(yesno()=='Y'){
			makealiance(plyr.key,othr.key,trtype+4,trtype);
			mci("~SMMessage sent.~SM~SM");
			}
		}
/*
	saveplyr(plyr.key,&plyr);
	saveplyr(othr.key,&othr);
*/
}

void chckali()
{
	char who1[26][36];
	long sk1[26],pl1[26];
	int ct1,ct2,i=0,rl1[26],key1[26],ooo[26],old,new,t1;
	ct1=-1;
	while(readplyr(i++,&othr)){
		if(strcmp(othr.realname,"empty")!=0){
			old=plyr.alliances[othr.key];
			new=othr.alliances[plyr.key];
			if(new<old)	{
				t1=new;
				new=old;
				old=t1;
				}
			if((new!=old) && (old>3 || new>3)){
				ct1++;
				strcpy(who1[ct1],othr.moniker);
				sk1[ct1]=othr.score;
				pl1[ct1]=othr.planets;
				rl1[ct1]=new;
				key1[ct1]=othr.key;
				ooo[ct1]=old;
				}
			}
		}
	if(ct1>=0){
		mci("~SM`0BAlliance Proposals");
		mci("~SM`09ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ~SM");
		mci("~SM`0BName                           Planets         Score         Relation");
		mci("~SM`09ÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ");
		ct2=0;
		while(ct2<=ct1){
			mci("~SM`0B%c   %-21s %12ld %13ld %20s",
				key1[ct2]+'A',who1[ct2],pl1[ct2],sk1[ct2],diptxt[rl1[ct2]-4]);
			ct2++;
			}
		mci("~SM`09ÄÄÄÄÄÄÍÍÍÍÍÍÍÍÍÍÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ~SM");
		ct2=0;
		while(ct2<=ct1){
			mci("~SM`0B%s offers you a %s pact.",
				who1[ct2],diptxt[rl1[ct2]-4]);
			mci("~SMDo you accept? : ");
			if(yesno()=='Y'){
				if(readplyr(key1[ct2],&othr))
					makealiance(plyr.key,othr.key,rl1[ct2]-4,rl1[ct2]-4);
				else
					mci("~SMPlayer not found!~SM");
				}
			else{
				makealiance(plyr.key,othr.key,ooo[ct2],ooo[ct2]);
				}
			nl();
			ct2++;
			}
		}
}

void bulletins()
{
}

void sendmail()
{
}

void pab()
{
	mci("~SM1) Imperial Embassy");
	mci("~SM2) Imperial Foodstores");
	mci("~SM3) Military Headquarters");
	mci("~SM4) Space Dock");
	mci("~SM~SMBomb Location? (0 = None): ");
	bombloc(od_get_answer("12340")-'0');
}

void bombloc(int ww)
{

	char bltxt[5][26]={"Done...","Imperial Embassy...","Imperial Foodstores...",
		"Military Headquarters...","Space Dock..."};
	char ttxt[31];

	if(ww==0){
		mci(bltxt[0]);
		return;
		}
	mci(bltxt[ww]);
	mci("~SMBomb %s?",othr.moniker);
	if(yesno()=='Y'){
		mci("~SMThere is a huge explosion~SM");
		switch(ww){
			case 1:
				xxx=othr.agents;
				if(xxx>300)	xxx=300;
				xxx=hilo(0,xxx);
				othr.agents-=xxx;
				mci("~SM%ld of %s's agents are killed.~SM",
					xxx,othr.moniker);
				break;
			case 2:
				xxx=othr.food;
				if(xxx>375000)	xxx=375000;
				xxx=hilo(0,xxx);
				othr.food-=xxx;
				commafmt(ttxt,sizeof(ttxt),xxx);
	mcistr(ttxt2,"~SM `09ş `0BA superfreighter containing `0E%s `0Bmegatons~SM of `0F%s's `0Bfood is destroyed.~SM",ttxt,othr.moniker);
				WriteNews(ttxt2);
				mci(ttxt2);
				break;
			case 3:
				xxx=othr.generals;
				if(xxx>140)	xxx=140;
				xxx=hilo(0,xxx);
				othr.generals-=xxx;
				commafmt(ttxt,sizeof(ttxt),xxx);
				mcistr(ttxt2,"~SM `09ş `0E%s `0Bof `0E%s's `0BGenerals are killed.~SM",
					ttxt,othr.moniker);
				WriteNews(ttxt2);
				mci(ttxt2);
				break;
			case 4:
				xxx=othr.carriers;
				if(xxx>100)	xxx=100;
				xxx=hilo(0,xxx);
				othr.carriers-=xxx;
				commafmt(ttxt,sizeof(ttxt),xxx);
				mcistr(ttxt2,"~SM `09ş `0E%s `0Bof `0E%s's `0BCarriers are destroyed.~SM",
					ttxt,othr.moniker);
				WriteNews(ttxt2);
				mci(ttxt2);
			}
		}
}


int findaliance(int p1, int p2)
{
	int x=-1;

	plyrrec pp1,pp2;
	readplyr(p1,&pp1);
	readplyr(p2,&pp2);
	if(pp1.alliances[p2]==pp2.alliances[p1])
		x=pp1.alliances[p2];

	return(x);
}



void makealiance(int p1, int p2, int al1, int al2)
{
	readplyr(p1,&plyr);
	readplyr(p2,&othr);

	plyr.alliances[p2]=al1;
	othr.alliances[p1]=al2;

	saveplyr(p1,&plyr);
	saveplyr(p2,&othr);
}

void gatherfo()
{
	long tt=0L,td=0L,th=0L,t1=0L,t2=0L,t3=0;
	int i=0,aint,ok2=FALSE,ok1;
	while(readplyr(i++,&plyr)){
		if(strcmp(othr.realname,"empty")!=0){
			ok1=FALSE;
			od_printf("\n\rAye");
			aint=findaliance(plyr.key,othr.key);
			od_printf("\n\rBee");
			if(aint==2 && othr.key != plyr.key){
				t1+=othr.troops/10;
				t2+=othr.defstatns/10;
				t3+=othr.cruisers/10;
				ok1=TRUE;
				ok2=TRUE;
				}
			od_printf("\n\rCee");
			if(aint==3 && othr.key != plyr.key){
				t1+=othr.troops/5;
				t2+=othr.defstatns/5;
				t3+=othr.cruisers/5;
				ok1=TRUE;
				ok2=TRUE;
				}
			od_printf("\n\rEee");
			}
		od_printf("\n\rEff");
		if(ok1){
			tt+=t1;
			td+=t2;
			th+=t3;
			othr.troops-=t1;
			othr.defstatns-=t2;
			othr.cruisers-=t3;
			saveplyr(othr.key,&othr);
			}
		}
	od_printf("\n\rGee");

	if(ok2){
		plyr.troops+=tt;
		plyr.defstatns+=td;
		plyr.cruisers+=th;
		nl();
		mci("A total of %ld Troops, %ld Defensive Stations, and %ld Heavy Cruisers",tt,td,th);nl();
		mci("from your various alliances come to your aid.");nl();
		nl();
		}
}

void delplyr()
{
	int i=0;

	mci("~SMCongratulations!~SM");
	mci("You have managed to destroyed your own dynasty!~SM");
	while(readplyr(i++,&othr))
		makealiance(plyr.key,plyr.key,1,1);
	init_othr();
	saveplyr(plyr.key,&othr);
	mci("~SM~SP");
	q2bbs();
}

int randomx(int maxnum)
{
	return(rand()%maxnum);
}

void dirname(char *path)
{
	int i;

	for(i=strlen(path)-1;i>0;i--){
		if(path[i]=='/'){
			path[i+1]='\0';
			return;
			}
		}
}
