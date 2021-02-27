#line 1 "EXECFUNC.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "cmdshell.h"

int exec_function(csi_t *csi)
{
	uchar	str[256],tmp2[128],ch,*p;
	int 	i,j,k,s,file;
	long	l;
	stats_t stats;
	node_t	node;
	file_t	f;
	time_t	t;

switch(*(csi->ip++)) {


	case CS_PRINTFILE_STR:
		printfile(csi->str,P_NOATCODES);
		return(0);

/********************************/
/* General Main Menu Type Stuff */
/********************************/
	case CS_AUTO_MESSAGE:
		automsg();
		return(0);
	case CS_MINUTE_BANK:
		if(sys_misc&SM_TIMEBANK) {	/* Allow users to deposit free time */
			s=(level_timeperday[useron.level]-useron.ttoday)+useron.textra;
			if(s<0) s=0;
			if(s>level_timepercall[useron.level])
				s=level_timepercall[useron.level];
			s-=(now-starttime)/60;
			if(s<0) s=0;
			bprintf(text[FreeMinLeft],s);
			bprintf(text[UserMinutes],ultoac(useron.min,tmp));
			if(max_minutes && useron.min>=max_minutes) {
				bputs(text[YouHaveTooManyMinutes]);
				return(0); }
			if(max_minutes)
				while(s>0 && s+useron.min>max_minutes) s--;
			bprintf(text[FreeMinToDeposit],s);
			s=getnum(s);
			if(s>0) {
				logline("  ","Minute Bank Deposit");
				useron.min=adjustuserrec(useron.number,U_MIN,10,s);
				useron.ttoday=adjustuserrec(useron.number,U_TTODAY,10,s);
				sprintf(str,"Minute Adjustment: %u",s*cdt_min_value);
				logline("*+",str); } }

		if(!(sys_misc&SM_NOCDTCVT)) {
			bprintf(text[ConversionRate],cdt_min_value);
			bprintf(text[UserCredits]
				,ultoac(useron.cdt,tmp)
				,ultoac(useron.freecdt,tmp2)
				,ultoac(level_freecdtperday[useron.level],str));
			bprintf(text[UserMinutes],ultoac(useron.min,tmp));
			if(useron.cdt/102400L<1L) {
				bprintf(text[YouOnlyHaveNCredits],ultoac(useron.cdt,tmp));
				return(0); }
			if(max_minutes && useron.min>=max_minutes) {
				bputs(text[YouHaveTooManyMinutes]);
				return(0); }
			s=useron.cdt/102400L;
			if(max_minutes)
				while(s>0 && (s*cdt_min_value)+useron.min>max_minutes) s--;
			bprintf(text[CreditsToMin],s);
			s=getnum(s);
			if(s>0) {
				logline("  ","Credit to Minute Conversion");
				useron.cdt=adjustuserrec(useron.number,U_CDT,10,-(s*102400L));
				useron.min=adjustuserrec(useron.number,U_MIN,10,s*cdt_min_value);
				sprintf(str,"Credit Adjustment: %ld",-(s*102400L));
				logline("$-",str);
				sprintf(str,"Minute Adjustment: %u",s*cdt_min_value);
				logline("*+",str); } }
		return(0);
	case CS_CHAT_SECTION:
		if(useron.rest&FLAG('C'))
			bputs(text[R_Chat]);
		else
			chatsection();
		return(0);
	case CS_USER_DEFAULTS:
		maindflts(useron);
		if(!(useron.rest&FLAG('G')))    /* not guest */
			getuserdat(&useron);
		return(0);
	case CS_TEXT_FILE_SECTION:
		text_sec();
		return(0);
	case CS_INFO_SYSTEM:   /* System information */
		bputs(text[SiHdr]);
		getstats(0,&stats);
		bprintf(text[SiSysName],sys_name);
		bprintf(text[SiSysID],sys_id);	/* QWK ID */
		for(i=0;i<total_faddrs;i++)
			bprintf(text[SiSysFaddr],faddrtoa(faddr[i]));
		if(sys_psname[0])				/* PostLink/PCRelay */
			bprintf(text[SiSysPsite],sys_psname,sys_psnum);
		bprintf(text[SiSysLocation],sys_location);
		bprintf(text[SiSysop],sys_op);
		bprintf(text[SiSysNodes],sys_nodes);
		bprintf(text[SiNodeNumberName],node_num,node_name);
		bprintf(text[SiNodePhone],node_phone);
		bprintf(text[SiTotalLogons],ultoac(stats.logons,tmp));
		bprintf(text[SiLogonsToday],ultoac(stats.ltoday,tmp));
		bprintf(text[SiTotalTime],ultoac(stats.timeon,tmp));
		bprintf(text[SiTimeToday],ultoac(stats.ttoday,tmp));
		ver();
		if(yesno(text[ViewSysInfoFileQ])) {
			CLS;
			sprintf(str,"%sSYSTEM.MSG",text_dir);
			printfile(str,0); }
		if(yesno(text[ViewLogonMsgQ])) {
			CLS;
			menu("LOGON"); }
		return(0);
	case CS_INFO_SUBBOARD:	 /* Sub-board information */
		if(!usrgrps) return(0);
		subinfo(usrsub[curgrp][cursub[curgrp]]);
		return(0);
	case CS_INFO_DIRECTORY:   /* Sub-board information */
		if(!usrlibs) return(0);
		dirinfo(usrdir[curlib][curdir[curlib]]);
		return(0);
	case CS_INFO_VERSION:	/* Version */
		ver();
		return(0);
	case CS_INFO_USER:	 /* User's statistics */
		bprintf(text[UserStats],useron.alias,useron.number);
		unixtodos(useron.laston,&date,&curtime);
		bprintf(text[UserDates]
			,unixtodstr(useron.firston,str)
			,unixtodstr(useron.expire,tmp)
			,unixtodstr(useron.laston,tmp2)
			,curtime.ti_hour,curtime.ti_min);
		bprintf(text[UserTimes]
			,useron.timeon,useron.ttoday
			,level_timeperday[useron.level]
			,useron.tlast
			,level_timepercall[useron.level]
			,useron.textra);
		bprintf(text[UserLogons]
			,useron.logons,useron.ltoday
			,level_callsperday[useron.level],useron.posts
			,i ? 100/i : useron.posts>useron.logons ? 100 : 0
			,useron.ptoday);
		bprintf(text[UserEmails]
			,useron.emails,useron.fbacks
			,getmail(useron.number,0),useron.etoday);
		CRLF;
		bprintf(text[UserUploads]
			,ultoac(useron.ulb,tmp),useron.uls);
		bprintf(text[UserDownloads]
			,ultoac(useron.dlb,tmp),useron.dls,nulstr);
		bprintf(text[UserCredits],ultoac(useron.cdt,tmp)
			,ultoac(useron.freecdt,tmp2)
			,ultoac(level_freecdtperday[useron.level],str));
		bprintf(text[UserMinutes],ultoac(useron.min,tmp));
		return(0);
	case CS_INFO_XFER_POLICY:
		if(!usrlibs) return(0);
		sprintf(str,"%sMENU\\TPOLICY.*",text_dir);
		if(fexist(str))
			menu("TPOLICY");
		else {
			bprintf(text[TransferPolicyHdr],sys_name);
			bprintf(text[TpUpload]
				,dir[usrdir[curlib][curdir[curlib]]]->up_pct);
			bprintf(text[TpDownload]
				,dir[usrdir[curlib][curdir[curlib]]]->dn_pct);
			}
		return(0);
	case CS_XTRN_EXEC:
		csi->logic=LOGIC_TRUE;
		for(i=0;i<total_xtrns;i++)
			if(!stricmp(xtrn[i]->code,csi->str))
				break;
		if(i<total_xtrns)
			exec_xtrn(i);
		else
			csi->logic=LOGIC_FALSE;
		return(0);
	case CS_XTRN_SECTION:
		if(useron.rest&FLAG('X'))
			bputs(text[R_ExternalPrograms]);
		else
			xtrn_sec(); 	/* If external available, don't pause */
		return(0);
	case CS_LOGOFF:
		if(!noyes(text[LogOffQ])) {
			if(logoff_mod[0])
				exec_bin(logoff_mod,csi);
			user_event(EVENT_LOGOFF);
			menu("LOGOFF");
			SYNC;
			hangup(); }
		return(0);
	case CS_LOGOFF_FAST:
		SYNC;
		if(online==ON_REMOTE && !(mdm_misc&MDM_NODTR))
			dtr(0);
		hangup();
		return(0);
	case CS_NODELIST_ALL:
		CRLF;
		bputs(text[NodeLstHdr]);
		for(i=1;i<=sys_nodes && i<=sys_lastnode;i++) {
			getnodedat(i,&node,0);
			printnodedat(i,node); }
		return(0);
	case CS_NODELIST_USERS:
		whos_online(1);
		return(0);
	case CS_USERLIST_SUB:
		userlist(UL_SUB);
		return(0);
	case CS_USERLIST_DIR:
		userlist(UL_DIR);
		return(0);
	case CS_USERLIST_ALL:
		userlist(UL_ALL);
		return(0);
	case CS_USERLIST_LOGONS:
		sprintf(str,"%sLOGON.LST",data_dir);
		if(flength(str)<1) {
			bputs("\r\n\r\n");
			bputs(text[NoOneHasLoggedOnToday]); }
		else {
			bputs(text[CallersToday]);
			printfile(str,P_NOATCODES|P_OPENCLOSE);
			CRLF; }
		return(0);
	case CS_PAGE_SYSOP:
		ch=kbd_state(); 	 /* Check scroll lock */
		if(ch&16 || (sys_chat_ar[0] && chk_ar(sys_chat_ar,useron))
			|| useron.exempt&FLAG('C')) {
			sysop_page();
			return(0); }
		bprintf(text[SysopIsNotAvailable],sys_op);
		return(0);
	case CS_PAGE_GURU:
		csi->logic=LOGIC_FALSE;
		for(i=0;i<total_gurus;i++)
			if(!stricmp(csi->str,guru[i]->code)
				&& chk_ar(guru[i]->ar,useron))
				break;
		if(i>=total_gurus)
			return(0);
        sprintf(str,"%s%s.DAT",ctrl_dir,guru[i]->code);
		if((file=nopen(str,O_RDONLY))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
			return(0); }
		if((p=MALLOC(filelength(file)+1))==NULL) {
			close(file);
			errormsg(WHERE,ERR_ALLOC,str,filelength(file)+1);
			return(0); }
		read(file,p,filelength(file));
		p[filelength(file)]=0;
		close(file);
		localguru(p,i);
		FREE(p);
		csi->logic=LOGIC_TRUE;
		return(0);
	case CS_PRIVATE_CHAT:
		privchat();
		return(0);
	case CS_PRIVATE_MESSAGE:
		nodemsg();
		return(0);

/*******************/
/* Sysop Functions */
/*******************/
	case CS_USER_EDIT:
		useredit(csi->str[0] ? finduser(csi->str) : 0,0);
		return(0);


/******************/
/* Mail Functions */
/******************/

	case CS_MAIL_READ:	 /* Read E-mail */
		readmail(useron.number,MAIL_YOUR);
		return(0);
	case CS_MAIL_READ_SENT: 	  /* Kill/read sent mail */
		if(useron.rest&FLAG('K'))
			bputs(text[R_ReadSentMail]);
		else
			readmail(useron.number,MAIL_SENT);
		return(0);
	case CS_MAIL_READ_ALL:
		readmail(useron.number,MAIL_ALL);
		return(0);
	case CS_MAIL_SEND:		 /* Send E-mail */
		if(strchr(csi->str,'@')) {
			i=1;
			netmail(csi->str,nulstr,0); }
		else if((i=finduser(csi->str))!=0)
			email(i,nulstr,nulstr,WM_EMAIL);
		csi->logic=!i;
		return(0);
	case CS_MAIL_SEND_FEEDBACK: 	  /* Feedback */
		if((i=finduser(csi->str))!=0)
			email(i,text[ReFeedback],nulstr,WM_EMAIL);
		csi->logic=!i;
		return(0);
	case CS_MAIL_SEND_NETMAIL:
		bputs(text[EnterNetMailAddress]);
		if(getstr(str,60,K_LINE)) {
			netmail(str,nulstr,0);
			csi->logic=LOGIC_TRUE; }
		else
			csi->logic=LOGIC_FALSE;
		return(0);

	case CS_MAIL_SEND_NETFILE:
		bputs(text[EnterNetMailAddress]);
		if(getstr(str,60,K_LINE)) {
			netmail(str,nulstr,WM_FILE);
			csi->logic=LOGIC_TRUE; }
		else
			csi->logic=LOGIC_FALSE;
		return(0);

	case CS_MAIL_SEND_FILE:   /* Upload Attached File to E-mail */
		if(strchr(csi->str,'@')) {
			i=1;
			netmail(csi->str,nulstr,WM_FILE); }
		else if((i=finduser(csi->str))!=0)
			email(i,nulstr,nulstr,WM_EMAIL|WM_FILE);
		csi->logic=!i;
		return(0);
	case CS_MAIL_SEND_BULK:
		if(csi->str[0])
			p=arstr(0,csi->str);
		else
			p=nulstr;
		bulkmail(p);
		if(p && p[0])
			FREE(p);
		return(0);

    case CS_INC_MAIN_CMDS:
        main_cmds++;
        return(0);

    case CS_INC_FILE_CMDS:
        xfer_cmds++;
        return(0);

    case CS_SYSTEM_LOG:                 /* System log */
		if(!chksyspass(0))
            return(0);
        unixtodos(now,&date,&curtime);
        sprintf(str,"%sLOGS\\%2.2d%2.2d%2.2d.LOG",data_dir
            ,date.da_mon,date.da_day,TM_YEAR(date.da_year-1900));
        printfile(str,0);
        return(0);
    case CS_SYSTEM_YLOG:                /* Yesterday's log */
		if(!chksyspass(0))
            return(0);
        now-=(ulong)60L*24L*60L;
        unixtodos(now,&date,&curtime);
        sprintf(str,"%sLOGS\\%2.2d%2.2d%2.2d.LOG",data_dir
            ,date.da_mon,date.da_day,TM_YEAR(date.da_year-1900));
        printfile(str,0);
        return(0);
    case CS_SYSTEM_STATS:               /* System Statistics */
        bputs(text[SystemStatsHdr]);
        getstats(0,&stats);
        bprintf(text[StatsTotalLogons],ultoac(stats.logons,tmp));
        bprintf(text[StatsLogonsToday],ultoac(stats.ltoday,tmp));
        bprintf(text[StatsTotalTime],ultoac(stats.timeon,tmp));
        bprintf(text[StatsTimeToday],ultoac(stats.ttoday,tmp));
        bprintf(text[StatsUploadsToday],ultoac(stats.ulb,tmp)
            ,stats.uls);
        bprintf(text[StatsDownloadsToday],ultoac(stats.dlb,tmp)
            ,stats.dls);
        bprintf(text[StatsPostsToday],ultoac(stats.ptoday,tmp));
        bprintf(text[StatsEmailsToday],ultoac(stats.etoday,tmp));
        bprintf(text[StatsFeedbacksToday],ultoac(stats.ftoday,tmp));
        return(0);
    case CS_NODE_STATS:              /* Node Statistics */
        i=atoi(csi->str);
        if(i>sys_nodes) {
            bputs(text[InvalidNode]);
            return(0); }
        if(!i) i=node_num;
        bprintf(text[NodeStatsHdr],i);
        getstats(i,&stats);
        bprintf(text[StatsTotalLogons],ultoac(stats.logons,tmp));
        bprintf(text[StatsLogonsToday],ultoac(stats.ltoday,tmp));
        bprintf(text[StatsTotalTime],ultoac(stats.timeon,tmp));
        bprintf(text[StatsTimeToday],ultoac(stats.ttoday,tmp));
        bprintf(text[StatsUploadsToday],ultoac(stats.ulb,tmp)
            ,stats.uls);
        bprintf(text[StatsDownloadsToday],ultoac(stats.dlb,tmp)
            ,stats.dls);
        bprintf(text[StatsPostsToday],ultoac(stats.ptoday,tmp));
        bprintf(text[StatsEmailsToday],ultoac(stats.etoday,tmp));
        bprintf(text[StatsFeedbacksToday],ultoac(stats.ftoday,tmp));
        return(0);
    case CS_CHANGE_USER:                 /* Change to another user */
		if(!chksyspass(0))
            return(0);
        bputs(text[ChUserPrompt]);
        if(!getstr(str,LEN_ALIAS,K_UPPER))
            return(0);
        if((i=finduser(str))==0)
            return(0);
        if(online==ON_REMOTE) {
            getuserrec(i,U_LEVEL,2,str);
            if(atoi(str)>logon_ml) {
                getuserrec(i,U_PASS,8,tmp);
                bputs(text[ChUserPwPrompt]);
                console|=CON_R_ECHOX;
                if(!(sys_misc&SM_ECHO_PW))
                    console|=CON_L_ECHOX;
                getstr(str,8,K_UPPER);
                console&=~(CON_R_ECHOX|CON_L_ECHOX);
                if(strcmp(str,tmp))
                    return(0); } }
        putmsgptrs();
		putuserrec(useron.number,U_CURSUB,8
			,sub[usrsub[curgrp][cursub[curgrp]]]->code);
		putuserrec(useron.number,U_CURDIR,8
			,dir[usrdir[curlib][curdir[curlib]]]->code);
        useron.number=i;
        getuserdat(&useron);
		getnodedat(node_num,&thisnode,1);
		thisnode.useron=useron.number;
		putnodedat(node_num,thisnode);
        getmsgptrs();
        if(REALSYSOP) sys_status&=~SS_TMPSYSOP;
        else sys_status|=SS_TMPSYSOP;
        statline=sys_def_stat;
        statusline();
        sprintf(str,"Changed into %s #%u",useron.alias,useron.number);
        logline("S+",str);
        return(0);
    case CS_SHOW_MEM:
#ifdef __MSDOS__
         bprintf(text[NBytesFreeMemory],farcoreleft());
#endif
        return(0);
    case CS_ERROR_LOG:
        sprintf(str,"%sERROR.LOG",data_dir);
        if(fexist(str)) {
            bputs(text[ErrorLogHdr]);
            printfile(str,0);
            if(!noyes(text[DeleteErrorLogQ]))
                remove(str); }
        else
            bputs(text[NoErrorLogExists]);
        for(i=1;i<=sys_nodes;i++) {
            getnodedat(i,&node,0);
            if(node.errors)
                break; }
        if(i<=sys_nodes || criterrs) {
            if(!noyes(text[ClearErrCounter])) {
                for(i=1;i<=sys_nodes;i++) {
                    getnodedat(i,&node,1);
                    node.errors=0;
                    putnodedat(i,node); }
                criterrs=0; } }
        return(0);
    case CS_ANSI_CAPTURE:           /* Capture ANSI codes */
        sys_status^=SS_ANSCAP;
        bprintf(text[ANSICaptureIsNow]
            ,sys_status&SS_ANSCAP ? text[ON] : text[OFF]);
        return(0);
    case CS_LIST_TEXT_FILE:              /* View ASCII/ANSI/Ctrl-A file */
		if(!chksyspass(0))
            return(0);
        bputs(text[Filename]);
        if(getstr(str,60,K_UPPER))
            printfile(str,0);
        return(0);
    case CS_EDIT_TEXT_FILE:              /* Edit ASCII/Ctrl-A file */
		if(!chksyspass(0))
            return(0);
        bputs(text[Filename]);
        if(getstr(str,60,K_UPPER))
            editfile(str);
        return(0);
    case CS_GURU_LOG:
        sprintf(str,"%sGURU.LOG",data_dir);
        if(fexist(str)) {
            printfile(str,0);
            CRLF;
            if(!noyes(text[DeleteGuruLogQ]))
                remove(str); }
        return(0);
    case CS_FILE_SET_ALT_PATH:
        altul=atoi(csi->str);
        if(altul>altpaths)
            altul=0;
        bprintf(text[AltULPathIsNow],altul ? altpath[altul-1] : text[OFF]);
        return(0);
    case CS_FILE_RESORT_DIRECTORY:
        for(i=1;i<=sys_nodes;i++)
            if(i!=node_num) {
                getnodedat(i,&node,0);
                if(node.status==NODE_INUSE
                    || node.status==NODE_QUIET)
                    break; }

        if(i<=sys_nodes) {
            bputs(text[ResortWarning]);
            return(0); }

        if(!stricmp(csi->str,"ALL")) {     /* all libraries */
            for(i=0;i<usrlibs;i++)
                for(j=0;j<usrdirs[i];j++)
                    resort(usrdir[i][j]);
            return(0); }
        if(!stricmp(csi->str,"LIB")) {     /* current library */
            for(i=0;i<usrdirs[curlib];i++)
                resort(usrdir[curlib][i]);
            return(0); }
        resort(usrdir[curlib][curdir[curlib]]);
        return(0);

    case CS_FILE_GET:

        if(!fexist(csi->str)) {
            bputs(text[FileNotFound]);
            return(0); }
		if(!chksyspass(0))
            return(0);

	case CS_FILE_SEND:

        menu("DLPROT");
        mnemonics(text[ProtocolOrQuit]);
        strcpy(str,"Q");
        for(i=0;i<total_prots;i++)
            if(prot[i]->dlcmd[0] && chk_ar(prot[i]->ar,useron)) {
                sprintf(tmp,"%c",prot[i]->mnemonic);
                strcat(str,tmp); }
        ch=getkeys(str,0);
        if(ch=='Q' || sys_status&SS_ABORT) {
            return(0); }
        for(i=0;i<total_prots;i++)
            if(prot[i]->mnemonic==ch && chk_ar(prot[i]->ar,useron))
                break;
        if(i<total_prots) {
			protocol(cmdstr(prot[i]->dlcmd,csi->str,csi->str,str),0);
            autohangup(); }
        return(0);

    case CS_FILE_PUT:
		if(!chksyspass(0))
            return(0);
        menu("ULPROT");
        mnemonics(text[ProtocolOrQuit]);
        strcpy(str,"Q");
        for(i=0;i<total_prots;i++)
            if(prot[i]->ulcmd[0] && chk_ar(prot[i]->ar,useron)) {
                sprintf(tmp,"%c",prot[i]->mnemonic);
                strcat(str,tmp); }
        ch=getkeys(str,0);
        if(ch=='Q' || sys_status&SS_ABORT) {
            lncntr=0;
            return(0); }
        for(i=0;i<total_prots;i++)
            if(prot[i]->mnemonic==ch && chk_ar(prot[i]->ar,useron))
                break;
        if(i<total_prots) {
			protocol(cmdstr(prot[i]->ulcmd,csi->str,csi->str,str),0);
            autohangup(); }
        return(0);

    case CS_FILE_UPLOAD_BULK:

        if(!usrlibs) return(0);

        if(!stricmp(csi->str,"ALL")) {     /* all libraries */
            for(i=0;i<usrlibs;i++)
                for(j=0;j<usrdirs[i];j++) {
                    if(lib[i]->offline_dir==usrdir[i][j])
                        continue;
                    if(bulkupload(usrdir[i][j])) return(0); }
            return(0); }
        if(!stricmp(csi->str,"LIB")) {     /* current library */
            for(i=0;i<usrdirs[curlib];i++) {
                if(lib[usrlib[curlib]]->offline_dir
                    ==usrdir[curlib][i])
                    continue;
                if(bulkupload(usrdir[curlib][i])) return(0); }
            return(0); }
        bulkupload(usrdir[curlib][curdir[curlib]]); /* current dir */
        return(0);

    case CS_FILE_FIND_OLD:
    case CS_FILE_FIND_OPEN:
    case CS_FILE_FIND_OFFLINE:
    case CS_FILE_FIND_OLD_UPLOADS:
        if(!usrlibs) return(0);
        if(!getfilespec(tmp))
            return(0);
        padfname(tmp,str);
        k=0;
        bputs("\r\nSearching ");
        if(!stricmp(csi->str,"ALL"))
            bputs("all libraries");
        else if(!stricmp(csi->str,"LIB"))
            bputs("library");
        else
            bputs("directory");
        bputs(" for files ");
        if(*(csi->ip-1)==CS_FILE_FIND_OLD_UPLOADS) {
            l=FI_OLDUL;
            bprintf("uploaded before %s\r\n",timestr(&ns_time)); }
        else if(*(csi->ip-1)==CS_FILE_FIND_OLD) { /* go by download date */
            l=FI_OLD;
            bprintf("not downloaded since %s\r\n",timestr(&ns_time)); }
        else if(*(csi->ip-1)==CS_FILE_FIND_OFFLINE) {
            l=FI_OFFLINE;
            bputs("not online...\r\n"); }
        else {
            l=FI_CLOSE;
            bputs("currently open...\r\n"); }
        if(!stricmp(csi->str,"ALL")) {
            for(i=0;i<usrlibs;i++)
                for(j=0;j<usrdirs[i];j++) {
                    if(lib[i]->offline_dir==usrdir[i][j])
                        continue;
                    if((s=listfileinfo(usrdir[i][j],str,l))==-1)
                        return(0);
                    else k+=s; } }
        else if(!stricmp(csi->str,"LIB")) {
            for(i=0;i<usrdirs[curlib];i++) {
                if(lib[usrlib[curlib]]->offline_dir==usrdir[curlib][i])
                        continue;
                if((s=listfileinfo(usrdir[curlib][i],str,l))==-1)
                    return(0);
                else k+=s; } }
        else {
            s=listfileinfo(usrdir[curlib][curdir[curlib]],str,l);
            if(s==-1)
                return(0);
            k=s; }
        if(k>1) {
            bprintf(text[NFilesListed],k); }
		return(0); }

if(*(csi->ip-1)>=CS_MSG_SET_AREA && *(csi->ip-1)<=CS_MSG_UNUSED1) {
	csi->ip--;
	return(execmsg(csi)); }

if(*(csi->ip-1)>=CS_FILE_SET_AREA && *(csi->ip-1)<=CS_FILE_UNUSED1) {
	csi->ip--;
	return(execfile(csi)); }

errormsg(WHERE,ERR_CHK,"shell function",*(csi->ip-1));
return(0);
}
