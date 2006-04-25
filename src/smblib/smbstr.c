/* smbstr.c */

/* Synchronet message base (SMB) library routines returning strings */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
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

#include <ctype.h>		/* is*() */
#include <string.h>		/* strcpy, strcat, memset, strchr */
#include <genwrap.h> 		/* stricmp */
#include "smblib.h"

char* SMBCALL smb_hfieldtype(ushort type)
{
	static char str[8];

	switch(type) {
		case SENDER:			return("Sender");
		case SENDERAGENT:		return("SenderAgent");
		case SENDERNETTYPE:		return("SenderNetType");
		case SENDERNETADDR:		return("SenderNetAddr");
		case SENDEREXT:			return("SenderExt");
		case SENDERORG:			return("SenderOrg");
		case SENDERIPADDR:		return("SenderIpAddr");
		case SENDERHOSTNAME:	return("SenderHostName");
		case SENDERPROTOCOL:	return("SenderProtocol");
		case SENDERPORT:		return("SenderPort");

		case REPLYTO:			return("ReplyTo");
		case REPLYTOAGENT:		return("ReplyToAgent");
		case REPLYTONETTYPE:	return("ReplyToNetType");
		case REPLYTONETADDR:	return("ReplyToNetAddr");
		case REPLYTOEXT:		return("ReplyToExt");
								
		case RECIPIENT:			return("Recipient");
		case RECIPIENTAGENT:	return("RecipientAgent");
		case RECIPIENTNETTYPE:	return("RecipientNetType");
		case RECIPIENTNETADDR:	return("RecipientNetAddr");
		case RECIPIENTEXT:		return("RecipientExt");

		case SUBJECT:			return("Subject");
		case SMB_SUMMARY:		return("Summary");
		case SMB_COMMENT:		return("Comment");
		case SMB_CARBONCOPY:	return("CarbonCopy");
		case SMB_GROUP:			return("Group");
		case SMB_EXPIRATION:	return("Expiration");
		case SMB_PRIORITY:		return("Priority");
		case SMB_COST:			return("Cost");

		case FIDOCTRL:			return("FidoCtrl");
		case FIDOAREA:			return("FidoArea");
		case FIDOSEENBY:		return("FidoSeenBy");
		case FIDOPATH:			return("FidoPath");
		case FIDOMSGID:			return("FidoMsgID");
		case FIDOREPLYID:		return("FidoReplyID");
		case FIDOPID:			return("FidoPID");
		case FIDOFLAGS:			return("FidoFlags");
		case FIDOTID:			return("FidoTID");

		case RFC822HEADER:		return("RFC822Header");
		case RFC822MSGID:		return("RFC822MsgID");
		case RFC822REPLYID:		return("RFC822ReplyID");
		case RFC822TO:			return("RFC822To");
		case RFC822FROM:		return("RFC822From");
		case RFC822REPLYTO:		return("RFC822ReplyTo");

		case USENETPATH:		return("UsenetPath");
		case USENETNEWSGROUPS:	return("UsenetNewsgroups");

		case SMTPCOMMAND:		return("SMTPCommand");
		case SMTPREVERSEPATH:	return("SMTPReversePath");
		case SMTPFORWARDPATH:	return("SMTPForwardPath");

		case SMTPSYSMSG:		return("SMTPSysMsg");

		case UNKNOWN:			return("UNKNOWN");
		case UNKNOWNASCII:		return("UNKNOWNASCII");
		case UNUSED:			return("UNUSED");
	}
	sprintf(str,"%02Xh",type);
	return(str);
}

ushort SMBCALL smb_hfieldtypelookup(const char* str)
{
	ushort type;

	if(isdigit(*str))
		return((ushort)strtol(str,NULL,0));

	for(type=0;type<=UNUSED;type++)
		if(stricmp(str,smb_hfieldtype(type))==0)
			return(type);

	return(UNKNOWN);
}

char* SMBCALL smb_dfieldtype(ushort type)
{
	static char str[8];

	switch(type) {
		case TEXT_BODY: return("TEXT_BODY");
		case TEXT_TAIL: return("TEXT_TAIL");
		case UNUSED:	return("UNUSED");
	}
	sprintf(str,"%02Xh",type);
	return(str);
}

char* SMBCALL smb_hashsourcetype(uchar type)
{
	static char str[8];

	switch(type) {
		case SMB_HASH_SOURCE_BODY:		return(smb_dfieldtype(TEXT_BODY));
		case SMB_HASH_SOURCE_MSG_ID:	return(smb_hfieldtype(RFC822MSGID));
		case SMB_HASH_SOURCE_FTN_ID:	return(smb_hfieldtype(FIDOMSGID));
	}
	sprintf(str,"%02Xh",type);
	return(str);
}

char* SMBCALL smb_hashsource(smbmsg_t* msg, int source)
{
	switch(source) {
		case SMB_HASH_SOURCE_MSG_ID:
			return(msg->id);
		case SMB_HASH_SOURCE_FTN_ID:
			return(msg->ftn_msgid);
	}
	return("hash");
}

/****************************************************************************/
/* Converts when_t.zone into ASCII format                                   */
/****************************************************************************/
char* SMBCALL smb_zonestr(short zone, char* outstr)
{
	char*		plus;
    static char str[32];

	switch((ushort)zone) {
		case 0:     return("UTC");
		case AST:   return("AST");
		case EST:   return("EST");
		case CST:   return("CST");
		case MST:   return("MST");
		case PST:   return("PST");
		case YST:   return("YST");
		case HST:   return("HST");
		case BST:   return("BST");
		case ADT:   return("ADT");
		case EDT:   return("EDT");
		case CDT:   return("CDT");
		case MDT:   return("MDT");
		case PDT:   return("PDT");
		case YDT:   return("YDT");
		case HDT:   return("HDT");
		case BDT:   return("BDT");
		case MID:   return("MID");
		case VAN:   return("VAN");
		case EDM:   return("EDM");
		case WIN:   return("WIN");
		case BOG:   return("BOG");
		case CAR:   return("CAR");
		case RIO:   return("RIO");
		case FER:   return("FER");
		case AZO:   return("AZO");
		case LON:   return("LON");
		case BER:   return("BER");
		case ATH:   return("ATH");
		case MOS:   return("MOS");
		case DUB:   return("DUB");
		case KAB:   return("KAB");
		case KAR:   return("KAR");
		case BOM:   return("BOM");
		case KAT:   return("KAT");
		case DHA:   return("DHA");
		case BAN:   return("BAN");
		case HON:   return("HON");
		case TOK:   return("TOK");
		case SYD:   return("SYD");
		case NOU:   return("NOU");
		case WEL:   return("WEL");
		}

	if(!OTHER_ZONE(zone)) {
		if(zone&(WESTERN_ZONE|US_ZONE))	/* West of UTC? */
			zone=-(zone&0xfff);
		else
			zone&=0xfff;
	}

	if(zone>0)
		plus="+";
	else
		plus="";
	sprintf(str,"UTC%s%d:%02u", plus, zone/60, zone<0 ? (-zone)%60 : zone%60);

	if(outstr==NULL)
		return(str);
	strcpy(outstr,str);
	return(outstr);
}

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/****************************************************************************/
char* SMBCALL smb_faddrtoa(fidoaddr_t* addr, char* outstr)
{
	static char str[64];
    char point[25];

	if(addr==NULL)
		return("0:0/0");
	sprintf(str,"%hu:%hu/%hu",addr->zone,addr->net,addr->node);
	if(addr->point) {
		sprintf(point,".%hu",addr->point);
		strcat(str,point); 
	}
	if(outstr==NULL)
		return(str);
	strcpy(outstr,str);
	return(outstr);
}

/****************************************************************************/
/* Returns the FidoNet address parsed from str.								*/
/****************************************************************************/
fidoaddr_t SMBCALL smb_atofaddr(const fidoaddr_t* sys_addr, const char *str)
{
	char *p;
	fidoaddr_t addr;
	fidoaddr_t tmp_addr={1,1,1,0};	/* Default system address: 1:1/1.0 */

	if(sys_addr==NULL)
		sys_addr=&tmp_addr;

	ZERO_VAR(addr);
	if((p=strchr(str,':'))!=NULL) {
		addr.zone=atoi(str);
		addr.net=atoi(p+1); 
	} else {
		addr.zone=sys_addr->zone;
		addr.net=atoi(str);
	}
	if(addr.zone==0)              /* no such thing as zone 0 */
		addr.zone=1;
	if((p=strchr(str,'/'))!=NULL)
		addr.node=atoi(p+1);
	else {
		if(addr.zone==sys_addr->zone)
			addr.net=sys_addr->net;
		addr.node=atoi(str); 
	}
	if((p=strchr(str,'.'))!=NULL)
		addr.point=atoi(p+1);
	return(addr);
}

/****************************************************************************/
/* Returns ASCIIZ representation of network address (net_t)					*/
/****************************************************************************/
char* SMBCALL smb_netaddr(net_t* net)
{
	if(net->type==NET_FIDO)
		return(smb_faddrtoa((fidoaddr_t*)net->addr,NULL));
	return(net->addr);
}

/****************************************************************************/
/* Returns net_type for passed e-mail address (i.e. "user@addr")			*/
/****************************************************************************/
ushort SMBCALL smb_netaddr_type(const char* str)
{
	char*	p;
	char*	tp;

	if((p=strchr(str,'@'))==NULL)
		return(NET_NONE);

	p++;
	SKIP_WHITESPACE(p);
	if(*p==0)
		return(NET_UNKNOWN);

	if(isalpha(*p) && strchr(p,'.')==NULL)
		return(NET_QWK);

	for(tp=p;*tp;tp++) {
		if(!isdigit(*tp) && *tp!=':' && *tp!='/' && *tp!='.')
			break;
	}
	if(isdigit(*p) && *tp)
		return(NET_FIDO);
	if(isalnum(*p))
		return(NET_INTERNET);

	return(NET_UNKNOWN);
}
