#line 2 "SCFGCHAT.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "scfg.h"

void page_cfg()
{
	static int dflt,bar;
	char str[81],done=0;
	int j,k;
	uint i;
	static page_t savpage;

while(1) {
	for(i=0;i<total_pages && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-40.40s %.-20s",page[i]->cmd,page[i]->ar);
	opt[i][0]=0;
	savnum=0;
	j=WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT;
	if(total_pages)
		j|=WIN_DEL|WIN_GET;
	if(total_pages<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savpage.cmd[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
External Sysop Chat Pagers:

This is a list of the configured external sysop chat pagers.

To add a pager, select the desired location and hit  INS .

To delete a pager, select it and hit  DEL .

To configure a pager, select it and hit  ENTER .
*/
	i=ulist(j,0,0,45,&dflt,&bar,"External Sysop Chat Pagers",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		sprintf(str,"%%!tone +chatpage.ton");
		SETHELP(WHERE);
/*
External Chat Pager Command Line:

This is the command line to execute for this external chat pager.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Command Line",str,50
			,K_EDIT)<1)
            continue;
		if((page=(page_t **)REALLOC(page,sizeof(page_t *)*(total_pages+1)))
            ==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,total_pages+1);
			total_pages=0;
			bail(1);
            continue; }
		if(total_pages)
			for(j=total_pages;j>i;j--)
				page[j]=page[j-1];
		if((page[i]=(page_t *)MALLOC(sizeof(page_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(page_t));
			continue; }
		memset((page_t *)page[i],0,sizeof(page_t));
		strcpy(page[i]->cmd,str);
		total_pages++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(page[i]);
		total_pages--;
		for(j=i;j<total_pages;j++)
			page[j]=page[j+1];
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savpage=*page[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*page[i]=savpage;
		changes=1;
        continue; }
	j=0;
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%.40s","Command Line",page[i]->cmd);
		sprintf(opt[k++],"%-27.27s%.40s","Access Requirements",page[i]->ar);
		sprintf(opt[k++],"%-27.27s%s","Intercept I/O Interrupts"
			,page[i]->misc&IO_INTS ? "Yes":"No");
		opt[k][0]=0;
		sprintf(str,"Sysop Chat Pager #%d",i+1);
		savnum=1;
		switch(ulist(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&j,0,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
External Chat Pager Command Line:

This is the command line to execute for this external chat pager.
*/
				strcpy(str,page[i]->cmd);
				if(!uinput(WIN_MID|WIN_SAV,0,10,"Command Line"
					,page[i]->cmd,50,K_EDIT))
					strcpy(page[i]->cmd,str);
				break;
			case 1:
				savnum=2;
				getar(str,page[i]->ar);
				break;
			case 2:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				savnum=2;
				SETHELP(WHERE);
/*
Intercept I/O Interrupts:

If you wish the DOS screen output and keyboard input to be intercepted
when running this chat pager, set this option to Yes.
*/
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0,"Intercept I/O Interrupts"
					,opt);
				if(!k && !(page[i]->misc&IO_INTS)) {
					page[i]->misc|=IO_INTS;
					changes=1; }
				else if(k==1 && page[i]->misc&IO_INTS) {
					page[i]->misc&=~IO_INTS;
					changes=1; }
                break;

				} } }
}

void chan_cfg()
{
	static int chan_dflt,chan_bar,opt_dflt;
	char str[81],code[9],done=0,*p;
	int j,k;
	uint i;
	static chan_t savchan;

while(1) {
	for(i=0;i<total_chans && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-25s",chan[i]->name);
	opt[i][0]=0;
	j=WIN_ACT|WIN_SAV|WIN_BOT|WIN_RHT;
	savnum=0;
	if(total_chans)
		j|=WIN_DEL|WIN_GET;
	if(total_chans<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savchan.name[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
Multinode Chat Channels:

This is a list of the configured multinode chat channels.

To add a channel, select the desired location with the arrow keys and
hit  INS .

To delete a channel, select it with the arrow keys and hit  DEL .

To configure a channel, select it with the arrow keys and hit  ENTER .
*/
	i=ulist(j,0,0,45,&chan_dflt,&chan_bar,"Multinode Chat Channels",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		strcpy(str,"Open");
		SETHELP(WHERE);
/*
Channel Name:

This is the name or description of the chat channel.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Chat Channel Name",str,25
			,K_EDIT)<1)
            continue;
		sprintf(code,"%.8s",str);
		p=strchr(code,SP);
		if(p) *p=0;
        strupr(code);
		SETHELP(WHERE);
/*
Chat Channel Internal Code:

Every chat channel must have its own unique code for Synchronet to refer
to it internally. This code is usually an abreviation of the chat
channel name.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Internal Code"
			,code,8,K_EDIT|K_UPPER)<1)
			continue;
		if(!code_ok(code)) {
			helpbuf=invalid_code;
			umsg("Invalid Code");
			helpbuf=0;
            continue; }
		if((chan=(chan_t **)REALLOC(chan,sizeof(chan_t *)*(total_chans+1)))
            ==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,total_chans+1);
			total_chans=0;
			bail(1);
            continue; }
		if(total_chans)
			for(j=total_chans;j>i;j--)
				chan[j]=chan[j-1];
		if((chan[i]=(chan_t *)MALLOC(sizeof(chan_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(chan_t));
			continue; }
		memset((chan_t *)chan[i],0,sizeof(chan_t));
		strcpy(chan[i]->name,str);
		strcpy(chan[i]->code,code);
		total_chans++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(chan[i]);
		total_chans--;
		for(j=i;j<total_chans;j++)
			chan[j]=chan[j+1];
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savchan=*chan[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*chan[i]=savchan;
		changes=1;
        continue; }
    j=0;
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%s","Name",chan[i]->name);
		sprintf(opt[k++],"%-27.27s%s","Internal Code",chan[i]->code);
		sprintf(opt[k++],"%-27.27s%lu","Cost in Credits",chan[i]->cost);
		sprintf(opt[k++],"%-27.27s%.40s","Access Requirements"
			,chan[i]->ar);
		sprintf(opt[k++],"%-27.27s%s","Password Protection"
			,chan[i]->misc&CHAN_PW ? "Yes" : "No");
		sprintf(opt[k++],"%-27.27s%s","Guru Joins When Empty"
			,chan[i]->misc&CHAN_GURU ? "Yes" : "No");
		sprintf(opt[k++],"%-27.27s%s","Channel Guru"
			,chan[i]->guru<total_gurus ? guru[chan[i]->guru]->name : "");
        sprintf(opt[k++],"%-27.27s%s","Channel Action Set"
            ,actset[chan[i]->actset]->name);
		opt[k][0]=0;
		SETHELP(WHERE);
/*
Chat Channel Configuration:

This menu is for configuring the selected chat channel.
*/
		savnum=1;
		sprintf(str,"%s Chat Channel",chan[i]->name);
		switch(ulist(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&opt_dflt,0,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Chat Channel Name:

This is the name or description of the chat channel.
*/
				strcpy(str,chan[i]->name);
				if(!uinput(WIN_MID|WIN_SAV,0,10,"Chat Channel Name"
					,chan[i]->name,25,K_EDIT))
					strcpy(chan[i]->name,str);
				break;
			case 1:
				SETHELP(WHERE);
/*
Chat Channel Internal Code:

Every chat channel must have its own unique code for Synchronet to refer
to it internally. This code is usually an abreviation of the chat
channel name.
*/
				strcpy(str,chan[i]->code);
				if(!uinput(WIN_MID|WIN_SAV,0,10,"Internal Code"
					,str,8,K_UPPER|K_EDIT))
					break;
				if(code_ok(str))
					strcpy(chan[i]->code,str);
				else {
					helpbuf=invalid_code;
					umsg("Invalid Code");
                    helpbuf=0; }
                break;
			case 2:
				ultoa(chan[i]->cost,str,10);
                SETHELP(WHERE);
/*
Chat Channel Cost to Join:

If you want users to be charged credits to join this chat channel, set
this value to the number of credits to charge. If you want this channel
to be free, set this value to 0.
*/
				uinput(WIN_MID|WIN_SAV,0,0,"Cost to Join (in Credits)"
                    ,str,10,K_EDIT|K_NUMBER);
				chan[i]->cost=atol(str);
                break;
			case 3:
				savnum=2;
				sprintf(str,"%s Chat Channel",chan[i]->name);
				getar(str,chan[i]->ar);
				break;
			case 4:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				savnum=2;
				SETHELP(WHERE);
/*
Allow Channel to be Password Protected:

If you want to allow the first user to join this channel to password
protect it, set this option to Yes.
*/
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Allow Channel to be Password Protected"
					,opt);
				if(!k && !(chan[i]->misc&CHAN_PW)) {
					chan[i]->misc|=CHAN_PW;
					changes=1; }
				else if(k==1 && chan[i]->misc&CHAN_PW) {
					chan[i]->misc&=~CHAN_PW;
					changes=1; }
				break;
			case 5:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				savnum=2;
				SETHELP(WHERE);
/*
Guru Joins This Channel When Empty:

If you want the system guru to join this chat channel when there is
only one user, set this option to Yes.
*/
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Guru Joins This Channel When Empty"
					,opt);
				if(!k && !(chan[i]->misc&CHAN_GURU)) {
					chan[i]->misc|=CHAN_GURU;
					changes=1; }
				else if(k==1 && chan[i]->misc&CHAN_GURU) {
					chan[i]->misc&=~CHAN_GURU;
					changes=1; }
				break;
			case 6:
SETHELP(WHERE);
/*
Channel Guru:

This is a list of available chat Gurus.  Select the one that you wish
to have available in this channel.
*/
				k=0;
				for(j=0;j<total_gurus && j<MAX_OPTS;j++)
					sprintf(opt[j],"%-25s",guru[j]->name);
				opt[j][0]=0;
				savnum=2;
				k=ulist(WIN_SAV|WIN_RHT,0,0,25,&j,0
					,"Available Chat Gurus",opt);
				if(k==-1)
					break;
				chan[i]->guru=k;
				break;
			case 7:
SETHELP(WHERE);
/*
Channel Action Set:

This is a list of available chat action sets.  Select the one that you wish
to have available in this channel.
*/
				k=0;
				for(j=0;j<total_actsets && j<MAX_OPTS;j++)
					sprintf(opt[j],"%-25s",actset[j]->name);
				opt[j][0]=0;
				savnum=2;
				k=ulist(WIN_SAV|WIN_RHT,0,0,25,&j,0
					,"Available Chat Action Sets",opt);
				if(k==-1)
					break;
				changes=1;
				chan[i]->actset=k;
				break; } } }
}

void chatact_cfg(uint setnum)
{
	static int chatact_dflt,chatact_bar;
	char str[128],cmd[128],out[128];
	int j,k;
	uint i,n,chatnum[MAX_OPTS+1];
	static chatact_t savchatact;

while(1) {
	for(i=0,j=0;i<total_chatacts && j<MAX_OPTS;i++)
		if(chatact[i]->actset==setnum) {
			sprintf(opt[j],"%-*.*s %s",LEN_CHATACTCMD,LEN_CHATACTCMD
				,chatact[i]->cmd,chatact[i]->out);
			chatnum[j++]=i; }
	chatnum[j]=total_chatacts;
	opt[j][0]=0;
	savnum=2;
	i=WIN_ACT|WIN_SAV;
	if(j)
		i|=WIN_DEL|WIN_GET;
	if(j<MAX_OPTS)
		i|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savchatact.cmd[0])
		i|=WIN_PUT;
	SETHELP(WHERE);
/*
Multinode Chat Actions:

This is a list of the configured multinode chat actions.  The users can
use these actions in multinode chat by turning on action commands with
the /A command in multinode chat.  Then if a line is typed which
begins with a valid action command and has a user name, chat handle,
or node number following, the output string will be displayed replacing
the %s symbols with the sending user's name and the receiving user's
name (in that order).

To add an action, select the desired location with the arrow keys and
hit  INS .

To delete an action, select it with the arrow keys and hit  DEL .

To configure an action, select it with the arrow keys and hit  ENTER .
*/
	sprintf(str,"%s Chat Actions",actset[setnum]->name);
	i=ulist(i,0,0,70,&chatact_dflt,&chatact_bar,str,opt);
	savnum=3;
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Chat Action Command:

This is the command word (normally a verb) to trigger the action output.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Action Command",cmd,LEN_CHATACTCMD
			,K_UPPER)<1)
            continue;
		SETHELP(WHERE);
/*
Chat Action Output String:

This is the output string displayed with this action output.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"",out,LEN_CHATACTOUT
			,K_MSG)<1)
            continue;
		if((chatact=(chatact_t **)REALLOC(chatact
            ,sizeof(chatact_t *)*(total_chatacts+1)))==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,total_chatacts+1);
			total_chatacts=0;
			bail(1);
            continue; }
		if(j)
			for(n=total_chatacts;n>chatnum[i];n--)
				chatact[n]=chatact[n-1];
		if((chatact[chatnum[i]]=(chatact_t *)MALLOC(sizeof(chatact_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(chatact_t));
			continue; }
		memset((chatact_t *)chatact[chatnum[i]],0,sizeof(chatact_t));
		strcpy(chatact[chatnum[i]]->cmd,cmd);
		strcpy(chatact[chatnum[i]]->out,out);
		chatact[chatnum[i]]->actset=setnum;
		total_chatacts++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(chatact[chatnum[i]]);
		total_chatacts--;
		for(j=chatnum[i];j<total_chatacts && j<MAX_OPTS;j++)
			chatact[j]=chatact[j+1];
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savchatact=*chatact[chatnum[i]];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*chatact[chatnum[i]]=savchatact;
		chatact[chatnum[i]]->actset=setnum;
		changes=1;
        continue; }
	SETHELP(WHERE);
/*
Chat Action Command:

This is the command that triggers this chat action.
*/
	strcpy(str,chatact[chatnum[i]]->cmd);
	if(!uinput(WIN_MID|WIN_SAV,0,10,"Chat Action Command"
		,chatact[chatnum[i]]->cmd,LEN_CHATACTCMD,K_EDIT|K_UPPER)) {
		strcpy(chatact[chatnum[i]]->cmd,str);
		continue; }
	SETHELP(WHERE);
/*
Chat Action Output String:

This is the output string that results from this chat action.
*/
	strcpy(str,chatact[chatnum[i]]->out);
	if(!uinput(WIN_MID|WIN_SAV,0,10,""
		,chatact[chatnum[i]]->out,LEN_CHATACTOUT,K_EDIT|K_MSG))
		strcpy(chatact[chatnum[i]]->out,str); }
}

void guru_cfg()
{
	static int guru_dflt,guru_bar,opt_dflt;
	char str[81],code[9],done=0,*p;
	int j,k;
	uint i;
	static guru_t savguru;

while(1) {
	for(i=0;i<total_gurus && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-25s",guru[i]->name);
	opt[i][0]=0;
	savnum=0;
	j=WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT;
	if(total_gurus)
		j|=WIN_DEL|WIN_GET;
	if(total_gurus<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savguru.name[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
Gurus:

This is a list of the configured Gurus.

To add a Guru, select the desired location with the arrow keys and
hit  INS .

To delete a Guru, select it with the arrow keys and hit  DEL .

To configure a Guru, select it with the arrow keys and hit  ENTER .
*/
	i=ulist(j,0,0,45,&guru_dflt,&guru_bar,"Artificial Gurus",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Guru Name:

This is the name of the selected Guru.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Guru Name",str,25
			,0)<1)
            continue;
		sprintf(code,"%.8s",str);
		p=strchr(code,SP);
		if(p) *p=0;
        strupr(code);
		SETHELP(WHERE);
/*
Guru Internal Code:

Every Guru must have its own unique code for Synchronet to refer to
it internally. This code is usually an abreviation of the Guru name.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Internal Code"
			,code,8,K_EDIT|K_UPPER)<1)
			continue;
		if(!code_ok(code)) {
			helpbuf=invalid_code;
			umsg("Invalid Code");
			helpbuf=0;
            continue; }
		if((guru=(guru_t **)REALLOC(guru,sizeof(guru_t *)*(total_gurus+1)))
            ==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,total_gurus+1);
			total_gurus=0;
			bail(1);
            continue; }
		if(total_gurus)
			for(j=total_gurus;j>i;j--)
				guru[j]=guru[j-1];
		if((guru[i]=(guru_t *)MALLOC(sizeof(guru_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(guru_t));
			continue; }
		memset((guru_t *)guru[i],0,sizeof(guru_t));
		strcpy(guru[i]->name,str);
		strcpy(guru[i]->code,code);
		total_gurus++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(guru[i]);
		total_gurus--;
		for(j=i;j<total_gurus;j++)
			guru[j]=guru[j+1];
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savguru=*guru[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*guru[i]=savguru;
		changes=1;
        continue; }
    j=0;
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%s","Guru Name",guru[i]->name);
		sprintf(opt[k++],"%-27.27s%s","Guru Internal Code",guru[i]->code);
		sprintf(opt[k++],"%-27.27s%.40s","Access Requirements",guru[i]->ar);
		opt[k][0]=0;
		savnum=1;
		SETHELP(WHERE);
/*
Guru Configuration:

This menu is for configuring the selected Guru.
*/
		switch(ulist(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&opt_dflt,0,guru[i]->name
			,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Guru Name:

This is the name of the selected Guru.
*/
				strcpy(str,guru[i]->name);
				if(!uinput(WIN_MID|WIN_SAV,0,10,"Guru Name"
					,guru[i]->name,25,K_EDIT))
					strcpy(guru[i]->name,str);
				break;
			case 1:
SETHELP(WHERE);
/*
Guru Internal Code:

Every Guru must have its own unique code for Synchronet to refer to
it internally. This code is usually an abreviation of the Guru name.
*/
				strcpy(str,guru[i]->code);
				if(!uinput(WIN_MID|WIN_SAV,0,0,"Guru Internal Code"
					,str,8,K_EDIT|K_UPPER))
					break;
				if(code_ok(str))
					strcpy(guru[i]->code,str);
				else {
					helpbuf=invalid_code;
					umsg("Invalid Code");
                    helpbuf=0; }
				break;
			case 2:
				savnum=2;
				getar(guru[i]->name,guru[i]->ar);
				break; } } }
}

void actsets_cfg()
{
    static int actset_dflt,actset_bar,opt_dflt;
    char str[81];
    int j,k,done;
    uint i;
    static actset_t savactset;

while(1) {
	for(i=0;i<total_actsets && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-25s",actset[i]->name);
	opt[i][0]=0;
	j=WIN_ACT|WIN_RHT|WIN_BOT|WIN_SAV;
	savnum=0;
    if(total_actsets)
        j|=WIN_DEL|WIN_GET;
	if(total_actsets<MAX_OPTS)
        j|=WIN_INS|WIN_INSACT|WIN_XTR;
    if(savactset.name[0])
        j|=WIN_PUT;
    SETHELP(WHERE);
/*
Chat Action Sets:

This is a list of the configured action sets.

To add an action set, select the desired location with the arrow keys and
hit  INS .

To delete an action set, select it with the arrow keys and hit  DEL .

To configure an action set, select it with the arrow keys and hit  ENTER .
*/
	i=ulist(j,0,0,45,&actset_dflt,&actset_bar,"Chat Action Sets",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
        SETHELP(WHERE);
/*
Chat Action Set Name:

This is the name of the selected chat action set.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Chat Action Set Name",str,25
			,0)<1)
            continue;
        if((actset=(actset_t **)REALLOC(actset,sizeof(actset_t *)*(total_actsets+1)))
            ==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,total_actsets+1);
			total_actsets=0;
			bail(1);
            continue; }
        if(total_actsets)
            for(j=total_actsets;j>i;j--)
                actset[j]=actset[j-1];
        if((actset[i]=(actset_t *)MALLOC(sizeof(actset_t)))==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(actset_t));
            continue; }
		memset((actset_t *)actset[i],0,sizeof(actset_t));
        strcpy(actset[i]->name,str);
        total_actsets++;
        changes=1;
        continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
        FREE(actset[i]);
        total_actsets--;
        for(j=i;j<total_actsets;j++)
            actset[j]=actset[j+1];
        changes=1;
        continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
        savactset=*actset[i];
        continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
        *actset[i]=savactset;
        changes=1;
        continue; }
    j=0;
    done=0;
    while(!done) {
        k=0;
        sprintf(opt[k++],"%-27.27s%s","Action Set Name",actset[i]->name);
        sprintf(opt[k++],"%-27.27s","Configure Chat Actions...");
		opt[k][0]=0;
        SETHELP(WHERE);
/*
Chat Action Set Configuration:

This menu is for configuring the selected chat action set.
*/
		sprintf(str,"%s Chat Action Set",actset[i]->name);
		savnum=1;
		switch(ulist(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&opt_dflt,0,str
            ,opt)) {
            case -1:
                done=1;
                break;
            case 0:
                SETHELP(WHERE);
/*
Chat Action Set Name:

This is the name of the selected action set.
*/
                strcpy(str,actset[i]->name);
				if(!uinput(WIN_MID|WIN_SAV,0,10,"Action Set Name"
                    ,actset[i]->name,25,K_EDIT))
                    strcpy(actset[i]->name,str);
                break;
            case 1:
                chatact_cfg(i);
                break; } } }
}

