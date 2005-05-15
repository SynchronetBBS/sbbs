/*****************************************************************************
 *
 * File ..................: v3_hack.c
 * Purpose ...............: Hacking functions
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

#include "vbbs.h"
#include "v3_defs.h"
#include "v3_hack.h"
#include "v3_io.h"
#include "v3_mci.h"
#include "v3_basic.h"

void bribe()
{
	char istr[21];
	s32 tlong;

	text("0550");
	if(noyes()=='Y')
	{
		text("0551");
		od_input_str(istr,9,'0','9');
		tlong = atol(istr);
		if(plyr.funds >= (float)tlong)
		{
			plyr.funds -= (float)tlong;
			s32_2C(1,tlong);
			text("0552");
			if(tlong < plyr.education[1]*1000)
				text("0553");
			else
			{
				text("0554");
				plyr.education[1] += 0.1f;
			}
		}
		else
			text("0553");
	}
    nl();
    plyr.actions--;
    paws();
}


void hacking()
{
	int		ii=0, x, mm, nn, oo=0;
	char	ch;

	text("0580");
	if((plyr.education[0]+plyr.education[1])>othr.security){
		s16_2A(1,othr.security);
		text("0581");
		ch=od_get_answer("ABCDE");
		switch(ch){
			case 'A': text("0582");break;
			case 'B': text("0583");break;
			case 'C': text("0584");break;
			case 'D': text("0585");break;
			case 'E': text("0586");break;
			}
		if((plyr.education[0]+plyr.education[1])>othr.security){
			ii = rand_num((int)plyr.education[0]+(int)plyr.education[1]);
			if (ii>othr.security)
			{
				text("0587");
				plyr.actions-=95;
				switch(ch){
					case 'A':
					{
						int tmp_users = (int)othr.users_f;
						inum = tmp_users;
						tmp_users -= rand_num(tmp_users)/6;
						inum -= tmp_users;
						othr.users_f = (s32)tmp_users;
						s16_2A(1,inum);
						str_2A(2,othr.bbsname);
						text("0590");
						break;
					}
					case 'B':
					{
						int plyr_users = (int)plyr.users_p;
						int othr_users = (int)othr.users_p;
						inum = plyr_users;
						othr_users -= rand_num(othr_users)/6;
						inum -= plyr_users;
						othr.users_p = othr_users;
						s16_2A(1,inum);
						str_2A(2,othr.bbsname);
						text("0591");
						break;
					}
					case 'C':
						othr.security -= 1;
						if (othr.security < 0)
							othr.security = 0;
						break;
					case 'D':
						x = rand_num(3);
						switch(x){
							case 0:	/* modem */
								mm = othr.mspeed.low + othr.mspeed.high + othr.mspeed.digital;
								nn = mm/3;
								if(nn>1)
									oo = rand_num(nn);
								if(oo>othr.mspeed.low && othr.mspeed.low>0)
									oo=othr.mspeed.low;
								else{
									if(oo>othr.mspeed.high &&
										othr.mspeed.high>0)
										oo=othr.mspeed.high;
									else
										if(oo>othr.mspeed.digital &&
											othr.mspeed.digital>0)
											oo=othr.mspeed.digital;

									}

								mci("~SMYour virus send pulses to the modem port, causing them to overheat.");
								mci("~SMAs a result, %d modems are fried!",oo);

								break;
							case 1:	/* computer */
								mci("~SMYour virus causes the computer to go into an endless loop, trying to ");
								mci("~SMcalculate the value of pi to the nth degree.  It causes the cpu to ");
								mci("~SMoverheat and finally burst into flames.");
								othr.cpu=0;
								break;
							case 2:	/* HDD */
								mci("~SMYour virus causes the computer to try to write data to a single spot");
								mci("~SMon the hard drive over, and over, and over, and over again.  The result");
								mci("~SMis that the media has been scraped off the disk by the heads.");
								/* othr.hdd=0; */
								break;
							}
						break;
					case 'E':
						break;
					}
				}
			else
				text("0588");
		}

		plyr.skore_now+=(ii*2);
		saveplyr(&plyr,plyr.plyrnum);
		saveplyr(&othr,othr.plyrnum);
	}

	mci("~SM~SM");
}

void burgle()
{
	char ch;
	int done=FALSE;

	text("0535");
	if(noyes()=='Y'){
		if(findBBS()==-1)	return;
		if(plyr.education[1]<=4 || plyr.education[1] <= othr.security){
			plyr.actions-=75;
			plyr.funds-=180.0f;
			text("0536");
			}
		else{
			mci("~CR~CRCongratulations!  You made it in!");
			while(!done){
				mci("~CRNow, what do you want to do now?:~CR");
				mci("A) Take Modems~CR");
				mci("B) Take Computer~CR");
				mci("C) Take Hard Drive~CR");
				mci("D) Take BBS Software~CR");
				mci("Q) Never mind.~CR");
				mci("~CRWhich? (A-D, or Q to Quit) : ");
				ch=od_get_answer("ABCDQ");
				switch(ch){
					case 'Q':
						mci("~CRYou find nothing of value here.~CR");
						done=TRUE;
						break;
					case 'A':
						break;
					case 'B':
						if(plyr.cpu<othr.cpu){
							plyr.cpu=othr.cpu;
							othr.cpu=0;
							mci("~CRYou now have a %s~CR",computer[plyr.cpu].cpuname);
							done=TRUE;
							saveplyr(&othr,othr.plyrnum);
							}
						else{
							mci("~CRYou already have better than what you can find in here.");
							mci("~CRBetter try something else.~CR");
							}
						break;
					case 'C':
						if(plyr.hd_size<othr.hd_size){
							plyr.hd_size=othr.hd_size;
							othr.hd_size=0;
							mci("~CR~CRYou now have a %s~CR",harddrive[plyr.hd_size].hdsize);
							done=TRUE;
							saveplyr(&othr,othr.plyrnum);
							}
						else{
							mci("~CRYou already have better than what you can find in here.");
							mci("~CRBetter try something else.~CR");
							}
						break;
					case 'D':
						if(plyr.bbs_sw<othr.bbs_sw){
							plyr.bbs_sw=othr.bbs_sw;
							othr.bbs_sw=0;
							mci("~CR~CRYou now have a %s software package~CR",bbssw[plyr.bbs_sw].bbsname);
							done=TRUE;
							saveplyr(&othr,othr.plyrnum);
							}
						else{
							mci("~CRYou already have better than what you can find in here.");
							mci("~CRBetter try something else.~CR");
							}
						break;
					}
				}
			}
		}
	else
		text("0537");
}

void use_skill()
{
	s32_2A(1, plyr.actions);
	text("0500");
	switch(od_get_answer("QHBSR"))
	{
    case 'Q':
        nl();nl();
        return;
    case 'H':
        hack();
        break;
    case 'B':
        burgle();
        break;
    case 'S':
        shoplift();
        break;
    case 'R':
        robbank();
        break;
  	}
}

void shoplift()
{
	int done=FALSE;
	char ch;

	text("0510");
	if(noyes()=='Y'){
		if(plyr.education[1] >= rand_num(10)){
			mci("~CR~CRCongratulations!  You made it in!");
			while(!done){
				mci("~CRNow, what do you want to do now?:~CR");
				mci("A) Take Modems~CR");
				mci("B) Take Computer~CR");
				mci("C) Take Hard Drive~CR");
				mci("D) Take BBS Software~CR");
				mci("Q) Never mind.~CR");
				mci("~CRWhich? (A-D, or Q to Quit) : ");
				ch=od_get_answer("ABCDQ");
				switch(ch){
					case 'Q':
						mci("~CRYou find nothing of value here.~CR");
						done=TRUE;
						break;
					case 'A':
						break;
					case 'B':
						if(plyr.cpu<MAX_CPU){
							plyr.cpu++;
							mci("~CRYou now have a %s~CR",computer[plyr.cpu].cpuname);
							done=TRUE;
							}
						else{
							mci("~CRYou already have better than what you can find in here.");
							mci("~CRBetter try something else.~CR");
							}
						break;
					case 'C':
						if(plyr.hd_size<MAX_HD){
							plyr.hd_size++;
							mci("~CR~CRYou now have a %s~CR",harddrive[plyr.hd_size].hdsize);
							done=TRUE;
							}
						else{
							mci("~CRYou already have better than what you can find in here.");
							mci("~CRBetter try something else.~CR");
							}
						break;
					case 'D':
						if(plyr.bbs_sw<MAX_SW){
							plyr.bbs_sw++;
							mci("~CR~CRYou now have a %s software package~CR",bbssw[plyr.bbs_sw].bbsname);
							done=TRUE;
							}
						else{
							mci("~CRYou already have better than what you can find in here.");
							mci("~CRBetter try something else.~CR");
							}
						break;
					}
				}
			}
		else{
			text("0515");
			plyr.actions-=(150*(plyr.rapsheet+1));
			plyr.rapsheet++;
			plyr.funds-=(plyr.funds/2.0f);
			paws();
			}
        }
    else
		text("0516");
	plyr.actions++;
}

void robbank()
{
	text("0520");
	if (yesno())
	{
		plyr.actions = -CONFIG_ACTIONS_PER_DAY;
		text("0521");
		paws();
	}
	else
		text("0522");
}

void hack()
{
	int i;
	float TFloat;

	text("0530");
	if(yesno()){
        if(findBBS()==-1)
			return;

		if(plyr.education[0] >= 1.0 || plyr.education[1]>=1.0){
			i = rand_num((int)othr.education[0]+(int)othr.education[1]+othr.security+othr.callid);
			if(i<((int)plyr.education[0]+(int)plyr.education[1]))
				hacking();
			else{
				mci("~CR~CRSorry.  You failed to hack into this BBS.~CR");
				mci("Their security is just to good for your~CR");
				mci("level of education.~CR~CR");
				}
			}
		else{
            plyr.rapsheet++;
        	plyr.actions-=(plyr.rapsheet*50);
			plyr.funds-=plyr.funds/2.0f;
			TFloat=plyr.funds/2.0f;
			s32_2C(1,(s32)TFloat);
			text("0531");
			paws();
            }
		}
}
