/*****************************************************************************
 *
 * File ..................: v3_store.c
 * Purpose ...............: Store routines
 * Last modification date : 20-Mar-2000
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
#include "v3_store.h"
#include "v3_defs.h"
#include "v3_mci.h"
#include "v3_basic.h"
#include "v3_defs.h"

void store()
{
	char ch1;
	int done = 0;

	while (!done)
	{
		text("0100"); // Computer Department Store
		ch1 = od_get_answer("ABCDEQ");
		switch (ch1)
		{
		case 'A':
			text("0101"); // BBS Software Dept
			buy_bbs_sw();
			break;
		case 'B':
			text("0102"); // Computer Dept
			buy_computer();
			break;
		case 'C':
			text("0103"); // Modem Dept
			buy_modems();
			break;
		case 'D':
			text("0104"); // Hard Drive Dept
			buy_harddrive();
			break;
		case 'E':
			text("0105"); // Phone Company
			phone_co();
			break;
		case 'Q':
			text("0106"); // Quit
			nl();
			done = 1;
			break;
		}
	}
}

void phone_co()
{
	char ch,ichar[20];
	int numcanbuy,i1,i2;
	long cost;

	while(1){
		text("0200");
		ch=od_get_answer("QPACDSR");
		switch(ch){
			case 'A':
				numcanbuy = (int)(plyr.funds/50.0f);
				text("0201");	// Add
				do
				{
					s32_2C(1,plyr.pacbell_installed);
					s32_2C(2,plyr.pacbell_ordered);
					if (plyr.pacbell_ordered>0)
						text("0211"); // You have n lines in service
					else
						text("0210"); // You have n lines in service and n lines on order
					if (numcanbuy>1)
					{
						text("0212"); // How many lines do you want to install?
						*ichar = '\0';
						od_input_str(ichar, 7, '0', '9');
						inum = atoi(ichar);
						if (inum>numcanbuy)
						{
							text("0213"); // You cannot afford to purchase that many lines.
							inum = -1;
						}
					}
					else
					{
						text("0214"); // You can not afford to install any more phone lines.
						return;
					}
				} while(inum<0);
				if (inum > 0)
				{
					i1 = 35;
					if (plyr.pacbell_installed+plyr.pacbell_ordered>1)
						i1 = 70;
					i2 = 20;
					if (plyr.pacbell_installed+plyr.pacbell_ordered>3)
						i2 = 50;
					s16_2A(1, i1);
					s32_2C(2, plyr.pacbell_installed+plyr.pacbell_ordered);
					text("0215"); // This installation will cost ...
					if (yesno())
					{
						text("0216"); // Your order has been placed in the computer ...
						plyr.funds -= i1;
						plyr.pacbell_ordered += inum;
					}
				}
				break;
			case 'C':
				text("0202");
				i1=20;
				if(plyr.pacbell_installed+plyr.pacbell_ordered>3)
					i1=50;
				if(plyr.callid)
					text("0220");
				else {
					i2=plyr.pacbell_installed*i1;
					s16_2A(1,i2);
					text("0221");
					if(yesno()) {
						plyr.callid=1;
						text("0222");
						}
					}
				break;
			case 'D':
				text("0203");
				do{
					s32_2C(1,plyr.pacbell_installed-plyr.pacbell_broke);
					s32_2C(2,plyr.pacbell_ordered);
					if(plyr.pacbell_ordered>0)
						text("0230");
					else
						text("0231");
					text("0232");
					od_input_str(ichar, 7, '0', '9');
					inum=atoi(ichar);
					if ( inum <= (int)plyr.pacbell_installed && inum>0){
						s16_2A(1,inum);
						text("0233");
						if(yesno()){
							plyr.pacbell_installed-=inum;
							plyr.pacbell_ordered=0;
							}
						}
				}while(inum<0);
				break;
			case 'P':
				text("0204");
				text("0239");
				break;
			case 'R':

				s32_2C(1,plyr.pacbell_installed+plyr.pacbell_broke);
				s32_2C(2,plyr.pacbell_broke);
				s32_2C(3,plyr.pacbell_ordered);
				if(plyr.callid)	str_2A(4,"Deactivated");
				else			str_2A(4,"Active");
				if(plyr.pacbell_installed>3){
					cost=plyr.pacbell_installed*50;
					str_2A(6,"Commercial");
					}
				else{
					cost=plyr.pacbell_installed*20;
					str_2A(6,"Residential");
					}
				if(plyr.callid)	cost+=9;
				s32_2C(5,cost);
				text("0205");
				text("0238");
				break;
			case 'S':
				text("0206");
				nl();
				if(plyr.funds>=65.0f){
					s32_2C(1,plyr.pacbell_broke);
					text("0240");
					if(plyr.pacbell_broke<1)
						text("0241");
					else
						switch(rand_num(9)){
							case 0: text("0242");break;
							case 1: text("0243");break;
							case 2: text("0244");break;
							case 3: text("0245");break;
							case 4: text("0246");break;
							case 5: text("0247");break;
							case 6: text("0248");break;
							case 7: text("0249");break;
							case 8: text("0250");break;
						}
					text("0251");
					plyr.funds -= 65.0f;
					plyr.pacbell_installed+=plyr.pacbell_broke;
					plyr.pacbell_broke=0;
					}
				else
					text("0207");
				break;
			case 'Q': return;
			}
		}
}

void buy_modems()
{
	char ck, ch;
	char ichar[21], kys[11] = "Q";
	int  cbo = 0;
	s32 xnum = 0;

	s32 num_needed = (s32)(bbssw[plyr.bbs_sw].numlines) - (s32)(plyr.mspeed.low+plyr.mspeed.high+plyr.mspeed.digital);
	if (num_needed>0)
	{
		// display purchase banner
		text("0750");
		// show modems available
		ck = 0;
		for (inum=0;inum<10;inum++)
		{
			if (((int)plyr.funds) >= mtype[inum].cost && ((int)plyr.cpu) >= mtype[inum].req)
			{
				LG = inum+65;
				kys[inum+1] = LG;
				kys[inum+2] = '\0';
				ck = (char)inum;
				cbo = 1;
				char_2A(1, LG);
				str_2A(2, mtype[inum].modemname);
				s32_2C(3, mtype[inum].cost);
				str_2A(4, computer[mtype[inum].req].cpuname);
				text("0751");
			}
		}
		// see if we were unable to buy any
		if (!cbo)
		{
			text("0752");
			return;
		}
		// find out which one we want to buy
		ck += 65;
		char_2A(1, ck);
		text("0753");
		ch = od_get_answer(kys);
		// show answer
		mci("%c\r\n", ch);
		// check for exit
		if (ch=='Q')
			return;
		// calculate what we can afford
		s32 afford = (s32)(plyr.funds / (float)(mtype[ch-65].cost));
		if (afford > num_needed)
			afford = num_needed;
		if (afford > plyr.actions)
			afford = plyr.actions;
		strcpy(TS,mtype[ch-65].modemname);
		xnum = -1L;
		do
		{
			s32_2C(1, afford);
			str_2A(2, mtype[ch-65].modemname);
			text("0754"); // You can afford ...
			od_input_str(ichar, 7, '0', '9');
			xnum = atol(ichar);
			if (xnum>num_needed)
			{
				text("0755");
				xnum = -1L;
            }
		} while(xnum<0L);

		if (xnum == 0L)
			return;

		u32_2A(1,xnum);
		text("0756");
		if (yesno())
		{
            if((ch-65)<3)
				plyr.mspeed.low += (unsigned int)xnum;
			else
				plyr.mspeed.high += (unsigned int)xnum;
			plyr.funds -= (float)(mtype[ch-65].cost*xnum);
			UseActions((int)xnum);
			text("0757");
		}
		else
			text("0758");
	}
	else
		text("0759");
}

void security_menu()
{
	char ch;

	while(1){
		s32_2A(1, plyr.actions);
		text("0540");
		ch=od_get_answer("BCQ");
		switch(ch){
			case 'C': buy_alarm(); break;
			case 'B': buy_security();	break;
			case 'Q': return;
			}
		}
}

void buy_security()
{
	inum = (plyr.security+1)*100;
	s16_2A(1,inum);
	text("0541");
	if (yesno())
	{
		if (plyr.funds<(plyr.security+1)*100)
			text("0542");
		else
		{
			text("0543");
			plyr.funds-=((plyr.security+1)*100);
			plyr.security += 1;
			plyr.actions -= 10;
        }
    }
	else
		text("0544");
}


void buy_alarm()
{
	inum=(int)plyr.callid+10;
	s16_2A(1,inum);
	text("0545");
	if(yesno())
    	{
		if(plyr.funds<(plyr.security+10))
			text("0546");
		else
        	{
			plyr.funds-=(((int)plyr.security+1)+10);
			plyr.security += 1;
			plyr.actions--;
			text("0547");
			}
		}
	else
		text("0548");
}

void buy_insurance()
{
	text("0420");
	if(yesno())
		{
		text("0421");
		plyr.insurance=7;
		}
	else
		text("0422");
}

void buy_bbs_sw()
{
    unsigned int i;
	int		num1,cnt=1;
    char    ch1,kys[12]="Q";
	s32 TLong = 0L;

    text("0730");
    for(i=0;i<MAX_SW;i++) {
        if (((int)plyr.funds) >= bbssw[i].cost && ((int)plyr.cpu) >= bbssw[i].req)
		{
            kys[cnt++]='A'+i;
            kys[cnt]='\0';
            LG='A'+i;
        }
        else
            LG=' ';
        sprintf(TS,"%-30s",bbssw[i].bbsname);
        TLong=bbssw[i].cost;
		char_2A(1,LG);
		str_2A(2,bbssw[i].bbsname);
		s32_2C(3,bbssw[i].cost);
        text("0735");
        if (plyr.bbs_sw>=i)
            text("0732");
        text("0733");
        }
	str_2A(4,kys);
    text("0734");
    ch1=od_get_answer(kys);
    if(ch1=='Q')
        return;
    num1=ch1-65;
    if(plyr.funds >= (float)bbssw[num1].cost) {
        plyr.funds -= (float)bbssw[num1].cost;
        plyr.bbs_sw=num1;
		str_2A(1,bbssw[plyr.bbs_sw].bbsname);
        text("0736");
        UseActions(1);
        }
    else
        text("0737");
}

void buy_computer()
{
    s16 	i;
	s16		num1;
    char    ch1, kys[23]="Q";

    for(i=1;i<23;i++)
        kys[i] = 0;

    text("0740");
    for(i=0;i<MAX_CPU;i++)
	{
        if (plyr.funds>=computer[i].cost)
		{
            kys[i+1] = 'A'+i;
            LG = 'A'+i;
        }
        else
        {
            kys[i+1] = 0;
            LG = ' ';
        }
		char_2A(1,LG);
		str_2A(2,computer[i].cpuname);
		s32_2C(3,computer[i].cost);
        text("0742");
        if (plyr.cpu>=i)
            text("0743");
        text("0744");
	}
    strcpy(TS,kys);
	str_2A(4,kys);
    text("0745");
    ch1 = od_get_answer(kys);
    if (ch1 == 'Q')
        return;
    num1 = ch1-65;
    if (plyr.funds >= (float)computer[num1].cost)
	{
        plyr.funds -= (float)computer[num1].cost;
        plyr.cpu = num1;
		str_2A(1,computer[plyr.cpu].cpuname);
        text("0746");
        UseActions(1);
	}
    else
        text("0747");
    paws();
}

void buy_harddrive()
{
	int		i, num1,cnt=1;
    char    ch1,ch2,kys[10]="Q";


    text("0760");
    for(i=0;i<MAX_HD;i++)
        {
        if(plyr.funds>=computer[i].cost)
            {
            kys[cnt++]='A'+i;
            kys[cnt]='\0';
            ch2='A'+i;
            }
        else
            ch2=' ';
        strcpy(TS,harddrive[i].hdsize);
		char_2A(1,ch2);
		str_2A(2,harddrive[i].hdsize);
		s32_2C(3,harddrive[i].cost);
        text("0761");
        if (plyr.hd_size >= i)
            text("0762");
        text("0764");
        }
	str_2A(5,kys);
    text("0765");
    ch1=od_get_answer(kys);
    if(ch1=='Q')
        return;
    num1=ch1-65;
    if (plyr.funds >= (float)harddrive[num1].cost && ((int)plyr.cpu) >= harddrive[num1].req)
	{
        plyr.funds -= (float)harddrive[num1].cost;
        plyr.hd_size=num1;
        UseActions(1);
		str_2A(1,harddrive[plyr.hd_size].hdsize);
        text("0766");
    }
    else
        text("0767");
    paws();
}


