#line 1 "XFER_SEC.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/*****************************************************/
/* This file contains the single function xfer_sec() */
/*****************************************************/

#include "sbbs.h"

void xfer_cfg_cmd();
void xfer_ext_cmd(char ch);
void xfer_str_cmd(char *str);

void scandirs(char mode);

extern char no_rip_menu;

/****************************************************************************/
/* Used to scan single or multiple directories. 'mode' is the scan type.    */
/****************************************************************************/
void scandirs(char mode)
{
	char ch,str[256];
	int s;
	uint i,k;

if(!usrlibs) return;
mnemonics(text[DirLibOrAll]);
ch=getkeys("DLA\r",0);
if(sys_status&SS_ABORT || ch==CR) {
	lncntr=0;
	return; }
if(ch!='A') {
	if(mode&FL_ULTIME) {			/* New file scan */
		bprintf(text[NScanHdr],timestr(&ns_time));
		str[0]=0; }
	else if(mode==FL_NO_HDR) {		/* Search for a string */
		if(!getfilespec(tmp))
			return;
		padfname(tmp,str); }
	else if(mode==FL_FINDDESC) {	/* Find text in description */
		if(!noyes(text[SearchExtendedQ]))
			mode=FL_EXFIND;
		if(sys_status&SS_ABORT) {
			lncntr=0;
			return; }
		bputs(text[SearchStringPrompt]);
		if(!getstr(str,40,K_LINE|K_UPPER)) {
			lncntr=0;
			return; } } }
if(ch=='D') {
	if((s=listfiles(usrdir[curlib][curdir[curlib]],str,0,mode))==-1)
		return;
	bputs("\r\1>");
	if(s>1)
		bprintf(text[NFilesListed],s);
	else if(!s && !(mode&FL_ULTIME))
		bputs(text[FileNotFound]);
	return; }
if(ch=='L') {
	k=0;
	for(i=0;i<usrdirs[curlib] && !msgabort();i++) {
		attr(LIGHTGRAY);
		outchar('.');
		if(i && !(i%5))
			bputs("\b\b\b\b\b     \b\b\b\b\b");
		if(mode&FL_ULTIME	/* New-scan */
			&& (lib[usrlib[curlib]]->offline_dir==usrdir[curlib][i]
			|| dir[usrdir[curlib][i]]->misc&DIR_NOSCAN))
			continue;
		else if((s=listfiles(usrdir[curlib][i],str,0,mode))==-1)
			return;
		else k+=s; }
	bputs("\r\1>");
	if(k>1)
		bprintf(text[NFilesListed],k);
	else if(!k && !(mode&FL_ULTIME))
		bputs(text[FileNotFound]);
	return; }

scanalldirs(mode);
}

/****************************************************************************/
/* Scan all directories in all libraries for files							*/
/****************************************************************************/
void scanalldirs(char mode)
{
	char str[256];
	int s;
	uint i,j,k,d;

if(!usrlibs) return;
k=0;
if(mode&FL_ULTIME) {			/* New file scan */
	bprintf(text[NScanHdr],timestr(&ns_time));
	str[0]=0; }
else if(mode==FL_NO_HDR) {		/* Search for a string */
	if(!getfilespec(tmp))
		return;
	padfname(tmp,str); }
else if(mode==FL_FINDDESC) {	/* Find text in description */
	if(!noyes(text[SearchExtendedQ]))
		mode=FL_EXFIND;
	if(sys_status&SS_ABORT) {
		lncntr=0;
		return; }
	bputs(text[SearchStringPrompt]);
	if(!getstr(str,40,K_LINE|K_UPPER)) {
		lncntr=0;
		return; } }
for(i=d=0;i<usrlibs;i++) {
	for(j=0;j<usrdirs[i] && !msgabort();j++,d++) {
		attr(LIGHTGRAY);
		outchar('.');
		if(d && !(d%5))
			bputs("\b\b\b\b\b     \b\b\b\b\b");
		if(mode&FL_ULTIME /* New-scan */
			&& (lib[usrlib[i]]->offline_dir==usrdir[i][j]
			|| dir[usrdir[i][j]]->misc&DIR_NOSCAN))
			continue;
		else if((s=listfiles(usrdir[i][j],str,0,mode))==-1)
			return;
		else k+=s; }
	if(j<usrdirs[i])   /* aborted */
		break; }
bputs("\r\1>");
if(k>1)
	bprintf(text[NFilesListed],k);
else if(!k && !(mode&FL_ULTIME))
	bputs(text[FileNotFound]);
}

