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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __unix__
# include <share.h>
# include <dos.h>
#endif
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include <OpenDoor.h>

#include "alliance.h"
#include "alliancem.h"
#include "door.h"
#include "fight.h"
#include "game.h"
#include "ibbs.h"
#include "input.h"
#include "language.h"
#include "mail.h"
#include "mstrings.h"
#include "myopen.h"
#include "packet.h"
#include "readcfg.h"
#include "structs.h"
#include "system.h"
#include "user.h"
#include "video.h"
#include "village.h"

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

#define MSGTXT_SZ	4000U

static int16_t InputStr(char *String, char *NextString, char *JustLen, int16_t CurLine);
static void SendMsj(struct Message *Message, int16_t WhichVillage);

// ------------------------------------------------------------------------- //

static void GetUserNames(const char *apszUserNames[50], int16_t WhichVillage, int16_t *NumUsers,
				  int16_t ClanIDs[50][2])
{
	FILE *fpUList;
	struct UserInfo User;
	int16_t CurUser;

	// set all to NULLs
	for (CurUser = 0; CurUser < 50; CurUser++)
		apszUserNames[CurUser] = NULL;
	*NumUsers = 0;

	// open user file
	fpUList = _fsopen("userlist.dat", "rb", _SH_DENYWR);
	if (!fpUList)    // no user list at all
		return;


	// go through, pick up those users which hail from this village

	for (CurUser = 0;;) {
		notEncryptRead_s(UserInfo, &User, fpUList, XOR_ULIST)
			break;

		// user from that village?
		if (User.ClanID[0] == WhichVillage) {
			// found one from there
			apszUserNames[CurUser] = DupeStr(User.szName);
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

static void GenericReply(struct Message *Reply, char *szReply, bool AllowReply)
{
	size_t stTemp;
	struct Message Message;
	FILE *fp;

	// malloc msg
	Message.Data.MsgTxt = malloc(MSGTXT_SZ);
	CheckMem(Message.Data.MsgTxt);

	Message.Data.MsgTxt[0] = 0;

	// strlcpy(Message.szFromName, GlobalPlayerClan->szName, sizeof(Message.szFromName));
	// fromname not used in generic msgs
	strlcpy(Message.szFromName, Reply->szFromName, sizeof(Message.szFromName));
	strlcpy(Message.szFromVillageName, Village.Data.szName, sizeof(Message.szFromVillageName));
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
	strlcpy(Message.szDate, System.szTodaysDate, sizeof(Message.szDate));

	if (AllowReply == false)
		Message.Flags = MF_NOFROM;

	Message.PublicMsgIndex = 0;
	Message.MessageType = MT_PRIVATE;

	strlcpy(Message.Data.MsgTxt, szReply, sizeof(Message.Data.MsgTxt));

	stTemp = strlen(szReply) + 1;
	if (stTemp > INT16_MAX)
		System_Error("Message Text too long");
	Message.Data.Length = (int16_t)stTemp;
	Message.Data.NumLines = 1;
	Message.Data.Offsets[0] = 0;
	Message.Data.Offsets[1] = 0;

	// save message
	fp = _fsopen(ST_MSJFILE, "a+b", _SH_DENYRW);
	if (!fp) {
		free(Message.Data.MsgTxt);
		DisplayStr(ST_NOMSJFILE);
		return;
	}
	EncryptWrite_s(Message, &Message, fp, XOR_MSG);
	CheckedEncryptWrite(Message.Data.MsgTxt, (size_t)Message.Data.Length, fp, XOR_MSG);
	fclose(fp);

	free(Message.Data.MsgTxt);
}


// ------------------------------------------------------------------------- //
void GenericMessage(char *szString, int16_t ToClanID[2], int16_t FromClanID[2], char *szFrom, bool AllowReply)
{
	struct Message Message;

	// if clan doesn't exist, return
	if (ClanExists(ToClanID) == false)  return;

	/* write generic message -- set it up as a "reply" */
	Message.ToClanID[0] = FromClanID[0];
	Message.ToClanID[1] = FromClanID[1];

	Message.FromClanID[0] = ToClanID[0];
	Message.FromClanID[1] = ToClanID[1];

	strlcpy(Message.szFromName, szFrom, sizeof(Message.szFromName));
	strlcpy(Message.szDate, System.szTodaysDate, sizeof(Message.szDate));
	strlcpy(Message.szFromVillageName, Village.Data.szName, sizeof(Message.szFromVillageName));
	Message.Flags = 0;
	Message.MessageType = MT_PRIVATE;

	GenericReply(&Message, szString, AllowReply);
}


// ------------------------------------------------------------------------- //
void MyWriteMessage2(int16_t ClanID[2], bool ToAll,
					 bool AllyReq, int16_t AllianceID, char *szAllyName,
					 bool GlobalPost, int16_t WhichVillage)
{
	struct Message Message;
	FILE *fp;
	size_t CurChar = 0;
	int16_t result, NumLines = 0, i;
	char string[128], JustLen = 78;
	char Line1[128], Line2[128], OldLine[128];

	if (ToAll == false && ClanID[0] == -1) {
		if (GetClanID(ClanID, false, false, -1, false) == false) {
			return;
		}
	}

	/* malloc msg */
	Message.Data.MsgTxt = malloc(MSGTXT_SZ);
	CheckMem(Message.Data.MsgTxt);

	Message.Data.MsgTxt[0] = 0;

	Message.ToClanID[0] = ClanID[0];
	Message.ToClanID[1] = ClanID[1];
	strlcpy(Message.szFromName, PClan->szName, sizeof(Message.szFromName));
	strlcpy(Message.szFromVillageName, Village.Data.szName, sizeof(Message.szFromVillageName));
	Message.FromClanID[0] = PClan->ClanID[0];
	Message.FromClanID[1] = PClan->ClanID[1];

	Message.BBSIDFrom = Config.BBSID;

	strlcpy(Message.szDate, System.szTodaysDate, sizeof(Message.szDate));
	Message.Flags = 0;
	Message.MessageType = MT_PRIVATE;

	if (AllyReq) {
		Message.Flags |= MF_ALLYREQ;
		strlcpy(Message.szAllyName, szAllyName, sizeof(Message.szAllyName));
		Message.AllianceID = AllianceID;
	}
	else if (AllianceID != -1) {
		// not an alliance, but AllyID exists, so make this an alliance-only MSG
		Message.AllianceID = AllianceID;
		strlcpy(Message.szAllyName, szAllyName, sizeof(Message.szAllyName));
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
					Message.PublicMsgIndex = Village.Data.PublicMsgIndex;
					Village.Data.PublicMsgIndex++;
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
				snprintf(string, sizeof(string), ST_MAILLIST, (int)(i + 1), &Message.Data.MsgTxt[ Message.Data.Offsets[i] ]);
				string[84] = 0;
				strlcat(string, "\n", sizeof(string));
				strlcat(string, ST_MAILENTERCOLOR, sizeof(string));
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
			if (CurChar < 0)
				CurChar = 0;

			strlcpy(Line1, OldLine, sizeof(Line1));

			if (NumLines == 0)
				OldLine[0] = 0;
			else
				strlcpy(OldLine, &Message.Data.MsgTxt[Message.Data.Offsets[NumLines-1]], sizeof(OldLine));

			Line2[0] = 0;
			continue;
		}


		// else continue
		if (CurChar < 0)
			CurChar = 0;
		Message.Data.Offsets[NumLines] = (int16_t)CurChar;
		strlcpy(&Message.Data.MsgTxt[CurChar], Line1, (size_t)(MSGTXT_SZ - (unsigned)CurChar));
		CurChar += (strlen(Line1) + 1);

		strlcpy(OldLine, Line1, sizeof(OldLine));
		strlcpy(Line1, Line2, sizeof(Line1));
		NumLines++;

		if (NumLines == 40) {
			rputs(ST_MAIL40LINES);
			break;
		}
	}
	Message.Data.Length = (int16_t)CurChar;
	Message.Data.NumLines = NumLines;
	if (Message.Data.Length < 0)
		System_Error("Negative Message Length in MyWriteMessage2()");

	// save message

	// if local post OR globalpost to ALL villages, do this
	if ((GlobalPost && WhichVillage == -1) || GlobalPost == false) {
		fp = _fsopen(ST_MSJFILE, "ab", _SH_DENYRW);
		if (!fp) {
			free(Message.Data.MsgTxt);
			rputs(ST_NOMSJFILE);
			return;
		}
		EncryptWrite_s(Message, &Message, fp, XOR_MSG);
		CheckedEncryptWrite(Message.Data.MsgTxt, (size_t)Message.Data.Length, fp, XOR_MSG);
		fclose(fp);
	}

	if (GlobalPost)
		SendMsj(&Message, WhichVillage);

	free(Message.Data.MsgTxt);
}




// ------------------------------------------------------------------------- //
static int16_t QInputStr(char *String, char *NextString, char *JustLen, struct Message *Reply,
				 int CurLine)
{
	size_t cur_char = 0;
	int FirstLine, i;
	unsigned char ch, key;
	char string[128];

	rputs(ST_MAILENTERCOLOR);
	rputs(String);
	cur_char = strlen(String);
	if (cur_char > INT_MAX)
		System_Error("String too long in QInputStr()");

	for (;;) {
		if (cur_char == (*JustLen)) {
			String[cur_char] = 0;

			// break off last String
			for (i = (int)strlen(String); i>0; i--) {
				if (String[i] == ' ') {
					i++;
					break;
				}
			}

			if (i > ((*JustLen) / 2)) {
				strlcpy(NextString, &String[i], sizeof(NextString));
				String[i] = 0;

				for (; i < (int)cur_char; i++)
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

		ch = (unsigned char)od_get_key(true);
		if (ch == '\b') {
			if (cur_char>0) {
				cur_char--;
				rputs("\b \b");
			}
			else if (cur_char == 0 && CurLine > 0)
				return 4;
		}
		else if (cur_char > 0 && (isalnum(ch) || ispunct(ch)) && (String[cur_char - 1] == '%'  || String[cur_char - 1]=='&'))
			continue;
		else if (cur_char > 0 && (isdigit(ch) || toupper(ch) == 'S') && String[cur_char - 1] == '@')
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
		else if (iscntrl(ch) && ch != '\r' && ch != '\n')
			continue;
		else if (ch == '/' && cur_char == 0) {
			rputs(ST_INPUTSTRCOMMAND);

			key = toupper(od_get_answer("SACQLR?/\r\n")) & 0x7F;

			if (key == '?') {
				rputs(ST_INPUTSTRHELP2);
				key = toupper(od_get_answer("SACQLR/\r\n")) & 0x7F;
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
			else if (key == 'Q') {
				rputs(ST_LONGSPACES);
				rputs(ST_MAILENTERCOLOR);

				// show old message
				for (i = 0; i < Reply->Data.NumLines; i++) {
					snprintf(string, sizeof(string), ST_RMAILQUOTELIST , (int)(i + 1), &Reply->Data.MsgTxt[  Reply->Data.Offsets[i]  ]);
					string[84] = 0;
					rputs(string);
				}
				rputs(ST_RMAILQUOTECHOOSE);
				od_input_str(string, 2, '0', '9');
				FirstLine = atoi(string);
				if (FirstLine < 1 || FirstLine > Reply->Data.NumLines) {
					continue;
				}

				strlcpy(String, &Reply->Data.MsgTxt[ Reply->Data.Offsets[ FirstLine-1 ] ], sizeof(String));

				strlcpy(string, ST_RMAILQUOTEBRACKET, sizeof(string));
				strlcat(string, &Reply->Data.MsgTxt[ Reply->Data.Offsets[ FirstLine-1 ]], sizeof(string));
				string[78] = 0;
				strlcpy(String, string, sizeof(String));

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
			String[cur_char]=(char)ch;
			cur_char++;
			od_putch((char)ch);
		}
	}

	return 0;
}


static void Reply_Message(struct Message *Reply)
{
	struct Message Message;
	FILE *fp;

	size_t CurChar = 0;
	int result, NumLines = 0, i, FirstLine, LastLine;
	int16_t Quoted = false;
	char string[128], JustLen = 78;
	char Line1[128], Line2[128], OldLine[128];
	bool MakePublic, GlobalPost = false;
	int16_t WhichVillage = 0;

	// malloc msg
	Message.Data.MsgTxt = malloc(MSGTXT_SZ);
	CheckMem(Message.Data.MsgTxt);
	Message.Data.MsgTxt[0] = 0;

	// Set up header
	Message.ToClanID[0] = Reply->FromClanID[0];
	Message.ToClanID[1] = Reply->FromClanID[1];
	Message.FromClanID[0] =  PClan->ClanID[0];
	Message.FromClanID[1] =  PClan->ClanID[1];
	strlcpy(Message.szFromName, PClan->szName, sizeof(Message.szFromName));
	strlcpy(Message.szDate, System.szTodaysDate, sizeof(Message.szDate));
	strlcpy(Message.szFromVillageName, Village.Data.szName, sizeof(Message.szFromVillageName));

	Message.BBSIDFrom = Config.BBSID;

	Message.Flags = 0;

	Message.MessageType = Reply->MessageType;

	if (Reply->MessageType == MT_ALLIANCE) {
		Message.AllianceID = Reply->AllianceID;
		strlcpy(Message.szAllyName, Reply->szAllyName, sizeof(Message.szAllyName));
	}

	if (Reply->Flags & MF_GLOBAL) {
		// global message
		GlobalPost = true;
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

		if ((FirstLine < 1 || FirstLine > Reply->Data.NumLines) == false || FirstLine == 0) {
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

			if ((LastLine < 1 || LastLine > Reply->Data.NumLines || LastLine < FirstLine) == false) {
				if (FirstLine != LastLine) {
					for (i = FirstLine;  i <= LastLine; i++) {
						if (CurChar >= MSGTXT_SZ || CurChar < 0)
							System_Error("Mem error in mail\n%P");

						Message.Data.Offsets[NumLines] = (int16_t)CurChar;
						strlcpy(&Message.Data.MsgTxt[CurChar], ST_RMAILQUOTEBRACKET, MSGTXT_SZ - (size_t)CurChar);
						strlcat(&Message.Data.MsgTxt[CurChar], &Reply->Data.MsgTxt[ Reply->Data.Offsets[i-1]], MSGTXT_SZ - (size_t)CurChar);
						Message.Data.MsgTxt[CurChar + 78] = 0;

						CurChar += (strlen(&Message.Data.MsgTxt[CurChar]) + 1);
						if (CurChar >= MSGTXT_SZ || CurChar < 0)
							System_Error("Mem error in mail\n%P");

						NumLines++;
					}
				}
				else {
					if (CurChar >= MSGTXT_SZ || CurChar < 0)
						System_Error("Mem error in mail\n%P");

					Message.Data.Offsets[NumLines] = (int16_t)CurChar;
					strlcpy(&Message.Data.MsgTxt[CurChar], ST_RMAILQUOTEBRACKET, MSGTXT_SZ - (size_t)CurChar);
					strlcat(&Message.Data.MsgTxt[CurChar], &Reply->Data.MsgTxt[ Reply->Data.Offsets[FirstLine-1]], MSGTXT_SZ - (size_t)CurChar);
					Message.Data.MsgTxt[CurChar+78] = 0;

					CurChar += (strlen(&Message.Data.MsgTxt[CurChar]) + 1);
					if (CurChar >= MSGTXT_SZ || CurChar < 0)
						System_Error("Mem error in mail\n%P");

					NumLines++;
				}
				Quoted = true;
			}
		}
	}

	Line1[0] = 0;
	Line2[0] = 0;

	rputs(ST_RMAILHEADER);
	rputs(ST_LONGDIVIDER);

	strlcpy(OldLine, "", sizeof(OldLine));

	if (Quoted) {
		rputs(ST_MAILENTERCOLOR);
		for (i = 0; i < NumLines; i++) {
			rputs(&Message.Data.MsgTxt[ Message.Data.Offsets[i] ]);
			rputs(ST_MAILENTERCOLOR);
			rputs("\n");

			strlcpy(OldLine, &Message.Data.MsgTxt[ Message.Data.Offsets[i] ], sizeof(OldLine));
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
					Message.PublicMsgIndex = Village.Data.PublicMsgIndex;
					Village.Data.PublicMsgIndex++;
				}

				// make public but limit it depending on original message
				if (GlobalPost && (Reply->Flags & MF_ONEVILLONLY) == false)
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
				snprintf(string, sizeof(string), ST_MAILLIST, (int)(i + 1), &Message.Data.MsgTxt[ Message.Data.Offsets[i] ]);
				string[84] = 0;
				strlcat(string, "\n", sizeof(string));
				strlcat(string, ST_MAILENTERCOLOR, sizeof(string));
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
			if (CurChar < 0)
				CurChar = 0;

			strlcpy(Line1, OldLine, sizeof(Line1));

			if (NumLines == 0)
				OldLine[0] = 0;
			else
				strlcpy(OldLine, &Message.Data.MsgTxt[Message.Data.Offsets[NumLines-1]], sizeof(OldLine));

			Line2[0] = 0;
			continue;
		}
		if (CurChar >= MSGTXT_SZ || CurChar < 0)
			System_Error("Mem error in mail\n%P");

		Message.Data.Offsets[NumLines] = (int16_t)CurChar;
		strlcpy(&Message.Data.MsgTxt[CurChar], Line1, MSGTXT_SZ - (size_t)CurChar);
		CurChar += (strlen(Line1) + 1);
		if (CurChar >= MSGTXT_SZ || CurChar < 0)
			System_Error("Mem error in mail\n%P");

		strlcpy(OldLine, Line1, sizeof(OldLine));
		strlcpy(Line1, Line2, sizeof(Line1));
		NumLines++;

		if (NumLines >= 40) {
			rputs(ST_MAIL40LINES);
			break;
		}
	}
	Message.Data.Length = (int16_t)CurChar;
	Message.Data.NumLines = (int16_t)NumLines;

	// save message
	if ((GlobalPost && WhichVillage == -1) || GlobalPost == false) {
		fp = _fsopen(ST_MSJFILE, "a+b", _SH_DENYRW);
		if (!fp) {
			rputs(ST_NOMSJFILE);
			free(Message.Data.MsgTxt);
			return;
		}
		EncryptWrite_s(Message, &Message, fp, XOR_MSG);
		CheckedEncryptWrite(Message.Data.MsgTxt, (size_t)Message.Data.Length, fp, XOR_MSG);
		fclose(fp);
	}

	if (GlobalPost)
		SendMsj(&Message, WhichVillage);

	free(Message.Data.MsgTxt);
}


// ------------------------------------------------------------------------- //
bool Mail_Read(void)
{
	FILE *fp;
	long CurOffset, CurMsgOffset;
	int16_t iTemp, CurLine;
	struct Message Message;
	bool AllianceFound, NewMail = false, ReplyingToAlly = false, WillAlly;
	char szString[128], key = 0;

	rputs(ST_RMAILCHECKING);

	CurOffset = 0;

	for (;;) {
		fp = _fsopen(ST_MSJFILE, "r+b", _SH_DENYWR);
		if (!fp) return false;

		fseek(fp, CurOffset, SEEK_SET);

		CurMsgOffset = ftell(fp);
		notEncryptRead_s(Message, &Message, fp, XOR_MSG) {
			fclose(fp);
			break;
		}

		if (Message.Data.Length < 0)
			System_Error("Negative message in Mail_Read()");

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
			AllianceFound = false;
			for (iTemp = 0; !AllianceFound && (iTemp < MAX_ALLIES); iTemp++) {
				if (PClan->Alliances[iTemp] == Message.AllianceID)
					AllianceFound = true;
			}

			// if alliance not found, skip message
			if (AllianceFound == false) {
				fclose(fp);
				CurOffset += Message.Data.Length;
				continue;
			}
		}




		// Message IS readable, continue...

		if (PClan->PublicMsgIndex < Message.PublicMsgIndex)
			PClan->PublicMsgIndex = Message.PublicMsgIndex;

		NewMail = true;

		// Display Msg header
		rputs(ST_LONGDIVIDER);

		if (Message.Flags & MF_NOFROM)
			snprintf(szString, sizeof(szString), ST_RMAILHEADER2,  Message.szDate);
		else
			snprintf(szString, sizeof(szString), ST_RMAILHEADER1,  Message.szFromName,  Message.szDate);
		rputs(szString);

		if (Message.MessageType == MT_PUBLIC) {
			snprintf(szString, sizeof(szString), ST_RMAILPUBLICPOST, (int)Message.PublicMsgIndex);
			rputs(szString);
		}
		rputs("\n");

		if (Message.Flags == MF_ALLYREQ) {
			snprintf(szString, sizeof(szString), ST_RMAILSUBJALLY,  Message.szFromName, Message.szAllyName);
			rputs(szString);
		}
		if (Message.MessageType == MT_ALLIANCE) {
			snprintf(szString, sizeof(szString), "|0L Alliance: |0M%s.\n" , Message.szAllyName);
			rputs(szString);
		}

		if (Message.BBSIDFrom != Config.BBSID) {
			snprintf(szString, sizeof(szString), "|0L Origin  : |0M%s\n\r", Message.szFromVillageName);
			rputs(szString);
		}


		rputs(ST_LONGDIVIDER);


		// display message body
		Message.Data.MsgTxt = malloc((size_t)Message.Data.Length);
		CheckMem(Message.Data.MsgTxt);
		EncryptRead(Message.Data.MsgTxt, (size_t)Message.Data.Length, fp, XOR_MSG);
		CurOffset = ftell(fp);
		fclose(fp);

		// display message entered
		for (CurLine = 0; CurLine < Message.Data.NumLines; CurLine++) {
			if (CurLine == 17) {
				rputs(ST_MORE);
				od_sleep(0);
				if (toupper(od_get_key(true)) == 'N') {
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
			if (WillAlly == false)
				snprintf(szString, sizeof(szString), ST_RMAILREJECTALLY, PClan->szName);
			else
				snprintf(szString, sizeof(szString), ST_RMAILAGREEALLY, PClan->szName);
			GenericReply(&Message, szString, false);
		}
		else {
			/* regular message */
			rputs(ST_RMAILFOOTER2);

			key = od_get_answer("QRDS\r\n");
		}


		// Act on user input
		if ((!(Message.Flags & MF_ALLYREQ) && key == 'R') ||
				((Message.Flags & MF_ALLYREQ) && ReplyingToAlly == YES)) {
			if ((Message.Flags & MF_ALLYREQ) == false)
				rputs(ST_RMAILREPLY);

			// reply to message here
			Reply_Message(&Message);

			// can't delete if public post
			if (Message.MessageType == MT_PUBLIC) {
				free(Message.Data.MsgTxt);
				continue;
			}

			// ask if you want to delete it
			if ((Message.Flags & MF_ALLYREQ) == false && YesNo(ST_RMAILDELETEQ) == NO) {
				// doesn't want to
				free(Message.Data.MsgTxt);
				continue;
			}

			// delete message
			Message.Flags |= MF_DELETED;

			fp = _fsopen(ST_MSJFILE, "r+b", _SH_DENYWR);
			if (!fp) {
				rputs(ST_NOMSJFILE);
				free(Message.Data.MsgTxt);
				return NewMail;
			}
			fseek(fp, CurMsgOffset, SEEK_SET);

			EncryptWrite_s(Message, &Message, fp, XOR_MSG);
			fclose(fp);
		}
		if (key == 'D' || key == '\r' || key == '\n' || (Message.Flags & MF_ALLYREQ) || (Message.Flags & MF_NOFROM)) {
			if ((Message.Flags & MF_ALLYREQ) == false && (Message.Flags & MF_NOFROM) == false)
				rputs(ST_RMAILDELETE);
			else
				rputs(ST_RMAILDELETING);

			// delete message
			Message.Flags |= MF_DELETED;

			fp = _fsopen(ST_MSJFILE, "r+b", _SH_DENYWR);
			if (!fp) {
				rputs(ST_NOMSJFILE);
				free(Message.Data.MsgTxt);
				return NewMail;
			}
			fseek(fp, CurMsgOffset, SEEK_SET);

			EncryptWrite_s(Message, &Message, fp, XOR_MSG);
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

static int16_t InputStr(char *String, char *NextString, char *JustLen, int16_t CurLine)
{
	size_t cur_char = 0;
	int i;
	unsigned char ch, key;

	rputs(ST_MAILENTERCOLOR);
	rputs(String);
	cur_char = strlen(String);
	if (cur_char > INT_MAX)
		System_Error("String too long in InputStr()");

	for (;;) {
		if (cur_char == (*JustLen)) {
			String[cur_char] = 0;

			// break off last String
			for (i = (int)strlen(String); i>0; i--) {
				if (String[i] == ' ') {
					i++;
					break;
				}
			}

			if (i > ((*JustLen) / 2)) {
				strlcpy(NextString, &String[i], sizeof(NextString));
				String[i] = 0;

				for (; i < (int)cur_char; i++)
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

		ch = (unsigned char)od_get_key(false);
		if (ch=='\b') {
			if (cur_char>0) {
				cur_char--;
				rputs("\b \b");
			}
			else if (cur_char == 0 && CurLine > 0)
				return 4;
		}
		else if (cur_char > 0 && (isalnum(ch) || ispunct(ch)) && (String[cur_char - 1] == '%'  || String[cur_char - 1]=='&'))
			continue;
		else if (cur_char > 0 && (isdigit(ch) || toupper(ch) == 'S') && String[cur_char - 1] == '@')
			continue;
		else if (ch== 13) {
			rputs("\n");
			String[cur_char]=0;
			NextString[0]=0;
			break;
		}
		else if (ch == '\t' && cur_char < 73) {
			rputs("     ");
			strlcat(&String[cur_char], "     ", MSGTXT_SZ - cur_char);
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

			key = toupper(od_get_answer("SACLR?/\r\n") & 0x7f) & 0x7f;

			if (key == '?') {
				rputs(ST_INPUTSTRHELP);
				key = toupper(od_get_answer("SACLR/\r\n") & 0x7f) & 0x7f;
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
			String[cur_char] = (char)ch;
			cur_char++;
			od_putch((char)ch);
		}
	}

	return 0;
}


static void Msg_Create(int16_t ToClanID[2], int16_t MessageType, bool AllyReq, int16_t AllianceID,
				char *szAllyName, bool GlobalPost, int16_t WhichVillage)
/*
 * Allows user to enter a message.
 *
 * PRE: ToClanID is the ClanID who will receive the message if it is
 *        a private message.  Otherwise it is ignored
 *      MessageType determines what type of message this is.
 *      AllyReq is true if this is an alliance request.
 *      AllianceID is the alliance which is sending this message if it
 *        is an MT_ALLIANCE type of message OR it is an AllyReq
 *      szAllyName is the name of the alliance sending the message if it
 *        is an AllyReq or MT_ALLIANCE type.
 *      GlobalPost is true if it is an IBBS message.
 *      WhichVillage is the village to receive the message if it is a
 *        GlobalPost.  Set to ALL_VILLAGES if none specific.
 */
{
	struct Message Message;
	FILE *fp;
	size_t CurChar = 0;
	int16_t result, NumLines = 0, i;
	char string[128], JustLen = 78;
	char Line1[128], Line2[128], OldLine[128];

	if (MessageType == MT_PRIVATE) {
		if (GetClanID(ToClanID, false, false, -1, false) == false) {
			return;
		}
	}

	/* malloc msg */
	Message.Data.MsgTxt = malloc(MSGTXT_SZ);
	CheckMem(Message.Data.MsgTxt);
	Message.Data.MsgTxt[0] = 0;

	// Set up header
	Message.ToClanID[0] = ToClanID[0];
	Message.ToClanID[1] = ToClanID[1];
	Message.FromClanID[0] = PClan->ClanID[0];
	Message.FromClanID[1] = PClan->ClanID[1];
	strlcpy(Message.szFromName, PClan->szName, sizeof(Message.szFromName));
	strlcpy(Message.szDate, System.szTodaysDate, sizeof(Message.szDate));
	strlcpy(Message.szFromVillageName, Village.Data.szName, sizeof(Message.szFromVillageName));

	Message.BBSIDFrom = Config.BBSID;

	Message.Flags = 0;

	Message.MessageType = MessageType;

	if (MessageType == MT_ALLIANCE) {
		Message.AllianceID = AllianceID;
		if (szAllyName == NULL)
			Message.szAllyName[0] = 0;
		else
			strlcpy(Message.szAllyName, szAllyName, sizeof(Message.szAllyName));
	}

	if (AllyReq) {
		Message.Flags |= MF_ALLYREQ;
		strlcpy(Message.szAllyName, szAllyName, sizeof(Message.szAllyName));
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
					Message.PublicMsgIndex = Village.Data.PublicMsgIndex;
					Village.Data.PublicMsgIndex++;
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
				snprintf(string, sizeof(string), ST_MAILLIST, (int)(i + 1), &Message.Data.MsgTxt[ Message.Data.Offsets[i] ]);
				string[84] = 0;
				strlcat(string, "\n", sizeof(string));
				strlcat(string, ST_MAILENTERCOLOR, sizeof(string));
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
			if (CurChar < 0)
				CurChar = 0;

			strlcpy(Line1, OldLine, sizeof(Line1));

			if (NumLines == 0)
				OldLine[0] = 0;
			else
				strlcpy(OldLine, &Message.Data.MsgTxt[Message.Data.Offsets[NumLines-1]], sizeof(OldLine));

			Line2[0] = 0;
			continue;
		}

		if (CurChar >= MSGTXT_SZ || CurChar < 0)
			System_Error("Mem error in mail\n%P");

		// else continue
		Message.Data.Offsets[NumLines] = (int16_t)CurChar;
		strlcpy(&Message.Data.MsgTxt[CurChar], Line1, MSGTXT_SZ - (size_t)CurChar);
		CurChar += (strlen(Line1) + 1);
		if (CurChar >= MSGTXT_SZ || CurChar < 0)
			System_Error("Mem error in mail\n%P");

		strlcpy(OldLine, Line1, sizeof(OldLine));
		strlcpy(Line1, Line2, sizeof(Line1));
		NumLines++;

		if (NumLines == 40) {
			rputs(ST_MAIL40LINES);
			break;
		}
	}
	if (CurChar >= MSGTXT_SZ || CurChar < 0)
		System_Error("Mem error in mail\n%P");
	Message.Data.Length = (int16_t)CurChar;
	Message.Data.NumLines = NumLines;


	// if local post OR globalpost to ALL villages, do this
	if ((GlobalPost && WhichVillage == ALL_VILLAGES) || GlobalPost == false) {
		fp = _fsopen(ST_MSJFILE, "ab", _SH_DENYRW);
		if (!fp) {
			free(Message.Data.MsgTxt);
			rputs(ST_NOMSJFILE);
			return;
		}
		EncryptWrite_s(Message, &Message, fp, XOR_MSG);
		CheckedEncryptWrite(Message.Data.MsgTxt, (size_t)Message.Data.Length, fp, XOR_MSG);
		fclose(fp);
	}

	if (GlobalPost)
		SendMsj(&Message, WhichVillage);

	free(Message.Data.MsgTxt);
}


void Mail_Write(int16_t MessageType)
{
	int16_t DummyID[2];

	DummyID[0] = -1;
	DummyID[1] = -1;

	Msg_Create(DummyID, MessageType, false, -1, NULL, false, -1);
}


// ------------------------------------------------------------------------- //

void Mail_Maint(void)
{
	FILE *OldMessage, *NewMessage;
	struct Message Message;

	DisplayStr("* Mail_Maint()\n");

	OldMessage = _fsopen("clans.msj", "rb", _SH_DENYRW);
	if (OldMessage) {   // MSJ file exists, so go on
		NewMessage = _fsopen("tmp.$$$", "wb", _SH_DENYRW);
		if (!NewMessage) {
			DisplayStr("Error opening temp file\n");
			sleep(3);
			return;
		}

		for (;;) {
			notEncryptRead_s(Message, &Message, OldMessage, XOR_MSG)
				break;
			if (Message.Data.Length < 0)
				System_Error("Negative message in Mail_Maint()");

			if (Message.Flags & MF_DELETED)
				//FIXME:      DaysBetween(Message.Date, GameInfo.TheDate) > 7)
			{
				// deleted or else a week old, so skip it
				fseek(OldMessage, Message.Data.Length, SEEK_CUR);
			}
			else {
				/* write it, not read yet */
				Message.Data.MsgTxt = malloc((size_t)Message.Data.Length);
				CheckMem(Message.Data.MsgTxt);

				// write it to new file
				EncryptRead(Message.Data.MsgTxt, (size_t)Message.Data.Length, OldMessage, XOR_MSG);

				EncryptWrite_s(Message, &Message, NewMessage, XOR_MSG);
				CheckedEncryptWrite(Message.Data.MsgTxt, (size_t)Message.Data.Length, NewMessage, XOR_MSG);

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
	int16_t iTemp, NumAlliances = 0;
	int16_t ClanId[2];
	struct clan TmpClan = {0};

	// see if this alliance has too many members
	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
		if (Alliance->Member[iTemp][0] != -1)
			NumAlliances++;

	if (NumAlliances == MAX_ALLIANCEMEMBERS) {
		rputs(ST_ALLIANCEATMAX);
		return;
	}

	/* find player as you would if writing mail. */
	if (GetClanID(ClanId, false, false, Alliance->ID, false) == false) {
		rputs("\n\n");
		return;
	}

	/* ask if he wants to invite this guy for sure */
	if (NoYes(ST_ALLIANCESURE) == NO) {
		rputs(ST_ABORTED);
		return;
	}

	GetClan(ClanId, &TmpClan);

	// see if in this alliance already
	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
		if (TmpClan.Alliances[iTemp] == Alliance->ID)
			break;

	if (iTemp != MAX_ALLIES) {
		rputs(ST_ALLIANCEALREADY);
		FreeClanMembers(&TmpClan);
		return;
	}

	/* see if he has too many allies */
	NumAlliances = 0;
	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
		if (TmpClan.Alliances[iTemp] != -1)
			NumAlliances++;

	if (NumAlliances == MAX_ALLIES) {
		rputs(ST_ALLIANCEOTHERMAX);
		FreeClanMembers(&TmpClan);
		return;
	}

	/* if so, set up message but with ALLY flag */
	rputs(ST_ALLIANCELETTER);
	MyWriteMessage2(ClanId, false, true, Alliance->ID, Alliance->szName, false, -1);

	FreeClanMembers(&TmpClan);
}

void Mail_WriteToAllies(struct Alliance *Alliance)
{
	int16_t ClanId[2];

	ClanId[0] = -1;
	ClanId[1] = -1;
	MyWriteMessage2(ClanId, true, false, Alliance->ID, Alliance->szName, false, -1);
}





static void SendMsj(struct Message *Message, int16_t WhichVillage)
{
	// if WhichVillage == -1, do a for loop and send packet to all BBSes

	struct Packet Packet;
	int16_t CurBBS;
	FILE *fp;


	Packet.Active = true;
	Packet.BBSIDFrom = IBBS.Data.BBSID;
	Packet.PacketType = PT_MSJ;
	strlcpy(Packet.szDate, System.szTodaysDate, sizeof(Packet.szDate));
	if (Message->Data.Length < 0 || Message->Data.Length > MSGTXT_SZ)
		System_Error("Negative message length in SendMsj()");
	Packet.PacketLength = (int16_t)(BUF_SIZE_Message + (unsigned)Message->Data.Length);
	strlcpy(Packet.GameID, Game.Data.GameID, sizeof(Packet.GameID));

	if (WhichVillage != -1) {
		Packet.BBSIDTo = WhichVillage;

		fp = fopen("tmp.$$$", "wb");
		if (!fp)    return;

		/* write packet */
		EncryptWrite_s(Packet, &Packet, fp, XOR_PACKET);

		EncryptWrite_s(Message, Message, fp, XOR_PACKET);
		CheckedEncryptWrite(Message->Data.MsgTxt, (size_t)Message->Data.Length, fp, XOR_PACKET);

		fclose(fp);

		// send packet to BBS
		IBBS_SendPacketFile(Packet.BBSIDTo, "tmp.$$$");
		unlink("tmp.$$$");
	}
	else {
		// go through ALL BBSes and send this to them all

		for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
			if (IBBS.Data.Nodes[CurBBS].Active == false ||
					CurBBS+1 == IBBS.Data.BBSID)
				continue;

			Packet.BBSIDTo = CurBBS+1;

			fp = fopen("tmp.$$$", "wb");
			if (!fp)    return;

			/* write packet */
			EncryptWrite_s(Packet, &Packet, fp, XOR_PACKET);

			EncryptWrite_s(Message, Message, fp, XOR_PACKET);
			CheckedEncryptWrite(Message->Data.MsgTxt, (size_t)Message->Data.Length, fp, XOR_PACKET);

			fclose(fp);

			// send packet to BBS
			IBBS_SendPacketFile(Packet.BBSIDTo, "tmp.$$$");
			unlink("tmp.$$$");
		}
	}
}

void PostMsj(struct Message *Message)
{
	// open our .msj file, append this and update MsgPublicIndex if this
	// is a public post (i.e. MsgPublicIndex != 0)

	// if MsgPublicIndex == 0, leave the msj alone since it is a private post

	FILE *fp;

	if (Message->Data.Length < 0 || Message->Data.Length > MSGTXT_SZ)
		System_Error("Negative message length in PostMsj()");
	fp = fopen(ST_MSJFILE, "ab");

	if (fp) {
		// if public post (MsgPublicIndex != 0), update msgPublicIndex
		if (Message->MessageType == MT_PUBLIC)
			Message->PublicMsgIndex = Village.Data.PublicMsgIndex++;

		EncryptWrite_s(Message, Message, fp, XOR_MSG);
		CheckedEncryptWrite(Message->Data.MsgTxt, (size_t)Message->Data.Length, fp, XOR_MSG);
		fclose(fp);
	}
}

void GlobalMsgPost(void)
{
	// choose what type of post:
	//
	//  public (everyone sees)
	//  private (direct)

	const char *apszVillageNames[MAX_IBBSNODES],
	    *pszChoices[2] = { "1. Public", "2. Private" };
	const char *apszUserNames[50];
	int16_t iTemp, NumVillages, WhichVillage, NumClans;
	int16_t ClanIDs[50][2], WhichClan, ClanID[2], PostType;
	unsigned char VillageIndex[MAX_IBBSNODES];

	GetStringChoice(pszChoices, 2, "|0SWhat type of post?\n|0E> |0F",
					&PostType, true, DT_LONG, true);

	if (PostType == -1)
		return;

	// load up village names
	NumVillages = 0;
	for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
		apszVillageNames[iTemp] = NULL;

		// skip if our village
		if (iTemp+1 == IBBS.Data.BBSID)
			continue;

		if (IBBS.Data.Nodes[iTemp].Active) {
			apszVillageNames[NumVillages] = VillageName(iTemp + 1);
			VillageIndex[NumVillages] = (unsigned char)(iTemp + 1);
			NumVillages++;
		}
	}

	if (PostType == 0) {
		// public
		if (YesNo("|0SWrite to a specific village?  (No=All villages)") == YES) {
			// specific village, choose one
			GetStringChoice(apszVillageNames, NumVillages,
							"|0SEnter the name of the village\n|0G> |0F",
							&WhichVillage, true, DT_LONG, true);

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

		MyWriteMessage2(ClanID, true, false, -1, "", true, WhichVillage);
	}
	else {
		// private

		// specific village, choose one
		GetStringChoice(apszVillageNames, NumVillages,
						"|0SEnter the name of the village\n|0G> |0F",
						&WhichVillage, true, DT_LONG, true);

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
						&WhichClan, true, DT_WIDE, true);

		if (WhichClan == -1) {
			rputs(ST_ABORTED);
			for (iTemp = 0; iTemp < 50; iTemp++) {
				if (apszUserNames[iTemp])
					free((void*)apszUserNames[iTemp]);
			}
			return;
		}

		ClanID[0] = ClanIDs[ WhichClan ][0];
		ClanID[1] = ClanIDs[ WhichClan ][1];

		for (iTemp = 0; iTemp < 50; iTemp++) {
			if (apszUserNames[iTemp])
				free((void*)apszUserNames[iTemp]);
		}

		MyWriteMessage2(ClanID, false, false, -1, "", true, WhichVillage);
	}
}
