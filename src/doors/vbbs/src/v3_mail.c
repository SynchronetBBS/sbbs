/*****************************************************************************
 *
 * File ..................: v3_mail.c
 * Purpose ...............: Intergame mail routines
 * Last modification date : 17-Mar-2000
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

#include <string.h>
#include "vbbs.h"
#include "v3_defs.h"
#include "v3_io.h"
#include "v3_mail.h"
#include "v3_mci.h"
#include "v3_basic.h"

char rettxt[81];
const char* strMailItems[] =
{
	"action",
	"dollar"
};

int countmail()
{
	mailrec mr;
	int i=0;

	while(readmaildat(&mr,i))	i++;
	return i;
}

void sendmail()
{
	FILE *fptr1,*fptr2;
	char ch;
	char tpath[161];
	char path[161];
	char ttxt[161];
	char ttxt1[161];
	int done=FALSE,done1=FALSE,lcnt=1;
	long lnum;
	mailrec mail;

	sprintf(tpath,"%smailtxt.tmp",gamedir);
	if(fexist(tpath))	remove(tpath);
	sprintf(path,"%smailtxt.vb3",gamedir);

	// clear out mail
	memset(&mail, 0, sizeof(mailrec));
	mail.idx=countmail();
	mail.letternum=mail.idx;
	if(findBBS()==-1)
		return;
	mail.from=plyr.plyrnum;
	mail.to=othr.plyrnum;
	mail.deleted=FALSE;
	mail.fsys=0;
	mail.tosys=0;
	(void)time(&mail.date);

	text("1100"); //mci("~SM`09S`01ubject: `1F");
	*mail.subject='\0';
	od_input_str(mail.subject,68,32,127);
	text("1101"); //mci("~SM   `07Type message below.  <ENTER> on a blank line ends.");
	text("1102"); //mci("~SM   [---+----+----+----+----+----+----+----+----+----+----+----+----+----+----]");
	while(!done)
	{
		mci("`07%3d`02:`0F",lcnt);
		*ttxt = '\0';
		od_input_str(ttxt,75,32,127);
		if(ttxt[0] == '\0')
		{
			done = TRUE;
		}
		else
		{
			sprintf(ttxt1,"%04d `09³ `07%-74s`09³~SM\n", mail.letternum, ttxt);
			mcimacros(ttxt1);
			writemail(tpath,ttxt1);
			lcnt++;
			*ttxt = '\0';
		}
	}
	text("1110"); //mci("~SM~SMWould you like to send something? ");
	if(yesno()){
		text("1111"); //mci("~SMWhat would you like to send? ~SM");
		mci("~SM(1) Actions           : %d",plyr.actions);
		commafmt_s32(ttxt, 21, (s32)plyr.funds);
		mci("~SM(2) Money             : %s",ttxt);
		text("1119"); //mci("~SM~SMWhich? [1-2, 0=Quit] : ");
		ch=od_get_answer("123456780");
		switch(ch){
			case '0': break;
			case '1':
				done1=FALSE;
				while(!done1){
					mci("~SMSend how many actions? [0-%d]: ",plyr.actions);
					od_input_str(ttxt,10,'0','9');
					if (strlen(ttxt)>0)
					{
						lnum=atoi(ttxt);
						if (lnum>(int)plyr.actions)
						{
							// mci("~SM~SMYou don't have that many actions to give.~SM");
							// mci("~SMPlease try again.~SM");
							text("1120");
						}
						else
						{
							mci("~SMSend %d actions to %s?", lnum, othr.sysopname);
							if (yesno())
							{
								mail.amount = (int)lnum;
								mail.item = ch-'0';
								plyr.actions -= lnum;
								done1 = TRUE;
							}
						}
					}
					else
						done1=TRUE;
					}
				break;
			case '2':
				commafmt_s32(ttxt, 21, (s32)plyr.funds);
				mci("~SMSend how much money? [0-%s]: ",ttxt);
				od_input_str(ttxt,10,'0','9');
				lnum=atol(ttxt);
				if(lnum<=plyr.funds){
					commafmt_s32(ttxt, 21, (s32)lnum);
					mci("~SMSend $%s? [Yes]: ",ttxt);
					if(yesno()){
						mail.item = ch-'0';
						mail.amount = (int16)lnum;
						plyr.funds -= (float)lnum;
						}
					}
				break;
			}
		}
	text("1130"); // mci("~SMSend this message?");
	if(yesno()){
		mail.idx=countmail();
		savemaildat(&mail,mail.idx);
		fptr1=fopen(path,"at");
		fptr2=fopen(tpath,"rt");
		while(fgets(ttxt,161,fptr2)!=NULL){
			fputs(ttxt,fptr1);
			}
		fclose(fptr1);
		fclose(fptr2);

		}
	if(fexist(tpath))	remove(tpath);
}

void network()
{
	while(1)
	{
		text("1140"); // Network Mail Menu
		switch(od_get_answer("PSQ"))
		{
		case 'P':	readnetmail();	break;
		case 'S':	sendmail();		break;
		case 'Q':	mci("Quit~SM");	return;
		}
	}
}

void viewmail(int letter)
{
	FILE *fptr;
	char ltr[6], path[_MAX_PATH], ttxt[500], ttxt1[500];

	sprintf(ltr,"%04d ",letter);

	sprintf(path,"%smailtxt.vb3",gamedir);
	fptr = fopen(path,"rt");
	if (fptr)
	{
		while (fgets(ttxt,161,fptr) != NULL)
		{
			if (strncmp(ttxt,ltr,4) == 0)
			{
				stripCR(ttxt);
				strcpy(ttxt1,ttxt+5);
				mci("%s",ttxt1);
			}
		}
	}
}

void showhdr(mailrec *mr)
{
	struct tm *tm;
	char ttxt[81] = {"01/01/1900"};

	tm=localtime(&mr->date);
	strftime(ttxt,80,"%m/%d/%C%Y  %H:%M%p",tm);
	str_2A(1,othr.sysopname);
	str_2A(2,ttxt);
	str_2A(3,mr->subject);
	// mci("~SM`09Ú~EFÄ75~SM");
	// mci("`09³ `0FFrom: ~EL68~&1`09³~SM");
	// mci("`09³ `0FDate: ~EL68~&2`09³~SM");
	// mci("`09³ `0FSubj: ~EL68~&3`09³~SM");
	// mci("[~EFÄ75]~SM");
	text("1150");
}

void showfooter(void)
{
	text("1151");
}

void readnetmail()
{
	int mnum;
	int foundone = FALSE;
	mailrec mail;

	mnum = 0;
	while (readmaildat(&mail,mnum))
	{
		if ((mail.tosys == 0) && (mail.to == plyr.plyrnum) && (mail.deleted == FALSE))
		{
			foundone = TRUE;
			showhdr(&mail);
			viewmail(mail.letternum);
			showfooter();
			if (mail.item > 0)
			{
				readplyr(&othr,mail.from);
				if (mail.amount > 1)
					mci("~SM!!! %s has sent you %d %ss.", othr.sysopname, mail.amount, strMailItems[mail.item-1]);
				else
					mci("~SM!!! %s has sent you %d %s.", othr.sysopname, mail.amount, strMailItems[mail.item-1]);
				if (mail.item == 1)
				{
					plyr.actions += mail.amount;
				}
				else if (mail.item == 2)
				{
					plyr.funds += mail.amount;
				}
				// clear item and amount so they don't get it again
				mail.amount = 0;
				mail.item = 0;
			}
			text("1160"); // mci("~SM~SMDelete this message? [Yes]: ");
			if (yesno())
			{
				mail.deleted = TRUE;
			}
		}
		savemaildat(&mail,mnum);
		mnum++;
	}
	if (!foundone)
	{
		text("1161"); // mci("~SM~SMYour mailbox is empty~SM");
	}
}
