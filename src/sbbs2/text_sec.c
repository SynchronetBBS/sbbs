#line 1 "TEXT_SEC.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

#define MAX_TXTSECS 	500 /* Maximum number of text file sections 	*/
#define MAX_TXTFILES    500 /* Maximum number of text files per section */

/****************************************************************************/
/* General Text File Section.                                               */
/* Called from function main_sec                                            */
/* Returns 1 if no text sections available, 0 otherwise.                    */
/****************************************************************************/
char text_sec()
{
	char	str[256],usrsec[MAX_TXTSECS],usrsecs,cursec,usemenu
			,*file[MAX_TXTFILES],addpath[83],addstr[83],*buf,ch;
	long	i,j;
    long    l,length;
    FILE    *stream;

// ch=getage(useron.birth); Removed 05/19/96
for(i=j=0;i<total_txtsecs;i++) {
    if(!chk_ar(txtsec[i]->ar,useron))
        continue;
    usrsec[j++]=i; }
usrsecs=j;
if(!usrsecs) {
    bputs(text[NoTextSections]);
    return(1); }
action=NODE_RTXT;
while(online) {
	sprintf(str,"%sMENU\\TEXT_SEC.*",text_dir);
    if(fexist(str))
        menu("TEXT_SEC");
    else {
        bputs(text[TextSectionLstHdr]);
        for(i=0;i<usrsecs && !msgabort();i++) {
            sprintf(str,text[TextSectionLstFmt],i+1,txtsec[usrsec[i]]->name);
            if(i<9) outchar(SP);
            bputs(str); } }
    ASYNC;
    mnemonics(text[WhichTextSection]);
    if((cursec=getnum(usrsecs))<1)
        break;
    cursec--;
    while(online) {
		sprintf(str,"%sMENU\\TEXT%u.*",text_dir,cursec+1);
        if(fexist(str)) {
            sprintf(str,"TEXT%u",cursec+1);
            menu(str);
            usemenu=1; }
        else {
            bprintf(text[TextFilesLstHdr],txtsec[usrsec[cursec]]->name);
            usemenu=0; }
        sprintf(str,"%sTEXT\\%s.IXT",data_dir,txtsec[usrsec[cursec]]->code);
        j=0;
        if(fexist(str)) {
			if((stream=fnopen((int *)&i,str,O_RDONLY))==NULL) {
                errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
                return(0); }
            while(!ferror(stream) && !msgabort()) {  /* file open too long */
                if(!fgets(str,81,stream))
                    break;
                str[strlen(str)-2]=0;   /* chop off CRLF */
                if((file[j]=MALLOC(strlen(str)+1))==NULL) {
                    errormsg(WHERE,ERR_ALLOC,nulstr,strlen(str)+1);
                    continue; }
                strcpy(file[j],str);
                fgets(str,81,stream);
                if(!usemenu) bprintf(text[TextFilesLstFmt],j+1,str);
                j++; }
            fclose(stream); }
        ASYNC;
        if(SYSOP) {
            strcpy(str,"QARE?");
            mnemonics(text[WhichTextFileSysop]); }
        else {
            strcpy(str,"Q?");
            mnemonics(text[WhichTextFile]); }
        i=getkeys(str,j);
		if(!(i&0x80000000L)) {		  /* no file number */
            for(l=0;l<j;l++)
                FREE(file[l]);
            if((i=='E' || i=='R') && !j)
                continue; }
        if(i=='Q' || !i)
            break;
        if(i==-1) {  /* ctrl-c */
            for(i=0;i<j;i++)
                FREE(file[i]);
            return(0); }
        if(i=='?')  /* ? means re-list */
            continue;
        if(i=='A') {    /* Add text file */
            if(j) {
                bputs(text[AddTextFileBeforeWhich]);
                i=getnum(j+1);
                if(i<1)
                    continue;
                i--;    /* number of file entries to skip */ }
            else
                i=0;
            bprintf(text[AddTextFilePath]
                ,data_dir,txtsec[usrsec[cursec]]->code);
            if(!getstr(addpath,80,K_UPPER))
                continue;
            strcat(addpath,crlf);
            bputs(text[AddTextFileDesc]);
            if(!getstr(addstr,74,0))
                continue;
            strcat(addstr,crlf);
            sprintf(str,"%sTEXT\\%s.IXT"
                ,data_dir,txtsec[usrsec[cursec]]->code);
            if(i==j) {  /* just add to end */
                if((i=nopen(str,O_WRONLY|O_APPEND|O_CREAT))==-1) {
                    errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_APPEND|O_CREAT);
                    return(0); }
                write(i,addpath,strlen(addpath));
                write(i,addstr,strlen(addstr));
                close(i);
                continue; }
            j=i; /* inserting in middle of file */
			if((stream=fnopen((int *)&i,str,O_RDWR))==NULL) {
                errormsg(WHERE,ERR_OPEN,str,O_RDWR);
                return(0); }
            length=filelength(i);
            for(i=0;i<j;i++) {  /* skip two lines for each entry */
                fgets(tmp,81,stream);
                fgets(tmp,81,stream); }
            l=ftell(stream);
            if((buf=(char *)MALLOC(length-l))==NULL) {
                fclose(stream);
                errormsg(WHERE,ERR_ALLOC,str,length-l);
                return(0); }
            fread(buf,1,length-l,stream);
            fseek(stream,l,SEEK_SET); /* go back to where we need to insert */
            fputs(addpath,stream);
            fputs(addstr,stream);
            fwrite(buf,1,length-l,stream);
            fclose(stream);
            FREE(buf);
            continue; }
        if(i=='R' || i=='E') {   /* Remove or Edit text file */
            ch=i;
            if(ch=='R')
                bputs(text[RemoveWhichTextFile]);
            else
                bputs(text[EditWhichTextFile]);
            i=getnum(j);
            if(i<1)
                continue;
            sprintf(str,"%sTEXT\\%s.IXT"
                ,data_dir,txtsec[usrsec[cursec]]->code);
            j=i-1;
			if((stream=fnopen((int *)&i,str,O_RDONLY))==NULL) {
                errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
                return(0); }
			// length=filelength(i); Removed 05/19/96
            for(i=0;i<j;i++) {  /* skip two lines for each entry */
                fgets(tmp,81,stream);
                fgets(tmp,81,stream); }
            fgets(addpath,81,stream);
            truncsp(addpath);
            fclose(stream);
            if(!strchr(addpath,'\\'))
                sprintf(tmp,"%sTEXT\\%s\\%s"
                    ,data_dir,txtsec[usrsec[cursec]]->code,addpath);
            else
                strcpy(tmp,addpath);
            if(ch=='R') {               /* Remove */
                if(fexist(tmp)) {
                    sprintf(str,text[DeleteTextFileQ],tmp);
                    if(!noyes(str))
                        if(remove(tmp)) errormsg(WHERE,ERR_REMOVE,tmp,0); }
                sprintf(str,"%sTEXT\\%s.IXT"
                    ,data_dir,txtsec[usrsec[cursec]]->code);
				removeline(str,addpath,2,0); }
            else {                      /* Edit */
                strcpy(str,tmp);
                editfile(str); }
            continue; }
		i=(i&~0x80000000L)-1;
        if(!strchr(file[i],'\\'))
            sprintf(str,"%sTEXT\\%s\\%s"
                ,data_dir,txtsec[usrsec[cursec]]->code,file[i]);
        else
            strcpy(str,file[i]);
        attr(LIGHTGRAY);
        printfile(str,0);
        sprintf(str,"Read Text File: %s",file[i]);
        logline("T-",str);
        pause();
        sys_status&=~SS_ABORT;
        for(i=0;i<j;i++)
            FREE(file[i]); } }
return(0);
}

