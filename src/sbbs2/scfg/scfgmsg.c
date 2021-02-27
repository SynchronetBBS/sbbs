#line 2 "SCFGMSG.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "scfg.h"

/****************************************************************************/
/* Converts when_t.zone into ASCII format									*/
/****************************************************************************/
char *zonestr(short zone)
{
	static char str[32];

switch((ushort)zone) {
	case 0: 	return("UT");
	case AST:	return("AST");
	case EST:	return("EST");
	case CST:	return("CST");
	case MST:	return("MST");
	case PST:	return("PST");
	case YST:	return("YST");
	case HST:	return("HST");
	case BST:	return("BST");
	case ADT:	return("ADT");
	case EDT:	return("EDT");
	case CDT:	return("CDT");
	case MDT:	return("MDT");
	case PDT:	return("PDT");
	case YDT:	return("YDT");
	case HDT:	return("HDT");
	case BDT:	return("BDT");
	case MID:	return("MID");
	case VAN:	return("VAN");
	case EDM:	return("EDM");
	case WIN:	return("WIN");
	case BOG:	return("BOG");
	case CAR:	return("CAR");
	case RIO:	return("RIO");
	case FER:	return("FER");
	case AZO:	return("AZO");
	case LON:	return("LON");
	case BER:	return("BER");
	case ATH:	return("ATH");
	case MOS:	return("MOS");
	case DUB:	return("DUB");
	case KAB:	return("KAB");
	case KAR:	return("KAR");
	case BOM:	return("BOM");
	case KAT:	return("KAT");
	case DHA:	return("DHA");
	case BAN:	return("BAN");
	case HON:	return("HON");
	case TOK:	return("TOK");
	case SYD:	return("SYD");
	case NOU:	return("NOU");
	case WEL:	return("WEL");
	}

sprintf(str,"%02hd:%02hu",zone/60,zone<0 ? (-zone)%60 : zone%60);
return(str);
}

char *utos(char *str)
{
	static char out[128];
	int i;

for(i=0;str[i];i++)
	if(str[i]=='_')
		out[i]=SP;
	else
		out[i]=str[i];
out[i]=0;
return(out);
}

char *stou(char *str)
{
	static char out[128];
	int i;

for(i=0;str[i];i++)
	if(str[i]==SP)
		out[i]='_';
	else
		out[i]=str[i];
out[i]=0;
return(out);
}



void clearptrs(int subnum)
{
	char str[256];
	ushort idx,ch;
	int last,file,i;
	long l=0L;
	struct ffblk ff;

upop("Clearing Pointers...");
sprintf(str,"%sUSER\\PTRS\\*.IXB",data_dir);
last=findfirst(str,&ff,0);
while(!last) {
	if(ff.ff_fsize>=((long)sub[subnum]->ptridx+1L)*10L) {
		sprintf(str,"%sUSER\\PTRS\\%s",data_dir,ff.ff_name);
		if((file=nopen(str,O_WRONLY))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
			bail(1); }
		while(filelength(file)<(long)(sub[subnum]->ptridx)*10) {
			lseek(file,0L,SEEK_END);
			idx=tell(file)/10;
			for(i=0;i<total_subs;i++)
				if(sub[i]->ptridx==idx)
					break;
			write(file,&l,4);
			write(file,&l,4);
			ch=0xff;			/* default to scan ON for unknown sub */
			if(i<total_subs) {
				if(!(sub[i]->misc&SUB_NSDEF))
					ch&=~5;
				if(!(sub[i]->misc&SUB_SSDEF))
					ch&=~2; }
			write(file,&ch,2); }
		lseek(file,((long)sub[subnum]->ptridx)*10L,SEEK_SET);
		write(file,&l,4);	/* date set to null */
		write(file,&l,4);	/* date set to null */
		ch=0xff;
		if(!(sub[subnum]->misc&SUB_NSDEF))
			ch&=~5;
		if(!(sub[subnum]->misc&SUB_SSDEF))
			ch&=~2;
		write(file,&ch,2);
		close(file); }
	last=findnext(&ff); }
upop(0);
}

void msgs_cfg()
{
	static int dflt,msgs_dflt,bar;
	char str[256],str2[256],done=0,*p;
	int j,k,q,s;
	int i,file,ptridx,n;
	long ported;
	sub_t tmpsub;
	static grp_t savgrp;
	FILE *stream;

while(1) {
	for(i=0;i<total_grps && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-25s",grp[i]->lname);
	opt[i][0]=0;
	j=WIN_ORG|WIN_ACT|WIN_CHE;
	if(total_grps)
		j|=WIN_DEL|WIN_DELACT|WIN_GET;
	if(total_grps<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savgrp.sname[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
Message Groups:

This is a listing of message groups for your BBS. Message groups are
used to logically separate your message sub-boards into groups. Every
sub-board belongs to a message group. You must have at least one message
group and one sub-board configured.

One popular use for message groups is to separate local sub-boards and
networked sub-boards. One might have a Local message group that contains
non-networked sub-boards of various topics and also have a FidoNet
message group that contains sub-boards that are echoed across FidoNet.
Some sysops separate sub-boards into more specific areas such as Main,
Technical, or Adult. If you have many sub-boards that have a common
subject denominator, you may want to have a separate message group for
those sub-boards for a more organized message structure.
*/
	i=ulist(j,0,0,45,&msgs_dflt,&bar,"Message Groups",opt);
	savnum=0;
	if(i==-1) {
		j=save_changes(WIN_MID);
		if(j==-1)
		   continue;
		if(!j)
			write_msgs_cfg();
		return; }
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Group Long Name:

This is a description of the message group which is displayed when a
user of the system uses the /* command from the main menu.
*/*/
		strcpy(str,"Main");
		if(uinput(WIN_MID|WIN_SAV,0,0,"Group Long Name",str,LEN_GLNAME
			,K_EDIT)<1)
			continue;
		SETHELP(WHERE);
/*
Group Short Name:

This is a short description of the message group which is used for the
main menu and reading message prompts.
*/
		sprintf(str2,"%.*s",LEN_GSNAME,str);
		if(uinput(WIN_MID,0,0,"Group Short Name",str2,LEN_GSNAME,K_EDIT)<1)
			continue;
		if((grp=(grp_t **)REALLOC(grp,sizeof(grp_t *)*(total_grps+1)))==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,total_grps+1);
			total_grps=0;
			bail(1);
            continue; }

		if(total_grps) {	/* was total_subs (?) */
            for(j=total_grps;j>i;j--)   /* insert above */
                grp[j]=grp[j-1];
            for(j=0;j<total_subs;j++)   /* move sub group numbers */
                if(sub[j]->grp>=i)
                    sub[j]->grp++; }

		if((grp[i]=(grp_t *)MALLOC(sizeof(grp_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(grp_t));
			continue; }
		memset((grp_t *)grp[i],0,sizeof(grp_t));
		strcpy(grp[i]->lname,str);
		strcpy(grp[i]->sname,str2);
		total_grps++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Delete All Data in Group:

If you wish to delete the messages in all the sub-boards in this group,
select Yes.
*/
		j=1;
		strcpy(opt[0],"Yes");
		strcpy(opt[1],"No");
		opt[2][0]=0;
		j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0,"Delete All Data in Group",opt);
		if(j==-1)
			continue;
		if(j==0)
			for(j=0;j<total_subs;j++)
				if(sub[j]->grp==i) {
					sprintf(str,"%s.s*",sub[j]->code);
					if(!sub[j]->data_dir[0])
						sprintf(tmp,"%sSUBS\\",data_dir);
					else
						strcpy(tmp,sub[j]->data_dir);
					delfiles(tmp,str);
					clearptrs(j); }
		FREE(grp[i]);
		for(j=0;j<total_subs;) {
			if(sub[j]->grp==i) {	/* delete subs of this group */
				FREE(sub[j]);
				total_subs--;
				k=j;
				while(k<total_subs) {	/* move all subs down */
					sub[k]=sub[k+1];
					for(q=0;q<total_qhubs;q++)
						for(s=0;s<qhub[q]->subs;s++)
							if(qhub[q]->sub[s]==k)
								qhub[q]->sub[s]--;
					k++; } }
			else j++; }
		for(j=0;j<total_subs;j++)	/* move sub group numbers down */
			if(sub[j]->grp>i)
				sub[j]->grp--;
		total_grps--;
		while(i<total_grps) {
			grp[i]=grp[i+1];
			i++; }
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savgrp=*grp[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*grp[i]=savgrp;
		changes=1;
		continue; }
	done=0;
	while(!done) {
		j=0;
		sprintf(opt[j++],"%-27.27s%s","Long Name",grp[i]->lname);
		sprintf(opt[j++],"%-27.27s%s","Short Name",grp[i]->sname);
		sprintf(opt[j++],"%-27.27s%.40s","Access Requirements"
			,grp[i]->ar);
		strcpy(opt[j++],"Clone Options");
		strcpy(opt[j++],"Export Areas...");
		strcpy(opt[j++],"Import Areas...");
		strcpy(opt[j++],"Message Sub-boards...");
		opt[j][0]=0;
		sprintf(str,"%s Group",grp[i]->sname);
		savnum=0;
		SETHELP(WHERE);
/*
Message Group Configuration:

This menu allows you to configure the security requirements for access
to this message group. You can also add, delete, and configure the
sub-boards of this group by selecting the Messages Sub-boards... option.
*/
		switch(ulist(WIN_ACT,6,4,60,&dflt,0,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Group Long Name:

This is a description of the message group which is displayed when a
user of the system uses the /* command from the main menu.
*/*/
				strcpy(str,grp[i]->lname);	/* save incase setting to null */
				if(!uinput(WIN_MID|WIN_SAV,0,17,"Name to use for Listings"
					,grp[i]->lname,LEN_GLNAME,K_EDIT))
					strcpy(grp[i]->lname,str);
				break;
			case 1:
				SETHELP(WHERE);
/*
Group Short Name:

This is a short description of the message group which is used for
main menu and reading messages prompts.
*/
				uinput(WIN_MID|WIN_SAV,0,17,"Name to use for Prompts"
					,grp[i]->sname,LEN_GSNAME,K_EDIT);
				break;
			case 2:
				sprintf(str,"%s Group",grp[i]->sname);
				getar(str,grp[i]->ar);
				break;
			case 3: 	/* Clone Options */
				j=0;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Clone Sub-board Options:

If you want to clone the options of the first sub-board of this group
into all sub-boards of this group, select Yes.

The options cloned are posting requirements, reading requirements,
operator requirments, moderated user requirments, toggle options,
network options (including EchoMail origin line, EchoMail address,
and QWK Network tagline), maximum number of messages, maximum number
of CRCs, maximum age of messages, storage method, and data directory.
*/
				j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
					,"Clone Options of First Sub-board into All of Group",opt);
				if(j==0) {
					k=-1;
					for(j=0;j<total_subs;j++)
						if(sub[j]->grp==i) {
							if(k==-1)
								k=j;
							else {
								changes=1;
								sub[j]->misc=(sub[k]->misc|SUB_HDRMOD);
								strcpy(sub[j]->post_ar,sub[k]->post_ar);
								strcpy(sub[j]->read_ar,sub[k]->read_ar);
								strcpy(sub[j]->op_ar,sub[k]->op_ar);
								strcpy(sub[j]->mod_ar,sub[k]->mod_ar);
								strcpy(sub[j]->origline,sub[k]->origline);
								strcpy(sub[j]->tagline,sub[k]->tagline);
								strcpy(sub[j]->data_dir,sub[k]->data_dir);
								strcpy(sub[j]->echomail_sem
									,sub[k]->echomail_sem);
								sub[j]->maxmsgs=sub[k]->maxmsgs;
								sub[j]->maxcrcs=sub[k]->maxcrcs;
								sub[j]->maxage=sub[k]->maxage;

								sub[j]->faddr=sub[k]->faddr; } } }
				break;
			case 4:
				k=0;
				ported=0;
				q=changes;
				strcpy(opt[k++],"SUBS.TXT    (Synchronet)");
				strcpy(opt[k++],"AREAS.BBS   (MSG)");
				strcpy(opt[k++],"AREAS.BBS   (SMB)");
				strcpy(opt[k++],"AREAS.BBS   (SBBSECHO)");
				strcpy(opt[k++],"FIDONET.NA  (Fido)");
				opt[k][0]=0;
				SETHELP(WHERE);
/*
Export Area File Format:

This menu allows you to choose the format of the area file you wish to
export the current message group into.
*/
				k=0;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Export Area File Format",opt);
				if(k==-1)
					break;
				if(k==0)
					sprintf(str,"%sSUBS.TXT",ctrl_dir);
				else if(k==1 || k==2)
					sprintf(str,"AREAS.BBS");
				else if(k==3)
					sprintf(str,"%sAREAS.BBS",data_dir);
				else if(k==4)
					sprintf(str,"FIDONET.NA");
				strupr(str);
				if(k && k<4)
					if(uinput(WIN_MID|WIN_SAV,0,0,"Uplinks"
						,str2,40,K_UPPER)<=0) {
						changes=q;
						break; }
				if(uinput(WIN_MID|WIN_SAV,0,0,"Filename"
					,str,40,K_UPPER|K_EDIT)<=0) {
					changes=q;
					break; }
				if(fexist(str)) {
					strcpy(opt[0],"Overwrite");
					strcpy(opt[1],"Append");
					opt[2][0]=0;
					j=0;
					j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"File Exists",opt);
					if(j==-1)
						break;
					if(j==0) j=O_WRONLY|O_TRUNC;
					else	 j=O_WRONLY|O_APPEND; }
				else
					j=O_WRONLY|O_CREAT;
				if((stream=fnopen(&file,str,j))==NULL) {
					umsg("Open Failure");
					break; }
				upop("Exporting Areas...");
				for(j=0;j<total_subs;j++) {
					if(sub[j]->grp!=i)
						continue;
					ported++;
					if(k==1) {		/* AREAS.BBS *.MSG */
						if(!sub[j]->echopath[0])
							sprintf(str,"%s%s\\",echomail_dir,sub[j]->code);
						else
							strcpy(str,sub[j]->echopath);
						fprintf(stream,"%-30s %-20s %s\r\n"
							,str,stou(sub[j]->sname),str2);
						continue; }
					if(k==2) {		/* AREAS.BBS SMB */
						if(!sub[j]->data_dir[0])
							sprintf(str,"%sSUBS\\%s",data_dir,sub[j]->code);
						else
							sprintf(str,"%s%s",sub[j]->data_dir,sub[j]->code);
						fprintf(stream,"%-30s %-20s %s\r\n"
							,str,stou(sub[j]->sname),str2);
                        continue; }
					if(k==3) {		/* AREAS.BBS SBBSECHO */
						fprintf(stream,"%-30s %-20s %s\r\n"
							,sub[j]->code,stou(sub[j]->sname),str2);
						continue; }
					if(k==4) {		/* FIDONET.NA */
						fprintf(stream,"%-20s %s\r\n"
							,stou(sub[j]->sname),sub[j]->lname);
						continue; }
					fprintf(stream,"%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n"
							"%s\r\n%s\r\n%s\r\n"
						,sub[j]->lname
						,sub[j]->sname
						,sub[j]->qwkname
						,sub[j]->code
						,sub[j]->data_dir
						,sub[j]->ar
						,sub[j]->read_ar
						,sub[j]->post_ar
						,sub[j]->op_ar
						);
					fprintf(stream,"%lX\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n"
						,sub[j]->misc
						,sub[j]->tagline
						,sub[j]->origline
						,sub[j]->echomail_sem
						,sub[j]->echopath
						,faddrtoa(sub[j]->faddr)
						);
					fprintf(stream,"%lu\r\n%lu\r\n%u\r\n%u\r\n%s\r\n"
						,sub[j]->maxmsgs
						,sub[j]->maxcrcs
						,sub[j]->maxage
						,sub[j]->ptridx
						,sub[j]->mod_ar
						);
					fprintf(stream,"***END-OF-SUB***\r\n\r\n"); }
				fclose(stream);
				upop(0);
				sprintf(str,"%lu Message Areas Exported Successfully",ported);
				umsg(str);
				changes=q;
				break;
			case 5:
				ported=0;
				k=0;
				strcpy(opt[k++],"SUBS.TXT    (Synchronet)");
				strcpy(opt[k++],"AREAS.BBS   (MSG)");
				strcpy(opt[k++],"AREAS.BBS   (SMB)");
				strcpy(opt[k++],"AREAS.BBS   (SBBSECHO)");
				strcpy(opt[k++],"FIDONET.NA  (Fido)");
				opt[k][0]=0;
				SETHELP(WHERE);
/*
Import Area File Format:

This menu allows you to choose the format of the area file you wish to
import into the current message group.
*/
				k=0;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Import Area File Format",opt);
				if(k==-1)
					break;
				if(k==0)
					sprintf(str,"%sSUBS.TXT",ctrl_dir);
				else if(k==1 || k==2)
					sprintf(str,"AREAS.BBS");
				else if(k==3)
					sprintf(str,"%sAREAS.BBS",data_dir);
				else if(k==4)
					sprintf(str,"FIDONET.NA");
				strupr(str);
				if(uinput(WIN_MID|WIN_SAV,0,0,"Filename"
					,str,40,K_UPPER|K_EDIT)<=0)
                    break;
				if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
					umsg("Open Failure");
                    break; }
				upop("Importing Areas...");
				while(!feof(stream)) {
					if(!fgets(str,128,stream)) break;
					truncsp(str);
					if(!str[0])
						continue;
					if(k) {
						p=str;
						while(*p && *p<=SP) p++;
						if(!*p || *p==';')
							continue;
						memset(&tmpsub,0,sizeof(sub_t));
						tmpsub.misc|=
							(SUB_FIDO|SUB_NAME|SUB_TOUSER|SUB_QUOTE|SUB_HYPER);
						if(k==1) {		/* AREAS.BBS *.MSG */
							p=strrchr(str,'\\');
                            if(p) *p=0;
                            else p=str;
							sprintf(tmpsub.echopath,"%.*s",LEN_DIR,str);
							p++;
							sprintf(tmpsub.code,"%.8s",p);
							while(*p && *p<=SP) p++;
							sprintf(tmpsub.sname,"%.*s",LEN_SSNAME,p);
							p=strchr(tmpsub.sname,SP);
							if(p) *p=0;
							strcpy(tmpsub.sname,utos(tmpsub.sname));
							sprintf(tmpsub.lname,"%.*s",LEN_SLNAME
								,tmpsub.sname);
							sprintf(tmpsub.qwkname,"%.*s",10
                                ,tmpsub.sname);
							}
						if(k==2) {		/* AREAS.BBS SMB */
							p=strrchr(str,'\\');
                            if(p) *p=0;
                            else p=str;
							sprintf(tmpsub.data_dir,"%.*s",LEN_DIR,str);
							p++;
							sprintf(tmpsub.code,"%.8s",p);
							while(*p && *p<=SP) p++;
							sprintf(tmpsub.sname,"%.*s",LEN_SSNAME,p);
							p=strchr(tmpsub.sname,SP);
							if(p) *p=0;
							strcpy(tmpsub.sname,utos(tmpsub.sname));
							sprintf(tmpsub.lname,"%.*s",LEN_SLNAME
								,tmpsub.sname);
							sprintf(tmpsub.qwkname,"%.*s",10
                                ,tmpsub.sname);
                            }
						else if(k==3) { /* AREAS.BBS SBBSECHO */
							p=str;
							while(*p && *p>SP) p++;
							*p=0;
							sprintf(tmpsub.code,"%.8s",str);
							p++;
							while(*p && *p<=SP) p++;
							sprintf(tmpsub.sname,"%.*s",LEN_SSNAME,p);
							p=strchr(tmpsub.sname,SP);
							if(p) *p=0;
							strcpy(tmpsub.sname,utos(tmpsub.sname));
							sprintf(tmpsub.lname,"%.*s",LEN_SLNAME
								,tmpsub.sname);
							sprintf(tmpsub.qwkname,"%.*s",10
                                ,tmpsub.sname);
							}
						else if(k==4) { /* FIDONET.NA */
                            p=str;
							while(*p && *p>SP) p++;
							*p=0;
							sprintf(tmpsub.code,"%.8s",str);
							sprintf(tmpsub.sname,"%.*s",LEN_SSNAME,utos(str));
							sprintf(tmpsub.qwkname,"%.10s",tmpsub.sname);
							p++;
							while(*p && *p<=SP) p++;
							sprintf(tmpsub.lname,"%.*s",LEN_SLNAME,p);
							}
						ported++; }
					else {
						memset(&tmpsub,0,sizeof(sub_t));
						tmpsub.grp=i;
						sprintf(tmpsub.lname,"%.*s",LEN_SLNAME,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.sname,"%.*s",LEN_SSNAME,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.qwkname,"%.*s",10,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.code,"%.*s",8,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.data_dir,"%.*s",LEN_DIR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.ar,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.read_ar,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.post_ar,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.op_ar,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpsub.misc=ahtoul(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.tagline,"%.*s",80,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.origline,"%.*s",50,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.echomail_sem,"%.*s",LEN_DIR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.echopath,"%.*s",LEN_DIR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpsub.faddr=atofaddr(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpsub.maxmsgs=atol(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpsub.maxcrcs=atol(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpsub.maxage=atoi(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpsub.ptridx=atoi(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.mod_ar,"%.*s",LEN_ARSTR,str);

						ported++;
						while(!feof(stream)
							&& strcmp(str,"***END-OF-SUB***")) {
							if(!fgets(str,128,stream)) break;
							truncsp(str); } }

					truncsp(tmpsub.code);
					truncsp(tmpsub.sname);
					truncsp(tmpsub.lname);
					truncsp(tmpsub.qwkname);
					for(j=0;j<total_subs;j++) {
						if(sub[j]->grp!=i)
							continue;
						if(!stricmp(sub[j]->code,tmpsub.code))
							break; }
					if(j==total_subs) {

						if((sub=(sub_t **)REALLOC(sub
							,sizeof(sub_t *)*(total_subs+1)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,total_subs+1);
							total_subs=0;
							bail(1);
							break; }

						for(ptridx=0;ptridx>-1;ptridx++) {
							for(n=0;n<total_subs;n++)
								if(sub[n]->ptridx==ptridx)
									break;
							if(n==total_subs)
								break; }

						if((sub[j]=(sub_t *)MALLOC(sizeof(sub_t)))
							==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(sub_t));
							break; }
						memset(sub[j],0,sizeof(sub_t)); }
					if(!k)
						memcpy(sub[j],&tmpsub,sizeof(sub_t));
					else {
                        sub[j]->grp=i;
						if(total_faddrs)
							sub[j]->faddr=faddr[0];
						strcpy(sub[j]->code,tmpsub.code);
						strcpy(sub[j]->sname,tmpsub.sname);
						strcpy(sub[j]->lname,tmpsub.lname);
						strcpy(sub[j]->qwkname,tmpsub.qwkname);
						strcpy(sub[j]->echopath,tmpsub.echopath);
						strcpy(sub[j]->data_dir,tmpsub.data_dir);
						if(j==total_subs)
							sub[j]->maxmsgs=1000;
						}
					if(j==total_subs) {
						sub[j]->ptridx=ptridx;
						sub[j]->misc=tmpsub.misc;
						total_subs++; }
					changes=1; }
				fclose(stream);
				upop(0);
				sprintf(str,"%lu Message Areas Imported Successfully",ported);
                umsg(str);
				break;

			case 6:
                sub_cfg(i);
				break; } } }

}

void msg_opts()
{
	char str[128],*p;
	static int msg_dflt;
	int i,j;

	while(1) {
		i=0;
		sprintf(opt[i++],"%-33.33s%s"
			,"BBS ID for QWK Packets",sys_id);
		sprintf(opt[i++],"%-33.33s%s"
			,"Local Time Zone",zonestr(sys_timezone));
		sprintf(opt[i++],"%-33.33s%u seconds"
			,"Maximum Retry Time",smb_retry_time);
		if(max_qwkmsgs)
			sprintf(str,"%lu",max_qwkmsgs);
		else
			sprintf(str,"Unlimited");
		sprintf(opt[i++],"%-33.33s%s"
			,"Maximum QWK Messages",str);
		sprintf(opt[i++],"%-33.33s%s","Pre-pack QWK Requirements",preqwk_ar);
		if(mail_maxage)
			sprintf(str,"Enabled (%u days old)",mail_maxage);
        else
            strcpy(str,"Disabled");
		sprintf(opt[i++],"%-33.33s%s","Purge E-mail by Age",str);
		if(sys_misc&SM_DELEMAIL)
			strcpy(str,"Immediately");
		else
			strcpy(str,"Daily");
		sprintf(opt[i++],"%-33.33s%s","Purge Deleted E-mail",str);
		if(mail_maxcrcs)
			sprintf(str,"Enabled (%lu mail CRCs)",mail_maxcrcs);
		else
			strcpy(str,"Disabled");
		sprintf(opt[i++],"%-33.33s%s","Duplicate E-mail Checking",str);
		sprintf(opt[i++],"%-33.33s%s","Allow Anonymous E-mail"
			,sys_misc&SM_ANON_EM ? "Yes" : "No");
		sprintf(opt[i++],"%-33.33s%s","Allow Quoting in E-mail"
			,sys_misc&SM_QUOTE_EM ? "Yes" : "No");
		sprintf(opt[i++],"%-33.33s%s","Allow Uploads in E-mail"
			,sys_misc&SM_FILE_EM ? "Yes" : "No");
		sprintf(opt[i++],"%-33.33s%s","Allow Forwarding to NetMail"
			,sys_misc&SM_FWDTONET ? "Yes" : "No");
		sprintf(opt[i++],"%-33.33s%s","Kill Read E-mail"
			,sys_misc&SM_DELREADM ? "Yes" : "No");
		sprintf(opt[i++],"%-33.33s%s","Users Can View Deleted Messages"
			,sys_misc&SM_USRVDELM ? "Yes" : sys_misc&SM_SYSVDELM
				? "Sysops Only":"No");
		strcpy(opt[i++],"Extra Attribute Codes...");
		opt[i][0]=0;
		savnum=0;
		SETHELP(WHERE);
/*
Message Options:

This is a menu of system-wide message related options. Messages include
E-mail and public posts (on sub-boards).
*/

		switch(ulist(WIN_ORG|WIN_ACT|WIN_MID|WIN_CHE,0,0,72,&msg_dflt,0
			,"Message Options",opt)) {
			case -1:
				i=save_changes(WIN_MID);
				if(i==-1)
				   continue;
				if(!i) {
					write_msgs_cfg();
					write_main_cfg(); }
				return;
			case 0:
				strcpy(str,sys_id);
				SETHELP(WHERE);
/*
BBS ID for QWK Packets:

This is a short system ID for your BBS that is used for QWK packets.
It should be an abbreviation of your BBS name or other related string.
This ID will be used for your outgoing and incoming QWK packets. If
you plan on networking via QWK packets with another Synchronet BBS,
this ID should not begin with a number. The maximum length of the ID
is eight characters and cannot contain spaces or other invalid DOS
filename characters. In a QWK packet network, each system must have
a unique QWK system ID.
*/

				uinput(WIN_MID|WIN_SAV,0,0,"BBS ID for QWK Packets"
					,str,8,K_EDIT|K_UPPER);
				if(code_ok(str))
					strcpy(sys_id,str);
				else
					umsg("Invalid ID");
				break;
			case 1:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=0;
				SETHELP(WHERE);
/*
United States Time Zone:

If your local time zone is the United States, select Yes.
*/

				i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"United States Time Zone",opt);
				if(i==-1)
					break;
				if(i==0) {
					strcpy(opt[i++],"Atlantic");
					strcpy(opt[i++],"Eastern");
					strcpy(opt[i++],"Central");
					strcpy(opt[i++],"Mountain");
					strcpy(opt[i++],"Pacific");
					strcpy(opt[i++],"Yukon");
					strcpy(opt[i++],"Hawaii/Alaska");
					strcpy(opt[i++],"Bering");
					opt[i][0]=0;
					i=0;
					i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"Time Zone",opt);
					if(i==-1)
						break;
					changes=1;
					switch(i) {
						case 0:
							sys_timezone=AST;
							break;
						case 1:
							sys_timezone=EST;
							break;
						case 2:
							sys_timezone=CST;
                            break;
						case 3:
							sys_timezone=MST;
                            break;
						case 4:
							sys_timezone=PST;
                            break;
						case 5:
							sys_timezone=YST;
                            break;
						case 6:
							sys_timezone=HST;
                            break;
						case 7:
							sys_timezone=BST;
							break; }
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					i=1;
					i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"Daylight Savings",opt);
					if(i==-1)
                        break;
					if(!i)
						sys_timezone|=DAYLIGHT;
					break; }
				i=0;
				strcpy(opt[i++],"Midway");
				strcpy(opt[i++],"Vancouver");
				strcpy(opt[i++],"Edmonton");
				strcpy(opt[i++],"Winnipeg");
				strcpy(opt[i++],"Bogota");
				strcpy(opt[i++],"Caracas");
				strcpy(opt[i++],"Rio de Janeiro");
				strcpy(opt[i++],"Fernando de Noronha");
				strcpy(opt[i++],"Azores");
				strcpy(opt[i++],"London");
				strcpy(opt[i++],"Berlin");
				strcpy(opt[i++],"Athens");
				strcpy(opt[i++],"Moscow");
				strcpy(opt[i++],"Dubai");
				strcpy(opt[i++],"Kabul");
				strcpy(opt[i++],"Karachi");
				strcpy(opt[i++],"Bombay");
				strcpy(opt[i++],"Kathmandu");
				strcpy(opt[i++],"Dhaka");
				strcpy(opt[i++],"Bangkok");
				strcpy(opt[i++],"Hong Kong");
				strcpy(opt[i++],"Tokyo");
				strcpy(opt[i++],"Sydney");
				strcpy(opt[i++],"Noumea");
				strcpy(opt[i++],"Wellington");
				strcpy(opt[i++],"Other...");
				opt[i][0]=0;
				i=0;
				i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Time Zone",opt);
				if(i==-1)
					break;
				changes=1;
				switch(i) {
					case 0:
						sys_timezone=MID;
						break;
					case 1:
						sys_timezone=VAN;
						break;
					case 2:
						sys_timezone=EDM;
						break;
					case 3:
						sys_timezone=WIN;
						break;
					case 4:
						sys_timezone=BOG;
						break;
					case 5:
						sys_timezone=CAR;
						break;
					case 6:
						sys_timezone=RIO;
						break;
					case 7:
						sys_timezone=FER;
						break;
					case 8:
						sys_timezone=AZO;
                        break;
					case 9:
						sys_timezone=LON;
                        break;
					case 10:
						sys_timezone=BER;
                        break;
					case 11:
						sys_timezone=ATH;
                        break;
					case 12:
						sys_timezone=MOS;
                        break;
					case 13:
						sys_timezone=DUB;
                        break;
					case 14:
						sys_timezone=KAB;
                        break;
					case 15:
						sys_timezone=KAR;
                        break;
					case 16:
						sys_timezone=BOM;
                        break;
					case 17:
						sys_timezone=KAT;
                        break;
					case 18:
						sys_timezone=DHA;
                        break;
					case 19:
						sys_timezone=BAN;
                        break;
					case 20:
						sys_timezone=HON;
                        break;
					case 21:
						sys_timezone=TOK;
                        break;
					case 22:
						sys_timezone=SYD;
                        break;
					case 23:
						sys_timezone=NOU;
                        break;
					case 24:
						sys_timezone=WEL;
                        break;
					default:
						if(sys_timezone>720 || sys_timezone<-720)
							sys_timezone=0;
						sprintf(str,"%02d:%02d"
							,sys_timezone/60,sys_timezone<0
							? (-sys_timezone)%60 : sys_timezone%60);
						uinput(WIN_MID|WIN_SAV,0,0
							,"Time (HH:MM) East (+) or West (-) of Universal "
								"Time"
							,str,6,K_EDIT|K_UPPER);
						sys_timezone=atoi(str)*60;
						p=strchr(str,':');
						if(p) {
							if(sys_timezone<0)
								sys_timezone-=atoi(p+1);
							else
								sys_timezone+=atoi(p+1); }
                        break;
						}
                break;
			case 2:
				SETHELP(WHERE);
/*
Maximum Message Base Retry Time:

This is the maximum number of seconds to allow while attempting to open
or lock a message base (a value in the range of 10 to 45 seconds should
be fine).
*/
				itoa(smb_retry_time,str,10);
				uinput(WIN_MID|WIN_SAV,0,0
					,"Maximum Message Base Retry Time (in seconds)"
					,str,2,K_NUMBER|K_EDIT);
				smb_retry_time=atoi(str);
				break;
			case 3:
				SETHELP(WHERE);
/*
Maximum Messages Per QWK Packet:

This is the maximum number of messages (excluding E-mail), that a user
can have in one QWK packet for download. This limit does not effect
QWK network nodes (Q restriction). If set to 0, no limit is imposed.
*/

				ultoa(max_qwkmsgs,str,10);
				uinput(WIN_MID|WIN_SAV,0,0
					,"Maximum Messages Per QWK Packet (0=No Limit)"
					,str,6,K_NUMBER|K_EDIT);
				max_qwkmsgs=atol(str);
                break;
			case 4:
				SETHELP(WHERE);
/*
Pre-pack QWK Requirements:

ALL user accounts on the BBS meeting this requirmenet will have a QWK
packet automatically created for them every day after midnight
(during the internal daily event).

This is mainly intended for QWK network nodes that wish to save connect
time by having their packets pre-packed. If a large number of users meet
this requirement, it can take up a large amount of disk space on your
system (in the DATA\FILE directory).
*/
				getar("Pre-pack QWK (Use with caution!)",preqwk_ar);
				break;
			case 5:
				sprintf(str,"%u",mail_maxage);
                SETHELP(WHERE);
/*
Maximum Age of Mail:

This value is the maximum number of days that mail will be kept.
*/
                uinput(WIN_MID|WIN_SAV,0,17,"Maximum Age of Mail "
                    "(in days)",str,5,K_EDIT|K_NUMBER);
                mail_maxage=atoi(str);
                break;
			case 6:
				strcpy(opt[0],"Daily");
				strcpy(opt[1],"Immediately");
				opt[2][0]=0;
				i=0;
				SETHELP(WHERE);
/*
Purge Deleted E-mail:

If you wish to have deleted e-mail physically (and permanently) removed
from your e-mail database immediately after a users exits the reading
e-mail prompt, set this option to Immediately.

For the best system performance and to avoid delays when deleting e-mail
from a large e-mail database, set this option to Daily (the default).
Your system maintenance will automatically purge deleted e-mail once a
day.
*/

				i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Purge Deleted E-mail",opt);
				if(!i && sys_misc&SM_DELEMAIL) {
					sys_misc&=~SM_DELEMAIL;
					changes=1; }
				else if(i==1 && !(sys_misc&SM_DELEMAIL)) {
					sys_misc|=SM_DELEMAIL;
					changes=1; }
                break;
			case 7:
				sprintf(str,"%lu",mail_maxcrcs);
                SETHELP(WHERE);
/*
Maximum Number of Mail CRCs:

This value is the maximum number of CRCs that will be kept for duplicate
mail checking. Once this maximum number of CRCs is reached, the oldest
CRCs will be automatically purged.
*/
                uinput(WIN_MID|WIN_SAV,0,17,"Maximum Number of Mail "
                    "CRCs",str,5,K_EDIT|K_NUMBER);
                mail_maxcrcs=atol(str);
                break;
			case 8:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=1;
				SETHELP(WHERE);
/*
Allow Anonymous E-mail:

If you want users with the A exemption to be able to send E-mail
anonymously, set this option to Yes.
*/

				i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow Anonymous E-mail",opt);
				if(!i && !(sys_misc&SM_ANON_EM)) {
					sys_misc|=SM_ANON_EM;
					changes=1; }
				else if(i==1 && sys_misc&SM_ANON_EM) {
					sys_misc&=~SM_ANON_EM;
					changes=1; }
				break;
			case 9:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=0;
				SETHELP(WHERE);
/*
Allow Quoting in E-mail:

If you want your users to be allowed to use message quoting when
responding in E-mail, set this option to Yes.
*/

				i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow Quoting in E-mail",opt);
				if(!i && !(sys_misc&SM_QUOTE_EM)) {
					sys_misc|=SM_QUOTE_EM;
					changes=1; }
				else if(i==1 && sys_misc&SM_QUOTE_EM) {
					sys_misc&=~SM_QUOTE_EM;
					changes=1; }
				break;
			case 10:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=0;
				SETHELP(WHERE);
/*
Allow File Attachment Uploads in E-mail:

If you want your users to be allowed to attach an uploaded file to
an E-mail message, set this option to Yes.
*/

				i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow File Attachment Uploads in E-mail",opt);
				if(!i && !(sys_misc&SM_FILE_EM)) {
					sys_misc|=SM_FILE_EM;
					changes=1; }
				else if(i==1 && sys_misc&SM_FILE_EM) {
					sys_misc&=~SM_FILE_EM;
					changes=1; }
				break;
			case 11:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=0;
				SETHELP(WHERE);
/*
Allow Users to Have Their E-mail Forwarded to NetMail:

If you want your users to be able to have any e-mail sent to them
optionally (at the sender's discretion) forwarded to a NetMail address,
set this option to Yes.
*/

				i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow Forwarding of E-mail to NetMail",opt);
				if(!i && !(sys_misc&SM_FWDTONET)) {
					sys_misc|=SM_FWDTONET;
					changes=1; }
				else if(i==1 && sys_misc&SM_FWDTONET) {
					sys_misc&=~SM_FWDTONET;
					changes=1; }
                break;
			case 12:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=1;
				SETHELP(WHERE);
/*
Kill Read E-mail Automatically:

If this option is set to Yes, e-mail that has been read will be
automatically deleted when message base maintenance is run.
*/

				i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Kill Read E-mail Automatically",opt);
				if(!i && !(sys_misc&SM_DELREADM)) {
					sys_misc|=SM_DELREADM;
					changes=1; }
				else if(i==1 && sys_misc&SM_DELREADM) {
					sys_misc&=~SM_DELREADM;
					changes=1; }
                break;
			case 13:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				strcpy(opt[2],"Sysops Only");
				opt[3][0]=0;
				i=1;
				SETHELP(WHERE);
/*
Users Can View Deleted Messages:

If this option is set to Yes, then users will be able to view messages
they've sent and deleted or messages sent to them and they've deleted
with the option of un-deleting the message before the message is
physically purged from the e-mail database.

If this option is set to No, then when a message is deleted, it is no
longer viewable (with SBBS) by anyone.

If this option is set to Sysops Only, then only sysops and sub-ops (when
appropriate) can view deleted messages.
*/

				i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Users Can View Deleted Messages",opt);
				if(!i && (sys_misc&(SM_USRVDELM|SM_SYSVDELM))
					!=(SM_USRVDELM|SM_SYSVDELM)) {
					sys_misc|=(SM_USRVDELM|SM_SYSVDELM);
					changes=1; }
				else if(i==1 && sys_misc&(SM_USRVDELM|SM_SYSVDELM)) {
					sys_misc&=~(SM_USRVDELM|SM_SYSVDELM);
					changes=1; }
				else if(i==2 && (sys_misc&(SM_USRVDELM|SM_SYSVDELM))
					!=SM_SYSVDELM) {
					sys_misc|=SM_SYSVDELM;
					sys_misc&=~SM_USRVDELM;
					changes=1; }
                break;
			case 14:
				SETHELP(WHERE);
/*
Extra Attribute Codes...

Synchronet can suppport the native text attribute codes of other BBS
programs in messages (menus, posts, e-mail, etc.) To enable the extra
attribute codes for another BBS program, set the corresponding option
to Yes.
*/

				j=0;
				while(1) {
					i=0;
					sprintf(opt[i++],"%-15.15s %-3.3s","WWIV"
						,sys_misc&SM_WWIV ? "Yes":"No");
					sprintf(opt[i++],"%-15.15s %-3.3s","PCBoard"
						,sys_misc&SM_PCBOARD ? "Yes":"No");
					sprintf(opt[i++],"%-15.15s %-3.3s","Wildcat"
						,sys_misc&SM_WILDCAT ? "Yes":"No");
					sprintf(opt[i++],"%-15.15s %-3.3s","Celerity"
						,sys_misc&SM_CELERITY ? "Yes":"No");
					sprintf(opt[i++],"%-15.15s %-3.3s","Renegade"
						,sys_misc&SM_RENEGADE ? "Yes":"No");
					opt[i][0]=0;
					j=ulist(WIN_BOT|WIN_RHT|WIN_SAV,2,2,0,&j,0
						,"Extra Attribute Codes",opt);
					if(j==-1)
						break;

					changes=1;
					switch(j) {
						case 0:
							sys_misc^=SM_WWIV;
							break;
						case 1:
							sys_misc^=SM_PCBOARD;
							break;
						case 2:
							sys_misc^=SM_WILDCAT;
							break;
						case 3:
							sys_misc^=SM_CELERITY;
							break;
						case 4:
							sys_misc^=SM_RENEGADE;
							break; } } } }
}
