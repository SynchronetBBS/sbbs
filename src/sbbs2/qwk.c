#line 1 "QWK.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/****************************************************/
/* Functions that pertain solely to the QWK packets */
/****************************************************/

#include "sbbs.h"
#include "etext.h"
#include "post.h"
#include "qwk.h"

time_t qwkmail_time;

/****************************************************************************/
/* Removes ctrl-a codes from the string 'instr'                             */
/****************************************************************************/
void remove_ctrl_a(char *instr)
{
	char str[512];
	uint i,j,k;

j=strlen(instr);
for(k=i=0;i<j;i++) {
	if(instr[i]==1)
		i++;
	else str[k++]=instr[i]; }
str[k]=0;
strcpy(instr,str);
}


/****************************************************************************/
/* Converts a long to an msbin real number. required for QWK NDX file		*/
/****************************************************************************/
float ltomsbin(long val)
{
	converter t;
	int   sign, exp;	/* sign and exponent */

t.f[0]=(float)val;
sign=t.uc[3]/0x80;
exp=((t.ui[1]>>7)-0x7f+0x81)&0xff;
t.ui[1]=(t.ui[1]&0x7f)|(sign<<7)|(exp<<8);
return(t.f[0]);
}

int route_circ(char *via, char *id)
{
	char str[256],*p,*sp;

strcpy(str,via);
p=str;
while(*p && *p<=SP)
	p++;
while(*p) {
	sp=strchr(p,'/');
	if(sp) *sp=0;
	if(!stricmp(p,id))
		return(1);
	if(!sp)
		break;
	p=sp+1; }
return(0);
}

int qwk_route(char *inaddr, char *fulladdr)
{
	char node[10],str[256],*p;
	int file,i;
	FILE *stream;

fulladdr[0]=0;
sprintf(str,"%.255s",inaddr);
p=strrchr(str,'/');
if(p) p++;
else p=str;
sprintf(node,"%.8s",p);                 /* node = destination node */
truncsp(node);

for(i=0;i<total_qhubs;i++)				/* Check if destination is our hub */
	if(!stricmp(qhub[i]->id,node))
        break;
if(i<total_qhubs) {
	strcpy(fulladdr,node);
	return(0); }

i=matchuser(node);						/* Check if destination is a node */
if(i) {
	getuserrec(i,U_REST,8,str);
	if(ahtoul(str)&FLAG('Q')) {
		strcpy(fulladdr,node);
		return(i); } }

sprintf(node,"%.8s",inaddr);            /* node = next hop */
p=strchr(node,'/');
if(p) *p=0;
truncsp(node);							

if(strchr(inaddr,'/')) {                /* Multiple hops */

	for(i=0;i<total_qhubs;i++)			/* Check if next hop is our hub */
		if(!stricmp(qhub[i]->id,node))
			break;
	if(i<total_qhubs) {
		strcpy(fulladdr,inaddr);
		return(0); }

	i=matchuser(node);					/* Check if next hop is a node */
	if(i) {
		getuserrec(i,U_REST,8,str);
		if(ahtoul(str)&FLAG('Q')) {
			strcpy(fulladdr,inaddr);
			return(i); } } }

p=strchr(node,SP);
if(p) *p=0;

sprintf(str,"%sQNET\\ROUTE.DAT",data_dir);
if((stream=fnopen(&file,str,O_RDONLY))==NULL)
	return(0);

strcat(node,":");
fulladdr[0]=0;
while(!feof(stream)) {
	if(!fgets(str,256,stream))
		break;
	if(!strnicmp(str+9,node,strlen(node))) {
		fclose(stream);
		truncsp(str);
		sprintf(fulladdr,"%s/%s",str+9+strlen(node),inaddr);
		break; } }

fclose(stream);
if(!fulladdr[0])			/* First hop not found in ROUTE.DAT */
	return(0);

sprintf(node,"%.8s",fulladdr);
p=strchr(node,'/');
if(p) *p=0;
truncsp(node);

for(i=0;i<total_qhubs;i++)				/* Check if first hop is our hub */
	if(!stricmp(qhub[i]->id,node))
        break;
if(i<total_qhubs)
	return(0);

i=matchuser(node);						/* Check if first hop is a node */
if(i) {
	getuserrec(i,U_REST,8,str);
	if(ahtoul(str)&FLAG('Q'))
		return(i); }
fulladdr[0]=0;
return(0);
}


/* Via is in format: NODE/NODE/... */
void update_qwkroute(char *via)
{
	static uint total_nodes;
	static char **qwk_node;
	static char **qwk_path;
	static time_t *qwk_time;
	char str[256],*p,*tp,node[9];
	int i,file;
	time_t t;
	FILE *stream;

if(via==NULL) {
	if(!total_nodes)
		return;
	sprintf(str,"%sQNET\\ROUTE.DAT",data_dir);
	if((stream=fnopen(&file,str,O_WRONLY|O_CREAT|O_TRUNC))!=NULL) {
		t=time(NULL);
		t-=(90L*24L*60L*60L);
		for(i=0;i<total_nodes;i++)
			if(qwk_time[i]>t)
				fprintf(stream,"%s %s:%s\r\n"
					,unixtodstr(qwk_time[i],str),qwk_node[i],qwk_path[i]);
		fclose(stream); }
	else
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
    for(i=0;i<total_nodes;i++) {
        FREE(qwk_node[i]);
        FREE(qwk_path[i]); }
	if(qwk_node) {
		FREE(qwk_node);
		qwk_node=NULL; }
	if(qwk_path) {
		FREE(qwk_path);
		qwk_path=NULL; }
	if(qwk_time) {
		FREE(qwk_time);
		qwk_time=NULL; }
    total_nodes=0;
    return; }

if(!total_nodes) {
	sprintf(str,"%sQNET\\ROUTE.DAT",data_dir);
	if((stream=fnopen(&file,str,O_RDONLY))!=NULL) {
		while(!feof(stream)) {
			if(!fgets(str,255,stream))
				break;
			truncsp(str);
			t=dstrtounix(str);
			p=strchr(str,':');
            if(!p) continue;
			*p=0;
			sprintf(node,"%.8s",str+9);
			tp=strchr(node,SP); 		/* change "node bbs:" to "node:" */
			if(tp) *tp=0;
			for(i=0;i<total_nodes;i++)
				if(!stricmp(qwk_node[i],node))
					break;
			if(i<total_nodes && qwk_time[i]>t)
				continue;
			if(i==total_nodes) {
				if((qwk_node=REALLOC(qwk_node,sizeof(char *)*(i+1)))==NULL) {
					errormsg(WHERE,ERR_ALLOC,str,9*(i+1));
					break; }
				if((qwk_path=REALLOC(qwk_path,sizeof(char *)*(i+1)))==NULL) {
					errormsg(WHERE,ERR_ALLOC,str,128*(i+1));
					break; }
				if((qwk_time=REALLOC(qwk_time,sizeof(time_t)*(i+1)))==NULL) {
					errormsg(WHERE,ERR_ALLOC,str,sizeof(time_t)*(i+1));
					break; }
				if((qwk_node[i]=MALLOC(9))==NULL) {
					errormsg(WHERE,ERR_ALLOC,str,9);
					break; }
				if((qwk_path[i]=MALLOC(128))==NULL) {
					errormsg(WHERE,ERR_ALLOC,str,128);
					break; }
				total_nodes++; }
			strcpy(qwk_node[i],node);
			p++;
			while(*p && *p<=SP) p++;
			sprintf(qwk_path[i],"%.127s",p);
			qwk_time[i]=t; }
		fclose(stream); } }

strupr(via);
p=strchr(via,'/');   /* Skip uplink */

while(p && *p) {
	p++;
	sprintf(node,"%.8s",p);
	tp=strchr(node,'/');
	if(tp) *tp=0;
	tp=strchr(node,SP); 		/* no spaces allowed */
	if(tp) *tp=0;
	truncsp(node);
	for(i=0;i<total_nodes;i++)
		if(!stricmp(qwk_node[i],node))
			break;
	if(i==total_nodes) {		/* Not in list */
		if((qwk_node=REALLOC(qwk_node,sizeof(char *)*(total_nodes+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,str,9*(total_nodes+1));
			break; }
		if((qwk_path=REALLOC(qwk_path,sizeof(char *)*(total_nodes+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,str,128*(total_nodes+1));
			break; }
		if((qwk_time=REALLOC(qwk_time,sizeof(time_t)*(total_nodes+1)))
			==NULL) {
			errormsg(WHERE,ERR_ALLOC,str,sizeof(time_t)*(total_nodes+1));
			break; }
		if((qwk_node[total_nodes]=MALLOC(9))==NULL) {
			errormsg(WHERE,ERR_ALLOC,str,9);
			break; }
		if((qwk_path[total_nodes]=MALLOC(128))==NULL) {
			errormsg(WHERE,ERR_ALLOC,str,128);
			break; }
		total_nodes++; }
	sprintf(qwk_node[i],"%.8s",node);
	sprintf(qwk_path[i],"%.*s",(uint)((p-1)-via),via);
	qwk_time[i]=time(NULL);
	p=strchr(p,'/'); }
}

/****************************************************************************/
/* Successful download of QWK packet										*/
/****************************************************************************/
void qwk_success(ulong msgcnt, char bi, char prepack)
{
	char	str[128];
	int 	i;
	ulong	l,msgs,deleted=0;
	mail_t	*mail;
	smbmsg_t msg;

if(!prepack) {
	logline("D-","Downloaded QWK packet");
	posts_read+=msgcnt;

	if(useron.rest&FLAG('Q')) {
		sprintf(str,"%sQNET\\%.8s.OUT\\",data_dir,useron.alias);
		delfiles(str,"*.*"); }

	sprintf(str,"%sFILE\\%04u.QWK",data_dir,useron.number);
	remove(str);

	if(!bi) {
		batch_download(-1);
		delfiles(temp_dir,"*.*"); } }

if(useron.rest&FLAG('Q'))
	useron.qwk|=(QWK_EMAIL|QWK_ALLMAIL|QWK_DELMAIL);
if(useron.qwk&(QWK_EMAIL|QWK_ALLMAIL)) {
	sprintf(smb.file,"%sMAIL",data_dir);
	smb.retry_time=smb_retry_time;
	if((i=smb_open(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,i);
		return; }

	msgs=loadmail(&mail,useron.number,0
		,useron.qwk&QWK_ALLMAIL ? LM_QWK : LM_UNREAD|LM_QWK);

	if((i=smb_locksmbhdr(&smb))!=0) {			  /* Lock the base, so nobody */
		if(msgs)
			FREE(mail);
		smb_close(&smb);
		errormsg(WHERE,ERR_LOCK,smb.file,i);	/* messes with the index */
        return; }

	if((i=smb_getstatus(&smb))!=0) {
		if(msgs)
			FREE(mail);
		smb_close(&smb);
		errormsg(WHERE,ERR_READ,smb.file,i);
		return; }

	/* Mark as READ and DELETE */
	for(l=0;l<msgs;l++) {
		if(mail[l].time>qwkmail_time)
			continue;
		msg.idx.offset=0;
		if(!loadmsg(&msg,mail[l].number))
			continue;
		if(!(msg.hdr.attr&MSG_READ)) {
			if(thisnode.status==NODE_INUSE)
                telluser(msg);
			msg.hdr.attr|=MSG_READ;
			msg.idx.attr=msg.hdr.attr;
			smb_putmsg(&smb,&msg); }
		if(!(msg.hdr.attr&MSG_PERMANENT)
			&& ((msg.hdr.attr&MSG_KILLREAD && msg.hdr.attr&MSG_READ)
			|| (useron.qwk&QWK_DELMAIL))) {
			msg.hdr.attr|=MSG_DELETE;
			msg.idx.attr=msg.hdr.attr;
			if((i=smb_putmsg(&smb,&msg))!=0)
				errormsg(WHERE,ERR_WRITE,smb.file,i);
			else
				deleted++; }
		smb_freemsgmem(&msg);
		smb_unlockmsghdr(&smb,&msg); }

	if(deleted && sys_misc&SM_DELEMAIL)
		delmail(useron.number,MAIL_YOUR);
	smb_close(&smb);
	if(msgs)
		FREE(mail); }

}

/****************************************************************************/
/* QWK mail packet section													*/
/****************************************************************************/
void qwk_sec()
{
	uchar	str[256],tmp2[256],ch,bi=0
			,*AttemptedToDownloadQWKpacket="Attempted to download QWK packet";
	int 	s;
	uint	i,j,k;
	ulong	msgcnt,l;
	time_t	*sav_ptr;
	file_t	fd;

getusrdirs();
fd.dir=total_dirs;
if((sav_ptr=(time_t *)MALLOC(sizeof(time_t)*total_subs))==NULL) {
	errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(time_t)*total_subs);
	return; }
for(i=0;i<total_subs;i++)
	sav_ptr[i]=sub[i]->ptr;
for(i=0;i<total_prots;i++)
	if(prot[i]->bicmd[0] && chk_ar(prot[i]->ar,useron))
		bi++;				/* number of bidirectional protocols configured */
if(useron.rest&FLAG('Q'))
	getusrsubs();
delfiles(temp_dir,"*.*");
while(online) {
	if((useron.misc&(WIP|RIP) || !(useron.misc&EXPERT))
		&& !(useron.rest&FLAG('Q')))
		menu("QWK");
	action=NODE_TQWK;
	ASYNC;
	bputs(text[QWKPrompt]);
	strcpy(str,"?UDCSPQ\r");
	if(bi)
		strcat(str,"B");
	ch=getkeys(str,0);
	if(ch>SP)
		logch(ch,0);
	if(sys_status&SS_ABORT || ch=='Q' || ch==CR)
		break;
	if(ch=='?') {
		if((useron.misc&(WIP|RIP) || !(useron.misc&EXPERT))
			&& !(useron.rest&FLAG('Q')))
			continue;
		menu("QWK");
		continue; }
	if(ch=='S') {
		new_scan_cfg(SUB_NSCAN);
		delfiles(temp_dir,"*.*");
		continue; }
	if(ch=='P') {
		new_scan_ptr_cfg();
		for(i=0;i<total_subs;i++)
			sav_ptr[i]=sub[i]->ptr;
		delfiles(temp_dir,"*.*");
		continue; }
	if(ch=='C') {
		while(online) {
			CLS;
			bputs("\1n\1gQWK Settings:\1n\r\n\r\n");
			bprintf("A) %-30s: %s\r\n"
				,"Ctrl-A Color Codes"
				,useron.qwk&QWK_EXPCTLA
				? "Expand to ANSI" : useron.qwk&QWK_RETCTLA ? "Leave in"
				: "Strip");
			bprintf("T) %-30s: %s\r\n"
				,"Archive Type"
				,useron.tmpext);
			bprintf("E) %-30s: %s\r\n"
				,"Include E-mail Messages"
				,useron.qwk&QWK_EMAIL ? "Un-read Only"
				: useron.qwk&QWK_ALLMAIL ? text[Yes] : text[No]);
			bprintf("I) %-30s: %s\r\n"
				,"Include File Attachments"
				,useron.qwk&QWK_ATTACH ? text[Yes] : text[No]);
			bprintf("D) %-30s: %s\r\n"
				,"Delete E-mail Automatically"
				,useron.qwk&QWK_DELMAIL ? text[Yes]:text[No]);
			bprintf("F) %-30s: %s\r\n"
				,"Include New Files List"
				,useron.qwk&QWK_FILES ? text[Yes]:text[No]);
			bprintf("N) %-30s: %s\r\n"
				,"Include Index Files"
				,useron.qwk&QWK_NOINDEX ? text[No]:text[Yes]);
			bprintf("C) %-30s: %s\r\n"
				,"Include Control Files"
				,useron.qwk&QWK_NOCTRL ? text[No]:text[Yes]);
			bprintf("Y) %-30s: %s\r\n"
				,"Include Messages from You"
				,useron.qwk&QWK_BYSELF ? text[Yes]:text[No]);
			bprintf("Z) %-30s: %s\r\n"
				,"Include Time Zone (TZ)"
				,useron.qwk&QWK_TZ ? text[Yes]:text[No]);
			bprintf("V) %-30s: %s\r\n"
				,"Include Message Path (VIA)"
				,useron.qwk&QWK_VIA ? text[Yes]:text[No]);
			//bprintf("Q) Quit\r\n\r\n");
			bputs(text[UserDefaultsWhich]);
			ch=getkeys("AQEDFIOQTYNCZV",0);
			if(sys_status&SS_ABORT || !ch || ch=='Q')
				break;
			switch(ch) {
				case 'A':
					if(!(useron.qwk&(QWK_EXPCTLA|QWK_RETCTLA)))
						useron.qwk|=QWK_EXPCTLA;
					else if(useron.qwk&QWK_EXPCTLA) {
						useron.qwk&=~QWK_EXPCTLA;
						useron.qwk|=QWK_RETCTLA; }
					else
						useron.qwk&=~(QWK_EXPCTLA|QWK_RETCTLA);
					break;
				case 'T':
					for(i=0;i<total_fcomps;i++)
						uselect(1,i,"Archive Types",fcomp[i]->ext,fcomp[i]->ar);
					s=uselect(0,0,0,0,0);
					if(s>=0) {
						strcpy(useron.tmpext,fcomp[s]->ext);
						putuserrec(useron.number,U_TMPEXT,3,useron.tmpext); }
					break;
				case 'E':
					if(!(useron.qwk&(QWK_EMAIL|QWK_ALLMAIL)))
						useron.qwk|=QWK_EMAIL;
					else if(useron.qwk&QWK_EMAIL) {
						useron.qwk&=~QWK_EMAIL;
						useron.qwk|=QWK_ALLMAIL; }
					else
						useron.qwk&=~(QWK_EMAIL|QWK_ALLMAIL);
					break;
				case 'I':
					useron.qwk^=QWK_ATTACH;
                    break;
				case 'D':
					useron.qwk^=QWK_DELMAIL;
					break;
				case 'F':
					useron.qwk^=QWK_FILES;
					break;
				case 'N':   /* NO IDX files */
					useron.qwk^=QWK_NOINDEX;
                    break;
				case 'C':
					useron.qwk^=QWK_NOCTRL;
                    break;
				case 'Z':
					useron.qwk^=QWK_TZ;
                    break;
				case 'V':
					useron.qwk^=QWK_VIA;
                    break;
				case 'Y':   /* Yourself */
					useron.qwk^=QWK_BYSELF;
					break; }
			putuserrec(useron.number,U_QWK,8,ultoa(useron.qwk,str,16)); }
		delfiles(temp_dir,"*.*");
		continue; }


	if(ch=='B') {   /* Bidirectional QWK and REP packet transfer */
		sprintf(str,"%s%s.QWK",temp_dir,sys_id);
		if(!fexist(str) && !pack_qwk(str,&msgcnt,0)) {
			for(i=0;i<total_subs;i++)
				sub[i]->ptr=sav_ptr[i];
			remove(str);
			last_ns_time=ns_time;
			continue; }
		bprintf(text[UploadingREP],sys_id);
		menu("BIPROT");
		mnemonics(text[ProtocolOrQuit]);
		strcpy(tmp2,"Q");
		for(i=0;i<total_prots;i++)
			if(prot[i]->bicmd[0] && chk_ar(prot[i]->ar,useron)) {
				sprintf(tmp,"%c",prot[i]->mnemonic);
				strcat(tmp2,tmp); }
		ch=getkeys(tmp2,0);
		if(ch=='Q' || sys_status&SS_ABORT) {
			for(i=0;i<total_subs;i++)
				sub[i]->ptr=sav_ptr[i];   /* re-load saved pointers */
			last_ns_time=ns_time;
            continue; }
		for(i=0;i<total_prots;i++)
			if(prot[i]->bicmd[0] && prot[i]->mnemonic==ch
				&& chk_ar(prot[i]->ar,useron))
				break;
		if(i<total_prots) {
			batup_total=1;
			batup_dir[0]=total_dirs;
			sprintf(batup_name[0],"%s.REP",sys_id);
			batdn_total=1;
			batdn_dir[0]=total_dirs;
			sprintf(batdn_name[0],"%s.QWK",sys_id);
			if(!create_batchdn_lst() || !create_batchup_lst()
				|| !create_bimodem_pth()) {
				batup_total=batdn_total=0;
				continue; }
			sprintf(str,"%s%s.QWK",temp_dir,sys_id);
			sprintf(tmp2,"%s.QWK",sys_id);
			padfname(tmp2,fd.name);
			sprintf(str,"%sBATCHDN.LST",node_dir);
			sprintf(tmp2,"%sBATCHUP.LST",node_dir);
			j=protocol(cmdstr(prot[i]->bicmd,str,tmp2,NULL),0);
			batdn_total=batup_total=0;
			if(prot[i]->misc&PROT_DSZLOG) {
				if(!checkprotlog(fd)) {
					logline("D!",AttemptedToDownloadQWKpacket);
					last_ns_time=ns_time;
					for(i=0;i<total_subs;i++)
						sub[i]->ptr=sav_ptr[i]; } /* re-load saved pointers */
				else {
					qwk_success(msgcnt,1,0);
					for(i=0;i<total_subs;i++)
						sav_ptr[i]=sub[i]->ptr; } }
			else if(j) {
				logline("D!",AttemptedToDownloadQWKpacket);
				last_ns_time=ns_time;
				for(i=0;i<total_subs;i++)
					sub[i]->ptr=sav_ptr[i]; }
			else {
				qwk_success(msgcnt,1,0);
				for(i=0;i<total_subs;i++)
					sav_ptr[i]=sub[i]->ptr; }
			sprintf(str,"%s%s.QWK",temp_dir,sys_id);
			remove(str);
			unpack_rep();
			delfiles(temp_dir,"*.*");
			//autohangup();
			}
		else {
			last_ns_time=ns_time;
			for(i=0;i<total_subs;i++)
				sub[i]->ptr=sav_ptr[i]; } }

	else if(ch=='D') {   /* Download QWK Packet of new messages */
		sprintf(str,"%s%s.QWK",temp_dir,sys_id);
		if(!fexist(str) && !pack_qwk(str,&msgcnt,0)) {
			for(i=0;i<total_subs;i++)
				sub[i]->ptr=sav_ptr[i];
			last_ns_time=ns_time;
			remove(str);
			continue; }
		if(online==ON_LOCAL) {			/* Local QWK packet creation */
			bputs(text[EnterPath]);
			if(!getstr(str,60,K_LINE|K_UPPER)) {
				for(i=0;i<total_subs;i++)
					sub[i]->ptr=sav_ptr[i];   /* re-load saved pointers */
				last_ns_time=ns_time;
				continue; }
			backslashcolon(str);
			sprintf(tmp2,"%s%s.QWK",str,sys_id);
			if(fexist(tmp2)) {
				for(i=0;i<10;i++) {
					sprintf(tmp2,"%s%s.QW%d",str,sys_id,i);
					if(!fexist(tmp2))
						break; }
				if(i==10) {
					bputs(text[FileAlreadyThere]);
					last_ns_time=ns_time;
					for(i=0;i<total_subs;i++)
						sub[i]->ptr=sav_ptr[i];
					continue; } }
			sprintf(tmp,"%s%s.QWK",temp_dir,sys_id);
			if(mv(tmp,tmp2,0)) { /* unsuccessful */
				for(i=0;i<total_subs;i++)
					sub[i]->ptr=sav_ptr[i];
				last_ns_time=ns_time; }
			else {
				bprintf(text[FileNBytesSent],tmp2,ultoac(flength(tmp2),tmp));
				qwk_success(msgcnt,0,0);
				for(i=0;i<total_subs;i++)
					sav_ptr[i]=sub[i]->ptr; }
			continue; }

		/***************/
		/* Send Packet */
		/***************/
		menu("DLPROT");
		mnemonics(text[ProtocolOrQuit]);
		strcpy(tmp2,"Q");
		for(i=0;i<total_prots;i++)
			if(prot[i]->dlcmd[0] && chk_ar(prot[i]->ar,useron)) {
				sprintf(tmp,"%c",prot[i]->mnemonic);
				strcat(tmp2,tmp); }
		ungetkey(useron.prot);
		ch=getkeys(tmp2,0);
		if(ch=='Q' || sys_status&SS_ABORT) {
			for(i=0;i<total_subs;i++)
				sub[i]->ptr=sav_ptr[i];   /* re-load saved pointers */
			last_ns_time=ns_time;
			continue; }
		for(i=0;i<total_prots;i++)
			if(prot[i]->dlcmd[0] && prot[i]->mnemonic==ch
				&& chk_ar(prot[i]->ar,useron))
				break;
		if(i<total_prots) {
			sprintf(str,"%s%s.QWK",temp_dir,sys_id);
			sprintf(tmp2,"%s.QWK",sys_id);
			padfname(tmp2,fd.name);
			j=protocol(cmdstr(prot[i]->dlcmd,str,nulstr,NULL),0);
			if(prot[i]->misc&PROT_DSZLOG) {
				if(!checkprotlog(fd)) {
					last_ns_time=ns_time;
					for(i=0;i<total_subs;i++)
						sub[i]->ptr=sav_ptr[i]; } /* re-load saved pointers */
                else {
					qwk_success(msgcnt,0,0);
					for(i=0;i<total_subs;i++)
						sav_ptr[i]=sub[i]->ptr; } }
			else if(j) {
				logline("D!",AttemptedToDownloadQWKpacket);
				last_ns_time=ns_time;
				for(i=0;i<total_subs;i++)
					sub[i]->ptr=sav_ptr[i]; }
			else {
				qwk_success(msgcnt,0,0);
				for(i=0;i<total_subs;i++)
					sav_ptr[i]=sub[i]->ptr; }
			autohangup(); }
		else {	 /* if not valid protocol (hungup?) */
			for(i=0;i<total_subs;i++)
				sub[i]->ptr=sav_ptr[i];
			last_ns_time=ns_time; } }

	else if(ch=='U') { /* Upload REP Packet */
/*
		if(useron.rest&FLAG('Q') && useron.rest&FLAG('P')) {
			bputs(text[R_Post]);
			continue; }
*/

		delfiles(temp_dir,"*.*");
		bprintf(text[UploadingREP],sys_id);
		for(k=0;k<total_fextrs;k++)
			if(!stricmp(fextr[k]->ext,useron.tmpext)
				&& chk_ar(fextr[k]->ar,useron))
				break;
		if(k>=total_fextrs) {
			bputs(text[QWKExtractionFailed]);
			errorlog("Couldn't extract REP packet - configuration error");
			continue; }

		if(online==ON_LOCAL) {		/* Local upload of rep packet */
			bputs(text[EnterPath]);
			if(!getstr(str,60,K_LINE|K_UPPER))
				continue;
			backslashcolon(str);
			sprintf(tmp,"%s.REP",sys_id);
			strcat(str,tmp);
			sprintf(tmp,"%s%s.REP",temp_dir,sys_id);
			if(!mv(str,tmp,0))
				unpack_rep();
			delfiles(temp_dir,"*.*");
            continue; }

		/******************/
		/* Receive Packet */
		/******************/
		menu("ULPROT");
		mnemonics(text[ProtocolOrQuit]);
		strcpy(tmp2,"Q");
		for(i=0;i<total_prots;i++)
			if(prot[i]->ulcmd[0] && chk_ar(prot[i]->ar,useron)) {
				sprintf(tmp,"%c",prot[i]->mnemonic);
				strcat(tmp2,tmp); }
		ch=getkeys(tmp2,0);
		if(ch=='Q' || sys_status&SS_ABORT)
			continue;
		for(i=0;i<total_prots;i++)
			if(prot[i]->ulcmd[0] && prot[i]->mnemonic==ch
				&& chk_ar(prot[i]->ar,useron))
				break;
		if(i>=total_prots)	/* This shouldn't happen */
			continue;
		sprintf(str,"%s%s.REP",temp_dir,sys_id);
		protocol(cmdstr(prot[i]->ulcmd,str,nulstr,NULL),0);
		unpack_rep();
		delfiles(temp_dir,"*.*");
		//autohangup();
		} }
delfiles(temp_dir,"*.*");
FREE(sav_ptr);
}

void qwksetptr(uint subnum, char *buf, int reset)
{
	long	l;
	ulong	last;

if(buf[2]=='/' && buf[5]=='/') {    /* date specified */
	l=dstrtounix(buf);
	sub[subnum]->ptr=getmsgnum(subnum,l);
	return; }
l=atol(buf);
if(l>=0)							  /* ptr specified */
	sub[subnum]->ptr=l;
else if(l) {						  /* relative ptr specified */
	getlastmsg(subnum,&last,0);
	if(-l>last)
		sub[subnum]->ptr=0;
	else
		sub[subnum]->ptr=last+l; }
else if(reset)
	getlastmsg(subnum,&sub[subnum]->ptr,0);
}


/****************************************************************************/
/* Process a QWK Config line												*/
/****************************************************************************/
void qwkcfgline(char *buf,uint subnum)
{
	char	str[128];
	int 	x,y;
	long	l;
	ulong	qwk=useron.qwk,last;
	file_t	f;

sprintf(str,"%.25s",buf);
strupr(str);
bprintf("\1n\r\n\1b\1hQWK Control [\1c%s\1b]: \1g%s\r\n"
	,subnum==INVALID_SUB ? "Mail":sub[subnum]->qwkname,str);

if(subnum!=INVALID_SUB) {					/* Only valid in sub-boards */

	if(!strncmp(str,"DROP ",5)) {              /* Drop from new-scan */
		l=atol(str+5);
		if(!l)
			sub[subnum]->misc&=~SUB_NSCAN;
		else {
			x=l/1000;
			y=l-(x*1000);
			if(x>=usrgrps || y>=usrsubs[x]) {
				bprintf(text[QWKInvalidConferenceN],l);
				sprintf(str,"Invalid conference number %lu",l);
				logline("Q!",str); }
			else
				sub[usrsub[x][y]]->misc&=~SUB_NSCAN; }
		return; }

	if(!strncmp(str,"ADD YOURS ",10)) {               /* Add to new-scan */
		sub[subnum]->misc|=(SUB_NSCAN|SUB_YSCAN);
		qwksetptr(subnum,str+10,0);
		return; }

	else if(!strncmp(str,"YOURS ",6)) {
		sub[subnum]->misc|=(SUB_NSCAN|SUB_YSCAN);
		qwksetptr(subnum,str+6,0);
		return; }

	else if(!strncmp(str,"ADD ",4)) {               /* Add to new-scan */
        sub[subnum]->misc|=SUB_NSCAN;
        sub[subnum]->misc&=~SUB_YSCAN;
		qwksetptr(subnum,str+4,0);
		return; }

	if(!strncmp(str,"RESET ",6)) {             /* set msgptr */
		qwksetptr(subnum,str+6,1);
		return; }

	if(!strncmp(str,"SUBPTR ",7)) {
		qwksetptr(subnum,str+7,1);
		return; }
	}

if(!strncmp(str,"RESETALL ",9)) {              /* set all ptrs */
	for(x=0;x<usrgrps;x++)
		for(y=0;y<usrsubs[x];y++)
			if(sub[usrsub[x][y]]->misc&SUB_NSCAN)
				qwksetptr(usrsub[x][y],str+9,1); }

else if(!strncmp(str,"ALLPTR ",7)) {              /* set all ptrs */
	for(x=0;x<usrgrps;x++)
		for(y=0;y<usrsubs[x];y++)
			if(sub[usrsub[x][y]]->misc&SUB_NSCAN)
				qwksetptr(usrsub[x][y],str+7,1); }

else if(!strncmp(str,"FILES ",6)) {                 /* files list */
	if(!strncmp(str+6,"ON ",3))
		useron.qwk|=QWK_FILES;
	else if(str[8]=='/' && str[11]=='/') {      /* set scan date */
		useron.qwk|=QWK_FILES;
		ns_time=dstrtounix(str+6); }
	else
		useron.qwk&=~QWK_FILES; }

else if(!strncmp(str,"OWN ",4)) {                   /* message from you */
	if(!strncmp(str+4,"ON ",3))
		useron.qwk|=QWK_BYSELF;
	else
		useron.qwk&=~QWK_BYSELF;
	return; }

else if(!strncmp(str,"NDX ",4)) {                   /* include indexes */
	if(!strncmp(str+4,"OFF ",4))
		useron.qwk|=QWK_NOINDEX;
	else
		useron.qwk&=~QWK_NOINDEX; }

else if(!strncmp(str,"CONTROL ",8)) {               /* exclude ctrl files */
	if(!strncmp(str+8,"OFF ",4))
		useron.qwk|=QWK_NOCTRL;
	else
		useron.qwk&=~QWK_NOCTRL; }

else if(!strncmp(str,"VIA ",4)) {                   /* include @VIA: */
	if(!strncmp(str+4,"ON  ",3))
		useron.qwk|=QWK_VIA;
	else
		useron.qwk&=~QWK_VIA; }

else if(!strncmp(str,"TZ ",3)) {                    /* include @TZ: */
	if(!strncmp(str+3,"ON ",3))
		useron.qwk|=QWK_TZ;
    else
		useron.qwk&=~QWK_TZ; }

else if(!strncmp(str,"ATTACH ",7)) {                /* file attachments */
	if(!strncmp(str+7,"ON ",3))
        useron.qwk|=QWK_ATTACH;
    else
        useron.qwk&=~QWK_ATTACH; }

else if(!strncmp(str,"DELMAIL ",8)) {               /* delete mail */
	if(!strncmp(str+8,"ON ",3))
		useron.qwk|=QWK_DELMAIL;
	else
		useron.qwk&=~QWK_DELMAIL; }

else if(!strncmp(str,"CTRL-A ",7)) {                /* Ctrl-a codes  */
	if(!strncmp(str+7,"KEEP ",5)) {
		useron.qwk|=QWK_RETCTLA;
		useron.qwk&=~QWK_EXPCTLA; }
	else if(!strncmp(str+7,"EXPAND ",7)) {
		useron.qwk|=QWK_EXPCTLA;
		useron.qwk&=~QWK_RETCTLA; }
	else
		useron.qwk&=~(QWK_EXPCTLA|QWK_RETCTLA); }

else if(!strncmp(str,"MAIL ",5)) {                  /* include e-mail */
	if(!strncmp(str+5,"ALL ",4)) {
		useron.qwk|=QWK_ALLMAIL;
		useron.qwk&=~QWK_EMAIL; }
	else if(!strncmp(str+5,"ON ",3)) {
		useron.qwk|=QWK_EMAIL;
		useron.qwk&=~QWK_ALLMAIL; }
	else
		useron.qwk&=~(QWK_ALLMAIL|QWK_EMAIL); }

else if(!strncmp(str,"FREQ ",5)) {                  /* file request */
	padfname(str+5,f.name);
	strupr(f.name);
	for(x=0;x<usrlibs;x++) {
		for(y=0;y<usrdirs[x];y++)
			if(findfile(usrdir[x][y],f.name))
				break;
		if(y<usrdirs[x])
			break; }
	if(x>=usrlibs) {
		bprintf("\r\n%s",f.name);
		bputs(text[FileNotFound]); }
	else {
		f.dir=usrdir[x][y];
		getfileixb(&f);
		f.size=0;
        getfiledat(&f);
		if(f.size==-1L)
			bprintf(text[FileIsNotOnline],f.name);
		else
			addtobatdl(f); } }

else bputs("\1r\1h\1iUnrecognized Control Command!\1n\r\n");

if(qwk!=useron.qwk)
	putuserrec(useron.number,U_QWK,8,ultoa(useron.qwk,tmp,16));
}


