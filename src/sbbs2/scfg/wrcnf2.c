#line 2 "WRCNF2.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "scfg.h"

extern int no_msghdr,all_msghdr;
extern int no_dirchk;

extern char *crlf;
extern char nulbuf[];
extern int	pslen;

#define put_int(var,stream) fwrite(&var,1,sizeof(var),stream)
#define put_str(var,stream) { pslen=strlen(var); \
							fwrite(var,1,pslen > sizeof(var) \
								? sizeof(var) : pslen ,stream); \
							fwrite(nulbuf,1,pslen > sizeof(var) \
                                ? 0 : sizeof(var)-pslen,stream); }

void backup(char *org)
{
	char old[128],new[128];
	int i,x;

x=strlen(org)-1;
if(x<=0)
	return;
strcpy(old,org);
strcpy(new,org);
for(i=backup_level;i;i--) {
	new[x]=(i-1)+'0';
	if(i==backup_level)
		remove(new);
	if(i==1) {
		rename(org,new);
		continue; }
	old[x]=(i-2)+'0';
	rename(old,new); }
}


void write_file_cfg()
{
	char	str[128],cmd[LEN_CMD+1],c;
	int 	i,j,k,file;
	short	n;
	long	l=0;
	FILE	*stream;

upop("Writing FILE.CNF...");
sprintf(str,"%sFILE.CNF",ctrl_dir);
backup(str);

if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
	|| (stream=fdopen(file,"wb"))==NULL) {
	errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
    return; }
setvbuf(stream,NULL,_IOFBF,2048);

put_int(min_dspace,stream);
put_int(max_batup,stream);
put_int(max_batdn,stream);
put_int(max_userxfer,stream);
put_int(l,stream);					/* unused */
put_int(cdt_up_pct,stream);
put_int(cdt_dn_pct,stream);
put_int(l,stream);					/* unused */
put_str(cmd,stream);
put_int(leech_pct,stream);
put_int(leech_sec,stream);
n=0;
for(i=0;i<32;i++)
	put_int(n,stream);

/* Extractable File Types */

put_int(total_fextrs,stream);
for(i=0;i<total_fextrs;i++) {
	put_str(fextr[i]->ext,stream);
	put_str(fextr[i]->cmd,stream);
	put_str(fextr[i]->ar,stream);
	n=0;
	for(j=0;j<8;j++)
		put_int(n,stream); }

/* Compressable File Types */

put_int(total_fcomps,stream);
for(i=0;i<total_fcomps;i++) {
	put_str(fcomp[i]->ext,stream);
	put_str(fcomp[i]->cmd,stream);
	put_str(fcomp[i]->ar,stream);
	n=0;
	for(j=0;j<8;j++)
        put_int(n,stream); }

/* Viewable File Types */

put_int(total_fviews,stream);
for(i=0;i<total_fviews;i++) {
	put_str(fview[i]->ext,stream);
	put_str(fview[i]->cmd,stream);
	put_str(fview[i]->ar,stream);
	n=0;
	for(j=0;j<8;j++)
        put_int(n,stream); }

/* Testable File Types */

put_int(total_ftests,stream);
for(i=0;i<total_ftests;i++) {
	put_str(ftest[i]->ext,stream);
	put_str(ftest[i]->cmd,stream);
	put_str(ftest[i]->workstr,stream);
	put_str(ftest[i]->ar,stream);
	n=0;
	for(j=0;j<8;j++)
        put_int(n,stream); }

/* Download Events */

put_int(total_dlevents,stream);
for(i=0;i<total_dlevents;i++) {
	put_str(dlevent[i]->ext,stream);
	put_str(dlevent[i]->cmd,stream);
	put_str(dlevent[i]->workstr,stream);
	put_str(dlevent[i]->ar,stream);
	n=0;
	for(j=0;j<8;j++)
        put_int(n,stream); }

/* File Transfer Protocols */

put_int(total_prots,stream);
for(i=0;i<total_prots;i++) {
	put_int(prot[i]->mnemonic,stream);
	put_str(prot[i]->name,stream);
	put_str(prot[i]->ulcmd,stream);
	put_str(prot[i]->dlcmd,stream);
	put_str(prot[i]->batulcmd,stream);
	put_str(prot[i]->batdlcmd,stream);
	put_str(prot[i]->blindcmd,stream);
	put_str(prot[i]->bicmd,stream);
	put_int(prot[i]->misc,stream);
	put_str(prot[i]->ar,stream);
	n=0;
	for(j=0;j<8;j++)
		put_int(n,stream); }

/* Alternate File Paths */

put_int(altpaths,stream);
for(i=0;i<altpaths;i++) {
	backslash(altpath[i]);
	fwrite(altpath[i],LEN_DIR+1,1,stream);
	n=0;
	for(j=0;j<8;j++)
		put_int(n,stream); }

/* File Libraries */

put_int(total_libs,stream);
for(i=0;i<total_libs;i++) {
	put_str(lib[i]->lname,stream);
	put_str(lib[i]->sname,stream);
	put_str(lib[i]->ar,stream);
	n=0;
	for(j=0;j<32;j++)
		put_int(n,stream);
	n=0xffff;
	for(j=0;j<16;j++)
		put_int(n,stream); }

/* File Directories */

put_int(total_dirs,stream);
for(j=0;j<total_libs;j++)
    for(i=0;i<total_dirs;i++)
        if(dir[i]->lib==j) {
			put_int(dir[i]->lib,stream);
			put_str(dir[i]->lname,stream);
			put_str(dir[i]->sname,stream);
			put_str(dir[i]->code,stream);
			if(dir[i]->data_dir[0])
				backslash(dir[i]->data_dir);
			md(dir[i]->data_dir);
			put_str(dir[i]->data_dir,stream);
			put_str(dir[i]->ar,stream);
			put_str(dir[i]->ul_ar,stream);
			put_str(dir[i]->dl_ar,stream);
			put_str(dir[i]->op_ar,stream);
			backslash(dir[i]->path);
			put_str(dir[i]->path,stream);
			if(dir[i]->misc&DIR_FCHK) {
				strcpy(str,dir[i]->path);
				md(str); }

			put_str(dir[i]->upload_sem,stream);
			put_int(dir[i]->maxfiles,stream);
			put_str(dir[i]->exts,stream);
			put_int(dir[i]->misc,stream);
			put_int(dir[i]->seqdev,stream);
			put_int(dir[i]->sort,stream);
			put_str(dir[i]->ex_ar,stream);
			put_int(dir[i]->maxage,stream);
			put_int(dir[i]->up_pct,stream);
			put_int(dir[i]->dn_pct,stream);
			c=0;
			put_int(c,stream);
			n=0;
			for(k=0;k<8;k++)
				put_int(n,stream);
			n=0xffff;
			for(k=0;k<16;k++)
				put_int(n,stream);
			}

/* Text File Sections */

put_int(total_txtsecs,stream);
for(i=0;i<total_txtsecs;i++) {
	sprintf(str,"%sTEXT\\%s",data_dir,txtsec[i]->code);
	md(str);
	put_str(txtsec[i]->name,stream);
	put_str(txtsec[i]->code,stream);
	put_str(txtsec[i]->ar,stream);
	n=0;
	for(j=0;j<8;j++)
		put_int(n,stream); }

fclose(stream);
}

void free_file_cfg()
{
	int i;

for(i=0;i<total_fextrs;i++)
	FREE(fextr[i]);
FREE(fextr);

for(i=0;i<total_fcomps;i++)
	FREE(fcomp[i]);
FREE(fcomp);

for(i=0;i<total_fviews;i++)
	FREE(fview[i]);
FREE(fview);

for(i=0;i<total_ftests;i++)
	FREE(ftest[i]);
FREE(ftest);

for(i=0;i<total_dlevents;i++)
	FREE(dlevent[i]);
FREE(dlevent);

for(i=0;i<total_prots;i++)
	FREE(prot[i]);
FREE(prot);

for(i=0;i<altpaths;i++)
	FREE(altpath[i]);
FREE(altpath);

for(i=0;i<total_libs;i++)
	FREE(lib[i]);
FREE(lib);

for(i=0;i<total_dirs;i++)
	FREE(dir[i]);
FREE(dir);

for(i=0;i<total_txtsecs;i++)
	FREE(txtsec[i]);
FREE(txtsec);
}


void write_chat_cfg()
{
	char	str[128];
	int 	i,j,file;
	short	n;
	FILE	*stream;

upop("Writing CHAT.CNF...");
sprintf(str,"%sCHAT.CNF",ctrl_dir);
backup(str);

if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
	|| (stream=fdopen(file,"wb"))==NULL) {
	errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
    return; }
setvbuf(stream,NULL,_IOFBF,2048);

put_int(total_gurus,stream);
for(i=0;i<total_gurus;i++) {
    put_str(guru[i]->name,stream);
    put_str(guru[i]->code,stream);
    put_str(guru[i]->ar,stream);
    n=0;
    for(j=0;j<8;j++)
        put_int(n,stream);
    }

put_int(total_actsets,stream);
for(i=0;i<total_actsets;i++)
	put_str(actset[i]->name,stream);

put_int(total_chatacts,stream);
for(i=0;i<total_chatacts;i++) {
	put_int(chatact[i]->actset,stream);
	put_str(chatact[i]->cmd,stream);
	put_str(chatact[i]->out,stream);
	n=0;
	for(j=0;j<8;j++)
		put_int(n,stream);
	}

put_int(total_chans,stream);
for(i=0;i<total_chans;i++) {
	put_int(chan[i]->actset,stream);
	put_str(chan[i]->name,stream);
	put_str(chan[i]->code,stream);
	put_str(chan[i]->ar,stream);
	put_int(chan[i]->cost,stream);
	put_int(chan[i]->guru,stream);
	put_int(chan[i]->misc,stream);
	n=0;
	for(j=0;j<8;j++)
		put_int(n,stream);
	}

put_int(total_pages,stream);
for(i=0;i<total_pages;i++) {
    put_int(page[i]->cmd,stream);
    put_int(page[i]->ar,stream);
    put_int(page[i]->misc,stream);
    n=0;
    for(j=0;j<8;j++)
        put_int(n,stream);
    }

fclose(stream);
}

void free_chat_cfg()
{
	int i;

for(i=0;i<total_actsets;i++)
	FREE(actset[i]);
FREE(actset);

for(i=0;i<total_chatacts;i++)
	FREE(chatact[i]);
FREE(chatact);

for(i=0;i<total_chans;i++)
	FREE(chan[i]);
FREE(chan);

for(i=0;i<total_gurus;i++)
	FREE(guru[i]);
FREE(guru);

for(i=0;i<total_pages;i++)
	FREE(page[i]);
FREE(page);

}

void write_xtrn_cfg()
{
	uchar	str[128],c;
	int 	i,j,sec,file;
	short	n;
	long	l;
	FILE	*stream;

upop("Writing XTRN.CNF...");
sprintf(str,"%sXTRN.CNF",ctrl_dir);
backup(str);

if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
	|| (stream=fdopen(file,"wb"))==NULL) {
	errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
    return; }
setvbuf(stream,NULL,_IOFBF,2048);

put_int(total_swaps,stream);
for(i=0;i<total_swaps;i++)
	put_str(swap[i]->cmd,stream);

put_int(total_xedits,stream);
for(i=0;i<total_xedits;i++) {
	put_str(xedit[i]->name,stream);
	put_str(xedit[i]->code,stream);
	put_str(xedit[i]->lcmd,stream);
	put_str(xedit[i]->rcmd,stream);
	put_int(xedit[i]->misc,stream);
	put_str(xedit[i]->ar,stream);
	put_int(xedit[i]->type,stream);
	c=0;
	put_int(c,stream);
	n=0;
	for(j=0;j<7;j++)
		put_int(n,stream);
	}

put_int(total_xtrnsecs,stream);
for(i=0;i<total_xtrnsecs;i++) {
	put_str(xtrnsec[i]->name,stream);
	put_str(xtrnsec[i]->code,stream);
	put_str(xtrnsec[i]->ar,stream);
	n=0;
	for(j=0;j<8;j++)
		put_int(n,stream);
	}

put_int(total_xtrns,stream);
for(sec=0;sec<total_xtrnsecs;sec++)
	for(i=0;i<total_xtrns;i++) {
		if(xtrn[i]->sec!=sec)
			continue;
		put_int(xtrn[i]->sec,stream);
		put_str(xtrn[i]->name,stream);
		put_str(xtrn[i]->code,stream);
		put_str(xtrn[i]->ar,stream);
		put_str(xtrn[i]->run_ar,stream);
		put_int(xtrn[i]->type,stream);
		put_int(xtrn[i]->misc,stream);
		put_int(xtrn[i]->event,stream);
		put_int(xtrn[i]->cost,stream);
		put_str(xtrn[i]->cmd,stream);
		put_str(xtrn[i]->clean,stream);
		put_str(xtrn[i]->path,stream);
		put_int(xtrn[i]->textra,stream);
		put_int(xtrn[i]->maxtime,stream);
		n=0;
		for(j=0;j<7;j++)
			put_int(n,stream);
		}

put_int(total_events,stream);
for(i=0;i<total_events;i++) {
	put_str(event[i]->code,stream);
	put_str(event[i]->cmd,stream);
	put_int(event[i]->days,stream);
	put_int(event[i]->time,stream);
	put_int(event[i]->node,stream);
	put_int(event[i]->misc,stream);
	put_int(event[i]->dir,stream);
	n=0;
	for(j=0;j<8;j++)
		put_int(n,stream);
	}

put_int(total_os2pgms,stream);
for(i=0;i<total_os2pgms;i++)
	put_str(os2pgm[i]->name,stream);
for(i=0;i<total_os2pgms;i++)
	put_int(os2pgm[i]->misc,stream);

fclose(stream);
}

void free_xtrn_cfg()
{
	int i;

for(i=0;i<total_swaps;i++)
	FREE(swap[i]);
FREE(swap);

for(i=0;i<total_xedits;i++)
	FREE(xedit[i]);
FREE(xedit);

for(i=0;i<total_xtrnsecs;i++)
	FREE(xtrnsec[i]);
FREE(xtrnsec);

for(i=0;i<total_xtrns;i++)
	FREE(xtrn[i]);
FREE(xtrn);

for(i=0;i<total_events;i++)
	FREE(event[i]);
FREE(event);

}
