/*

The Clans BBS Door Game
Copyright (C) 1997-2002 Allen Ussher

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/*
 * Mail ADT
 *
 *
 */


#include <stdio.h>
#include <stdlib.h>
#ifndef __FreeBSD__
#include <malloc.h>
#endif
#include <string.h>
#include <ctype.h>
#ifdef __unix__
#include "unix_wrappers.h"
#else
#include <share.h>
#include <dos.h>
#endif

#include <OpenDoor.h>
#include "structs.h"
#include "language.h"
#include "mstrings.h"
#include "myopen.h"
#include "user.h"
#include "door.h"
#include "alliance.h"
#include "fight.h"
#include "video.h"
#include "packet.h"
#include "ibbs.h"
#include "input.h"

extern struct clan *PClan;
extern struct Language *Language;
extern struct config *Config;
extern struct system System;
extern struct village Village;
extern struct ibbs IBBS;
extern struct game Game;

// Message Types:
#define MT_PUBLIC       0
#define MT_PRIVATE      1
#define MT_ALLIANCE     2

// Message Flags:
#define MF_DELETED      1           // Message has been deleted
#define MF_NOFROM       2           // No "from" field, automatic message,
// don't allow replies to it either
#define MF_ALLYREQ      4           // Ally request message
#define MF_GLOBAL       8           // League message
#define MF_ONEVILLONLY  16          // message sent to only ONE village, not all

#define ALL_VILLAGES    -1

_INT16 InputStr(char *String, char *NextString, char *JustLen, _INT16 CurLine);
void SendMsj(struct Message *Message, _INT16 WhichVillage);

// ------------------------------------------------------------------------- //

void GetUserNames(char *apszUserNames[50], _INT16 WhichVillage, _INT16 *NumUsers,
				  _INT16 ClanIDs[50][2])
{
	FILE *fpUList;
	struct UserInfo User;
	_INT16 CurUser;

	// set all to NULLs
	for (CurUser = 0; CurUser < 50; CurUser++)
		apszUserNames[CurUser] = NULL;
	*NumUsers = 0;

	// open user file
	fpUList = _fsopen("userlist.dat", "rb", SH_DENYWR);
	if (!fpUList)    // no user list at all
		return;


	// go through, pick up those users which hail from this village

	for (CurUser = 0;;) {
		if (EncryptRead(&User, sizeof(struct UserInfo), fpUList, XOR_ULIST) == 0)
			break;

		// user from that village?
		if (User.ClanID[0] == WhichVillage) {
			// found one from there
			apszUserNames[CurUser] = malloc(strlen(User.szName) + 1);
			CheckMem(apszUserNames[CurUser]);
			strcpy(apszUserNames[CurUser], User.szName);
			ClanIDs[CurUser][0] = User.ClanID[0];
			ClanIDs[CurUser][1] = User.ClanID[1];
			CurUser++;
		}
	}

	// close file
	fclose(fpUList);

	*NumUsers = CurUser;
}

// ------------------------------------------------------------------------- //

void GenericReply(struct Message *Reply, char *szReply, BOOL AllowReply)
{
	struct Message Message;
	FILE *fp;
	_INT16 player_num, result, i, FirstLine, LastLine;
	char filename[50], key;

	// malloc msg
	Message.Data.MsgTxt = malloc(4000);
	CheckMem(Message.Data.MsgTxt);

	Message.Data.MsgTxt[0] = 0;

	// strcpy(Message.szFromName, GlobalPlayerClan->szName);
	// fromname not used in generic msgs
	strcpy(Message.szFromName, Reply->szFromName);
	strcpy(Message.szFromVillageName, Village.Data->szName);
	Message.ToClanID[0] = Reply->FromClanID[0];
	Message.ToClanID[1] = Reply->FromClanID[1];

	if (PClan == NULL) {
		Message.FromClanID[0] =  -1;
		Message.FromClanID[1] =  -1;
	}
	else {
		Message.FromClanID[0] =  PClan->ClanID[0];
		Message.FromClanID[1] =  PClan->ClanID[1];
	}
	strcpy(Message.szDate, System.szTodaysDate);

	if (AllowReply == FALSE)
		Message.Flags = MF_NOFROM;

	Message.PublicMsgIndex = 0;
	Message.MessageType = MT_PRIVATE;

	strcpy(Message.Data.MsgTxt, szReply);

	Message.Data.Length = strlen(szReply) + 1;
	Message.Data.NumLines = 1;
	Message.Data.Offsets[0] = 0;
	Message.Data.Offsets[1] = 0;

	// save message
	fp = _fsopen(ST_MSJFILE, "a+b", SH_DENYRW);
	if (!fp) {
		free(Message.Data.MsgTxt);
		DisplayStr(ST_NOMSJFILE);
		return;
	}
	EncryptWrite(&Message, sizeof(Message), fp, XOR_MSG);
	EncryptWrite(Message.Data.MsgTxt, Message.Data.Length, fp, XOR_MSG);
	fclose(fp);

	free(Message.Data.MsgTxt);
	(void)result;
	(void)LastLine;
	(void)filename;
	(void)key;
	(void)FirstLine;
	(void)i;
	(void)player_num;
}


// ------------------------------------------------------------------------- //
void GenericMessage(char *szString, _INT16 ToClanID[2], _INT16 FromClanID[2], char *szFrom, BOOL AllowReply)
{
	struct Message Message;

	// if clan doesn't exist, return
	if (ClanExists(ToClanID) == FALSE)  return;

	/* write generic message -- set it up as a "reply" */
	Message.ToClanID[0] = FromClanID[0];
	Message.ToClanID[1] = FromClanID[1];

	Message.FromClanID[0] = ToClanID[0];
	Message.FromClanID[1] = ToClanID[1];

	strcpy(Message.szFromName, szFrom);
	strcpy(Message.szDate, System.szTodaysDate);
	strcpy(Message.szFromVillageName, Village.Data->szName);
	Message.Flags = 0;
	Message.MessageType = MT_PRIVATE;

	GenericReply(&Message, szString, AllowReply);
}


// ------------------------------------------------------------------------- //
void MyWriteMessage2(_INT16 ClanID[2], BOOL ToAll,
					 BOOL AllyReq, _INT16 AllianceID, char *szAllyName,
					 BOOL GlobalPost, _INT16 WhichVillage)
{
	struct Message Message;
	FILE *fp;
	_INT16 result, NumLines = 0, CurChar = 0, i;
	char string[128], JustLen = 78;
	char Line1[128], Line2[128], OldLine[128];

	if (ToAll == FALSE && ClanID[0] == -1) {
		if (GetClanID(ClanID, FALSE, FALSE, -1, FALSE) == FALSE) {
			return;
		}
	}

	/* malloc msg */
	Message.Data.MsgTxt = malloc(4000);
	CheckMem(Message.Data.MsgTxt);

	Message.Data.MsgTxt[0] = 0;

	Message.ToClanID[0] = ClanID[0];
	Message.ToClanID[1] = ClanID[1];
	strcpy(Message.szFromName, PClan->szName);
	strcpy(Message.szFromVillageName, Village.Data->szName);
	Message.FromClanID[0] = PClan->ClanID[0];
	Message.FromClanID[1] = PClan->ClanID[1];

	Message.BBSIDFrom = Config->BBSID;

	strcpy(Message.szDate, System.szTodaysDate);
	Message.Flags = 0;
	Message.MessageType = MT_PRIVATE;

	if (AllyReq) {
		Message.Flags |= MF_ALLYREQ;
		strcpy(Message.szAllyName, szAllyName);
		Message.AllianceID = AllianceID;
	}
	else if (AllianceID != -1) {
		// not an alliance, but AllyID exists, so make this an alliance-only MSG
		Message.AllianceID = AllianceID;
		strcpy(Message.szAllyName, szAllyName);
		Message.MessageType = MT_ALLIANCE;
	}

	if (GlobalPost) {
		Message.Flags |= MF_GLOBAL;

		if (ToAll)
			Message.MessageType = MT_PUBLIC;
	}

	// directed at one BBS only
	if (WhichVillage != -1 && GlobalPost && ClanID[0] == -1)
		Message.Flags |= MF_ONEVILLONLY;

	rputs(ST_MAILHEADER);
	rputs(ST_LONGDIVIDER);

	Line1[0] = 0;
	Line2[0] = 0;
	OldLine[0] = 0;
	for (;;) {
		rputs(ST_MAILENTERCOLOR);
		result = InputStr(Line1, Line2, &JustLen, NumLines);

		// if no swearing allowed, convert it :)
		//if (GameInfo.NoSwearing)
		//    RemoveSwear(Line1);

		if (result == 1) {  // save
			if (NumLines == 0) {
				rputs(ST_MAILNOTEXT);
				continue;
			}

			/* ask if public post or not */
			if (ToAll) {
				/* public post */
				if (!(GlobalPost && (Message.Flags & MF_ONEVILLONLY))) {
					Message.PublicMsgIndex = Village.Data->PublicMsgIndex;
					Village.Data->PublicMsgIndex++;
				}
			}
			else
				Message.PublicMsgIndex = 0;

			rputs(ST_MAILSAVED);
			break;
		}
		else if (result == -1) {  // abort
			rputs(ST_MAILABORTED);

			free(Message.Data.MsgTxt);
			return;
		}
		else if (result == -2) {  // start over
			rputs(ST_MAILSTARTOVER);
			NumLines = 0;
			CurChar = 0;
			Line1[0] = 0;
			Line2[0] = 0;

			continue;
		}
		else if (result == 3) { // list
			if (NumLines == 0) {
				rputs(ST_MAILNOTEXTYET);
				continue;
			}
			for (i = 0; i < NumLines; i++) {
				sprintf(string, ST_MAILLIST, i+1, &Message.Data.MsgTxt[ Message.Data.Offsets[i] ]);
				string[84] = 0;
				strcat(string, "\n");
				strcat(string, ST_MAILENTERCOLOR);
				rputs(string);
			}
			continue;
		}
		else if (result == 4) { // backspace
			if (NumLines == 0)
				continue;       // can't go back any more

			// otherwise
			rputs("\n|03Backing up a line\n");

			// else continue
			NumLines--;
			CurChar -= (strlen(OldLine) + 1);

			strcpy(Line1, OldLine);

			if (NumLines == 0)
				OldLine[0] = 0;
			else
				strcpy(OldLine, &Message.Data.MsgTxt[Message.Data.Offsets[NumLines-1]]);

			Line2[0] = 0;
			continue;
		}


		// else continue
		Message.Data.Offsets[NumLines] = CurChar;
		strcpy(&Message.Data.MsgTxt[CurChar], Line1);
		CurChar += (strlen(Line1) + 1);

		strcpy(OldLine, Line1);
		strcpy(Line1, Line2);
		NumLines++;

		if (NumLines == 40) {
			rputs(ST_MAIL40LINES);
			break;
		}
	}
	Message.Data.Length = CurChar;
	Message.Data.NumLines = NumLines;


	// save message

	// if local post OR globalpost to ALL villages, do this
	if ((GlobalPost && WhichVillage == -1) || GlobalPost == FALSE) {
		fp = _fsopen(ST_MSJFILE, "ab", SH_DENYRW);
		if (!fp) {
			rputs(ST_NOMSJFILE);
			return;
		}
		EncryptWrite(&Message, sizeof(Message), fp, XOR_MSG);
		EncryptWrite(Message.Data.MsgTxt, Message.Data.Length, fp, XOR_MSG);
		fclose(fp);
	}

	if (GlobalPost)
		SendMsj(&Message, WhichVillage);

	free(Message.Data.MsgTxt);
}




// ------------------------------------------------------------------------- //
_INT16 QInputStr(char *String, char *NextString, char *JustLen, struct Message *Reply,
				 _INT16 CurLine)
{
	_INT16 cur_char = 0, i, FirstLine, LastLine;
	unsigned char ch, key;
	char string[128];

	rputs(ST_MAILENTERCOLOR);
	rputs(String);
	cur_char = strlen(String);

	for (;;) {
		if (cur_char == (*JustLen)) {
			String[cur_char] = 0;

			// break off last String
			for (i = strlen(String); i>0; i--) {
				if (String[i] == ' ') {
					i++;
					break;
				}
			}

			if (i > ((*JustLen)/2)) {
				strcpy(NextString, &String[i]);
				String[i] = 0;

				for (; i < cur_char; i++)
					rputs("\b \b");

				rputs("\n");
				return 0;
			}
			else {
				rputs("\n");
				NextString[0] = 0;
				return 0;
			}
		}

		ch = od_get_key(TRUE);
		if (ch == '\b') {
			if (cur_char>0) {
				cur_char--;
				rputs("\b \b");
			}
			else if (cur_char == 0 && CurLine > 0)
				return 4;
		}
		else if ((isalnum(ch) || ispunct(ch)) && (String[cur_char-1] == '%'  || String[cur_char-1]=='&'))
			continue;
		else if ((isdigit(ch) || toupper(ch) == 'S') && String[cur_char-1] == '@')
			continue;
		else if (ch == 25) { // CTRL-Y
			rputs("\r                                                                     \r");
			rputs(ST_MAILENTERCOLOR);
			cur_char = 0;
			String[0] = 0;
			NextString[0] = 0;

			continue;
		}
		else if (ch== 13) {
			rputs("\n");
			String[cur_char]=0;
			NextString[0]=0;
			break;
		}
		/*
		        else if (ch == '\t')    // tab
		        {
		            rputs("     ");
		            String[cur_char] = '\t';
		            cur_char++;
		        }
		*/
		else if (iscntrl(ch) && ch != '\r' && ch != '\n')
			continue;
		else if (ch == '/' && cur_char == 0) {
			rputs(ST_INPUTSTRCOMMAND);

			key = toupper(od_get_answer("SACQLR?/\r\n"));

			if (key == '?') {
				rputs(ST_INPUTSTRHELP2);
				key = toupper(od_get_answer("SACQLR/\r\n"));
			}

			if (key == 'C' || key == '\r' || key == '\n') {
				rputs(ST_LONGSPACES);
				rputs(ST_MAILENTERCOLOR);
				continue;
			}
			else if (key == 'R') {
				rputs(ST_LONGSPACES);
				rputs(ST_INPUTSTRSTARTOVER);
				rputs(STR_NOYES);
				rputs("|15");

				if (od_get_answer("YN\r\n") == 'Y') {
					rputs(ST_YES);
					rputs("\n");
					return -2;
				}
				rputs(ST_LONGSPACES);
				rputs(ST_MAILENTERCOLOR);
			}
			else if (key == 'S') {
				rputs(ST_LONGSPACES);
				rputs(ST_INPUTSTRSAVE);
				rputs(STR_YESNO);
				rputs("|15");

				if (od_get_answer("YN\r\n") == 'N') {
					rputs(ST_LONGSPACES);
					rputs(ST_MAILENTERCOLOR);
					continue;
				}

				rputs(ST_YES);
				rputs("\n");

				NextString[0] = 0;
				return 1;
			}
			else if (key == 'A') {
				rputs(ST_LONGSPACES);
				rputs(ST_INPUTSTRABORT);
				rputs(STR_NOYES);
				rputs("|15");

				if (od_get_answer("YN\r\n") == 'Y') {
					rputs(ST_YES);
					rputs("\n");
					return -1;
				}
				rputs(ST_LONGSPACES);
				rputs(ST_MAILENTERCOLOR);
			}
			/*
			else if (key == 'W')
			{
			  rputs("\r|05Enter new wrap-around detection width x10 [|1379|05] : |05");
			  i = (od_get_answer("2345678")-'0') * 10  - 1;
			  *JustLen = i;
			  rputs("\r                                                                     \r");
			  rputs(ST_MAILENTERCOLOR);
			}
			*/
			else if (key == 'L') {
				rputs("\n");
				NextString[0]=0;
				String[0]=0;
				return 3;
			}
			else if (key == 'Q') {
				rputs(ST_LONGSPACES);
				rputs(ST_MAILENTERCOLOR);

				// show old message
				for (i = 0; i < Reply->Data.NumLines; i++) {
					sprintf(string, ST_RMAILQUOTELIST , i+1, &Reply->Data.MsgTxt[  Reply->Data.Offsets[i]  ]);
					string[84] = 0;
					rputs(string);
				}
				rputs(ST_RMAILQUOTECHOOSE);
				od_input_str(string, 2, '0', '9');
				FirstLine = atoi(string);
				if (FirstLine < 1 || FirstLine > Reply->Data.NumLines) {
					continue;
				}

				strcpy(String, &Reply->Data.MsgTxt[ Reply->Data.Offsets[ FirstLine-1 ] ]);

				strcpy(string, ST_RMAILQUOTEBRACKET);
				strcat(string, &Reply->Data.MsgTxt[ Reply->Data.Offsets[ FirstLine-1 ]]);
				string[78] = 0;
				strcpy(String, string);

				rputs(ST_LONGSPACES);
				rputs("|09");
				rputs(String);
				rputs("\n");
				rputs(ST_MAILENTERCOLOR);

				NextString[0]=0;
				return 0;
			}
			else if (key == '/') {
				rputs(ST_LONGSPACES);
				rputs(ST_MAILENTERCOLOR);
				rputs("/");
				String[cur_char]= '/';
				cur_char++;
			}
		}
		else {
			String[cur_char]=ch;
			cur_char++;
			od_putch(ch);
		}
	}

	(void)LastLine;
	return 0;
}


void Reply_Message(struct Message *Reply)
{
	struct Message Message;
	FILE *fp;

	_INT16 player_num, result, NumLines = 0, CurChar = 0, i, FirstLine, LastLine;
	_INT16 Quoted = FALSE;
	char string[128], JustLen = 78;
	char Line1[128], Line2[128], key, OldLine[128];
	BOOL MakePublic, GlobalPost = FALSE;
	_INT16 WhichVillage = 0;

	// malloc msg
	Message.Data.MsgTxt = malloc(4000);
	CheckMem(Message.Data.MsgTxt);
	Message.Data.MsgTxt[0] = 0;

	// Set up header
	Message.ToClanID[0] = Reply->FromClanID[0];
	Message.ToClanID[1] = Reply->FromClanID[1];
	Message.FromClanID[0] =  PClan->ClanID[0];
	Message.FromClanID[1] =  PClan->ClanID[1];
	strcpy(Message.szFromName, PClan->szName);
	strcpy(Message.szDate, System.szTodaysDate);
	strcpy(Message.szFromVillageName, Village.Data->szName);

	Message.BBSIDFrom = Config->BBSID;

	Message.Flags = 0;

	Message.MessageType = Reply->MessageType;

	if (Reply->MessageType == MT_ALLIANCE) {
		Message.AllianceID = Reply->AllianceID;
		strcpy(Message.szAllyName, Reply->szAllyName);
	}

	if (Reply->Flags & MF_GLOBAL) {
		// global message
		GlobalPost = TRUE;
		WhichVillage = Reply->BBSIDFrom;

		Message.Flags |= MF_GLOBAL;

		// directed at one BBS only
		if (Reply->Flags & MF_ONEVILLONLY)
			Message.Flags |= MF_ONEVILLONLY;
	}

	// ask to quote
	if (YesNo(ST_RMAILQUOTE) == YES) {
		rputs(ST_RMAILQFIRSTLINE);
		od_input_str(string, 2, '0', '9');

		FirstLine = atoi(string);

		if ((FirstLine < 1 || FirstLine > Reply->Data.NumLines) == FALSE || FirstLine == 0) {
			if (FirstLine == 0) {
				rputs(ST_RMAILQUOTEALL);
				FirstLine = 1;
				LastLine = Reply->Data.NumLines;
			}
			else {
				rputs(ST_RMAILLASTLINE);
				od_input_str(string, 2, '0', '9');
				LastLine = atoi(string);

				if (LastLine > Reply->Data.NumLines)
					LastLine = Reply->Data.NumLines;
			}

			if ((LastLine < 1 || LastLine > Reply->Data.NumLines || LastLine < FirstLine) == FALSE) {
				if (FirstLine != LastLine) {
					for (i = FirstLine;  i <= LastLine; i++) {
						Message.Data.Offsets[NumLines] = CurChar;
						strcpy(&Message.Data.MsgTxt[CurChar], ST_RMAILQUOTEBRACKET);
						strcat(&Message.Data.MsgTxt[CurChar], &Reply->Data.MsgTxt[ Reply->Data.Offsets[i-1]]);
						Message.Data.MsgTxt[CurChar+78] = 0;

						if (CurChar >= 4000)
							rputs("Mem error in mail\n%P");

						CurChar += (strlen(&Message.Data.MsgTxt[CurChar]) + 1);

						NumLines++;
					}
				}
				else {
					Message.Data.Offsets[NumLines] = CurChar;
					strcpy(&Message.Data.MsgTxt[CurChar], ST_RMAILQUOTEBRACKET);
					strcat(&Message.Data.MsgTxt[CurChar], &Reply->Data.MsgTxt[ Reply->Data.Offsets[FirstLine-1]]);
					Message.Data.MsgTxt[CurChar+78] = 0;

					CurChar += (strlen(&Message.Data.MsgTxt[CurChar]) + 1);

					if (CurChar >= 4000)
						rputs("Mem error in mail\n");

					NumLines++;
				}
				Quoted = TRUE;
			}
		}
	}

	Line1[0] = 0;
	Line2[0] = 0;

	rputs(ST_RMAILHEADER);
	rputs(ST_LONGDIVIDER);

	strcpy(OldLine, "");

	if (Quoted) {
		rputs(ST_MAILENTERCOLOR);
		for (i = 0; i < NumLines; i++) {
			rputs(&Message.Data.MsgTxt[ Message.Data.Offsets[i] ]);
			rputs(ST_MAILENTERCOLOR);
			rputs("\n");

			strcpy(OldLine, &Message.Data.MsgTxt[ Message.Data.Offsets[i] ]);
		}
	}

	rputs(ST_MAILENTERCOLOR);

	for (;;) {
		result = QInputStr(Line1, Line2, &JustLen, Reply, NumLines);

		// if no swearing allowed, convert it :)
		// if (GameInfo.NoSwearing)
		//     RemoveSwear(Line1);

		if (result == 1) {  // save
			if (NumLines == 0) {
				rputs(ST_MAILNOTEXT);
				continue;
			}

			/* ask if public post or not */
			if (Reply->MessageType != MT_PUBLIC)
				MakePublic = NoYes(ST_MAILPUBLICQ);
			else
				MakePublic = YesNo(ST_MAILPUBLICQ);

			if (MakePublic) {
				Message.MessageType = MT_PUBLIC;

				if (!(GlobalPost && (Message.Flags & MF_ONEVILLONLY))) {
					Message.PublicMsgIndex = Village.Data->PublicMsgIndex;
					Village.Data->PublicMsgIndex++;
				}

				// make public but limit it depending on original message
				if (GlobalPost && (Reply->Flags & MF_ONEVILLONLY) == FALSE)
					WhichVillage = -1;
			}
			else {
				Message.PublicMsgIndex = 0;
				Message.MessageType = MT_PRIVATE;
			}

			rputs(ST_MAILSAVED);
			break;
		}
		else if (result == -1) {  // abort
			rputs(ST_MAILABORTED);

			free(Message.Data.MsgTxt);
			return;
		}
		else if (result == -2) {  // start over
			rputs(ST_MAILSTARTOVER);
			NumLines = 0;
			CurChar = 0;
			Line1[0] = 0;
			Line2[0] = 0;

			continue;
		}
		else if (result == 3) { // list
			if (NumLines == 0) {
				rputs(ST_MAILNOTEXTYET);
				continue;
			}
			for (i = 0; i < NumLines; i++) {
				sprintf(string, ST_MAILLIST, i+1, &Message.Data.MsgTxt[ Message.Data.Offsets[i] ]);
				string[84] = 0;
				strcat(string, "\n");
				strcat(string, ST_MAILENTERCOLOR);
				rputs(string);
			}
			continue;
		}
		else if (result == 4) { // backspace
			if (NumLines == 0)
				continue;       // can't go back any more

			// otherwise
			rputs("\n|03Backing up a line\n");

			// else continue
			NumLines--;
			CurChar -= (strlen(OldLine) + 1);

			strcpy(Line1, OldLine);

			if (NumLines == 0)
				OldLine[0] = 0;
			else
				strcpy(OldLine, &Message.Data.MsgTxt[Message.Data.Offsets[NumLines-1]]);

			Line2[0] = 0;
			continue;
		}

		// else continue
		Message.Data.Offsets[NumLines] = CurChar;
		strcpy(&Message.Data.MsgTxt[CurChar], Line1);
		CurChar += (strlen(Line1) + 1);

		strcpy(OldLine, Line1);
		strcpy(Line1, Line2);
		NumLines++;

		if (NumLines == 40) {
			rputs(ST_MAIL40LINES);
			break;
		}
	}
	Message.Data.Length = CurChar;
	Message.Data.NumLines = NumLines;

	// save message
	if ((GlobalPost && WhichVillage == -1) || GlobalPost == FALSE) {
		fp = _fsopen(ST_MSJFILE, "a+b", SH_DENYRW);
		if (!fp) {
			rputs(ST_NOMSJFILE);
			free(Message.Data.MsgTxt);
			return;
		}
		EncryptWrite(&Message, sizeof(struct Message), fp, XOR_MSG);
		EncryptWrite(Message.Data.MsgTxt, Message.Data.Length, fp, XOR_MSG);
		fclose(fp);
	}

	if (GlobalPost)
		SendMsj(&Message, WhichVillage);

	free(Message.Data.MsgTxt);
	(void)key;
	(void)player_num;
}


// ------------------------------------------------------------------------- //
BOOL Mail_Read(void)
{
	FILE *fp;
	long CurOffset, CurMsgOffset;
	_INT16 iTemp, CurLine;
	struct Message Message;
	BOOL AllianceFound, NewMail = FALSE, ReplyingToAlly = FALSE, WillAlly;
	char szString[128], key = 0;

	rputs(ST_RMAILCHECKING);

	CurOffset = 0;

	for (;;) {
		fp = _fsopen(ST_MSJFILE, "r+b", SH_DENYWR);
		if (!fp) return FALSE;

		fseek(fp, CurOffset, SEEK_SET);

		CurMsgOffset = ftell(fp);
		if (!EncryptRead(&Message, sizeof(Message), fp, XOR_MSG)) {
			fclose(fp);
			break;
		}

		CurOffset = ftell(fp);

		// Skip message if not this clan's || it's public and it has been read ||
		//    it has been deleted
		if ((Message.MessageType == MT_PRIVATE && ((Message.ToClanID[0] != PClan->ClanID[0]) || (Message.ToClanID[1] != PClan->ClanID[1]))) ||
				(Message.MessageType == MT_PUBLIC && PClan->PublicMsgIndex >= Message.PublicMsgIndex) ||
				(Message.Flags & MF_DELETED)) {
			fclose(fp);
			CurOffset += Message.Data.Length;
			continue;
		}

		// if an alliesonly msg, see if user is in that alliance, if not, skip
		if (Message.MessageType == MT_ALLIANCE) {
			AllianceFound = FALSE;
			for (iTemp = 0; !AllianceFound && (iTemp < MAX_ALLIES); iTemp++) {
				if (PClan->Alliances[iTemp] == Message.AllianceID)
					AllianceFound = TRUE;
			}

			// if alliance not found, skip message
			if (AllianceFound == FALSE) {
				fclose(fp);
				CurOffset += Message.Data.Length;
				continue;
			}
		}




		// Message IS readable, continue...

		if (PClan->PublicMsgIndex < Message.PublicMsgIndex)
			PClan->PublicMsgIndex = Message.PublicMsgIndex;

		NewMail = TRUE;

		// Display Msg header
		rputs(ST_LONGDIVIDER);

		if (Message.Flags & MF_NOFROM)
			sprintf(szString, ST_RMAILHEADER2,  Message.szDate);
		else
			sprintf(szString, ST_RMAILHEADER1,  Message.szFromName,  Message.szDate);
		rputs(szString);

		if (Message.MessageType == MT_PUBLIC) {
			sprintf(szString, ST_RMAILPUBLICPOST, Message.PublicMsgIndex);
			rputs(szString);
		}
		rputs("\n");

		if (Message.Flags == MF_ALLYREQ) {
			sprintf(szString, ST_RMAILSUBJALLY,  Message.szFromName, Message.szAllyName);
			rputs(szString);
		}
		if (Message.MessageType == MT_ALLIANCE) {
			sprintf(szString, "|0L Alliance: |0M%s.\n" , Message.szAllyName);
			rputs(szString);
		}

		if (Message.BBSIDFrom != Config->BBSID) {
			sprintf(szString, "|0L Origin  : |0M%s\n\r", Message.szFromVillageName);
			rputs(szString);
		}


		rputs(ST_LONGDIVIDER);


		// display message body
		Message.Data.MsgTxt = malloc(Message.Data.Length);
		CheckMem(Message.Data.MsgTxt);
		EncryptRead(Message.Data.MsgTxt, Message.Data.Length, fp, XOR_MSG);
		CurOffset = ftell(fp);
		fclose(fp);

		// display message entered
		for (CurLine = 0; CurLine < Message.Data.NumLines; CurLine++) {
			if (CurLine == 17) {
				rputs(ST_MORE);
				od_sleep(0);
				if (toupper(od_get_key(TRUE)) == 'N') {
					rputs(ST_RETURNSPACES);
					break;
				}
				rputs(ST_RETURNSPACES);
			}

			rputs(ST_RMAILCOLOR);
			rputs(&Message.Data.MsgTxt[ Message.Data.Offsets[CurLine] ]);
			rputs("\n|16");
		}

		// Get input from user
		if (Message.MessageType == MT_PUBLIC) {
			/* can't delete message */
			rputs(ST_RMAILFOOTER1);

			key = od_get_answer("QRS\r\n");
			if (key == '\r' || key == '\n')
				key = 'S';
		}
		else if (Message.Flags & MF_NOFROM) {
			// Can't reply
			key = 0;
			door_pause();
		}
		else if (Message.Flags & MF_ALLYREQ) {
			/* ally message, ask if he wants to ally */
			WillAlly = NoYes(ST_RMAILALLYQ);
			if (WillAlly == YES) {
				/* ally here */
				FormAlliance(Message.AllianceID);
			}

			ReplyingToAlly = YesNo(ST_RMAILREPLYQ);

			/* make generic message saying he said no */
			if (WillAlly == FALSE)
				sprintf(szString, ST_RMAILREJECTALLY, PClan->szName);
			else
				sprintf(szString, ST_RMAILAGREEALLY, PClan->szName);
			GenericReply(&Message, szString, FALSE);
		}
		else {
			/* regular message */
			rputs(ST_RMAILFOOTER2);

			key = od_get_answer("QRDS\r\n");
		}


		// Act on user input
		if ((!(Message.Flags & MF_ALLYREQ) && key == 'R') ||
				((Message.Flags & MF_ALLYREQ) && ReplyingToAlly == YES)) {
			if ((Message.Flags & MF_ALLYREQ) == FALSE)
				rputs(ST_RMAILREPLY);

			// reply to message here
			Reply_Message(&Message);

			// can't delete if public post
			if (Message.MessageType == MT_PUBLIC) {
				free(Message.Data.MsgTxt);
				continue;
			}

			// ask if you want to delete it
			if ((Message.Flags & MF_ALLYREQ) == FALSE && YesNo(ST_RMAILDELETEQ) == NO) {
				// doesn't want to
				free(Message.Data.MsgTxt);
				continue;
			}

			// delete message
			Message.Flags |= MF_DELETED;

			fp = _fsopen(ST_MSJFILE, "r+b", SH_DENYWR);
			if (!fp) {
				rputs(ST_NOMSJFILE);
				free(Message.Data.MsgTxt);
				return NewMail;
			}
			fseek(fp, CurMsgOffset, SEEK_SET);

			EncryptWrite(&Message, sizeof(Message), fp, XOR_MSG);
			fclose(fp);
		}
		if (key == 'D' || key == '\r' || key == '\n' || (Message.Flags & MF_ALLYREQ) || (Message.Flags & MF_NOFROM)) {
			if ((Message.Flags & MF_ALLYREQ) == FALSE && (Message.Flags & MF_NOFROM) == FALSE)
				rputs(ST_RMAILDELETE);
			else
				rputs(ST_RMAILDELETING);

			// delete message
			Message.Flags |= MF_DELETED;

			fp = _fsopen(ST_MSJFILE, "r+b", SH_DENYWR);
			if (!fp) {
				rputs(ST_NOMSJFILE);
				free(Message.Data.MsgTxt);
				return NewMail;
			}
			fseek(fp, CurMsgOffset, SEEK_SET);

			EncryptWrite(&Message, sizeof(Message), fp, XOR_MSG);
			fclose(fp);
		}
		if (key == 'S')
			rputs(ST_RMAILSKIP);

		if (key == 'Q') {
			rputs(ST_RMAILQUIT);
			free(Message.Data.MsgTxt);
			return NewMail;
		}

		free(Message.Data.MsgTxt);


	} /* for (;;) */

	return NewMail;

}
_INT16 InputStr(char *String, char *NextString, char *JustLen, _INT16 CurLine)
{
	_INT16 cur_char = 0, i;
	unsigned char ch, key;

	rputs(ST_MAILENTERCOLOR);
	rputs(String);
	cur_char = strlen(String);

	for (;;) {
		if (cur_char == (*JustLen)) {
			String[cur_char] = 0;

			// break off last String
			for (i = strlen(String); i>0; i--) {
				if (String[i] == ' ') {
					i++;
					break;
				}
			}

			if (i > ((*JustLen)/2)) {
				strcpy(NextString, &String[i]);
				String[i] = 0;

				for (; i < cur_char; i++)
					rputs("\b \b");

				rputs("\n");
				return 0;
			}
			else {
				rputs("\n");
				NextString[0] = 0;
				return 0;
			}
		}

		ch = od_get_key(FALSE);
		if (ch=='\b') {
			if (cur_char>0) {
				cur_char--;
				rputs("\b \b");
			}
			else if (cur_char == 0 && CurLine > 0)
				return 4;
		}
		else if ((isalnum(ch) || ispunct(ch)) && (String[cur_char-1] == '%'  || String[cur_char-1]=='&') && cur_char > 0)
			continue;
		else if ((isdigit(ch) || toupper(ch) == 'S') && String[cur_char-1] == '@' && cur_char > 0)
			continue;
		else if (ch== 13) {
			rputs("\n");
			String[cur_char]=0;
			NextString[0]=0;
			break;
		}
		else if (ch == '\t' && cur_char < 73) {
			rputs("     ");
			strcat(&String[cur_char], "     ");
			cur_char += 5;
			continue;
		}
		else if (ch == 25) { // CTRL-Y
			rputs("\r                                                                     \r");
			rputs(ST_MAILENTERCOLOR);
			cur_char = 0;
			String[0] = 0;
			NextString[0] = 0;

			continue;
		}
		else if (iscntrl(ch))
			continue;
		else if (ch == '/' && cur_char == 0) {
			rputs(ST_INPUTSTRCOMMAND);

			key = toupper(od_get_answer("SACLR?/\r\n"));

			if (key == '?') {
				rputs(ST_INPUTSTRHELP);
				key = toupper(od_get_answer("SACLR/\r\n"));
			}

			if (key == 'C' || key == '\r' || key == '\n') {
				rputs(ST_LONGSPACES);
				rputs(ST_MAILENTERCOLOR);
				continue;
			}
			else if (key == 'R') {
				rputs(ST_LONGSPACES);
				rputs(ST_INPUTSTRSTARTOVER);
				rputs(STR_NOYES);
				rputs("|15");

				if (od_get_answer("YN\r\n") == 'Y') {
					rputs(ST_YES);
					rputs("\n");
					return -2;
				}
				rputs(ST_LONGSPACES);
				rputs(ST_MAILENTERCOLOR);
			}
			else if (key == 'S') {
				rputs(ST_LONGSPACES);
				rputs(ST_INPUTSTRSAVE);
				rputs(STR_YESNO);
				rputs("|15");

				if (od_get_answer("YN\r\n") == 'N') {
					rputs(ST_LONGSPACES);
					rputs(ST_MAILENTERCOLOR);
					continue;
				}

				rputs(ST_YES);
				rputs("\n");

				NextString[0] = 0;
				return 1;
			}
			else if (key == 'A') {
				rputs(ST_LONGSPACES);
				rputs(ST_INPUTSTRABORT);
				rputs(STR_NOYES);
				rputs("|15");

				if (od_get_answer("YN\r\n") == 'Y') {
					rputs(ST_YES);
					rputs("\n");
					return -1;
				}
				rputs(ST_LONGSPACES);
				rputs(ST_MAILENTERCOLOR);
			}
			else if (key == 'L') {
				rputs("\n");
				NextString[0]=0;
				String[0]=0;
				return 3;
			}
			else if (key == '/') {
				rputs(ST_LONGSPACES);
				rputs(ST_MAILENTERCOLOR);
				rputs("/");
				String[cur_char]= '/';
				cur_char++;
			}
		}
		else {
			String[cur_char]=ch;
			cur_char++;
			od_putch(ch);
		}
	}

	return 0;
}


void Msg_Create(_INT16 ToClanID[2], BOOL MessageType, BOOL AllyReq, _INT16 AllianceID,
				char *szAllyName, BOOL GlobalPost, _INT16 WhichVillage)
/*
 * Allows user to enter a message.
 *
 * PRE: ToClanID is the ClanID who will receive the message if it is
 *        a private message.  Otherwise it is ignored
 *      MessageType determines what type of message this is.
 *      AllyReq is TRUE if this is an alliance request.
 *      AllianceID is the alliance which is sending this message if it
 *        is an MT_ALLIANCE type of message OR it is an AllyReq
 *      szAllyName is the name of the alliance sending the message if it
 *        is an AllyReq or MT_ALLIANCE type.
 *      GlobalPost is TRUE if it is an IBBS message.
 *      WhichVillage is the village to receive the message if it is a
 *        GlobalPost.  Set to ALL_VILLAGES if none specific.
 */
{
	struct Message Message;
	FILE *fp;
	_INT16 result, NumLines = 0, CurChar = 0, i;
	char string[128], JustLen = 78;
	char Line1[128], Line2[128], OldLine[128];

	if (MessageType == MT_PRIVATE) {
		if (GetClanID(ToClanID, FALSE, FALSE, -1, FALSE) == FALSE) {
			return;
		}
	}

	/* malloc msg */
	Message.Data.MsgTxt = malloc(4000);
	CheckMem(Message.Data.MsgTxt);
	Message.Data.MsgTxt[0] = 0;

	// Set up header
	Message.ToClanID[0] = ToClanID[0];
	Message.ToClanID[1] = ToClanID[1];
	Message.FromClanID[0] = PClan->ClanID[0];
	Message.FromClanID[1] = PClan->ClanID[1];
	strcpy(Message.szFromName, PClan->szName);
	strcpy(Message.szDate, System.szTodaysDate);
	strcpy(Message.szFromVillageName, Village.Data->szName);

	Message.BBSIDFrom = Config->BBSID;

	Message.Flags = 0;

	Message.MessageType = MessageType;

	if (MessageType == MT_ALLIANCE) {
		Message.AllianceID = AllianceID;
		strcpy(Message.szAllyName, szAllyName);
	}

	if (AllyReq) {
		Message.Flags |= MF_ALLYREQ;
		strcpy(Message.szAllyName, szAllyName);
		Message.AllianceID = AllianceID;
	}

	if (GlobalPost)
		Message.Flags |= MF_GLOBAL;

	// directed at one BBS only
	if (WhichVillage != ALL_VILLAGES && GlobalPost && MessageType == MT_PUBLIC)
		Message.Flags |= MF_ONEVILLONLY;

	rputs(ST_MAILHEADER);
	rputs(ST_LONGDIVIDER);

	Line1[0] = 0;
	Line2[0] = 0;
	OldLine[0] = 0;
	for (;;) {
		rputs(ST_MAILENTERCOLOR);
		result = InputStr(Line1, Line2, &JustLen, NumLines);

		// if no swearing allowed, convert it :)
		//if (GameInfo.NoSwearing)
		//    RemoveSwear(Line1);

		if (result == 1) {  // save
			if (NumLines == 0) {
				rputs(ST_MAILNOTEXT);
				continue;
			}

			/* ask if public post or not */
			if (Message.MessageType == MT_PUBLIC) {
				/* public post */
				if (!(GlobalPost && (Message.Flags & MF_ONEVILLONLY))) {
					Message.PublicMsgIndex = Village.Data->PublicMsgIndex;
					Village.Data->PublicMsgIndex++;
				}
			}
			else
				Message.PublicMsgIndex = 0;

			rputs(ST_MAILSAVED);
			break;
		}
		else if (result == -1) {  // abort
			rputs(ST_MAILABORTED);

			free(Message.Data.MsgTxt);
			return;
		}
		else if (result == -2) {  // start over
			rputs(ST_MAILSTARTOVER);
			NumLines = 0;
			CurChar = 0;
			Line1[0] = 0;
			Line2[0] = 0;

			continue;
		}
		else if (result == 3) { // list
			if (NumLines == 0) {
				rputs(ST_MAILNOTEXTYET);
				continue;
			}
			for (i = 0; i < NumLines; i++) {
				sprintf(string, ST_MAILLIST, i+1, &Message.Data.MsgTxt[ Message.Data.Offsets[i] ]);
				string[84] = 0;
				strcat(string, "\n");
				strcat(string, ST_MAILENTERCOLOR);
				rputs(string);
			}
			continue;
		}
		else if (result == 4) { // backspace
			if (NumLines == 0)
				continue;       // can't go back any more

			// otherwise
			rputs("\n|03Backing up a line\n");

			// else continue
			NumLines--;
			CurChar -= (strlen(OldLine) + 1);

			strcpy(Line1, OldLine);

			if (NumLines == 0)
				OldLine[0] = 0;
			else
				strcpy(OldLine, &Message.Data.MsgTxt[Message.Data.Offsets[NumLines-1]]);

			Line2[0] = 0;
			continue;
		}


		// else continue
		Message.Data.Offsets[NumLines] = CurChar;
		strcpy(&Message.Data.MsgTxt[CurChar], Line1);
		CurChar += (strlen(Line1) + 1);

		strcpy(OldLine, Line1);
		strcpy(Line1, Line2);
		NumLines++;

		if (NumLines == 40) {
			rputs(ST_MAIL40LINES);
			break;
		}
	}
	Message.Data.Length = CurChar;
	Message.Data.NumLines = NumLines;


	// if local post OR globalpost to ALL villages, do this
	if ((GlobalPost && WhichVillage == ALL_VILLAGES) || GlobalPost == FALSE) {
		fp = _fsopen(ST_MSJFILE, "ab", SH_DENYRW);
		if (!fp) {
			rputs(ST_NOMSJFILE);
			return;
		}
		EncryptWrite(&Message, sizeof(struct Message), fp, XOR_MSG);
		EncryptWrite(Message.Data.MsgTxt, Message.Data.Length, fp, XOR_MSG);
		fclose(fp);
	}

	if (GlobalPost)
		SendMsj(&Message, WhichVillage);

	free(Message.Data.MsgTxt);
}


void Mail_Write(_INT16 MessageType)
{
	_INT16 DummyID[2];

	DummyID[0] = -1;
	DummyID[1] = -1;

	Msg_Create(DummyID, MessageType, FALSE, -1, NULL, FALSE, -1);
}


// ------------------------------------------------------------------------- //

void Mail_Maint(void)
{
	FILE *OldMessage, *NewMessage;
	struct Message Message;

	DisplayStr("* Mail_Maint()\n");

	OldMessage = _fsopen("clans.msj", "rb", SH_DENYRW);
	if (OldMessage) {   // MSJ file exists, so go on
		NewMessage = _fsopen("tmp.$$$", "wb", SH_DENYRW);
		if (!NewMessage) {
			DisplayStr("Error opening temp file\n");
			sleep(3);
			return;
		}

		for (;;) {
			if (!EncryptRead(&Message, sizeof(struct Message), OldMessage, XOR_MSG))
				break;

			if (Message.Flags & MF_DELETED)
				//FIXME:      DaysBetween(Message.Date, GameInfo.TheDate) > 7)
			{
				// deleted or else a week old, so skip it
				fseek(OldMessage, Message.Data.Length, SEEK_CUR);
			}
			else {
				/* write it, not read yet */
				Message.Data.MsgTxt = malloc(Message.Data.Length);
				CheckMem(Message.Data.MsgTxt);

				// write it to new file
				EncryptRead(Message.Data.MsgTxt, Message.Data.Length, OldMessage, XOR_MSG);

				EncryptWrite(&Message, sizeof(struct Message), NewMessage, XOR_MSG);
				EncryptWrite(Message.Data.MsgTxt, Message.Data.Length, NewMessage, XOR_MSG);

				free(Message.Data.MsgTxt);
			}
		}

		fclose(NewMessage);
		fclose(OldMessage);

		// delete old, and rename new
		unlink("clans.msj");
		rename("tmp.$$$", "clans.msj");
	}
}

void Mail_RequestAlliance(struct Alliance *Alliance)
{
	_INT16 iTemp, NumAlliances = 0;
	_INT16 ClanId[2], CurClan;
	char szToName[25], szFileName[40];
	FILE *fpPlayerFile;
	struct clan *TmpClan;

	// see if this alliance has too many members
	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
		if (Alliance->Member[iTemp][0] != -1)
			NumAlliances++;

	if (NumAlliances == MAX_ALLIANCEMEMBERS) {
		rputs(ST_ALLIANCEATMAX);
		return;
	}

	/* find player as you would if writing mail. */
	if (GetClanID(ClanId, FALSE, FALSE, Alliance->ID, FALSE) == FALSE) {
		rputs("\n\n");
		return;
	}

	/* ask if he wants to invite this guy for sure */
	if (NoYes(ST_ALLIANCESURE) == NO) {
		rputs(ST_ABORTED);
		return;
	}

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);
	GetClan(ClanId, TmpClan);

	// see if in this alliance already
	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
		if (TmpClan->Alliances[iTemp] == Alliance->ID)
			break;

	if (iTemp != MAX_ALLIES) {
		rputs(ST_ALLIANCEALREADY);
		FreeClan(TmpClan);
		return;
	}

	/* see if he has too many allies */
	NumAlliances = 0;
	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
		if (TmpClan->Alliances[iTemp] != -1)
			NumAlliances++;

	if (NumAlliances == MAX_ALLIES) {
		rputs(ST_ALLIANCEOTHERMAX);
		FreeClan(TmpClan);
		return;
	}

	/* if so, set up message but with ALLY flag */
	rputs(ST_ALLIANCELETTER);
	MyWriteMessage2(ClanId, FALSE, TRUE, Alliance->ID, Alliance->szName, FALSE, -1);

	FreeClan(TmpClan);
	(void)szFileName;
	(void)fpPlayerFile;
	(void)CurClan;
	(void)szToName;
}

void Mail_WriteToAllies(struct Alliance *Alliance)
{
	_INT16 ClanId[2];

	ClanId[0] = -1;
	ClanId[1] = -1;
	MyWriteMessage2(ClanId, TRUE, FALSE, Alliance->ID, Alliance->szName, FALSE, -1);
}





void SendMsj(struct Message *Message, _INT16 WhichVillage)
{
	// if WhichVillage == -1, do a for loop and send packet to all BBSes

	struct Packet Packet;
	_INT16 ClanID[2], CurBBS;
	FILE *fp;


	Packet.Active = TRUE;
	Packet.BBSIDFrom = IBBS.Data->BBSID;
	Packet.PacketType = PT_MSJ;
	strcpy(Packet.szDate, System.szTodaysDate);
	Packet.PacketLength = sizeof(struct Message) + Message->Data.Length;
	strcpy(Packet.GameID, Game.Data->GameID);

	if (WhichVillage != -1) {
		Packet.BBSIDTo = WhichVillage;

		fp = fopen("tmp.$$$", "wb");
		if (!fp)    return;

		/* write packet */
		EncryptWrite(&Packet, sizeof(struct Packet), fp, XOR_PACKET);

		EncryptWrite(Message, sizeof(struct Message), fp, XOR_PACKET);
		EncryptWrite(Message->Data.MsgTxt, Message->Data.Length, fp, XOR_PACKET);

		fclose(fp);

		// send packet to BBS
		IBBS_SendPacketFile(Packet.BBSIDTo, "tmp.$$$");
		unlink("tmp.$$$");
	}
	else {
		// go through ALL BBSes and send this to them all

		for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
			if (IBBS.Data->Nodes[CurBBS].Active == FALSE ||
					CurBBS+1 == IBBS.Data->BBSID)
				continue;

			Packet.BBSIDTo = CurBBS+1;

			fp = fopen("tmp.$$$", "wb");
			if (!fp)    return;

			/* write packet */
			EncryptWrite(&Packet, sizeof(struct Packet), fp, XOR_PACKET);

			EncryptWrite(Message, sizeof(struct Message), fp, XOR_PACKET);
			EncryptWrite(Message->Data.MsgTxt, Message->Data.Length, fp, XOR_PACKET);

			fclose(fp);

			// send packet to BBS
			IBBS_SendPacketFile(Packet.BBSIDTo, "tmp.$$$");
			unlink("tmp.$$$");
		}
	}
	(void)ClanID;
}

void PostMsj(struct Message *Message)
{
	// open our .msj file, append this and update MsgPublicIndex if this
	// is a public post (i.e. MsgPublicIndex != 0)

	// if MsgPublicIndex == 0, leave the msj alone since it is a private post

	FILE *fp;

	fp = fopen(ST_MSJFILE, "ab");

	if (fp) {
		// if public post (MsgPublicIndex != 0), update msgPublicIndex
		if (Message->MessageType == MT_PUBLIC)
			Message->PublicMsgIndex = Village.Data->PublicMsgIndex++;

		EncryptWrite(Message, sizeof(struct Message), fp, XOR_MSG);
		EncryptWrite(Message->Data.MsgTxt, Message->Data.Length, fp, XOR_MSG);
		fclose(fp);
	}
}

void GlobalMsgPost(void)
{
	// choose what type of post:
	//
	//  public (everyone sees)
	//  private (direct)

	char cKey, *apszVillageNames[MAX_IBBSNODES], *apszUserNames[50],
	*pszChoices[2] = { "1. Public", "2. Private" };
	_INT16 iTemp, NumVillages, WhichVillage, NumClans;
	_INT16 ClanIDs[50][2], WhichClan, ClanID[2], PostType;
	unsigned char VillageIndex[MAX_IBBSNODES];

	GetStringChoice(pszChoices, 2, "|0SWhat type of post?\n|0E> |0F",
					&PostType, TRUE, DT_LONG, TRUE);

	if (PostType == -1)
		return;

	// load up village names
	NumVillages = 0;
	for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
		apszVillageNames[iTemp] = NULL;

		// skip if our village
		if (iTemp+1 == IBBS.Data->BBSID)
			continue;

		if (IBBS.Data->Nodes[iTemp].Active) {
			apszVillageNames[NumVillages] = IBBS.Data->Nodes[iTemp].Info.pszVillageName;
			VillageIndex[NumVillages] = iTemp+1;
			NumVillages++;
		}
	}

	if (PostType == 0) {
		// public
		if (YesNo("|0SWrite to a specific village?  (No=All villages)") == YES) {
			// specific village, choose one
			GetStringChoice(apszVillageNames, NumVillages,
							"|0SEnter the name of the village\n|0G> |0F",
							&WhichVillage, TRUE, DT_LONG, TRUE);

			if (WhichVillage == -1) {
				rputs(ST_ABORTED);
				return;
			}
			WhichVillage = VillageIndex[ WhichVillage ];
		}
		else {
			// write to all villages
			WhichVillage = -1;
		}

		ClanID[0] = -1;
		ClanID[1] = -1;

		MyWriteMessage2(ClanID, TRUE, FALSE, -1, "", TRUE, WhichVillage);
	}
	else {
		// private

		// specific village, choose one
		GetStringChoice(apszVillageNames, NumVillages,
						"|0SEnter the name of the village\n|0G> |0F",
						&WhichVillage, TRUE, DT_LONG, TRUE);

		if (WhichVillage == -1) {
			rputs(ST_ABORTED);
			return;
		}
		WhichVillage = VillageIndex[ WhichVillage ];

		// load up names of characters from that village
		GetUserNames(apszUserNames, WhichVillage, &NumClans, ClanIDs);

		// choose one
		GetStringChoice(apszUserNames, NumClans,
						"|0SEnter the name of the clan to write to\n|0G> |0F",
						&WhichClan, TRUE, DT_WIDE, TRUE);

		if (WhichClan == -1) {
			rputs(ST_ABORTED);
			for (iTemp = 0; iTemp < 50; iTemp++) {
				if (apszUserNames[iTemp])
					free(apszUserNames[iTemp]);
			}
			return;
		}

		ClanID[0] = ClanIDs[ WhichClan ][0];
		ClanID[1] = ClanIDs[ WhichClan ][1];

		for (iTemp = 0; iTemp < 50; iTemp++) {
			if (apszUserNames[iTemp])
				free(apszUserNames[iTemp]);
		}

		MyWriteMessage2(ClanID, FALSE, FALSE, -1, "", TRUE, WhichVillage);
	}
	(void)cKey;
}
