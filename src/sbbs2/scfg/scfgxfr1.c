#line 2 "SCFGXFR1.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "scfg.h"

void xfer_opts()
{
	static int xfr_dflt;
	char str[81],done;
	int i,j,k,l,dflt,bar;
	static fextr_t savfextr;
	static fview_t savfview;
	static ftest_t savftest;
	static fcomp_t savfcomp;
	static prot_t savprot;
	static dlevent_t savdlevent;
	static char savaltpath[LEN_DIR+1];

while(1) {
	i=0;
	sprintf(opt[i++],"%-33.33s%uk","Min Bytes Free Disk Space",min_dspace);
	sprintf(opt[i++],"%-33.33s%u","Max Files in Batch UL Queue"
		,max_batup);
	sprintf(opt[i++],"%-33.33s%u","Max Files in Batch DL Queue"
		,max_batdn);
	sprintf(opt[i++],"%-33.33s%u","Max Users in User Transfers",max_userxfer);
	sprintf(opt[i++],"%-33.33s%u%%","Default Credit on Upload"
		,cdt_up_pct);
	sprintf(opt[i++],"%-33.33s%u%%","Default Credit on Download"
		,cdt_dn_pct);
	if(leech_pct)
		sprintf(str,"%u%% after %u seconds",leech_pct,leech_sec);
	else
		strcpy(str,"Disabled");
	sprintf(opt[i++],"%-33.33s%s","Leech Protocol Detection",str);
	strcpy(opt[i++],"Viewable Files...");
	strcpy(opt[i++],"Testable Files...");
	strcpy(opt[i++],"Download Events...");
	strcpy(opt[i++],"Extractable Files...");
	strcpy(opt[i++],"Compressable Files...");
	strcpy(opt[i++],"Transfer Protocols...");
	strcpy(opt[i++],"Alternate File Paths...");
	opt[i][0]=0;
	SETHELP(WHERE);
/*
File Transfer Configuration:

This menu has options and sub-menus that pertain specifically to the
file transfer section of the BBS.
*/
	switch(ulist(WIN_ORG|WIN_ACT|WIN_CHE,0,0,72,&xfr_dflt,0
		,"File Transfer Configuration",opt)) {
		case -1:
			i=save_changes(WIN_MID);
			if(i==-1)
				break;
			if(!i)
				write_file_cfg();
            return;
		case 0:
			SETHELP(WHERE);
/*
Minimum Kilobytes Free Disk Space to Allow Uploads:

This is the minimum free space in a file directory to allow user
uploads.
*/
			uinput(WIN_MID,0,0
				,"Minimum Kilobytes Free Disk Space to Allow Uploads"
				,itoa(min_dspace,tmp,10),5,K_EDIT|K_NUMBER);
			min_dspace=atoi(tmp);
			break;
		case 1:
			SETHELP(WHERE);
/*
Maximum Files in Batch Upload Queue:

This is the maximum number of files that can be placed in the batch
upload queue.
*/
			uinput(WIN_MID,0,0,"Maximum Files in Batch Upload Queue"
				,itoa(max_batup,tmp,10),5,K_EDIT|K_NUMBER);
			max_batup=atoi(tmp);
            break;
		case 2:
			SETHELP(WHERE);
/*
Maximum Files in Batch Download Queue:

This is the maximum number of files that can be placed in the batch
download queue.
*/
			uinput(WIN_MID,0,0,"Maximum Files in Batch Download Queue"
				,itoa(max_batdn,tmp,10),5,K_EDIT|K_NUMBER);
			max_batdn=atoi(tmp);
            break;
		case 3:
			SETHELP(WHERE);
/*
Maximum Destination Users in User to User Transfer:

This is the maximum number of users allowed in the destination user list
of a user to user upload.
*/
			uinput(WIN_MID,0,0
				,"Maximum Destination Users in User to User Transfers"
				,itoa(max_userxfer,tmp,10),5,K_EDIT|K_NUMBER);
			max_userxfer=atoi(tmp);
			break;
		case 4:
SETHELP(WHERE);
/*
Default Percentage of Credits to Credit Uploader on Upload:

This is the default setting that will be used when new file
directories are created.
*/
			uinput(WIN_MID,0,0
				,"Default Percentage of Credits to Credit Uploader on Upload"
				,itoa(cdt_up_pct,tmp,10),4,K_EDIT|K_NUMBER);
			cdt_up_pct=atoi(tmp);
			break;
		case 5:
SETHELP(WHERE);
/*
Default Percentage of Credits to Credit Uploader on Download:

This is the default setting that will be used when new file
directories are created.
*/
			uinput(WIN_MID,0,0
				,"Default Percentage of Credits to Credit Uploader on Download"
				,itoa(cdt_dn_pct,tmp,10),4,K_EDIT|K_NUMBER);
			cdt_dn_pct=atoi(tmp);
			break;
		case 6:
			SETHELP(WHERE);
/*
Leech Protocol Detection Percentage:

This value is the sensitivity of the leech protocol detection feature of
Synchronet. If the transfer is apparently unsuccessful, but the transfer
time was at least this percentage of the estimated transfer time (based
on the estimated CPS of the connection result code), then a leech
protocol error is issued and the user's leech download counter is
incremented. Setting this value to 0 disables leech protocol detection.
*/
			savnum=0;
			uinput(WIN_MID|WIN_SAV,0,0
				,"Leech Protocol Detection Percentage (0=Disabled)"
				,itoa(leech_pct,tmp,10),3,K_EDIT|K_NUMBER);
			leech_pct=atoi(tmp);
			if(!leech_pct)
				break;
			SETHELP(WHERE);
/*
Leech Protocol Minimum Time (in Seconds):

This option allows you to adjust the sensitivity of the leech protocol
detection feature. This value is the minimum length of transfer time
(in seconds) that must elapse before an aborted tranfser will be
considered a possible leech attempt.
*/
			uinput(WIN_MID,0,0
				,"Leech Protocol Minimum Time (in Seconds)"
				,itoa(leech_sec,tmp,10),3,K_EDIT|K_NUMBER);
			leech_sec=atoi(tmp);
			break;
		case 7: 	/* Viewable file types */
			dflt=bar=0;
			while(1) {
				for(i=0;i<total_fviews && i<MAX_OPTS;i++)
					sprintf(opt[i],"%-3.3s  %-40s",fview[i]->ext,fview[i]->cmd);
				opt[i][0]=0;
				i=WIN_ACT|WIN_SAV;	/* save cause size can change */
				if(total_fviews<MAX_OPTS)
					i|=WIN_INS;
				if(total_fviews)
					i|=WIN_DEL|WIN_GET;
				if(savfview.cmd[0])
					i|=WIN_PUT;
				savnum=0;
				SETHELP(WHERE);
/*
Viewable File Types:

This is a list of file types that have content information that can be
viewed through the execution of an external program. Here are a couple of
command line examples for a few file types.
*/
				i=ulist(i,0,0,50,&dflt,&bar,"Viewable File Types",opt);
				if(i==-1)
					break;
				if((i&MSK_ON)==MSK_DEL) {
					i&=MSK_OFF;
					FREE(fview[i]);
					total_fviews--;
					while(i<total_fviews) {
						fview[i]=fview[i+1];
						i++; }
					changes=1;
					continue; }
				if((i&MSK_ON)==MSK_INS) {
					i&=MSK_OFF;
					if((fview=(fview_t **)REALLOC(fview
						,sizeof(fview_t *)*(total_fviews+1)))==NULL) {
						errormsg(WHERE,ERR_ALLOC,nulstr,total_fviews+1);
						total_fviews=0;
						bail(1);
						continue; }
					if(!total_fviews) {
						if((fview[0]=(fview_t *)MALLOC(
							sizeof(fview_t)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(fview_t));
							continue; }
						memset(fview[0],0,sizeof(fview_t));
						strcpy(fview[0]->ext,"ZIP");
						strcpy(fview[0]->cmd,"%!pkunzip -v %f"); }
					else {
						for(j=total_fviews;j>i;j--)
							fview[j]=fview[j-1];
						if((fview[i]=(fview_t *)MALLOC(
							sizeof(fview_t)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(fview_t));
							continue; }
						*fview[i]=*fview[i+1]; }
					total_fviews++;
					changes=1;
					continue; }
				if((i&MSK_ON)==MSK_GET) {
					i&=MSK_OFF;
					savfview=*fview[i];
					continue; }
				if((i&MSK_ON)==MSK_PUT) {
					i&=MSK_OFF;
					*fview[i]=savfview;
					changes=1;
					continue; }
				k=0;
				done=0;
				while(!done) {
					j=0;
					sprintf(opt[j++],"%-22.22s%s","File Extension"
						,fview[i]->ext);
					sprintf(opt[j++],"%-22.22s%-40s","Command Line"
						,fview[i]->cmd);
					sprintf(opt[j++],"%-22.22s%s","Access Requirements"
						,fview[i]->ar);
					opt[j][0]=0;
					savnum=1;
					switch(ulist(WIN_RHT|WIN_BOT|WIN_SAV|WIN_ACT,0,0,0,&k,0
						,"Viewable File Type",opt)) {
						case -1:
							done=1;
							break;
						case 0:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Viewable File Type Extension"
								,fview[i]->ext,3,K_UPPER|K_EDIT);
							break;
						case 1:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Command Line"
								,fview[i]->cmd,50,K_EDIT);
							break;
						case 2:
							savnum=2;
							sprintf(str,"Viewable File Type %s"
								,fview[i]->ext);
							getar(str,fview[i]->ar);
                            break; } } }
            break;
		case 8:    /* Testable file types */
			dflt=bar=0;
			while(1) {
				for(i=0;i<total_ftests && i<MAX_OPTS;i++)
					sprintf(opt[i],"%-3.3s  %-40s",ftest[i]->ext,ftest[i]->cmd);
				opt[i][0]=0;
				i=WIN_ACT|WIN_SAV;	/* save cause size can change */
				if(total_ftests<MAX_OPTS)
					i|=WIN_INS;
				if(total_ftests)
					i|=WIN_DEL|WIN_GET;
				if(savftest.cmd[0])
					i|=WIN_PUT;
				savnum=0;
				SETHELP(WHERE);
/*
Testable File Types:

This is a list of file types that will have a command line executed to
test the file integrity upon their upload. The file types are specified
by extension and if one file extension is listed more than once, each
command line will be executed. The command lines must return a DOS error
code of 0 (no error) in order for the file to pass the test. This method
of file testing upon upload is also known as an upload event. This test
or event, can do more than just test the file, it can perform any
function that the sysop wishes. Such as adding comments to an archived
file, or extracting an archive and performing a virus scan. While the
external program is executing, a text string is displayed to the user.
This working string can be set for each file type and command line
listed.
*/
				i=ulist(i,0,0,50,&dflt,&bar,"Testable File Types",opt);
				if(i==-1)
					break;
				if((i&MSK_ON)==MSK_DEL) {
					i&=MSK_OFF;
					FREE(ftest[i]);
					total_ftests--;
					while(i<total_ftests) {
						ftest[i]=ftest[i+1];
						i++; }
					changes=1;
					continue; }
				if((i&MSK_ON)==MSK_INS) {
					i&=MSK_OFF;
					if((ftest=(ftest_t **)REALLOC(ftest
						,sizeof(ftest_t *)*(total_ftests+1)))==NULL) {
						errormsg(WHERE,ERR_ALLOC,nulstr,total_ftests+1);
						total_ftests=0;
						bail(1);
						continue; }
					if(!total_ftests) {
						if((ftest[0]=(ftest_t *)MALLOC(
							sizeof(ftest_t)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(ftest_t));
							continue; }
						memset(ftest[0],0,sizeof(ftest_t));
						strcpy(ftest[0]->ext,"ZIP");
						strcpy(ftest[0]->cmd,"%!pkunzip -t %f");
						strcpy(ftest[0]->workstr,"Testing ZIP Integrity..."); }
					else {

						for(j=total_ftests;j>i;j--)
							ftest[j]=ftest[j-1];
						if((ftest[i]=(ftest_t *)MALLOC(
							sizeof(ftest_t)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(ftest_t));
							continue; }
						*ftest[i]=*ftest[i+1]; }
					total_ftests++;
					changes=1;
					continue; }
				if((i&MSK_ON)==MSK_GET) {
					i&=MSK_OFF;
					savftest=*ftest[i];
					continue; }
				if((i&MSK_ON)==MSK_PUT) {
					i&=MSK_OFF;
					*ftest[i]=savftest;
					changes=1;
					continue; }
				done=0;
				k=0;
				while(!done) {
					j=0;
					sprintf(opt[j++],"%-22.22s%s","File Extension"
						,ftest[i]->ext);
					sprintf(opt[j++],"%-22.22s%-40s","Command Line"
						,ftest[i]->cmd);
					sprintf(opt[j++],"%-22.22s%s","Working String"
						,ftest[i]->workstr);
					sprintf(opt[j++],"%-22.22s%s","Access Requirements"
						,ftest[i]->ar);
					opt[j][0]=0;
					savnum=1;
					switch(ulist(WIN_RHT|WIN_BOT|WIN_SAV|WIN_ACT,0,0,0,&k,0
						,"Testable File Type",opt)) {
						case -1:
							done=1;
							break;
						case 0:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Testable File Type Extension"
								,ftest[i]->ext,3,K_UPPER|K_EDIT);
							break;
						case 1:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Command Line"
								,ftest[i]->cmd,50,K_EDIT);
							break;
						case 2:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Working String"
								,ftest[i]->workstr,40,K_EDIT|K_MSG);
							break;
						case 3:
							savnum=2;
							sprintf(str,"Testable File Type %s",ftest[i]->ext);
							getar(str,ftest[i]->ar);
							break; } } }
			break;
		case 9:    /* Download Events */
			dflt=bar=0;
			while(1) {
				for(i=0;i<total_dlevents && i<MAX_OPTS;i++)
					sprintf(opt[i],"%-3.3s  %-40s",dlevent[i]->ext,dlevent[i]->cmd);
				opt[i][0]=0;
				i=WIN_ACT|WIN_SAV;	/* save cause size can change */
				if(total_dlevents<MAX_OPTS)
					i|=WIN_INS;
				if(total_dlevents)
					i|=WIN_DEL|WIN_GET;
				if(savdlevent.cmd[0])
					i|=WIN_PUT;
				savnum=0;
				SETHELP(WHERE);
/*
Download Events:

This is a list of file types that will have a command line executed to
perform an event upon their download (e.g. trigger a download event).
The file types are specified by extension and if one file extension
is listed more than once, each command line will be executed. The
command lines must return a DOS error code of 0 (no error) in order
for the file to pass the test. This test or event, can do more than
just test the file, it can perform any function that the sysop wishes.
Such as adding comments to an archived file, or extracting an archive
and performing a virus scan. While the external program is executing,
a text string is displayed to the user. This working string can be set
for each file type and command line listed.
*/
				i=ulist(i,0,0,50,&dflt,&bar,"Download Events",opt);
				if(i==-1)
					break;
				if((i&MSK_ON)==MSK_DEL) {
					i&=MSK_OFF;
					FREE(dlevent[i]);
					total_dlevents--;
					while(i<total_dlevents) {
						dlevent[i]=dlevent[i+1];
						i++; }
					changes=1;
					continue; }
				if((i&MSK_ON)==MSK_INS) {
					i&=MSK_OFF;
					if((dlevent=(dlevent_t **)REALLOC(dlevent
						,sizeof(dlevent_t *)*(total_dlevents+1)))==NULL) {
						errormsg(WHERE,ERR_ALLOC,nulstr,total_dlevents+1);
						total_dlevents=0;
						bail(1);
						continue; }
					if(!total_dlevents) {
						if((dlevent[0]=(dlevent_t *)MALLOC(
							sizeof(dlevent_t)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(dlevent_t));
							continue; }
						memset(dlevent[0],0,sizeof(dlevent_t));
						strcpy(dlevent[0]->ext,"ZIP");
						strcpy(dlevent[0]->cmd,"%!pkzip -z %f "
							"< ..\\text\\zipmsg.txt");
						strcpy(dlevent[0]->workstr,"Adding ZIP Comment..."); }
					else {

						for(j=total_dlevents;j>i;j--)
							dlevent[j]=dlevent[j-1];
						if((dlevent[i]=(dlevent_t *)MALLOC(
							sizeof(dlevent_t)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(dlevent_t));
							continue; }
						*dlevent[i]=*dlevent[i+1]; }
					total_dlevents++;
					changes=1;
					continue; }
				if((i&MSK_ON)==MSK_GET) {
					i&=MSK_OFF;
					savdlevent=*dlevent[i];
					continue; }
				if((i&MSK_ON)==MSK_PUT) {
					i&=MSK_OFF;
					*dlevent[i]=savdlevent;
					changes=1;
					continue; }
				done=0;
				k=0;
				while(!done) {
					j=0;
					sprintf(opt[j++],"%-22.22s%s","File Extension"
						,dlevent[i]->ext);
					sprintf(opt[j++],"%-22.22s%-40s","Command Line"
						,dlevent[i]->cmd);
					sprintf(opt[j++],"%-22.22s%s","Working String"
						,dlevent[i]->workstr);
					sprintf(opt[j++],"%-22.22s%s","Access Requirements"
						,dlevent[i]->ar);
					opt[j][0]=0;
					savnum=1;
					switch(ulist(WIN_RHT|WIN_BOT|WIN_SAV|WIN_ACT,0,0,0,&k,0
						,"Download Event",opt)) {
						case -1:
							done=1;
							break;
						case 0:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Download Event Extension"
								,dlevent[i]->ext,3,K_UPPER|K_EDIT);
							break;
						case 1:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Command Line"
								,dlevent[i]->cmd,50,K_EDIT);
							break;
						case 2:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Working String"
								,dlevent[i]->workstr,40,K_EDIT|K_MSG);
							break;
						case 3:
							savnum=2;
							sprintf(str,"Download Event %s",dlevent[i]->ext);
							getar(str,dlevent[i]->ar);
							break; } } }
            break;
		case 10:	 /* Extractable file types */
			dflt=bar=0;
            while(1) {
				for(i=0;i<total_fextrs && i<MAX_OPTS;i++)
                    sprintf(opt[i],"%-3.3s  %-40s"
                        ,fextr[i]->ext,fextr[i]->cmd);
				opt[i][0]=0;
                i=WIN_ACT|WIN_SAV;  /* save cause size can change */
				if(total_fextrs<MAX_OPTS)
                    i|=WIN_INS;
                if(total_fextrs)
                    i|=WIN_DEL|WIN_GET;
				if(savfextr.cmd[0])
                    i|=WIN_PUT;
                savnum=0;
                SETHELP(WHERE);
/*
Extractable File Types:

This is a list of archive file types that can be extracted to the temp
directory by an external program. The file types are specified by their
extension. For each file type you must specify the command line used to
extract the file(s).
*/
				i=ulist(i,0,0,50,&dflt,&bar,"Extractable File Types",opt);
                if(i==-1)
                    break;
				if((i&MSK_ON)==MSK_DEL) {
					i&=MSK_OFF;
                    FREE(fextr[i]);
                    total_fextrs--;
                    while(i<total_fextrs) {
                        fextr[i]=fextr[i+1];
                        i++; }
                    changes=1;
                    continue; }
				if((i&MSK_ON)==MSK_INS) {
					i&=MSK_OFF;
					if((fextr=(fextr_t **)REALLOC(fextr
						,sizeof(fextr_t *)*(total_fextrs+1)))==NULL) {
						errormsg(WHERE,ERR_ALLOC,nulstr,total_fextrs+1);
						total_fextrs=0;
						bail(1);
						continue; }
                    if(!total_fextrs) {
                        if((fextr[0]=(fextr_t *)MALLOC(
                            sizeof(fextr_t)))==NULL) {
                            errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(fextr_t));
                            continue; }
						memset(fextr[0],0,sizeof(fextr_t));
                        strcpy(fextr[0]->ext,"ZIP");
                        strcpy(fextr[0]->cmd,"%!pkunzip -o %f %g %s"); }
                    else {

                        for(j=total_fextrs;j>i;j--)
                            fextr[j]=fextr[j-1];
                        if((fextr[i]=(fextr_t *)MALLOC(
                            sizeof(fextr_t)))==NULL) {
                            errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(fextr_t));
                            continue; }
                        *fextr[i]=*fextr[i+1]; }
                    total_fextrs++;
                    changes=1;
                    continue; }
				if((i&MSK_ON)==MSK_GET) {
					i&=MSK_OFF;
                    savfextr=*fextr[i];
                    continue; }
				if((i&MSK_ON)==MSK_PUT) {
					i&=MSK_OFF;
                    *fextr[i]=savfextr;
                    changes=1;
                    continue; }
				k=0;
				done=0;
				while(!done) {
					j=0;
					sprintf(opt[j++],"%-22.22s%s","File Extension"
						,fextr[i]->ext);
					sprintf(opt[j++],"%-22.22s%-40s","Command Line"
						,fextr[i]->cmd);
					sprintf(opt[j++],"%-22.22s%s","Access Requirements"
						,fextr[i]->ar);
					opt[j][0]=0;
					savnum=1;
					switch(ulist(WIN_RHT|WIN_BOT|WIN_SAV|WIN_ACT,0,0,0,&k,0
						,"Extractable File Type",opt)) {
						case -1:
							done=1;
							break;
						case 0:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Extractable File Type Extension"
								,fextr[i]->ext,3,K_UPPER|K_EDIT);
							break;
						case 1:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Command Line"
								,fextr[i]->cmd,50,K_EDIT);
							break;
						case 2:
							savnum=2;
							sprintf(str,"Extractable File Type %s"
								,fextr[i]->ext);
							getar(str,fextr[i]->ar);
                            break; } } }
            break;
		case 11:	 /* Compressable file types */
			dflt=bar=0;
			while(1) {
				for(i=0;i<total_fcomps && i<MAX_OPTS;i++)
					sprintf(opt[i],"%-3.3s  %-40s",fcomp[i]->ext,fcomp[i]->cmd);
				opt[i][0]=0;
				i=WIN_ACT|WIN_SAV;	/* save cause size can change */
				if(total_fcomps<MAX_OPTS)
					i|=WIN_INS;
				if(total_fcomps)
					i|=WIN_DEL|WIN_GET;
				if(savfcomp.cmd[0])
					i|=WIN_PUT;
				savnum=0;
				SETHELP(WHERE);
/*
Compressable File Types:

This is a list of compression methods available for different file types.
These will be used for items such as creating QWK packets, temporary
files from the transfer section, and more.
*/
				i=ulist(i,0,0,50,&dflt,&bar,"Compressable File Types",opt);
				if(i==-1)
					break;
				if((i&MSK_ON)==MSK_DEL) {
					i&=MSK_OFF;
					FREE(fcomp[i]);
					total_fcomps--;
					while(i<total_fcomps) {
						fcomp[i]=fcomp[i+1];
						i++; }
					changes=1;
					continue; }
				if((i&MSK_ON)==MSK_INS) {
					i&=MSK_OFF;
					if((fcomp=(fcomp_t **)REALLOC(fcomp
						,sizeof(fcomp_t *)*(total_fcomps+1)))==NULL) {
						errormsg(WHERE,ERR_ALLOC,nulstr,total_fcomps+1);
						total_fcomps=0;
						bail(1);
						continue; }
					if(!total_fcomps) {
						if((fcomp[0]=(fcomp_t *)MALLOC(
							sizeof(fcomp_t)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(fcomp_t));
							continue; }
						memset(fcomp[0],0,sizeof(fcomp_t));
						strcpy(fcomp[0]->ext,"ZIP");
						strcpy(fcomp[0]->cmd,"%!pkzip %f %s"); }
					else {
						for(j=total_fcomps;j>i;j--)
							fcomp[j]=fcomp[j-1];
						if((fcomp[i]=(fcomp_t *)MALLOC(
							sizeof(fcomp_t)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(fcomp_t));
							continue; }
						*fcomp[i]=*fcomp[i+1]; }
					total_fcomps++;
					changes=1;
					continue; }
				if((i&MSK_ON)==MSK_GET) {
					i&=MSK_OFF;
					savfcomp=*fcomp[i];
					continue; }
				if((i&MSK_ON)==MSK_PUT) {
					i&=MSK_OFF;
					*fcomp[i]=savfcomp;
					changes=1;
					continue; }
				k=0;
				done=0;
				while(!done) {
					j=0;
					sprintf(opt[j++],"%-22.22s%s","File Extension"
						,fcomp[i]->ext);
					sprintf(opt[j++],"%-22.22s%-40s","Command Line"
						,fcomp[i]->cmd);
					sprintf(opt[j++],"%-22.22s%s","Access Requirements"
						,fcomp[i]->ar);
					opt[j][0]=0;
					savnum=1;
					switch(ulist(WIN_RHT|WIN_BOT|WIN_SAV|WIN_ACT,0,0,0,&k,0
						,"Compressable File Type",opt)) {
						case -1:
							done=1;
							break;
						case 0:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Compressable File Type Extension"
								,fcomp[i]->ext,3,K_UPPER|K_EDIT);
							break;
						case 1:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Command Line"
								,fcomp[i]->cmd,50,K_EDIT);
							break;
						case 2:
							savnum=2;
							sprintf(str,"Compressable File Type %s"
								,fcomp[i]->ext);
							getar(str,fcomp[i]->ar);
                            break; } } }
            break;
		case 12:	/* Transfer protocols */
			dflt=bar=0;
			while(1) {
				for(i=0;i<total_prots && i<MAX_OPTS;i++)
					sprintf(opt[i],"%c  %-40s"
						,prot[i]->mnemonic,prot[i]->ulcmd);
				opt[i][0]=0;
				i=WIN_ACT|WIN_SAV;	/* save cause size can change */
				if(total_prots<MAX_OPTS)
					i|=WIN_INS;
				if(total_prots)
					i|=WIN_DEL|WIN_GET;
				if(savprot.mnemonic)
					i|=WIN_PUT;
				savnum=0;
				SETHELP(WHERE);
/*
File Transfer Protocols:

This is a list of file transfer protocols that can be used to transfer
files either to or from a remote user. For each protocol, you can
specify the mnemonic (hot-key) to use to specify that protocol, the
command line to use for uploads, downloads, batch uploads, batch
downloads, bidirectional file transfers, and the support of DSZLOG. If
the protocol doesn't support a certain method of transfer, or you don't
wish it to be available for a certain method of transfer, leave the
command line for that method blank. Be advised, that if you add or
remove any transfer protocols, you will need to edit the protocol menus
(ULPROT, DLPROT, BATUPROT, BATDPROT, and BIPROT) in the TEXT\MENU
directory accordingly.
*/
				i=ulist(i,0,0,50,&dflt,&bar,"File Transfer Protocols",opt);
				if(i==-1)
					break;
				if((i&MSK_ON)==MSK_DEL) {
					i&=MSK_OFF;
					FREE(prot[i]);
					total_prots--;
					while(i<total_prots) {
						prot[i]=prot[i+1];
						i++; }
					changes=1;
					continue; }
				if((i&MSK_ON)==MSK_INS) {
					i&=MSK_OFF;
					if((prot=(prot_t **)REALLOC(prot
						,sizeof(prot_t *)*(total_prots+1)))==NULL) {
						errormsg(WHERE,ERR_ALLOC,nulstr,total_prots+1);
						total_prots=0;
						bail(1);
						continue; }
					if(!total_prots) {
						if((prot[0]=(prot_t *)MALLOC(
							sizeof(prot_t)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(prot_t));
							continue; }
						memset(prot[0],0,sizeof(prot_t));
						prot[0]->mnemonic='Y';
						prot[0]->misc=PROT_DSZLOG;
						strcpy(prot[0]->ulcmd
							,"%!dsz port %p estimate 0 %e rb %f");
						strcpy(prot[0]->dlcmd
							,"%!dsz port %p estimate 0 %e sb %f");
						strcpy(prot[0]->batulcmd
							,"%!dsz port %p estimate 0 %e rb %f");
						strcpy(prot[0]->batdlcmd
							,"%!dsz port %p estimate 0 %e sb @%f"); }
					else {

						for(j=total_prots;j>i;j--)
							prot[j]=prot[j-1];
						if((prot[i]=(prot_t *)MALLOC(
							sizeof(prot_t)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(prot_t));
							continue; }
						*prot[i]=*prot[i+1]; }
					total_prots++;
					changes=1;
					continue; }
				if((i&MSK_ON)==MSK_GET) {
					i&=MSK_OFF;
					savprot=*prot[i];
					continue; }
				if((i&MSK_ON)==MSK_PUT) {
					i&=MSK_OFF;
					*prot[i]=savprot;
					changes=1;
					continue; }
				done=0;
				k=0;
				while(!done) {
					j=0;
					sprintf(opt[j++],"%-25.25s%c","Mnemonic (Command Key)"
						,prot[i]->mnemonic);
					sprintf(opt[j++],"%-25.25s%-40s","Protocol Name"
						,prot[i]->name);
					sprintf(opt[j++],"%-25.25s%-40s","Access Requirements"
						,prot[i]->ar);
					sprintf(opt[j++],"%-25.25s%-40s","Upload Command Line"
						,prot[i]->ulcmd);
					sprintf(opt[j++],"%-25.25s%-40s","Download Command Line"
						,prot[i]->dlcmd);
					sprintf(opt[j++],"%-25.25s%-40s","Batch UL Command Line"
						,prot[i]->batulcmd);
					sprintf(opt[j++],"%-25.25s%-40s","Batch DL Command Line"
						,prot[i]->batdlcmd);
					sprintf(opt[j++],"%-25.25s%-40s","Bidir Command Line"
						,prot[i]->bicmd);
					sprintf(opt[j++],"%-25.25s%s","Uses DSZLOG"
						,prot[i]->misc&PROT_DSZLOG ? "Yes":"No");
					opt[j][0]=0;
					savnum=1;
					switch(ulist(WIN_RHT|WIN_BOT|WIN_SAV|WIN_ACT,0,0,0,&k,0
						,"File Transfer Protocol",opt)) {
						case -1:
							done=1;
							break;
						case 0:
							str[0]=prot[i]->mnemonic;
							str[1]=0;
							uinput(WIN_MID|WIN_SAV,0,0
								,"Mnemonic (Command Key)"
								,str,1,K_UPPER|K_EDIT);
							if(str[0])
								prot[i]->mnemonic=str[0];
							break;
						case 1:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Protocol Name"
								,prot[i]->name,25,K_EDIT);
                            break;
						case 2:
							savnum=2;
							sprintf(str,"Protocol %s",prot[i]->name);
							getar(str,prot[i]->ar);
							break;
						case 3:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Upload Command"
								,prot[i]->ulcmd,50,K_EDIT);
							break;
						case 4:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Download Command"
								,prot[i]->dlcmd,50,K_EDIT);
                            break;
						case 5:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Batch UL Command"
								,prot[i]->batulcmd,50,K_EDIT);
                            break;
						case 6:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Batch DL Command"
								,prot[i]->batdlcmd,50,K_EDIT);
                            break;
						case 7:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Bi-dir Command"
								,prot[i]->bicmd,50,K_EDIT);
                            break;
						case 8:
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							l=0;
							savnum=2;
							l=ulist(WIN_MID|WIN_SAV,0,0,0,&l,0
								,"Uses DSZLOG",opt);
							if(!l && !(prot[i]->misc&PROT_DSZLOG)) {
								prot[i]->misc|=PROT_DSZLOG;
								changes=1; }
							else if(l==1 && prot[i]->misc&PROT_DSZLOG) {
								prot[i]->misc&=~PROT_DSZLOG;
								changes=1; }
							break; } } }
			break;
		case 13:	/* Alternate file paths */
			dflt=bar=0;
			while(1) {
				for(i=0;i<altpaths;i++)
					sprintf(opt[i],"%3d: %-40s",i+1,altpath[i]);
				opt[i][0]=0;
				i=WIN_ACT|WIN_SAV;	/* save cause size can change */
				if((int)altpaths<MAX_OPTS)
					i|=WIN_INS;
				if(altpaths)
					i|=WIN_DEL|WIN_GET;
				if(savaltpath[0])
					i|=WIN_PUT;
				savnum=0;
				SETHELP(WHERE);
/*
Alternate File Paths:

This option allows the sysop to add and configure alternate file paths
for files stored on drives and directories other than the configured
storage path for a file directory. This command is useful for those who
have file directories where they wish to have files listed from
multiple CD-ROMs or hard disks.
*/
				i=ulist(i,0,0,50,&dflt,&bar,"Alternate File Paths",opt);
				if(i==-1)
					break;
				if((i&MSK_ON)==MSK_DEL) {
					i&=MSK_OFF;
					FREE(altpath[i]);
					altpaths--;
					while(i<altpaths) {
						altpath[i]=altpath[i+1];
						i++; }
					changes=1;
					continue; }
				if((i&MSK_ON)==MSK_INS) {
					i&=MSK_OFF;
					if((altpath=(char **)REALLOC(altpath
						,sizeof(char *)*(altpaths+1)))==NULL) {
						errormsg(WHERE,ERR_ALLOC,nulstr,altpaths+1);
						altpaths=0;
						bail(1);
                        continue; }
					if(!altpaths) {
						if((altpath[0]=(char *)MALLOC(LEN_DIR+1))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,LEN_DIR+1);
							continue; }
						memset(altpath[0],0,LEN_DIR+1); }
					else {
						for(j=altpaths;j>i;j--)
							altpath[j]=altpath[j-1];
						if((altpath[i]=(char *)MALLOC(LEN_DIR+1))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,LEN_DIR+1);
							continue; }
						memcpy(altpath[i],altpath[i+1],LEN_DIR+1); }
					altpaths++;
					changes=1;
					continue; }
				if((i&MSK_ON)==MSK_GET) {
					i&=MSK_OFF;
					memcpy(savaltpath,altpath[i],LEN_DIR+1);
					continue; }
				if((i&MSK_ON)==MSK_PUT) {
					i&=MSK_OFF;
					memcpy(altpath[i],savaltpath,LEN_DIR+1);
					changes=1;
					continue; }
				sprintf(str,"Path %d",i+1);
				uinput(WIN_MID|WIN_SAV,0,0,str,altpath[i],50,K_UPPER|K_EDIT); }
			break; } }
}

