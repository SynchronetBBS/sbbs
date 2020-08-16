/* sbbs4defs.h */

/* Synchronet v4 constants, macros, and structure definitions */

/* $Id: sbbs4defs.h,v 1.6 2018/07/24 01:11:07 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _SBBS4DEFS_H
#define _SBBS4DEFS_H

#define USER_REC_LINE_LEN	1000					/* includes CRLF terminator */
#define USER_REC_LEN		(USER_REC_LINE_LEN-2)	/* does not include CRLF */

char* user_dat_columns[] = {
	 "Alias"
	,"Name"
	,"Handle"
	,"IpAddress"
	,"Hostname"
	,"Comment"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"Netmail"
	,"Address"
	,"Location"
	,"Zip Code"
	,"Password"
	,"PhoneNumber"
	,"BirthDate"
	,"Gender"
	,"Connection"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"LastOnDate"
	,"FirstOnDate"
	,"ExpirationDate"
	,"PasswordModDate"
	,"NewFileScanDate"
	,"CurrentLogonTime"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"Logons"
	,"LogonsToday"
	,"TimeOn"
	,"ExtraTime"
	,"TimeToday"
	,"TimeUsedLastLogon"
	,"MessagesPosted"
	,"EmailsSent"
	,"FeedbackSent"
	,"EmailsToday"
	,"PostsToday"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"BytesUploaded"
	,"FilesUplaoded"
	,"BytesDownloaded"
	,"FilesDownloaded"
	,"Credits"
	,"FreeCredits"
	,"Minutes"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"SecurityLevel"
	,"FlagSet1"
	,"FlagSet2"
	,"FlagSet3"
	,"FlagSet4"
	,"ExemptionFlags"
	,"RestrictionFlags"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"MiscSettings"
	,"QWKSettings"
	,"ChatSettings"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"Reserved"
	,"TerminalRows"
	,"TransferProtocol"
	,"ExternalEditor"
	,"CommandShell"
	,"TempFileType"
	,"CurrentSubboard"
	,"CurrentDirectory"
	,"CurrentExternalProgram"
	/* Insert new fields here */
	,"Padding"
	,NULL
};

char* stats_dat_columns[] = {
	 "Date"
	,"Logons"
	,"Timeon"
	,"FilesUploaded"
	,"BytesUploaded"
	,"FilesDownloaded"
	,"BytesDownloaded"
	,"Posts"
	,"EmailsSent"
	,"FeedbackSent"
	,NULL 
};

#endif /* Don't add anything after this #endif statement */
