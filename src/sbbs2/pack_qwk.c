#line 1 "PACK_QWK.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "qwk.h"
#include "post.h"
#include "etext.h"

/****************************************************************************/
/* Creates QWK packet, returning 1 if successful, 0 if not. 				*/
/****************************************************************************/
char pack_qwk(char *packet, ulong *msgcnt, int prepack)
{
	uchar	str[256],tmp2[256],ch,*p;
	uchar	HUGE16 *qwkbuf;
	int 	file,mode;
	uint	i,j,k,n,conf;
	long	l,size,msgndx;
	ulong	totalcdt,totalsize,totaltime,lastmsg
			,mailmsgs=0,files,submsgs,msgs,posts,netfiles=0,preqwk=0;
	float	f;	/* Sparky is responsible */
	time_t	start;
	node_t	node;
	mail_t	*mail;
	post_t	HUGE16 *post;
	FILE	*stream,*qwk,*personal,*ndx;
    struct  ffblk ff;
	smbmsg_t msg;

delfiles(temp_dir,"*.*");
sprintf(str,"%sFILE\\%04u.QWK",data_dir,useron.number);
if(fexist(str)) {
	for(k=0;k<total_fextrs;k++)
		if(!stricmp(fextr[k]->ext,useron.tmpext)
			&& chk_ar(fextr[k]->ar,useron))
			break;
	if(k>=total_fextrs)
		k=0;
	i=external(cmdstr(fextr[k]->cmd,str,"*.*",NULL),EX_OUTL|EX_OUTR);
	if(!i)
        preqwk=1; }

if(useron.rest&FLAG('Q') && useron.qwk&QWK_RETCTLA)
	useron.qwk|=(QWK_NOINDEX|QWK_NOCTRL|QWK_VIA|QWK_TZ);

if(useron.qwk&QWK_EXPCTLA)
	mode=A_EXPAND;
else if(useron.qwk&QWK_RETCTLA)
	mode=A_LEAVE;
else mode=0;
if(useron.qwk&QWK_TZ)
	mode|=TZ;
if(useron.qwk&QWK_VIA)
	mode|=VIA;
(*msgcnt)=0L;
if(!prepack && !(useron.qwk&QWK_NOCTRL)) {
	/***************************/
	/* Create CONTROL.DAT file */
	/***************************/
	sprintf(str,"%sCONTROL.DAT",temp_dir);
	if((stream=fnopen(&file,str,O_WRONLY|O_CREAT|O_TRUNC))==NULL) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return(0); }

	now=time(NULL);
	unixtodos(now,&date,&curtime);

	fprintf(stream,"%s\r\n%s\r\n%s\r\n%s, Sysop\r\n0000,%s\r\n"
		"%02u-%02u-%u,%02u:%02u:%02u\r\n"
		,sys_name
		,sys_location
		,node_phone
		,sys_op
		,sys_id
		,date.da_mon,date.da_day,date.da_year
		,curtime.ti_hour,curtime.ti_min,curtime.ti_sec);
	k=0;
	for(i=0;i<usrgrps;i++)
		for(j=0;j<usrsubs[i];j++)
			k++;	/* k is how many subs */
	fprintf(stream,"%s\r\n\r\n0\r\n0\r\n%u\r\n",useron.alias,k);
	fprintf(stream,"0\r\nE-mail\r\n");   /* first conference is e-mail */
	for(i=0;i<usrgrps;i++)
		for(j=0;j<usrsubs[i];j++)
			fprintf(stream,"%u\r\n%s\r\n"
				,sub[usrsub[i][j]]->qwkconf ? sub[usrsub[i][j]]->qwkconf
				: ((i+1)*1000)+j+1,sub[usrsub[i][j]]->qwkname);
	fprintf(stream,"HELLO\r\nBBSNEWS\r\nGOODBYE\r\n");
	fclose(stream);
	/***********************/
	/* Create DOOR.ID File */
	/***********************/
	sprintf(str,"%sDOOR.ID",temp_dir);
	if((stream=fnopen(&file,str,O_WRONLY|O_CREAT|O_TRUNC))==NULL) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return(0); }
	p="CONTROLTYPE = ";
	fprintf(stream,"DOOR = %s\r\nVERSION = %s\r\n"
		"SYSTEM = %s v%s\r\n"
		"CONTROLNAME = SBBS\r\n"
		"%sADD\r\n"
		"%sDROP\r\n"
		"%sYOURS\r\n"
		"%sRESET\r\n"
		"%sRESETALL\r\n"
		"%sFILES\r\n"
		"%sATTACH\r\n"
		"%sOWN\r\n"
		"%sMAIL\r\n"
		"%sDELMAIL\r\n"
		"%sCTRL-A\r\n"
		"%sFREQ\r\n"
		"%sNDX\r\n"
		"%sTZ\r\n"
		"%sVIA\r\n"
		"%sCONTROL\r\n"
		"MIXEDCASE = YES\r\n"
		,decrypt(Synchronet,0)
		,VERSION
		,decrypt(Synchronet,0)
		,VERSION
		,p,p,p,p
		,p,p,p,p
		,p,p,p,p
		,p,p,p,p
		);
	fclose(stream);
	if(useron.rest&FLAG('Q')) {
		/***********************/
		/* Create NETFLAGS.DAT */
		/***********************/
		sprintf(str,"%sNETFLAGS.DAT",temp_dir);
		if((stream=fnopen(&file,str,O_WRONLY|O_CREAT))==NULL) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT);
			return(0); }
		ch=1;						/* Net enabled */
		if(usrgrps)
			for(i=0;i<(usrgrps*1000)+usrsubs[usrgrps-1];i++)
				fputc(ch,stream);
		fclose(stream); }
	} /* !prepack */

/****************************************************/
/* Create MESSAGES.DAT, write header and leave open */
/****************************************************/
sprintf(str,"%sMESSAGES.DAT",temp_dir);
if((qwk=fnopen(&file,str,O_CREAT|O_WRONLY|O_TRUNC))==NULL) {
	errormsg(WHERE,ERR_OPEN,str,O_CREAT|O_WRONLY|O_TRUNC);
	return(0); }
l=filelength(file);
if(!l) {
	fprintf(qwk,"%-128s",decrypt(QWKheader,0));
	msgndx=1; }
else
	msgndx=l/128L;
fseek(qwk,0,SEEK_END);
sprintf(str,"%sNEWFILES.DAT",temp_dir);
remove(str);
if(!(useron.rest&FLAG('T')) && useron.qwk&QWK_FILES)
	files=create_filelist("NEWFILES.DAT",FL_ULTIME);
else
	files=0;

start=time(NULL);

if(useron.rest&FLAG('Q'))
	useron.qwk|=(QWK_EMAIL|QWK_ALLMAIL|QWK_DELMAIL);

if(!(useron.qwk&QWK_NOINDEX)) {
    sprintf(str,"%sPERSONAL.NDX",temp_dir);
    if((personal=fnopen(&file,str,O_CREAT|O_WRONLY|O_APPEND))==NULL) {
        fclose(qwk);
        errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
        return(0); }
    fseek(personal,0L,SEEK_END); }
else
    personal=NULL;

if(useron.qwk&(QWK_EMAIL|QWK_ALLMAIL) && !prepack) {
	sprintf(smb.file,"%sMAIL",data_dir);
	smb.retry_time=smb_retry_time;
	if((i=smb_open(&smb))!=0) {
        fclose(qwk);
		if(personal)
			fclose(personal);
		errormsg(WHERE,ERR_OPEN,smb.file,i);
		return(0); }

	/***********************/
	/* Pack E-mail, if any */
	/***********************/
	qwkmail_time=time(NULL);
	mailmsgs=loadmail(&mail,useron.number,0,useron.qwk&QWK_ALLMAIL ? LM_QWK
		: LM_UNREAD|LM_QWK);
	if(mailmsgs && !(sys_status&SS_ABORT)) {
		bputs(text[QWKPackingEmail]);
		if(!(useron.qwk&QWK_NOINDEX)) {
			sprintf(str,"%s000.NDX",temp_dir);
			if((ndx=fnopen(&file,str,O_CREAT|O_WRONLY|O_APPEND))==NULL) {
				fclose(qwk);
				if(personal)
					fclose(personal);
				smb_close(&smb);
				errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
				FREE(mail);
				return(0); }
			fseek(ndx,0L,SEEK_END); }
		else
			ndx=NULL;

		if(useron.rest&FLAG('Q'))
			mode|=TO_QNET;
		else
			mode&=~TO_QNET;

		for(l=0;l<mailmsgs;l++) {
			bprintf("\b\b\b\b\b\b\b\b\b\b\b\b%4lu of %-4lu"
				,l+1,mailmsgs);

			msg.idx.offset=mail[l].offset;
			if(!loadmsg(&msg,mail[l].number))
				continue;

			if(msg.hdr.auxattr&MSG_FILEATTACH && useron.qwk&QWK_ATTACH) {
				sprintf(str,"%sFILE\\%04u.IN\\%s"
					,data_dir,useron.number,msg.subj);
				sprintf(tmp,"%s%s",temp_dir,msg.subj);
				if(fexist(str) && !fexist(tmp))
					mv(str,tmp,1); }

			size=msgtoqwk(msg,qwk,mode,INVALID_SUB,0);
			smb_unlockmsghdr(&smb,&msg);
			smb_freemsgmem(&msg);
			if(ndx) {
				msgndx++;
				f=ltomsbin(msgndx); 	/* Record number */
				ch=0;					/* Sub number, not used */
				if(personal) {
					fwrite(&f,4,1,personal);
					fwrite(&ch,1,1,personal); }
				fwrite(&f,4,1,ndx);
				fwrite(&ch,1,1,ndx);
				msgndx+=size/128L; } }
		bprintf(text[QWKPackedEmail],mailmsgs);
		if(ndx)
			fclose(ndx); }
	smb_close(&smb);					/* Close the e-mail */
	if(mailmsgs)
		FREE(mail);
	}

/*********************/
/* Pack new messages */
/*********************/
for(i=0;i<usrgrps;i++) {
	for(j=0;j<usrsubs[i] && !msgabort();j++)
		if(sub[usrsub[i][j]]->misc&SUB_NSCAN
			|| (!(useron.rest&FLAG('Q'))
			&& sub[usrsub[i][j]]->misc&SUB_FORCED)) {
			if(!chk_ar(sub[usrsub[i][j]]->read_ar,useron))
				continue;
			lncntr=0;						/* defeat pause */
			if(useron.rest&FLAG('Q') && !(sub[usrsub[i][j]]->misc&SUB_QNET))
				continue;	/* QWK Net Node and not QWK networked, so skip */

			msgs=getlastmsg(usrsub[i][j],&lastmsg,0);
			if(!msgs || lastmsg<=sub[usrsub[i][j]]->ptr) { /* no msgs */
				if(sub[usrsub[i][j]]->ptr>lastmsg)	{ /* corrupted ptr */
					sub[usrsub[i][j]]->ptr=lastmsg; /* so fix automatically */
					sub[usrsub[i][j]]->last=lastmsg; }
				bprintf(text[NScanStatusFmt]
					,grp[sub[usrsub[i][j]]->grp]->sname
					,sub[usrsub[i][j]]->lname,0L,msgs);
				continue; }

			sprintf(smb.file,"%s%s"
				,sub[usrsub[i][j]]->data_dir,sub[usrsub[i][j]]->code);
			smb.retry_time=smb_retry_time;
			if((k=smb_open(&smb))!=0) {
				errormsg(WHERE,ERR_OPEN,smb.file,k);
				continue; }

			k=0;
			if(useron.qwk&QWK_BYSELF)
				k|=LP_BYSELF;
			if(!(sub[usrsub[i][j]]->misc&SUB_YSCAN))
				k|=LP_OTHERS;
			post=loadposts(&posts,usrsub[i][j],sub[usrsub[i][j]]->ptr,k);

			bprintf(text[NScanStatusFmt]
				,grp[sub[usrsub[i][j]]->grp]->sname
				,sub[usrsub[i][j]]->lname,posts,msgs);
			if(!posts)	{ /* no new messages */
				smb_close(&smb);
				continue; }
			bputs(text[QWKPackingSubboard]);	
			submsgs=0;
			conf=sub[usrsub[i][j]]->qwkconf;
			if(!conf)
				conf=((i+1)*1000)+j+1;

			if(!(useron.qwk&QWK_NOINDEX)) {
				sprintf(str,"%s%u.NDX",temp_dir,conf);
                if((ndx=fnopen(&file,str,O_CREAT|O_WRONLY|O_APPEND))==NULL) {
					fclose(qwk);
					if(personal)
						fclose(personal);
					smb_close(&smb);
					errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
					LFREE(post);
					return(0); }
				fseek(ndx,0L,SEEK_END); }
			else
				ndx=NULL;

			for(l=0;l<posts && !msgabort();l++) {
				bprintf("\b\b\b\b\b%-5lu",l+1);

				sub[usrsub[i][j]]->ptr=post[l].number;	/* set ptr */
				sub[usrsub[i][j]]->last=post[l].number; /* set last read */

				msg.idx.offset=post[l].offset;
				if(!loadmsg(&msg,post[l].number))
					continue;

				if(useron.rest&FLAG('Q')) {
					if(msg.from_net.type && msg.from_net.type!=NET_QWK &&
						!(sub[usrsub[i][j]]->misc&SUB_GATE)) { /* From other */
						smb_freemsgmem(&msg);			 /* net, don't gate */
						smb_unlockmsghdr(&smb,&msg);
						continue; }
					mode|=(TO_QNET|TAGLINE);
					if(msg.from_net.type==NET_QWK) {
						mode&=~TAGLINE;
						if(route_circ(msg.from_net.addr,useron.alias)
							|| !strnicmp(msg.subj,"NE:",3)) {
							smb_freemsgmem(&msg);
							smb_unlockmsghdr(&smb,&msg);
							continue; } } }
				else
					mode&=~(TAGLINE|TO_QNET);

				size=msgtoqwk(msg,qwk,mode,usrsub[i][j],conf);
				smb_unlockmsghdr(&smb,&msg);

				if(ndx) {
					msgndx++;
					f=ltomsbin(msgndx); 	/* Record number */
					ch=0;					/* Sub number, not used */
					if(personal
						&& (!stricmp(msg.to,useron.alias)
							|| !stricmp(msg.to,useron.name))) {
						fwrite(&f,4,1,personal);
						fwrite(&ch,1,1,personal); }
					fwrite(&f,4,1,ndx);
					fwrite(&ch,1,1,ndx);
					msgndx+=size/128L; }

				smb_freemsgmem(&msg);
				(*msgcnt)++;
				submsgs++;
				if(max_qwkmsgs
					&& !(useron.rest&FLAG('Q')) && (*msgcnt)>=max_qwkmsgs) {
					bputs(text[QWKmsgLimitReached]);
					break; } }
			if(!(sys_status&SS_ABORT))
				bprintf(text[QWKPackedSubboard],submsgs,(*msgcnt));
			if(ndx) {
				fclose(ndx);
				sprintf(str,"%s%u.NDX",temp_dir,conf);
				if(!flength(str))
					remove(str); }
			smb_close(&smb);
			LFREE(post);
			if(l<posts)
				break; }
	if(j<usrsubs[i]) /* if sub aborted, abort all */
		break; }

if((*msgcnt)+mailmsgs && time(NULL)-start)
	bprintf("\r\n\r\n\1n\1hPacked %lu messages in %lu seconds "
		"(%lu messages/second)."
		,(*msgcnt)+mailmsgs,time(NULL)-start
		,((*msgcnt)+mailmsgs)/(time(NULL)-start));

fclose(qwk);			/* close MESSAGE.DAT */
if(personal) {
	fclose(personal);		 /* close PERSONAL.NDX */
	sprintf(str,"%sPERSONAL.NDX",temp_dir);
	if(!flength(str))
		remove(str); }
CRLF;

if(!prepack && (sys_status&SS_ABORT || !online))
    return(0);

if(!prepack && useron.rest&FLAG('Q')) { /* If QWK Net node, check for files */
    sprintf(str,"%sQNET\\%s.OUT\\*.*",data_dir,useron.alias);
    i=findfirst(str,&ff,0);
    while(!i) {                 /* Move files into temp dir */
        sprintf(str,"%sQNET\\%s.OUT\\%s",data_dir,useron.alias,ff.ff_name);
		strupr(str);
        sprintf(tmp2,"%s%s",temp_dir,ff.ff_name);
		lncntr=0;	/* Default pause */
		bprintf(text[RetrievingFile],str);
		if(!mv(str,tmp2,1))
			netfiles++;
		i=findnext(&ff); }
	if(netfiles)
		CRLF; }

if(batdn_total) {
	for(i=0,totalcdt=0;i<batdn_total;i++)
			totalcdt+=batdn_cdt[i];
	if(!(useron.exempt&FLAG('D'))
		&& totalcdt>useron.cdt+useron.freecdt) {
		bprintf(text[YouOnlyHaveNCredits]
			,ultoac(useron.cdt+useron.freecdt,tmp)); }
	else {
		for(i=0,totalsize=totaltime=0;i<batdn_total;i++) {
			totalsize+=batdn_size[i];
			if(!(dir[batdn_dir[i]]->misc&DIR_TFREE) && cur_cps)
				totaltime+=batdn_size[i]/(ulong)cur_cps; }
		if(!(useron.exempt&FLAG('T')) && !SYSOP && totaltime>timeleft)
			bputs(text[NotEnoughTimeToDl]);
		else {
			for(i=0;i<batdn_total;i++) {
				lncntr=0;
				unpadfname(batdn_name[i],tmp);
				sprintf(tmp2,"%s%s",temp_dir,tmp);
				if(!fexist(tmp2)) {
					seqwait(dir[batdn_dir[i]]->seqdev);
					bprintf(text[RetrievingFile],tmp);
					sprintf(str,"%s%s"
						,batdn_alt[i]>0 && batdn_alt[i]<=altpaths
						? altpath[batdn_alt[i]-1]
						: dir[batdn_dir[i]]->path
						,tmp);
					mv(str,tmp2,1); /* copy the file to temp dir */
					getnodedat(node_num,&thisnode,1);
					thisnode.aux=0xfe;
					putnodedat(node_num,thisnode);
					CRLF; } } } } }

if(!(*msgcnt) && !mailmsgs && !files && !netfiles && !batdn_total
	&& (prepack || !preqwk)) {
	bputs(text[QWKNoNewMessages]);
	return(0); }

if(!prepack && !(useron.rest&FLAG('Q'))) {      /* Don't include in network */
	/***********************/					/* packets */
	/* Copy QWK Text files */
	/***********************/
	sprintf(str,"%sQWK\\HELLO",text_dir);
	if(fexist(str)) {
		sprintf(tmp2,"%sHELLO",temp_dir);
		mv(str,tmp2,1); }
	sprintf(str,"%sQWK\\BBSNEWS",text_dir);
	if(fexist(str)) {
		sprintf(tmp2,"%sBBSNEWS",temp_dir);
		mv(str,tmp2,1); }
	sprintf(str,"%sQWK\\GOODBYE",text_dir);
	if(fexist(str)) {
		sprintf(tmp2,"%sGOODBYE",temp_dir);
		mv(str,tmp2,1); }
	sprintf(str,"%sQWK\\BLT-*.*",text_dir);
	i=findfirst(str,&ff,0);
	while(!i) { 				/* Copy BLT-*.* files */
		padfname(ff.ff_name,str);
		if(isdigit(str[4]) && isdigit(str[9])) {
			sprintf(str,"%sQWK\\%s",text_dir,ff.ff_name);
			sprintf(tmp2,"%s%s",temp_dir,ff.ff_name);
			mv(str,tmp2,1); }
		i=findnext(&ff); } }

if(prepack) {
	for(i=1;i<=sys_nodes;i++) {
		getnodedat(i,&node,0);
		if((node.status==NODE_INUSE || node.status==NODE_QUIET
			|| node.status==NODE_LOGON) && node.useron==useron.number)
			break; }
	if(i<=sys_nodes)	/* Don't pre-pack with user online */
		return(0); }

/*******************/
/* Compress Packet */
/*******************/
sprintf(tmp2,"%s*.*",temp_dir);
i=external(cmdstr(temp_cmd(),packet,tmp2,NULL),EX_OUTL|EX_OUTR);
if(!fexist(packet)) {
	bputs(text[QWKCompressionFailed]);
	if(i)
		errormsg(WHERE,ERR_EXEC,cmdstr(temp_cmd(),packet,tmp2,NULL),i);
	else
		errorlog("Couldn't compress QWK packet");
	return(0); }

if(prepack) 		/* Early return if pre-packing */
	return(1);

l=flength(packet);
sprintf(str,"%s.QWK",sys_id);
bprintf(text[FiFilename],str);
bprintf(text[FiFileSize],ultoac(l,tmp));
if(l>0L && cur_cps)
	i=l/(ulong)cur_cps;
else
	i=0;
bprintf(text[FiTransferTime],sectostr(i,tmp));
CRLF;
if(!(useron.exempt&FLAG('T')) && i>timeleft) {
	bputs(text[NotEnoughTimeToDl]);
	return(0); }

if(useron.rest&FLAG('Q')) {
	sprintf(str,"%s.QWK",sys_id);
	sprintf(tmp,"%s*.*",temp_dir);
	i=findfirst(tmp,&ff,0);
	while(!i) {
		if(stricmp(str,ff.ff_name)) {
			sprintf(tmp,"%s%s",temp_dir,ff.ff_name);
			remove(tmp); }
		i=findnext(&ff); } }

return(1);
}
