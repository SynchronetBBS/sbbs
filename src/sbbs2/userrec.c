#line 1 "USERREC.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

/****************************************************************************/
/* Fills 'str' with record for usernumber starting at start for length bytes*/
/* Called from function ???													*/
/****************************************************************************/
void getuserrec(int usernumber,int start, char length, char *str)
{
	char c,path[256];
	int i,file;

if(!usernumber) {
	errormsg(WHERE,ERR_CHK,"user number",0);
	return; }
sprintf(path,"%sUSER\\USER.DAT",data_dir);
if((file=nopen(path,O_RDONLY|O_DENYNONE))==-1) {
	errormsg(WHERE,ERR_OPEN,path,O_RDONLY);
	return; }
if(usernumber<1
	|| filelength(file)<(long)((long)(usernumber-1L)*U_LEN)+(long)start) {
	close(file);
	errormsg(WHERE,ERR_CHK,"user number",usernumber);
	return; }
lseek(file,(long)((long)(usernumber-1)*U_LEN)+start,SEEK_SET);

i=0;
while(i<LOOP_NODEDAB
	&& lock(file,(long)((long)(usernumber-1)*U_LEN)+start,length)==-1) {
	if(i>10)
		mswait(55);
	i++; }

if(i>=LOOP_NODEDAB) {
	close(file);
	errormsg(WHERE,ERR_LOCK,"USER.DAT",usernumber);
    return; }

if(read(file,str,length)!=length) {
	unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
	close(file);
	errormsg(WHERE,ERR_READ,"USER.DAT",length);
	return; }

unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
close(file);
for(c=0;c<length;c++)
	if(str[c]==ETX || str[c]==CR) break;
str[c]=0;
}

/****************************************************************************/
/* Places into USER.DAT at the offset for usernumber+start for length bytes */
/* Called from various locations											*/
/****************************************************************************/
void putuserrec(int usernumber,int start, char length, char *str)
{
	char c,str2[256];
	int file,i;
	node_t node;

if(usernumber<1) {
	errormsg(WHERE,ERR_CHK,"user number",usernumber);
	return; }
sprintf(str2,"%sUSER\\USER.DAT",data_dir);
if((file=nopen(str2,O_WRONLY|O_DENYNONE))==-1) {
	errormsg(WHERE,ERR_OPEN,str2,O_WRONLY);
	return; }
strcpy(str2,str);
if(strlen(str2)<length) {
	for(c=strlen(str2);c<length;c++)
		str2[c]=ETX;
	str2[c]=0; }
lseek(file,(long)((long)((long)((long)usernumber-1)*U_LEN)+start),SEEK_SET);

i=0;
while(i<LOOP_NODEDAB
	&& lock(file,(long)((long)(usernumber-1)*U_LEN)+start,length)==-1) {
	if(i>10)
		mswait(55);
	i++; }

if(i>=LOOP_NODEDAB) {
	close(file);
	errormsg(WHERE,ERR_LOCK,"USER.DAT",usernumber);
    return; }

write(file,str2,length);
unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
close(file);
for(i=1;i<=sys_nodes;i++) {	/* instant user data update */
	if(i==node_num)
		continue;
	getnodedat(i,&node,0);
	if(node.useron==usernumber && (node.status==NODE_INUSE
		|| node.status==NODE_QUIET)) {
		getnodedat(i,&node,1);
		node.misc|=NODE_UDAT;
		putnodedat(i,node);
		break; } }
}

/****************************************************************************/
/* Updates user 'usernumber's record (numeric string) by adding 'adj' to it */
/* returns the new value.													*/
/****************************************************************************/
ulong adjustuserrec(int usernumber,int start, char length, long adj)
{
	char str[256],c,path[256];
	int i,file;
	ulong val;
	node_t node;

if(usernumber<1) {
	errormsg(WHERE,ERR_CHK,"user number",usernumber);
	return(0UL); }
sprintf(path,"%sUSER\\USER.DAT",data_dir);
if((file=nopen(path,O_RDWR|O_DENYNONE))==-1) {
	errormsg(WHERE,ERR_OPEN,path,O_RDWR);
	return(0UL); }
lseek(file,(long)((long)(usernumber-1)*U_LEN)+start,SEEK_SET);

i=0;
while(i<LOOP_NODEDAB
	&& lock(file,(long)((long)(usernumber-1)*U_LEN)+start,length)==-1) {
	if(i>10)
		mswait(55);
	i++; }

if(i>=LOOP_NODEDAB) {
	close(file);
	errormsg(WHERE,ERR_LOCK,"USER.DAT",usernumber);
	return(0); }

if(read(file,str,length)!=length) {
	unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
	close(file);
	errormsg(WHERE,ERR_READ,path,length);
	return(0UL); }
for(c=0;c<length;c++)
	if(str[c]==ETX || str[c]==CR) break;
str[c]=0;
val=atol(str);
if(adj<0L && val<-adj)		/* don't go negative */
	val=0UL;
else val+=adj;
lseek(file,(long)((long)(usernumber-1)*U_LEN)+start,SEEK_SET);
putrec(str,0,length,ultoa(val,tmp,10));
if(write(file,str,length)!=length) {
	unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
	close(file);
	errormsg(WHERE,ERR_WRITE,path,length);
	return(val); }
unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
close(file);
for(i=1;i<=sys_nodes;i++) { /* instant user data update */
	if(i==node_num)
		continue;
	getnodedat(i,&node,0);
	if(node.useron==usernumber && (node.status==NODE_INUSE
		|| node.status==NODE_QUIET)) {
		getnodedat(i,&node,1);
		node.misc|=NODE_UDAT;
		putnodedat(i,node);
        break; } }
return(val);
}

/****************************************************************************/
/* Subtract credits from the current user online, accounting for the new    */
/* "free credits" field.                                                    */
/****************************************************************************/
void subtract_cdt(long amt)
{
    long mod;

if(!amt)
    return;
if(useron.freecdt) {
    if(amt>useron.freecdt) {      /* subtract both credits and */
        mod=amt-useron.freecdt;   /* free credits */
        putuserrec(useron.number,U_FREECDT,10,"0");
        useron.freecdt=0;
        useron.cdt=adjustuserrec(useron.number,U_CDT,10,-mod); }
    else {                          /* subtract just free credits */
        useron.freecdt-=amt;
        putuserrec(useron.number,U_FREECDT,10
            ,ultoa(useron.freecdt,tmp,10)); } }
else    /* no free credits */
    useron.cdt=adjustuserrec(useron.number,U_CDT,10,-amt);
}
