#line 1 "CHAT.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/****************************************/
/* Local and Node-to-Node Chat routines */
/****************************************/

#include "sbbs.h"

#define PCHAT_LEN 1000		/* Size of Private chat file */

int getnodetopage(int all, int telegram);


char *weekday[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday"
				,"Saturday"};
char *month[]={"January","February","March","April","May","June"
			,"July","August","September","October","November","December"};


/****************************************************************************/
/* Chat between local keyboard and remote user on current node.				*/
/* Called from inkey (inkey is then re-entrant)								*/
/****************************************************************************/
void localchat()
{
	uchar	str[256],act;
	int con=console;	/* save console state */
	time_t 	beg=time(NULL);

console&=~(CON_L_ECHOX|CON_R_ECHOX);	/* turn off X's */
console|=(CON_L_ECHO|CON_R_ECHO);		/* make sure echo is enabled */
nosound();
sys_status&=~SS_SYSPAGE;
act=action;					/* save the user's current action */
action=NODE_LCHT;
bprintf(text[SysopIsHere],sys_op);
while(sys_status&SS_LCHAT && online) {
	SYNC;
	getstr(str,78,K_WRAP|K_MSG); }
bputs(text[EndOfChat]);
now=time(NULL);
if(!(sys_status&SS_USERON))
	answertime=now;
starttime+=now-beg;   /* credit user for time in chat */
RESTORELINE;
action=act;
console=con;					/* restore console */
}

void sysop_page(void)
{
	int i;

for(i=0;i<total_pages;i++)
	if(chk_ar(page[i]->ar,useron))
		break;
if(i<total_pages) {
	bprintf(text[PagingGuru],sys_op);
	external(cmdstr(page[i]->cmd,nulstr,nulstr,NULL)
		,page[i]->misc&IO_INTS ? EX_OUTL|EX_OUTR|EX_INR
			: EX_OUTL); }
else if(sys_misc&SM_SHRTPAGE) {
	bprintf(text[PagingGuru],sys_op);
	for(i=0;i<10 && !lkbrd(1);i++) {
		beep(1000,200);
		mswait(200);
		outchar('.'); }
	CRLF; }
else {
	sys_status^=SS_SYSPAGE;
	bprintf(text[SysopPageIsNow]
		,sys_status&SS_SYSPAGE ? text[ON] : text[OFF]);
	nosound();	}
}

/****************************************************************************/
/* Returns 1 if user online has access to channel "channum"                 */
/****************************************************************************/
char chan_access(uint cnum)
{

if(!total_chans || cnum>=total_chans || !chk_ar(chan[cnum]->ar,useron)) {
	bputs(text[CantAccessThatChannel]);
	return(0); }
if(!(useron.exempt&FLAG('J')) && chan[cnum]->cost>useron.cdt+useron.freecdt) {
	bputs(text[NotEnoughCredits]);
	return(0); }
return(1);
}

void privchat(void)
{
	uchar	str[128],ch,c,*p,localbuf[5][81],remotebuf[5][81]
			,localline=0,remoteline=0,localchar=0,remotechar=0
			,*sep="\1w\1h컴컴�[\1i\1r%c\1n\1h]컴컴� "
				"\1yPrivate Chat - \1rCtrl-C to Quit \1y- "
				"Time Left: \1g%-8s\1w"
				" 쳐컴�[\1i\1b%c\1n\1h]컴컴�";
	int 	in,out,i,j,n,done,echo=1,x,y,activity;
	node_t	node;

n=getnodetopage(0,0);
if(!n)
	return;
if(n==node_num) {
	bputs(text[NoNeedToPageSelf]);
	return; }
getnodedat(n,&node,0);
if(node.action==NODE_PCHT && node.aux!=node_num) {
	bprintf(text[NodeNAlreadyInPChat],n);
	return; }
if((node.action!=NODE_PAGE || node.aux!=node_num)
	&& node.misc&NODE_POFF && !SYSOP) {
	bprintf(text[CantPageNode],node.misc&NODE_ANON
		? text[UNKNOWN_USER] : username(node.useron,tmp));
	return; }
if(node.action!=NODE_PAGE) {
	bprintf("\r\n\1n\1mPaging \1h%s #%u\1n\1m for private chat\r\n"
		,node.misc&NODE_ANON ? text[UNKNOWN_USER] : username(node.useron,tmp)
		,node.useron);
	sprintf(str,text[NodePChatPageMsg]
		,node_num,thisnode.misc&NODE_ANON
			? text[UNKNOWN_USER] : useron.alias);
	putnmsg(n,str);
	sprintf(str,"Paged %s on node %d to private chat"
		,username(node.useron,tmp),n);
	logline("C",str); }

getnodedat(node_num,&thisnode,1);
thisnode.action=action=NODE_PAGE;
thisnode.aux=n;
putnodedat(node_num,thisnode);

if(node.action!=NODE_PAGE || node.aux!=node_num) {
	bprintf(text[WaitingForNodeInPChat],n);
	while(online && !(sys_status&SS_ABORT)) {
		getnodedat(n,&node,0);
		if((node.action==NODE_PAGE || node.action==NODE_PCHT)
			&& node.aux==node_num) {
			bprintf(text[NodeJoinedPrivateChat]
				,n,node.misc&NODE_ANON ? text[UNKNOWN_USER]
					: username(node.useron,tmp));
			break; }
		if(!inkey(0))
			mswait(1);
		action=NODE_PAGE;
		checkline();
		gettimeleft();
		SYNC; } }

getnodedat(node_num,&thisnode,1);
thisnode.action=action=NODE_PCHT;
putnodedat(node_num,thisnode);

if(!online || sys_status&SS_ABORT)
	return;

if(useron.chat&CHAT_SPLITP && useron.misc&ANSI && rows>=24)
	sys_status|=SS_SPLITP;
/*
if(!(useron.misc&EXPERT))
	menu("PRIVCHAT");
*/

if(!(sys_status&SS_SPLITP))
	bputs(text[WelcomeToPrivateChat]);

sprintf(str,"%sCHAT.DAB",node_dir);
if((out=open(str,O_RDWR|O_DENYNONE|O_CREAT|O_BINARY
	,S_IREAD|S_IWRITE))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_RDWR|O_DENYNONE|O_CREAT);
	return; }

sprintf(str,"%sCHAT.DAB",node_path[n-1]);
if(!fexist(str))		/* Wait while it's created for the first time */
	mswait(2000);
if((in=open(str,O_RDWR|O_DENYNONE|O_CREAT|O_BINARY
	,S_IREAD|S_IWRITE))==-1) {
	close(out);
	errormsg(WHERE,ERR_OPEN,str,O_RDWR|O_DENYNONE|O_CREAT);
	return; }

if((p=(char *)MALLOC(PCHAT_LEN))==NULL) {
	close(in);
	close(out);
	errormsg(WHERE,ERR_ALLOC,str,PCHAT_LEN);
	return; }
memset(p,0,PCHAT_LEN);
write(in,p,PCHAT_LEN);
write(out,p,PCHAT_LEN);
FREE(p);
lseek(in,0L,SEEK_SET);
lseek(out,0L,SEEK_SET);

getnodedat(node_num,&thisnode,1);
thisnode.misc&=~NODE_RPCHT; 		/* Clear "reset pchat flag" */
putnodedat(node_num,thisnode);

getnodedat(n,&node,1);
node.misc|=NODE_RPCHT;				/* Set "reset pchat flag" */
putnodedat(n,node); 				/* on other node */

									/* Wait for other node */
									/* to acknowledge and reset */
while(online && !(sys_status&SS_ABORT)) {
	getnodedat(n,&node,0);
	if(!(node.misc&NODE_RPCHT))
		break;
	getnodedat(node_num,&thisnode,0);
	if(thisnode.misc&NODE_RPCHT)
		break;
	checkline();
	gettimeleft();
	SYNC;
	inkey(0);
	mswait(1); }


action=NODE_PCHT;
SYNC;

if(sys_status&SS_SPLITP) {
	lncntr=0;
	CLS;
	ANSI_SAVE();
	GOTOXY(1,13);
	bprintf(sep
		,thisnode.misc&NODE_MSGW ? 'T':SP
		,sectostr(timeleft,tmp)
		,thisnode.misc&NODE_NMSG ? 'M':SP);
	CRLF; }

done=0;

while(online && !(sys_status&SS_ABORT) && !done) {
	RIOSYNC(0);
	lncntr=0;
	if(sys_status&SS_SPLITP)
		lbuflen=0;
	action=NODE_PCHT;
	if(!localchar) {
		if(sys_status&SS_SPLITP) {
			getnodedat(node_num,&thisnode,0);
			if(thisnode.misc&NODE_INTR)
				break;
			if(thisnode.misc&NODE_UDAT && !(useron.rest&FLAG('G'))) {
				getuserdat(&useron);
				getnodedat(node_num,&thisnode,1);
				thisnode.misc&=~NODE_UDAT;
				putnodedat(node_num,thisnode); } }
		else
			nodesync(); }
	activity=0;
	if((ch=inkey(K_GETSTR))!=0) {
		activity=1;
		if(echo)
			attr(color[clr_chatlocal]);
		else
			lclatr(color[clr_chatlocal]);
		if(ch==BS) {
			if(localchar) {
				if(echo)
					bputs("\b \b");
				else
					lputs("\b \b");
				localchar--;
				localbuf[localline][localchar]=0; } }
		else if(ch==TAB) {
			if(echo)
				outchar(SP);
			else
				lputc(SP);
			localbuf[localline][localchar]=SP;
			localchar++;
			while(localchar<78 && localchar%8) {
				if(echo)
					outchar(SP);
				else
					lputc(SP);
				localbuf[localline][localchar++]=SP; } }
		else if(ch>=SP || ch==CR) {
			if(ch!=CR) {
				if(echo)
					outchar(ch);
				else
					lputc(ch);
				localbuf[localline][localchar]=ch; }

			if(ch==CR || (localchar>68 && ch==SP) || ++localchar>78) {

				localbuf[localline][localchar]=0;
				localchar=0;

				if(sys_status&SS_SPLITP && lclwy()==24) {
					GOTOXY(1,13);
					bprintf(sep
						,thisnode.misc&NODE_MSGW ? 'T':SP
						,sectostr(timeleft,tmp)
						,thisnode.misc&NODE_NMSG ? 'M':SP);
					attr(color[clr_chatlocal]);
					for(x=13,y=0;x<rows;x++,y++) {
						bprintf("\x1b[%d;1H\x1b[K",x+1);
						if(y<=localline)
							bprintf("%s\r\n",localbuf[y]); }
					GOTOXY(1,15+localline);
					localline=0; }
				else {
					if(localline>=4)
						for(i=0;i<4;i++)
							memcpy(localbuf[i],localbuf[i+1],81);
					else
						localline++;
					if(echo) {
						CRLF;
						if(sys_status&SS_SPLITP)
							bputs("\x1b[K"); }
					else
						lputs(crlf); }
				// SYNC;
				} }

		read(out,&c,1);
		lseek(out,-1L,SEEK_CUR);
		if(!c)		/* hasn't wrapped */
			write(out,&ch,1);
		else {
			if(!tell(out))
				lseek(out,0L,SEEK_END);
			lseek(out,-1L,SEEK_CUR);
			ch=0;
			write(out,&ch,1);
			lseek(out,-1L,SEEK_CUR); }
		if(tell(out)>=PCHAT_LEN)
			lseek(out,0L,SEEK_SET);
		}
	else while(online) {
		if(!(sys_status&SS_SPLITP))
			remotechar=localchar;
		if(tell(in)>=PCHAT_LEN)
			lseek(in,0L,SEEK_SET);
		ch=0;
		read(in,&ch,1);
		lseek(in,-1L,SEEK_CUR);
		if(!ch) break;					  /* char from other node */
		activity=1;
		x=lclwx();
		y=lclwy();
		if(sys_status&SS_SPLITP)
			ANSI_RESTORE();
		attr(color[clr_chatremote]);
		if(sys_status&SS_SPLITP)
			bputs("\b \b");             /* Delete fake cursor */
		if(ch==BS) {
			if(remotechar) {
				bputs("\b \b");
				remotechar--;
				remotebuf[remoteline][remotechar]=0; } }
		else if(ch==TAB) {
			outchar(SP);
			remotebuf[remoteline][remotechar]=SP;
			remotechar++;
			while(remotechar<78 && remotechar%8) {
				outchar(SP);
				remotebuf[remoteline][remotechar++]=SP; } }
		else if(ch>=SP || ch==CR) {
			if(ch!=CR) {
				outchar(ch);
				remotebuf[remoteline][remotechar]=ch; }

			if(ch==CR || (remotechar>68 && ch==SP) || ++remotechar>78) {

				remotebuf[remoteline][remotechar]=0;
				remotechar=0;

				if(sys_status&SS_SPLITP && lclwy()==12) {
					CRLF;
					bprintf(sep
						,thisnode.misc&NODE_MSGW ? 'T':SP
						,sectostr(timeleft,tmp)
						,thisnode.misc&NODE_NMSG ? 'M':SP);
					attr(color[clr_chatremote]);
					for(i=0;i<12;i++) {
						bprintf("\x1b[%d;1H\x1b[K",i+1);
						if(i<=remoteline)
							bprintf("%s\r\n",remotebuf[i]); }
					remoteline=0;
					GOTOXY(1,6); }
				else {
					if(remoteline>=4)
						for(i=0;i<4;i++)
							memcpy(remotebuf[i],remotebuf[i+1],81);
					else
						remoteline++;
					if(echo) {
						CRLF;
						if(sys_status&SS_SPLITP)
							bputs("\x1b[K"); }
					else
						lputs(crlf); } } }
		if(sys_status&SS_SPLITP) {
			bputs("\1i_\1n");  /* Fake cursor */
			ANSI_SAVE();
			GOTOXY(x,y); }
		ch=0;
		write(in,&ch,1);

		if(!(sys_status&SS_SPLITP))
			localchar=remotechar;
		}
	if(!activity) { 						/* no activity so chk node.dab */
		getnodedat(n,&node,0);
		if((node.action!=NODE_PCHT && node.action!=NODE_PAGE)
			|| node.aux!=node_num) {
			bprintf(text[NodeLeftPrivateChat]
				,n,node.misc&NODE_ANON ? text[UNKNOWN_USER]
				: username(node.useron,tmp));
			break; }
		getnodedat(node_num,&thisnode,0);
		if(thisnode.misc&NODE_RPCHT) {		/* pchat has been reset */
			lseek(in,0L,SEEK_SET);			/* so seek to beginning */
			lseek(out,0L,SEEK_SET);
			getnodedat(node_num,&thisnode,1);
			thisnode.misc&=~NODE_RPCHT;
			putnodedat(node_num,thisnode); }
		mswait(0); }
	checkline();
	gettimeleft(); }
if(sys_status&SS_SPLITP)
	CLS;
sys_status&=~SS_SPLITP;
close(in);
close(out);
}

/****************************************************************************/
/* The chat section                                                         */
/****************************************************************************/
void chatsection()
{
	uchar	line[256],str[256],ch,c,done,no_rip_menu
			,usrs,preusrs,qusrs,*gurubuf=NULL,channel,savch,*p
			,pgraph[400],buf[400]
			,usr[MAX_NODES],preusr[MAX_NODES],qusr[MAX_NODES];
	int 	file,in,out;
	long	i,j,k,n;
	node_t 	node;

action=NODE_CHAT;
if(useron.misc&(RIP|WIP) || !(useron.misc&EXPERT))
    menu("CHAT");
ASYNC;
bputs(text[ChatPrompt]);
while(online) {
	no_rip_menu=0;
	ch=getkeys("ACDJPQST?\r",0);
	if(ch>SP)
		logch(ch,0);
	switch(ch) {
		case 'S':
			useron.chat^=CHAT_SPLITP;
			putuserrec(useron.number,U_CHAT,8
                ,ltoa(useron.chat,str,16));
			bprintf("\r\nPrivate split-screen chat is now: %s\r\n"
				,useron.chat&CHAT_SPLITP ? text[ON]:text[OFF]);
			break;
		case 'A':
			CRLF;
			useron.chat^=CHAT_NOACT;
			putuserrec(useron.number,U_CHAT,8
				,ltoa(useron.chat,str,16));
			getnodedat(node_num,&thisnode,1);
			thisnode.misc^=NODE_AOFF;
			printnodedat(node_num,thisnode);
			putnodedat(node_num,thisnode);
			no_rip_menu=1;
			break;
		case 'D':
			CRLF;
			useron.chat^=CHAT_NOPAGE;
			putuserrec(useron.number,U_CHAT,8
                ,ltoa(useron.chat,str,16));
			getnodedat(node_num,&thisnode,1);
			thisnode.misc^=NODE_POFF;
			printnodedat(node_num,thisnode);
			putnodedat(node_num,thisnode);
			no_rip_menu=1;
			break;
		case 'J':
			if(!chan_access(0))
				break;
			if(useron.misc&(RIP|WIP) ||!(useron.misc&EXPERT))
				menu("MULTCHAT");
			getnodedat(node_num,&thisnode,1);
			bputs(text[WelcomeToMultiChat]);
			channel=thisnode.aux=1;		/* default channel 1 */
			putnodedat(node_num,thisnode);
			bprintf(text[WelcomeToChannelN],channel,chan[0]->name);
			if(gurubuf) {
				FREE(gurubuf);
				gurubuf=NULL; }
			if(chan[0]->misc&CHAN_GURU && chan[0]->guru<total_gurus
				&& chk_ar(guru[chan[0]->guru]->ar,useron)) {
				sprintf(str,"%s%s.DAT",ctrl_dir,guru[chan[0]->guru]->code);
				if((file=nopen(str,O_RDONLY))==-1) {
					errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
					break; }
				if((gurubuf=MALLOC(filelength(file)+1))==NULL) {
					close(file);
					errormsg(WHERE,ERR_ALLOC,str,filelength(file)+1);
					break; }
				read(file,gurubuf,filelength(file));
				gurubuf[filelength(file)]=0;
				close(file); }
			usrs=0;
			for(i=1;i<=sys_nodes && i<=sys_lastnode;i++) {
				if(i==node_num)
					continue;
				getnodedat(i,&node,0);
				if(node.action!=NODE_MCHT || node.status!=NODE_INUSE)
					continue;
				if(node.aux && (node.aux&0xff)!=channel)
					continue;
				printnodedat(i,node);
				preusr[usrs]=usr[usrs++]=i; }
			preusrs=usrs;
			if(gurubuf)
				bprintf(text[NodeInMultiChatLocally]
					,sys_nodes+1,guru[chan[channel-1]->guru]->name,channel);
			bputs(text[YoureOnTheAir]);
			done=0;
			while(online && !done) {
				checkline();
				gettimeleft();
				action=NODE_MCHT;
				qusrs=usrs=0;
            	for(i=1;i<=sys_nodes;i++) {
					if(i==node_num)
						continue;
					getnodedat(i,&node,0);
					if(node.action!=NODE_MCHT
						|| (node.aux && channel && (node.aux&0xff)!=channel))
						continue;
					if(node.status==NODE_QUIET)
						qusr[qusrs++]=i;
					else if(node.status==NODE_INUSE)
						usr[usrs++]=i; }
				if(preusrs>usrs) {
					if(!usrs && channel && chan[channel-1]->misc&CHAN_GURU
						&& chan[channel-1]->guru<total_gurus)
						bprintf(text[NodeJoinedMultiChat]
							,sys_nodes+1,guru[chan[channel-1]->guru]->name
							,channel);
					outchar(7);
					for(i=0;i<preusrs;i++) {
						for(j=0;j<usrs;j++)
							if(preusr[i]==usr[j])
								break;
						if(j==usrs) {
							getnodedat(preusr[i],&node,0);
							if(node.misc&NODE_ANON)
								sprintf(str,"%.80s",text[UNKNOWN_USER]);
							else
								username(node.useron,str);
							bprintf(text[NodeLeftMultiChat]
								,preusr[i],str,channel); } } }
				else if(preusrs<usrs) {
					if(!preusrs && channel && chan[channel-1]->misc&CHAN_GURU
						&& chan[channel-1]->guru<total_gurus)
						bprintf(text[NodeLeftMultiChat]
							,sys_nodes+1,guru[chan[channel-1]->guru]->name
							,channel);
					outchar(7);
					for(i=0;i<usrs;i++) {
						for(j=0;j<preusrs;j++)
							if(usr[i]==preusr[j])
								break;
						if(j==preusrs) {
							getnodedat(usr[i],&node,0);
                            if(node.misc&NODE_ANON)
								sprintf(str,"%.80s",text[UNKNOWN_USER]);
							else
								username(node.useron,str);
							bprintf(text[NodeJoinedMultiChat]
								,usr[i],str,channel); } } }
                preusrs=usrs;
				for(i=0;i<usrs;i++)
					preusr[i]=usr[i];
				attr(color[clr_multichat]);
				SYNC;
				sys_status&=~SS_ABORT;
				if((ch=inkey(0))!=0 || wordwrap[0]) {
					if(ch=='/') {
						bputs(text[MultiChatCommandPrompt]);
						strcpy(str,"ACELWQ?*");
						if(SYSOP)
							strcat(str,"0");
						i=getkeys(str,total_chans);
						if(i&0x80000000L) {  /* change channel */
							savch=i&~0x80000000L;
							if(savch==channel)
								continue;
							if(!chan_access(savch-1))
								continue;
							bprintf(text[WelcomeToChannelN]
								,savch,chan[savch-1]->name);

							usrs=0;
							for(i=1;i<=sys_nodes;i++) {
								if(i==node_num)
									continue;
								getnodedat(i,&node,0);
								if(node.action!=NODE_MCHT
									|| node.status!=NODE_INUSE)
									continue;
								if(node.aux && (node.aux&0xff)!=savch)
									continue;
								printnodedat(i,node);
								if(node.aux&0x1f00) {	/* password */
									bprintf(text[PasswordProtected]
										,node.misc&NODE_ANON
										? text[UNKNOWN_USER]
										: username(node.useron,tmp));
									if(!getstr(str,8,K_UPPER|K_ALPHA|K_LINE))
										break;
									if(strcmp(str,unpackchatpass(tmp,node)))
										break;
										bputs(text[CorrectPassword]);  }
								preusr[usrs]=usr[usrs++]=i; }
							if(i<=sys_nodes) {	/* failed password */
								bputs(text[WrongPassword]);
								continue; }
							if(gurubuf) {
								FREE(gurubuf);
								gurubuf=NULL; }
							if(chan[savch-1]->misc&CHAN_GURU
								&& chan[savch-1]->guru<total_gurus
								&& chk_ar(guru[chan[savch-1]->guru]->ar,useron
								)) {
								sprintf(str,"%s%s.DAT",ctrl_dir
									,guru[chan[savch-1]->guru]->code);
                                if((file=nopen(str,O_RDONLY))==-1) {
                                    errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
                                    break; }
                                if((gurubuf=MALLOC(filelength(file)+1))==NULL) {
                                    close(file);
									errormsg(WHERE,ERR_ALLOC,str
										,filelength(file)+1);
                                    break; }
                                read(file,gurubuf,filelength(file));
                                gurubuf[filelength(file)]=0;
                                close(file); }
							preusrs=usrs;
							if(gurubuf)
								bprintf(text[NodeInMultiChatLocally]
									,sys_nodes+1
									,guru[chan[savch-1]->guru]->name
									,savch);
							channel=savch;
							if(!usrs && chan[savch-1]->misc&CHAN_PW
								&& !noyes(text[PasswordProtectChanQ])) {
								bputs(text[PasswordPrompt]);
								if(getstr(str,8,K_UPPER|K_ALPHA|K_LINE)) {
									getnodedat(node_num,&thisnode,1);
									thisnode.aux=channel;
									packchatpass(str,&thisnode); }
								else {
									getnodedat(node_num,&thisnode,1);
									thisnode.aux=channel; } }
							else {
								getnodedat(node_num,&thisnode,1);
								thisnode.aux=channel; }
							putnodedat(node_num,thisnode);
							bputs(text[YoureOnTheAir]);
							if(chan[channel-1]->cost
								&& !(useron.exempt&FLAG('J')))
								subtract_cdt(chan[channel-1]->cost); }
						else switch(i) {	/* other command */
							case '0':	/* Global channel */
								if(!SYSOP)
									break;
                            	usrs=0;
								for(i=1;i<=sys_nodes;i++) {
									if(i==node_num)
										continue;
									getnodedat(i,&node,0);
									if(node.action!=NODE_MCHT
										|| node.status!=NODE_INUSE)
										continue;
									printnodedat(i,node);
									preusr[usrs]=usr[usrs++]=i; }
								preusrs=usrs;
								getnodedat(node_num,&thisnode,1);
								thisnode.aux=channel=0;
								putnodedat(node_num,thisnode);
								break;
							case 'A':   /* Action commands */
								useron.chat^=CHAT_ACTION;
								bprintf("\r\nAction commands are now %s\r\n"
									,useron.chat&CHAT_ACTION
									? text[ON]:text[OFF]);
								putuserrec(useron.number,U_CHAT,8
                                    ,ltoa(useron.chat,str,16));
								break;
							case 'C':   /* List of action commands */
								CRLF;
								for(i=0;i<total_chatacts;i++) {
									if(chatact[i]->actset
										!=chan[channel-1]->actset)
										continue;
									bprintf("%-*.*s",LEN_CHATACTCMD
										,LEN_CHATACTCMD,chatact[i]->cmd);
									if(!((i+1)%8)) {
										CRLF; }
									else
										bputs(" "); }
								CRLF;
								break;
							case 'E':   /* Toggle echo */
								useron.chat^=CHAT_ECHO;
								bprintf(text[EchoIsNow]
									,useron.chat&CHAT_ECHO
									? text[ON]:text[OFF]);
								putuserrec(useron.number,U_CHAT,8
                                    ,ltoa(useron.chat,str,16));
								break;
							case 'L':	/* list nodes */
								CRLF;
								for(i=1;i<=sys_nodes && i<=sys_lastnode;i++) {
									getnodedat(i,&node,0);
									printnodedat(i,node); }
								CRLF;
								break;
							case 'W':   /* page node(s) */
								j=getnodetopage(0,0);
								if(!j)
									break;
								for(i=0;i<usrs;i++)
									if(usr[i]==j)
										break;
								if(i>=usrs) {
									bputs(text[UserNotFound]);
									break; }

								bputs(text[NodeMsgPrompt]);
								if(!getstr(line,66,K_LINE|K_MSG))
									break;

								sprintf(buf,text[ChatLineFmt]
									,thisnode.misc&NODE_ANON
									? text[AnonUserChatHandle]
									: useron.handle
									,node_num,'*',line);
								strcat(buf,crlf);
								if(useron.chat&CHAT_ECHO)
									bputs(buf);
								putnmsg(j,buf);
								break;
							case 'Q':	/* quit */
								done=1;
								break;
							case '*':
								sprintf(str,"%sMENU\\CHAN.*",text_dir);
								if(fexist(str))
									menu("CHAN");
								else {
									bputs(text[ChatChanLstHdr]);
									bputs(text[ChatChanLstTitles]);
									if(total_chans>=10) {
										bputs("     ");
										bputs(text[ChatChanLstTitles]); }
									CRLF;
									bputs(text[ChatChanLstUnderline]);
									if(total_chans>=10) {
										bputs("     ");
										bputs(text[ChatChanLstUnderline]); }
									CRLF;
									if(total_chans>=10)
										j=(total_chans/2)+(total_chans&1);
									else
										j=total_chans;
									for(i=0;i<j && !msgabort();i++) {
										bprintf(text[ChatChanLstFmt],i+1
											,chan[i]->name
											,chan[i]->cost);
										if(total_chans>=10) {
											k=(total_chans/2)
												+i+(total_chans&1);
											if(k<total_chans) {
												bputs("     ");
												bprintf(text[ChatChanLstFmt]
													,k+1
													,chan[k]->name
													,chan[k]->cost); } }
										CRLF; }
									CRLF; }
								break;
							case '?':	/* menu */
								menu("MULTCHAT");
								break;	} }
					else {
						ungetkey(ch);
						j=0;
						pgraph[0]=0;
						while(j<5) {
							if(!getstr(line,66,K_WRAP|K_MSG|K_CHAT))
								break;
							if(j) {
								sprintf(str,text[ChatLineFmt]
									,thisnode.misc&NODE_ANON
									? text[AnonUserChatHandle]
									: useron.handle
									,node_num,':',nulstr);
								sprintf(tmp,"%*s",bstrlen(str),nulstr);
								strcat(pgraph,tmp); }
							strcat(pgraph,line);
							strcat(pgraph,crlf);
							if(!wordwrap[0])
								break;
							j++; }
						if(pgraph[0]) {
							if(useron.chat&CHAT_ACTION) {
								for(i=0;i<total_chatacts;i++) {
									if(chatact[i]->actset
										!=chan[channel-1]->actset)
                                        continue;
									sprintf(str,"%s ",chatact[i]->cmd);
									if(!strnicmp(str,pgraph,strlen(str)))
										break;
									sprintf(str,"%.*s"
										,LEN_CHATACTCMD+2,pgraph);
									str[strlen(str)-2]=0;
									if(!stricmp(chatact[i]->cmd,str))
										break; }

								if(i<total_chatacts) {
									p=pgraph+strlen(str);
									n=atoi(p);
									for(j=0;j<usrs;j++) {
										getnodedat(usr[j],&node,0);
										if(usrs==1) /* no need to search */
											break;
										if(n) {
											if(usr[j]==n)
												break;
											continue; }
										username(node.useron,str);
										if(!strnicmp(str,p,strlen(str)))
											break;
										getuserrec(node.useron,U_HANDLE
											,LEN_HANDLE,str);
										if(!strnicmp(str,p,strlen(str)))
											break; }
									if(!usrs
										&& chan[channel-1]->guru<total_gurus)
										strcpy(str
										,guru[chan[channel-1]->guru]->name);
									else if(j>=usrs)
										strcpy(str,"everyone");
									else if(node.misc&NODE_ANON)
										strcpy(str,text[UNKNOWN_USER]);
									else
										username(node.useron,str);

									/* Display on same node */
									bprintf(chatact[i]->out
										,thisnode.misc&NODE_ANON
										? text[UNKNOWN_USER] : useron.alias
										,str);
									CRLF;

									if(usrs && j<usrs) {
										/* Display to dest user */
										sprintf(buf,chatact[i]->out
											,thisnode.misc&NODE_ANON
											? text[UNKNOWN_USER] : useron.alias
											,"you");
										strcat(buf,crlf);
										putnmsg(usr[j],buf); }


									/* Display to all other users */
									sprintf(buf,chatact[i]->out
										,thisnode.misc&NODE_ANON
										? text[UNKNOWN_USER] : useron.alias
										,str);
									strcat(buf,crlf);

									for(i=0;i<usrs;i++) {
										if(i==j)
											continue;
										getnodedat(usr[i],&node,0);
										putnmsg(usr[i],buf); }
									for(i=0;i<qusrs;i++) {
										getnodedat(qusr[i],&node,0);
										putnmsg(qusr[i],buf); }
									continue; } }

							sprintf(buf,text[ChatLineFmt]
								,thisnode.misc&NODE_ANON
								? text[AnonUserChatHandle]
								: useron.handle
								,node_num,':',pgraph);
							if(useron.chat&CHAT_ECHO)
								bputs(buf);
							for(i=0;i<usrs;i++) {
								getnodedat(usr[i],&node,0);
								putnmsg(usr[i],buf); }
							for(i=0;i<qusrs;i++) {
								getnodedat(qusr[i],&node,0);
								putnmsg(qusr[i],buf); }
							if(!usrs && channel && gurubuf
								&& chan[channel-1]->misc&CHAN_GURU)
								guruchat(pgraph,gurubuf,chan[channel-1]->guru);
								} } }
				else
					mswait(1);
				if(sys_status&SS_ABORT)
					break; }
			lncntr=0;
			break;
		case 'P':   /* private node-to-node chat */
            privchat();
            break;
		case 'C':
			no_rip_menu=1;
			ch=kbd_state(); 	 /* Check scroll lock */
			if(ch&16 || (sys_chat_ar[0] && chk_ar(sys_chat_ar,useron))
				|| useron.exempt&FLAG('C')) {
				sysop_page();
				break; }
			bprintf(text[SysopIsNotAvailable],sys_op);
			if(total_gurus && chk_ar(guru[0]->ar,useron)) {
				sprintf(str,text[ChatWithGuruInsteadQ],guru[0]->name);
				if(!yesno(str))
					break; }
			else
				break;
		case 'T':
			if(!total_gurus) {
				bprintf(text[SysopIsNotAvailable],"The Guru");
				break; }
			if(total_gurus==1 && chk_ar(guru[0]->ar,useron))
				i=0;
			else {
				for(i=0;i<total_gurus;i++)
					uselect(1,i,nulstr,guru[i]->name,guru[i]->ar);
				i=uselect(0,0,0,0,0);
				if(i<0)
					break; }
			if(gurubuf)
				FREE(gurubuf);
			sprintf(str,"%s%s.DAT",ctrl_dir,guru[i]->code);
			if((file=nopen(str,O_RDONLY))==-1) {
				errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
				return; }
			if((gurubuf=MALLOC(filelength(file)+1))==NULL) {
				close(file);
				errormsg(WHERE,ERR_ALLOC,str,filelength(file)+1);
				return; }
			read(file,gurubuf,filelength(file));
			gurubuf[filelength(file)]=0;
			close(file);
			localguru(gurubuf,i);
			no_rip_menu=1;
			FREE(gurubuf);
			gurubuf=NULL;
			break;
		case '?':
			if(useron.misc&EXPERT)
				menu("CHAT");
			break;
		default:	/* 'Q' or <CR> */
			lncntr=0;
			if(gurubuf)
				FREE(gurubuf);
			return; }
	action=NODE_CHAT;
	if(!(useron.misc&EXPERT) || useron.misc&WIP
		|| (useron.misc&RIP && !no_rip_menu)) {
#if 0
		sys_status&=~SS_ABORT;
		if(lncntr) {
			SYNC;
			CRLF;
			if(lncntr)			/* CRLF or SYNC can cause pause */
				pause(); }
#endif
		menu("CHAT"); }
	ASYNC;
	bputs(text[ChatPrompt]); }
if(gurubuf)
	FREE(gurubuf);
}

int getnodetopage(int all, int telegram)
{
	static	uchar last[26];
	char	str[128];
	int 	i,j;
	ulong	l;
	node_t	node;

if(!lastnodemsg)
	last[0]=0;
if(lastnodemsg) {
	getnodedat(lastnodemsg,&node,0);
    if(node.status!=NODE_INUSE && !SYSOP)
		lastnodemsg=1; }
for(j=0,i=1;i<=sys_nodes && i<=sys_lastnode;i++) {
	getnodedat(i,&node,0);
	if(i==node_num)
		continue;
	if(node.status==NODE_INUSE || (SYSOP && node.status==NODE_QUIET)) {
		if(!lastnodemsg)
			lastnodemsg=i;
		j++; } }

if(!last[0])
	sprintf(last,"%u",lastnodemsg);

if(!j && !telegram) {
    bputs(text[NoOtherActiveNodes]);
	return(0); }

if(all)
	sprintf(str,text[NodeToSendMsgTo],lastnodemsg);
else
	sprintf(str,text[NodeToPrivateChat],lastnodemsg);
mnemonics(str);

strcpy(str,last);
getstr(str,25,K_UPRLWR|K_LINE|K_EDIT|K_AUTODEL);
if(sys_status&SS_ABORT)
	return(0);
if(!str[0])
	return(0);

j=atoi(str);
if(j && j<=sys_lastnode && j<=sys_nodes) {
	getnodedat(j,&node,0);
	if(node.status!=NODE_INUSE && !SYSOP) {
		bprintf(text[NodeNIsNotInUse],j);
		return(0); }
	if(telegram && node.misc&(NODE_POFF|NODE_ANON) && !SYSOP) {
		bprintf(text[CantPageNode],node.misc&NODE_ANON
			? text[UNKNOWN_USER] : username(node.useron,tmp));
		return(0); }
	strcpy(last,str);
	if(telegram)
		return(node.useron);
	return(j); }
if(all && !stricmp(str,"ALL"))
	return(-1);

if(str[0]=='\'') {
	j=userdatdupe(0,U_HANDLE,LEN_HANDLE,str+1,0);
	if(!j) {
		bputs(text[UnknownUser]);
		return(0); } }
else if(str[0]=='#')
    j=atoi(str+1);
else
	j=finduser(str);
if(!j)
	return(0);
if(j>lastuser())
	return(0);
getuserrec(j,U_MISC,8,tmp);
l=ahtoul(tmp);
if(l&(DELETED|INACTIVE)) {              /* Deleted or Inactive User */
    bputs(text[UnknownUser]);
	return(0); }

for(i=1;i<=sys_nodes && i<=sys_lastnode;i++) {
	getnodedat(i,&node,0);
	if((node.status==NODE_INUSE || (SYSOP && node.status==NODE_QUIET))
		&& node.useron==j) {
		if(telegram && node.misc&NODE_POFF && !SYSOP) {
			bprintf(text[CantPageNode],node.misc&NODE_ANON
				? text[UNKNOWN_USER] : username(node.useron,tmp));
			return(0); }
		if(telegram)
			return(j);
		strcpy(last,str);
		return(i); } }
if(telegram) {
	strcpy(last,str);
	return(j); }
bputs(text[UserNotFound]);
return(0);
}


/****************************************************************************/
/* Sending single line messages between nodes                               */
/****************************************************************************/
void nodemsg()
{
	static	inside;
	char	str[256],line[256],buf[512],logbuf[512],ch;
	int 	i,j,usernumber,done=0;
	node_t	node,savenode;

if(inside>1)	/* nested once only */
	return;
sys_status|=SS_IN_CTRLP;
getnodedat(node_num,&savenode,0);
inside++;
wordwrap[0]=0;
while(online && !done) {
	if(useron.rest&FLAG('C')) {
		bputs(text[R_SendMessages]);
		break; }
	SYNC;
	mnemonics(text[PrivateMsgPrompt]);
	sys_status&=~SS_ABORT;
	while(online) { 	 /* Watch for incoming messages */
		ch=toupper(inkey(0));
		if(ch && strchr("TMCQ\r",ch))
			break;
		if(sys_status&SS_ABORT)
			break;
		if(online==ON_REMOTE && rioctl(IOSTATE)&ABORT) {
			sys_status|=SS_ABORT;
			break; }
		getnodedat(node_num,&thisnode,0);
		if(thisnode.misc&(NODE_MSGW|NODE_NMSG)) {
			SAVELINE;
			CRLF;
			if(thisnode.misc&NODE_NMSG)
				getnmsg();
			if(thisnode.misc&NODE_MSGW)
				getsmsg(useron.number);
			CRLF;
			RESTORELINE; }
		else
			nodesync();
		gettimeleft();
		checkline(); }

	if(!online || sys_status&SS_ABORT) {
		sys_status&=~SS_ABORT;
		CRLF;
		break; }

	switch(toupper(ch)) {
		case 'T':   /* Telegram */
			bputs("Telegram\r\n");
			usernumber=getnodetopage(0,1);
			if(!usernumber)
				break;

			if(usernumber==1 && useron.rest&FLAG('S')) { /* ! val fback */
				bprintf(text[R_Feedback],sys_op);
				break; }
			if(usernumber!=1 && useron.rest&FLAG('E')) {
				bputs(text[R_Email]);
				break; }
			now=time(NULL);
			bprintf("\1n\r\n\1mSending telegram to \1h%s #%u\1n\1m (Max 5 "
				"lines, Blank line ends):\r\n\r\n\1g\1h"
				,username(usernumber,tmp),usernumber);
			sprintf(buf,"\1n\1g\7Telegram from \1n\1h%s\1n\1g on %s:\r\n\1h"
				,thisnode.misc&NODE_ANON ? text[UNKNOWN_USER] : useron.alias
				,timestr(&now));
			i=0;
			logbuf[0]=0;
			while(online && i<5) {
				bprintf("%4s",nulstr);
				if(!getstr(line,70,K_WRAP|K_MSG))
					break;
				sprintf(str,"%4s%s\r\n",nulstr,line);
				strcat(buf,str);
				if(line[0])
					strcat(logbuf,line);
				i++; }
			if(!i)
				break;
			if(sys_status&SS_ABORT) {
				CRLF;
				break; }
			putsmsg(usernumber,buf);
			sprintf(str,"Sent telegram to %s #%u"
				,username(usernumber,tmp),usernumber);
			logline("C",str);
			logline(nulstr,logbuf);
			bprintf("\1n\1mTelegram sent to \1h%s\r\n"
				,username(usernumber,tmp));
			break;
		case 'M':   /* Message */
			bputs("Message\r\n");
			i=getnodetopage(1,0);
			if(!i)
				break;
			if(i!=-1) {
				getnodedat(i,&node,0);
				usernumber=node.useron;
				if(node.misc&NODE_POFF && !SYSOP)
					bprintf(text[CantPageNode],node.misc&NODE_ANON
						? text[UNKNOWN_USER] : username(node.useron,tmp));
				else {
					bprintf("\r\n\1n\1mSending message to \1h%s\r\n"
						,node.misc&NODE_ANON ? text[UNKNOWN_USER]
						: username(node.useron,tmp));
					bputs(text[NodeMsgPrompt]);
					if(!getstr(line,69,K_LINE))
						break;
					sprintf(buf,text[NodeMsgFmt],node_num
						,thisnode.misc&NODE_ANON
							? text[UNKNOWN_USER] : useron.alias,line);
					putnmsg(i,buf);
					if(!(node.misc&NODE_ANON))
						bprintf("\r\n\1n\1mMessage sent to \1h%s #%u\r\n"
							,username(usernumber,tmp),usernumber);
					sprintf(str,"Sent message to %s on node %d:"
						,username(usernumber,tmp),i);
					logline("C",str);
					logline(nulstr,line); } }
			else {	/* ALL */
				bputs(text[NodeMsgPrompt]);
				if(!getstr(line,70,K_LINE))
					break;
				sprintf(buf,text[AllNodeMsgFmt],node_num
					,thisnode.misc&NODE_ANON
						? text[UNKNOWN_USER] : useron.alias,line);
				for(i=1;i<=sys_nodes;i++) {
					if(i==node_num)
						continue;
					getnodedat(i,&node,0);
					if((node.status==NODE_INUSE
						|| (SYSOP && node.status==NODE_QUIET))
						&& (SYSOP || !(node.misc&NODE_POFF)))
						putnmsg(i,buf); }
				logline("C","Sent message to all nodes");
				logline(nulstr,line); }
			break;
		case 'C':   /* Chat */
			bputs("Chat\r\n");
			if(action==NODE_PCHT) { /* already in pchat */
				done=1;
				break; }
			privchat();
			action=savenode.action;
			break;
		default:
			bputs("Quit\r\n");
			done=1;
			break; } }
inside--;
if(!inside)
	sys_status&=~SS_IN_CTRLP;
getnodedat(node_num,&thisnode,1);
thisnode.action=action=savenode.action;
thisnode.aux=savenode.aux;
thisnode.extaux=savenode.extaux;
putnodedat(node_num,thisnode);
}

/****************************************************************************/
/* The guru will respond from the 'guru' buffer to 'line'                   */
/****************************************************************************/
void guruchat(char *line, char *gurubuf, int gurunum)
{
	char	str[256],cstr[256],*ptr,c,*answer[100],answers,theanswer[769]
			,mistakes=1,hu=0;
	int 	i,j,k,file;
	long 	len;
	struct	tm *tm;

now=time(NULL);
tm=localtime(&now);

for(i=0;i<100;i++) {
	if((answer[i]=MALLOC(513))==NULL) {
		errormsg(WHERE,ERR_ALLOC,nulstr,513);
		while(i) {
			i--;
			FREE(answer[i]); }
		sys_status&=~SS_GURUCHAT;
		return; } }
ptr=gurubuf;
len=strlen(gurubuf);
strupr(line);
j=strlen(line);
k=0;
for(i=0;i<j;i++) {
	if(!isalnum(line[i]) && !k)	/* beginning non-alphanumeric */
		continue;
	if(!isalnum(line[i]) && line[i]==line[i+1])	/* redundant non-alnum */
		continue;
	if(!isalnum(line[i]) && line[i+1]=='?')	/* fix "WHAT ?" */
		continue;
	cstr[k++]=line[i]; }
cstr[k]=0;
while(--k) {
	if(!isalnum(cstr[k]))
		continue;
	break; }
if(k<1) {
	for(i=0;i<100;i++)
		FREE(answer[i]);
	return; }
if(cstr[k+1]=='?')
	k++;
cstr[k+1]=0;
while(*ptr && ptr<gurubuf+len) {
	if(*ptr=='(') {
		ptr++;
		if(!guruexp(&ptr,cstr)) {
			while(*ptr && *ptr!='(' && ptr<gurubuf+len)
				ptr++;
			continue; } }
	else {
		while(*ptr && *ptr!=LF && ptr<gurubuf+len) /* skip LF after ')' */
			ptr++;
		ptr++;
		answers=0;
		while(*ptr && answers<100 && ptr<gurubuf+len) {
			i=0;
			while(*ptr && *ptr!=CR && i<512 && ptr<gurubuf+len) {
				answer[answers][i]=*ptr;
				ptr++;
				i++;
				if(*ptr=='\\' && *(ptr+1)==CR) {	/* multi-line answer */
					ptr+=3;	/* skip \CRLF */
					answer[answers][i++]=CR;
					answer[answers][i++]=LF; } }
			answer[answers][i]=0;
			if(!strlen(answer[answers]) || answer[answers][0]=='(') {
				ptr-=strlen(answer[answers]);
				break; }
			ptr+=2;	/* skip CRLF */
			answers++; }
		if(answers==100)
			while(*ptr && *ptr!='(' && ptr<gurubuf+len)
				ptr++;
		i=random(answers);
		for(j=0,k=0;j<strlen(answer[i]);j++) {
			if(answer[i][j]=='`') {
				j++;
				theanswer[k]=0;
				switch(toupper(answer[i][j])) {
					case 'A':
						if(sys_status&SS_USERON)
							strcat(theanswer,useron.alias);
						else
							strcat(theanswer,text[UNKNOWN_USER]);
						break;
					case 'B':
						if(sys_status&SS_USERON)
							strcat(theanswer,useron.birth);
						else
							strcat(theanswer,"00/00/00");
						break;
					case 'C':
                    	if(sys_status&SS_USERON)
							strcat(theanswer,useron.comp);
						else
							strcat(theanswer,"PC Jr.");
						break;
					case 'D':
                    	if(sys_status&SS_USERON)
							strcat(theanswer,ultoac(useron.dlb,tmp));
						else
							strcat(theanswer,"0");
						break;
					case 'G':
						strcat(theanswer,guru[gurunum]->name);
						break;
					case 'H':
						hu=1;
						break;
					case 'I':
						strcat(theanswer,sys_id);
						break;
					case 'J':
						sprintf(tmp,"%u",tm->tm_mday);
						break;
					case 'L':
                    	if(sys_status&SS_USERON)
							strcat(theanswer,itoa(useron.level,tmp,10));
						else
							strcat(theanswer,"0");
						break;
					case 'M':
						strcat(theanswer,month[tm->tm_mon]);
						break;
					case 'N':   /* Note */
                    	if(sys_status&SS_USERON)
							strcat(theanswer,useron.note);
						else
							strcat(theanswer,text[UNKNOWN_USER]);
						break;
					case 'O':
						strcat(theanswer,sys_op);
						break;
					case 'P':
                    	if(sys_status&SS_USERON)
							strcat(theanswer,useron.phone);
						else
							strcat(theanswer,"000-000-0000");
						break;
					case 'Q':
							sys_status&=~SS_GURUCHAT;
						break;
					case 'R':
                    	if(sys_status&SS_USERON)
							strcat(theanswer,useron.name);
						else
							strcat(theanswer,text[UNKNOWN_USER]);
						break;
					case 'S':
						strcat(theanswer,sys_name);
						break;
					case 'T':
						sprintf(tmp,"%u:%02u",tm->tm_hour>12 ? tm->tm_hour-12
							: tm->tm_hour,tm->tm_min);
						strcat(theanswer,tmp);
						break;
					case 'U':
                    	if(sys_status&SS_USERON)
							strcat(theanswer,ultoac(useron.ulb,tmp));
						else
							strcat(theanswer,"0");
						break;
					case 'W':
						strcat(theanswer,weekday[tm->tm_wday]);
						break;
					case 'Y':   /* Current year */
						sprintf(tmp,"%u",1900+tm->tm_year);
						strcat(theanswer,tmp);
						break;
					case 'Z':
						if(sys_status&SS_USERON)
							strcat(theanswer,useron.zipcode);
                        else
							strcat(theanswer,"90210");
						break;
					case '$':   /* Credits */
                    	if(sys_status&SS_USERON)
							strcat(theanswer,ultoac(useron.cdt,tmp));
						else
							strcat(theanswer,"0");
						break;
					case '#':
                    	if(sys_status&SS_USERON)
							strcat(theanswer,itoa(getage(useron.birth)
								,tmp,10));
						else
							strcat(theanswer,"0");
						break;
					case '!':
						mistakes=!mistakes;
						break;
					case '_':
						mswait(500);
						break; }
				k=strlen(theanswer); }
			else
				theanswer[k++]=answer[i][j]; }
		theanswer[k]=0;
		mswait(500+random(1000));	 /* thinking time */
		if(action!=NODE_MCHT) {
			for(i=0;i<k;i++) {
				if(mistakes && theanswer[i]!=theanswer[i-1] &&
					((!isalnum(theanswer[i]) && !random(100))
					|| (isalnum(theanswer[i]) && !random(30)))) {
					c=j=random(3)+1;	/* 1 to 3 chars */
					if(c<strcspn(theanswer+(i+1),"\0., "))
						c=j=1;
					while(j) {
						outchar(97+random(26));
						mswait(25+random(150));
						j--; }
					if(random(100)) {
						mswait(100+random(300));
						while(c) {
							bputs("\b \b");
							mswait(50+random(50));
							c--; } } }
				outchar(theanswer[i]);
				if(theanswer[i]==theanswer[i+1])
					mswait(25+random(50));
				else
					mswait(25+random(150));
				if(theanswer[i]==SP)
					mswait(random(50)); } }
		else {
			mswait(strlen(theanswer)*100);
			bprintf(text[ChatLineFmt],guru[gurunum]->name
				,sys_nodes+1,':',theanswer); }
		CRLF;
		sprintf(str,"%sGURU.LOG",data_dir);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1)
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
		else {
			if(action==NODE_MCHT) {
				sprintf(str,"[Multi] ");
				write(file,str,strlen(str)); }
			sprintf(str,"%s:\r\n",sys_status&SS_USERON
				? useron.alias : "UNKNOWN");
			write(file,str,strlen(str));
			write(file,line,strlen(line));
			if(action!=NODE_MCHT)
				write(file,crlf,2);
			sprintf(str,"%s:\r\n",guru[gurunum]->name);
			write(file,str,strlen(str));
			write(file,theanswer,strlen(theanswer));
			write(file,crlf,2);
			close(file); }
		if(hu)
			hangup();
		break; } }
for(i=0;i<100;i++)
	FREE(answer[i]);
}

/****************************************************************************/
/* An expression from the guru's buffer 'ptrptr' is evaluated and true or   */
/* false is returned.                                                       */
/****************************************************************************/
char guruexp(char **ptrptr, char *line)
{
	char	c,*cp,str[256],true=0,and=0,or=0,nest,*ar;

if((**ptrptr)==')') {	/* expressions of () are always true */
	(*ptrptr)++;
	return(1); }
while((**ptrptr)!=')' && (**ptrptr)) {
	if((**ptrptr)=='[') {
		(*ptrptr)++;
		sprintf(str,"%.128s",*ptrptr);
		while(**ptrptr && (**ptrptr)!=']')
            (*ptrptr)++;
        (*ptrptr)++;
		cp=strchr(str,']');
		if(cp) *cp=0;
		ar=arstr(NULL,str);
		c=chk_ar(ar,useron);
		if(ar[0]!=AR_NULL)
			FREE(ar);
		if(!c && and) {
			true=0;
            break; }
		if(c && or) {
			true=1;
			break; }
		if(c)
			true=1;
		continue; }
	if((**ptrptr)=='(') {
		(*ptrptr)++;
		c=guruexp(&(*ptrptr),line);
		if(!c && and) {
			true=0;
			break; }
		if(c && or) {
			true=1;
			break; }
		if(c)
			true=1;	}
	if((**ptrptr)==')')
		break;
	c=0;
	while((**ptrptr) && isspace(**ptrptr))
		(*ptrptr)++;
	while((**ptrptr)!='|' && (**ptrptr)!='&' && (**ptrptr)!=')' &&(**ptrptr)) {
		str[c++]=(**ptrptr);
		(*ptrptr)++; }
	str[c]=0;
	if((**ptrptr)=='|') {
		if(!c && true)
			break;
		and=0;
		or=1; }
	else if((**ptrptr)=='&') {
		if(!c && !true)
			break;
		and=1;
		or=0; }
	if(!c) {				/* support ((exp)op(exp)) */
		(*ptrptr)++;
		continue; }
	if((**ptrptr)!=')')
		(*ptrptr)++;
	c=0;					/* c now used for start line flag */
	if(str[strlen(str)-1]=='^') {	/* ^signifies start of line only */
		str[strlen(str)-1]=0;
		c=1; }
	if(str[strlen(str)-1]=='~') {	/* ~signifies non-isolated word */
		str[strlen(str)-1]=0;
		cp=strstr(line,str);
		if(c && cp!=line)
			cp=0; }
	else {
		cp=strstr(line,str);
		if(cp && c) {
			if(cp!=line || isalnum(*(cp+strlen(str))))
				cp=0; }
		else {	/* must be isolated word */
			while(cp)
				if((cp!=line && isalnum(*(cp-1)))
					|| isalnum(*(cp+strlen(str))))
					cp=strstr(cp+strlen(str),str);
				else
					break; } }
	if(!cp && and) {
		true=0;
		break; }
	if(cp && or) {
		true=1;
		break; }
	if(cp)
		true=1; }
nest=0;
while((**ptrptr)!=')' && (**ptrptr)) {		/* handle nested exp */
	if((**ptrptr)=='(')						/* (TRUE|(IGNORE)) */
		nest++;
	(*ptrptr)++;
	while((**ptrptr)==')' && nest && (**ptrptr)) {
		nest--;
		(*ptrptr)++; } }
(*ptrptr)++;	/* skip over ')' */
return(true);
}

/****************************************************************************/
/* Guru chat with the appearance of Local chat with sysop.                  */
/****************************************************************************/
void localguru(char *gurubuf, int gurunum)
{
	char	ch,str[256];
	int 	con=console;		 /* save console state */

if(sys_status&SS_GURUCHAT || !total_gurus)
	return;
sys_status|=SS_GURUCHAT;
console&=~(CON_L_ECHOX|CON_R_ECHOX);	/* turn off X's */
console|=(CON_L_ECHO|CON_R_ECHO);					/* make sure echo is on */
if(action==NODE_CHAT) {	/* only page if from chat section */
	bprintf(text[PagingGuru],guru[gurunum]->name);
	ch=random(25)+25;
	while(ch--) {
		mswait(200);
		outchar('.'); } }
bprintf(text[SysopIsHere],guru[gurunum]->name);
getnodedat(node_num,&thisnode,1);
thisnode.aux=gurunum;
putnodedat(node_num,thisnode);
attr(color[clr_chatlocal]);
guruchat("HELLO",gurubuf,gurunum);
while(online && (sys_status&SS_GURUCHAT)) {
	checkline();
	action=NODE_GCHT;
	SYNC;
	if((ch=inkey(0))!=0) {
		ungetkey(ch);
		attr(color[clr_chatremote]);
		if(getstr(str,78,K_WRAP|K_CHAT)) {
			attr(color[clr_chatlocal]);
			guruchat(str,gurubuf,gurunum); } }
	else
		mswait(1); }
bputs(text[EndOfChat]);
sys_status&=~SS_GURUCHAT;
console=con;				/* restore console state */
}

/****************************************************************************/
/* Packs the password 'pass' into 5bit ASCII inside node_t. 32bits in 		*/
/* node.extaux, and the other 8bits in the upper byte of node.aux			*/
/****************************************************************************/
void packchatpass(char *pass, node_t *node)
{
	char	bits;
	int		i,j;

node->aux&=~0xff00;		/* clear the password */
node->extaux=0L;
if((j=strlen(pass))==0) /* there isn't a password */
	return;
node->aux|=(int)((pass[0]-64)<<8);  /* 1st char goes in low 5bits of aux */
if(j==1)	/* password is only one char, we're done */
	return;
node->aux|=(int)((pass[1]-64)<<13); /* low 3bits of 2nd char go in aux */
node->extaux|=(long)((pass[1]-64)>>3); /* high 2bits of 2nd char go extaux */
bits=2;
for(i=2;i<j;i++) {	/* now process the 3rd char through the last */
	node->extaux|=(long)((long)(pass[i]-64)<<bits);
	bits+=5; }
}

/****************************************************************************/
/* Unpacks the password 'pass' from the 5bit ASCII inside node_t. 32bits in */
/* node.extaux, and the other 8bits in the upper byte of node.aux			*/
/****************************************************************************/
char *unpackchatpass(char *pass, node_t node)
{
	char 	bits;
	int 	i;

pass[0]=(node.aux&0x1f00)>>8;
pass[1]=((node.aux&0xe000)>>13)|((node.extaux&0x3)<<3);
bits=2;
for(i=2;i<8;i++) {
	pass[i]=(node.extaux>>bits)&0x1f;
	bits+=5; }
pass[8]=0;
for(i=0;i<8;i++)
	if(pass[i])
		pass[i]+=64;
return(pass);
}
