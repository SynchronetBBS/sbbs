#line 1 "FIDO.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/*********************************************/
/* Functions that pertain solely to FidoNet  */
/*********************************************/

#include "sbbs.h"

extern char *mon[];
void inetmail(char *into, char *subj, char mode);
void qnetmail(char *into, char *subj, char mode);
int qwk_route(char *inaddr, char *fulladdr);

void pt_zone_kludge(fmsghdr_t hdr,int fido)
{
	char str[256];

sprintf(str,"\1INTL %u:%u/%u %u:%u/%u\r"
	,hdr.destzone,hdr.destnet,hdr.destnode
	,hdr.origzone,hdr.orignet,hdr.orignode);
write(fido,str,strlen(str));

if(hdr.destpoint) {
    sprintf(str,"\1TOPT %u\r"
		,hdr.destpoint);
    write(fido,str,strlen(str)); }

if(hdr.origpoint) {
    sprintf(str,"\1FMPT %u\r"
		,hdr.origpoint);
    write(fido,str,strlen(str)); }
}

int lookup_netuser(char *into)
{
	char to[128],name[26],str[256],q[128];
	int i;
	FILE *stream;

if(strchr(into,'@'))
	return(0);
strcpy(to,into);
strupr(to);
sprintf(str,"%sQNET\\USERS.DAT",data_dir);
if((stream=fnopen(&i,str,O_RDONLY))==NULL)
	return(0);
while(!feof(stream)) {
	if(!fgets(str,250,stream))
		break;
	str[25]=0;
	truncsp(str);
	strcpy(name,str);
	strupr(name);
	str[35]=0;
	truncsp(str+27);
	sprintf(q,"Do you mean %s @%s",str,str+27);
	if(strstr(name,to) && yesno(q)) {
		fclose(stream);
		sprintf(into,"%s@%s",str,str+27);
		return(0); }
	if(sys_status&SS_ABORT)
		break; }
fclose(stream);
return(1);
}

/****************************************************************************/
/* Send FidoNet NetMail from BBS											*/
/****************************************************************************/
void netmail(char *into, char *title, char mode)
{
	char str[256],subj[128],to[256],fname[128],*buf,*p,ch;
	int file,fido,x,cc_found,cc_sent;
	uint i;
	long length,l;
	faddr_t addr;
	fmsghdr_t hdr;

sprintf(subj,"%.127s",title);


strcpy(to,into);

lookup_netuser(to);

p=strrchr(to,'@');      /* Find '@' in name@addr */
if(p && !isdigit(*(p+1)) && !strchr(p,'.') && !strchr(p,':')) {
	mode&=~WM_FILE;
	qnetmail(to,title,mode|WM_NETMAIL);
	return; }
if(p==NULL || !strchr(p+1,'/') || !total_faddrs) {
	if(!p && dflt_faddr.zone)
		addr=dflt_faddr;
	else if(inetmail_misc&NMAIL_ALLOW) {
		if(mode&WM_FILE && !SYSOP && !(inetmail_misc&NMAIL_FILE))
			mode&=~WM_FILE;
		inetmail(into,title,mode|WM_NETMAIL);
		return; }
	else if(dflt_faddr.zone)
		addr=dflt_faddr;
	else {
		bprintf("\1n\r\nInvalid NetMail address.\r\n");
		return; } }
else {
	addr=atofaddr(p+1); 	/* Get fido address */
	*p=0;					/* Chop off address */
	}

if(mode&WM_FILE && !SYSOP && !(netmail_misc&NMAIL_FILE))
	mode&=~WM_FILE;

if((!SYSOP && !(netmail_misc&NMAIL_ALLOW)) || useron.rest&FLAG('M')
    || !total_faddrs) {
    bputs(text[NoNetMailAllowed]);
    return; }

truncsp(to);				/* Truncate off space */

memset(&hdr,0,sizeof(hdr));   /* Initialize header to null */
strcpy(hdr.from,netmail_misc&NMAIL_ALIAS ? useron.alias : useron.name);
sprintf(hdr.to,"%.35s",to);

/* Look-up in nodelist? */

if(netmail_cost && !(useron.exempt&FLAG('S'))) {
	if(useron.cdt+useron.freecdt<netmail_cost) {
		bputs(text[NotEnoughCredits]);
		return; }
	sprintf(str,text[NetMailCostContinueQ],netmail_cost);
	if(noyes(str))
		return; }


now=time(NULL);
unixtodos(now,&date,&curtime);
sprintf(hdr.time,"%02u %3.3s %02u  %02u:%02u:%02u"
	,date.da_day,mon[date.da_mon-1],TM_YEAR(date.da_year-1900)
	,curtime.ti_hour,curtime.ti_min,curtime.ti_sec);

hdr.destzone	=addr.zone;
hdr.destnet 	=addr.net;
hdr.destnode	=addr.node;
hdr.destpoint	=addr.point;

for(i=0;i<total_faddrs;i++)
	if(addr.zone==faddr[i].zone && addr.net==faddr[i].net)
        break;
if(i==total_faddrs) {
	for(i=0;i<total_faddrs;i++)
		if(addr.zone==faddr[i].zone)
			break; }
if(i==total_faddrs)
	i=0;
hdr.origzone	=faddr[i].zone;
hdr.orignet 	=faddr[i].net;
hdr.orignode	=faddr[i].node;
hdr.origpoint	=faddr[i].point;

strcpy(str,faddrtoa(faddr[i]));
bprintf(text[NetMailing],hdr.to,faddrtoa(addr),hdr.from,str);

hdr.attr=(FIDO_LOCAL|FIDO_PRIVATE);

if(netmail_misc&NMAIL_CRASH) hdr.attr|=FIDO_CRASH;
if(netmail_misc&NMAIL_HOLD)  hdr.attr|=FIDO_HOLD;
if(netmail_misc&NMAIL_KILL)  hdr.attr|=FIDO_KILLSENT;
if(mode&WM_FILE) hdr.attr|=FIDO_FILE;

sprintf(str,"%sNETMAIL.MSG",node_dir);
remove(str);	/* Just incase it's already there */
// mode&=~WM_FILE;
if(!writemsg(str,nulstr,subj,WM_NETMAIL|mode,INVALID_SUB,into)) {
	bputs(text[Aborted]);
	return; }

if(mode&WM_FILE) {
	strcpy(fname,subj);
	sprintf(str,"%sFILE\\%04u.OUT",data_dir,useron.number);
	mkdir(str);
	strcpy(tmp,data_dir);
	if(tmp[0]=='.')    /* Relative path */
		sprintf(tmp,"%s%s",node_dir,data_dir);
	sprintf(str,"%sFILE\\%04u.OUT\\%s",tmp,useron.number,fname);
	strcpy(subj,str);
	if(fexist(str)) {
		bputs(text[FileAlreadyThere]);
		return; }
	if(online==ON_LOCAL) {		/* Local upload */
		bputs(text[EnterPath]);
		if(!getstr(str,60,K_LINE|K_UPPER)) {
			bputs(text[Aborted]);
			return; }
		backslash(str);
		strcat(str,fname);
		if(mv(str,subj,1))
			return; }
	else { /* Remote */
		menu("ULPROT");
		mnemonics(text[ProtocolOrQuit]);
		strcpy(str,"Q");
		for(x=0;x<total_prots;x++)
			if(prot[x]->ulcmd[0] && chk_ar(prot[x]->ar,useron)) {
				sprintf(tmp,"%c",prot[x]->mnemonic);
				strcat(str,tmp); }
		ch=getkeys(str,0);
		if(ch=='Q' || sys_status&SS_ABORT) {
			bputs(text[Aborted]);
			return; }
		for(x=0;x<total_prots;x++)
			if(prot[x]->ulcmd[0] && prot[x]->mnemonic==ch
				&& chk_ar(prot[x]->ar,useron))
				break;
		if(x<total_prots)	/* This should be always */
			protocol(cmdstr(prot[x]->ulcmd,subj,nulstr,NULL),0); }
	l=flength(subj);
	if(l>0)
		bprintf(text[FileNBytesReceived],fname,ultoac(l,tmp));
	else {
		bprintf(text[FileNotReceived],fname);
        return; } }

p=subj;
if((SYSOP || useron.exempt&FLAG('F'))
	&& !strncmpi(p,"CR:",3)) {     /* Crash over-ride by sysop */
	p+=3;				/* skip CR: */
	if(*p==SP) p++; 	/* skip extra space if it exists */
	hdr.attr|=FIDO_CRASH; }

if((SYSOP || useron.exempt&FLAG('F'))
	&& !strncmpi(p,"FR:",3)) {     /* File request */
	p+=3;				/* skip FR: */
	if(*p==SP) p++;
	hdr.attr|=FIDO_FREQ; }

if((SYSOP || useron.exempt&FLAG('F'))
	&& !strncmpi(p,"RR:",3)) {     /* Return receipt request */
	p+=3;				/* skip RR: */
	if(*p==SP) p++;
	hdr.attr|=FIDO_RRREQ; }

if((SYSOP || useron.exempt&FLAG('F'))
	&& !strncmpi(p,"FA:",3)) {     /* File Attachment */
	p+=3;				/* skip FA: */
	if(*p==SP) p++;
	hdr.attr|=FIDO_FILE; }

sprintf(hdr.subj,"%.71s",p);

sprintf(str,"%sNETMAIL.MSG",node_dir);
if((file=nopen(str,O_RDONLY))==-1) {
    close(fido);
    errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
    return; }
length=filelength(file);
if((buf=(char *)MALLOC(length))==NULL) {
    close(fido);
    close(file);
    errormsg(WHERE,ERR_ALLOC,str,length);
    return; }
read(file,buf,length);
close(file);

cc_sent=0;
while(1) {
	for(i=1;i;i++) {
		sprintf(str,"%s%u.MSG",netmail_dir,i);
		if(!fexist(str))
			break; }
	if(!i) {
		bputs(text[TooManyEmailsToday]);
		return; }
	if((fido=nopen(str,O_WRONLY|O_CREAT|O_EXCL))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_EXCL);
		return; }
	write(fido,&hdr,sizeof(hdr));

	pt_zone_kludge(hdr,fido);

	if(netmail_misc&NMAIL_DIRECT) {
		sprintf(str,"\1FLAGS DIR\r\n");
		write(fido,str,strlen(str)); }
	if(mode&WM_FILE) {
		sprintf(str,"\1FLAGS KFS\r\n");
		write(fido,str,strlen(str)); }

	if(cc_sent) {
		sprintf(str,"* Originally to: %s\r\n\r\n",into);
		write(fido,str,strlen(str)); }

	l=0L;
	while(l<length) {
		if(buf[l]==1)	/* Ctrl-A, so skip it and the next char */
			l++;
		else if(buf[l]!=LF) {
			if((uchar)buf[l]==0x8d)   /* r0dent i converted to normal i */
				buf[l]='i';
			write(fido,buf+l,1); }
		l++; }
	l=0;
	write(fido,&l,1);	/* Null terminator */
	close(fido);

	if(!(useron.exempt&FLAG('S')))
		subtract_cdt(netmail_cost);
	if(mode&WM_FILE)
		sprintf(str,"Sent NetMail file attachment to %s (%s)"
			,hdr.to,faddrtoa(addr));
	else
		sprintf(str,"Sent NetMail to %s (%s)"
			,hdr.to,faddrtoa(addr));
	logline("EN",str);

	cc_found=0;
	for(l=0;l<length && cc_found<=cc_sent;l++)
		if(l+3<length && !strnicmp(buf+l,"CC:",3)) {
			cc_found++;
			l+=2; }
		else {
			while(l<length && *(buf+l)!=LF)
				l++; }
	if(!cc_found)
		break;
	while(l<length && *(buf+l)==SP) l++;
	for(i=0;l<length && *(buf+l)!=LF && i<128;i++,l++)
		str[i]=buf[l];
	if(!i)
		break;
	str[i]=0;
	p=strrchr(str,'@');
	if(p) {
		addr=atofaddr(p+1);
		*p=0;
		sprintf(hdr.to,"%.35s",str); }
	else {
		atofaddr(str);
		strcpy(hdr.to,"Sysop"); }
	hdr.destzone	=addr.zone;
	hdr.destnet 	=addr.net;
	hdr.destnode	=addr.node;
	hdr.destpoint	=addr.point;
	cc_sent++; }

if(netmail_sem[0])		/* update semaphore file */
	if((file=nopen(netmail_sem,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
		close(file);

FREE(buf);
}

/****************************************************************************/
/* Send FidoNet NetMail from QWK REP Packet 								*/
/****************************************************************************/
void qwktonetmail(FILE *rep, char *block, char *into, uchar fromhub)
{
	char	HUGE16 *qwkbuf,to[129],name[129],sender[129],senderaddr[129]
		   ,str[256],*p,*cp,*addr,fulladdr[129],ch,buf[SDT_BLOCK_LEN];
	int 	i,fido,inet=0,qnet=0,x;
	ushort	net,xlat;
	long	l,offset,length,m,n;
	faddr_t fidoaddr;
    fmsghdr_t hdr;
	smbmsg_t msg;

if(useron.rest&FLAG('M')) {
    bputs(text[NoNetMailAllowed]);
	return; }

sprintf(str,"%.6s",block+116);
n=atol(str);	  /* i = number of 128 byte records */

if(n<2L || n>999999L) {
	errormsg(WHERE,ERR_CHK,"QWK blocks",n);
	return; }
if((qwkbuf=MALLOC(n*128L))==NULL) {
	errormsg(WHERE,ERR_ALLOC,nulstr,n*128L);
	return; }
memcpy((char *)qwkbuf,block,128);
fread(qwkbuf+128,n-1,128,rep);

if(into==NULL)
	sprintf(to,"%-128.128s",(char *)qwkbuf+128);  /* To user on first line */
else
	strcpy(to,into);

p=strchr(to,0xe3);		/* chop off at first CR */
if(p) *p=0;

strcpy(name,to);
p=strchr(name,'@');
if(p) *p=0;
truncsp(name);


p=strrchr(to,'@');       /* Find '@' in name@addr */
if(p && !isdigit(*(p+1)) && !strchr(p,'.') && !strchr(p,':')) { /* QWKnet */
	qnet=1;
	*p=0; }
else if(p==NULL || !isdigit(*(p+1)) || !total_faddrs) {
	if(p==NULL && dflt_faddr.zone)
		fidoaddr=dflt_faddr;
	else if(inetmail_misc&NMAIL_ALLOW) {	/* Internet */
		inet=1;
/*
		if(p)
			*p=0;				/* Chop off address */
*/
		}
	else if(dflt_faddr.zone)
		fidoaddr=dflt_faddr;
	else {
		bprintf("\1n\r\nInvalid NetMail address.\r\n");
		FREE(qwkbuf);
		return; } }
else {
	fidoaddr=atofaddr(p+1); 	/* Get fido address */
	*p=0;					/* Chop off address */
	}


if(!inet && !qnet &&		/* FidoNet */
	((!SYSOP && !(netmail_misc&NMAIL_ALLOW)) || !total_faddrs)) {
    bputs(text[NoNetMailAllowed]);
	FREE(qwkbuf);
    return; }

truncsp(to);			/* Truncate off space */

if(!stricmp(to,"SBBS") && !SYSOP && qnet) {
	FREE(qwkbuf);
	return; }

l=128;					/* Start of message text */

if(qnet || inet) {

	if(into==NULL) {	  /* If name@addr on first line, skip first line */
		while(l<(n*128L) && (uchar)qwkbuf[l]!=0xe3) l++;
        l++; }

	memset(&msg,0,sizeof(smbmsg_t));
    memcpy(msg.hdr.id,"SHD\x1a",4);
    msg.hdr.version=smb_ver();
    msg.hdr.when_imported.time=time(NULL);
    msg.hdr.when_imported.zone=sys_timezone;

	if(fromhub || useron.rest&FLAG('Q')) {
		net=NET_QWK;
		smb_hfield(&msg,SENDERNETTYPE,sizeof(net),&net);
		if(!strncmp((uchar *)qwkbuf+l,"@VIA:",5)) {
			sprintf(str,"%.128s",qwkbuf+l+5);
			cp=strchr(str,0xe3);
			if(cp) *cp=0;
			l+=strlen(str)+1;
			cp=str;
			while(*cp && *cp<=SP) cp++;
			sprintf(senderaddr,"%s/%s"
				,fromhub ? qhub[fromhub-1]->id : useron.alias,cp);
			strupr(senderaddr);
			smb_hfield(&msg,SENDERNETADDR,strlen(senderaddr),senderaddr); }
		else {
			if(fromhub)
				strcpy(senderaddr,qhub[fromhub-1]->id);
			else
				strcpy(senderaddr,useron.alias);
			strupr(senderaddr);
			smb_hfield(&msg,SENDERNETADDR,strlen(senderaddr),senderaddr); }
		sprintf(sender,"%.25s",block+46); }    /* From name */
	else {	/* Not Networked */
		msg.hdr.when_written.zone=sys_timezone;
		sprintf(str,"%u",useron.number);
        smb_hfield(&msg,SENDEREXT,strlen(str),str);
		strcpy(sender,(qnet || inetmail_misc&NMAIL_ALIAS)
			? useron.alias : useron.name);
		}
	truncsp(sender);
	smb_hfield(&msg,SENDER,strlen(sender),sender);
    if(fromhub)
        msg.idx.from=0;
	else
		msg.idx.from=useron.number;
	if(!strncmp((uchar *)qwkbuf+l,"@TZ:",4)) {
		sprintf(str,"%.128s",qwkbuf+l);
		cp=strchr(str,0xe3);
		if(cp) *cp=0;
		l+=strlen(str)+1;
		cp=str+4;
		while(*cp && *cp<=SP) cp++;
		msg.hdr.when_written.zone=(short)ahtoul(cp); }
	else
		msg.hdr.when_written.zone=sys_timezone;

    date.da_mon=((qwkbuf[8]&0xf)*10)+(qwkbuf[9]&0xf);
    date.da_day=((qwkbuf[11]&0xf)*10)+(qwkbuf[12]&0xf);
    date.da_year=((qwkbuf[14]&0xf)*10)+(qwkbuf[15]&0xf);
	if(date.da_year<Y2K_2DIGIT_WINDOW)
		date.da_year+=100;
	date.da_year+=1900;
    curtime.ti_hour=((qwkbuf[16]&0xf)*10)+(qwkbuf[17]&0xf);
    curtime.ti_min=((qwkbuf[19]&0xf)*10)+(qwkbuf[20]&0xf);  /* From QWK time */
    curtime.ti_sec=0;

    msg.hdr.when_written.time=dostounix(&date,&curtime);

	sprintf(str,"%.25s",block+71);              /* Title */
	smb_hfield(&msg,SUBJECT,strlen(str),str);
    strlwr(str);
	msg.idx.subj=crc16(str); }

if(qnet) {

	p++;
	addr=p;
	msg.idx.to=qwk_route(addr,fulladdr);
	if(!fulladdr[0]) {		/* Invalid address, so BOUNCE it */
	/**
		errormsg(WHERE,ERR_CHK,addr,0);
		FREE(qwkbuf);
		smb_freemsgmem(msg);
		return;
	**/
		smb_hfield(&msg,SENDER,strlen(sys_id),sys_id);
		msg.idx.from=0;
		msg.idx.to=useron.number;
		strcpy(to,sender);
		strcpy(fulladdr,senderaddr);
		sprintf(str,"BADADDR: %s",addr);
		smb_hfield(&msg,SUBJECT,strlen(str),str);
		strlwr(str);
		msg.idx.subj=crc16(str);
		net=NET_NONE;
        smb_hfield(&msg,SENDERNETTYPE,sizeof(net),&net);
	}

	smb_hfield(&msg,RECIPIENT,strlen(name),name);
	net=NET_QWK;
	smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(net),&net);

	truncsp(fulladdr);
	smb_hfield(&msg,RECIPIENTNETADDR,strlen(fulladdr),fulladdr);

	bprintf(text[NetMailing],to,fulladdr,sender,sys_id); }

if(inet) {				/* Internet E-mail */

    if(inetmail_cost && !(useron.exempt&FLAG('S'))) {
        if(useron.cdt+useron.freecdt<inetmail_cost) {
            bputs(text[NotEnoughCredits]);
            FREE(qwkbuf);
			smb_freemsgmem(&msg);
            return; }
        sprintf(str,text[NetMailCostContinueQ],inetmail_cost);
        if(noyes(str)) {
            FREE(qwkbuf);
			smb_freemsgmem(&msg);
            return; } }

	net=NET_INTERNET;
	smb_hfield(&msg,RECIPIENT,strlen(name),name);
    msg.idx.to=0;   /* Out-bound NetMail set to 0 */
    smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(net),&net);
	smb_hfield(&msg,RECIPIENTNETADDR,strlen(to),to);

	bprintf(text[NetMailing],name,to
        ,inetmail_misc&NMAIL_ALIAS ? useron.alias : useron.name
		,sys_inetaddr); }

if(qnet || inet) {

    bputs(text[WritingIndx]);

	if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
		errormsg(WHERE,ERR_OPEN,"MAIL",i);
		FREE(qwkbuf);
		smb_freemsgmem(&msg);
		return; }
	sprintf(smb.file,"%sMAIL",data_dir);
	smb.retry_time=smb_retry_time;
	if((i=smb_open(&smb))!=0) {
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i);
		FREE(qwkbuf);
		smb_freemsgmem(&msg);
		return; }

	if(filelength(fileno(smb.shd_fp))<1L) {   /* Create it if it doesn't exist */
		smb.status.max_crcs=mail_maxcrcs;
		smb.status.max_msgs=MAX_SYSMAIL;
		smb.status.max_age=mail_maxage;
		smb.status.attr=SMB_EMAIL;
		if((i=smb_create(&smb))!=0) {
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			errormsg(WHERE,ERR_CREATE,smb.file,i);
			FREE(qwkbuf);
			smb_freemsgmem(&msg);
			return; } }

	length=n*256L;	// Extra big for CRLF xlat, was (n-1L)*256L (03/16/96)


	if(length&0xfff00000UL || !length) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		sprintf(str,"REP msg (%ld)",n);
		errormsg(WHERE,ERR_LEN,str,length);
		FREE(qwkbuf);
		smb_freemsgmem(&msg);
		return; }

	if((i=smb_open_da(&smb))!=0) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i);
		FREE(qwkbuf);
		smb_freemsgmem(&msg);
		return; }
	if(sys_misc&SM_FASTMAIL)
		offset=smb_fallocdat(&smb,length,1);
	else
		offset=smb_allocdat(&smb,length,1);
	smb_close_da(&smb);

	fseek(smb.sdt_fp,offset,SEEK_SET);
	xlat=XLAT_NONE;
	fwrite(&xlat,2,1,smb.sdt_fp);
	m=2;
	for(;l<n*128L && m<length;l++) {
		if(qwkbuf[l]==0 || qwkbuf[l]==LF)
			continue;
		if((uchar)qwkbuf[l]==0xe3) {
			fwrite(crlf,2,1,smb.sdt_fp);
			m+=2;
			continue; }
		fputc(qwkbuf[l],smb.sdt_fp);
		m++; }

	for(ch=0;m<length;m++)			/* Pad out with NULLs */
		fputc(ch,smb.sdt_fp);
	fflush(smb.sdt_fp);

	msg.hdr.offset=offset;

	smb_dfield(&msg,TEXT_BODY,length);

	i=smb_addmsghdr(&smb,&msg,SMB_SELFPACK);
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);

	smb_freemsgmem(&msg);
	if(i) {
		smb_freemsgdat(&smb,offset,length,1);
		errormsg(WHERE,ERR_WRITE,smb.file,i); }
	else {		/* Successful */
		if(inet) {
			if(inetmail_sem[0]) 	 /* update semaphore file */
				if((fido=nopen(inetmail_sem,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
					close(fido);
			if(!(useron.exempt&FLAG('S')))
				subtract_cdt(inetmail_cost); }
		sprintf(str,"Sent %s NetMail to %s (%s) via QWK"
			,qnet ? "QWK":"Internet",name,qnet ? fulladdr : to);
		logline("EN",str); }

	FREE((char *)qwkbuf);
	return; }


/****************************** FidoNet **********************************/

if(!fidoaddr.zone || !netmail_dir[0]) {  // No fido netmail allowed
	bprintf("\1n\r\nInvalid NetMail address.\r\n");
	FREE(qwkbuf);
	return; }

memset(&hdr,0,sizeof(hdr));   /* Initialize header to null */

if(fromhub || useron.rest&FLAG('Q')) {
	sprintf(str,"%.25s",block+46);              /* From */
	truncsp(str);
	sprintf(tmp,"@%s",fromhub ? qhub[fromhub-1]->id : useron.alias);
	strupr(tmp);
	strcat(str,tmp); }
else
	strcpy(str,netmail_misc&NMAIL_ALIAS ? useron.alias : useron.name);
sprintf(hdr.from,"%.35s",str);

sprintf(hdr.to,"%.35s",to);

/* Look-up in nodelist? */

if(netmail_cost && !(useron.exempt&FLAG('S'))) {
	if(useron.cdt+useron.freecdt<netmail_cost) {
		bputs(text[NotEnoughCredits]);
		FREE(qwkbuf);
		return; }
	sprintf(str,text[NetMailCostContinueQ],netmail_cost);
	if(noyes(str)) {
		FREE(qwkbuf);
		return; } }

hdr.destzone	=fidoaddr.zone;
hdr.destnet 	=fidoaddr.net;
hdr.destnode	=fidoaddr.node;
hdr.destpoint	=fidoaddr.point;

for(i=0;i<total_faddrs;i++)
	if(fidoaddr.zone==faddr[i].zone && fidoaddr.net==faddr[i].net)
        break;
if(i==total_faddrs) {
	for(i=0;i<total_faddrs;i++)
		if(fidoaddr.zone==faddr[i].zone)
			break; }
if(i==total_faddrs)
	i=0;
hdr.origzone	=faddr[i].zone;
hdr.orignet 	=faddr[i].net;
hdr.orignode	=faddr[i].node;
hdr.origpoint   =faddr[i].point;

strcpy(str,faddrtoa(faddr[i]));
bprintf(text[NetMailing],hdr.to,faddrtoa(fidoaddr),hdr.from,str);

date.da_mon=((qwkbuf[8]&0xf)*10)+(qwkbuf[9]&0xf);
date.da_day=((qwkbuf[11]&0xf)*10)+(qwkbuf[12]&0xf);
date.da_year=((qwkbuf[14]&0xf)*10)+(qwkbuf[15]&0xf);
if(date.da_year<Y2K_2DIGIT_WINDOW)
	date.da_year+=100;
date.da_year+=1900;
curtime.ti_hour=((qwkbuf[16]&0xf)*10)+(qwkbuf[17]&0xf);
curtime.ti_min=((qwkbuf[19]&0xf)*10)+(qwkbuf[20]&0xf);		/* From QWK time */
curtime.ti_sec=0;
sprintf(hdr.time,"%02u %3.3s %02u  %02u:%02u:%02u"          /* To FidoNet */
	,date.da_day,mon[date.da_mon-1],TM_YEAR(date.da_year-1900)
	,curtime.ti_hour,curtime.ti_min,curtime.ti_sec);

hdr.attr=(FIDO_LOCAL|FIDO_PRIVATE);

if(netmail_misc&NMAIL_CRASH) hdr.attr|=FIDO_CRASH;
if(netmail_misc&NMAIL_HOLD)  hdr.attr|=FIDO_HOLD;
if(netmail_misc&NMAIL_KILL)  hdr.attr|=FIDO_KILLSENT;

sprintf(str,"%.25s",block+71);      /* Title */
truncsp(str);
p=str;
if((SYSOP || useron.exempt&FLAG('F'))
    && !strncmpi(p,"CR:",3)) {     /* Crash over-ride by sysop */
    p+=3;               /* skip CR: */
    if(*p==SP) p++;     /* skip extra space if it exists */
    hdr.attr|=FIDO_CRASH; }

if((SYSOP || useron.exempt&FLAG('F'))
    && !strncmpi(p,"FR:",3)) {     /* File request */
    p+=3;               /* skip FR: */
    if(*p==SP) p++;
    hdr.attr|=FIDO_FREQ; }

if((SYSOP || useron.exempt&FLAG('F'))
    && !strncmpi(p,"RR:",3)) {     /* Return receipt request */
    p+=3;               /* skip RR: */
    if(*p==SP) p++;
    hdr.attr|=FIDO_RRREQ; }

if((SYSOP || useron.exempt&FLAG('F'))
	&& !strncmpi(p,"FA:",3)) {     /* File attachment */
	p+=3;				/* skip FA: */
    if(*p==SP) p++;
	hdr.attr|=FIDO_FILE; }

sprintf(hdr.subj,"%.71s",p);

for(i=1;i;i++) {
	sprintf(str,"%s%u.MSG",netmail_dir,i);
	if(!fexist(str))
		break; }
if(!i) {
	bputs(text[TooManyEmailsToday]);
	return; }
if((fido=nopen(str,O_WRONLY|O_CREAT|O_EXCL))==-1) {
	FREE(qwkbuf);
	errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_EXCL);
	return; }
write(fido,&hdr,sizeof(hdr));

pt_zone_kludge(hdr,fido);

if(netmail_misc&NMAIL_DIRECT) {
	sprintf(str,"\1FLAGS DIR\r\n");
    write(fido,str,strlen(str)); }

l=128L;

if(into==NULL) {	  /* If name@addr on first line, skip first line */
	while(l<n*128L && (uchar)qwkbuf[l]!=0xe3) l++;
	l++; }

while(l<n*128L) {
	if(qwkbuf[l]==1)   /* Ctrl-A, so skip it and the next char */
		l++;
	else if(qwkbuf[l]!=LF) {
		if((uchar)qwkbuf[l]==0xe3) /* QWK cr/lf char converted to hard CR */
			qwkbuf[l]=CR;
		write(fido,(char *)qwkbuf+l,1); }
	l++; }
l=0;
write(fido,&l,1);	/* Null terminator */
close(fido);
FREE((char *)qwkbuf);
if(netmail_sem[0])		/* update semaphore file */
	if((fido=nopen(netmail_sem,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
		close(fido);
if(!(useron.exempt&FLAG('S')))
	subtract_cdt(netmail_cost);
sprintf(str,"Sent NetMail to %s @%s via QWK",hdr.to,faddrtoa(fidoaddr));
logline("EN",str);
}

