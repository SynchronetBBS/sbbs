#line 2 "SCFGXFR2.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "scfg.h"

void xfer_cfg()
{
	static int libs_dflt,libs_bar,dflt;
	char str[256],str2[81],done=0,*p;
	int file,j,k,q;
	uint i;
	long ported;
	static lib_t savlib;
	dir_t tmpdir;
	FILE *stream;

while(1) {
	for(i=0;i<total_libs && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-25s",lib[i]->lname);
	opt[i][0]=0;
	j=WIN_ACT|WIN_CHE|WIN_ORG;
	if(total_libs)
		j|=WIN_DEL|WIN_GET|WIN_DELACT;
	if(total_libs<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savlib.sname[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
File Libraries:

This is a listing of file libraries for your BBS. File Libraries are
used to logically separate your file directories into groups. Every
directory belongs to a file library.

One popular use for file libraries is to separate CD-ROM and hard disk
directories. One might have an Uploads file library that contains
uploads to the hard disk directories and also have a PC-SIG file
library that contains directories from a PC-SIG CD-ROM. Some sysops
separate directories into more specific areas such as Main, Graphics,
or Adult. If you have many directories that have a common subject
denominator, you may want to have a separate file library for those
directories for a more organized file structure.
*/
	i=ulist(j,0,0,45,&libs_dflt,&libs_bar,"File Libraries",opt);
	if((signed)i==-1) {
		j=save_changes(WIN_MID);
		if(j==-1)
			continue;
		if(!j)
			write_file_cfg();
		return; }
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		strcpy(str,"Main");
		SETHELP(WHERE);
/*
Library Long Name:

This is a description of the file library which is displayed when a
user of the system uses the /* command from the file transfer menu.
*/*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Library Long Name",str,LEN_GLNAME
			,K_EDIT)<1)
			continue;
		sprintf(str2,"%.*s",LEN_GSNAME,str);
		SETHELP(WHERE);
/*
Library Short Name:

This is a short description of the file library which is used for the
file transfer menu prompt.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Library Short Name",str2,LEN_GSNAME
			,K_EDIT)<1)
			continue;
		if((lib=(lib_t **)REALLOC(lib,sizeof(lib_t *)*(total_libs+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,total_libs+1);
			total_libs=0;
			bail(1);
            continue; }

		if(total_libs) {
			for(j=total_libs;j>i;j--)
                lib[j]=lib[j-1];
			for(j=0;j<total_dirs;j++)
				if(dir[j]->lib>=i)
					dir[j]->lib++; }
		if((lib[i]=(lib_t *)MALLOC(sizeof(lib_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(lib_t));
			continue; }
		memset((lib_t *)lib[i],0,sizeof(lib_t));
		strcpy(lib[i]->lname,str);
		strcpy(lib[i]->sname,str2);
		total_libs++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Delete All Data in Library:

If you wish to delete the database files for all directories in this
library, select Yes.
*/
		j=1;
		strcpy(opt[0],"Yes");
		strcpy(opt[1],"No");
		opt[2][0]=0;
		j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
			,"Delete All Library Data Files",opt);
		if(j==-1)
			continue;
		if(j==0)
			for(j=0;j<total_dirs;j++)
				if(dir[j]->lib==i) {
					sprintf(str,"%s.*",dir[j]->code);
					if(!dir[j]->data_dir[0])
						sprintf(tmp,"%sDIRS\\",data_dir);
					else
						strcpy(tmp,dir[j]->data_dir);
					delfiles(tmp,str); }
		FREE(lib[i]);
		for(j=0;j<total_dirs;) {
			if(dir[j]->lib==i) {
				FREE(dir[j]);
				total_dirs--;
				k=j;
				while(k<total_dirs) {
					dir[k]=dir[k+1];
					k++; } }
			else j++; }
		for(j=0;j<total_dirs;j++)
			if(dir[j]->lib>i)
				dir[j]->lib--;
		total_libs--;
		while(i<total_libs) {
			lib[i]=lib[i+1];
			i++; }
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savlib=*lib[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*lib[i]=savlib;
		changes=1;
        continue; }
	done=0;
	while(!done) {
		j=0;
		sprintf(opt[j++],"%-27.27s%s","Long Name",lib[i]->lname);
		sprintf(opt[j++],"%-27.27s%s","Short Name",lib[i]->sname);
		sprintf(opt[j++],"%-27.27s%.40s","Access Requirements"
			,lib[i]->ar);
		strcpy(opt[j++],"Clone Options");
		strcpy(opt[j++],"Export Areas...");
		strcpy(opt[j++],"Import Areas...");
		strcpy(opt[j++],"File Directories...");
		opt[j][0]=0;
		savnum=0;
		sprintf(str,"%s Library",lib[i]->sname);
		SETHELP(WHERE);
/*
File Library Configuration:

This menu allows you to configure the security requirments for access
to this file library. You can also add, delete, and configure the
directories of this library by selecting the File Directories... option.
*/
		switch(ulist(WIN_ACT,6,4,60,&dflt,0,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Library Long Name:

This is a description of the file library which is displayed when a
user of the system uses the /* command from the file transfer menu.
*/*/
				strcpy(str,lib[i]->lname);	/* save */
				if(!uinput(WIN_MID|WIN_SAV,0,0,"Name to use for Listings"
					,lib[i]->lname,LEN_GLNAME,K_EDIT))
					strcpy(lib[i]->lname,str);	/* restore */
				break;
			case 1:
				SETHELP(WHERE);
/*
Library Short Name:

This is a short description of the file librarly which is used for the
file transfer menu prompt.
*/
				uinput(WIN_MID|WIN_SAV,0,0,"Name to use for Prompts"
					,lib[i]->sname,LEN_GSNAME,K_EDIT);
				break;
			case 2:
				sprintf(str,"%s Library",lib[i]->sname);
				getar(str,lib[i]->ar);
				break;
			case 3: /* clone options */
				j=0;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Clone Directory Options:

If you want to clone the options of the first directory of this library
into all directories of this library, select Yes.

The options cloned are upload requirments, download requirments,
operator requirements, exempted user requirements, toggle options,
maximum number of files, allowed file extensions, default file
extension, and sort type.
*/
				j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
					,"Clone Options of First Directory into All of Library"
					,opt);
				if(j==0) {
					k=-1;
					for(j=0;j<total_dirs;j++)
						if(dir[j]->lib==i) {
							if(k==-1)
								k=j;
							else {
								changes=1;
								dir[j]->misc=dir[k]->misc;
								strcpy(dir[j]->ul_ar,dir[k]->ul_ar);
								strcpy(dir[j]->dl_ar,dir[k]->dl_ar);
								strcpy(dir[j]->op_ar,dir[k]->op_ar);
								strcpy(dir[j]->ex_ar,dir[k]->ex_ar);
								strcpy(dir[j]->exts,dir[k]->exts);
								strcpy(dir[j]->data_dir,dir[k]->data_dir);
								strcpy(dir[j]->upload_sem,dir[k]->upload_sem);
								dir[j]->maxfiles=dir[k]->maxfiles;
								dir[j]->maxage=dir[k]->maxage;
								dir[j]->up_pct=dir[k]->up_pct;
								dir[j]->dn_pct=dir[k]->dn_pct;
								dir[j]->seqdev=dir[k]->seqdev;
								dir[j]->sort=dir[k]->sort; } } }
                break;
			case 4:
				k=0;
				ported=0;
				q=changes;
				strcpy(opt[k++],"DIRS.TXT    (Synchronet)");
				strcpy(opt[k++],"FILEBONE.NA (Fido)");
				opt[k][0]=0;
				SETHELP(WHERE);
/*
Export Area File Format:

This menu allows you to choose the format of the area file you wish to
export to.
*/
				k=0;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Export Area File Format",opt);
				if(k==-1)
					break;
				if(k==0)
					sprintf(str,"%sDIRS.TXT",ctrl_dir);
				else if(k==1)
					sprintf(str,"FILEBONE.NA");
				strupr(str);
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
				for(j=0;j<total_dirs;j++) {
					if(dir[j]->lib!=i)
						continue;
					ported++;
					if(k==1) {
						fprintf(stream,"Area %-8s  0     !      %s\r\n"
							,dir[j]->code,dir[j]->lname);
						continue; }
					fprintf(stream,"%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n"
							"%s\r\n%s\r\n"
						,dir[j]->lname
						,dir[j]->sname
						,dir[j]->code
						,dir[j]->data_dir
						,dir[j]->ar
						,dir[j]->ul_ar
						,dir[j]->dl_ar
						,dir[j]->op_ar
						);
					fprintf(stream,"%s\r\n%s\r\n%u\r\n%s\r\n%lX\r\n%u\r\n"
							"%u\r\n"
						,dir[j]->path
						,dir[j]->upload_sem
						,dir[j]->maxfiles
						,dir[j]->exts
						,dir[j]->misc
						,dir[j]->seqdev
						,dir[j]->sort
						);
					fprintf(stream,"%s\r\n%u\r\n%u\r\n%u\r\n"
						,dir[j]->ex_ar
						,dir[j]->maxage
						,dir[j]->up_pct
						,dir[j]->dn_pct
						);
					fprintf(stream,"***END-OF-DIR***\r\n\r\n"); }
				fclose(stream);
				upop(0);
				sprintf(str,"%lu File Areas Exported Successfully",ported);
				umsg(str);
				changes=q;
				break;

			case 5:
				ported=0;
				k=0;
				SETHELP(WHERE);
/*
Import Area File Format:

This menu allows you to choose the format of the area file you wish to
import into the current file library.
*/
				strcpy(opt[k++],"DIRS.TXT    (Synchronet)");
                strcpy(opt[k++],"FILEBONE.NA (Fido)");
				opt[k][0]=0;
				k=0;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Import Area File Format",opt);
				if(k==-1)
					break;
				if(k==0)
					sprintf(str,"%sDIRS.TXT",ctrl_dir);
				else if(k==1)
					sprintf(str,"FILEBONE.NA");
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
						if(strnicmp(p,"AREA ",5))
							continue;
						memset(&tmpdir,0,sizeof(dir_t));
						tmpdir.misc|=
							(DIR_FCHK|DIR_DUPES|DIR_CDTUL|DIR_CDTDL|DIR_DIZ);
						if(k==1) {
							p+=5;
							while(*p && *p<=SP) p++;
							sprintf(tmpdir.code,"%.8s",p);
							truncsp(tmpdir.code);
							while(*p>SP) p++;			/* Skip areaname */
							while(*p && *p<=SP) p++;	/* Skip space */
							while(*p>SP) p++;			/* Skip level */
							while(*p && *p<=SP) p++;	/* Skip space */
							while(*p>SP) p++;			/* Skip flags */
							while(*p && *p<=SP) p++;	/* Skip space */
							sprintf(tmpdir.lname,"%.*s",LEN_SLNAME,p); }
						ported++; }
					else {
						memset(&tmpdir,0,sizeof(dir_t));
						sprintf(tmpdir.lname,"%.*s",LEN_SLNAME,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpdir.sname,"%.*s",LEN_SSNAME,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpdir.code,"%.*s",8,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpdir.data_dir,"%.*s",LEN_DIR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpdir.ar,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpdir.ul_ar,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpdir.dl_ar,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpdir.op_ar,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
                        truncsp(str);
                        sprintf(tmpdir.path,"%.*s",LEN_DIR,str);
						if(!fgets(str,128,stream)) break;
                        truncsp(str);
                        sprintf(tmpdir.upload_sem,"%.*s",LEN_DIR,str);
						if(!fgets(str,128,stream)) break;
                        truncsp(str);
						tmpdir.maxfiles=atoi(str);
						if(!fgets(str,128,stream)) break;
                        truncsp(str);
						sprintf(tmpdir.exts,"%.*s",40,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpdir.misc=ahtoul(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpdir.seqdev=atoi(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpdir.sort=atoi(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpdir.ex_ar,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpdir.maxage=atoi(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpdir.up_pct=atoi(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpdir.dn_pct=atoi(str);

						ported++;
						while(!feof(stream)
							&& strcmp(str,"***END-OF-DIR***")) {
							if(!fgets(str,128,stream)) break;
							truncsp(str); } }

					for(j=0;j<total_dirs;j++) {
						if(dir[j]->lib!=i)
							continue;
						if(!stricmp(dir[j]->code,tmpdir.code))
							break; }
					if(j==total_dirs) {

						if((dir=(dir_t **)REALLOC(dir
							,sizeof(dir_t *)*(total_dirs+1)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,"dir",total_dirs+1);
							total_dirs=0;
							bail(1);
							break; }

						if((dir[j]=(dir_t *)MALLOC(sizeof(dir_t)))
							==NULL) {
							errormsg(WHERE,ERR_ALLOC,"dir",sizeof(dir_t));
							break; }
						memset(dir[j],0,sizeof(dir_t)); }
					if(!k)
						memcpy(dir[j],&tmpdir,sizeof(dir_t));
					else {
						strcpy(dir[j]->code,tmpdir.code);
						strcpy(dir[j]->sname,tmpdir.code);
						strcpy(dir[j]->lname,tmpdir.lname);
						if(j==total_dirs) {
							dir[j]->maxfiles=1000;
							dir[j]->up_pct=cdt_up_pct;
							dir[j]->dn_pct=cdt_dn_pct; }
						}
					dir[j]->lib=i;
					if(j==total_dirs) {
						dir[j]->misc=tmpdir.misc;
						total_dirs++; }
					changes=1; }
				fclose(stream);
				upop(0);
				sprintf(str,"%lu File Areas Imported Successfully",ported);
                umsg(str);
                break;

			case 6:
				dir_cfg(i);
				break; } } }

}

void dir_cfg(uint libnum)
{
	static int dflt,bar,tog_dflt,tog_bar,adv_dflt,opt_dflt;
	char str[81],str2[81],code[9],path[128],done=0,*p;
	int j,n;
	uint i,dirnum[MAX_OPTS+1];
	static dir_t savdir;

while(1) {
	for(i=0,j=0;i<total_dirs && j<MAX_OPTS;i++)
		if(dir[i]->lib==libnum) {
			sprintf(opt[j],"%-25s",dir[i]->lname);
			dirnum[j++]=i; }
	dirnum[j]=total_dirs;
	opt[j][0]=0;
	sprintf(str,"%s Directories",lib[libnum]->sname);
	savnum=0;
	i=WIN_SAV|WIN_ACT;
	if(j)
		i|=WIN_DEL|WIN_GET|WIN_DELACT;
	if(j<MAX_OPTS)
		i|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savdir.sname[0])
		i|=WIN_PUT;
	SETHELP(WHERE);
/*
File Directories:

This is a list of file directories that have been configured for the
selected file library.

To add a directory, select the desired position with the arrow keys and
hit  INS .

To delete a directory, select it with the arrow keys and hit  DEL .

To configure a directory, select it with the arrow keys and hit  ENTER .
*/
	i=ulist(i,24,1,45,&dflt,&bar,str,opt);
	savnum=1;
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		strcpy(str,"Games");
		SETHELP(WHERE);
/*
Directory Long Name:

This is a description of the file directory which is displayed in all
directory listings.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Directory Long Name",str,LEN_SLNAME
			,K_EDIT)<1)
			continue;
		sprintf(str2,"%.*s",LEN_SSNAME,str);
		SETHELP(WHERE);
/*
Directory Short Name:

This is a short description of the file directory which is displayed at
the file transfer prompt.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Directory Short Name",str2,LEN_SSNAME
			,K_EDIT)<1)
            continue;
		sprintf(code,"%.8s",str2);
		p=strchr(code,SP);
		if(p) *p=0;
		strupr(code);
		SETHELP(WHERE);
/*
Directory Internal Code:

Every directory must have its own unique code for Synchronet to refer to
it internally. This code should be descriptive of the directory's
contents, usually an abreviation of the directory's name.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Directory Internal Code",code,8
			,K_EDIT|K_UPPER)<1)
            continue;
		if(!code_ok(code)) {
			helpbuf=invalid_code;
			umsg("Invalid Code");
			helpbuf=0;
			continue; }
		sprintf(path,"%sDIRS\\%s",data_dir,code);
		SETHELP(WHERE);
/*
Directory File Path:

This is the drive and directory where your uploads to and downloads from
this directory will be stored. Example: C:\XFER\GAMES
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Directory File Path",path,50
			,K_EDIT|K_UPPER)<1)
			continue;
		if((dir=(dir_t **)REALLOC(dir,sizeof(dir_t *)*(total_dirs+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,total_dirs+1);
			total_dirs=0;
			bail(1);
            continue; }

		if(j)
			for(n=total_dirs;n>dirnum[i];n--)
                dir[n]=dir[n-1];
		if((dir[dirnum[i]]=(dir_t *)MALLOC(sizeof(dir_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(dir_t));
			continue; }
		memset((dir_t *)dir[dirnum[i]],0,sizeof(dir_t));
		dir[dirnum[i]]->lib=libnum;
		dir[dirnum[i]]->maxfiles=MAX_FILES<500 ? MAX_FILES:500;
		if(strcmpi(str2,"OFFLINE"))
			dir[dirnum[i]]->misc=(DIR_FCHK|DIR_MULT|DIR_DUPES
				|DIR_CDTUL|DIR_CDTDL|DIR_DIZ);
		strcpy(dir[dirnum[i]]->code,code);
		strcpy(dir[dirnum[i]]->lname,str);
		strcpy(dir[dirnum[i]]->sname,str2);
		strcpy(dir[dirnum[i]]->path,path);
		dir[dirnum[i]]->up_pct=cdt_up_pct;
		dir[dirnum[i]]->dn_pct=cdt_dn_pct;
		total_dirs++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Delete Directory Data Files:

If you want to delete all the database files for this directory,
select Yes.
*/
		j=1;
		strcpy(opt[0],"Yes");
		strcpy(opt[1],"No");
		opt[2][0]=0;
		j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
			,"Delete Data in Sub-board",opt);
		if(j==-1)
			continue;
		if(j==0) {
				sprintf(str,"%s.*",dir[dirnum[i]]->code);
				if(!dir[dirnum[i]]->data_dir[0])
					sprintf(tmp,"%sDIRS\\",data_dir);
				else
					strcpy(tmp,dir[dirnum[i]]->data_dir);
				delfiles(tmp,str); }
		FREE(dir[dirnum[i]]);
		total_dirs--;
		for(j=dirnum[i];j<total_dirs;j++)
			dir[j]=dir[j+1];
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savdir=*dir[dirnum[i]];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*dir[dirnum[i]]=savdir;
		dir[dirnum[i]]->lib=libnum;
		changes=1;
        continue; }
	i=dirnum[dflt];
	j=0;
	done=0;
	while(!done) {
		n=0;
		sprintf(opt[n++],"%-27.27s%s","Long Name",dir[i]->lname);
		sprintf(opt[n++],"%-27.27s%s","Short Name",dir[i]->sname);
		sprintf(opt[n++],"%-27.27s%s","Internal Code",dir[i]->code);
		sprintf(opt[n++],"%-27.27s%.40s","Access Requirements"
			,dir[i]->ar);
		sprintf(opt[n++],"%-27.27s%.40s","Upload Requirements"
			,dir[i]->ul_ar);
		sprintf(opt[n++],"%-27.27s%.40s","Download Requirements"
			,dir[i]->dl_ar);
		sprintf(opt[n++],"%-27.27s%.40s","Operator Requirements"
            ,dir[i]->op_ar);
		sprintf(opt[n++],"%-27.27s%.40s","Exemption Requirements"
			,dir[i]->ex_ar);
		sprintf(opt[n++],"%-27.27s%.40s","Transfer File Path"
			,dir[i]->path);
		sprintf(opt[n++],"%-27.27s%u","Maximum Number of Files"
			,dir[i]->maxfiles);
		if(dir[i]->maxage)
			sprintf(str,"Enabled (%u days old)",dir[i]->maxage);
        else
            strcpy(str,"Disabled");
        sprintf(opt[n++],"%-27.27s%s","Purge by Age",str);
		sprintf(opt[n++],"%-27.27s%u%%","Credit on Upload"
			,dir[i]->up_pct);
		sprintf(opt[n++],"%-27.27s%u%%","Credit on Download"
			,dir[i]->dn_pct);
		strcpy(opt[n++],"Toggle Options...");
		strcpy(opt[n++],"Advanced Options...");
		opt[n][0]=0;
		sprintf(str,"%s Directory",dir[i]->sname);
		SETHELP(WHERE);
/*
Directory Configuration:

This menu allows you to configure the individual selected directory.
Options with a trailing ... provide a sub-menu of more options.
*/
		savnum=1;
		switch(ulist(WIN_SAV|WIN_ACT|WIN_RHT|WIN_BOT
			,0,0,60,&opt_dflt,0,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Directory Long Name:

This is a description of the file directory which is displayed in all
directory listings.
*/
				strcpy(str,dir[i]->lname);	/* save */
				if(!uinput(WIN_L2R|WIN_SAV,0,17,"Name to use for Listings"
					,dir[i]->lname,LEN_SLNAME,K_EDIT))
					strcpy(dir[i]->lname,str);
				break;
			case 1:
				SETHELP(WHERE);
/*
Directory Short Name:

This is a short description of the file directory which is displayed at
the file transfer prompt.
*/
				uinput(WIN_L2R|WIN_SAV,0,17,"Name to use for Prompts"
					,dir[i]->sname,LEN_SSNAME,K_EDIT);
				break;
			case 2:
                SETHELP(WHERE);
/*
Directory Internal Code:

Every directory must have its own unique code for Synchronet to refer to
it internally. This code should be descriptive of the directory's
contents, usually an abreviation of the directory's name.
*/
                strcpy(str,dir[i]->code);
                uinput(WIN_L2R|WIN_SAV,0,17,"Internal Code (unique)"
                    ,str,8,K_EDIT|K_UPPER);
                if(code_ok(str))
                    strcpy(dir[i]->code,str);
                else {
                    helpbuf=invalid_code;
                    umsg("Invalid Code");
                    helpbuf=0; }
                break;
			case 3:
				savnum=2;
				sprintf(str,"%s Access",dir[i]->sname);
				getar(str,dir[i]->ar);
				break;
			case 4:
				savnum=2;
				sprintf(str,"%s Upload",dir[i]->sname);
				getar(str,dir[i]->ul_ar);
				break;
			case 5:
				savnum=2;
				sprintf(str,"%s Download",dir[i]->sname);
				getar(str,dir[i]->dl_ar);
				break;
			case 6:
				savnum=2;
				sprintf(str,"%s Operator",dir[i]->sname);
				getar(str,dir[i]->op_ar);
                break;
			case 7:
				savnum=2;
				sprintf(str,"%s Exemption",dir[i]->sname);
				getar(str,dir[i]->ex_ar);
                break;
			case 8:
				SETHELP(WHERE);
/*
File Path:

This is the default storage path for files uploaded to this directory.
If this path is blank, files are stored in a directory off of the
DATA\DIRS directory using the internal code of this directory as the
name of the dirdirectory (i.e. DATA\DIRS\<CODE>).

This path can be overridden on a per file basis using Alternate File
Paths.
*/
				uinput(WIN_L2R|WIN_SAV,0,17,"File Path"
					,dir[i]->path,50,K_EDIT|K_UPPER);
				break;
			case 9:
				SETHELP(WHERE);
/*
Maximum Number of Files:

This value is the maximum number of files allowed in this directory.
*/
				sprintf(str,"%u",dir[i]->maxfiles);
				uinput(WIN_L2R|WIN_SAV,0,17,"Maximum Number of Files"
					,str,5,K_EDIT|K_NUMBER);
				n=atoi(str);
				if(n>MAX_FILES) {
					sprintf(str,"Maximum Files is %u",MAX_FILES);
					umsg(str); }
				else
					dir[i]->maxfiles=n;
				break;
			case 10:
				sprintf(str,"%u",dir[i]->maxage);
                SETHELP(WHERE);
/*
Maximum Age of Files:

This value is the maximum number of days that files will be kept in
the directory based on the date the file was uploaded or last
downloaded (If the Purge by Last Download toggle option is used).

The Synchronet file base maintenance program (DELFILES) must be used
to automatically remove files based on age.
*/
				uinput(WIN_MID|WIN_SAV,0,17,"Maximum Age of Files (in days)"
                    ,str,5,K_EDIT|K_NUMBER);
				dir[i]->maxage=atoi(str);
                break;
			case 11:
SETHELP(WHERE);
/*
Percentage of Credits to Credit Uploader on Upload:

This is the percentage of a file's credit value that is given to users
when they upload files. Most often, this value will be set to 100 to
give full credit value (100%) for uploads.

If you want uploaders to receive no credits upon upload, set this value
to 0.
*/
				uinput(WIN_MID|WIN_SAV,0,0
					,"Percentage of Credits to Credit Uploader on Upload"
					,itoa(dir[i]->up_pct,tmp,10),4,K_EDIT|K_NUMBER);
				dir[i]->up_pct=atoi(tmp);
				break;
			case 12:
SETHELP(WHERE);
/*
Percentage of Credits to Credit Uploader on Download:

This is the percentage of a file's credit value that is given to users
who upload a file that is later downloaded by another user. This is an
award type system where more popular files will generate more credits
for the uploader.

If you do not want uploaders to receive credit when files they upload
are later downloaded, set this value to 0.
*/
				uinput(WIN_MID|WIN_SAV,0,0
					,"Percentage of Credits to Credit Uploader on Download"
					,itoa(dir[i]->dn_pct,tmp,10),4,K_EDIT|K_NUMBER);
				dir[i]->dn_pct=atoi(tmp);
				break;
			case 13:
				while(1) {
					n=0;
					sprintf(opt[n++],"%-27.27s%s","Check for File Existence"
						,dir[i]->misc&DIR_FCHK ? "Yes":"No");
					strcpy(str,"Slow Media Device");
					if(dir[i]->seqdev) {
						sprintf(tmp," #%u",dir[i]->seqdev);
						strcat(str,tmp); }
					sprintf(opt[n++],"%-27.27s%s",str
						,dir[i]->seqdev ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Force Content Ratings"
						,dir[i]->misc&DIR_RATE ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Upload Date in Listings"
						,dir[i]->misc&DIR_ULDATE ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Multiple File Numberings"
						,dir[i]->misc&DIR_MULT ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Search for Duplicates"
						,dir[i]->misc&DIR_DUPES ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Search for New Files"
						,dir[i]->misc&DIR_NOSCAN ? "No":"Yes");
					sprintf(opt[n++],"%-27.27s%s","Search for Auto-ADDFILES"
						,dir[i]->misc&DIR_NOAUTO ? "No":"Yes");
					sprintf(opt[n++],"%-27.27s%s","Import FILE_ID.DIZ"
						,dir[i]->misc&DIR_DIZ ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Free Downloads"
						,dir[i]->misc&DIR_FREE ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Free Download Time"
						,dir[i]->misc&DIR_TFREE ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Deduct Upload Time"
						,dir[i]->misc&DIR_ULTIME ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Credit Uploads"
						,dir[i]->misc&DIR_CDTUL ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Credit Downloads"
						,dir[i]->misc&DIR_CDTDL ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Credit with Minutes"
						,dir[i]->misc&DIR_CDTMIN ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Anonymous Uploads"
						,dir[i]->misc&DIR_ANON ? dir[i]->misc&DIR_AONLY
						? "Only":"Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Purge by Last Download"
						,dir[i]->misc&DIR_SINCEDL ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Mark Moved Files as New"
						,dir[i]->misc&DIR_MOVENEW ? "Yes":"No");
					opt[n][0]=0;
					savnum=2;
					SETHELP(WHERE);
/*
Directory Toggle Options:

This is the toggle options menu for the selected file directory.

The available options from this menu can all be toggled between two or
more states, such as Yes and No.
*/
					n=ulist(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,3,2,36,&tog_dflt
						,&tog_bar,"Toggle Options",opt);
					if(n==-1)
                        break;
					savnum=3;
					switch(n) {
						case 0:
							n=0;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Check for File Existence When Listing:

If you want the actual existence of files to be verified while listing
directories, set this value to Yes.

Directories with files located on CD-ROM or other slow media should have
this option set to No.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Check for File Existence When Listing",opt);
							if(n==0 && !(dir[i]->misc&DIR_FCHK)) {
								dir[i]->misc|=DIR_FCHK;
								changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_FCHK)) {
								dir[i]->misc&=~DIR_FCHK;
								changes=1; }
							break;
						case 1:
                            n=0;
                            strcpy(opt[0],"Yes");
                            strcpy(opt[1],"No");
                            opt[2][0]=0;
							SETHELP(WHERE);
/*
Slow Media Device:

If this directory contains files located on CD-ROM or other slow media
device, you should set this option to Yes. Each slow media device on
your system should have a unique Device Number. If you only have one
slow media device, then this number should be set to 1.

CD-ROM multidisk changers are considered one device and all the
directories on all the CD-ROMs in each changer should be set to the same
device number.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Slow Media Device"
								,opt);
							if(n==0) {
								if(!dir[i]->seqdev) {
									changes=1;
									strcpy(str,"1"); }
								else
									sprintf(str,"%u",dir[i]->seqdev);
								uinput(WIN_MID|WIN_SAV,0,0
									,"Device Number"
									,str,2,K_EDIT|K_UPPER);
								dir[i]->seqdev=atoi(str); }
							else if(n==1 && dir[i]->seqdev) {
								dir[i]->seqdev=0;
                                changes=1; }
                            break;
						case 2:
							n=1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Force Content Ratings in Descriptions:

If you would like all uploads to this directory to be prompted for
content rating (G, R, or X), set this value to Yes.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Force Content Ratings in Descriptions",opt);
							if(n==0 && !(dir[i]->misc&DIR_RATE)) {
								dir[i]->misc|=DIR_RATE;
								changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_RATE)) {
								dir[i]->misc&=~DIR_RATE;
								changes=1; }
							break;
						case 3:
							n=1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Include Upload Date in File Descriptions:

If you wish the upload date of each file in this directory to be
automatically included in the file description, set this option to
Yes.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Include Upload Date in Descriptions",opt);
							if(n==0 && !(dir[i]->misc&DIR_ULDATE)) {
								dir[i]->misc|=DIR_ULDATE;
								changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_ULDATE)) {
								dir[i]->misc&=~DIR_ULDATE;
								changes=1; }
                            break;
						case 4:
							n=0;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Ask for Multiple File Numberings:

If you would like uploads to this directory to be prompted for multiple
file (disk) numbers, set this value to Yes.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Ask for Multiple File Numberings",opt);
							if(n==0 && !(dir[i]->misc&DIR_MULT)) {
								dir[i]->misc|=DIR_MULT;
								changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_MULT)) {
								dir[i]->misc&=~DIR_MULT;
								changes=1; }
							break;
						case 5:
							n=0;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Search Directory for Duplicate Filenames:

If you would like to have this directory searched for duplicate
filenames when a user attempts to upload a file, set this option to Yes.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Search for Duplicate Filenames",opt);
							if(n==0 && !(dir[i]->misc&DIR_DUPES)) {
								dir[i]->misc|=DIR_DUPES;
								changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_DUPES)) {
								dir[i]->misc&=~DIR_DUPES;
								changes=1; }
							break;
						case 6:
							n=0;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Search Directory for New Files:

If you would like to have this directory searched for newly uploaded
files when a user scans All libraries for new files, set this option to
Yes.

If this directory is located on CD-ROM or other read only media
(where uploads are unlikely to occur), it will improve new file scans
if this option is set to No.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Search for New files",opt);
							if(n==0 && dir[i]->misc&DIR_NOSCAN) {
								dir[i]->misc&=~DIR_NOSCAN;
								changes=1; }
							else if(n==1 && !(dir[i]->misc&DIR_NOSCAN)) {
								dir[i]->misc|=DIR_NOSCAN;
								changes=1; }
                            break;
						case 7:
							n=0;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Search Directory for Auto-ADDFILES:

If you would like to have this directory searched for a file list to
import automatically when using the ADDFILES * (Auto-ADD) feature,
set this option to Yes.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Search for Auto-ADDFILES",opt);
							if(n==0 && dir[i]->misc&DIR_NOAUTO) {
								dir[i]->misc&=~DIR_NOAUTO;
								changes=1; }
							else if(n==1 && !(dir[i]->misc&DIR_NOAUTO)) {
								dir[i]->misc|=DIR_NOAUTO;
								changes=1; }
                            break;
						case 8:
							n=0;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Import FILE_ID.DIZ and DESC.SDI Descriptions:

If you would like archived descriptions (FILE_ID.DIZ and DESC.SDI)
of uploaded files to be automatically imported as the extended
description, set this option to Yes.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Import FILE_ID.DIZ and DESC.SDI",opt);
							if(n==0 && !(dir[i]->misc&DIR_DIZ)) {
								dir[i]->misc|=DIR_DIZ;
								changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_DIZ)) {
								dir[i]->misc&=~DIR_DIZ;
								changes=1; }
                            break;
						case 9:
							n=1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Downloads are Free:

If you would like all downloads from this directory to be free (cost
no credits), set this option to Yes.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Downloads are Free",opt);
							if(n==0 && !(dir[i]->misc&DIR_FREE)) {
								dir[i]->misc|=DIR_FREE;
								changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_FREE)) {
								dir[i]->misc&=~DIR_FREE;
								changes=1; }
                            break;
						case 10:
							n=1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Free Download Time:

If you would like all downloads from this directory to not subtract
time from the user, set this option to Yes.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Free Download Time",opt);
							if(n==0 && !(dir[i]->misc&DIR_TFREE)) {
								dir[i]->misc|=DIR_TFREE;
								changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_TFREE)) {
								dir[i]->misc&=~DIR_TFREE;
								changes=1; }
                            break;
						case 11:
							n=1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Deduct Upload Time:

If you would like all uploads to this directory to have the time spent
uploading subtracted from their time online, set this option to Yes.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Deduct Upload Time",opt);
							if(n==0 && !(dir[i]->misc&DIR_ULTIME)) {
								dir[i]->misc|=DIR_ULTIME;
								changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_ULTIME)) {
								dir[i]->misc&=~DIR_ULTIME;
								changes=1; }
                            break;
						case 12:
							n=0;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Give Credit for Uploads:

If you want users who upload to this directory to get credit for their
initial upload, set this option to Yes.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Give Credit for Uploads",opt);
							if(n==0 && !(dir[i]->misc&DIR_CDTUL)) {
								dir[i]->misc|=DIR_CDTUL;
								changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_CDTUL)) {
								dir[i]->misc&=~DIR_CDTUL;
								changes=1; }
                            break;
						case 13:
							n=0;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Give Uploader Credit for Downloads:

If you want users who upload to this directory to get credit when their
files are downloaded, set this optin to Yes.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Give Uploader Credit for Downloads",opt);
							if(n==0 && !(dir[i]->misc&DIR_CDTDL)) {
								dir[i]->misc|=DIR_CDTDL;
								changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_CDTDL)) {
								dir[i]->misc&=~DIR_CDTDL;
								changes=1; }
                            break;
						case 14:
							n=1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Credit Uploader with Minutes instead of Credits:

If you wish to give the uploader of files to this directory minutes,
intead of credits, set this option to Yes.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Credit Uploader with Minutes",opt);
							if(n==0 && !(dir[i]->misc&DIR_CDTMIN)) {
								dir[i]->misc|=DIR_CDTMIN;
								changes=1; }
							else if(n==1 && dir[i]->misc&DIR_CDTMIN){
								dir[i]->misc&=~DIR_CDTMIN;
								changes=1; }
                            break;

						case 15:
							n=1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							strcpy(opt[2],"Only");
							opt[3][0]=0;
							SETHELP(WHERE);
/*
Allow Anonymous Uploads:

If you want users with the A exemption to be able to upload anonymously
to this directory, set this option to Yes. If you want all uploads to
this directory to be forced anonymous, set this option to Only.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Allow Anonymous Uploads",opt);
							if(n==0 && (dir[i]->misc&(DIR_ANON|DIR_AONLY))
								!=DIR_ANON) {
								dir[i]->misc|=DIR_ANON;
								dir[i]->misc&=~DIR_AONLY;
								changes=1; }
							else if(n==1 && dir[i]->misc&(DIR_ANON|DIR_AONLY)){
								dir[i]->misc&=~(DIR_ANON|DIR_AONLY);
								changes=1; }
							else if(n==2 && (dir[i]->misc&(DIR_ANON|DIR_AONLY))
								!=(DIR_ANON|DIR_AONLY)) {
								dir[i]->misc|=(DIR_ANON|DIR_AONLY);
								changes=1; }
							break;
						case 16:
                            n=0;
                            strcpy(opt[0],"Yes");
                            strcpy(opt[1],"No");
                            opt[2][0]=0;
                            SETHELP(WHERE);
/*
Purge Files Based on Date of Last Download:

Using the Synchronet file base maintenance utility (DELFILES), you can
have files removed based on the number of days since last downloaded
rather than the number of days since the file was uploaded (default),
by setting this option to Yes.
*/
                            n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Purge Files Based on Date of Last Download"
                                ,opt);
							if(n==0 && !(dir[i]->misc&DIR_SINCEDL)) {
								dir[i]->misc|=DIR_SINCEDL;
                                changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_SINCEDL)) {
								dir[i]->misc&=~DIR_SINCEDL;
                                changes=1; }
                            break;
						case 17:
                            n=0;
                            strcpy(opt[0],"Yes");
                            strcpy(opt[1],"No");
                            opt[2][0]=0;
                            SETHELP(WHERE);
/*
Mark Moved Files as New:

If this option is set to Yes, then all files moved from this directory
will have their upload date changed to the current date so the file will
appear in users' new-file scans again.
*/
                            n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Mark Moved Files as New"
                                ,opt);
							if(n==0 && !(dir[i]->misc&DIR_MOVENEW)) {
								dir[i]->misc|=DIR_MOVENEW;
                                changes=1; }
							else if(n==1 && (dir[i]->misc&DIR_MOVENEW)) {
								dir[i]->misc&=~DIR_MOVENEW;
                                changes=1; }
                            break;
							} }
				break;
		case 14:
			while(1) {
				n=0;
				sprintf(opt[n++],"%-27.27s%s","Extensions Allowed"
					,dir[i]->exts);
				if(!dir[i]->data_dir[0])
					sprintf(str,"%sDIRS\\",data_dir);
				else
					strcpy(str,dir[i]->data_dir);
				sprintf(opt[n++],"%-27.27s%.40s","Data Directory"
					,str);
				sprintf(opt[n++],"%-27.27s%.40s","Upload Semaphore File"
                    ,dir[i]->upload_sem);
				sprintf(opt[n++],"%-27.27s%s","Sort Value and Direction"
					, dir[i]->sort==SORT_NAME_A ? "Name Ascending"
					: dir[i]->sort==SORT_NAME_D ? "Name Descending"
					: dir[i]->sort==SORT_DATE_A ? "Date Ascending"
					: "Date Descending");
				opt[n][0]=0;
				savnum=2;
				SETHELP(WHERE);
/*
Directory Advanced Options:

This is the advanced options menu for the selected file directory.
*/
					n=ulist(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,3,4,60,&adv_dflt,0
						,"Advanced Options",opt);
					if(n==-1)
                        break;
					savnum=3;
                    switch(n) {
						case 0:
							SETHELP(WHERE);
/*
File Extensions Allowed:

This option allows you to limit the types of files uploaded to this
directory. This is a list of file extensions that are allowed, each
separated by a comma (Example: ZIP,EXE). If this option is left
blank, all file extensions will be allowed to be uploaded.
*/
							uinput(WIN_L2R|WIN_SAV,0,17
								,"File Extensions Allowed"
								,dir[i]->exts,40,K_EDIT|K_UPPER);
							break;
						case 1:
SETHELP(WHERE);
/*
Data Directory:

Use this if you wish to place the data directory for this directory
on another drive or in another directory besides the default setting.
*/
							uinput(WIN_MID|WIN_SAV,0,17,"Data"
								,dir[i]->data_dir,50,K_EDIT|K_UPPER);
							break;
						case 2:
SETHELP(WHERE);
/*
Upload Semaphore File:

This is a filename that will be used as a semaphore (signal) to your
FidoNet front-end that new files are ready to be hatched for export.
*/
							uinput(WIN_MID|WIN_SAV,0,17,"Upload Semaphore"
								,dir[i]->upload_sem,50,K_EDIT|K_UPPER);
							break;
						case 3:
							n=0;
							strcpy(opt[0],"Name Ascending");
							strcpy(opt[1],"Name Descending");
							strcpy(opt[2],"Date Ascending");
							strcpy(opt[3],"Date Descending");
							opt[4][0]=0;
							SETHELP(WHERE);
/*
Sort Value and Direction:

This option allows you to determine the sort value and direction. Files
that are uploaded are automatically sorted by filename or upload date,
ascending or descending. If you change the sort value or direction after
a directory already has files in it, use the sysop transfer menu ;RESORT
command to resort the directory with the new sort parameters.
*/
							n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0
								,"Sort Value and Direction",opt);
							if(n==0 && dir[i]->sort!=SORT_NAME_A) {
								dir[i]->sort=SORT_NAME_A;
								changes=1; }
							else if(n==1 && dir[i]->sort!=SORT_NAME_D) {
								dir[i]->sort=SORT_NAME_D;
								changes=1; }
							else if(n==2 && dir[i]->sort!=SORT_DATE_A) {
								dir[i]->sort=SORT_DATE_A;
								changes=1; }
							else if(n==3 && dir[i]->sort!=SORT_DATE_D) {
								dir[i]->sort=SORT_DATE_D;
								changes=1; }
							break; } }
			break;
			} } }

}
