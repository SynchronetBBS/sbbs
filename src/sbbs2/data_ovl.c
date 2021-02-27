#line 1 "DATA_OVL.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

/****************************************************************************/
/* Writes into user.number's slot in USER.DAT data in structure 'user'      */
/* Called from functions newuser, useredit and main                         */
/****************************************************************************/
void putuserdat(user_t user)
{
    int i,file;
    char userdat[U_LEN+1],str[U_LEN+1];
    node_t node;

if(!user.number) {
    errormsg(WHERE,ERR_CHK,"user number",0);
    return; }
memset(userdat,ETX,U_LEN);
putrec(userdat,U_ALIAS,LEN_ALIAS+5,user.alias);
putrec(userdat,U_NAME,LEN_NAME,user.name);
putrec(userdat,U_HANDLE,LEN_HANDLE,user.handle);
putrec(userdat,U_HANDLE+LEN_HANDLE,2,crlf);

putrec(userdat,U_NOTE,LEN_NOTE,user.note);
putrec(userdat,U_COMP,LEN_COMP,user.comp);
putrec(userdat,U_COMP+LEN_COMP,2,crlf);

putrec(userdat,U_COMMENT,LEN_COMMENT,user.comment);
putrec(userdat,U_COMMENT+LEN_COMMENT,2,crlf);

putrec(userdat,U_NETMAIL,LEN_NETMAIL,user.netmail);
putrec(userdat,U_NETMAIL+LEN_NETMAIL,2,crlf);

putrec(userdat,U_ADDRESS,LEN_ADDRESS,user.address);
putrec(userdat,U_LOCATION,LEN_LOCATION,user.location);
putrec(userdat,U_ZIPCODE,LEN_ZIPCODE,user.zipcode);
putrec(userdat,U_ZIPCODE+LEN_ZIPCODE,2,crlf);

putrec(userdat,U_PASS,LEN_PASS,user.pass);
putrec(userdat,U_PHONE,LEN_PHONE,user.phone);
putrec(userdat,U_BIRTH,LEN_BIRTH,user.birth);
putrec(userdat,U_MODEM,LEN_MODEM,user.modem);
putrec(userdat,U_LASTON,8,ultoa(user.laston,str,16));
putrec(userdat,U_FIRSTON,8,ultoa(user.firston,str,16));
putrec(userdat,U_EXPIRE,8,ultoa(user.expire,str,16));
putrec(userdat,U_PWMOD,8,ultoa(user.pwmod,str,16));
putrec(userdat,U_PWMOD+8,2,crlf);

putrec(userdat,U_LOGONS,5,itoa(user.logons,str,10));
putrec(userdat,U_LTODAY,5,itoa(user.ltoday,str,10));
putrec(userdat,U_TIMEON,5,itoa(user.timeon,str,10));
putrec(userdat,U_TEXTRA,5,itoa(user.textra,str,10));
putrec(userdat,U_TTODAY,5,itoa(user.ttoday,str,10));
putrec(userdat,U_TLAST,5,itoa(user.tlast,str,10));
putrec(userdat,U_POSTS,5,itoa(user.posts,str,10));
putrec(userdat,U_EMAILS,5,itoa(user.emails,str,10));
putrec(userdat,U_FBACKS,5,itoa(user.fbacks,str,10));
putrec(userdat,U_ETODAY,5,itoa(user.etoday,str,10));
putrec(userdat,U_PTODAY,5,itoa(user.ptoday,str,10));
putrec(userdat,U_PTODAY+5,2,crlf);

putrec(userdat,U_ULB,10,ultoa(user.ulb,str,10));
putrec(userdat,U_ULS,5,itoa(user.uls,str,10));
putrec(userdat,U_DLB,10,ultoa(user.dlb,str,10));
putrec(userdat,U_DLS,5,itoa(user.dls,str,10));
putrec(userdat,U_CDT,10,ultoa(user.cdt,str,10));
putrec(userdat,U_MIN,10,ultoa(user.min,str,10));
putrec(userdat,U_MIN+10,2,crlf);

putrec(userdat,U_LEVEL,2,itoa(user.level,str,10));
putrec(userdat,U_FLAGS1,8,ultoa(user.flags1,str,16));
putrec(userdat,U_TL,2,nulstr);	/* unused */
putrec(userdat,U_FLAGS2,8,ultoa(user.flags2,str,16));
putrec(userdat,U_EXEMPT,8,ultoa(user.exempt,str,16));
putrec(userdat,U_REST,8,ultoa(user.rest,str,16));
putrec(userdat,U_REST+8,2,crlf);

putrec(userdat,U_ROWS,2,itoa(user.rows,str,10));
userdat[U_SEX]=user.sex;
userdat[U_PROT]=user.prot;
putrec(userdat,U_MISC,8,ultoa(user.misc,str,16));
putrec(userdat,U_LEECH,2,itoa(user.leech,str,16));

putrec(userdat,U_CURSUB,8,user.cursub);
putrec(userdat,U_CURDIR,8,user.curdir);

// putrec(userdat,U_CMDSET,2,itoa(user.cmdset,str,16)); /* Unused */
putrec(userdat,U_CMDSET+2,2,crlf);

putrec(userdat,U_XFER_CMD+LEN_XFER_CMD,2,crlf);

putrec(userdat,U_MAIL_CMD+LEN_MAIL_CMD,2,crlf);

putrec(userdat,U_FREECDT,10,ultoa(user.freecdt,str,10));

putrec(userdat,U_FLAGS3,8,ultoa(user.flags3,str,16));
putrec(userdat,U_FLAGS4,8,ultoa(user.flags4,str,16));

if(user.xedit)
	putrec(userdat,U_XEDIT,8,xedit[user.xedit-1]->code);
else
	putrec(userdat,U_XEDIT,8,nulstr);

putrec(userdat,U_SHELL,8,shell[user.shell]->code);

putrec(userdat,U_QWK,8,ultoa(user.qwk,str,16));
putrec(userdat,U_TMPEXT,3,user.tmpext);
putrec(userdat,U_CHAT,8,ultoa(user.chat,str,16));
putrec(userdat,U_NS_TIME,8,ultoa(user.ns_time,str,16));

putrec(userdat,U_UNUSED,29,crlf);
putrec(userdat,U_UNUSED+29,2,crlf);

sprintf(str,"%sUSER\\USER.DAT",data_dir);
if((file=nopen(str,O_WRONLY|O_CREAT|O_DENYNONE))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_DENYNONE);
	return; }

/***		This shouldn't be necessary
if(filelength(file)<(long)((long)user.number*U_LEN))
    lseek(file,0L,SEEK_END);
else
***/

lseek(file,(long)((long)((long)user.number-1)*U_LEN),SEEK_SET);

i=0;
while(i<LOOP_NODEDAB
	&& lock(file,(long)((long)(user.number-1)*U_LEN),U_LEN)==-1) {
	if(i>10)
		mswait(55);
	i++; }

if(i>=LOOP_NODEDAB) {
	close(file);
	errormsg(WHERE,ERR_LOCK,"USER.DAT",user.number);
    return; }

if(write(file,userdat,U_LEN)!=U_LEN) {
	unlock(file,(long)((long)(user.number-1)*U_LEN),U_LEN);
    close(file);
    errormsg(WHERE,ERR_WRITE,str,U_LEN);
    return; }
unlock(file,(long)((long)(user.number-1)*U_LEN),U_LEN);
close(file);
for(i=1;i<=sys_nodes;i++) { /* instant user data update */
    if(i==node_num)
        continue;
    getnodedat(i,&node,0);
    if(node.useron==user.number && (node.status==NODE_INUSE
        || node.status==NODE_QUIET)) {
        getnodedat(i,&node,1);
        node.misc|=NODE_UDAT;
        putnodedat(i,node);
        break; } }
}


/****************************************************************************/
/* Puts 'name' into slot 'number' in USER\\NAME.DAT							*/
/****************************************************************************/
void putusername(int number, char *name)
{
	char str[256];
	int file;
	long length,l;

sprintf(str,"%sUSER\\NAME.DAT",data_dir);
if((file=nopen(str,O_RDWR|O_CREAT))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_RDWR|O_CREAT);
	return; }
length=filelength(file);
if(length && length%(LEN_ALIAS+2)) {
	close(file);
	errormsg(WHERE,ERR_LEN,str,length);
	return; }
if(length<(((long)number-1)*(LEN_ALIAS+2))) {
	sprintf(str,"%*s",LEN_ALIAS,nulstr);
	strset(str,ETX);
	strcat(str,crlf);
	lseek(file,0L,SEEK_END);
	while(filelength(file)<((long)number*(LEN_ALIAS+2)))
		write(file,str,(LEN_ALIAS+2)); }
lseek(file,(long)(((long)number-1)*(LEN_ALIAS+2)),SEEK_SET);
putrec(str,0,LEN_ALIAS,name);
putrec(str,LEN_ALIAS,2,crlf);
write(file,str,LEN_ALIAS+2);
close(file);
}

/****************************************************************************/
/* Fills the 'ptr' element of the each element of the sub[] array of sub_t  */
/* and the sub_misc and sub_ptr global variables                            */
/* Called from function main                                                */
/****************************************************************************/
void getmsgptrs()
{
	char	str[256];
	ushort	ch;
	uint	i;
	int 	file;
	long	length;
	FILE	*stream;

now=time(NULL);
if(!useron.number)
    return;
bputs(text[LoadingMsgPtrs]);
sprintf(str,"%sUSER\\PTRS\\%4.4u.IXB",data_dir,useron.number);
if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
	for(i=0;i<total_subs;i++) {
		sub_ptr[i]=sub[i]->ptr=sub_last[i]=sub[i]->last=0;
		if(sub[i]->misc&SUB_NSDEF)
			sub[i]->misc|=SUB_NSCAN;
		else
			sub[i]->misc&=~SUB_NSCAN;
		if(sub[i]->misc&SUB_SSDEF)
			sub[i]->misc|=SUB_SSCAN;
		else
			sub[i]->misc&=~SUB_SSCAN;
		sub_misc[i]=sub[i]->misc; }
	bputs(text[LoadedMsgPtrs]);
	return; }
length=filelength(file);
for(i=0;i<total_subs;i++) {
	if(length<(sub[i]->ptridx+1)*10L) {
		sub[i]->ptr=sub[i]->last=0L;
		if(sub[i]->misc&SUB_NSDEF)
			sub[i]->misc|=SUB_NSCAN;
		else
			sub[i]->misc&=~SUB_NSCAN;
		if(sub[i]->misc&SUB_SSDEF)
			sub[i]->misc|=SUB_SSCAN;
		else
			sub[i]->misc&=~SUB_SSCAN; }
	else {
		fseek(stream,(long)sub[i]->ptridx*10L,SEEK_SET);
		fread(&sub[i]->ptr,4,1,stream);
		fread(&sub[i]->last,4,1,stream);
		fread(&ch,2,1,stream);
		if(ch&5)	/* Either bit 0 or 2 */
			sub[i]->misc|=SUB_NSCAN;
		else
			sub[i]->misc&=~SUB_NSCAN;
		if(ch&2)
			sub[i]->misc|=SUB_SSCAN;
		else
			sub[i]->misc&=~SUB_SSCAN;
		if(ch&0x100)
			sub[i]->misc|=SUB_YSCAN;
		else
			sub[i]->misc&=~SUB_YSCAN; }
	sub_ptr[i]=sub[i]->ptr;
	sub_last[i]=sub[i]->last;
    sub_misc[i]=sub[i]->misc; }
fclose(stream);
bputs(text[LoadedMsgPtrs]);
}

/****************************************************************************/
/* Writes to DATA\USER\PTRS\xxxx.DAB the msgptr array for the current user	*/
/* Called from functions main and newuser                                   */
/****************************************************************************/
void putmsgptrs()
{
	char	str[256];
	ushort	idx,ch;
	uint	i,j;
	int 	file;
	ulong	l=0L,length;

if(!useron.number)
    return;
sprintf(str,"%sUSER\\PTRS\\%4.4u.IXB",data_dir,useron.number);
if((file=nopen(str,O_WRONLY|O_CREAT))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT);
	return; }
length=filelength(file);
for(i=0;i<total_subs;i++) {
	if(sub[i]->ptr==sub_ptr[i] && sub[i]->last==sub_last[i]
		&& length>=((long)(sub[i]->ptridx+1)*10)
		&& ((sub[i]->misc&(SUB_NSCAN|SUB_SSCAN|SUB_YSCAN))
			==(sub_misc[i]&(SUB_NSCAN|SUB_SSCAN|SUB_YSCAN))))
        continue;
	while(filelength(file)<(long)(sub[i]->ptridx)*10) {
        lseek(file,0L,SEEK_END);
		idx=tell(file)/10;
		for(j=0;j<total_subs;j++)
			if(sub[j]->ptridx==idx)
				break;
        write(file,&l,4);
        write(file,&l,4);
		ch=0xff;					/* default to scan ON for new sub */
		if(j<total_subs) {
			if(!(sub[j]->misc&SUB_NSCAN))
				ch&=~5;
			if(!(sub[j]->misc&SUB_SSCAN))
				ch&=~2;
			if(sub[j]->misc&SUB_YSCAN)
				ch|=0x100; }
		write(file,&ch,2); }
	lseek(file,(long)((long)(sub[i]->ptridx)*10),SEEK_SET);
    write(file,&(sub[i]->ptr),4);
	write(file,&(sub[i]->last),4);
	ch=0xff;
	if(!(sub[i]->misc&SUB_NSCAN))
		ch&=~5;
	if(!(sub[i]->misc&SUB_SSCAN))
		ch&=~2;
	if(sub[i]->misc&SUB_YSCAN)
		ch|=0x100;
	write(file,&ch,2); }
close(file);
if(!flength(str))				/* Don't leave 0 byte files */
	remove(str);
}

/****************************************************************************/
/* Checks for a duplicate user filed starting at user record offset         */
/* 'offset', reading in 'datlen' chars, comparing to 'str' for each user    */
/* except 'usernumber' if it is non-zero. Comparison is NOT case sensitive. */
/* del is 1 if the search is to included deleted/inactive users 0 otherwise */
/* Returns the usernumber of the dupe if found, 0 if not                    */
/****************************************************************************/
uint userdatdupe(uint usernumber, uint offset, uint datlen, char *dat
    , char del)
{
    char str[256];
    int i,file;
    long l,length;

truncsp(dat);
sprintf(str,"%sUSER\\USER.DAT",data_dir);
if((file=nopen(str,O_RDONLY|O_DENYNONE))==-1)
    return(0);
length=filelength(file);
bputs(text[SearchingForDupes]);
for(l=0;l<length && online;l+=U_LEN) {
	checkline();
    if(usernumber && l/U_LEN==usernumber-1)
        continue;
    lseek(file,l+offset,SEEK_SET);
	i=0;
	while(i<LOOP_NODEDAB && lock(file,l,U_LEN)==-1) {
		if(i>10)
			mswait(55);
		i++; }

	if(i>=LOOP_NODEDAB) {
		close(file);
		errormsg(WHERE,ERR_LOCK,"USER.DAT",l);
		return(0); }

    read(file,str,datlen);
    for(i=0;i<datlen;i++)
        if(str[i]==ETX) break;
    str[i]=0;
    truncsp(str);
    if(!stricmp(str,dat)) {
        if(!del) {      /* Don't include deleted users in search */
            lseek(file,l+U_MISC,SEEK_SET);
            read(file,str,8);
            getrec(str,0,8,str);
			if(ahtoul(str)&(DELETED|INACTIVE)) {
				unlock(file,l,U_LEN);
				continue; } }
		unlock(file,l,U_LEN);
        close(file);
        bputs(text[SearchedForDupes]);
		return((l/U_LEN)+1); }
	else
		unlock(file,l,U_LEN); }
close(file);
bputs(text[SearchedForDupes]);
return(0);
}


/****************************************************************************/
/* Removes any files in the user transfer index (XFER.IXT) that match the   */
/* specifications of dest, or source user, or filename or any combination.  */
/****************************************************************************/
void rmuserxfers(int fromuser, int destuser, char *fname)
{
    char str[256],*ixtbuf;
    int file;
    long l,length;

sprintf(str,"%sXFER.IXT",data_dir);
if(!fexist(str))
    return;
if(!flength(str)) {
    remove(str);
    return; }
if((file=nopen(str,O_RDONLY))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
    return; }
length=filelength(file);
if((ixtbuf=(char *)MALLOC(length))==NULL) {
    close(file);
    errormsg(WHERE,ERR_ALLOC,str,length);
    return; }
if(read(file,ixtbuf,length)!=length) {
    close(file);
	FREE(ixtbuf);
    errormsg(WHERE,ERR_READ,str,length);
    return; }
close(file);
if((file=nopen(str,O_WRONLY|O_TRUNC))==-1) {
	FREE(ixtbuf);
    errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_TRUNC);
    return; }
for(l=0;l<length;l+=24) {
    if(fname!=NULL && fname[0]) {               /* fname specified */
        if(!strncmp(ixtbuf+l+5,fname,12)) {     /* this is the file */
            if(destuser && fromuser) {          /* both dest and from user */
                if(atoi(ixtbuf+l)==destuser && atoi(ixtbuf+l+18)==fromuser)
                    continue; }                 /* both match */
            else if(fromuser) {                 /* from user */
                if(atoi(ixtbuf+l+18)==fromuser) /* matches */
                    continue; }
            else if(destuser) {                 /* dest user */
                if(atoi(ixtbuf+l)==destuser)    /* matches */
                    continue; }
            else continue; } }                  /* no users, so match */
    else if(destuser && fromuser) {
        if(atoi(ixtbuf+l+18)==fromuser && atoi(ixtbuf+l)==destuser)
            continue; }
    else if(destuser && atoi(ixtbuf+l)==destuser)
        continue;
    else if(fromuser && atoi(ixtbuf+l+18)==fromuser)
        continue;
    write(file,ixtbuf+l,24); }
close(file);
FREE(ixtbuf);
}

