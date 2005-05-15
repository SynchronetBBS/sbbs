/*****************************************************************************
 *
 * File ..................: vbbs.c
 * Purpose ...............: Main file
 * Last modification date : 15-Mar-2001
 *
 *****************************************************************************
 * Copyright (C) 1997-2001
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

#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

#if defined(_MSC_VER)
#include "vc_strings.h"
#elif defined(__BORLANDC__)
#include "bc_str.h"
#include <alloc.h>
#else
#include <sys/poll.h>
#include <strings.h>
#endif

#include "vbbs.h"
#include "v3_defs.h"
#include "v3_basic.h"
#include "v3_io.h"
#include "v3_mci.h"
#include "v3_mail.h"
#include "v3_store.h"
#include "v3_maint.h"
#include "vbbs_db.h"

time_t today;
int report_flag = FALSE;
static int gNeedCleanup = FALSE;

classtype classes[5] =
{
	{ 1,  4, 4, 240 },
  	{ 2, 10, 2,  40 },
  	{ 3,  6, 2,  50 },
  	{ 4,  5, 5, 190 },
  	{ 5,  6, 1,  20 }
};

// from v3_defs.h
s16   	actcount,
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
s16 	scant[6];
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

#ifdef ODPLAT_WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
  	init(lpszCmdLine, nCmdShow);
	vbbs_main();
	return 0;
}
#else
int main(int argc, char *argv[])
{
  	init(argc, argv);
	vbbs_main();
	return 0;
}
#endif

void vbbs_main()
{
  	char ch = '\0',ttxt[91];

	while(1)
  	{
		/* check for menu display */
		if (!plyr.expert)
		{
			/* toggle expert field */
			plyr.expert = 1;
			/* show main menu */
			sprintf(ttxt,"%smainmenu.ans",gamedir);
			od_send_file(ttxt);
		}

		status();

#if defined(__BORLANDC__) && defined(DEBUG)
		od_printf("mem: %dk\n", coreleft()/1024);
#endif

		/* Main Prompt */
		/* The s16_2A and other int2x functions are found in v3_mci.c */
		u32_2A(1, plyr.users_f);
		u32_2A(2, plyr.users_p);
		s32_2A(3, plyr.actions);
		s32_2A(4, plyr.employee_actions);
		if(plyr.employee_actions>0)		text("0002");
		else							text("0001");
		ch = od_get_answer("ABCEHILMNPQRSTUVWX?");
		/* Perform the appropriate action based on the user's choice */
		switch(ch)
		{
		case 'A':
			text("0050");
			if(plyr.actions+plyr.employee_actions>0)	ans_chat();
			else										not_enough();
			break;
		case 'B':
			text("0051");
			bank();
			break;
		case 'C':
			text("0052");
			if(plyr.actions+plyr.employee_actions>0)	charge();
			else										not_enough();
			break;
		case 'E':
			text("0053");
			employ();
			break;
		case 'H':
			nl();
			od_send_file("vbbs.txt");
			break;
		case 'I':
			text("0054"); /* Inspect */
			if ((plyr.actions+plyr.employee_actions>0) && (plyr.funds >= 1.0f))
			{
				inspect();
				advance();
			}
			else
				not_enough();
			break;
		case 'L':
			text("0055");
			bbslist();
			break;
		case 'M':
			text("0056"); /* Mail */
			if (plyr.actions+plyr.employee_actions>0)
				readmail();
			else
				not_enough();
			break;
		case 'N':
			text("0057");
			if(plyr.actions+plyr.employee_actions>0){
				network();
				}
			else
				not_enough();
			break;
		case 'P':
			text("0058");
			if(plyr.actions+plyr.employee_actions>0)
				personal();
			else
				not_enough();
			break;
		case 'Q':
			text("0059");
			q2bbs();
			break;
		case 'R':
			text("0060"); // Report
			saveplyr(&plyr, plyr.plyrnum);
			VBBS3_Report(&plyr, FALSE);
			break;
		case 'S':
			text("0061");
			if(plyr.actions+plyr.employee_actions>0)
			{
				store();
				advance();
			}
			else
				not_enough();
			break;
		case 'T':
			text("0062");
			text("0407");
			if(!noyes()){
				nl();
				change_title();
				change_handle();
				}
			nl();
			break;
		case 'U':
			text("0063");	// Users Online
			users_online();
			break;
		case 'V':
			text("0064");
			if (plyr.actions+plyr.employee_actions>0)
			{
				vbbsscan();
				advance();
			}
			else
				not_enough();
			break;
		case 'W':
			text("0065"); // Work
			if (check_first())
			{
				if (plyr.employee_actions>0)
					text("0066"); // Employees help.
				if (plyr.actions+plyr.employee_actions>0)
				{
					actions();
					UseActions(1);
					advance();
				}
				else
					not_enough();
			}
			else
				not_enough();
			break;
		case '?':
			text("0067"); // Menu
			sprintf(ttxt,"%smainmenu.ans",gamedir);
			od_send_file(ttxt);
			break;
		}
  	}
}


int ReadOrAddCurrentUser(void)
{
	int got_user = FALSE;

	plyrcnt = 0;

	while (readplyr(&othr,plyrcnt++) != FALSE)
	{
		if (strcmp(othr.realname, od_control.user_name) == 0)
		{
			got_user = TRUE;
			plyr = othr;
		}
	}

	plyrcnt--;

	if (!got_user && (plyrcnt < MAX_USERS))
	{
		new_user();
		text("0005");
		change_title();
		change_handle();
		if (saveplyr(&plyr,plyr.plyrnum)==FALSE)
			got_user = FALSE;
		else
		{
			got_user = TRUE;
			plyrcnt++;
		}
	}

	return got_user;
}

void vbbs_cleanup(void)
{
	if (gNeedCleanup)
	{
		/* clear our flag so this only happens once */
		gNeedCleanup = FALSE;
		/* save the player */
		if (plyr.plyrnum >= 0)
			saveplyr(&plyr, plyr.plyrnum);
		// close all databases
		gSystemDB_shutdown();
		/* make sure the lock file gets removed */
		vbbs_io_unlock_game();
	}
}

void q2bbs()
{
	/* delete players with a score less than 1 */
	if (plyr.skore_now<1)
	{
		plyr.status=DELETED;
	}
	text("0009");	/* Disconnect from your service? */
	if (yesno())
	{
		/* display actions left */
		s32_2A(1, plyr.actions);
		text("0010");
		paws();
		topten();
		paws();
		str_2A(1, "BBS");
		text("0011");
		/* save player and cleanup lockfile */
		vbbs_cleanup();
		/* exit open doors */
		od_exit(0, FALSE);
	}
	else
		nl();
}

void employ()
{
	int y;

	y=plyr.mspeed.high+plyr.mspeed.low+plyr.mspeed.digital;
	inum2=(y/100)+1;
	s16_2A(1,inum2);
	nl();
	nl();
	if(y<100)
		text("0090");
	else{
		text("0091");
		if(yesno()){
			u32_2A(1,(y*2000)*4);
			text("0092");
			if(((y*2000)*4)>plyr.funds)
				text("0093");
			if((u32)(plyr.employees*2000)>plyr.funds)
				text("0094");
			}
		else{
			text("0095");
			UseActions(1);
			return;
			}
		text("0096");
		plyr.employees+=inum2;
		}
}

void actions()
{
	int chance = rand_num(100) + 1; /* 1 - 100 */

	nl();
	nl();

	/* if we haven't scanned lately, small chance of a virus */
	if ((virusscan > 10) && (chance <= 15))
		virus_det();
	else
	{
		/* grab a random action */
		int action = rand_num(actcount) + 1;
		/* display the results */
		read_actions(action);
	}
}

void exit_callback(void)
{
	/* make sure we cleaned up */
	vbbs_cleanup();
}

#ifdef ODPLAT_WIN32
void init(LPSTR lpszCmdLine, int nCmdShow)
#else
void init(int argc, char *argv[])
#endif
{
	char vbbstext[121];

 #if defined(VBBS_WIN32)
	char cmd_line[256];
	char* ptr;

	// copy in the full command line
	strcpy(cmd_line, GetCommandLine());
	// find the first space
	ptr = strchr(cmd_line, ' ');
	// if there is no space, use the current directory
	if (ptr)
	{
		// make it the end of the string
		*ptr = 0;
		// find the last slash
		ptr = strrchr(cmd_line, '\\');
		// if no slash, then use current directory
		if (ptr)
		{
			// skip the slash
			ptr++;
			// make it the end
			*ptr = 0;
			// if the first char is a quote, then it's a special case
			if (cmd_line[0] == '\"')
				strcpy(gamedir, cmd_line+1);
			else
				strcpy(gamedir, cmd_line);
		}
		else
		{
			strcpy(gamedir, ".\\");
		}
	}
	else
	{
		strcpy(gamedir, ".\\");
	}
 #elif defined(VBBS_DOS)
	char* ptr;
	strcpy(gamedir, argv[0]);
	ptr = strrchr(gamedir, '\\');
	ptr[1] = 0;
 #else
	char* ptr;
	strcpy(gamedir, argv[0]);
	ptr = strrchr(gamedir, '/');
	ptr[1] = 0;
 #endif
	basedir(gamedir);

	strcpy(od_control.od_prog_name, "Virtual BBS");
    strcpy(od_control.od_prog_version, "Version "VBBS_VERSION);
	strcpy(od_control.od_prog_copyright, "Copyright 1999-2002 by Darryl Perry");
	strcpy(od_control.od_logfile_name, "vbbs.log");
	strcpy(od_registered_to, "Darryl Perry");
#ifdef ODPLAT_WIN32
	// call new parse cmd line
  	od_control.od_cmd_show = nCmdShow;
	od_parse_cmd_line(lpszCmdLine);
#else
	od_parse_cmd_line(argc, argv);
#endif
	//od_registration_key = 123456L; /* open doors reg code goes here */
	od_init();
	vbbs_randomize();
	od_clr_scr();
	fillvars();

	sprintf(vbbstext,"/n[1;37m%s, The Virtual BBS Simulation Game[0;32m", od_control.od_prog_name);
  	mci(vbbstext);

	nl();
	od_repeat('\304',78);nl();
	mci(od_control.od_prog_copyright); nl();
	mci("Copyright 2002-2004 by Michael Montague"); nl();
	mci("Version %s", VBBS_VERSION); nl();
	nl();
	mci("Maintained by Vagabond Software"); nl();
	mci("http://vs3.vbsoft.org/"); nl();

	// tell open doors never to write EXITINFO.BBS
	od_control.od_extended_info = 0;
	// create a multinode lock-out file for the time being
	if (vbbs_io_lock_game(od_control.user_name))
	{
		nl();
		od_printf("Another user is currently playing Virtual BBS.  Please try again later.\r\n");
		paws();
		// exit open doors
		od_exit(1, FALSE);
	}
	// set our exit function to clean up the node file
	od_control.od_before_exit = exit_callback;
	// set the dirty flag
	gNeedCleanup = TRUE;
	// set an invalid player
	plyr.plyrnum = -1;
	// get today
	time(&today);
	// open all the databases
	gSystemDB_init();
	// do daily maint
	v3_maint_daily();
	// initialize game variables
	CONFIG_ACTIONS_PER_DAY = readapd();
	actcount = count_actions();
	vtxtcount = count_vtext();
	msgsrcount = count_msgsr();
	vlcnt = vlcount();
	plyrcnt = countplyrs();
	subscribe_arrive = -99;
#ifdef DEBUG
	// show the user name we got
	od_printf("User: %s\r\n", od_control.user_name);
#endif
	// get current user or add him
	if (!ReadOrAddCurrentUser())
	{
		nl();
		od_printf("Unable to access user file.  File may be locked or full.\r\n");
		paws();
		vbbs_io_unlock_game();
		od_exit(1, FALSE);
	}
	// display a message if they haven't played recently
	get_time_diff();
	// if player ordered phone lines, they are now available
	if (plyr.pacbell_ordered>0)
	{
		s32_2C(1,plyr.pacbell_ordered);
		text("0801");
		plyr.pacbell_installed += plyr.pacbell_ordered;
		plyr.pacbell_ordered = 0;
	}
	s16_2A(1,plyrcnt);
	text("0802");
	plyr.skore_before=plyr.skore_now;
	text("0810");
	if ((plyr.hackin[1] == 0) && (plyr.hackin[2] == 0))
		text("0811");
	else
	{
		text("0812");
		if (plyr.insurance>0)
		{
			text("0813");
			text("0814");
			plyr.funds += 200;
			plyr.insurance = 0;
		}
		text("0815");
		if (plyr.hackin[1] == 0)
			text("0816");
	}
	text("0817");
	if(plyr.cpu>=0 && plyr.cpu<11)	text("0818");
	else                            text("0819");

	text("0820");
	if(plyr.security<1)				text("0821");
	else	if(plyr.security==-1.0)	text("0822");
			else					text("0823");

	text("0824");
	if(plyr.callid<1)				text("0826");
	else	if(plyr.callid==-1.0)	text("0827");
			else					text("0828");
	text("0829");
	if(plyr.pacbell_broke)			text("0830");
	else							text("0831");
	if(plyr.pacbell_installed<1)
		text("0833");
	if(plyr.lastbill>5)
	{
		if(plyr.pacbell_installed>3)
			inum=plyr.pacbell_installed*50;
		else
			inum=plyr.pacbell_installed*20;
		if(plyr.callid)
			inum+=9;
		inum+=4;
		s16_2A(1,inum);
		text("0834");
		plyr.funds-=inum;
		plyr.lastbill=0;
	}
	else
		plyr.lastbill++;

	if ((today-plyr.laston)>(1440L*60L))
	{
		if (plyr.actions >= 0)
			plyr.actions = 0;
		plyr.actions += CONFIG_ACTIONS_PER_DAY;
		if (plyr.employees>0)
			plyr.employee_actions += (unsigned int)((float)plyr.employees * (CONFIG_ACTIONS_PER_DAY*0.8f));
		plyr.laston = today;
		if (plyr.pacbell_ordered>0)
		{
			s32_2C(1,plyr.pacbell_ordered);
			text("0835");
			plyr.pacbell_installed+=plyr.pacbell_ordered;
			plyr.pacbell_ordered=0;
		}
		if(plyr.insurance>0){
			if(plyr.funds>=1){
				plyr.funds--;
				text("0836");
                plyr.insurance--;
                if(plyr.insurance<3)
					text("0837");
				}
			else{
				text("0838");
				plyr.insurance=0;
				}
			}
            plyr.hacktoday=0;
		if(plyr.education[0]>0.0 || plyr.education[1]>0.0){
			text("0839");
			plyr.education[0] -= 0.2f;
			if (plyr.education[0]<0.0f)
				plyr.education[0] = 0.0f;
			plyr.education[1] -= 0.2f;
			if(plyr.education[1]<0.0f)
				plyr.education[1] = 0.0f;
			saveplyr(&plyr,plyr.plyrnum);
			}
		}

	chatstat = -(rand_num(CONFIG_ACTIONS_PER_DAY/3)+(CONFIG_ACTIONS_PER_DAY/6));
	if (plyr.msgs_waiting)
		text("0840");

	text("0841");
	text("0842");
}

void virus_det()
{
	// get random virus
	int virus = rand_num(vtxtcount) + 1;
	// display the results
	text("0870");
	read_vtext(virus);
	text("0871");
	virusscan = 0;
}

void VBBS3_Report(tUserRecord* pl, int bInspect)
{
	int rankplayer = rankplyr(pl->plyrnum);
	s32 total_modems = pl->mspeed.low + pl->mspeed.high + pl->mspeed.digital;
	s32 score_diff = (s32)pl->skore_now - (s32)pl->skore_before;
	u32 unused_ports = 0;

	// calculate unused ports
	if (bbssw[pl->bbs_sw].numlines < total_modems)
		unused_ports = bbssw[pl->bbs_sw].numlines - total_modems;
	
	str_2A(0, pl->bbsname);
	str_2A(1, computer[pl->cpu].cpuname);
	str_2A(2, harddrive[pl->hd_size].hdsize);
	s16_2A(3, pl->mspeed.low);
	s16_2A(4, pl->mspeed.high);
	s16_2A(5, pl->mspeed.digital);
	u32_2A(6, bbssw[pl->bbs_sw].numlines);
	str_2A(7, bbssw[pl->bbs_sw].bbsname);
	u32_2A(8, unused_ports);
	s16_2A(9, pl->pacbell_installed);
	s16_2A(10, pl->pacbell_ordered);
	s16_2A(23, pl->pacbell_broke);
	u32_2A(11, pl->users_f);
	u32_2A(12, pl->users_p);
	str_2A(13, skill_txt[pl->skill_lev]);
	s16_2A(14, (int)pl->education[0]); //float
	s16_2A(15, (int)pl->education[1]); //float
	s16_2A(16, pl->callid);
	s32_2A(17, pl->actions);
	s16_2A(18, pl->msgs_waiting);
	u32_2A(19, pl->skore_before);
	s32_2A(20, score_diff);
	s16_2A(21, rankplayer);
	u32_2A(22, pl->skore_now);

									text("0700"); // ~SM~SM
	if (bInspect)					text("0701"); // Your nearest competition:~SM
									text("0702"); // Report on the setup of
									text("0703"); // ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
									text("0704"); // Computer:
									text("0705"); // Storage:
	if (pl->mspeed.low>0)			text("0706"); // Modems: ~&3 2400bps
	if (pl->mspeed.high>0)  		text("0707"); // Modems: ~&4 High Speed
	if (pl->mspeed.digital>0)  		text("0708"); // Modems: ~&5 Digital
									text("0709"); // Software: Can support up to ~&6 line(s) <~&7> (~&8 ports unused)
									text("0710"); // Phone Lines: ~&9
	if (pl->pacbell_ordered>0)		text("0711"); // Phone Order: ~&A phone line to be installed.
	if (pl->pacbell_broke>0)		text("0727"); // Line Status: ~&N phone lines are damaged.~SM
									text("0712"); // Users: ~&B free, ~&C paying.

	if (!bInspect)
	{
									text("0713"); // Experience: ~&D~SM
									text("0714"); // Education: ~&E Public, ~&F Private.~SM
									text("0715"); // Caller ID: ~&G~SM
									text("0716"); // Actions: ~&H left today.~SM
		if (pl->msgs_waiting>0)		text("0717"); // Messages: ~&I waiting to be read~SM
		if (pl->insurance>0)		text("0719"); // Insurance: NO COVERAGE~SM
		else						text("0718"); // Insurance: You're Covered!~SM
		if (pl->skore_before<pl->skore_now)
		{
									text("0720"); // Start Score: ~&J (UP by ~&K points)~SM
		}
		if (pl->skore_before>pl->skore_now)
		{
									text("0721"); // Start Score: ~&J (DOWN by ~&K points)~SM
		}
									text("0722"); // Score: ~&M points!
		if (pl->skore_now<1)		text("0723"); // (WORK Harder!)
		if (rankplayer == 0)		text("0724"); // (You are the #1 BBS!)~SM
		else						text("0725"); // (You are ~&L away from the #1 BBS!)~SM
	}
	else
	{
		text("0726"); // ~SM`0FThis is the latest available information about your nearest competition.~SM
		plyr.funds -= 1.0f;
	}
}

void bank()
{
	s32_2C(1,(s32)plyr.funds);
	if(plyr.funds>=100000.0f) {
		text("0260");
		if(noyes())
        	return;
        else
			{
			text("0261");
			plyr.funds-=(plyr.funds/5.0f);
            }
        }
	text("0262");
}

void charge()
{
	char t1[2];
	int	j,k;

	if((plyr.users_f+plyr.users_p)<50)
		text("0270");
	else{
		text("0271");
		if(yesno()){
			text("0272");
			t1[0]=od_get_answer("0123456");
			t1[1]='\0';
			plyr.charge_lev=atoi(t1);
			if(plyr.charge_lev>0){
				j = rand_num((int)(plyr.users_f/2));
				k = rand_num((int)(plyr.users_f-j));
				plyr.users_p+=j;
				plyr.users_f-=j;
				plyr.users_f-=k;
				if(plyr.users_f<1)
					plyr.users_f=0;
				inum=j;
				s16_2A(1,inum);
				text("0273");
				inum=k;
				s16_2A(1,inum);
				text("0274");
				}
			advance();
			}
		}
}

void delay1(int msec)
{
	/* this releases time slices properly in dos or windows */
	od_sleep(msec);
}

void readmail()
{
	unsigned int i;
	int j,k,ll,q;

	if(plyr.msgs_waiting>0)
	{
		s16_2A(1,plyr.msgs_waiting);
		text("0280"); /* You have n messages waiting to be read. Would you like to answer your mail now? */
		if(yesno())
		{
			upskore(plyr.msgs_waiting);
			if (plyr.employee_actions>0)
				text("0281"); // Employees help.
			k = (40/plyr.msgs_waiting);
			if (k<1)
			{
            	k = 1;
				plyr.msgs_waiting = 40;
			}
			text("0282");
			for(i=0,ll=0;i<plyr.msgs_waiting;i++)
			{
				for(q=0;q<k;q++){
					fflush(stdout);
					text("0283");
					}
				if(plyr.employee_actions>0)
					delay1(250);
				else
					delay1(500);
                ll+=k;
			}
			if (ll<41)
				for (q=ll;q<41;q++)
					text("0283");
			delay1(500);
			UseActions(1);
			plyr.msgs_waiting=0;
			j = rand_num(100);
//			mci(" (%d)",j);
			if(j>69){
				prob_mail();
				text("0284");
			} else	{
                nl();
				j = rand_num((plyr.skill_lev+1)*2);
				text("0285");
				if(j>0)
					{
					inum=j;
					s16_2A(1,inum);
					text("0286");
					plyr.users_f+=j;
					}
				nl();
				}
			}
        }
	else
		text("0287");
}

void prob_mail()
{
	int 	k,i,j,tmp1[7]={1,2,5,0,8,9,0},tmp2[7]={0,0,50,0,5,0,0};

	text("0290");

	if(plyr.msgs_waiting>2)
		j = rand_num(3);
	else
		j=plyr.msgs_waiting-1;

	for(i=j;i>=0;i--)
	{
		inum=i+1;
		UseActions(1);
		s16_2A(1,inum);
		text("0291");
		k = rand_num(msgsrcount);
		read_msgsr(k);
		text("0292");
		switch(od_get_answer("DIPRL"))
		{
		case 'D':
			text("0293");
			act_change(ACT_SUB_FREE_USERS,((plyr.skill_lev+1)*8),1);
            downskore((plyr.skill_lev+1)*10);
			break;
		case 'I':
			text("0294");
			act_change(ACT_SUB_FREE_USERS,1,1);
            downskore(plyr.skill_lev/5);
			break;
		case 'P':
			text("0295");
			j = rand_num(7);
			act_change(tmp1[j],tmp2[j],1);
			upskore((plyr.skill_lev+1)*8);
			break;
		case 'R':
			text("0296");
			act_change(ACT_SUB_FREE_USERS,1,1);
			downskore((plyr.skill_lev+1)*4);
			break;
		case 'L':
			text("0297");
			act_change(ACT_SUB_PAY_USERS,1,1);
			downskore((plyr.skill_lev+1)*8);
			break;
		}
	}
}

void topten()
{
	int i;
	text("0298");
	sort_users();
	for(i=0;i<10 && i<plyrcnt;i++){
		inum2=i+1;
		if(readplyr(&othr,UsersIdx[i].recno)!=FALSE)
			toptenrecord(UsersIdx[i].recno);
        }
}

void vbbsscan()
{
	int 	i,j,k,ts=1;

	str_2A(1,harddrive[plyr.hd_size].hdsize);
	s16_2A(2,(plyr.hd_size+1)*60);
	text("0020");

	if(plyr.hd_size>2){
		text("0021");
		if(yesno()){
			ts=2;
			text("0022");
			}
		else{
			ts=1;
			text("0023");
			}
		}
	else{
		ts=1;
		text("0023");
		}

	text("0024");
	for(i=0;i<13;i++){
		fflush(stdout);
		text("0025");
		delay1(250);
		}
	nl();
	j = rand_num(virusscan*3/ts);
	if(j>virusscan)	{
		text("0026");
		k = rand_num(6)+1;
        for(;k>0;k--)
        	viruslist();
		}
	else
		text("0027");

	text("0028");
	if(ts==2)
		virusscan -= rand_num(5)+5;
	else
		virusscan = 0;
	UseActions(1);
	upskore(10-abs(virusscan));
}

int capacity(void)
{
	unsigned long totusr, tmp1;
	/* unsigned long tmp2; */
	int totmdms;

	totusr = plyr.users_f+plyr.users_p;
	totmdms = plyr.mspeed.low+plyr.mspeed.high+plyr.mspeed.digital;
	inum = plyr.pacbell_installed;
	if (inum>totmdms)
		inum = totmdms;

	/* not currently used
	if ((plyr.mspeed.high+plyr.mspeed.digital)>0)
		tmp2 = totusr/(plyr.mspeed.high+plyr.mspeed.digital);
	else
		tmp2 = 0;
	*/

	if (inum>0)
		tmp1 = totusr/inum;
	else
		tmp1 = 0;
	inum2 = (int)(tmp1/4);

	return inum2;
}

/* this fixes a bug in the MSVC optimizer (v1.52) */
#if defined(_MSC_VER) && !defined(_WIN32)
#pragma optimize ("", off)
#endif

int check_first()
{
/*	int cap; */
/*	int tmp1, tmp2; */

	if (plyr.actions<1)
		return FALSE;

	/* FIXME: this function is completely broken */
#if 0
	/* calculate the bbs capacity */
	cap = capacity();
	/* see if we have any modems */
	if (cap > 0)
	{
		/* more than 75% full? */
		if (cap > 75)
		{
			/* more than 90% full? */
			if (cap > 90)
			{
				tmp1 = rand_num((int)plyr.users_f)/8;
				tmp2 = rand_num((int)plyr.users_p)/8;
				plyr.users_f -= tmp1;
				plyr.users_p -= tmp2;
				inum = tmp1;
				int16_2A(1, inum);
				text("0300");
				inum = tmp2;
				int16_2A(1, inum);
				text("0301");
			}
			else
				text("0302"); /* Your users are complaining ... */
		}
	}
	else
	{
		text("0862");  /* WARNING You have no modems ... */
		plyr.mspeed.low = 1;
	}
#endif

	return TRUE;
}

#if defined(_MSC_VER) && !defined(_WIN32)
#pragma optimize ("", on)
#endif

void status()
{
	if(chatstat>=0){
		if(chatstat>0){
			text("0303");
			}
		else{
			text("0304");
			}
		}

	if (subscribe_arrive>0 && plyr.users_p>0)
	{
		text("0305");
		act_change(ACT_ADD_FUNDS,(int)((plyr.users_p/3)*(plyr.charge_lev+1)),1);
		subscribe_arrive = -99;
	}
	if(plyr.skill_lev>10)	plyr.skill_lev=10;

	if(plyr.msgs_waiting!=0){
		if (rand_num(100)>50){
			inum = (int)(plyr.users_p/6);
			if (inum>((int)plyr.msgs_waiting)){
				inum2 = rand_num(plyr.msgs_waiting);
				if(inum2>((int)plyr.users_p)){
					mci("~CR~CRYour paying users don't like it when you ignore your emails.");
					act_change(ACT_SUB_PAY_USERS,inum2,1);
					}
				}
			}
		else{
			inum = ((int)plyr.users_f/6);
			if(inum>((int)plyr.msgs_waiting)){
				inum2 = rand_num((int)plyr.msgs_waiting);
				if(inum2>((int)plyr.users_f)){
					mci("~CR~CRYour free users don't like it when you ignore your emails.");
					act_change(ACT_SUB_FREE_USERS,inum2,1);
					}
				}
			}
		}
}

void users_online()
{
	int totmdms;

	totmdms = plyr.mspeed.low+plyr.mspeed.high+plyr.mspeed.digital;
	inum = plyr.pacbell_installed;
	if (inum>totmdms)
		inum = totmdms;

	inum2 = capacity();

	s16_2A(1,inum);
	s16_2A(2,inum2);

	text("0880");
	if ((plyr.mspeed.high+plyr.mspeed.digital)>0)
	{
		inum-=plyr.mspeed.low;
		inum2 = (int)(inum2/4);
		s16_2A(1,inum);
		s16_2A(2,inum2);
		text("0881");
	}
}

void UseActions(int numacts)
{
	s16 j;

	if (plyr.employee_actions>0)
	{
		j = rand_num(9)+1;
		if (plyr.employee_actions<j)
		{
			plyr.actions--;
			text("0300");
		}
		plyr.employee_actions -= j;
		if (plyr.employee_actions<0)
			plyr.employee_actions = 0;
	}
	else
	{
		plyr.actions -= numacts;
	}
}

void advance()
{
	u32 level = (((plyr.skill_lev+1)*(plyr.skill_lev+1))*1000);
	if (level<plyr.skore_now)
		plyr.skill_lev++;
	if (rand_num(10)>4)
		plyr.msgs_waiting++;
	virusscan++;
	chatstat++;
    subscribe_arrive++;
}

void ans_chat()
{
	int j;

	if(chatstat<0)
		text("0310");
	else{
		nl();
		nl();
		j = rand_num(4);
		switch(j){
			case 0:
				text("0311");
				act_change(ACT_ADD_FREE_USERS,0,1);
				break;
			case 1:
				text("0312");
				act_change(ACT_ADD_FUNDS,50,1);
				break;
			case 2:
				text("0313");
				downskore((plyr.skill_lev+1)*2);
				break;
			case 3:
				text("0314");
				downskore((plyr.skill_lev+1)*3);
				break;
			}
		}
	chatstat = -rand_num(CONFIG_ACTIONS_PER_DAY/3);
	advance();
	UseActions(1);
}

void not_enough()
{
	text("0315");
}

void bbslist()
{
	char ttxt[512];
	text("0890");
	sort_users();
	for(inum=0;inum<MAX_USERS && inum<plyrcnt;inum++){
		inum2=inum+1;
		readplyr(&othr,UsersIdx[inum].recno);
		s16_2A(1,inum+1);
		str_2A(2,othr.bbsname);
		str_2A(3,othr.sysopname);
		u32_2A(4,othr.skore_now);
		mcigettext("0894",ttxt);
		mcimacros(ttxt);
		str_2A(5,ttxt);
		s16_2A(6,strlen(ttxt));
		mcigettext("0895",ttxt);
		mcimacros(ttxt);
		str_2A(7,ttxt);
		mciEseries(ttxt);
		text("0891");
		if (UsersIdx[inum].recno == (int)plyr.plyrnum)
			text("0892");
		else
			text("0893");
		}
}

void toptenrecord(int i)
{
	u32 lines, users;

	readplyr(&othr,i);
	users = (u32)(othr.users_f+othr.users_p);
	lines = othr.mspeed.high+othr.mspeed.low+othr.mspeed.digital;
	// FIXME: should pacbell_installed be unsigned?
	if (lines > (u32)(othr.pacbell_installed))
		lines = othr.pacbell_installed;
	// FIXME: should bbssw[].numlines be unsigned?
	if (lines > (u32)(bbssw[othr.bbs_sw].numlines))
		lines = (u32)bbssw[othr.bbs_sw].numlines;

	u32_2A(1,users);
	u32_2A(2,lines);
	str_2A(3,othr.sysopname);
	str_2A(5,othr.bbsname);
	u32_2A(6,othr.skore_now);
	s16_2A(4,rankplyr(i)+1);
	text("0900");
}

void change_title()
{
	char	tstr[31];
	int     foundone=0,i;

	text("0400");
	do{
		text("0401");
		*tstr='\0';
		od_input_str(tstr,30,32,127);
		if(strlen(tstr)>0){
			sort_users();
			foundone=TRUE;
			for(i=0;i<MAX_USERS && i<plyrcnt;i++){
				readplyr(&othr,UsersIdx[i].recno);
				if(strcmp(othr.bbsname,tstr)==0 && strcmp(othr.realname,plyr.realname)!=0){
					text("0402");
					foundone=FALSE;
					}
				}
			}
		else
			sprintf(tstr,"%s's BBS",plyr.realname);
		}while(!foundone);
	strcpy(plyr.bbsname,tstr);
}


void change_handle()
{
	char	tstr[31];
	int     foundone=0,i;

	do{
		text("0404");
		*tstr='\0';
		od_input_str(tstr,30,32,127);
		if(strlen(tstr)>0){
			sort_users();
			foundone=TRUE;
			for(i=0;i<MAX_USERS && i<plyrcnt;i++){
				readplyr(&othr,UsersIdx[i].recno);
				if(strcmp(othr.sysopname,tstr)==0 && strcmp(othr.realname,plyr.realname)!=0){
					text("0405");
					foundone=FALSE;
					}
				}
			}
		else
			strcpy(tstr,plyr.realname);
		}while(!foundone);
	strcpy(plyr.sysopname,tstr);
}

void personal()
{
	text("0410");
	while(1){
		s32_2A(1, plyr.actions);
		text("0413");
		text("0411");
		switch(od_get_answer("QLUSIBRD?")){
			case 'Q':
				text("0412");
				return;
			case 'L':
				if(plyr.actions>0)
				{
					learn();
					advance();
                }
				else
                	not_enough();
				break;
            case 'U':
				if(plyr.actions>0){
					use_skill();
					advance();
					}
				else
					not_enough();
				break;
			case 'S':
				if(plyr.actions>0){
					security_menu();
                    advance();
                    }
                else
                	not_enough();
                return;
			case 'I':
				if(plyr.actions>0){
					buy_insurance();
                    advance();
                    }
                else
                	not_enough();
				break;
			case 'B':
                bank();
				break;
			case 'R':
				saveplyr(&plyr,plyr.plyrnum);
				VBBS3_Report(&plyr, FALSE);
				paws();
				break;
			}
		}
}

void inspect()
{
	int 		myrank;
	tUserRecord	inspect;

	UseActions(1);

    sort_users();
    myrank = rankplyr(plyr.plyrnum);
    if (myrank<1)
		text("0490");
	else
	{
		saveplyr(&plyr, plyr.plyrnum);
		readplyr(&inspect, UsersIdx[myrank-1].recno);
		VBBS3_Report(&inspect, TRUE);
    }
}

int	findBBS()
{
	char istr[36],tstr1[36],tstr2[36];
	int  i,x=-1,done=FALSE;

	sort_users();
	while(!done){
		text("0491");
		*istr='\0';
		od_input_str(istr,35,32,127);
		if(istr[0]=='\0')  {done=TRUE; x=-1; }
		if(istr[0]=='?' && !done) bbslist();
		else {
			if(!done){
				strcpy(tstr1,istr);
				strupr(tstr1);
				for(i=0;i<MAX_USERS && i<plyrcnt;i++) {
					readplyr(&othr,UsersIdx[i].recno);
					strcpy(tstr2,othr.bbsname);
					strupr(tstr2);
					if(strstr(tstr2,tstr1)) {
						str_2A(1,othr.bbsname);
						text("0492");
						if(yesno()){
							x=i;
							done=TRUE;
							}
						}
					}
				}
			}
		}
	readplyr(&othr,UsersIdx[x].recno);
	return(x);
}

void sort_users()
{
	tUserIdx	temp;
	unsigned int ii,out,in;

	saveplyr(&plyr,plyr.plyrnum);

	ii=0;
	for(ii=0;ii<MAX_USERS;ii++)
		UsersIdx[ii].skore=-1L;

	ii=0;
	while(readplyr(&othr,ii)){
		UsersIdx[ii].skore=othr.skore_now;
		UsersIdx[ii].recno=othr.plyrnum;
		ii++;
		}

	for(out=0; out<ii-1;out++)
		for(in=out+1;in<ii;in++)
			if(UsersIdx[out].skore<UsersIdx[in].skore){
				temp=UsersIdx[out];
				UsersIdx[out]=UsersIdx[in];
				UsersIdx[in]=temp;
				}
}

long filesize(FILE *stream)
{
   long curpos, length;

   curpos = ftell(stream);
   fseek(stream, 0L, SEEK_END);
   length = ftell(stream);
   fseek(stream, curpos, SEEK_SET);
   return length;
}

int rankplyr(int plyrno)
{
	int i;

	sort_users();
    for(i=0;i<MAX_USERS && i<plyrcnt;i++)
    	if(UsersIdx[i].recno==plyrno)
        	return(i);
	return(-1);
}


void get_time_diff()
{
	time_t tx, secdif;
	int inum;

	/* get current time */
	time(&tx);
	/* calculate the second difference */
	secdif = plyr.laston - tx;
	/* count up the number of days */
	inum = 0;
	while (secdif>(1440L*60L))
	{
		secdif -= (1440L*60L);
		inum++;
	}
	/* complain if they don't play every day */
	s16_2A(1,inum);
	if (inum>1) text("0800");
}

void vbbs_randomize()
{
	/* seed the random number generator */
	srand((unsigned int)time(NULL));
}

/* returns 0 to (num-1) */
int rand_num(int num)
{
	float max = (float)num;
	int rnum;

	rnum = (int)(max*rand()/(RAND_MAX+1.0f));

	return rnum;
}

