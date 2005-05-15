/*****************************************************************************
 *
 * File ..................: v3_cfg.c
 * Purpose ...............: Game Configuration
 * Last modification date : 30-Sept-2000
 *
 *****************************************************************************
 * Copyright (C) 1999-2000
 *
 * Darryl Perry		FIDO:		1:211/105
 * Sacramento, CA.
 * USA
 *
 * This file is part of Virtual BBS.
 *
 * This Game is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Virtual BBS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Virutal BBS; see the file COPYING.  If not, write to the
 * Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/

#include <math.h>

#include "vbbs.h"
#include "v3_defs.h"
#include "v3_cfg.h"
#include "v3_io.h"
#include "v3_mci.h"
#include "v3_basic.h"

/* this fixes "floating point formats not linked" run-time error */
#if defined(__BORLANDC__)
extern _floatconvert;
#pragma extref _floatconvert
#endif /* defined(__BORLANDC__) */

/* from v3_defs.h */
int16   actcount,
		vircount,
		vtxtcount,
		vlcnt,
		plyrcnt,
		msgsrcount,
		subscribe_arrive,
		CONFIG_ACTIONS_PER_DAY,
		inum, inum2,
		virusscan,
		chatstat;
int16 	scant[6];
char 	LG, TS[81];

char 	gamedir[_MAX_PATH];

tUserRecord plyr, othr, tmpplyr;
tUserIdx	UsersIdx[MAX_USERS];
hackrec 	hacker;

hdtype  	harddrive[MAX_HD];
cputype 	computer[MAX_CPU];
modemtype  	mtype[MAX_MODEM];
bbstype 	bbssw[20];
char		skill_txt[10][30];

void main(int argv, char *argc[])
{
	char ch1;
	int done=0;

	init(argv,argc);

	while(!done){
		mci("~SCVirtual BBS's REAL sysop menu~SM");
		mci("~SM(A) Player Editor");
		mci("~SM(Q) Quit");
		mci("~SMCommand: ");
		ch1=od_get_answer("AQ");
		switch(ch1){
			case 'A':
				pickplyr();
				break;
			case 'Q':
				mci("~SM~SMExiting Sysop Menu...~SM~SM");
				done=1;
				break;
        	}
        }
}


void plyredit(int i)
{
	int x, done=0;
	char ch, ttxt[512];
	char laston_str[64];

	plyrcnt=0;
	while(readplyr(&othr,plyrcnt++));
	plyrcnt--;

	readplyr(&othr,i);
	strftime(laston_str, 64, "%m/%d/%Y %H:%M", localtime(&othr.laston));

	while(!done)
	{
		od_clr_scr();
		mci("`0EVirtual BBS");
		mci("~SCPùLùAùYùEùR  EùDùIùTùOùR~SM~SM");
		mci("#%02d/%02d ~SM",
			othr.plyrnum+1,
			plyrcnt);
		mci("`0FRealname   `0F: %-30s      Last on     `0F: %s~SM",
			othr.realname,
			laston_str);
		mci("~SM(A) Sysop Name : %-32s (F) Level       : %d",
			othr.sysopname,othr.skill_lev);
		mci("~SM(B) BBS Name   : %-32s (G) Score       : %ld",
			othr.bbsname,othr.skore_now);
		mci("~SM(C) Computer   : (%2d) %-27.27s (H) Actions     : %d",
			othr.cpu+1,computer[othr.cpu].cpuname,othr.actions);
		mci("~SM(D) Storage    : %-32s (I) Money       : %1.0f",
			harddrive[othr.hd_size].hdsize,othr.funds);
		mci("~SM(E) BBS S/W    : %-32s",bbssw[othr.bbs_sw].bbsname);
		mci("~SM~SMPhone Lines:                 Modems:                     Security:");
        mci("~SM(K) Installed  : %-11d (N) Low speed : %-11d (R) CallerID    : %d",
			othr.pacbell_installed,othr.mspeed.low,othr.callid);
		mci("~SM(L) Ordered    : %-11d (O) High Speed: %-11d (R) Insurance   : %d",
			othr.pacbell_ordered,othr.mspeed.high,othr.insurance);
		mci("~SM(M) Damaged    : %-11d (P) Digital   : %-11d (T) Alarm       : %d",
			othr.pacbell_broke,othr.mspeed.digital,othr.security);
		mci("~SM~SMEducation:                   Users:                      Empoyees");
		mci("~SM(U) Public     : %-9.2f (W) Free      : %-11ld (Y) Employees   : %d",
			othr.education[0],othr.users_f,othr.employees);
		mci("~SM(V) Private    : %-9.2f (X) Paying    : %-11ld (Z) Action      : %d",
			othr.education[1],othr.users_p,othr.actions);
		mci("~SM~SMCommand : ");

		ch=od_get_answer("1ABCDEFGHIJKLMNOPQRSTUVWXYZ[]");
		nl();nl();
		switch(ch){
			case 'A':
				mci("~SMNew Sysop Name: ");
				*ttxt='\0';
				od_input_str(ttxt,30,32,126);
				if(strlen(ttxt)>0)
					strcpy(othr.sysopname,ttxt);
				break;
			case 'B':
				mci("~SMNew BBS Name: ");
				*ttxt='\0';
				od_input_str(ttxt,30,32,126);
				if(strlen(ttxt)>0)
					strcpy(othr.bbsname,ttxt);
				break;
			case 'C':
				mci("~SM~SMSelect new Computer.~SM");
				for(x=0;x<MAX_CPU;x++){
					mci("%-2d %s~SM",x,computer[x].cpuname);
					}
				mci("~SMPick One (0-%d) : ",MAX_CPU-1);
				*ttxt='\0';
				od_input_str(ttxt,2,'0','9');
				othr.cpu=atoi(ttxt);
				break;
			case 'D':
				mci("~SM~SMSelect new Storage.~SM");
				for(x=0;x<MAX_HD;x++){
					mci("%-2d %s~SM",x,harddrive[x].hdsize);
					}
				mci("~SMPick One (0-%d) : ",MAX_HD-1);
				*ttxt='\0';
				od_input_str(ttxt,2,'0','9');
				othr.hd_size=atoi(ttxt);
				break;
			case 'E':
				mci("~SM~SMSelect new BBS Software.~SM");
				for(x=0;x<MAX_SW;x++){
					mci("%-2d %s~SM",x+1,bbssw[x].bbsname);
					}
				mci("~SMPick One (0-%d) : ",MAX_SW-1);
				*ttxt='\0';
				od_input_str(ttxt,2,'0','9');
				othr.bbs_sw=atoi(ttxt)-1;
				break;
			case 'F':
				text("New Skill Level: ");
				*ttxt='\0';
				od_input_str(ttxt, 2, '0', '9');
				othr.skill_lev=atoi(ttxt);
				break;
			case 'G':
				text("~SMNew Score: ");
				*ttxt='\0';
				od_input_str(ttxt, 5, '0', '9');
				othr.skore_now=atoi(ttxt);
				break;
			case 'H':
				mci("~SMActions Amount: ");
				*ttxt='\0';
				od_input_str(ttxt,10,'0','9');
				othr.actions=atol(ttxt);
				break;
			case 'I':
				mci("~SMNew Funds Amount: ");
				*ttxt='\0';
				od_input_str(ttxt,10,'0','9');
				othr.funds = (float)atof(ttxt);
				break;
			case 'K':
				mci("~SMInstalled Phone Lines: ");
				*ttxt='\0';
				od_input_str(ttxt,5,'0','9');
				othr.pacbell_installed = atoi(ttxt);
				break;
			case 'L':
				mci("~SMOrdered Phone Lines: ");
				*ttxt='\0';
				od_input_str(ttxt,5,'0','9');
				othr.pacbell_ordered = atoi(ttxt);
				break;
			case 'M':
				mci("~SMDamaged Phone Lines: ");
				*ttxt='\0';
				od_input_str(ttxt,5,'0','9');
				othr.pacbell_broke = atoi(ttxt);
				break;
			case 'N':
				mci("~SMLow Speed Modems: ");
				*ttxt='\0';
				od_input_str(ttxt,5,'0','9');
				othr.mspeed.low = atoi(ttxt);
				break;
			case 'O':
				mci("~SMHigh Speed Modems: ");
				*ttxt='\0';
				od_input_str(ttxt,5,'0','9');
				othr.mspeed.high = atoi(ttxt);
				break;
			case 'P':
				mci("~SMDigital Modems: ");
				*ttxt='\0';
				od_input_str(ttxt,5,'0','9');
				othr.mspeed.digital = atoi(ttxt);
				break;
			case 'U':
				mci("~SMNew Public Education Amount: ");
				*ttxt='\0';
				od_input_str(ttxt,6,33,126);
				othr.education[0] = (float)atof(ttxt);
				break;
			case 'V':
				mci("~SMNew Private Education Amount: ");
				*ttxt='\0';
				od_input_str(ttxt,6,33,126);
				othr.education[1] = (float)atof(ttxt);
				break;
			case 'W':
				mci("~SMFree Users: ");
				*ttxt='\0';
				od_input_str(ttxt,10,'0','9');
				othr.users_f=atoi(ttxt);
				break;
			case 'X':
				mci("~SMPaying Users: ");
				*ttxt='\0';
				od_input_str(ttxt,10,'0','9');
				othr.users_p=atoi(ttxt);
				break;
			case 'Y':
				mci("~SMEmployees: ");
				*ttxt='\0';
				od_input_str(ttxt,3,'0','9');
				othr.employees=atoi(ttxt);
				if(othr.employees>254)	othr.employees=254;
				if(othr.employees<1)	othr.employees=0;
				break;
			case 'Z':
				mci("~SMEmployee Actions: ");
				*ttxt='\0';
				od_input_str(ttxt,3,'0','9');
				othr.employee_actions=atoi(ttxt);
				if(othr.employee_actions>254)	othr.employee_actions=254;
				if(othr.employee_actions<1)		othr.employee_actions=0;
				break;
			case ']':
				saveplyr(&othr,i);
				i++;
				if (i>=plyrcnt)	i=0;
				readplyr(&othr,i);
				break;
			case '[':
				saveplyr(&othr,i);
            	i--;
				if(i<=0)	i=plyrcnt;
				readplyr(&othr,i);
				break;
        	case 'Q':
				readplyr(&plyr,0);
				done=1;
				break;
			}
		saveplyr(&othr,i);
		}
}

void pickplyr()
{
	int page=0,i,pnum,done=0;
	char ch,ttxt[21],string[121];

	while (!done)
	{
		od_clr_scr();
		mci("~SMPlayer Listings~SM");
		mci("~SMNum Realname             Handle               BBS Name             Score~SM");
		od_repeat('\xC4', 78);
		nl();
		for(i=0;i<15;i++)
		{
			if(readplyr(&othr,i+(page*15))==TRUE)
			{
				sprintf(string,"%-3.3d %-20.20s %-20.20s %-20.20s %-ld",
					othr.plyrnum+1,
					othr.realname,
					othr.sysopname,
					othr.bbsname,
					othr.skore_now);
				mci(string);
			}
			else
			{
				sprintf(string,"%-3.3d %-20.20s %-20.20s %-20.20s %-ld",
					i+(page*15),
					"No Record",
					"",
					"",
					0L);
				mci(string);
			}
			nl();
		}
		od_repeat('\xC4',78); nl();
		mci("Player Edit : ]/[=Next/Prev, Q=Quit : ");
		ch=od_get_answer("AMD[]Q");
		switch(ch)
		{
		case ']':
			page++;
			break;
		case '[':
			page--;
			if (page<0) page = 0;
			break;
		case 'A':
			new_user();
			plyrcnt++;
			saveplyr(&othr,othr.plyrnum);
			break;
		case 'D':
			nl();
			mci("Delete which player? : ");
			*ttxt='\0';
			od_input_str(ttxt,3,'0','9');
			pnum=atoi(ttxt)-1;
			readplyr(&othr,pnum);
			mci("~SM~SMDelete %s (%s) of %s?",
				othr.sysopname,othr.realname,othr.bbsname);
			if(yesno()=='Y'){
				othr.status=DELETED;
				saveplyr(&othr,othr.plyrnum);
				}
			break;
		case 'M':
			mci("~SMModify which player? : ");
			*ttxt='\0';
			od_input_str(ttxt,3,'0','9');
			pnum=atoi(ttxt)-1;
			plyredit(pnum);
			break;
		case 'Q':
			done=1;
			break;
		}
	}
}

#ifdef ODPLAT_WIN32
void init(LPSTR lpszCmdLine, int nCmdShow)
#else
void init(int argc, char *argv[])
#endif
{
#ifdef ODPLAT_WIN32
	strcpy(gamedir, ".\\");
#else
	char* ptr;
	strcpy(gamedir, argv[0]);
	ptr = strrchr(gamedir, '\\');
	ptr[1] = 0;
#endif

	strcpy(od_control.od_prog_name, "Virtual BBS");
        strcpy(od_control.od_prog_version, "Version 3.10df");
	strcpy(od_control.od_prog_copyright, "Copyright 1999 by Darryl Perry");
	strcpy(od_control.od_logfile_name, "vbbs.log");
	strcpy(od_registered_to, "Darryl Perry");
	od_parse_cmd_line(argc, argv);
	od_registration_key = 10664;
	od_init();
	vbbs_randomize();
	fillvars();
}

void vbbs_randomize()
{
	// seed the random number generator
	srand((unsigned int)time(NULL));
}

// returns 0 to (num-1)
int rand_num(int num)
{
	float max = (float)num;
	int rnum;

	rnum = (int)(max*rand()/(RAND_MAX+1.0f));

	return rnum;
}

