/*****************************************************************************
 *
 * File ..................: v3_learn.c
 * Purpose ...............: Learning and Schooling routines.
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
#include "v3_learn.h"
#include "v3_hack.h"
#include "v3_mci.h"
#include "v3_basic.h"


void learn()
{
	s32_2A(1, plyr.actions);
	text("0430");
	switch(od_get_answer("QTHBV"))
	{
	case 'Q':
		nl();
		nl();
		return;
	case 'T':
		school();
		break;
	case 'H':
		hire_tutor();
		break;
    case 'B':
        bribe();
        break;
	case 'V':
		visit_hacker();
		break;
	}
}

void hire_tutor()
{
	char	ch,mf;
    int		actns[5] = { 100, 100, 100, 80, 50 },
    		edlev[5] = { 1, 1, 1, 2, 2 },
			num1;
    float	mcosts[5] = { 0.202f,
                          0.101f,
                          0.0508f,
                          0.0501f,
                          0.502f };
	float TFloat = 0.0f;

	s32_2A(1, plyr.actions);
	text("0435");
	ch=od_get_answer("012345");
       if(ch=='0')
       	return;
	else{
		num1=ch-'0';
		mf=0;
		if(((num1+2) % 2)==1)	mf=1;
		if((float)plyr.funds*mcosts[num1-1]<mcosts[num1-1]*1000000L){
			if(mf==1)	text("0436");
			else    	text("0437");
			}
		else{
			TFloat=mcosts[num1-1]*100000L;
			inum=actns[num1-1];
			if(mf) 	text("0438");
			else	text("0439");
			s16_2A(1,inum);
			s32_2C(2,(s32)TFloat);
			text("0440");
			if(yesno()){
                plyr.funds-=(plyr.funds*mcosts[num1-1]);
				plyr.education[1]+=edlev[num1-1];
				}
			}
		}
	plyr.actions--;
}

void visit_hacker()
{
	int 	j;
	float TFloat = 0.0f;
	s32 TLong = 0L;

	TFloat = (float)((plyr.skill_lev+1)*1900);
	u32_2C(1,(u32)TFloat);
	text("0460");
	if(noyes()=='Y'){
		if(plyr.funds<TLong){
			text("0461");
			plyr.actions--;
            }
		else{
			if(plyr.hacktoday>0){
                plyr.actions-=50;
				TLong = (s32)(plyr.funds/2.0f);
				plyr.funds -= (float)TLong;
				s32_2C(1,TLong);
				text("0462");
				}
			else{
				plyr.funds/=10.0f;
            	j = rand_num(80)+10;
            	plyr.actions-=j;
				plyr.education[1] += 0.2f;
				plyr.hacktoday=1;
				text("0463");
				}
            }
        }
	else
		text("0464");
}

void school()
{
	s32_2A(1, plyr.actions);
	text("0450");
	switch(od_get_answer("0123456")){
		case '0':
			return;
		case '1':
			takeclass(0);
			break;
		case '2':
			takeclass(1);
			break;
		case '3':
			takeclass(2);
			break;
		case '4':
			takeclass(3);
			break;
		case '5':
			takeclass(4);
			break;
        case '6':
           	hardknocks();
            break;
		}
}

void takeclass(int classnum)
{

	char	grades[6]="ABCDF",ttxt[6];
	int		istrs[2],ii;
	float   olded,edu;
	float TFloat = 0.0f;
	s32 TLong = 0L;

	nl();
	nl();
	inum = rand_num(classes[classnum].classes_available+1);
	if(inum<1)
		inum=1;
	s16_2A(1,inum);
	if(inum<2)
		text("0470");
	else
		text("0471");
	inum=classes[classnum].average_hours;
	s16_2A(1,inum);
	s16_2A(2,inum);
	text("0472");
	TLong=classes[classnum].cost;
	inum=classes[classnum].apch;
	s16_2A(1,inum);
	text("0473");
	*ttxt='\0';
	od_input_str(ttxt, 2, '0', '9');
	istrs[0]=atoi(ttxt);
    if(istrs[0]<1)
    	return;
	if(istrs[0]>inum)
		istrs[0]=inum;
	inum=classes[classnum].average_hours*classes[classnum].apch;
	s16_2A(1,inum);
	text("0474");
	*ttxt='\0';
	od_input_str(ttxt, 3, '0', '9');
	istrs[1]=atoi(ttxt);
	if((istrs[1]<=(classes[classnum].apch/2))){
		text("0475");
		plyr.actions--;
        return;
		}
	inum=istrs[0];
	s16_2A(1,inum);
	text("0476");
	inum=classes[classnum].cost*istrs[0];
	s16_2A(1,inum);
	text("0477");
	inum=istrs[1];
	s16_2A(1,inum);
	text("0478");
	TFloat=(float)((((float)istrs[1]*(float)istrs[0])/(float)CONFIG_ACTIONS_PER_DAY));
	u32_2C(1, (u32)TFloat);
	text("0479");
	if(plyr.funds>=classes[classnum].cost*istrs[0]){
		text("0480");
		if(noyes()=='Y'){
			edu=((float)istrs[0])/10;
			olded=plyr.education[0];
			plyr.funds-=(classes[classnum].cost*istrs[0]);
			plyr.education[0]+=edu;
			plyr.actions-=(istrs[1]*istrs[0]);
			ii = rand_num(5);
			char_2A(1,grades[ii]);
			text("0481");
			switch(ii){
				case 0: text("0482"); break;
				case 1: text("0483"); break;
				case 2: text("0484"); break;
				case 3: text("0485"); break;
				case 4: text("0486"); break;
				}
			if((int)plyr.education[0]>(int)olded)
				text("0487");
			}
		else{
			plyr.actions--;
        	}
    	}
    else
		{
		text("0488");
		plyr.actions--;
        }
    paws();
}

void hardknocks()
{
	inum = rand_num(51)+100;
	s16_2A(1,inum);
	text("0570");
	if(yesno())
	{
		plyr.actions-=inum;
        plyr.education[1]++;
    }
}

