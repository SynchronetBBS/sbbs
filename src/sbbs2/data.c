#line 1 "DATA.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/**************************************************************/
/* Functions that store and retrieve data from disk or memory */
/**************************************************************/

#include "sbbs.h"

/****************************************************************************/
/* Looks for a perfect match amoung all usernames (not deleted users)		*/
/* Returns the number of the perfect matched username or 0 if no match		*/
/* Called from functions waitforcall and newuser							*/
/****************************************************************************/
uint matchuser(char *str)
{
	int file;
	char str2[256],c;
	ulong l,length;
	FILE *stream;

sprintf(str2,"%sUSER\\NAME.DAT",data_dir);
if((stream=fnopen(&file,str2,O_RDONLY))==NULL)
	return(0);
length=filelength(file);
for(l=0;l<length;l+=LEN_ALIAS+2) {
	fread(str2,LEN_ALIAS+2,1,stream);
	for(c=0;c<LEN_ALIAS;c++)
		if(str2[c]==ETX) break;
	str2[c]=0;
	if(!stricmp(str,str2)) {
		fclose(stream);
		return((l/(LEN_ALIAS+2))+1); } }
fclose(stream);
return(0);
}

/****************************************************************************/
/* Looks for close or perfect matches between str and valid usernames and  	*/
/* numbers and prompts user for near perfect matches in names.				*/
/* Returns the number of the matched user or 0 if unsuccessful				*/
/* Called from functions main_sec, useredit and readmailw					*/
/****************************************************************************/
uint finduser(char *instr)
{
	int file,i;
	char str[128],str2[256],str3[256],ynq[25],c,pass=1;
	ulong l,length;
	FILE *stream;

i=atoi(instr);
if(i>0) {
	username(i,str2);
	if(str2[0] && strcmp(str2,"DELETED USER"))
		return(i); }
strcpy(str,instr);
strupr(str);
sprintf(str3,"%sUSER\\NAME.DAT",data_dir);
if(flength(str3)<1L)
	return(0);
if((stream=fnopen(&file,str3,O_RDONLY))==NULL) {
	errormsg(WHERE,ERR_OPEN,str3,O_RDONLY);
	return(0); }
sprintf(ynq,"%.2s",text[YN]);
ynq[2]='Q';
ynq[3]=0;
length=filelength(file);
while(pass<3) {
	fseek(stream,0L,SEEK_SET);	/* seek to beginning for each pass */
	for(l=0;l<length;l+=LEN_ALIAS+2) {
		if(!online) break;
		fread(str2,LEN_ALIAS+2,1,stream);
		for(c=0;c<LEN_ALIAS;c++)
			if(str2[c]==ETX) break;
		str2[c]=0;
		if(!c)		/* deleted user */
			continue;
		strcpy(str3,str2);
		strupr(str2);
		if(pass==1 && !strcmp(str,str2)) {
			fclose(stream);
			return((l/(LEN_ALIAS+2))+1); }
		if(pass==2 && strstr(str2,str)) {
			bprintf(text[DoYouMeanThisUserQ],str3
				,(uint)(l/(LEN_ALIAS+2))+1);
			c=getkeys(ynq,0);
			if(sys_status&SS_ABORT) {
				fclose(stream);
				return(0); }
			if(c==text[YN][0]) {
				fclose(stream);
				return((l/(LEN_ALIAS+2))+1); }
			if(c=='Q') {
				fclose(stream);
				return(0); } } }
	pass++; }
bputs(text[UnknownUser]);
fclose(stream);
return(0);
}

/****************************************************************************/
/* Returns the number of files in the directory 'dirnum'                    */
/****************************************************************************/
int getfiles(uint dirnum)
{
	char str[256];
	long l;

sprintf(str,"%s%s.IXB",dir[dirnum]->data_dir,dir[dirnum]->code);
l=flength(str);
if(l>0L)
	return(l/F_IXBSIZE);
return(0);
}

/****************************************************************************/
/* Returns the number of user transfers in XFER.IXT for either a dest user  */
/* source user, or filename.												*/
/****************************************************************************/
int getuserxfers(int fromuser, int destuser, char *fname)
{
	char str[256];
	int file,found=0;
	FILE *stream;

sprintf(str,"%sXFER.IXT",data_dir);
if(!fexist(str))
	return(0);
if(!flength(str)) {
	remove(str);
	return(0); }
if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
	return(0); }
while(!ferror(stream)) {
	if(!fgets(str,81,stream))
		break;
	str[22]=0;
	if(fname!=NULL && fname[0] && !strncmp(str+5,fname,12))
			found++;
	else if(fromuser && atoi(str+18)==fromuser)
			found++;
	else if(destuser && atoi(str)==destuser)
			found++; }
fclose(stream);
return(found);
}

/****************************************************************************/
/* Returns the number of the last user in USER.DAT (deleted ones too)		*/
/* Called from function useredit											*/
/****************************************************************************/
uint lastuser()
{
	char str[256];
	long length;

sprintf(str,"%sUSER\\USER.DAT",data_dir);
if((length=flength(str))>0)
	return((uint)(length/U_LEN));
return(0);
}

/****************************************************************************/
/* Returns the number of files in the database for 'dir'					*/
/****************************************************************************/
uint gettotalfiles(uint dirnum)
{
	char str[81];

sprintf(str,"%s%s.IXB",dir[dirnum]->data_dir,dir[dirnum]->code);
return((uint)(flength(str)/F_IXBSIZE));
}


