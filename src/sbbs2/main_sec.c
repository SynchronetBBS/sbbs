#line 1 "MAIN_SEC.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/******************************************************/
/* This file contains the single function, main_sec() */
/******************************************************/

#include "sbbs.h"

extern void logoff();

void main_cfg_cmd();
void main_ext_cmd(char ch);
void main_str_cmd(char *str);
void sysop_page(void);
ulong getposts(uint subnum);

char no_rip_menu=0;

void new_scan_ptr_cfg()
{
	char	str[128];
	long	i,j,s;
	ulong	l;
	time_t	t,lt;

while(online) {
	bputs(text[CfgGrpLstHdr]);
	for(i=0;i<usrgrps && online;i++) {
		checkline();
		if(i<9) outchar(SP);
		if(i<99) outchar(SP);
		bprintf(text[CfgGrpLstFmt],i+1,grp[usrgrp[i]]->lname); }
	SYNC;
	mnemonics(text[WhichOrAll]);
	s=getkeys("AQ",i);
	if(!s || s==-1 || s=='Q')
		break;
	if(s=='A') {
		mnemonics("\r\nEnter number of messages from end, ~Date, ~Quit, or"
			" [Last Message]: ");
		s=getkeys("DLQ",9999);
		if(s==-1 || s=='Q')
			continue;
		if(s=='D') {
			t=time(NULL);
			if(inputnstime(&t) && !(sys_status&SS_ABORT)) {
				bputs(text[LoadingMsgPtrs]);
				for(i=0;i<usrgrps && online;i++)
					for(j=0;j<usrsubs[i] && online;j++) {
						checkline();
						sub[usrsub[i][j]]->ptr=getmsgnum(usrsub[i][j],t); } }
			continue; }
		if(s=='L')
			s=0;
		if(s)
			s&=~0x80000000L;
		bputs(text[LoadingMsgPtrs]);
		for(i=0;i<usrgrps;i++)
			for(j=0;j<usrsubs[i] && online;j++) {
				checkline();
				getlastmsg(usrsub[i][j],&l,0);
				if(s>l)
					sub[usrsub[i][j]]->ptr=0;
				else
					sub[usrsub[i][j]]->ptr=l-s; }
		continue; }
	i=(s&~0x80000000L)-1;
	while(online) {
		l=0;
		bprintf(text[CfgSubLstHdr],grp[usrgrp[i]]->lname);
		for(j=0;j<usrsubs[i] && online;j++) {
			checkline();
			if(j<9) outchar(SP);
			if(j<99) outchar(SP);
			t=getmsgtime(usrsub[i][j],sub[usrsub[i][j]]->ptr);
			if(t>l)
				l=t;
			bprintf(text[SubPtrLstFmt],j+1,sub[usrsub[i][j]]->lname
				,timestr(&t),nulstr); }
		SYNC;
		mnemonics(text[WhichOrAll]);
		s=getkeys("AQ",j);
		if(sys_status&SS_ABORT) {
			lncntr=0;
			return; }
		if(s==-1 || !s || s=='Q')
			break;
		if(s=='A') {    /* The entire group */
			mnemonics("\r\nEnter number of messages from end, ~Date, ~Quit, or"
				" [Last Message]: ");
			s=getkeys("DLQ",9999);
			if(s==-1 || s=='Q')
				continue;
			if(s=='D') {
				t=l;
				if(inputnstime(&t) && !(sys_status&SS_ABORT)) {
					bputs(text[LoadingMsgPtrs]);
					for(j=0;j<usrsubs[i] && online;j++) {
						checkline();
						sub[usrsub[i][j]]->ptr=getmsgnum(usrsub[i][j],t); } }
				continue; }
			if(s=='L')
				s=0;
			if(s)
				s&=~0x80000000L;
			bputs(text[LoadingMsgPtrs]);
			for(j=0;j<usrsubs[i] && online;j++) {
				checkline();
				getlastmsg(usrsub[i][j],&l,0);
				if(s>l)
					sub[usrsub[i][j]]->ptr=0;
				else
					sub[usrsub[i][j]]->ptr=l-s; }
			continue; }
		else {
			j=(s&~0x80000000L)-1;
			mnemonics("\r\nEnter number of messages from end, ~Date, ~Quit, or"
				" [Last Message]: ");
			s=getkeys("DLQ",9999);
			if(s==-1 || s=='Q')
				continue;
			if(s=='D') {
				t=getmsgtime(usrsub[i][j],sub[usrsub[i][j]]->ptr);
				if(inputnstime(&t) && !(sys_status&SS_ABORT)) {
					bputs(text[LoadingMsgPtrs]);
					sub[usrsub[i][j]]->ptr=getmsgnum(usrsub[i][j],t); }
				continue; }
			if(s=='L')
				s=0;
			if(s)
				s&=~0x80000000L;
			getlastmsg(usrsub[i][j],&l,0);
			if(s>l)
				sub[usrsub[i][j]]->ptr=0;
			else
				sub[usrsub[i][j]]->ptr=l-s; }
			} }
}

void new_scan_cfg(ulong misc)
{
	long	s;
	ulong	i,j;
	ulong	t;

while(online) {
	bputs(text[CfgGrpLstHdr]);
	for(i=0;i<usrgrps && online;i++) {
		checkline();
		if(i<9) outchar(SP);
		if(i<99) outchar(SP);
		bprintf(text[CfgGrpLstFmt],i+1,grp[usrgrp[i]]->lname); }
	SYNC;
	if(misc&SUB_NSCAN)
		mnemonics(text[NScanCfgWhichGrp]);
	else
		mnemonics(text[SScanCfgWhichGrp]);
	s=getnum(i);
	if(s<1)
		break;
	i=s-1;
	while(online) {
		if(misc&SUB_NSCAN)
			misc&=~SUB_YSCAN;
		bprintf(text[CfgSubLstHdr],grp[usrgrp[i]]->lname);
		for(j=0;j<usrsubs[i] && online;j++) {
			checkline();
			if(j<9) outchar(SP);
			if(j<99) outchar(SP);
			bprintf(text[CfgSubLstFmt],j+1
				,sub[usrsub[i][j]]->lname
				,sub[usrsub[i][j]]->misc&misc ?
					(misc&SUB_NSCAN && sub[usrsub[i][j]]->misc&SUB_YSCAN) ?
					"To You Only" : text[On] : text[Off]);
				}
		SYNC;
		if(misc&SUB_NSCAN)
			mnemonics(text[NScanCfgWhichSub]);
		else
			mnemonics(text[SScanCfgWhichSub]);
		s=getkeys("AQ",j);
		if(sys_status&SS_ABORT) {
			lncntr=0;
			return; }
		if(!s || s==-1 || s=='Q')
			break;
		if(s=='A') {
			t=sub[usrsub[i][0]]->misc&misc;
			if(misc&SUB_NSCAN && !t)
				if(!noyes("Messages to you only"))
					misc|=SUB_YSCAN;
			for(j=0;j<usrsubs[i] && online;j++) {
				checkline();
				if(t) sub[usrsub[i][j]]->misc&=~misc;
				else  {
					if(misc&SUB_NSCAN)
						sub[usrsub[i][j]]->misc&=~SUB_YSCAN;
					sub[usrsub[i][j]]->misc|=misc; } }
			continue; }
		j=(s&~0x80000000L)-1;
		if(misc&SUB_NSCAN && !(sub[usrsub[i][j]]->misc&misc)) {
			if(!noyes("Messages to you only"))
				sub[usrsub[i][j]]->misc|=SUB_YSCAN;
			else
				sub[usrsub[i][j]]->misc&=~SUB_YSCAN; }
		sub[usrsub[i][j]]->misc^=misc; } }
}

/****************************************************************************/
/* Performs a new message scan all all sub-boards							*/
/****************************************************************************/
void scanallsubs(char mode)
{
	char str[256];
	int i,j,found=0;

if(/* action==NODE_MAIN && */ mode&(SCAN_FIND|SCAN_TOYOU)) {
	i=yesno(text[DisplayTitlesOnlyQ]);
	if(mode&SCAN_FIND) {
		bputs(text[SearchStringPrompt]);
		if(!getstr(str,40,K_LINE|K_UPPER))
			return;
		if(i) { 			/* if titles only */
			for(i=0;i<usrgrps;i++) {
				for(j=0;j<usrsubs[i] && !msgabort();j++)
					found=searchsub(usrsub[i][j],str);
				if(j<usrsubs[i])
					break; }
			if(!found)
				CRLF;
			sprintf(tmp,"Searched messages for '%s'",str);
			logline(nulstr,tmp);
			return; } }
	else if(mode&SCAN_TOYOU && i) {
		for(i=0;i<usrgrps;i++) {
			for(j=0;j<usrsubs[i] && !msgabort();j++)
				found=searchsub_toyou(usrsub[i][j]);
			if(j<usrsubs[i])
				break; }
		if(!found)
			CRLF;
		return; } }

if(useron.misc&(RIP|WIP) && !(useron.misc&EXPERT)) {
	menu("MSGSCAN"); }
for(i=0;i<usrgrps;i++) {
	for(j=0;j<usrsubs[i] && !msgabort();j++)
		if(((mode&SCAN_NEW && sub[usrsub[i][j]]->misc&(SUB_NSCAN|SUB_FORCED))
			|| mode&SCAN_FIND
			|| (mode&SCAN_TOYOU && sub[usrsub[i][j]]->misc&SUB_SSCAN))
			&& scanposts(usrsub[i][j],mode,str)) break;
	if(j<usrsubs[i])
		break; }
bputs(text[MessageScan]);
if(i<usrgrps) {
	bputs(text[MessageScanAborted]);
	return; }
bputs(text[MessageScanComplete]);
if(mode&SCAN_NEW && !(mode&(SCAN_BACK|SCAN_TOYOU))
	&& useron.misc&ANFSCAN && !(useron.rest&FLAG('T'))) {
	xfer_cmds++;
	scanalldirs(FL_ULTIME); }
}

/****************************************************************************/
/* Used to scan single or multiple sub-boards. 'mode' is the scan type.     */
/****************************************************************************/
void scansubs(char mode)
{
	char ch,str[256];
	int i=0,j,found=0;

mnemonics(text[SubGroupOrAll]);
ch=getkeys("SGA\r",0);
if(sys_status&SS_ABORT || ch==CR)
	return;

if(ch!='A' && mode&(SCAN_FIND|SCAN_TOYOU)) {
	if(yesno(text[DisplayTitlesOnlyQ])) i=1;
	if(mode&SCAN_FIND) {
		bputs(text[SearchStringPrompt]);
		if(!getstr(str,40,K_LINE|K_UPPER))
			return;
		if(i) { 			/* if titles only */
			if(ch=='S')
				found=searchsub(usrsub[curgrp][cursub[curgrp]],str);
			else if(ch=='G')
				for(i=0;i<usrsubs[curgrp] && !msgabort();i++)
					found=searchsub(usrsub[curgrp][i],str);
			sprintf(tmp,"Searched messages for '%s'",str);
			logline(nulstr,tmp);
			if(!found)
				CRLF;
			return; } }
	else if(mode&SCAN_TOYOU && i) {
		if(ch=='S')
			found=searchsub_toyou(usrsub[curgrp][cursub[curgrp]]);
		else if(ch=='G')
			for(i=0;i<usrsubs[curgrp] && !msgabort();i++)
				found=searchsub_toyou(usrsub[curgrp][i]);
		if(!found)
			CRLF;
		return; } }

if(ch=='S') {
	if(useron.misc&(RIP|WIP) && !(useron.misc&EXPERT)) {
		menu("MSGSCAN"); }
	i=scanposts(usrsub[curgrp][cursub[curgrp]],mode,str);
	bputs(text[MessageScan]);
	if(i) bputs(text[MessageScanAborted]);
	else bputs(text[MessageScanComplete]);
	return; }
if(ch=='G') {
	if(useron.misc&(RIP|WIP) && !(useron.misc&EXPERT)) {
		menu("MSGSCAN"); }
	for(i=0;i<usrsubs[curgrp] && !msgabort();i++)
		if(((mode&SCAN_NEW &&
			sub[usrsub[curgrp][i]]->misc&(SUB_NSCAN|SUB_FORCED))
			|| (mode&SCAN_TOYOU && sub[usrsub[curgrp][i]]->misc&SUB_SSCAN)
			|| mode&SCAN_FIND)
			&& scanposts(usrsub[curgrp][i],mode,str)) break;
	bputs(text[MessageScan]);
	if(i==usrsubs[curgrp]) bputs(text[MessageScanComplete]);
		else bputs(text[MessageScanAborted]);
	return; }

scanallsubs(mode);
}
