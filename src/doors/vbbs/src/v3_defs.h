#ifndef _V3_DEFS_H_
#define _V3_DEFS_H_

#define MAX_CPU		15
#define MAX_HD		15
#define MAX_SW 	   	10
#define MAX_SKILL	10

extern s16   actcount,
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
extern s16 		scant[6];
extern char 	LG, TS[81];

extern char		gamedir[_MAX_PATH];

extern classtype classes[5];

extern tUserRecord plyr, othr, tmpplyr;
extern tUserIdx	UsersIdx[MAX_USERS];
extern hackrec 	hacker;

extern hdtype  	harddrive[MAX_HD];
extern cputype 	computer[MAX_CPU];
extern modemtype  	mtype[MAX_MODEM];
extern bbstype 	bbssw[20];
extern char		skill_txt[10][30];

#endif

